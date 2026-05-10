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

WardenWin::WardenWin() : Warden(), _serverTicks(0) {}

WardenWin::~WardenWin() { }

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

    // Zero-initialize so checksums never cover uninitialized stack bytes.
    WardenInitModuleRequest Request;
    memset(&Request, 0, sizeof(Request));

    // Block 1 - SFile* function table
    Request.Command1 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size1 = 20;
    Request.Unk1 = 1;
    Request.Unk2 = 0;
    Request.Type = 1;
    Request.String_library1 = 0;
    Request.Function1[0] = 0x002485F0;                      // 0x00400000 + 0x002485F0 SFileOpenFile
    Request.Function1[1] = 0x002487F0;                      // 0x00400000 + 0x002487F0 SFileGetFileSize
    Request.Function1[2] = 0x00248460;                      // 0x00400000 + 0x00248460 SFileReadFile
    Request.Function1[3] = 0x00248730;                      // 0x00400000 + 0x00248730 SFileCloseFile

    // Block 2 - FrameScript::GetText
    Request.Command2 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size2 = 8;
    Request.Unk3 = 4;
    Request.Unk4 = 0;
    Request.String_library2 = 0;
    Request.Function2 = 0x00419D40;                         // 0x00400000 + 0x00419D40 FrameScript::GetText
    Request.Function2_set = 1;

    // Block 3 - PerformanceCounter
    Request.Command3 = WARDEN_SMSG_MODULE_INITIALIZE;
    Request.Size3 = 8;
    Request.Unk5 = 1;
    Request.Unk6 = 1;
    Request.String_library3 = 0;
    Request.Function3 = 0x0046AE20;                         // 0x00400000 + 0x0046AE20 PerformanceCounter
    Request.Function3_set = 1;

    // Compute checksums AFTER all covered fields have been assigned.
    Request.CheckSumm1 = BuildChecksum(&Request.Unk1, 20);
    Request.CheckSumm2 = BuildChecksum(&Request.Unk2, 8);
    Request.CheckSumm3 = BuildChecksum(&Request.Unk5, 8);

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

    // No checks defined for this build at all - controlled disable rather than
    // sending an empty request that the client may reject.
    if (_memChecksTodo.empty() && _otherChecksTodo.empty())
    {
        sLog.outWarden("No Warden checks loaded for build %u (account %u). Skipping request.",
                       build, _session->GetAccountId());
        Warden::RequestData();
        return;
    }

    _serverTicks = GameTime::GetGameTimeMS();

    _currentChecks.clear();

    // Build check request - memory checks
    for (uint16 i = 0; i < sWorld.getConfig(CONFIG_UINT32_WARDEN_NUM_MEM_CHECKS); ++i)
    {
        if (_memChecksTodo.empty())
        {
            break;
        }

        id = _memChecksTodo.back();
        _memChecksTodo.pop_back();

        // Skip checks whose metadata could not be resolved (missing/malformed DB row).
        if (!sWardenCheckMgr->GetWardenDataById(build, id))
        {
            sLog.outWarden("Skipping unknown memory check id %u for build %u", id, build);
            continue;
        }

        _currentChecks.push_back(id);
    }

    ByteBuffer buff;
    buff << uint8(WARDEN_SMSG_CHEAT_CHECKS_REQUEST);

    for (uint16 i = 0; i < sWorld.getConfig(CONFIG_UINT32_WARDEN_NUM_OTHER_CHECKS); ++i)
    {
        if (_otherChecksTodo.empty())
        {
            break;
        }

        id = _otherChecksTodo.back();
        _otherChecksTodo.pop_back();

        wd = sWardenCheckMgr->GetWardenDataById(build, id);
        if (!wd)
        {
            sLog.outWarden("Skipping unknown check id %u for build %u", id, build);
            continue;
        }

        _currentChecks.push_back(id);

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

    uint8 xorByte = _inputKey[0];

    // Add TIMING_CHECK
    buff << uint8(0x00);
    buff << uint8(TIMING_CHECK ^ xorByte);

    uint8 index = 1;

    for (std::list<uint16>::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); )
    {
        wd = sWardenCheckMgr->GetWardenDataById(build, *itr);
        if (!wd)
        {
            // Defensive: should never trip because we filtered above, but if a row
            // disappears between filter and use, drop the id from the cycle.
            sLog.outWarden("Warden check id %u disappeared mid-cycle (build %u), removing", *itr, build);
            itr = _currentChecks.erase(itr);
            continue;
        }

        type = wd->Type;
        buff << uint8(type ^ xorByte);
        switch (type)
        {
            case MEM_CHECK:
            {
                buff << uint8(0x00);
                buff << uint32(wd->Address);
                buff << uint8(wd->Length);
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
        ++itr;
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

    // Reject responses we never asked for instead of trying to parse against
    // a stale (or empty) _currentChecks list.
    if (_state != WardenState::STATE_REQUESTED_DATA)
    {
        sLog.outWarden("Account %u sent CHEAT_CHECKS_RESULT in unexpected state %s",
                       _session->GetAccountId(), WardenState::to_string(_state));
        buff.rpos(buff.wpos());
        return;
    }

    // We need at least Length(2) + Checksum(4) + Timing(1) + Ticks(4) = 11 bytes
    // before we can safely start reading.
    if (buff.size() - buff.rpos() < 11)
    {
        sLog.outWarden("%s sent truncated Warden response (size %u). Action: %s",
                       _session->GetPlayerName(), uint32(buff.size() - buff.rpos()), Penalty().c_str());
        buff.rpos(buff.wpos());
        return;
    }

    uint16 Length;
    buff >> Length;
    uint32 Checksum;
    buff >> Checksum;

    // Length must fit within the buffer or memcmp/checksum will read OOB.
    if (Length > buff.size() - buff.rpos())
    {
        sLog.outWarden("%s sent invalid Warden length %u (remaining %u). Action: %s",
                       _session->GetPlayerName(), Length, uint32(buff.size() - buff.rpos()), Penalty().c_str());
        buff.rpos(buff.wpos());
        return;
    }

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
    std::list<uint16> failedChecks;

    try
    {
        for (std::list<uint16>::iterator itr = _currentChecks.begin(); itr != _currentChecks.end(); ++itr)
        {
            rd = sWardenCheckMgr->GetWardenDataById(_session->GetClientBuild(), *itr);
            rs = sWardenCheckMgr->GetWardenResultById(_session->GetClientBuild(), *itr);

            // Without metadata we cannot know how many bytes this check consumes;
            // any further parsing would desynchronize us from the rest of the buffer.
            if (!rd)
            {
                sLog.outWarden("Warden response for unknown check id %u (account %u) - aborting parse",
                               *itr, _session->GetAccountId());
                buff.rpos(buff.wpos());
                break;
            }

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
                        failedChecks.push_back(*itr);
                        continue;
                    }

                    if (!rs)
                    {
                        sLog.outWarden("MEM_CHECK id %u missing expected result row - skipping", *itr);
                        buff.read_skip(rd->Length);
                        continue;
                    }

                    if (buff.rpos() + rd->Length > buff.size())
                    {
                        sLog.outWarden("MEM_CHECK id %u response truncated - aborting parse", *itr);
                        buff.rpos(buff.wpos());
                        break;
                    }

                    if (memcmp(buff.contents() + buff.rpos(), rs->Result.AsByteArray(0, false), rd->Length) != 0)
                    {
                        sLog.outWarden("RESULT MEM_CHECK fail CheckId %u account Id %u", *itr, _session->GetAccountId());
                        failedChecks.push_back(*itr);
                        buff.read_skip(rd->Length);
                        continue;
                    }

                    buff.read_skip(rd->Length);
                    sLog.outWarden("RESULT MEM_CHECK passed CheckId %u account Id %u", *itr, _session->GetAccountId());
                    break;
                }
                case PAGE_CHECK_A:
                case PAGE_CHECK_B:
                case DRIVER_CHECK:
                case MODULE_CHECK:
                {
                    if (buff.rpos() + 1 > buff.size())
                    {
                        sLog.outWarden("PAGE/DRIVER/MODULE check id %u response truncated - aborting", *itr);
                        buff.rpos(buff.wpos());
                        break;
                    }

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
                        failedChecks.push_back(*itr);
                        buff.read_skip(1);
                        continue;
                    }

                    buff.read_skip(1);
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
                        failedChecks.push_back(*itr);
                        continue;
                    }

                    uint8 luaStrLen;
                    buff >> luaStrLen;

                    if (luaStrLen != 0)
                    {
                        if (buff.rpos() + luaStrLen > buff.size())
                        {
                            sLog.outWarden("LUA_STR_CHECK id %u length %u exceeds buffer - aborting", *itr, luaStrLen);
                            buff.rpos(buff.wpos());
                            break;
                        }

                        std::vector<char> str(luaStrLen + 1);
                        memcpy(str.data(), buff.contents() + buff.rpos(), luaStrLen);
                        str[luaStrLen] = '\0';
                        sLog.outWarden("Lua string: %s", str.data());
                    }
                    buff.read_skip(luaStrLen);
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
                        failedChecks.push_back(*itr);
                        continue;
                    }

                    if (!rs)
                    {
                        sLog.outWarden("MPQ_CHECK id %u missing expected result row - skipping", *itr);
                        buff.read_skip(20);
                        continue;
                    }

                    if (buff.rpos() + 20 > buff.size())
                    {
                        sLog.outWarden("MPQ_CHECK id %u response truncated - aborting", *itr);
                        buff.rpos(buff.wpos());
                        break;
                    }

                    if (memcmp(buff.contents() + buff.rpos(), rs->Result.AsByteArray(0, false), 20) != 0) // SHA1
                    {
                        sLog.outWarden("RESULT MPQ_CHECK fail, CheckId %u account Id %u", *itr, _session->GetAccountId());
                        failedChecks.push_back(*itr);
                        buff.read_skip(20);
                        continue;
                    }

                    buff.read_skip(20);
                    sLog.outWarden("RESULT MPQ_CHECK passed, CheckId %u account Id %u", *itr, _session->GetAccountId());
                    break;
                }
                default:                                        // Should never happen
                    sLog.outWarden("Unhandled Warden check type %u (CheckId %u) - aborting parse",
                                   uint32(type), *itr);
                    buff.rpos(buff.wpos());
                    break;
            }
        }
    }
    catch (ByteBufferException&)
    {
        // ByteBuffer reads throw on underflow; treat as malformed packet.
        sLog.outWarden("%s sent malformed Warden response (buffer underflow). Action: %s",
                       _session->GetPlayerName(), Penalty().c_str());
        buff.rpos(buff.wpos());
        return;
    }

    if (!failedChecks.empty())
    {
        // Log every failure so audits aren't limited to the last failed id, then
        // apply the strictest penalty configured for any failed check.
        WardenCheck* worst = NULL;
        for (std::list<uint16>::iterator itr = failedChecks.begin(); itr != failedChecks.end(); ++itr)
        {
            WardenCheck* check = sWardenCheckMgr->GetWardenDataById(_session->GetClientBuild(), *itr);
            if (!check)
            {
                continue;
            }

            sLog.outWarden("%s failed Warden check %u (%s)",
                           _session->GetPlayerName(), *itr,
                           check->Comment.empty() ? "Undocumented Check" : check->Comment.c_str());
            LogPositiveToDB(check);

            if (!worst || check->Action > worst->Action)
            {
                worst = check;
            }
        }

        if (worst)
        {
            sLog.outWarden("%s Warden penalty action: %s",
                           _session->GetPlayerName(), Penalty(worst).c_str());
        }
    }

    Warden::HandleData(buff);
}
