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

#ifndef _WARDEN_BASE_H
#define _WARDEN_BASE_H

#include <map>
#include "ARC4.h"
#include "BigNumber.h"
#include "ByteBuffer.h"
#include "WardenCheckMgr.h"
#include "Database/DatabaseEnv.h"

// the default client version with info in warden_checks; for other version checks, see warden_build_specific
#define DEFAULT_CLIENT_BUILD  5875

/**
 * @brief Warden opcode enumeration
 */
enum WardenOpcodes
{
    // Client->Server
    WARDEN_CMSG_MODULE_MISSING = 0,      ///< Module missing
    WARDEN_CMSG_MODULE_OK = 1,           ///< Module OK
    WARDEN_CMSG_CHEAT_CHECKS_RESULT = 2, ///< Cheat checks result
    WARDEN_CMSG_MEM_CHECKS_RESULT = 3,   ///< Memory checks result (only sent if MEM_CHECK bytes doesn't match)
    WARDEN_CMSG_HASH_RESULT = 4,         ///< Hash result
    WARDEN_CMSG_MODULE_FAILED = 5,       ///< Module failed (sent when client failed to load uploaded module due to cache fail)

    // Server->Client
    WARDEN_SMSG_MODULE_USE = 0,           ///< Use module
    WARDEN_SMSG_MODULE_CACHE = 1,         ///< Module cache
    WARDEN_SMSG_CHEAT_CHECKS_REQUEST = 2, ///< Cheat checks request
    WARDEN_SMSG_MODULE_INITIALIZE = 3,    ///< Initialize module
    WARDEN_SMSG_MEM_CHECKS_REQUEST = 4,   ///< Memory checks request (byte len; while (!EOF) { byte unk(1); byte index(++); string module(can be 0); int offset; byte len; byte[] bytes_to_compare[len]; })
    WARDEN_SMSG_HASH_REQUEST = 5          ///< Hash request
};

/**
 * @brief Warden check type enumeration
 */
enum WardenCheckType
{
    MEM_CHECK = 0xF3,      ///< Memory check (243: byte moduleNameIndex + uint Offset + byte Len - check to ensure memory isn't modified)
    PAGE_CHECK_A = 0xB2,   ///< Page check A (178: uint Seed + byte[20] SHA1 + uint Addr + byte Len - scans all pages for specified hash)
    PAGE_CHECK_B = 0xBF,   ///< Page check B (191: uint Seed + byte[20] SHA1 + uint Addr + byte Len - scans only pages starts with MZ+PE headers for specified hash)
    MPQ_CHECK = 0x98,      ///< MPQ check (152: byte fileNameIndex - check to ensure MPQ file isn't modified)
    LUA_STR_CHECK = 0x8B,  ///< Lua string check (139: byte luaNameIndex - check to ensure LUA string isn't used)
    DRIVER_CHECK = 0x71,   ///< Driver check (113: uint Seed + byte[20] SHA1 + byte driverNameIndex - check to ensure driver isn't loaded)
    TIMING_CHECK = 0x57,   ///< Timing check (87: empty - check to ensure GetTickCount() isn't detoured)
    PROC_CHECK = 0x7E,     ///< Procedure check (126: uint Seed + byte[20] SHA1 + byte moluleNameIndex + byte procNameIndex + uint Offset + byte Len - check to ensure proc isn't detoured)
    MODULE_CHECK = 0xD9    ///< Module check (217: uint Seed + byte[20] SHA1 - check to ensure module isn't injected)
};

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

/**
 * @brief Warden module use structure
 */
struct WardenModuleUse
{
    uint8 Command; ///< Command
    uint8 ModuleId[16]; ///< Module ID
    uint8 ModuleKey[16]; ///< Module key
    uint32 Size; ///< Size
};

/**
 * @brief Warden module transfer structure
 */
struct WardenModuleTransfer
{
    uint8 Command; ///< Command
    uint16 DataSize; ///< Data size
    uint8 Data[500]; ///< Data
};

/**
 * @brief Warden hash request structure
 */
struct WardenHashRequest
{
    uint8 Command; ///< Command
    uint8 Seed[16]; ///< Seed
};

namespace WardenState
{
    enum Value
    {
        STATE_INITIAL,
        STATE_REQUESTED_MODULE,
        STATE_SENT_MODULE,
        STATE_REQUESTED_HASH,
        STATE_INITIALIZE_MODULE,
        STATE_REQUESTED_DATA,
        STATE_RESTING
    };

    inline const char* to_string(WardenState::Value value)
    {
        switch (value)
        {
            case WardenState::STATE_INITIAL:
                return "STATE_INITIAL";
            case WardenState::STATE_REQUESTED_MODULE:
                return "STATE_REQUESTED_MODULE";
            case WardenState::STATE_SENT_MODULE:
                return "STATE_SENT_MODULE";
            case WardenState::STATE_REQUESTED_HASH:
                return "STATE_REQUESTED_HASH";
            case WardenState::STATE_INITIALIZE_MODULE:
                return "STATE_INITIALIZE_MODULE";
            case WardenState::STATE_REQUESTED_DATA:
                return "STATE_REQUESTED_DATA";
            case WardenState::STATE_RESTING:
                return "STATE_RESTING";
        }
        return "UNDEFINED STATE";
    }
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

struct ClientWardenModule
{
    uint8 Id[16];
    uint8 Key[16];
    uint32 CompressedSize;
    uint8* CompressedData;
};

class WorldSession;

class Warden
{
    public:
        Warden();
        virtual ~Warden();

        virtual void Init(WorldSession* session, BigNumber* k) = 0;
        virtual ClientWardenModule* GetModuleForClient() = 0;
        virtual void InitializeModule();
        virtual void RequestHash();
        virtual void HandleHashResult(ByteBuffer &buff) = 0;
        virtual void RequestData();
        virtual void HandleData(ByteBuffer &buff);

        void SendModuleToClient();
        void RequestModule();
        void Update();
        void DecryptData(uint8* buffer, uint32 length);
        void EncryptData(uint8* buffer, uint32 length);

    void SetNewState(WardenState::Value state);

        static bool IsValidCheckSum(uint32 checksum, const uint8 *data, const uint16 length);
        static uint32 BuildChecksum(const uint8 *data, uint32 length);

        // If no check is passed, the default action from config is executed
        std::string Penalty(WardenCheck* check = NULL);

    protected:
        void LogPositiveToDB(WardenCheck* check);

        WorldSession* _session;
        uint8 _inputKey[16];
        uint8 _outputKey[16];
        uint8 _seed[16];
        ARC4 _inputCrypto;
        ARC4 _outputCrypto;
        uint32 _checkTimer;                          // Timer for sending check requests
        uint32 _clientResponseTimer;                 // Timer for client response delay
        uint32 _previousTimestamp;
        ClientWardenModule* _module;
        WardenState::Value _state;
};

#endif
