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

#include "../modules/Bots/playerbot/PlayerbotPacketPolicy.h"

#include <map>
#include <string>

namespace
{
    struct TestSession
    {
        void HandleTestOpcode()
        {
        }
    };

    struct TestOpcodeHandler
    {
        int status;
        void (TestSession::*handler)();
    };
}

TEST(PlayerbotPacketPolicyLookupDoesNotInsertUnknownOpcodes)
{
    std::map<unsigned int, std::string> handlers;
    handlers[7] = "known";
    std::map<unsigned int, std::string> const& readOnlyHandlers = handlers;

    std::string const* known = ai::FindPlayerbotEventHandler(readOnlyHandlers, 7u);
    std::string const* unknown = ai::FindPlayerbotEventHandler(readOnlyHandlers, 8u);

    CHECK(known != NULL);
    CHECK(known && *known == "known");
    CHECK(unknown == NULL);
    CHECK_EQ(handlers.size(), 1u);
}

TEST(PlayerbotPacketPolicyRejectsUnsafeOpcodeHandlers)
{
    int const loggedIn = 1;
    TestOpcodeHandler handlers[] =
    {
        {loggedIn, &TestSession::HandleTestOpcode},
        {2, &TestSession::HandleTestOpcode},
        {loggedIn, NULL}
    };

    CHECK(ai::FindDispatchablePlayerbotOpcodeHandler(
        handlers, 0u, loggedIn) == &handlers[0]);
    CHECK(ai::FindDispatchablePlayerbotOpcodeHandler(handlers, 1u, loggedIn) == NULL);
    CHECK(ai::FindDispatchablePlayerbotOpcodeHandler(handlers, 2u, loggedIn) == NULL);
    CHECK(ai::FindDispatchablePlayerbotOpcodeHandler(handlers, 3u, loggedIn) == NULL);
}
