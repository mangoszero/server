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

/**
 * @file ConfigTest.cpp
 * @brief Config key lookup, in particular that it ignores case.
 *
 * Config keys have never been consistently cased. The core reads `RA.Secure`
 * and `GMLogFile` while every shipped config since Release 20 writes
 * `Ra.Secure` and `GmLogFile`. That resolved for years and stopped resolving
 * when the backend became an exact-match std::map, which silently disabled the
 * RA security controls and the GM command audit log on upgraded servers.
 * These cases pin the tolerant behaviour so it cannot regress again.
 */

#include "TestHarness.h"

#include "Config/Config.h"

#include <cstdio>
#include <fstream>
#include <string>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace
{
    /// A config file that deletes itself, so a failing case leaves no litter.
    class TempConfig
    {
        public:
            explicit TempConfig(const std::string& body)
                : m_path(MakePath())
            {
                std::ofstream out(m_path.c_str());
                out << body;
            }

            ~TempConfig()
            {
                std::remove(m_path.c_str());
            }

            const char* Path() const { return m_path.c_str(); }

        private:
            /// Unique per instance AND per process: a build machine may run
            /// the suite twice at once in one directory, and a bare counter
            /// would have both runs truncate and delete the same file.
            static std::string MakePath()
            {
                static int counter = 0;
                return "mangos_config_test_"
                       + std::to_string(CurrentPid()) + "_"
                       + std::to_string(++counter) + ".conf";
            }

            static unsigned long CurrentPid()
            {
#ifdef _WIN32
                return (unsigned long)::GetCurrentProcessId();
#else
                return (unsigned long)::getpid();
#endif
            }

            std::string m_path;
    };
}

/// The exact pairs that broke in the field: the conf writes one case and the
/// core reads another.
TEST(ConfigLookupIgnoresCase)
{
    TempConfig file(
        "[MangosdConf]\n"
        "Ra.Secure      = 1\n"
        "Ra.Stricted    = 1\n"
        "Ra.MinLevel    = 3\n"
        "GmLogFile      = \"world-gamemaster.log\"\n");

    Config config;
    REQUIRE(config.SetSource(file.Path()));

    // Read with the spelling the core actually uses.
    CHECK(config.GetBoolDefault("RA.Secure", false));
    CHECK(config.GetBoolDefault("RA.Stricted", false));
    CHECK_EQ(config.GetIntDefault("RA.MinLevel", 0), 3);
    CHECK_STR(config.GetStringDefault("GMLogFile", ""), "world-gamemaster.log");
}

/// The spelling as written must keep working; this is a widening, not a move.
TEST(ConfigLookupAcceptsOriginalCase)
{
    TempConfig file(
        "[MangosdConf]\n"
        "Ra.Enable      = 1\n"
        "GmLogTimestamp = 1\n");

    Config config;
    REQUIRE(config.SetSource(file.Path()));

    CHECK(config.GetBoolDefault("Ra.Enable", false));
    CHECK(config.GetBoolDefault("GmLogTimestamp", false));
}

/// Any casing resolves, not merely the two the shipped files happen to use.
TEST(ConfigLookupIgnoresCaseBothWays)
{
    TempConfig file(
        "[MangosdConf]\n"
        "LOUDKEY  = 7\n"
        "quietkey = 9\n");

    Config config;
    REQUIRE(config.SetSource(file.Path()));

    CHECK_EQ(config.GetIntDefault("loudkey", 0), 7);
    CHECK_EQ(config.GetIntDefault("LoudKey", 0), 7);
    CHECK_EQ(config.GetIntDefault("QUIETKEY", 0), 9);
    CHECK_EQ(config.GetIntDefault("QuietKey", 0), 9);
}

/// Two spellings of one key in one section are one setting, and the later
/// value wins -- the same rule an exact-duplicate line has always followed.
TEST(ConfigLookupCollapsesCaseDuplicatesLastWins)
{
    TempConfig file(
        "[MangosdConf]\n"
        "Duplicated = 1\n"
        "DUPLICATED = 2\n");

    Config config;
    REQUIRE(config.SetSource(file.Path()));

    CHECK_EQ(config.GetIntDefault("duplicated", 0), 2);
    CHECK_EQ(config.GetIntDefault("Duplicated", 0), 2);
    CHECK_EQ(config.GetIntDefault("DUPLICATED", 0), 2);
}

/// Across sections the first section still wins, now including a spelling that
/// matches only once case is folded away.
TEST(ConfigLookupFirstSectionWinsAcrossCase)
{
    TempConfig file(
        "[First]\n"
        "Shared = 1\n"
        "[Second]\n"
        "SHARED = 2\n");

    Config config;
    REQUIRE(config.SetSource(file.Path()));

    CHECK_EQ(config.GetIntDefault("SHARED", 0), 1);
    CHECK_EQ(config.GetIntDefault("shared", 0), 1);
}

/// Case folding must not make unrelated keys collide, and a genuinely absent
/// key must still fall back to its default rather than matching a near miss.
TEST(ConfigLookupKeepsDistinctKeysDistinct)
{
    TempConfig file(
        "[MangosdConf]\n"
        "Log.Level  = 3\n"
        "Log.Levels = 4\n");

    Config config;
    REQUIRE(config.SetSource(file.Path()));

    CHECK_EQ(config.GetIntDefault("log.level", 0), 3);
    CHECK_EQ(config.GetIntDefault("LOG.LEVELS", 0), 4);
    CHECK_EQ(config.GetIntDefault("Log.Lev", 42), 42);
    CHECK_EQ(config.GetIntDefault("CompletelyAbsent", 11), 11);
}
