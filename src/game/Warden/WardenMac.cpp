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

/**
 * @file WardenMac.cpp
 * @brief macOS-specific Warden implementation
 *
 * This file implements WardenMac which provides the macOS-specific
 * implementation of the Warden anti-cheat system for Mac clients.
 *
 * Key differences from Windows:
 * - Different module binary
 * - Different seed value
 * - MD5-based module verification
 *
 * @see WardenMac for the Mac-specific class
 * @see Warden for the base class
 */

#include "WardenKeyGeneration.h"
#include "Common.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Log.h"
#include "Opcodes.h"
#include "ByteBuffer.h"
#include <openssl/evp.h>
#include "World.h"
#include "Player.h"
#include "Util.h"
#include "WardenMac.h"
#include "WardenModuleMac.h"
#include "GameTime.h"

/**
 * @brief WardenMac constructor
 *
 * Initializes the Mac-specific Warden implementation.
 */
WardenMac::WardenMac() : Warden() {}

/**
 * @brief WardenMac destructor
 */
WardenMac::~WardenMac() {}

/**
 * @brief Initialize Mac Warden
 * @param pClient Client session
 * @param K Session key
 *
 * Initializes the Mac-specific Warden with:
 * - SHA1-based key generation from session key
 * - Fixed Mac-specific seed
 * - Mac-specific module loading
 */
void WardenMac::Init(WorldSession* pClient, BigNumber* K)
{
    _session = pClient;
    // Generate Warden Key
    SHA1Randx WK(K->AsByteArray(), K->GetNumBytes());
    WK.Generate(_inputKey, 16);
    WK.Generate(_outputKey, 16);
    /**
     * Seed: 4D808D2C77D905C41A6380EC08586AFE (0x05 packet)
     * Hash: <?> (0x04 packet)
     * Module MD5: 0DBBF209A27B1E279A9FEC5C168A15F7
     * New Client Key: <?>
     * New Cerver Key: <?>
     */
    uint8 mod_seed[16] = { 0x4D, 0x80, 0x8D, 0x2C, 0x77, 0xD9, 0x05, 0xC4, 0x1A, 0x63, 0x80, 0xEC, 0x08, 0x58, 0x6A, 0xFE };

    memcpy(_seed, mod_seed, 16);

    _inputCrypto.Init(_inputKey);
    _outputCrypto.Init(_outputKey);
    sLog.outWarden("Server side Mac warden for client %u (build %u) initializing...", pClient->GetAccountId(), _session->GetClientBuild());
    sLog.outWarden("C->S Key: %s", ByteArrayToHexStr(_inputKey, 16).c_str());
    sLog.outWarden("S->C Key: %s", ByteArrayToHexStr(_outputKey, 16).c_str());
    sLog.outWarden("  Seed: %s", ByteArrayToHexStr(_seed, 16).c_str());
    sLog.outWarden("Loading Module...");

    _module = GetModuleForClient();

    sLog.outWarden("Module Key: %s", ByteArrayToHexStr(_module->Key, 16).c_str());
    sLog.outWarden("Module ID: %s", ByteArrayToHexStr(_module->Id, 16).c_str());
    RequestModule();
}

/**
 * @brief Get Mac Warden module
 * @return Client Warden module
 *
 * Returns the Mac-specific Warden module with its
 * compressed data, key, and MD5 hash.
 */
ClientWardenModule* WardenMac::GetModuleForClient()
{
    ClientWardenModule *mod = new ClientWardenModule;

    uint32 len = sizeof(Module_0DBBF209A27B1E279A9FEC5C168A15F7_Data);

    // data assign
    mod->CompressedSize = len;
    mod->CompressedData = new uint8[len];
    memcpy(mod->CompressedData, Module_0DBBF209A27B1E279A9FEC5C168A15F7_Data, len);
    memcpy(mod->Key, Module_0DBBF209A27B1E279A9FEC5C168A15F7_Key, 16);

    // md5 hash
    unsigned int digestLen = 0;
    EVP_Digest(mod->CompressedData, len, (uint8*)&mod->Id, &digestLen, EVP_md5(), NULL);

    return mod;
}

/**
 * @brief Initialize Mac Warden module
 *
 * Initializes the Mac-specific Warden module on the client.
 */
void WardenMac::InitializeModule()
{
    sLog.outWarden("Initialize module");
    Warden::InitializeModule();
}

struct keyData {
    union
    {
        struct
        {
            uint8 bytes[16];
        } bytes;

        struct
        {
            int ints[4];
        } ints;
    };
};

