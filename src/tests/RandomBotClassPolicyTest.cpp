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

#include "../modules/Bots/playerbot/RandomBotClassPolicy.h"

#include <map>

using ai::GetMissingRandomBotClasses;
using ai::FindRandomBotRaceCandidates;

namespace
{
    void CheckClasses(std::vector<uint8> const& actual,
                      std::initializer_list<uint8> expected)
    {
        CHECK_EQ(actual.size(), expected.size());

        size_t index = 0;
        for (uint8 cls : expected)
        {
            if (index < actual.size())
            {
                CHECK_EQ(actual[index], cls);
            }
            ++index;
        }
    }
}

TEST(RandomBotClassPolicyEmptyAccountNeedsEveryPlayableClassicClass)
{
    CheckClasses(GetMissingRandomBotClasses({}),
                 {CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,
                  CLASS_PRIEST, CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK,
                  CLASS_DRUID});
}

TEST(RandomBotClassPolicyPartialAccountNeedsOnlyItsMissingClass)
{
    CheckClasses(GetMissingRandomBotClasses(
                    {CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,
                     CLASS_PRIEST, CLASS_SHAMAN, CLASS_MAGE, CLASS_DRUID}),
                 {CLASS_WARLOCK});
}

TEST(RandomBotClassPolicyDuplicatesCannotHideAMissingClass)
{
    CheckClasses(GetMissingRandomBotClasses(
                    {CLASS_WARRIOR, CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER,
                     CLASS_ROGUE, CLASS_PRIEST, CLASS_SHAMAN, CLASS_MAGE,
                     CLASS_WARLOCK}),
                 {CLASS_DRUID});
}

TEST(RandomBotClassPolicyIgnoresNonPlayableClassIds)
{
    CheckClasses(GetMissingRandomBotClasses(
                    {0, CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,
                     CLASS_PRIEST, 6, CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK,
                     10, CLASS_DRUID, 255}),
                 {});
}

TEST(RandomBotClassPolicyCompleteAccountNeedsNothing)
{
    CheckClasses(GetMissingRandomBotClasses(
                    {CLASS_WARRIOR, CLASS_PALADIN, CLASS_HUNTER, CLASS_ROGUE,
                     CLASS_PRIEST, CLASS_SHAMAN, CLASS_MAGE, CLASS_WARLOCK,
                     CLASS_DRUID}),
                 {});
}

TEST(RandomBotClassPolicyRejectsMissingAndEmptyRaceCandidates)
{
    std::map<uint8, std::vector<uint8> > candidates;

    CHECK(FindRandomBotRaceCandidates(candidates, CLASS_WARRIOR) == NULL);

    candidates[CLASS_WARRIOR] = {};
    CHECK(FindRandomBotRaceCandidates(candidates, CLASS_WARRIOR) == NULL);

    candidates[CLASS_WARRIOR] = {RACE_HUMAN, RACE_ORC};
    std::vector<uint8> const* races =
        FindRandomBotRaceCandidates(candidates, CLASS_WARRIOR);
    CHECK(races != NULL);
    CHECK_EQ(races->size(), 2);
    CHECK_EQ((*races)[0], RACE_HUMAN);
    CHECK_EQ((*races)[1], RACE_ORC);
}
