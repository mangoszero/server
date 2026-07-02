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



#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "Opcodes.h"
#include "Log.h"
#include "UpdateMask.h"
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Unit.h"
#include "Spell.h"
#include "DynamicObject.h"
#include "SpellAuras.h"
#include "Group.h"
#include "UpdateData.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "SharedDefines.h"
#include "Pet.h"
#include "GameObject.h"
#include "GossipDef.h"
#include "Creature.h"
#include "Totem.h"
#include "CreatureAI.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundWS.h"
#include "Language.h"
#include "SocialMgr.h"
#include "VMapFactory.h"
#include "Util.h"
#include "TemporarySummon.h"
#include "ScriptMgr.h"
#include "Formulas.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "G3D/Vector3.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Sends a resurrection request to a dead player target.
 *
 * @param eff_idx The effect index providing resurrection values.
 */
void Spell::EffectResurrectNew(SpellEffectIndex eff_idx)
{
    if (!unitTarget || unitTarget->IsAlive())
    {
        return;
    }

    if (unitTarget->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (!unitTarget->IsInWorld())
    {
        return;
    }

    Player* pTarget = ((Player*)unitTarget);

    if (pTarget->isRessurectRequested())      // already have one active request
    {
        return;
    }

    uint32 health = damage;
    uint32 mana = m_spellInfo->EffectMiscValue[eff_idx];
    pTarget->setResurrectRequestData(m_caster->GetObjectGuid(), m_caster->GetMapId(), m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ(), health, mana);
    SendResurrectRequest(pTarget);
}

/**
 * @brief Instantly kills the unit target and handles spell-specific side effects.
 *
 * @param eff_idx Unused effect index.
 */
void Spell::EffectInstaKill(SpellEffectIndex /*eff_idx*/)
{
    if (!unitTarget || !unitTarget->IsAlive())
    {
        return;
    }

    // Demonic Sacrifice
    if (m_spellInfo->Id == 18788 && unitTarget->GetTypeId() == TYPEID_UNIT)
    {
        uint32 entry = unitTarget->GetEntry();
        uint32 spellID;
        switch (entry)
        {
            case   416: spellID = 18789; break;             // imp
            case   417: spellID = 18792; break;             // fellhunter
            case  1860: spellID = 18790; break;             // void
            case  1863: spellID = 18791; break;             // succubus
            default:
                sLog.outError("EffectInstaKill: Unhandled creature entry (%u) case.", entry);
                return;
        }

        m_caster->CastSpell(m_caster, spellID, true);
    }

    if (m_caster == unitTarget)                             // prevent interrupt message
    {
        finish();
        WorldPacket data(SMSG_SPELLINSTAKILLLOG, (8 + 4));  // sent for selfkill only, other type is logged at SpellExecute
        data << m_caster->GetObjectGuid();
        data << uint32(m_spellInfo->Id);
        m_caster->SendMessageToSet(&data, true);
    }

    m_caster->DealDamage(unitTarget, unitTarget->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
}

/**
 * @brief Applies environmental damage to the caster.
 *
 * @param eff_idx The effect index used to calculate the base damage.
 */
void Spell::EffectEnvironmentalDMG(SpellEffectIndex eff_idx)
{
    uint32 absorb = 0;
    uint32 resist = 0;

    // Note: this hack with damage replace required until GO casting not implemented
    // environment damage spells already have around enemies targeting but this not help in case nonexistent GO casting support
    // currently each enemy selected explicitly and self cast damage, we prevent apply self casted spell bonuses/etc
    damage = m_spellInfo->CalculateSimpleValue(eff_idx);

    m_caster->CalculateDamageAbsorbAndResist(m_caster, GetSpellSchoolMask(m_spellInfo), SPELL_DIRECT_DAMAGE, damage, &absorb, &resist);

    m_caster->SendSpellNonMeleeDamageLog(m_caster, m_spellInfo->Id, damage, GetSpellSchoolMask(m_spellInfo), absorb, resist, false, 0, false);
    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)m_caster)->EnvironmentalDamage(DAMAGE_FIRE, damage);
    }
}

/**
 * @brief Computes school-damage special cases and accumulates resulting damage.
 *
 * @param effect_idx The damage effect index.
 */
