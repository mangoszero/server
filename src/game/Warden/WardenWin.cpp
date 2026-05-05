/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 * Copyright (C) 2008-2015 TrinityCore <http://www.trinitycore.org/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "HMACSHA1.h"
#include "WardenKeyGeneration.h"
#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Log.h"
#include "Opcodes.h"
#include "ByteBuffer.h"
#include <openssl/md5.h>
#include "Database/DatabaseEnv.h"
#include "World.h"
#include "Player.h"
#include "Util.h"
#include "WardenWin.h"
#include "WardenModuleWin.h"
#include "WardenCheckMgr.h"
#include "GameTime.h"

WardenWin::WardenWin() : Warden(), _serverTicks(0), _customChainActive(false)
{
    _customChainInFlight.checkId = 0;
    _customChainInFlight.hopIndex = 0;
    _customChainInFlight.currentAddress = 0;
    _customChainInFlight.finalLength = 0;
}

WardenWin::~WardenWin() { }

bool WardenWin::ParseChainOffsets(const std::string& str, std::vector<uint32>& out)
{
    out.clear();

    std::string s;
    s.reserve(str.size());
    for (char c : str)
    {
        if (c != ' ' && c != '\t' && c != '\r' && c != '\n')
            s.push_back(c);
    }

    if (s.empty())
        return true; // zero-hop chain is legal

    size_t pos = 0;
    while (pos <= s.size())
    {
        size_t comma = s.find(',', pos);
        std::string token = s.substr(pos, (comma == std::string::npos) ? std::string::npos : comma - pos);
        if (token.empty())
            return false;

        const char* cstr = token.c_str();
        int base = 10;
        if (token.size() > 2 && token[0] == '0' && (token[1] == 'x' || token[1] == 'X'))
        {
            base = 16;
            cstr += 2;
            if (*cstr == '\0')
                return false;
        }

        char* endp = nullptr;
        unsigned long val = strtoul(cstr, &endp, base);
        if (!endp || *endp != '\0')
            return false;

        out.push_back(static_cast<uint32>(val));

        if (comma == std::string::npos)
            break;
        pos = comma + 1;
    }

    return true;
}

void WardenWin::StartCustomChain(WardenCheck* wd)
{
    _customChainInFlight.checkId = wd->CheckId;
    _customChainInFlight.offsets.clear();
    _customChainInFlight.hopIndex = 0;
    _customChainInFlight.currentAddress = wd->Address;
    _customChainInFlight.finalLength = wd->Length;

    if (!ParseChainOffsets(wd->Str, _customChainInFlight.offsets))
    {
        sLog.outWarden("CUSTOM_CHECK CheckId %u has malformed offset chain '%s'; skipping",
                       wd->CheckId, wd->Str.c_str());
        _customChainActive = false;
        return;
    }

    _customChainActive = true;
}

void WardenWin::Init(WorldSession* session, BigNumber* k)
{
    _session = session;
    // Generate Warden Key
    SHA1Randx WK(k->AsByteArray(), k->GetNumBytes());
    WK.Generate(_inputKey, 16);
    WK.Generate(_outputKey, 16);

    memcpy(_seed, Module.Seed, 16);

    _inputCrypto.Init(_inputKey);
    _outputCrypto.Init(_outputKey);
    sLog.outWarden("Server side warden for client %u (build %u) initializing...", session->GetAccountId(), _session->GetClientBuild());
    sLog.outWarden("C->S Key: %s", ByteArrayToHexStr(_inputKey, 16).c_str());
    sLog.outWarden("S->C Key: %s", ByteArrayToHexStr(_outputKey, 16).c_str());
    sLog.outWarden("  Seed: %s", ByteArrayToHexStr(_seed, 16).c_str());
    sLog.outWarden("Loading Module...");

    _module = GetModuleForClient();

    sLog.outWarden("Module Key: %s", ByteArrayToHexStr(_module->Key, 16).c_str());
    sLog.outWarden("Module ID: %s", ByteArrayToHexStr(_module->Id, 16).c_str());
    RequestModule();
}

ClientWardenModule* WardenWin::GetModuleForClient()
{
    ClientWardenModule *mod = new ClientWardenModule;

    uint32 length = sizeof(Module.Module);

    // data assign
    mod->CompressedSize = length;
    mod->CompressedData = new uint8[length];
    memcpy(mod->CompressedData, Module.Module, length);
    memcpy(mod->Key, Module.ModuleKey, 16);

    // md5 hash
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, mod->CompressedData, length);
    MD5_Final((uint8*)&mod->Id, &ctx);

    return mod;
}

