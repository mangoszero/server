/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef _WARDEN_WIN_H
#define _WARDEN_WIN_H

#include <list>
#include "Warden.h"
#include <vector>

#if defined(__GNUC__)
#pragma pack(1)
#else
#pragma pack(push, 1)
#endif

/**
 * @brief Warden init module request structure
 */
struct WardenInitModuleRequest
{
    uint8 Command1; ///< Command 1
    uint16 Size1; ///< Size 1
    uint32 CheckSumm1; ///< Checksum 1
    uint8 Unk1; ///< Unknown 1
    uint8 Unk2; ///< Unknown 2
    uint8 Type; ///< Type
    uint8 String_library1; ///< String library 1
    uint32 Function1[4]; ///< Function 1 array

    uint8 Command2; ///< Command 2
    uint16 Size2; ///< Size 2
    uint32 CheckSumm2; ///< Checksum 2
    uint8 Unk3; ///< Unknown 3
    uint8 Unk4; ///< Unknown 4
    uint8 String_library2; ///< String library 2
    uint32 Function2; ///< Function 2
    uint8 Function2_set; ///< Function 2 set

    uint8 Command3; ///< Command 3
    uint16 Size3; ///< Size 3
    uint32 CheckSumm3; ///< Checksum 3
    uint8 Unk5; ///< Unknown 5
    uint8 Unk6; ///< Unknown 6
    uint8 String_library3; ///< String library 3
    uint32 Function3; ///< Function 3
    uint8 Function3_set; ///< Function 3 set
};

#if defined(__GNUC__)
#pragma pack()
#else
#pragma pack(pop)
#endif

class WorldSession;

/**
 * @brief Warden Windows class
 *
 * Platform-specific Warden implementation for Windows clients.
 */
class WardenWin : public Warden
{
    public:
        /**
         * @brief Constructor
         */
        WardenWin();

        /**
         * @brief Destructor
         */
        ~WardenWin();

        /**
         * @brief Initialize Warden
         * @param session World session
         * @param K Key
         */
        void Init(WorldSession* session, BigNumber* K) override;

        /**
         * @brief Get module for client
         * @return Client warden module
         */
        ClientWardenModule* GetModuleForClient() override;

        /**
         * @brief Initialize module
         */
        void InitializeModule() override;

        /**
         * @brief Handle hash result
         * @param buff Byte buffer
         */
        void HandleHashResult(ByteBuffer &buff) override;

        /**
         * @brief Request data
         */
        void RequestData() override;

        /**
         * @brief Handle data
         * @param buff Byte buffer
         */
        void HandleData(ByteBuffer &buff) override;

    private:
        struct PointerChainState
        {
            uint16 checkId;
            std::vector<uint32> offsets;
            size_t hopIndex;
            uint32 currentAddress;
            uint8  finalLength;
            bool   invertMatch;     ///< true = fail when terminal bytes MATCH expected (signature detect)
        };

        static bool ParseChainOffsets(const std::string& str, std::vector<uint32>& out);
        void StartPointerChain(WardenCheck* wd);

        uint32 _serverTicks; ///< Server ticks
        std::list<uint16> _otherChecksTodo; ///< Other checks to do
        std::list<uint16> _memChecksTodo; ///< Memory checks to do
        std::list<uint16> _currentChecks; ///< Current checks
        PointerChainState _pointerChainInFlight; ///< in-flight pointer-chain walk state
        bool _pointerChainActive; ///< whether a pointer-chain walk is in progress
};

#endif
