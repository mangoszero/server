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

#include <string>

enum PlayerbotSecurityLevel
{
    PLAYERBOT_SECURITY_DENY_ALL = 0,
    PLAYERBOT_SECURITY_TALK = 1,
    PLAYERBOT_SECURITY_INVITE = 2,
    PLAYERBOT_SECURITY_ALLOW_GROUP = 3,
    PLAYERBOT_SECURITY_ALLOW_ALL = 4
};

namespace ai
{
    inline PlayerbotSecurityLevel GetPlayerbotCommandSecurityLevel(std::string const& command)
    {
        // ExternalEventHelper separates a trigger from its parameters on spaces only. Keep
        // this boundary identical so other whitespace cannot gain a lower security tier.
        std::string::size_type end = command.find(' ');
        std::string const name = command.substr(0, end);

        if (name == "who")
        {
            return PLAYERBOT_SECURITY_TALK;
        }
        if (name == "follow" || name == "stay" || name == "attack")
        {
            // These are registered as single-word chat triggers whose remainder is a
            // parameter. A multi-word trigger with one of these prefixes would inherit this
            // tier and therefore requires an explicit security review before registration.
            return PLAYERBOT_SECURITY_ALLOW_GROUP;
        }
        return PLAYERBOT_SECURITY_ALLOW_ALL;
    }

    inline PlayerbotSecurityLevel GetPlayerbotDispatchedCommandSecurityLevel(
        std::string const& command, bool dispatchesRaidWarning)
    {
        // Named raid warnings are converted to the fixed "warning" trigger, which resets
        // the bot and enables its runaway strategy. Authorize that command, not its text.
        if (dispatchesRaidWarning)
        {
            return GetPlayerbotCommandSecurityLevel("warning");
        }
        return GetPlayerbotCommandSecurityLevel(command);
    }

    inline PlayerbotSecurityLevel GetPlayerOwnedBotSecurityLevel(bool isMaster, bool sameSubgroup)
    {
        if (isMaster)
        {
            return PLAYERBOT_SECURITY_ALLOW_ALL;
        }
        if (sameSubgroup)
        {
            return PLAYERBOT_SECURITY_ALLOW_GROUP;
        }
        return PLAYERBOT_SECURITY_TALK;
    }
}