void WardenWin::InitializeModule()
{
    sLog.outWarden("Initialize module");

    // Create packet structure
    WardenInitModuleRequest Request;
    Request.Command1 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size1 = 20;
    Request.CheckSumm1 = BuildChecksum(&Request.Unk1, 20);
    Request.Unk1 = 1;
    Request.Unk2 = 0;
    Request.Type = 1;
    Request.String_library1 = 0;
    Request.Function1[0] = 0x002485F0;                      // 0x00400000 + 0x002485F0 SFileOpenFile
    Request.Function1[1] = 0x002487F0;                      // 0x00400000 + 0x002487F0 SFileGetFileSize
    Request.Function1[2] = 0x00248460;                      // 0x00400000 + 0x00248460 SFileReadFile
    Request.Function1[3] = 0x00248730;                      // 0x00400000 + 0x00248730 SFileCloseFile

    Request.Command2 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size2 = 8;
    Request.CheckSumm2 = BuildChecksum(&Request.Unk2, 8);
    Request.Unk3 = 4;
    Request.Unk4 = 0;
    Request.String_library2 = 0;
    Request.Function2 = 0x00419D40;                         // 0x00400000 + 0x00419D40 FrameScript::GetText
    Request.Function2_set = 1;

    Request.Command3 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size3 = 8;
    Request.CheckSumm3 = BuildChecksum(&Request.Unk5, 8);
    Request.Unk5 = 1;
    Request.Unk6 = 1;
    Request.String_library3 = 0;
    Request.Function3 = 0x0046AE20;                         // 0x00400000 + 0x0046AE20 PerformanceCounter
    Request.Function3_set = 1;

    // Encrypt with warden RC4 key.
    EncryptData((uint8*)&Request, sizeof(WardenInitModuleRequest));

    WorldPacket pkt(SMSG_WARDEN_DATA, sizeof(WardenInitModuleRequest));
    pkt.append((uint8*)&Request, sizeof(WardenInitModuleRequest));
    _session->SendPacket(&pkt);

    Warden::InitializeModule();
}

void WardenWin::HandleHashResult(ByteBuffer &buff)
{
    buff.rpos(buff.wpos());

    // Verify key
    if (memcmp(buff.contents() + 1, Module.ClientKeySeedHash, sizeof(Module.ClientKeySeedHash)) != 0)
    {
        sLog.outWarden("%s failed hash reply. Action: %s", _session->GetPlayerName(), Penalty().c_str());
        return;
    }

    sLog.outWarden("Request hash reply: succeed");

    // Change keys here
    memcpy(_inputKey, Module.ClientKeySeed, 16);
    memcpy(_outputKey, Module.ServerKeySeed, 16);

    _inputCrypto.Init(_inputKey);
    _outputCrypto.Init(_outputKey);

    _previousTimestamp = GameTime::GetGameTimeMS();
}

