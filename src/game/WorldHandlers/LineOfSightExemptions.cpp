/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include <unordered_set>
#include <string>
#include "LineOfSightExemptions.h"

#include <cstdlib>

namespace
{
    std::unordered_set<uint32> g_exempt;

    std::string Trim(const std::string& in)
    {
        size_t first = in.find_first_not_of(" \t\r\n\"'");
        if (first == std::string::npos)
        {
            return std::string();
        }
        const size_t last = in.find_last_not_of(" \t\r\n\"'");
        return in.substr(first, last - first + 1);
    }
}

void LineOfSightExemptions::Load(const std::string& spellIds)
{
    g_exempt.clear();

    size_t pos = 0;
    while (pos <= spellIds.size())
    {
        const size_t comma = spellIds.find(',', pos);
        const std::string token =
            Trim(spellIds.substr(pos, comma == std::string::npos ? std::string::npos
                                                                 : comma - pos));
        if (!token.empty())
        {
            const long id = std::strtol(token.c_str(), nullptr, 10);
            if (id > 0)
            {
                g_exempt.insert(uint32(id));
            }
        }
        if (comma == std::string::npos)
        {
            break;
        }
        pos = comma + 1;
    }
}

bool LineOfSightExemptions::Has(uint32 spellId)
{
    return g_exempt.find(spellId) != g_exempt.end();
}

void LineOfSightExemptions::Clear()
{
    g_exempt.clear();
}
