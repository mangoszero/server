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

#include "TestHarness.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// No argument runs everything, which is what CI does and what the ctest entry does.
// -only/-skip are for iterating: the network and crypto stress cases cost minutes and
// have nothing to say about a change elsewhere.
static void Usage()
{
    std::printf("usage: mangos_tests [-only <substr>]... [-skip <substr>]...\n");
}

int main(int argc, char** argv)
{
    std::vector<std::string> only;
    std::vector<std::string> skip;

    for (int i = 1; i < argc; ++i)
    {
        const bool hasValue = (i + 1 < argc);
        if (std::strcmp(argv[i], "-only") == 0 && hasValue)
        {
            only.push_back(argv[++i]);
        }
        else if (std::strcmp(argv[i], "-skip") == 0 && hasValue)
        {
            skip.push_back(argv[++i]);
        }
        else
        {
            Usage();
            return 2;
        }
    }

    std::printf("mangos unit tests\n\n");
    return testing::RunAll(only, skip);
}