void WardenWin::RequestData()
{
    sLog.outWarden("Request data");

    uint16 build = _session->GetClientBuild();
    uint16 id = 0;
    uint8 type = 0;
    WardenCheck* wd = NULL;

    // If all checks were done, fill the todo list again
    if (_memChecksTodo.empty())
    {
        sWardenCheckMgr->GetWardenCheckIds(true, build, _memChecksTodo);
    }

    if (_otherChecksTodo.empty())
    {
        sWardenCheckMgr->GetWardenCheckIds(false, build, _otherChecksTodo);
    }

    _serverTicks = GameTime::GetGameTimeMS();

    _currentChecks.clear();

    // Build check request
    for (uint16 i = 0; i < sWorld.getConfig(CONFIG_UINT32_WARDEN_NUM_MEM_CHECKS); ++i)
    {
        // If a CUSTOM_CHECK chain is mid-walk, consume one slot for it (do not pop a new id).
        if (_customChainActive)
        {
            _currentChecks.push_back(_customChainInFlight.checkId);
            continue;
        }

        // If todo list is done break loop (will be filled on next Update() run)
        if (_memChecksTodo.empty())
        {
            break;
        }

        // Peek the next id; if it's a CUSTOM_CHECK and a chain is already active, defer it.
        id = _memChecksTodo.back();
        WardenCheck* peek = sWardenCheckMgr->GetWardenDataById(build, id);

        // Pop and schedule.
        _memChecksTodo.pop_back();

        if (peek && peek->Type == CUSTOM_CHECK)
        {
            StartCustomChain(peek);
            if (!_customChainActive)
            {
                // Malformed chain; loader should have filtered it. Skip without scheduling.
                continue;
            }
        }

        // Add the id to the list sent in this cycle
        _currentChecks.push_back(id);
    }

    ByteBuffer buff;
    buff << uint8(WARDEN_SMSG_CHEAT_CHECKS_REQUEST);

    for (uint16 i = 0; i < sWorld.getConfig(CONFIG_UINT32_WARDEN_NUM_OTHER_CHECKS); ++i)
    {
        // If todo list is done break loop (will be filled on next Update() run)
        if (_otherChecksTodo.empty())
        {
            break;
        }

        // Get check id from the end and remove it from todo
        id = _otherChecksTodo.back();
        _otherChecksTodo.pop_back();

        // Add the id to the list sent in this cycle
        _currentChecks.push_back(id);

        // if we are here, the function is guaranteed to not return NULL
        // but ... who knows
        wd = sWardenCheckMgr->GetWardenDataById(build, id);
        if (wd)
        {
            switch (wd->Type)
            {
                case MPQ_CHECK:
                case LUA_STR_CHECK:
                case DRIVER_CHECK:
                    buff << uint8(wd->Str.size());
                    buff.append(wd->Str.c_str(), wd->Str.size());
                    break;
                default:
                    break;
            }
        }
    }

    uint8 xorByte = _inputKey[0];

    // Add TIMING_CHECK
    buff << uint8(0x00);
    buff << uint8(TIMING_CHECK ^ xorByte);

    uint8 index = 1;

    for (std::list<uint16>::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); ++itr)
    {
        wd = sWardenCheckMgr->GetWardenDataById(build, *itr);

        type = wd->Type;
        // CUSTOM_CHECK is server-only; emit it on the wire as a MEM_CHECK so the client module accepts it.
        uint8 wireType = (type == CUSTOM_CHECK) ? uint8(MEM_CHECK) : type;
        buff << uint8(wireType ^ xorByte);
        switch (type)
        {
            case MEM_CHECK:
            {
                buff << uint8(0x00);
                buff << uint32(wd->Address);
                buff << uint8(wd->Length);
                break;
            }
            case CUSTOM_CHECK:
            {
                uint32 addr = _customChainInFlight.currentAddress;
                uint8  len  = (_customChainInFlight.hopIndex < _customChainInFlight.offsets.size())
                                ? uint8(4)                              // intermediate pointer hop
                                : _customChainInFlight.finalLength;     // terminal hop
                buff << uint8(0x00);
                buff << uint32(addr);
                buff << uint8(len);
                break;
            }
            case PAGE_CHECK_A:
            case PAGE_CHECK_B:
            {
                buff.append(wd->Data.AsByteArray(0, false), wd->Data.GetNumBytes());
                buff << uint32(wd->Address);
                buff << uint8(wd->Length);
                break;
            }
            case MPQ_CHECK:
            case LUA_STR_CHECK:
            {
                buff << uint8(index++);
                break;
            }
            case DRIVER_CHECK:
            {
                buff.append(wd->Data.AsByteArray(0, false), wd->Data.GetNumBytes());
                buff << uint8(index++);
                break;
            }
            case MODULE_CHECK:
            {
                uint32 seed = rand32();
                buff << uint32(seed);
                HMACSHA1 hmac(4, (uint8*)&seed);
                hmac.UpdateData(wd->Str);
                hmac.Finalize();
                buff.append(hmac.GetDigest(), hmac.GetLength());
                break;
            }
            /*case PROC_CHECK:
            {
                buff.append(wd->i.AsByteArray(0, false).get(), wd->i.GetNumBytes());
                buff << uint8(index++);
                buff << uint8(index++);
                buff << uint32(wd->Address);
                buff << uint8(wd->Length);
                break;
            }*/
            default:
                break;                                      // Should never happen
        }
    }
    buff << uint8(xorByte);
    buff.hexlike();

    // Encrypt with warden RC4 key
    EncryptData((uint8*)buff.contents(), buff.size());

    WorldPacket pkt(SMSG_WARDEN_DATA, buff.size());
    pkt.append(buff);
    _session->SendPacket(&pkt);

    std::stringstream stream;
    stream << "Sent check id's: ";
    for (std::list<uint16>::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); ++itr)
    {
        stream << *itr << " ";
    }

    sLog.outWarden("%s", stream.str().c_str());

    Warden::RequestData();
}

