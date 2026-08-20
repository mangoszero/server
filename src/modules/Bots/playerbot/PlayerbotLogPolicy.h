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

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <string>

namespace ai
{
    constexpr std::size_t PLAYERBOT_LOG_MESSAGE_CAPACITY = 1024;

    inline std::string FormatPlayerbotLogMessage(char const* format, va_list args)
    {
        if (!format)
        {
            return std::string();
        }

        char buffer[PLAYERBOT_LOG_MESSAGE_CAPACITY] = {};
        int const result = std::vsnprintf(buffer, sizeof(buffer), format, args);
        buffer[sizeof(buffer) - 1] = '\0';
        return result < 0 ? std::string() : std::string(buffer);
    }

    inline bool WritePlayerbotLogLine(FILE* file, std::string const& message)
    {
        if (!file || std::fputs(message.c_str(), file) == EOF)
        {
            return false;
        }

        return std::fputc('\n', file) != EOF;
    }
}
