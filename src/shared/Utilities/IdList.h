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

#ifndef MANGOS_H_IDLIST
#define MANGOS_H_IDLIST

// Parsing a comma-separated id list out of a config value. This lived as a pair of
// statics on the vmap library's factory, which had nothing to do with either config
// or ids and was reached only because it happened to be linked.

#include "Platform/Define.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace MaNGOS
{
    inline std::vector<uint32> ParseIdList(const std::string& text)
    {
        std::vector<uint32> ids;
        size_t pos = 0;
        while (pos <= text.size())
        {
            const size_t comma = text.find(',', pos);
            std::string token = text.substr(
                pos, comma == std::string::npos ? std::string::npos : comma - pos);

            const size_t first = token.find_first_not_of(" \t\r\n\"'");
            if (first != std::string::npos)
            {
                const size_t last = token.find_last_not_of(" \t\r\n\"'");
                token = token.substr(first, last - first + 1);
                const long id = std::strtol(token.c_str(), nullptr, 10);
                if (id > 0)
                {
                    ids.push_back(uint32(id));
                }
            }

            if (comma == std::string::npos)
            {
                break;
            }
            pos = comma + 1;
        }
        return ids;
    }
}

using MaNGOS::ParseIdList;

#endif