void Spell::EffectSchoolDMG(SpellEffectIndex effect_idx)
{
    if (unitTarget && unitTarget->IsAlive())
    {
        switch (m_spellInfo->SpellFamilyName)
        {
            case SPELLFAMILY_GENERIC:
            {
                switch (m_spellInfo->Id)                    // better way to check unknown
                {
                    // Meteor like spells (divided damage to targets)
                    case 24340: case 26558: case 28884:     // Meteor
                    case 26789:                             // Shard of the Fallen Star
                    {
                        uint32 count = 0;
                        for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
                        {
                            if (ihit->effectMask & (1 << effect_idx))
                            {
                                ++count;
                            }
                        }

                        damage /= count;                    // divide to all targets
                        break;
                    }
                    // percent from health with min
                    case 25599:                             // Thundercrash
                    {
                        damage = unitTarget->GetHealth() / 2;
                        if (damage < 200)
                        {
                            damage = 200;
                        }
                        break;
                    }
                    // Judgement of Command
                    case 20467:    case 20963:    case 20964:    case 20965:    case 20966:
                    {
                        if (!unitTarget->hasUnitState(UNIT_STAT_STUNNED) && m_caster->GetTypeId() == TYPEID_PLAYER)
                        {
                            damage /= 2;
                        }
                        break;
                    }
                }
                break;
            }

            case SPELLFAMILY_MAGE:
                break;
            case SPELLFAMILY_WARRIOR:
            {
                // Bloodthirst
                if (m_spellInfo->SpellIconID == 38 && m_spellInfo->SpellFamilyFlags & UI64LIT(0x2000000))
                {
                    damage = uint32(damage * (m_caster->GetTotalAttackPowerValue(BASE_ATTACK)) / 100);
                }
                // Shield Slam
                else if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x100000000))
                {
                    damage += int32(m_caster->GetShieldBlockValue());
                }
                break;
            }
            case SPELLFAMILY_WARLOCK:
            {
                // Conflagrate - consumes Immolate
                if (m_spellInfo->SpellFamilyFlags & UI64LIT(0x0000000000000200))
                {
                    // for caster applied auras only
                    Unit::AuraList const& mPeriodic = unitTarget->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
                    for (Unit::AuraList::const_iterator i = mPeriodic.begin(); i != mPeriodic.end(); ++i)
                    {
                        if ((*i)->GetCasterGuid() == m_caster->GetObjectGuid() &&
                            // Immolate
                            (*i)->GetSpellProto()->IsFitToFamily(SPELLFAMILY_WARLOCK, UI64LIT(0x0000000000000004)))
                        {
                            unitTarget->RemoveAurasByCasterSpell((*i)->GetId(), m_caster->GetObjectGuid());
                            break;
                        }
                    }
                }
                break;
            }
            case SPELLFAMILY_DRUID:
            {
                // Ferocious Bite
                if ((m_spellInfo->SpellFamilyFlags & UI64LIT(0x000800000)) && m_spellInfo->SpellVisual == 6587)
                {
                    // converts each extra point of energy into ($f1+$AP/630) additional damage
                    float multiple = m_caster->GetTotalAttackPowerValue(BASE_ATTACK) / 630 + m_spellInfo->DmgMultiplier[effect_idx];
                    damage += int32(m_caster->GetPower(POWER_ENERGY) * multiple);
                    m_caster->SetPower(POWER_ENERGY, 0);
                }
                break;
            }
            case SPELLFAMILY_ROGUE:
            {
                // Eviscerate
                if ((m_spellInfo->SpellFamilyFlags & UI64LIT(0x00020000)) && m_caster->GetTypeId() == TYPEID_PLAYER)
                {
                    if (uint32 combo = ((Player*)m_caster)->GetComboPoints())
                    {
                        damage += int32(m_caster->GetTotalAttackPowerValue(BASE_ATTACK) * combo * 0.03f);
                    }
                }
                break;
            }
            case SPELLFAMILY_HUNTER:
                break;
            case SPELLFAMILY_PALADIN:
                break;
        }

        if (damage >= 0)
        {
            m_damage += damage;
        }
    }
}

/**
 * @brief Triggers another spell on the current unit target.
 *
 * @param eff_idx The effect index providing the triggered spell id.
 */