/**
 * @brief Validates the Mac client hash reply and initializes the session crypto keys.
 *
 * @param buff The received Warden payload buffer.
 */
void WardenMac::HandleHashResult(ByteBuffer &buff)
{

    // test
    int keyIn[4];

    keyData mod_seed = { { { { 0x4D, 0x80, 0x8D, 0x2C, 0x77, 0xD9, 0x05, 0xC4, 0x1A, 0x63, 0x80, 0xEC, 0x08, 0x58, 0x6A, 0xFE } } } };

    for (int i = 0; i < 4; ++i)
    {
        keyIn[i] = mod_seed.ints.ints[i];
    }

    int keyOut[4];
    int keyIn1, keyIn2;
    keyOut[0] = keyIn[0];
    keyIn[0] ^= 0xDEADBEEFu;
    keyIn1 = keyIn[1];
    keyIn[1] -= 0x35014542u;
    keyIn2 = keyIn[2];
    keyIn[2] += 0x5313F22u;
    keyIn[3] *= 0x1337F00Du;
    keyOut[1] = keyIn1 - 0x6A028A84;
    keyOut[2] = keyIn2 + 0xA627E44;
    keyOut[3] = 0x1337F00D * keyIn[3];
    // end test

    buff.rpos(buff.wpos());

    Sha1Hash sha1;
    sha1.UpdateData((uint8*)keyIn, 16);
    sha1.Finalize();

    // Verify key
    if (memcmp(buff.contents() + 1, sha1.GetDigest(), 20) != 0)
    {
        sLog.outWarden("%s failed hash reply. Action: %s", _session->GetPlayerName(), Penalty().c_str());
        if (sWorld.getConfig(CONFIG_UINT32_WARDEN_CLIENT_FAIL_ACTION) > uint32(WARDEN_ACTION_LOG))
        {
            _session->KickPlayer();
        }
        return;
    }

    sLog.outWarden("Request hash reply: succeed");

    // change keys here
    memcpy(_inputKey, keyIn, 16);
    memcpy(_outputKey, keyOut, 16);

    _inputCrypto.Init(_inputKey);
    _outputCrypto.Init(_outputKey);

    _previousTimestamp = GameTime::GetGameTimeMS();
}

/**
 * @brief Sends a Mac Warden data request packet to the client.
 */
void WardenMac::RequestData()
{
    sLog.outWarden("Request data");

    ByteBuffer buff;
    buff << uint8(WARDEN_SMSG_CHEAT_CHECKS_REQUEST);

    std::string str = "Test string!";

    buff << uint8(str.size());
    buff.append(str.c_str(), str.size());

    buff.hexlike();

    // Encrypt with warden RC4 key.
    EncryptData(const_cast<uint8*>(buff.contents()), buff.size());

    WorldPacket pkt(SMSG_WARDEN_DATA, buff.size());
    pkt.append(buff);
    _session->SendPacket(&pkt);

    Warden::RequestData();
}

/**
 * @brief Processes a Mac Warden data response from the client.
 *
 * @param buff The received Warden payload buffer.
 */
void WardenMac::HandleData(ByteBuffer &buff)
{
    sLog.outWarden("Handle data");

    bool found = false;

    std::string str = "Test string!";

    Sha1Hash sha1;
    sha1.UpdateData(str);
    uint32 magic = 0xFEEDFACE;                              // unsure
    sha1.UpdateData((uint8*)&magic, 4);
    sha1.Finalize();

    uint8 sha1Hash[20];
    buff.read(sha1Hash, 20);

    if (memcmp(sha1Hash, sha1.GetDigest(), 20) != 0)
    {
        sLog.outWarden("Handle data failed: SHA1 hash is wrong!");
        found = true;
    }

    uint8 ourMD5Hash[16];
    unsigned int ourMD5HashLen = 0;
    EVP_Digest(str.c_str(), str.size(), ourMD5Hash, &ourMD5HashLen, EVP_md5(), NULL);

    uint8 theirsMD5Hash[16];
    buff.read(theirsMD5Hash, 16);

    if (memcmp(ourMD5Hash, theirsMD5Hash, 16) != 0)
    {
        sLog.outWarden("Handle data failed: MD5 hash is wrong!");
        found = true;
    }

    if (found && sWorld.getConfig(CONFIG_UINT32_WARDEN_CLIENT_FAIL_ACTION) > uint32(WARDEN_ACTION_LOG))
    {
        _session->KickPlayer();
    }
    else
    {
        sLog.outWarden("SHA1 and MD5 hash verified. Handle data passed.");
    }

    Warden::HandleData(buff);
}
