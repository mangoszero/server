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

#pragma once

#include "SharedDefines.h"

#include <map>
#include <vector>

namespace ai
{
    inline std::vector<uint8> const* FindRandomBotRaceCandidates(
        std::map<uint8, std::vector<uint8> > const& candidates, uint8 cls)
    {
        std::map<uint8, std::vector<uint8> >::const_iterator itr = candidates.find(cls);
        if (itr == candidates.end() || itr->second.empty())
        {
            return nullptr;
        }
        return &itr->second;
    }

    inline std::vector<uint8> GetMissingRandomBotClasses(std::vector<uint8> const& existing)
    {
        bool present[MAX_CLASSES] = {};
        for (std::vector<uint8>::const_iterator itr = existing.begin(); itr != existing.end(); ++itr)
        {
            if (*itr < MAX_CLASSES)
            {
                present[*itr] = true;
            }
        }

        uint8 const playable[] = {
            CLASS_WARRIOR,
            CLASS_PALADIN,
            CLASS_HUNTER,
            CLASS_ROGUE,
            CLASS_PRIEST,
            CLASS_SHAMAN,
            CLASS_MAGE,
            CLASS_WARLOCK,
            CLASS_DRUID
        };

        std::vector<uint8> missing;
        for (uint8 cls : playable)
        {
            if (!present[cls])
            {
                missing.push_back(cls);
            }
        }
        return missing;
    }
}
