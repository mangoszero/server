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

#include "../modules/Bots/playerbot/PlayerbotMountPolicy.h"

using ai::GetPlayerbotMountSpells;
using ai::IsPlayerbotMountSpellCompatible;

namespace
{
    void CheckMounts(std::vector<uint32_t> const& actual,
                     std::initializer_list<uint32_t> expected)
    {
        CHECK_EQ(actual.size(), expected.size());

        size_t index = 0;
        for (uint32_t spellId : expected)
        {
            if (index < actual.size())
            {
                CHECK_EQ(actual[index], spellId);
            }
            ++index;
        }
    }

    bool Contains(std::vector<uint32_t> const& spells, uint32_t spellId)
    {
        return std::find(spells.begin(), spells.end(), spellId) != spells.end();
    }
}

TEST(PlayerbotMountPolicyChoosesOnlyNativeNormalMounts)
{
    CheckMounts(GetPlayerbotMountSpells(RACE_HUMAN, CLASS_WARRIOR, 75),
                {458, 470, 472, 6648});
    CheckMounts(GetPlayerbotMountSpells(RACE_ORC, CLASS_WARRIOR, 75),
                {580, 6653, 6654});
    CheckMounts(GetPlayerbotMountSpells(RACE_DWARF, CLASS_WARRIOR, 75),
                {6777, 6898, 6899});
    CheckMounts(GetPlayerbotMountSpells(RACE_NIGHTELF, CLASS_WARRIOR, 75),
                {8394, 10789, 10793});
    CheckMounts(GetPlayerbotMountSpells(RACE_UNDEAD, CLASS_WARRIOR, 75),
                {17462, 17463, 17464});
    CheckMounts(GetPlayerbotMountSpells(RACE_TAUREN, CLASS_WARRIOR, 75),
                {18989, 18990});
    CheckMounts(GetPlayerbotMountSpells(RACE_TROLL, CLASS_WARRIOR, 75),
                {8395, 10796, 10799});
    CheckMounts(GetPlayerbotMountSpells(RACE_GNOME, CLASS_WARRIOR, 75),
                {10873, 10969, 17453, 17454});
}

TEST(PlayerbotMountPolicyChoosesOnlyNativeFastMounts)
{
    CheckMounts(GetPlayerbotMountSpells(RACE_HUMAN, CLASS_WARRIOR, 150),
                {23227, 23228, 23229});
    CheckMounts(GetPlayerbotMountSpells(RACE_ORC, CLASS_WARRIOR, 150),
                {23250, 23251, 23252});
    CheckMounts(GetPlayerbotMountSpells(RACE_DWARF, CLASS_WARRIOR, 150),
                {23238, 23239, 23240});
    CheckMounts(GetPlayerbotMountSpells(RACE_NIGHTELF, CLASS_WARRIOR, 150),
                {23219, 23221, 23338});
    CheckMounts(GetPlayerbotMountSpells(RACE_UNDEAD, CLASS_WARRIOR, 150),
                {17465, 23246});
    CheckMounts(GetPlayerbotMountSpells(RACE_TAUREN, CLASS_WARRIOR, 150),
                {23247, 23248, 23249});
    CheckMounts(GetPlayerbotMountSpells(RACE_TROLL, CLASS_WARRIOR, 150),
                {23241, 23242, 23243});
    CheckMounts(GetPlayerbotMountSpells(RACE_GNOME, CLASS_WARRIOR, 150),
                {23222, 23223, 23225});
}

TEST(PlayerbotMountPolicyAddsValidClassMountsAtTheCurrentTier)
{
    CheckMounts(GetPlayerbotMountSpells(RACE_HUMAN, CLASS_PALADIN, 75),
                {458, 470, 472, 6648, 13819});
    CheckMounts(GetPlayerbotMountSpells(RACE_DWARF, CLASS_PALADIN, 150),
                {23238, 23239, 23240, 23214});
    CheckMounts(GetPlayerbotMountSpells(RACE_ORC, CLASS_WARLOCK, 75),
                {580, 6653, 6654, 5784});
    CheckMounts(GetPlayerbotMountSpells(RACE_UNDEAD, CLASS_WARLOCK, 150),
                {17465, 23246, 23161});
}

TEST(PlayerbotMountPolicyRejectsInvalidClassAndRaceCombinations)
{
    std::vector<uint32_t> const taurenPaladin =
        GetPlayerbotMountSpells(RACE_TAUREN, CLASS_PALADIN, 150);
    std::vector<uint32_t> const nightElfWarlock =
        GetPlayerbotMountSpells(RACE_NIGHTELF, CLASS_WARLOCK, 150);

    CHECK(!Contains(taurenPaladin, 23214));
    CHECK(!Contains(nightElfWarlock, 23161));
}

TEST(PlayerbotMountPolicyRejectsRewardAndCrossFactionMounts)
{
    std::vector<uint32_t> const human =
        GetPlayerbotMountSpells(RACE_HUMAN, CLASS_WARRIOR, 150);
    uint32_t const forbidden[] = {
        17229, // Winterspring Frostsaber
        17481, // Deathcharger
        22717, // Black War Steed
        23509, // Frostwolf Howler
        23510, // Stormpike Battle Charger
        24242, // Swift Razzashi Raptor
        24252, // Swift Zulian Tiger
        25953  // Blue Qiraji Battle Tank
    };

    for (uint32_t spellId : forbidden)
    {
        CHECK(!Contains(human, spellId));
    }
}

TEST(PlayerbotMountPolicyHonoursRidingThresholds)
{
    CHECK(GetPlayerbotMountSpells(RACE_HUMAN, CLASS_WARRIOR, 74).empty());
    CheckMounts(GetPlayerbotMountSpells(RACE_HUMAN, CLASS_WARRIOR, 149),
                {458, 470, 472, 6648});
    CheckMounts(GetPlayerbotMountSpells(RACE_HUMAN, CLASS_WARRIOR, 150),
                {23227, 23228, 23229});
}

TEST(PlayerbotMountPolicyValidatesLoadedSpellShapeAndTier)
{
    CHECK(IsPlayerbotMountSpellCompatible(75, true, 3000, -1, 59));
    CHECK(IsPlayerbotMountSpellCompatible(150, true, 3000, -1, 99));

    CHECK(!IsPlayerbotMountSpellCompatible(74, true, 3000, -1, 59));
    CHECK(!IsPlayerbotMountSpellCompatible(75, false, 3000, -1, 59));
    CHECK(!IsPlayerbotMountSpellCompatible(75, true, 0, -1, 59));
    CHECK(!IsPlayerbotMountSpellCompatible(75, true, 3000, 60000, 59));
    CHECK(!IsPlayerbotMountSpellCompatible(75, true, 3000, -1, 99));
    CHECK(!IsPlayerbotMountSpellCompatible(150, true, 3000, -1, 59));
}
