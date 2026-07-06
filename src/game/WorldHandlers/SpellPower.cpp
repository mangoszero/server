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
 * @brief Consumes or updates the cast item after spell use when required.
 */
void Spell::TakeCastItem()
{
    if (!m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    // not remove cast item at triggered spell (equipping, weapon damage, etc)
    if (m_IsTriggeredSpell && !(m_targets.m_targetMask & TARGET_FLAG_TRADE_ITEM))
    {
        return;
    }

    ItemPrototype const* proto = m_CastItem->GetProto();

    if (!proto)
    {
        // This code is to avoid a crash
        // I'm not sure, if this is really an error, but I guess every item needs a prototype
        sLog.outError("Cast item (%s) has no item prototype", m_CastItem->GetGuidStr().c_str());
        return;
    }

    bool expendable = false;
    bool withoutCharges = false;

    for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
    {
        if (proto->Spells[i].SpellId)
        {
            // item has limited charges
            if (proto->Spells[i].SpellCharges)
            {
                if (proto->Spells[i].SpellCharges < 0 && !(proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE))
                {
                    expendable = true;
                }

                int32 charges = m_CastItem->GetSpellCharges(i);

                // item has charges left
                if (charges)
                {
                    (charges > 0) ? --charges : ++charges;  // abs(charges) less at 1 after use
                    if (proto->Stackable == 1)
                    {
                        m_CastItem->SetSpellCharges(i, charges);
                    }
                    m_CastItem->SetState(ITEM_CHANGED, (Player*)m_caster);
                }

                // all charges used
                withoutCharges = (charges == 0);
            }
        }
    }

    if (expendable && withoutCharges)
    {
        uint32 count = 1;
        ((Player*)m_caster)->DestroyItemCount(m_CastItem, count, true);

        // prevent crash at access to deleted m_targets.getItemTarget
        ClearCastItem();
    }
}

/**
 * @brief Deducts the spell power cost from the caster.
 */
void Spell::TakePower()
{
    if (m_CastItem || m_IsTriggeredSpell)   // all triggered spells ignore power req
    {
        return;
    }

    // health as power used
    if (m_spellInfo->PowerType == POWER_HEALTH)
    {
        m_caster->ModifyHealth(-(int32)m_powerCost);
        return;
    }

    // the static data (m_spellInfo) should be checked elsewhere
    // [+ZERO] actual DBC power values are 0..3 and uint32(-2)

    Powers powerType = Powers(m_spellInfo->PowerType);

    bool hit = true;
    for (uint8 j = 0; j < 3; ++j)
    {
        // Spell targets a single enemy
        if (m_spellInfo->ImplicitTargetA[j] == TARGET_CHAIN_DAMAGE ||
            m_spellInfo->ImplicitTargetA[j] == TARGET_CURRENT_ENEMY_COORDINATES)
        {
            if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                if (powerType == POWER_ENERGY || powerType == POWER_RAGE)
                {
                    for (std::list<TargetInfo>::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                    {
                        if (ihit->missCondition != SPELL_MISS_NONE)
                        {
                            hit = false;
                        }
                        break;
                    }
                }
            }
        }
    }
    if (hit || m_spellInfo->AttributesEx & SPELL_ATTR_EX_REQ_TARGET_COMBO_POINTS || m_spellInfo->AttributesEx & SPELL_ATTR_EX_REQ_COMBO_POINTS)
    {
        m_caster->ModifyPower(powerType, -(int32)m_powerCost);
    }
    else
    {
        m_caster->ModifyPower(powerType, -(int32)m_powerCost / 5);
    }

    // Set the five second timer
    if (powerType == POWER_MANA && m_powerCost > 0)
    {
        m_caster->SetLastManaUse();
    }
}

/**
 * @brief Consumes spell reagents from the player caster inventory.
 */
void Spell::TakeReagents()
{
    if (m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (IgnoreItemRequirements())                           // reagents used in triggered spell removed by original spell or don't must be removed.
    {
        return;
    }

    Player* p_caster = (Player*)m_caster;
    if (p_caster->CanNoReagentCast(m_spellInfo))
    {
        return;
    }

    for (uint32 x = 0; x < MAX_SPELL_REAGENTS; ++x)
    {
        if (m_spellInfo->Reagent[x] <= 0)
        {
            continue;
        }

        uint32 itemid = m_spellInfo->Reagent[x];
        uint32 itemcount = m_spellInfo->ReagentCount[x];

        // if CastItem is also spell reagent
        if (m_CastItem)
        {
            ItemPrototype const* proto = m_CastItem->GetProto();
            if (proto && proto->ItemId == itemid)
            {
                for (int s = 0; s < MAX_ITEM_PROTO_SPELLS; ++s)
                {
                    // CastItem will be used up and does not count as reagent
                    int32 charges = m_CastItem->GetSpellCharges(s);
                    if (proto->Spells[s].SpellCharges < 0 && abs(charges) < 2)
                    {
                        ++itemcount;
                        break;
                    }
                }

                m_CastItem = NULL;
            }
        }

        // if getItemTarget is also spell reagent
        if (m_targets.getItemTargetEntry() == itemid)
        {
            m_targets.setItemTarget(NULL);
        }

        p_caster->DestroyItemCount(itemid, itemcount, true);
    }
}

/**
 * @brief Applies additional configured threat from spell_threat data.
 */
void Spell::HandleThreatSpells()
{
    if (m_UniqueTargetInfo.empty())
    {
        return;
    }

    SpellThreatEntry const* threatEntry = sSpellMgr.GetSpellThreatEntry(m_spellInfo->ID);

    if (!threatEntry || (!threatEntry->threat && threatEntry->ap_bonus == 0.0f))
    {
        return;
    }

    float threat = threatEntry->threat;
    if (threatEntry->ap_bonus != 0.0f)
    {
        threat += threatEntry->ap_bonus * m_caster->GetTotalAttackPowerValue(GetWeaponAttackType(m_spellInfo));
    }

    bool positive = true;
    uint8 effectMask = 0;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_spellInfo->Effect[i])
        {
            effectMask |= (1 << i);
        }
    }

    if (m_negativeEffectMask & effectMask)
    {
        // can only handle spells with clearly defined positive/negative effect, check at spell_threat loading probably not perfect
        // so abort when only some effects are negative.
        if ((m_negativeEffectMask & effectMask) != effectMask)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u, rank %u, is not clearly positive or negative, ignoring bonus threat", m_spellInfo->ID, sSpellMgr.GetSpellRank(m_spellInfo->ID));
            return;
        }
        positive = false;
    }

    // before 2.0.1 threat from positive effects not dependent from targets amount
    if (!positive)
    {
        threat /= m_UniqueTargetInfo.size();
    }

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->missCondition != SPELL_MISS_NONE)
        {
            continue;
        }

        Unit* target = m_caster->GetObjectGuid() == ihit->targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, ihit->targetGUID);
        if (!target)
        {
            continue;
        }

        // positive spells distribute threat among all units that are in combat with target, like healing
        if (positive)
        {
            target->GetHostileRefManager().threatAssist(m_caster /*real_caster ??*/, threat, m_spellInfo);
        }
        // for negative spells threat gets distributed among affected targets
        else
        {
            if (!target->CanHaveThreatList())
            {
                continue;
            }

            target->AddThreat(m_caster, threat, false, GetSpellSchoolMask(m_spellInfo), m_spellInfo);
        }
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u added an additional %f threat for %s %zu target(s)", m_spellInfo->ID, threat, positive ? "assisting" : "harming", m_UniqueTargetInfo.size());
}
