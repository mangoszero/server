/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "TotemAI.h"
#include "Totem.h"
#include "Creature.h"
#include "DBCStores.h"
#include "SpellMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

/**
 * @brief Determines whether TotemAI can control the given creature.
 *
 * @param creature The creature being evaluated.
 * @return The AI selection priority for totem creatures.
 */
int TotemAI::Permissible(const Creature* creature)
{
    if (creature->IsTotem())
    {
        return PERMIT_BASE_PROACTIVE;
    }

    return PERMIT_BASE_NO;
}

/**
 * @brief Initializes a totem AI instance.
 *
 * @param c The creature controlled by this AI.
 */
TotemAI::TotemAI(Creature* c) : CreatureAI(c)
{
}

/**
 * @brief Ignores line-of-sight events for totems.
 *
 * @param The unit entering line of sight.
 */
void TotemAI::MoveInLineOfSight(Unit*)
{
}

/**
 * @brief Stops totem combat when entering evade mode.
 */
void TotemAI::EnterEvadeMode()
{
    m_creature->CombatStop(true);
}

/**
 * @brief Updates active totem behavior.
 *
 * Searches for a valid hostile target within spell range and casts the totem's
 * configured spell when possible.
 *
 * @param diff The elapsed time since the last update in milliseconds.
 */
void TotemAI::UpdateAI(const uint32 /*diff*/)
{
    if (getTotem().GetTotemType() != TOTEM_ACTIVE)
    {
        return;
    }

    if (!m_creature->IsAlive() || m_creature->IsNonMeleeSpellCasted(false))
    {
        return;
    }

    // Search spell
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(getTotem().GetSpell());
    if (!spellInfo)
    {
        return;
    }

    // Get spell rangy
    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(spellInfo->rangeIndex);
    float max_range = GetSpellMaxRange(srange);

    // SPELLMOD_RANGE not applied in this place just because nonexistent range mods for attacking totems

    // pointer to appropriate target if found any
    Unit* victim = m_creature->GetMap()->GetUnit(i_victimGuid);

    // Search victim if no, not attackable, or out of range, or friendly (possible in case duel end)
    if (!victim ||
        !victim->IsTargetableForAttack() || !m_creature->IsWithinDistInMap(victim, max_range) ||
        m_creature->IsFriendlyTo(victim) || !victim->IsVisibleForOrDetect(m_creature, m_creature, false))
    {
        victim = NULL;

        MaNGOS::NearestAttackableUnitInObjectRangeCheck u_check(m_creature, m_creature, max_range);
        MaNGOS::UnitLastSearcher<MaNGOS::NearestAttackableUnitInObjectRangeCheck> checker(victim, u_check);
        Cell::VisitAllObjects(m_creature, checker, max_range);
    }

    // If have target
    if (victim)
    {
        // remember
        i_victimGuid = victim->GetObjectGuid();

        // attack
        m_creature->SetInFront(victim);                     // client change orientation by self
        m_creature->CastSpell(victim, getTotem().GetSpell(), false);
    }
    else
    {
        i_victimGuid.Clear();
    }
}

/**
 * @brief Totems do not use generic visibility checks in this AI.
 *
 * @param The unit being tested.
 * @return false.
 */
bool TotemAI::IsVisible(Unit*) const
{
    return false;
}

/**
 * @brief Ignores direct attack start requests for totems.
 *
 * @param The target unit.
 */
void TotemAI::AttackStart(Unit*)
{
}

/**
 * @brief Retrieves the controlled creature as a totem.
 *
 * @return Reference to the controlled totem.
 */
Totem& TotemAI::getTotem()
{
    return static_cast<Totem&>(*m_creature);
}
