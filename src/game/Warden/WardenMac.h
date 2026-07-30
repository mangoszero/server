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

#ifndef _WARDEN_MAC_H
#define _WARDEN_MAC_H

#include "Warden.h"

class WorldSession;

/**
 * @brief Warden Mac class
 *
 * Platform-specific Warden implementation for Mac clients.
 */
class WardenMac : public Warden
{
    public:
        /**
         * @brief Constructor
         */
        WardenMac();

        /**
         * @brief Destructor
         */
        ~WardenMac();

        /**
         * @brief Initialize Warden
         * @param session World session
         * @param k Key
         */
        void Init(WorldSession* session, BigNumber* k) override;

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
        void HandleHashResult(ByteBuffer& buff) override;

        /**
         * @brief Request data
         */
        void RequestData() override;

        /**
         * @brief Handle data
         * @param buff Byte buffer
         */
        void HandleData(ByteBuffer& buff) override;
};

#endif