void Spell::EffectTriggerSpell(SpellEffectIndex eff_idx)
{
    // only unit case known
    if (!unitTarget)
    {
        if (gameObjTarget || itemTarget)
        {
            sLog.outError("Spell::EffectTriggerSpell (Spell: %u): Unsupported non-unit case!", m_spellInfo->Id);
        }
        return;
    }

    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[eff_idx];

    // special cases
    switch (triggered_spell_id)
    {
        // Temporal Parasite Summon #2, special case because chance is set to 101% in DBC while description is 67%
        case 16630:
            if (urand(0, 100) < 67)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;
        // Temporal Parasite Summon #3, special case because chance is set to 101% in DBC while description is 34%
        case 16631:
            if (urand(0, 100) < 34)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;

        // Vanish (not exist)
        case 18461:
        {
            unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT);
            unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_DECREASE_SPEED);
            unitTarget->RemoveSpellsCausingAura(SPELL_AURA_MOD_STALKED);

            // if this spell is given to NPC it must handle rest by it's own AI
            if (unitTarget->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            // get highest rank of the Stealth spell
            uint32 spellId = 0;
            const PlayerSpellMap& sp_list = ((Player*)unitTarget)->GetSpellMap();
            for (PlayerSpellMap::const_iterator itr = sp_list.begin(); itr != sp_list.end(); ++itr)
            {
                // only highest rank is shown in spell book, so simply check if shown in spell book
                if (!itr->second.active || itr->second.disabled || itr->second.state == PLAYERSPELL_REMOVED)
                {
                    continue;
                }

                SpellEntry const* spellInfo = sSpellStore.LookupEntry(itr->first);
                if (!spellInfo)
                {
                    continue;
                }

                if (spellInfo->IsFitToFamily(SPELLFAMILY_ROGUE, UI64LIT(0x0000000000400000)))
                {
                    spellId = spellInfo->Id;
                    break;
                }
            }

            // no Stealth spell found
            if (!spellId)
            {
                return;
            }

            // reset cooldown on it if needed
            if (((Player*)unitTarget)->HasSpellCooldown(spellId))
            {
                ((Player*)unitTarget)->RemoveSpellCooldown(spellId);
            }

            m_caster->CastSpell(unitTarget, spellId, true);
            return;
        }

        // Terrordale Haunting Spirit #2, special case because chance is set to 101% in DBC while description is 55%
        case 23209:
            if (urand(0, 100) < 55)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;
        // Terrordale Haunting Spirit #3, special case because chance is set to 101% in DBC while description is 35%
        case 23253:
            if (urand(0, 100) < 35)
            {
                m_caster->CastSpell(unitTarget, triggered_spell_id, true);
            }
            return;

        // just skip
        case 23770:                                         // Sayge's Dark Fortune of *
            // not exist, common cooldown can be implemented in scripts if need.
            return;
        // Brittle Armor - (need add max stack of 24575 Brittle Armor)
        case 29284:
            m_caster->CastSpell(unitTarget, 24575, true, m_CastItem, NULL, m_originalCasterGUID);
            return;
        // Mercurial Shield - (need add max stack of 26464 Mercurial Shield)
        case 29286:
            m_caster->CastSpell(unitTarget, 26464, true, m_CastItem, NULL, m_originalCasterGUID);
            return;
    }

    // normal case
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);
    if (!spellInfo)
    {
        sLog.outError("EffectTriggerSpell of spell %u: triggering unknown spell id %i", m_spellInfo->Id, triggered_spell_id);
        return;
    }

    // select formal caster for triggered spell
    Unit* caster = m_caster;

    // some triggered spells require specific equipment
    if (spellInfo->EquippedItemClass >= 0 && m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        // main hand weapon required
        if (spellInfo->AttributesEx3 & SPELL_ATTR_EX3_MAIN_HAND)
        {
            Item* item = ((Player*)m_caster)->GetWeaponForAttack(BASE_ATTACK, true, false);

            // skip spell if no weapon in slot or broken
            if (!item)
            {
                return;
            }

            // skip spell if weapon not fit to triggered spell
            if (!item->IsFitToSpellRequirements(spellInfo))
            {
                return;
            }
        }

        // offhand hand weapon required
        if (spellInfo->AttributesEx3 & SPELL_ATTR_EX3_REQ_OFFHAND)
        {
            Item* item = ((Player*)m_caster)->GetWeaponForAttack(OFF_ATTACK, true, false);

            // skip spell if no weapon in slot or broken
            if (!item)
            {
                return;
            }

            // skip spell if weapon not fit to triggered spell
            if (!item->IsFitToSpellRequirements(spellInfo))
            {
                return;
            }
        }
    }
    else
    {
        // Note: not exist spells with weapon req. and IsSpellHaveCasterSourceTargets == true
        // so this just for speedup places in else
        caster = IsSpellWithCasterSourceTargetsOnly(spellInfo) ? unitTarget : m_caster;
    }

    caster->CastSpell(unitTarget, spellInfo, true, m_CastItem, NULL, m_originalCasterGUID, m_spellInfo);
}

/**
 * @brief Triggers a missile spell at the stored destination coordinates.
 *
 * @param effect_idx The effect index providing the triggered spell id.
 */
