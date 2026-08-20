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
 */

#include "TestHarness.h"

#include "../modules/Bots/playerbot/PlayerbotSecurityPolicy.h"

using ai::GetPlayerbotCommandSecurityLevel;
using ai::GetPlayerbotDispatchedCommandSecurityLevel;
using ai::GetPlayerOwnedBotSecurityLevel;

TEST(PlayerbotSecurityPolicyWhoNeedsOnlyTalkPermission)
{
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("who"), PLAYERBOT_SECURITY_TALK);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("who gear"), PLAYERBOT_SECURITY_TALK);
}

TEST(PlayerbotSecurityPolicyGroupTacticsNeedGroupPermission)
{
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("follow"), PLAYERBOT_SECURITY_ALLOW_GROUP);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("follow master"), PLAYERBOT_SECURITY_ALLOW_GROUP);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("stay"), PLAYERBOT_SECURITY_ALLOW_GROUP);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("attack my target"), PLAYERBOT_SECURITY_ALLOW_GROUP);
}

TEST(PlayerbotSecurityPolicyOwnerPermissionProtectsDestructiveAndPersistentCommands)
{
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("destroy [item]"), PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("sell"), PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("teleport"), PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("co +passive"), PLAYERBOT_SECURITY_ALLOW_ALL);
}

TEST(PlayerbotSecurityPolicyDoesNotAuthorizeCommandPrefixes)
{
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("whoever"), PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("attackers"), PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("follow-up"), PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotCommandSecurityLevel("attack\tsell"), PLAYERBOT_SECURITY_ALLOW_ALL);
}

TEST(PlayerbotSecurityPolicyPermissionOrderingKeepsGroupBelowOwner)
{
    CHECK(PLAYERBOT_SECURITY_INVITE < PLAYERBOT_SECURITY_ALLOW_GROUP);
    CHECK(PLAYERBOT_SECURITY_ALLOW_GROUP < PLAYERBOT_SECURITY_ALLOW_ALL);
}

TEST(PlayerbotSecurityPolicyComposesActorAndCommandPermissions)
{
    struct Actor
    {
        bool isMaster;
        bool sameSubgroup;
        bool canWho;
        bool canFollow;
        bool canSell;
    };

    Actor const actors[] =
    {
        {true, false, true, true, true},
        {false, true, true, true, false},
        {false, false, true, false, false}
    };

    for (Actor const& actor : actors)
    {
        PlayerbotSecurityLevel actual = GetPlayerOwnedBotSecurityLevel(actor.isMaster, actor.sameSubgroup);
        CHECK_EQ(actual >= GetPlayerbotCommandSecurityLevel("who"), actor.canWho);
        CHECK_EQ(actual >= GetPlayerbotCommandSecurityLevel("follow"), actor.canFollow);
        CHECK_EQ(actual >= GetPlayerbotCommandSecurityLevel("sell"), actor.canSell);
    }
}

TEST(PlayerbotSecurityPolicyAuthorizesTheRaidWarningCommandThatWillRun)
{
    CHECK_EQ(GetPlayerbotDispatchedCommandSecurityLevel("follow Bot", false),
             PLAYERBOT_SECURITY_ALLOW_GROUP);
    CHECK_EQ(GetPlayerbotDispatchedCommandSecurityLevel("follow Bot", true),
             PLAYERBOT_SECURITY_ALLOW_ALL);
    CHECK_EQ(GetPlayerbotDispatchedCommandSecurityLevel("who Bot", true),
             PLAYERBOT_SECURITY_ALLOW_ALL);
}

TEST(PlayerbotSecurityPolicyOwnerKeepsFullPermission)
{
    CHECK_EQ(GetPlayerOwnedBotSecurityLevel(true, false), PLAYERBOT_SECURITY_ALLOW_ALL);
}

TEST(PlayerbotSecurityPolicySameSubgroupGetsOnlyGroupPermission)
{
    CHECK_EQ(GetPlayerOwnedBotSecurityLevel(false, true), PLAYERBOT_SECURITY_ALLOW_GROUP);
}

TEST(PlayerbotSecurityPolicyDifferentSubgroupGetsOnlyTalkPermission)
{
    CHECK_EQ(GetPlayerOwnedBotSecurityLevel(false, false), PLAYERBOT_SECURITY_TALK);
}