void WardenWin::HandleData(ByteBuffer &buff)
{
    sLog.outWarden("Handle data");

    uint16 Length;
    buff >> Length;
    uint32 Checksum;
    buff >> Checksum;

    if (!IsValidCheckSum(Checksum, buff.contents() + buff.rpos(), Length))
    {
        buff.rpos(buff.wpos());
        sLog.outWarden("%s failed checksum. Action: %s", _session->GetPlayerName(), Penalty().c_str());
        return;
    }

    // TIMING_CHECK
    {
        uint8 result;
        buff >> result;
        /// @todo test it.
        if (result == 0x00)
        {
            sLog.outWarden("%s failed timing check. Action: %s", _session->GetPlayerName(), Penalty().c_str());
            return;
        }

        uint32 newClientTicks;
        buff >> newClientTicks;

        uint32 ticksNow = GameTime::GetGameTimeMS();
        uint32 ourTicks = newClientTicks + (ticksNow - _serverTicks);

        sLog.outWarden("ServerTicks %u, RequestTicks %u, ClientTicks %u", ticksNow, _serverTicks, newClientTicks);  // Now, At request, At response
        sLog.outWarden("Waittime %u", ourTicks - newClientTicks);

    }

    WardenCheckResult* rs;
    WardenCheck *rd;
    uint8 type;
    uint16 checkFailed = 0;

    for (std::list<uint16>::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); ++itr)
    {
        rd = sWardenCheckMgr->GetWardenDataById(_session->GetClientBuild(), *itr);
        rs = sWardenCheckMgr->GetWardenResultById(_session->GetClientBuild(), *itr);

        type = rd->Type;
        switch (type)
        {
            case MEM_CHECK:
            {
                uint8 Mem_Result;
                buff >> Mem_Result;

                if (Mem_Result != 0)
                {
                    sLog.outWarden("RESULT MEM_CHECK not 0x00, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    checkFailed = *itr;
                    continue;
                }
                if (memcmp(buff.contents() + buff.rpos(), rs->Result.AsByteArray(0, false), rd->Length) != 0)
                {
                    sLog.outWarden("RESULT MEM_CHECK fail CheckId %u account Id %u", *itr, _session->GetAccountId());
                    checkFailed = *itr;
                    buff.rpos(buff.rpos() + rd->Length);
                    continue;
                }

                buff.rpos(buff.rpos() + rd->Length);
                sLog.outWarden("RESULT MEM_CHECK passed CheckId %u account Id %u", *itr, _session->GetAccountId());
                break;
            }
            case CUSTOM_CHECK:
            {
                uint8 status;
                buff >> status;

                if (status != 0)
                {
                    sLog.outWarden("RESULT CUSTOM_CHECK status not 0x00, CheckId %u account Id %u (hop %zu)",
                                   *itr, _session->GetAccountId(), _customChainInFlight.hopIndex);
                    checkFailed = *itr;
                    _customChainActive = false;
                    continue;
                }

                if (!_customChainActive || _customChainInFlight.checkId != *itr)
                {
                    sLog.outWarden("CUSTOM_CHECK response for CheckId %u with no active chain", *itr);
                    checkFailed = *itr;
                    _customChainActive = false;
                    buff.rpos(buff.wpos());     // can't safely know read length; drain to avoid desync
                    return;
                }

                if (_customChainInFlight.hopIndex < _customChainInFlight.offsets.size())
                {
                    // Intermediate hop: read 4-byte LE pointer, advance chain, await next cycle.
                    uint32 ptr;
                    memcpy(&ptr, buff.contents() + buff.rpos(), sizeof(uint32));
                    buff.rpos(buff.rpos() + sizeof(uint32));

                    if (ptr == 0)
                    {
                        sLog.outWarden("RESULT CUSTOM_CHECK NULL deref at hop %zu, CheckId %u account Id %u",
                                       _customChainInFlight.hopIndex, *itr, _session->GetAccountId());
                        checkFailed = *itr;
                        _customChainActive = false;
                        continue;
                    }

                    _customChainInFlight.currentAddress = ptr + _customChainInFlight.offsets[_customChainInFlight.hopIndex];
                    ++_customChainInFlight.hopIndex;
                    sLog.outWarden("CUSTOM_CHECK hop advanced to %zu (next addr 0x%08X), CheckId %u",
                                   _customChainInFlight.hopIndex, _customChainInFlight.currentAddress, *itr);
                    break;
                }

                // Terminal hop: compare finalLength bytes against expected result.
                if (memcmp(buff.contents() + buff.rpos(), rs->Result.AsByteArray(0, false),
                           _customChainInFlight.finalLength) != 0)
                {
                    sLog.outWarden("RESULT CUSTOM_CHECK fail CheckId %u account Id %u",
                                   *itr, _session->GetAccountId());
                    checkFailed = *itr;
                    buff.rpos(buff.rpos() + _customChainInFlight.finalLength);
                    _customChainActive = false;
                    continue;
                }

                buff.rpos(buff.rpos() + _customChainInFlight.finalLength);
                sLog.outWarden("RESULT CUSTOM_CHECK passed CheckId %u account Id %u",
                               *itr, _session->GetAccountId());
                _customChainActive = false;
                break;
            }
            case PAGE_CHECK_A:
            case PAGE_CHECK_B:
            case DRIVER_CHECK:
            case MODULE_CHECK:
            {
                const uint8 byte = 0xE9;
                if (memcmp(buff.contents() + buff.rpos(), &byte, sizeof(uint8)) != 0)
                {
                    if (type == PAGE_CHECK_A || type == PAGE_CHECK_B)
                    {
                        sLog.outWarden("RESULT PAGE_CHECK fail, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    }
                    if (type == MODULE_CHECK)
                    {
                        sLog.outWarden("RESULT MODULE_CHECK fail, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    }
                    if (type == DRIVER_CHECK)
                    {
                        sLog.outWarden("RESULT DRIVER_CHECK fail, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    }
                    checkFailed = *itr;
                    buff.rpos(buff.rpos() + 1);
                    continue;
                }

                buff.rpos(buff.rpos() + 1);
                if (type == PAGE_CHECK_A || type == PAGE_CHECK_B)
                {
                    sLog.outWarden("RESULT PAGE_CHECK passed CheckId %u account Id %u", *itr, _session->GetAccountId());
                }
                else if (type == MODULE_CHECK)
                {
                    sLog.outWarden("RESULT MODULE_CHECK passed CheckId %u account Id %u", *itr, _session->GetAccountId());
                }
                else if (type == DRIVER_CHECK)
                {
                    sLog.outWarden("RESULT DRIVER_CHECK passed CheckId %u account Id %u", *itr, _session->GetAccountId());
                }
                break;
            }
            case LUA_STR_CHECK:
            {
                uint8 Lua_Result;
                buff >> Lua_Result;

                if (Lua_Result != 0)
                {
                    sLog.outWarden("RESULT LUA_STR_CHECK fail, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    checkFailed = *itr;
                    continue;
                }

                uint8 luaStrLen;
                buff >> luaStrLen;

                if (luaStrLen != 0)
                {
                    char *str = new char[luaStrLen + 1];
                    memcpy(str, buff.contents() + buff.rpos(), luaStrLen);
                    str[luaStrLen] = '\0'; // null terminator
                    sLog.outWarden("Lua string: %s", str);
                    delete[] str;
                }
                buff.rpos(buff.rpos() + luaStrLen);         // Skip string
                sLog.outWarden("RESULT LUA_STR_CHECK passed, CheckId %u account Id %u", *itr, _session->GetAccountId());
                break;
            }
            case MPQ_CHECK:
            {
                uint8 Mpq_Result;
                buff >> Mpq_Result;

                if (Mpq_Result != 0)
                {
                    sLog.outWarden("RESULT MPQ_CHECK not 0x00 account id %u", _session->GetAccountId());
                    checkFailed = *itr;
                    continue;
                }

                if (memcmp(buff.contents() + buff.rpos(), rs->Result.AsByteArray(0, false), 20) != 0) // SHA1
                {
                    sLog.outWarden("RESULT MPQ_CHECK fail, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    checkFailed = *itr;
                    buff.rpos(buff.rpos() + 20);            // 20 bytes SHA1
                    continue;
                }

                buff.rpos(buff.rpos() + 20);                // 20 bytes SHA1
                sLog.outWarden("RESULT MPQ_CHECK passed, CheckId %u account Id %u", *itr, _session->GetAccountId());
                break;
            }
            default:                                        // Should never happen
                break;
        }
    }

    if (checkFailed > 0)
    {
        WardenCheck* check = sWardenCheckMgr->GetWardenDataById(_session->GetClientBuild(), checkFailed);   //note it IS NOT NULL here
        sLog.outWarden("%s failed Warden check %u. Action: %s", _session->GetPlayerName(), checkFailed, Penalty(check).c_str());
        LogPositiveToDB(check);
    }

    Warden::HandleData(buff);
}