void Spell::EffectTriggerMissileSpell(SpellEffectIndex effect_idx)
{
    uint32 triggered_spell_id = m_spellInfo->EffectTriggerSpell[effect_idx];

    // normal case
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(triggered_spell_id);

    if (!spellInfo)
    {
        if (unitTarget)
        {
            DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell ScriptStart spellid %u in EffectTriggerMissileSpell", m_spellInfo->Id);
            m_caster->GetMap()->ScriptsStart(DBS_ON_SPELL, m_spellInfo->Id, m_caster, unitTarget);
        }
        else
        {
            sLog.outError("EffectTriggerMissileSpell of spell %u (eff: %u): triggering unknown spell id %u",
                m_spellInfo->Id, effect_idx, triggered_spell_id);
        }
        return;
    }

    if (m_CastItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->Id);
    }

    m_caster->CastSpell(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ, spellInfo, true, m_CastItem, NULL, m_originalCasterGUID, m_spellInfo);
}

void Spell::EffectTeleportUnits(SpellEffectIndex eff_idx)   // TODO - Use target settings for this effect!
{
    if (!unitTarget || unitTarget->IsTaxiFlying())
    {
        return;
    }

    // Target dependend on TargetB, if there is none provided, decide dependend on A
    uint32 targetType = m_spellInfo->EffectImplicitTargetB[eff_idx];
    if (!targetType)
    {
        targetType = m_spellInfo->EffectImplicitTargetA[eff_idx];
    }

    switch (targetType)
    {
        case TARGET_INNKEEPER_COORDINATES:
        {
            // Only players can teleport to innkeeper
            if (unitTarget->GetTypeId() != TYPEID_PLAYER)
            {
                return;
            }

            ((Player*)unitTarget)->TeleportToHomebind(unitTarget == m_caster ? TELE_TO_SPELL : 0);
            return;
        }
        case TARGET_AREAEFFECT_INSTANT:                     // in all cases first TARGET_TABLE_X_Y_Z_COORDINATES
        case TARGET_TABLE_X_Y_Z_COORDINATES:
        {
            SpellTargetPosition const* st = sSpellMgr.GetSpellTargetPosition(m_spellInfo->Id);
            if (!st)
            {
                sLog.outError("Spell::EffectTeleportUnits - unknown Teleport coordinates for spell ID %u", m_spellInfo->Id);
                return;
            }

            if (st->target_mapId == unitTarget->GetMapId())
            {
                unitTarget->NearTeleportTo(st->target_X, st->target_Y, st->target_Z, st->target_Orientation, unitTarget == m_caster);
            }
            else if (unitTarget->GetTypeId() == TYPEID_PLAYER)
            {
                ((Player*)unitTarget)->TeleportTo(st->target_mapId, st->target_X, st->target_Y, st->target_Z, st->target_Orientation, unitTarget == m_caster ? TELE_TO_SPELL : 0);
            }
            break;
        }
        case TARGET_EFFECT_SELECT:
        {
            // m_destN filled, but sometimes for wrong dest and does not have TARGET_FLAG_DEST_LOCATION

            float x = unitTarget->GetPositionX();
            float y = unitTarget->GetPositionY();
            float z = unitTarget->GetPositionZ();
            float orientation = m_caster->GetOrientation();

            m_caster->NearTeleportTo(x, y, z, orientation, unitTarget == m_caster);
            return;
        }
        default:
        {
            // If not exist data for dest location - return
            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
            {
                sLog.outError("Spell::EffectTeleportUnits - unknown EffectImplicitTargetB[%u] = %u for spell ID %u", eff_idx, m_spellInfo->EffectImplicitTargetB[eff_idx], m_spellInfo->Id);
                return;
            }
            // Init dest coordinates
            float x = m_targets.m_destX;
            float y = m_targets.m_destY;
            float z = m_targets.m_destZ;
            float orientation = unitTarget->GetOrientation();
            // Teleport
            unitTarget->NearTeleportTo(x, y, z, orientation, unitTarget == m_caster);
            return;
        }
    }

    // post effects for TARGET_TABLE_X_Y_Z_COORDINATES
    switch (m_spellInfo->Id)
    {
        // Dimensional Ripper - Everlook
        case 23442:
        {
            int32 r = irand(0, 119);
            if (r >= 70)                                    // 7/12 success
            {
                if (r < 100)                                // 4/12 evil twin
                {
                    m_caster->CastSpell(m_caster, 23445, true);
                }
                else                                        // 1/12 fire
                {
                    m_caster->CastSpell(m_caster, 23449, true);
                }
            }
            return;
        }
    }
}
