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

/**
 * @file Spell.cpp
 * @brief Spell casting and effect implementation
 *
 * This file implements the Spell class which handles spell casting:
 * - Spell validation and casting requirements
 * - Spell effect execution (damage, healing, summon, etc.)
 * - Spell targeting and area effects
 * - Spell cooldowns and resource costs
 * - Spell interruption and pushback
 * - Spell aura application
 * - Spell hit/miss calculations
 *
 * Spells are the primary combat mechanic in WoW, encompassing
 * abilities, talents, and item effects.
 *
 * @see Spell for the spell class
 * @see SpellAura for spell auras
 * @see SpellMgr for spell management
 */



#include "Spell.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "VMapFactory.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "TemporarySummon.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Checks whether a target matches the spell's creature type restrictions.
 *
 * @param target The target being validated.
 * @return True if the target type is allowed; otherwise, false.
 */
bool Spell::CheckTargetCreatureType(Unit* target) const
{
    uint32 spellCreatureTargetMask = m_spellInfo->TargetCreatureType;

    // Curse of Doom : not find another way to fix spell target check :/
    if (m_spellInfo->ID == 603)                             // in 1.12 "Curse of doom" have only 1 rank.
    {
        // not allow cast at player
        if (target->GetTypeId() == TYPEID_PLAYER)
        {
            return false;
        }

        spellCreatureTargetMask = 0x7FF;
    }

    // Dismiss Pet and Taming Lesson skipped
    if (m_spellInfo->ID == 2641 || m_spellInfo->ID == 23356)
    {
        spellCreatureTargetMask =  0;
    }

    if (spellCreatureTargetMask)
    {
        uint32 TargetCreatureType = target->GetCreatureTypeMask();

        return !TargetCreatureType || (spellCreatureTargetMask & TargetCreatureType);
    }
    return true;
}

/**
 * @brief Gets the current spell container slot used by this spell.
 *
 * @return The current spell container type.
 */
CurrentSpellTypes Spell::GetCurrentContainer()
{
    if (IsNextMeleeSwingSpell())
    {
        return (CURRENT_MELEE_SPELL);
    }
    else if (IsAutoRepeat())
    {
        return (CURRENT_AUTOREPEAT_SPELL);
    }
    else if (IsChanneledSpell(m_spellInfo))
    {
        return (CURRENT_CHANNELED_SPELL);
    }
    else
    {
        return (CURRENT_GENERIC_SPELL);
    }
}

/**
 * @brief Validates whether a candidate target is acceptable for a specific effect.
 *
 * @param target The target being checked.
 * @param eff The effect index being validated.
 * @return True if the target is valid for the effect; otherwise, false.
 */
bool Spell::CheckTarget(Unit* target, SpellEffectIndex eff)
{
    // Check targets for creature type mask and remove not appropriate (skip explicit self target case, maybe need other explicit targets)
    if (m_spellInfo->ImplicitTargetA[eff] != TARGET_SELF)
    {
        if (!CheckTargetCreatureType(target))
        {
            return false;
        }
    }

    // Check targets for not_selectable unit flag and remove
    // A player can cast spells on his pet (or other controlled unit) though in any state
    if (target != m_caster && target->GetCharmerOrOwnerGuid() != m_caster->GetObjectGuid())
    {
        // any unattackable target skipped
        if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NON_ATTACKABLE))
        {
            return false;
        }

        // unselectable targets skipped in all cases except TARGET_SCRIPT targeting
        // in case TARGET_SCRIPT target selected by server always and can't be cheated
        if ((!m_IsTriggeredSpell || target != m_targets.getUnitTarget()) &&
            target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE) &&
            m_spellInfo->ImplicitTargetA[eff] != TARGET_SCRIPT &&
            m_spellInfo->ImplicitTargetB[eff] != TARGET_SCRIPT &&
            m_spellInfo->ImplicitTargetA[eff] != TARGET_AREAEFFECT_INSTANT &&
            m_spellInfo->ImplicitTargetB[eff] != TARGET_AREAEFFECT_INSTANT &&
            m_spellInfo->ImplicitTargetA[eff] != TARGET_AREAEFFECT_CUSTOM &&
            m_spellInfo->ImplicitTargetB[eff] != TARGET_AREAEFFECT_CUSTOM &&
            m_spellInfo->ImplicitTargetA[eff] != TARGET_NARROW_FRONTAL_CONE &&
            m_spellInfo->ImplicitTargetB[eff] != TARGET_NARROW_FRONTAL_CONE)
        {
            return false;
        }
    }

    // Check player targets and remove if in GM mode or GM invisibility (for not self casting case)
    if (target != m_caster && target->GetTypeId() == TYPEID_PLAYER)
    {
        if (((Player*)target)->GetVisibility() == VISIBILITY_OFF)
        {
            return false;
        }

        if (((Player*)target)->isGameMaster() && !IsPositiveSpell(m_spellInfo->ID))
        {
            return false;
        }
    }

    // Check targets for LOS visibility (except spells without range limitations )
    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->ID, NULL, SPELL_DISABLE_LOS))
    {
        switch (m_spellInfo->Effect[eff])
        {
            case SPELL_EFFECT_SUMMON_PLAYER:                    // from anywhere
                break;
            case SPELL_EFFECT_DUMMY:
                if (m_spellInfo->ID != 20577)                   // Cannibalize
                {
                    break;
                }
                // fall through
            case SPELL_EFFECT_RESURRECT_NEW:
                // player far away, maybe his corpse near?
                if (target != m_caster && !target->IsWithinLOSInMap(m_caster))
                {
                    if (!m_targets.getCorpseTargetGuid())
                    {
                        return false;
                    }

                    Corpse* corpse = m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid());
                    if (!corpse)
                    {
                        return false;
                    }

                    if (target->GetObjectGuid() != corpse->GetOwnerGuid())
                    {
                        return false;
                    }

                    if (!corpse->IsWithinLOSInMap(m_caster))
                    {
                        return false;
                    }
                }

                // all ok by some way or another, skip normal check
                break;
            default:                                            // normal case
            {
                // Get GO cast coordinates if original caster -> GO
                if (target != m_caster)
                {
                    if (WorldObject* caster = GetCastingObject())
                    {
                        if (!target->IsWithinLOSInMap(caster))
                        {
                            return false;
                        }
                    }
                }
                break;
            }
        }
    }

    if (target->GetTypeId() != TYPEID_PLAYER && m_spellInfo->HasAttribute(SPELL_ATTR_EX3_TARGET_ONLY_PLAYER) &&
        m_spellInfo->ImplicitTargetA[eff] != TARGET_SCRIPT && m_spellInfo->ImplicitTargetA[eff] != TARGET_SELF)
    {
        return false;
    }

    return true;
}
