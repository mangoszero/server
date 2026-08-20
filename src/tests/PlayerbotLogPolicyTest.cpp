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

#include "../modules/Bots/playerbot/PlayerbotLogPolicy.h"

#include <cstdarg>
#include <cstdio>
#include <string>

namespace
{
    std::string FormatPlayerbotLog(char const* format, ...)
    {
        va_list args;
        va_start(args, format);
        std::string const result = ai::FormatPlayerbotLogMessage(format, args);
        va_end(args);
        return result;
    }
}

TEST(PlayerbotLogPolicyBoundsFormattedMessages)
{
    std::string const payload(ai::PLAYERBOT_LOG_MESSAGE_CAPACITY * 2, 'x');
    std::string const actual = FormatPlayerbotLog("%s", payload.c_str());

    CHECK_EQ(actual.size(), ai::PLAYERBOT_LOG_MESSAGE_CAPACITY - 1);
    CHECK(actual == payload.substr(0, ai::PLAYERBOT_LOG_MESSAGE_CAPACITY - 1));
}

TEST(PlayerbotLogPolicyWritesPercentCharactersLiterally)
{
    char const* path = "playerbot_log_policy_test.tmp";
    std::remove(path);
    FILE* file = std::fopen(path, "w+");
    CHECK(file != NULL);
    if (!file)
    {
        return;
    }

    CHECK(ai::WritePlayerbotLogLine(file, "100% ready %s"));
    std::rewind(file);

    char buffer[64] = {};
    CHECK(std::fgets(buffer, sizeof(buffer), file) != NULL);
    CHECK(std::string(buffer) == "100% ready %s\n");

    std::fclose(file);
    CHECK_EQ(std::remove(path), 0);
}

TEST(PlayerbotLogPolicyRejectsMissingFile)
{
    CHECK(!ai::WritePlayerbotLogLine(NULL, "not written"));
}
