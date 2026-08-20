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

#include "Process/Process.h"

namespace Process
{

namespace
{

/**
 * @brief ASCII case-insensitive equality.
 *
 * Deliberately ASCII-only and locale-free: these are four fixed keywords typed
 * on a command line, and std::tolower under a Turkish locale would stop
 * matching "install". The cast through unsigned char is what keeps a negative
 * char out of the arithmetic.
 */
bool EqualsIgnoreCaseAscii(std::string_view left, std::string_view right)
{
    if (left.size() != right.size())
    {
        return false;
    }

    for (std::size_t i = 0; i < left.size(); ++i)
    {
        unsigned char a = static_cast<unsigned char>(left[i]);
        unsigned char b = static_cast<unsigned char>(right[i]);

        if (a >= 'A' && a <= 'Z')
        {
            a = static_cast<unsigned char>(a - 'A' + 'a');
        }

        if (b >= 'A' && b <= 'Z')
        {
            b = static_cast<unsigned char>(b - 'A' + 'a');
        }

        if (a != b)
        {
            return false;
        }
    }

    return true;
}

} // namespace

ServiceAction ParseServiceAction(std::string_view word)
{
    // Case-insensitive: these are typed on a command line, and "-s Install"
    // failing silently because of the capital is not a lesson worth teaching.
    if (EqualsIgnoreCaseAscii(word, "install"))
    {
        return ServiceAction::Install;
    }

    if (EqualsIgnoreCaseAscii(word, "uninstall"))
    {
        return ServiceAction::Uninstall;
    }

    if (EqualsIgnoreCaseAscii(word, "run"))
    {
        return ServiceAction::Run;
    }

    if (EqualsIgnoreCaseAscii(word, "stop"))
    {
        return ServiceAction::Stop;
    }

    return ServiceAction::None;
}

} // namespace Process
