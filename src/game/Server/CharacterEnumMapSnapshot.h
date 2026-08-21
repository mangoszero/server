/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOS_CHARACTER_ENUM_MAP_SNAPSHOT_H
#define MANGOS_CHARACTER_ENUM_MAP_SNAPSHOT_H

#include "Platform/Define.h"

#include <map>
#include <utility>

/**
 * Records the map id serialized for each character by the most recent
 * SMSG_CHAR_ENUM. Login compares against this wire-facing snapshot, rather
 * than a fresh database read, to detect movement since the screen was shown.
 */
class CharacterEnumMapSnapshot
{
    public:
        using MapByGuid = std::map<uint64, uint32>;

        void Replace(MapByGuid snapshot)
        {
            m_maps = std::move(snapshot);
        }

        bool Matches(uint64 guid, uint32 mapId) const
        {
            MapByGuid::const_iterator found = m_maps.find(guid);
            return found != m_maps.end() && found->second == mapId;
        }

    private:
        MapByGuid m_maps;
};

/**
 * One-shot gate for SMSG_LOGIN_VERIFY_WORLD. An unchanged character-screen
 * destination omits the initial packet, while an admission failure can still
 * claim the fallback send without risking a duplicate.
 */
class LoginVerifyDeliveryState
{
    public:
        bool TakeInitial(bool matchingEnumMap)
        {
            return matchingEnumMap ? false : Take();
        }

        bool TakeAdmissionFallback()
        {
            return Take();
        }

    private:
        bool Take()
        {
            if (m_sent)
            {
                return false;
            }

            m_sent = true;
            return true;
        }

        bool m_sent = false;
};

#endif
