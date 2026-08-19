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
 * @file PlayerbotOutOfRangeMoverTest.cpp
 * @brief Pins the playerbot out-of-range mover decision table.
 *
 * A ranged bot standing between a short spell's DBC range and the global
 * spellDistance (30) could never cast that spell -- shamans holding at range
 * cast Earth Shock exactly zero times over whole sessions -- because the only
 * code that looked like it handled the case was dead: the Engine consults an
 * action's prerequisites only after isPossible() has already passed. The fix
 * routes recovery through Engine::DoNextAction's IMPOSSIBLE branch, whose
 * decision core these cases pin.
 *
 * The rule that most needs pinning is the first: an UNRESOLVED spell name must
 * produce no mover, because many action names are intent labels heading an
 * ActionNode cascade ("mage armor" steps down to Frost Armor) and a mover
 * pushed for one of those replaces the alternatives that make the cascade
 * work. That mistake has shipped before; see GenericTriggers.cpp.
 */

#include "TestHarness.h"

#include "../modules/Bots/playerbot/strategy/actions/OutOfRangeMoverPolicy.h"

using ai::ChooseOutOfRangeMover;
using ai::OUT_OF_RANGE_MOVER_NONE;
using ai::OUT_OF_RANGE_MOVER_REACH_MELEE;
using ai::OUT_OF_RANGE_MOVER_REACH_SPELL;

// The live values behind the worked example: ATTACK_DISTANCE is 5.0f
// (src/game/Object/Object.h) and a learned Earth Shock clamps to its 20-yard
// DBC range minus one.
namespace
{
    const float attackDistance = 5.0f;
    const float earthShockRange = 19.0f;
    const unsigned int earthShockId = 8042;
}

TEST(OutOfRangeSpellWithCurrentTargetGetsQualifiedMover)
{
    // Earth Shock known, target at 25 yards, range 19: the dead-zone case.
    CHECK_EQ(ChooseOutOfRangeMover(earthShockId, true, 25.0f, earthShockRange, attackDistance),
             OUT_OF_RANGE_MOVER_REACH_SPELL);
}

TEST(InRangeSpellGetsNoMover)
{
    // At 18 yards the cast is not blocked by range, so whatever made it
    // impossible (mana, cooldown) cannot be fixed by moving.
    CHECK_EQ(ChooseOutOfRangeMover(earthShockId, true, 18.0f, earthShockRange, attackDistance),
             OUT_OF_RANGE_MOVER_NONE);
}

TEST(ExactlyAtRangeGetsNoMover)
{
    CHECK_EQ(ChooseOutOfRangeMover(earthShockId, true, earthShockRange, earthShockRange, attackDistance),
             OUT_OF_RANGE_MOVER_NONE);
}

TEST(UnresolvedIntentNameGetsNoMover)
{
    // The cascade-preservation rule: an unresolved name must fall through to
    // its ActionNode alternatives whatever the geometry says.
    CHECK_EQ(ChooseOutOfRangeMover(0, true, 25.0f, 30.0f, attackDistance),
             OUT_OF_RANGE_MOVER_NONE);
}

TEST(NonCurrentTargetActionGetsNoMover)
{
    // Enemy-healer and party-member actions override their target value and
    // keep their existing behaviour.
    CHECK_EQ(ChooseOutOfRangeMover(earthShockId, false, 25.0f, earthShockRange, attackDistance),
             OUT_OF_RANGE_MOVER_NONE);
}

TEST(MeleeRangePolicyGetsReachMelee)
{
    // A melee-policy cast (range pinned to ATTACK_DISTANCE, e.g. Judgement)
    // reuses the ordinary melee mover rather than a qualified spell mover.
    CHECK_EQ(ChooseOutOfRangeMover(20271, true, 25.0f, attackDistance, attackDistance),
             OUT_OF_RANGE_MOVER_REACH_MELEE);
}
