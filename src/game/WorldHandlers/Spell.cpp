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
#include "World.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "Player.h"
#include "Pet.h"
#include "Unit.h"
#include "DynamicObject.h"
#include "Group.h"
#include "UpdateData.h"
#include "ObjectAccessor.h"
#include "CellImpl.h"
#include "Policies/Singleton.h"
#include "SharedDefines.h"
#include "LootMgr.h"
#include "VMapFactory.h"
#include "BattleGround/BattleGround.h"
#include "Util.h"
#include "Chat.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

extern pEffect SpellEffects[TOTAL_SPELL_EFFECTS];

/**
 * @brief Checks whether a spell matches the quest tame spell pattern.
 *
 * @param spellId The spell identifier to test.
 * @return True if the spell is a quest tame spell; otherwise, false.
 */
bool IsQuestTameSpell(uint32 spellId)
{
    SpellEntry const* spellproto = sSpellStore.LookupEntry(spellId);
    if (!spellproto)
    {
        return false;
    }

    return spellproto->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_THREAT &&
        spellproto->Effect[EFFECT_INDEX_1] == SPELL_EFFECT_APPLY_AURA && spellproto->EffectApplyAuraName[EFFECT_INDEX_1] == SPELL_AURA_DUMMY;
}

SpellCastTargets::SpellCastTargets()
{
    m_unitTarget = NULL;
    m_itemTarget = NULL;
    m_GOTarget   = NULL;

    m_itemTargetEntry  = 0;

    m_srcX = m_srcY = m_srcZ = m_destX = m_destY = m_destZ = 0.0f;
    m_strTarget.clear();
    m_targetMask = 0;
}

SpellCastTargets::~SpellCastTargets()
{
}

/**
 * @brief Sets a unit target and copies its current position as the destination.
 *
 * @param target The unit target.
 */
void SpellCastTargets::setUnitTarget(Unit* target)
{
    if (!target)
    {
        return;
    }

    m_destX = target->GetPositionX();
    m_destY = target->GetPositionY();
    m_destZ = target->GetPositionZ();
    m_unitTarget = target;
    m_unitTargetGUID = target->GetObjectGuid();
    m_targetMask |= TARGET_FLAG_UNIT;
}

/**
 * @brief Sets the destination coordinates for the cast.
 *
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 */
void SpellCastTargets::setDestination(float x, float y, float z)
{
    m_destX = x;
    m_destY = y;
    m_destZ = z;
    m_targetMask |= TARGET_FLAG_DEST_LOCATION;
}

/**
 * @brief Sets the source coordinates for the cast.
 *
 * @param x The source X coordinate.
 * @param y The source Y coordinate.
 * @param z The source Z coordinate.
 */
void SpellCastTargets::setSource(float x, float y, float z)
{
    m_srcX = x;
    m_srcY = y;
    m_srcZ = z;
    m_targetMask |= TARGET_FLAG_SOURCE_LOCATION;
}

/**
 * @brief Sets the game object target for the cast.
 *
 * @param target The game object target.
 */
void SpellCastTargets::setGOTarget(GameObject* target)
{
    m_GOTarget = target;
    m_GOTargetGUID = target->GetObjectGuid();
    //    m_targetMask |= TARGET_FLAG_OBJECT;
}

/**
 * @brief Sets the item target for the cast.
 *
 * @param item The item target.
 */
void SpellCastTargets::setItemTarget(Item* item)
{
    if (!item)
    {
        return;
    }

    m_itemTarget = item;
    m_itemTargetGUID = item->GetObjectGuid();
    m_itemTargetEntry = item->GetEntry();
    m_targetMask |= TARGET_FLAG_ITEM;
}

/**
 * @brief Sets the current trade slot as the item target.
 *
 * @param caster The player performing the cast.
 */
void SpellCastTargets::setTradeItemTarget(Player* caster)
{
    m_itemTargetGUID = ObjectGuid(uint64(TRADE_SLOT_NONTRADED));
    m_itemTargetEntry = 0;
    m_targetMask |= TARGET_FLAG_TRADE_ITEM;

    Update(caster);
}

/**
 * @brief Sets the corpse target for the cast.
 *
 * @param corpse The corpse target.
 */
void SpellCastTargets::setCorpseTarget(Corpse* corpse)
{
    m_CorpseTargetGUID = corpse->GetObjectGuid();
}

/**
 * @brief Resolves stored target GUIDs into live object pointers.
 *
 * @param caster The casting unit used to resolve map-relative targets.
 */
void SpellCastTargets::Update(Unit* caster)
{
    m_GOTarget   = m_GOTargetGUID ? caster->GetMap()->GetGameObject(m_GOTargetGUID) : NULL;
    m_unitTarget = m_unitTargetGUID
        ? (m_unitTargetGUID == caster->GetObjectGuid() ? caster : sObjectAccessor.GetUnit(*caster, m_unitTargetGUID))
        : NULL;

    m_itemTarget = NULL;
    if (caster->GetTypeId() == TYPEID_PLAYER)
    {
        Player* player = ((Player*)caster);

        if (m_targetMask & TARGET_FLAG_ITEM)
        {
            m_itemTarget = player->GetItemByGuid(m_itemTargetGUID);
        }
        else if (m_targetMask & TARGET_FLAG_TRADE_ITEM)
        {
            if (TradeData* pTrade = player->GetTradeData())
            {
                if (m_itemTargetGUID.GetRawValue() < TRADE_SLOT_COUNT)
                {
                    m_itemTarget = pTrade->GetTraderData()->GetItem(TradeSlots(m_itemTargetGUID.GetRawValue()));
                }
            }
        }

        if (m_itemTarget)
        {
            m_itemTargetEntry = m_itemTarget->GetEntry();
        }
    }
}

/**
 * @brief Deserializes spell cast targets from a packet buffer.
 *
 * @param data The packet buffer to read.
 * @param caster The casting unit.
 */
void SpellCastTargets::read(ByteBuffer& data, Unit* caster)
{
    data >> m_targetMask;

    if (m_targetMask == TARGET_FLAG_SELF)
    {
        m_destX = caster->GetPositionX();
        m_destY = caster->GetPositionY();
        m_destZ = caster->GetPositionZ();
        m_unitTarget = caster;
        m_unitTargetGUID = caster->GetObjectGuid();
        return;
    }

    // TARGET_FLAG_UNK2 is used for non-combat pets, maybe other?
    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_UNK2))
    {
        data >> m_unitTargetGUID.ReadAsPacked();
    }

    if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_OBJECT_UNK | TARGET_FLAG_GAMEOBJECT_ITEM))
    {
        data >> m_GOTargetGUID.ReadAsPacked();
    }

    if ((m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM)) && caster->GetTypeId() == TYPEID_PLAYER)
    {
        data >> m_itemTargetGUID.ReadAsPacked();
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data >> m_srcX >> m_srcY >> m_srcZ;
        if (!MaNGOS::IsValidMapCoord(m_srcX, m_srcY, m_srcZ))
        {
            throw ByteBufferException(false, data.rpos(), 0, data.size());
        }
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data >> m_destX >> m_destY >> m_destZ;
        if (!MaNGOS::IsValidMapCoord(m_destX, m_destY, m_destZ))
        {
            throw ByteBufferException(false, data.rpos(), 0, data.size());
        }
    }

    if (m_targetMask & TARGET_FLAG_STRING)
    {
        data >> m_strTarget;
    }

    if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
    {
        data >> m_CorpseTargetGUID.ReadAsPacked();
    }

    // find real units/GOs
    Update(caster);
}

/**
 * @brief Serializes spell cast targets into a packet buffer.
 *
 * @param data The packet buffer to write.
 */
void SpellCastTargets::write(ByteBuffer& data) const
{
    data << uint16(m_targetMask);

    if (m_targetMask & (TARGET_FLAG_UNIT | TARGET_FLAG_PVP_CORPSE | TARGET_FLAG_OBJECT | TARGET_FLAG_CORPSE | TARGET_FLAG_UNK2))
    {
        if (m_targetMask & TARGET_FLAG_UNIT)
        {
            if (m_unitTarget)
            {
                data << m_unitTarget->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (m_targetMask & (TARGET_FLAG_OBJECT | TARGET_FLAG_OBJECT_UNK))
        {
            if (m_GOTarget)
            {
                data << m_GOTarget->GetPackGUID();
            }
            else
            {
                data << uint8(0);
            }
        }
        else if (m_targetMask & (TARGET_FLAG_CORPSE | TARGET_FLAG_PVP_CORPSE))
        {
            data << m_CorpseTargetGUID.WriteAsPacked();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (m_targetMask & (TARGET_FLAG_ITEM | TARGET_FLAG_TRADE_ITEM))
    {
        if (m_itemTarget)
        {
            data << m_itemTarget->GetPackGUID();
        }
        else
        {
            data << uint8(0);
        }
    }

    if (m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
    {
        data << m_srcX << m_srcY << m_srcZ;
    }

    if (m_targetMask & TARGET_FLAG_DEST_LOCATION)
    {
        data << m_destX << m_destY << m_destZ;
    }

    if (m_targetMask & TARGET_FLAG_STRING)
    {
        data << m_strTarget;
    }
}

Spell::Spell(Unit* caster, SpellEntry const* info, bool triggered, ObjectGuid originalCasterGUID, SpellEntry const* triggeredBy)
{
    MANGOS_ASSERT(caster != NULL && info != NULL);
    MANGOS_ASSERT(info == sSpellStore.LookupEntry(info->Id));   // `info` must be pointer to sSpellStore element

    m_spellInfo = info;
    m_triggeredBySpellInfo = triggeredBy;
    m_caster = caster;
    m_selfContainer = NULL;
    m_referencedFromCurrentSpell = false;
    m_executedCurrently = false;
    m_delayStart = 0;
    m_delayAtDamageCount = 0;

    m_applyMultiplierMask = 0;

    // Get data for type of attack
    m_attackType = GetWeaponAttackType(m_spellInfo);

    m_spellSchoolMask = GetSpellSchoolMask(info);           // Can be override for some spell (wand shoot for example)

    if (m_attackType == RANGED_ATTACK)
    {
        // wand case
        if (!(m_caster->getClassMask() & CLASSMASK_WAND_USERS) && m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            m_spellSchoolMask = GetSchoolMask(m_caster->GetWeaponDamageSchool(RANGED_ATTACK));
        }

    }
    // Set health leech amount to zero
    m_healthLeech = 0;

    m_originalCasterGUID = originalCasterGUID ? originalCasterGUID : m_caster->GetObjectGuid();

    UpdateOriginalCasterPointer();

    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        m_currentBasePoints[i] = m_spellInfo->CalculateSimpleValue(SpellEffectIndex(i));
    }

    m_spellState = SPELL_STATE_CREATED;

    m_castPositionX = m_castPositionY = m_castPositionZ = 0;
    m_TriggerSpells.clear();
    m_preCastSpells.clear();
    m_IsTriggeredSpell = triggered;
    // m_AreaAura = false;
    m_CastItem = NULL;

    unitTarget = NULL;
    itemTarget = NULL;
    gameObjTarget = NULL;
    focusObject = NULL;
    m_triggeredByAuraSpell  = NULL;

    // Auto Shot & Shoot (wand)
    m_autoRepeat = IsAutoRepeatRangedSpell(m_spellInfo);

    m_powerCost = 0;                                        // setup to correct value in Spell::prepare, don't must be used before.
    m_casttime = 0;                                         // setup to correct value in Spell::prepare, don't must be used before.
    m_timer = 0;                                            // will set to cast time in prepare
    m_duration = 0;

    m_needAliveTargetMask = 0;

    // determine reflection
    m_canReflect = false;

    if (m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MAGIC && !m_spellInfo->HasAttribute(SPELL_ATTR_EX2_IGNORE_LOS))
    {
        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (m_spellInfo->Effect[j] == 0)
            {
                continue;
            }

            if (!IsPositiveTarget(m_spellInfo->EffectImplicitTargetA[j], m_spellInfo->EffectImplicitTargetB[j]))
            {
                m_canReflect = true;
            }
            else
            {
                m_canReflect = m_spellInfo->HasAttribute(SPELL_ATTR_EX_CANT_BE_REFLECTED);
            }

            if (m_canReflect)
            {
                continue;
            }
            else
            {
                break;
            }
        }
    }

    CleanupTargetList();
}

Spell::~Spell()
{
}

/**
 * @brief Resolves which spell entry should be used for bonus level penalty calculations.
 *
 * @param spellProto The spell being evaluated.
 * @return The spell entry to use for penalty calculations.
 */
SpellEntry const* Spell::GetSpellBonusLevelPenaltySpell(SpellEntry const* spellProto) const
{
    if (!spellProto || !m_triggeredBySpellInfo)
    {
        return spellProto;
    }

    if (m_currentBasePoints[EFFECT_INDEX_1] == int32(m_triggeredBySpellInfo->Id) &&
        sSpellMgr.GetSpellBonusData(spellProto->Id))
    {
        return m_triggeredBySpellInfo;
    }

    return spellProto;
}

template<typename T>

/**
 * @brief Finds a nearby corpse-like world object matching the search predicate.
 *
 * @tparam T The corpse search predicate type.
 * @return The first matching world object, or null if none are found.
 */
WorldObject* Spell::FindCorpseUsing()
{
    // non-standard target selection
    SpellRangeEntry const* srange = sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex);
    float max_range = GetSpellMaxRange(srange);

    WorldObject* result = NULL;

    T u_check(m_caster, max_range);
    MaNGOS::WorldObjectSearcher<T> searcher(result, u_check);

    Cell::VisitGridObjects(m_caster, searcher, max_range);

    if (!result)
    {
        Cell::VisitWorldObjects(m_caster, searcher, max_range);
    }

    return result;
}

/**
 * @brief Builds the spell target lists for each active effect.
 */
void Spell::FillTargetMap()
{
    // TODO: ADD the correct target FILLS!!!!!!

    UnitList tmpUnitLists[MAX_EFFECT_INDEX];                // Stores the temporary Target Lists for each effect
    uint8 effToIndex[MAX_EFFECT_INDEX] = {0, 1, 2};         // Helper array, to link to another tmpUnitList, if the targets for both effects match
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        // not call for empty effect.
        // Also some spells use not used effect targets for store targets for dummy effect in triggered spells
        if (m_spellInfo->Effect[i] == SPELL_EFFECT_NONE)
        {
            continue;
        }

        // targets for TARGET_SCRIPT_COORDINATES (A) and TARGET_SCRIPT
        // for TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT (A) all is checked in Spell::CheckCast and in Spell::CheckItem
        // filled in Spell::CheckCast call
        if (m_spellInfo->EffectImplicitTargetA[i] == TARGET_SCRIPT_COORDINATES ||
            m_spellInfo->EffectImplicitTargetA[i] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
            (m_spellInfo->EffectImplicitTargetA[i] == TARGET_SCRIPT && m_spellInfo->EffectImplicitTargetB[i] != TARGET_SELF) ||
            (m_spellInfo->EffectImplicitTargetB[i] == TARGET_SCRIPT && m_spellInfo->EffectImplicitTargetA[i] != TARGET_SELF))
        {
            continue;
        }

        // TODO: find a way so this is not needed?
        // for area auras always add caster as target (needed for totems for example)
        if (IsAreaAuraEffect(m_spellInfo->Effect[i]))
        {
            AddUnitTarget(m_caster, SpellEffectIndex(i));
        }

        // no double fill for same targets
        for (int j = 0; j < i; ++j)
        {
            // Check if same target, but handle i.e. AreaAuras different
            if (m_spellInfo->EffectImplicitTargetA[i] == m_spellInfo->EffectImplicitTargetA[j] && m_spellInfo->EffectImplicitTargetB[i] == m_spellInfo->EffectImplicitTargetB[j] &&
                m_spellInfo->Effect[j] != SPELL_EFFECT_NONE &&
                !IsAreaAuraEffect(m_spellInfo->Effect[i]) && !IsAreaAuraEffect(m_spellInfo->Effect[j]))
                // Add further conditions here if required
            {
                effToIndex[i] = j;                          // effect i has same targeting list as effect j
                break;
            }
        }

        if (effToIndex[i] == i)                             // New target combination
        {
            // TargetA/TargetB dependent from each other, we not switch to full support this dependences
            // but need it support in some know cases
            switch (m_spellInfo->EffectImplicitTargetA[i])
            {
                case TARGET_NONE:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_NONE:
                            if (m_caster->GetObjectGuid().IsPet())
                            {
                                SetTargetMap(SpellEffectIndex(i), TARGET_SELF, tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            else
                            {
                                SetTargetMap(SpellEffectIndex(i), TARGET_EFFECT_SELECT, tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_SELF:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_NONE:                   // Fill Target based on A only
                            // Arcane Missiles have strange targeting for auras
                            // Gnomish Death Ray triggered 13280
                            if ((m_spellInfo->SpellFamilyName == SPELLFAMILY_MAGE && m_spellInfo->SpellFamilyFlags & UI64LIT(0x00000800)) ||
                                (m_spellInfo->Id == 13280))
                            {
                                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                                {
                                    if (Unit* target = sObjectAccessor.Instance().GetUnit(*m_caster, ((Player*)m_caster)->GetSelectionGuid()))
                                    {
                                        if (!m_caster->IsFriendlyTo(target))
                                        {
                                            tmpUnitLists[i /*==effToIndex[i]*/].push_back(target);
                                        }
                                    }
                                }
                            }
                            else
                            {
                                SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            break;
                        case TARGET_EFFECT_SELECT:
                        case TARGET_SCRIPT:                 // B-target only used with CheckCast here
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        case TARGET_AREAEFFECT_INSTANT:     // use B case that not dependent from from A in fact
                            if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) == 0)
                            {
                                m_targets.setDestination(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ());
                            }
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_EFFECT_SELECT:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        // dest point setup required
                        case TARGET_AREAEFFECT_INSTANT:
                        case TARGET_AREAEFFECT_CUSTOM:
                        case TARGET_ALL_ENEMY_IN_AREA:
                        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
                        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
                        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
                        case TARGET_AREAEFFECT_GO_AROUND_DEST:
                            // triggered spells get dest point from default target set, ignore it
                            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) || m_IsTriggeredSpell)
                            {
                                if (WorldObject* castObject = GetCastingObject())
                                {
                                    m_targets.setDestination(castObject->GetPositionX(), castObject->GetPositionY(), castObject->GetPositionZ());
                                }
                            }
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        // target pre-selection required
                        case TARGET_INNKEEPER_COORDINATES:
                        case TARGET_TABLE_X_Y_Z_COORDINATES:
                        case TARGET_CASTER_COORDINATES:
                        case TARGET_SCRIPT_COORDINATES:
                        case TARGET_CURRENT_ENEMY_COORDINATES:
                        case TARGET_DUELVSPLAYER_COORDINATES:
                            // need some target for processing
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_CASTER_COORDINATES:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_ALL_ENEMY_IN_AREA:
                            // Note: this hack with search required until GO casting not implemented
                            // environment damage spells already have around enemies targeting but this not help in case nonexistent GO casting support
                            // currently each enemy selected explicitly and self cast damage
                            if (m_spellInfo->Effect[i] == SPELL_EFFECT_ENVIRONMENTAL_DAMAGE)
                            {
                                if (m_targets.getUnitTarget())
                                {
                                    tmpUnitLists[i /*==effToIndex[i]*/].push_back(m_targets.getUnitTarget());
                                }
                            }
                            else
                            {
                                SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                                SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            }
                            break;
                        case TARGET_NONE:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            tmpUnitLists[i /*==effToIndex[i]*/].push_back(m_caster);
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_TABLE_X_Y_Z_COORDINATES:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_NONE:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);

                            // need some target for processing
                            SetTargetMap(SpellEffectIndex(i), TARGET_EFFECT_SELECT, tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        case TARGET_AREAEFFECT_INSTANT:     // All 17/7 pairs used for dest teleportation, A processed in effect code
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_DUELVSPLAYER_COORDINATES:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            if (Unit* currentTarget = m_targets.getUnitTarget())
                            {
                                tmpUnitLists[i /*==effToIndex[i]*/].push_back(currentTarget);
                            }
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
                case TARGET_SCRIPT:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_SELF:
                            // Fill target based on B only, A is only used with CheckCast here.
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            break;
                    }
                    break;
                default:
                    switch (m_spellInfo->EffectImplicitTargetB[i])
                    {
                        case TARGET_NONE:
                        case TARGET_EFFECT_SELECT:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        case TARGET_SCRIPT_COORDINATES:     // B case filled in CheckCast but we need fill unit list base at A case
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                        default:
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetA[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            SetTargetMap(SpellEffectIndex(i), m_spellInfo->EffectImplicitTargetB[i], tmpUnitLists[i /*==effToIndex[i]*/]);
                            break;
                    }
                    break;
            }
        }

        if (m_caster->GetTypeId() == TYPEID_PLAYER)
        {
            Player* me = (Player*)m_caster;
            for (UnitList::const_iterator itr = tmpUnitLists[effToIndex[i]].begin(); itr != tmpUnitLists[effToIndex[i]].end(); ++itr)
            {
                Player* targetOwner = (*itr)->GetCharmerOrOwnerPlayerOrPlayerItself();
                if (targetOwner && targetOwner != me && targetOwner->IsPvP() && !me->IsInDuelWith(targetOwner))
                {
                    me->UpdatePvP(true);
                    me->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_ENTER_PVP_COMBAT);
                    break;
                }
            }
        }

        for (UnitList::iterator itr = tmpUnitLists[effToIndex[i]].begin(); itr != tmpUnitLists[effToIndex[i]].end();)
        {
            if (!CheckTarget(*itr, SpellEffectIndex(i)))
            {
                itr = tmpUnitLists[effToIndex[i]].erase(itr);
                continue;
            }
            else
            {
                ++itr;
            }
        }

        for (UnitList::const_iterator iunit = tmpUnitLists[effToIndex[i]].begin(); iunit != tmpUnitLists[effToIndex[i]].end(); ++iunit)
        {
            AddUnitTarget((*iunit), SpellEffectIndex(i));
        }
    }
}

/**
 * @brief Prepares proc-trigger metadata for the current spell cast.
 */
void Spell::prepareDataForTriggerSystem()
{
    //==========================================================================================
    // Now fill data for trigger system, need know:
    // an spell trigger another or not ( m_canTrigger )
    // Create base triggers flags for Attacker and Victim ( m_procAttacker and  m_procVictim)
    //==========================================================================================
    // Fill flag can spell trigger or not
    // TODO: possible exist spell attribute for this
    m_canTrigger = false;

    if (m_CastItem)
    {
        m_canTrigger = false;                                // Do not trigger from item cast spell
    }
    else if (!m_IsTriggeredSpell)
    {
        m_canTrigger = true;                                 // Normal cast - can trigger
    }
    else if (!m_triggeredByAuraSpell)
    {
        m_canTrigger = true;                                 // Triggered from SPELL_EFFECT_TRIGGER_SPELL - can trigger
    }

    if (!m_canTrigger)                                      // Exceptions (some periodic triggers)
    {
        switch (m_spellInfo->SpellFamilyName)
        {
            case SPELLFAMILY_MAGE:
                // Arcane Missiles / Blizzard triggers need do it
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000000000200080)))
                {
                    m_canTrigger = true;
                }
                break;
            case SPELLFAMILY_WARLOCK:
                // For Hellfire Effect / Rain of Fire / Seed of Corruption triggers need do it
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000800000000060)))
                {
                    m_canTrigger = true;
                }
                break;
            case SPELLFAMILY_HUNTER:
                // Hunter Explosive Trap Effect/Immolation Trap Effect/Frost Trap Aura/Snake Trap Effect
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0000200000000014)))
                {
                    m_canTrigger = true;
                }
                break;
            case SPELLFAMILY_PALADIN:
                // For Holy Shock triggers need do it
                if (m_spellInfo->IsFitToFamilyMask(UI64LIT(0x0001000000200000)))
                {
                    m_canTrigger = true;
                }
                break;
            default:
                break;
        }
    }

    // Get data for type of attack and fill base info for trigger
    switch (m_spellInfo->DmgClass)
    {
        case SPELL_DAMAGE_CLASS_MELEE:
            m_procAttacker = PROC_FLAG_SUCCESSFUL_MELEE_SPELL_HIT;
            if (m_attackType == OFF_ATTACK)
            {
                m_procAttacker |= PROC_FLAG_SUCCESSFUL_OFFHAND_HIT;
            }
            m_procVictim   = PROC_FLAG_TAKEN_MELEE_SPELL_HIT;
            break;
        case SPELL_DAMAGE_CLASS_RANGED:
            // Auto attack
            if (m_spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG))
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_HIT;
            }
            else // Ranged spell attack
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_SPELL_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_SPELL_HIT;
            }
            break;
        default:
            if (IsPositiveSpell(m_spellInfo->Id))           // Check for positive spell
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_POSITIVE_SPELL;
                m_procVictim   = PROC_FLAG_TAKEN_POSITIVE_SPELL;
            }
            else if (m_spellInfo->HasAttribute(SPELL_ATTR_EX2_AUTOREPEAT_FLAG))   // Wands auto attack
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_RANGED_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_RANGED_HIT;
            }
            else                                           // Negative spell
            {
                m_procAttacker = PROC_FLAG_SUCCESSFUL_NEGATIVE_SPELL_HIT;
                m_procVictim   = PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT;
            }
            break;
    }

    // some negative spells have positive effects to another or same targets
    // avoid triggering negative hit for only positive targets
    m_negativeEffectMask = 0x0;
    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
    {
        if (m_spellInfo->Effect[i] != SPELL_EFFECT_NONE && !IsPositiveEffect(m_spellInfo, SpellEffectIndex(i)))
        {
            m_negativeEffectMask |= (1 << i);
        }
    }

    // Hunter traps spells (for Entrapment trigger)
    // Gives your Immolation Trap, Frost Trap, Explosive Trap, and Snake Trap ....
    if (m_spellInfo->SpellFamilyName == SPELLFAMILY_HUNTER && m_spellInfo->SpellFamilyFlags & UI64LIT(0x000020000000001C))
    {
        m_procAttacker |= PROC_FLAG_ON_TRAP_ACTIVATION;
    }
}

/**
 * @brief Clears all accumulated target lists and delay tracking.
 */
void Spell::CleanupTargetList()
{
    m_UniqueTargetInfo.clear();
    m_UniqueGOTargetInfo.clear();
    m_UniqueItemInfo.clear();
    m_delayMoment = 0;
}

/**
 * @brief Adds a unit target entry for a spell effect.
 *
 * @param pVictim The unit target.
 * @param effIndex The effect index being applied.
 */
void Spell::AddUnitTarget(Unit* pVictim, SpellEffectIndex effIndex)
{
    if (m_spellInfo->Effect[effIndex] == 0)
    {
        return;
    }

    // Check for effect immune skip if immuned
    bool immuned = pVictim->IsImmuneToSpellEffect(m_spellInfo, effIndex, pVictim == m_caster);

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    // Lookup target in already in list
    for (TargetList::iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (targetGUID == ihit->targetGUID)                 // Found in list
        {
            if (!immuned)
            {
                ihit->effectMask |= 1 << effIndex;          // Add only effect mask if not immuned
            }
            return;
        }
    }

    // This is new target calculate data for him

    // Get spell hit result on target
    TargetInfo target;
    target.targetGUID = targetGUID;                         // Store target GUID
    target.effectMask = immuned ? 0 : (1 << effIndex);      // Store index of effect if not immuned
    target.processed  = false;                              // Effects not applied on target

    // Calculate hit result
    target.missCondition = m_caster->SpellHitResult(pVictim, m_spellInfo, m_canReflect);

    // spell fly from visual cast object
    WorldObject* affectiveObject = GetAffectiveCasterObject();

    // Spell have speed (possible inherited from triggering spell) - need calculate incoming time
    float speed = m_spellInfo->speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->speed : m_spellInfo->speed;
    if (speed > 0.0f && affectiveObject && (pVictim != affectiveObject || (m_targets.m_targetMask & (TARGET_FLAG_SOURCE_LOCATION | TARGET_FLAG_DEST_LOCATION))))
    {
        // calculate spell incoming interval
        float dist;                                         // distance to impact
        if (pVictim == affectiveObject)                     // Calculate dist to destination target also for self-cast spells
        {
            if (m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION)
            {
                dist = affectiveObject->GetDistance(m_targets.m_destX, m_targets.m_destY, m_targets.m_destZ);
            }
            else                                            // Must have Source Target
            {
                dist = affectiveObject->GetDistance(m_targets.m_srcX, m_targets.m_srcY, m_targets.m_srcZ);
            }
        }
        else                                                // normal unit target, take distance
        {
            dist = affectiveObject->GetDistance(pVictim->GetPositionX(), pVictim->GetPositionY(), pVictim->GetPositionZ());
        }

        if (dist < 5.0f)
        {
            dist = 5.0f;
        }
        target.timeDelay = (uint64) floor(dist / speed * 1000.0f);

        // Calculate minimum incoming time
        if (m_delayMoment == 0 || m_delayMoment > target.timeDelay)
        {
            m_delayMoment = target.timeDelay;
        }
    }
    else
    {
        target.timeDelay = UI64LIT(0);
    }

    // If target reflect spell back to caster
    if (target.missCondition == SPELL_MISS_REFLECT)
    {
        // Calculate reflected spell result on caster
        target.reflectResult =  m_caster->SpellHitResult(m_caster, m_spellInfo, m_canReflect);

        if (target.reflectResult == SPELL_MISS_REFLECT)     // Impossible reflect again, so simply deflect spell
        {
            target.reflectResult = SPELL_MISS_PARRY;
        }

        // Increase time interval for reflected spells by 1.5
        target.timeDelay += target.timeDelay >> 1;
    }
    else
    {
        target.reflectResult = SPELL_MISS_NONE;
    }

    // Add target to list
    m_UniqueTargetInfo.push_back(target);
}

/**
 * @brief Resolves and adds a unit target by guid for a spell effect.
 *
 * @param unitGuid The unit guid to resolve.
 * @param effIndex The effect index being applied.
 */
void Spell::AddUnitTarget(ObjectGuid unitGuid, SpellEffectIndex effIndex)
{
    if (Unit* unit = m_caster->GetObjectGuid() == unitGuid ? m_caster : sObjectAccessor.GetUnit(*m_caster, unitGuid))
    {
        AddUnitTarget(unit, effIndex);
    }
}

/**
 * @brief Adds a game object target entry for a spell effect.
 *
 * @param pVictim The game object target.
 * @param effIndex The effect index being applied.
 */
void Spell::AddGOTarget(GameObject* pVictim, SpellEffectIndex effIndex)
{
    if (m_spellInfo->Effect[effIndex] == 0)
    {
        return;
    }

    ObjectGuid targetGUID = pVictim->GetObjectGuid();

    // Lookup target in already in list
    for (GOTargetList::iterator ihit = m_UniqueGOTargetInfo.begin(); ihit != m_UniqueGOTargetInfo.end(); ++ihit)
    {
        if (targetGUID == ihit->targetGUID)                 // Found in list
        {
            ihit->effectMask |= (1 << effIndex);            // Add only effect mask
            return;
        }
    }

    // This is new target calculate data for him

    GOTargetInfo target;
    target.targetGUID = targetGUID;
    target.effectMask = (1 << effIndex);
    target.processed  = false;                              // Effects not apply on target

    // spell fly from visual cast object
    WorldObject* affectiveObject = GetAffectiveCasterObject();

    // Spell can have speed - need calculate incoming time
    float speed = m_spellInfo->speed == 0.0f && m_triggeredBySpellInfo ? m_triggeredBySpellInfo->speed : m_spellInfo->speed;
    if (speed > 0.0f && affectiveObject && pVictim != affectiveObject)
    {
        // calculate spell incoming interval
        float dist = affectiveObject->GetDistance(pVictim->GetPositionX(), pVictim->GetPositionY(), pVictim->GetPositionZ());
        if (dist < 5.0f)
        {
            dist = 5.0f;
        }
        target.timeDelay = (uint64) floor(dist / speed * 1000.0f);
        if (m_delayMoment == 0 || m_delayMoment > target.timeDelay)
        {
            m_delayMoment = target.timeDelay;
        }
    }
    else
    {
        target.timeDelay = UI64LIT(0);
    }

    // Add target to list
    m_UniqueGOTargetInfo.push_back(target);
}

/**
 * @brief Resolves and adds a game object target by guid for a spell effect.
 *
 * @param goGuid The game object guid to resolve.
 * @param effIndex The effect index being applied.
 */
void Spell::AddGOTarget(ObjectGuid goGuid, SpellEffectIndex effIndex)
{
    if (GameObject* go = m_caster->GetMap()->GetGameObject(goGuid))
    {
        AddGOTarget(go, effIndex);
    }
}

/**
 * @brief Adds an item target entry for a spell effect.
 *
 * @param pitem The item target.
 * @param effIndex The effect index being applied.
 */
void Spell::AddItemTarget(Item* pitem, SpellEffectIndex effIndex)
{
    if (m_spellInfo->Effect[effIndex] == 0)
    {
        return;
    }

    // Lookup target in already in list
    for (ItemTargetList::iterator ihit = m_UniqueItemInfo.begin(); ihit != m_UniqueItemInfo.end(); ++ihit)
    {
        if (pitem == ihit->item)                            // Found in list
        {
            ihit->effectMask |= (1 << effIndex);            // Add only effect mask
            return;
        }
    }

    // This is new target add data

    ItemTargetInfo target;
    target.item       = pitem;
    target.effectMask = (1 << effIndex);
    m_UniqueItemInfo.push_back(target);
}







/**
 * @brief Checks whether required alive targets are present in the current target list.
 *
 * @return True if all required effects have a valid alive target; otherwise, false.
 */
bool Spell::IsAliveUnitPresentInTargetList()
{
    // Not need check return true
    if (m_needAliveTargetMask == 0)
    {
        return true;
    }

    uint8 needAliveTargetMask = m_needAliveTargetMask;

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if (ihit->missCondition == SPELL_MISS_NONE && (needAliveTargetMask & ihit->effectMask))
        {
            Unit* unit = m_caster->GetObjectGuid() == ihit->targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, ihit->targetGUID);

            // either unit is alive and normal spell, or unit dead and deathonly-spell
            if (unit && (unit->IsAlive() != IsDeathOnlySpell(m_spellInfo)))
            {
                needAliveTargetMask &= ~ihit->effectMask;   // remove from need alive mask effect that have alive target
            }
        }
    }

    // is all effects from m_needAliveTargetMask have alive targets
    return needAliveTargetMask == 0;
}

// Helper for Chain Healing
// Spell target first
// Raidmates then descending by injury suffered (MaxHealth - Health)
// Other players/mobs then descending by injury suffered (MaxHealth - Health)
struct ChainHealingOrder
{
    const Unit* MainTarget;
    ChainHealingOrder(Unit const* Target) : MainTarget(Target) {};
    // functor for operator ">"
    bool operator()(Unit const* _Left, Unit const* _Right) const
    {
        return (ChainHealingHash(_Left) < ChainHealingHash(_Right));
    }

    int32 ChainHealingHash(Unit const* Target) const
    {
        if (Target == MainTarget)
        {
            return 0;
        }
        else if (Target->GetTypeId() == TYPEID_PLAYER && MainTarget->GetTypeId() == TYPEID_PLAYER &&
            ((Player const*)Target)->IsInSameRaidWith((Player const*)MainTarget))
        {
            if (Target->GetHealth() == Target->GetMaxHealth())
            {
                return 40000;
            }
            else
            {
                return 20000 - Target->GetMaxHealth() + Target->GetHealth();
            }
        }
        else
        {
            return 40000 - Target->GetMaxHealth() + Target->GetHealth();
        }
    }
};

class ChainHealingFullHealth
{
    public:
        const Unit* MainTarget;
        ChainHealingFullHealth(const Unit* Target) : MainTarget(Target) {};

        bool operator()(const Unit* Target)
        {
            return (Target != MainTarget && Target->GetHealth() == Target->GetMaxHealth());
        }
};

// Helper for targets nearest to the spell target
// The spell target is always first unless there is a target at _completely_ the same position (unbelievable case)
struct TargetDistanceOrderNear
{
    const Unit* MainTarget;
    TargetDistanceOrderNear(const Unit* Target) : MainTarget(Target) {};
    // functor for operator ">"
    bool operator()(const Unit* _Left, const Unit* _Right) const
    {
        return MainTarget->GetDistanceOrder(_Left, _Right);
    }
};

/**
 * @brief Populates a unit target list for a specific implicit target mode.
 *
 * @param effIndex The effect index being processed.
 * @param targetMode The implicit target mode.
 * @param targetUnitMap The unit list being populated.
 */
void Spell::SetTargetMap(SpellEffectIndex effIndex, uint32 targetMode, UnitList& targetUnitMap)
{
    float radius;
    uint32 EffectChainTarget = m_spellInfo->EffectChainTarget[effIndex];
    uint32 unMaxTargets = m_spellInfo->MaxAffectedTargets;  // Get spell max affected targets

    GetSpellRangeAndRadius(effIndex, radius, EffectChainTarget, unMaxTargets);

    std::list<GameObject*> tempTargetGOList;

    switch (targetMode)
    {
        case TARGET_TOTEM_EARTH:
        case TARGET_TOTEM_WATER:
        case TARGET_TOTEM_AIR:
        case TARGET_TOTEM_FIRE:
        {
            float angle = m_caster->GetOrientation();
            switch (targetMode)
            {
                case TARGET_TOTEM_FIRE:  angle += M_PI_F * 0.25f; break;            // front - left
                case TARGET_TOTEM_AIR:   angle += M_PI_F * 0.75f; break;            // back  - left
                case TARGET_TOTEM_WATER: angle += M_PI_F * 1.25f; break;            // back  - right
                case TARGET_TOTEM_EARTH: angle += M_PI_F * 1.75f; break;            // front - right
            }

            float x, y;
            float z = m_caster->GetPositionZ();
            // Do not search for a free spot. TODO: Should there be searched for a free spot. There was once a discussion that in case this space was impossible (LOS) m_caster's position should be used.
            // TODO Bring this back to memory and search for it!
            m_caster->GetNearPoint2D(x, y, radius, angle);
            m_caster->UpdateAllowedPositionZ(x, y, z);
            m_targets.setDestination(x, y, z);

            // Add Summoner
            targetUnitMap.push_back(m_caster);
            break;
        }
        case TARGET_SELF:
            targetUnitMap.push_back(m_caster);
            break;
        case TARGET_RANDOM_ENEMY_CHAIN_IN_AREA:
        case TARGET_RANDOM_FRIEND_CHAIN_IN_AREA:
        case TARGET_RANDOM_UNIT_CHAIN_IN_AREA:
        {
            m_targets.m_targetMask = 0;
            unMaxTargets = EffectChainTarget;
            float max_range = radius + unMaxTargets * CHAIN_SPELL_JUMP_RADIUS;

            UnitList tempTargetUnitMap;

            switch (targetMode)
            {
                case TARGET_RANDOM_ENEMY_CHAIN_IN_AREA:
                {
                    MaNGOS::AnyAoETargetUnitInObjectRangeCheck u_check(m_caster, max_range);
                    MaNGOS::UnitListSearcher<MaNGOS::AnyAoETargetUnitInObjectRangeCheck> searcher(tempTargetUnitMap, u_check);
                    Cell::VisitAllObjects(m_caster, searcher, max_range);
                    break;
                }
                case TARGET_RANDOM_UNIT_CHAIN_IN_AREA: // This works the same as Target_random_friend_chain_in_area but is named differently for some reason
                case TARGET_RANDOM_FRIEND_CHAIN_IN_AREA:
                {
                    MaNGOS::AnyFriendlyUnitInObjectRangeCheck u_check(m_caster, max_range);
                    MaNGOS::UnitListSearcher<MaNGOS::AnyFriendlyUnitInObjectRangeCheck> searcher(tempTargetUnitMap, u_check);
                    Cell::VisitAllObjects(m_caster, searcher, max_range);
                    break;
                }
            }

            if (tempTargetUnitMap.empty())
            {
                break;
            }

            tempTargetUnitMap.sort(TargetDistanceOrderNear(m_caster));

            // Now to get us a random target that's in the initial range of the spell
            uint32 t = 0;
            UnitList::iterator itr = tempTargetUnitMap.begin();
            while (itr != tempTargetUnitMap.end() && (*itr)->IsWithinDist(m_caster, radius))
            {
                ++t, ++itr;
            }

            if (!t)
            {
                break;
            }

            itr = tempTargetUnitMap.begin();
            std::advance(itr, rand() % t);
            Unit* pUnitTarget = *itr;
            targetUnitMap.push_back(pUnitTarget);

            tempTargetUnitMap.erase(itr);

            tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

            t = unMaxTargets - 1;
            Unit* prev = pUnitTarget;
            UnitList::iterator next = tempTargetUnitMap.begin();

            while (t && next != tempTargetUnitMap.end())
            {
                if (!prev->IsWithinDist(*next, CHAIN_SPELL_JUMP_RADIUS))
                {
                    break;
                }

                if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, NULL, SPELL_DISABLE_LOS) && !prev->IsWithinLOSInMap(*next))
                {
                    ++next;
                    continue;
                }
                prev = *next;
                targetUnitMap.push_back(prev);
                tempTargetUnitMap.erase(next);
                tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                next = tempTargetUnitMap.begin();
                --t;
            }
            break;
        }
        case TARGET_PET:
        {
            Pet* tmpUnit = m_caster->GetPet();
            if (!tmpUnit)
            {
                break;
            }
            targetUnitMap.push_back(tmpUnit);
            break;
        }
        case TARGET_CHAIN_DAMAGE:
        {
            if (EffectChainTarget <= 1)
            {
                if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(m_targets.getUnitTarget(), this, effIndex))
                {
                    m_targets.setUnitTarget(pUnitTarget);
                    targetUnitMap.push_back(pUnitTarget);
                }
            }
            else
            {
                Unit* pUnitTarget = m_targets.getUnitTarget();
                WorldObject* originalCaster = GetAffectiveCasterObject();
                if (!pUnitTarget || !originalCaster)
                {
                    break;
                }

                unMaxTargets = EffectChainTarget;

                float max_range;
                if (m_spellInfo->DmgClass == SPELL_DAMAGE_CLASS_MELEE)
                {
                    max_range = radius;
                }
                else
                    // FIXME: This very like horrible hack and wrong for most spells
                {
                    max_range = radius + unMaxTargets * CHAIN_SPELL_JUMP_RADIUS;
                }

                UnitList tempTargetUnitMap;
                {
                    MaNGOS::AnyAoEVisibleTargetUnitInObjectRangeCheck u_check(pUnitTarget, originalCaster, max_range);
                    MaNGOS::UnitListSearcher<MaNGOS::AnyAoEVisibleTargetUnitInObjectRangeCheck> searcher(tempTargetUnitMap, u_check);
                    Cell::VisitAllObjects(m_caster, searcher, max_range);
                }

                if (tempTargetUnitMap.empty())
                {
                    break;
                }

                tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

                if (*tempTargetUnitMap.begin() == pUnitTarget)
                {
                    tempTargetUnitMap.erase(tempTargetUnitMap.begin());
                }

                targetUnitMap.push_back(pUnitTarget);
                uint32 t = unMaxTargets - 1;
                Unit* prev = pUnitTarget;
                UnitList::iterator next = tempTargetUnitMap.begin();

                while (t && next != tempTargetUnitMap.end())
                {
                    if (!prev->IsWithinDist(*next, CHAIN_SPELL_JUMP_RADIUS))
                    {
                        break;
                    }

                    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, NULL, SPELL_DISABLE_LOS) && !prev->IsWithinLOSInMap(*next))
                    {
                        ++next;
                        continue;
                    }

                    prev = *next;
                    targetUnitMap.push_back(prev);
                    tempTargetUnitMap.erase(next);
                    tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                    next = tempTargetUnitMap.begin();

                    --t;
                }
            }
            break;
        }
        case TARGET_ALL_ENEMY_IN_AREA:
            FillAreaTargets(targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_AOE_DAMAGE);
            break;
        case TARGET_AREAEFFECT_INSTANT:
        {
            SpellTargets targetB = SPELL_TARGETS_AOE_DAMAGE;
            switch (m_spellInfo->Effect[effIndex])
            {
                case SPELL_EFFECT_QUEST_COMPLETE:
                    targetB = SPELL_TARGETS_ALL;
                    break;
                default:
                    // Select friendly targets for positive effect
                    if (IsPositiveEffect(m_spellInfo, effIndex))
                    {
                        targetB = SPELL_TARGETS_FRIENDLY;
                    }
                    break;
            }

            UnitList tempTargetUnitMap;
            SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->Id);

            // fill real target list if no spell script target defined
            FillAreaTargets(bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap,
                radius, PUSH_DEST_CENTER, bounds.first != bounds.second ? SPELL_TARGETS_ALL : targetB);

            if (!tempTargetUnitMap.empty())
            {
                for (UnitList::const_iterator iter = tempTargetUnitMap.begin(); iter != tempTargetUnitMap.end(); ++iter)
                {
                    if ((*iter)->GetTypeId() != TYPEID_UNIT)
                    {
                        continue;
                    }

                    for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                    {
                        if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                        {
                            continue;
                        }

                        // only creature entries supported for this target type
                        if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                        {
                            continue;
                        }

                        if ((*iter)->GetEntry() == i_spellST->targetEntry)
                        {
                            if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)(*iter))->IsCorpse())
                            {
                                targetUnitMap.push_back((*iter));
                            }
                            else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && (*iter)->IsAlive())
                            {
                                targetUnitMap.push_back((*iter));
                            }

                            break;
                        }
                    }
                }
            }
            break;
        }
        case TARGET_AREAEFFECT_CUSTOM:
        {
            if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_PERSISTENT_AREA_AURA)
            {
                break;
            }
            else if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_SUMMON)
            {
                targetUnitMap.push_back(m_caster);
                break;
            }

            UnitList tempTargetUnitMap;
            SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->Id);
            // fill real target list if no spell script target defined
            FillAreaTargets(bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_ALL);

            if (!tempTargetUnitMap.empty())
            {
                for (UnitList::const_iterator iter = tempTargetUnitMap.begin(); iter != tempTargetUnitMap.end(); ++iter)
                {
                    if ((*iter)->GetTypeId() != TYPEID_UNIT)
                    {
                        continue;
                    }

                    for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                    {
                        if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                        {
                            continue;
                        }

                        // only creature entries supported for this target type
                        if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                        {
                            continue;
                        }

                        if ((*iter)->GetEntry() == i_spellST->targetEntry)
                        {
                            if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)(*iter))->IsCorpse())
                            {
                                targetUnitMap.push_back((*iter));
                            }
                            else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && (*iter)->IsAlive())
                            {
                                targetUnitMap.push_back((*iter));
                            }

                            break;
                        }
                    }
                }
            }
            else
            {
                // remove not targetable units if spell has no script targets
                for (UnitList::iterator itr = targetUnitMap.begin(); itr != targetUnitMap.end();)
                {
                    if (!(*itr)->IsTargetableForAttack(m_spellInfo->HasAttribute(SPELL_ATTR_EX3_CAST_ON_DEAD)))
                    {
                        targetUnitMap.erase(itr++);
                    }
                    else
                    {
                        ++itr;
                    }
                }
            }
            break;
        }
        case TARGET_AREAEFFECT_GO_AROUND_SOURCE:
        case TARGET_AREAEFFECT_GO_AROUND_DEST:
        {
            float x, y, z;

            if (targetMode == TARGET_AREAEFFECT_GO_AROUND_SOURCE)
            {
                if (m_targets.m_targetMask & TARGET_FLAG_SOURCE_LOCATION)
                {
                    m_targets.getSource(x, y, z);
                }
                else
                {
                    m_caster->GetPosition(x, y, z);
                }
            }
            else
            {
                m_targets.getDestination(x, y, z);
            }

            // It may be possible to fill targets for some spell effects
            // automatically (SPELL_EFFECT_WMO_REPAIR(88) for example) but
            // for some/most spells we clearly need/want to limit with spell_target_script

            // Some spells untested, for affected GO type 33. May need further adjustments for spells related.

            SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->Id);
            for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
            {
                if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                {
                    continue;
                }

                if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                {
                    // search all GO's with entry, within range of m_destN
                    MaNGOS::GameObjectEntryInPosRangeCheck go_check(*m_caster, i_spellST->targetEntry, x, y, z, radius);
                    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectEntryInPosRangeCheck> checker(tempTargetGOList, go_check);
                    Cell::VisitGridObjects(m_caster, checker, radius);
                }
            }

            break;
        }
        case TARGET_ALL_ENEMY_IN_AREA_INSTANT:
        {
            // targets the ground, not the units in the area
            switch (m_spellInfo->Effect[effIndex])
            {
                case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                    break;
                case SPELL_EFFECT_SUMMON:
                    targetUnitMap.push_back(m_caster);
                    break;
                default:
                    FillAreaTargets(targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_AOE_DAMAGE);
                    break;
            }
            break;
        }
        case TARGET_DUELVSPLAYER_COORDINATES:
        {
            if (Unit* currentTarget = m_targets.getUnitTarget())
            {
                m_targets.setDestination(currentTarget->GetPositionX(), currentTarget->GetPositionY(), currentTarget->GetPositionZ());
            }
            break;
        }
        case TARGET_ALL_PARTY_AROUND_CASTER:
        case TARGET_ALL_PARTY_AROUND_CASTER_2:
        case TARGET_ALL_PARTY:
        {
            FillRaidOrPartyTargets(targetUnitMap, m_caster, radius, false, true, true);
            break;
        }
        case TARGET_ALL_RAID_AROUND_CASTER:
        {
            FillRaidOrPartyTargets(targetUnitMap, m_caster, radius, true, true, false);
            break;
        }
        case TARGET_SINGLE_FRIEND:
        case TARGET_SINGLE_FRIEND_2:
            if (m_targets.getUnitTarget())
            {
                targetUnitMap.push_back(m_targets.getUnitTarget());
            }
            break;
        case TARGET_CASTER_COORDINATES:
        {
            // Check original caster is GO - set its coordinates as src cast
            if (WorldObject* caster = GetCastingObject())
            {
                m_targets.setSource(caster->GetPositionX(), caster->GetPositionY(), caster->GetPositionZ());
            }
            break;
        }
        case TARGET_ALL_HOSTILE_UNITS_AROUND_CASTER:
            FillAreaTargets(targetUnitMap, radius, PUSH_SELF_CENTER, SPELL_TARGETS_HOSTILE);
            break;
        case TARGET_ALL_FRIENDLY_UNITS_AROUND_CASTER:
            // selected friendly units (for casting objects) around casting object
            FillAreaTargets(targetUnitMap, radius, PUSH_SELF_CENTER, SPELL_TARGETS_FRIENDLY, GetCastingObject());
            break;
        case TARGET_ALL_FRIENDLY_UNITS_IN_AREA:
            FillAreaTargets(targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_FRIENDLY);
            break;
        // TARGET_SINGLE_PARTY means that the spells can only be casted on a party member and not on the caster (some seals, fire shield from imp, etc..)
        case TARGET_SINGLE_PARTY:
        {
            Unit* target = m_targets.getUnitTarget();
            // Those spells apparently can't be casted on the caster.
            if (target && target != m_caster)
            {
                // Can only be casted on group's members or its pets
                Group*  pGroup = NULL;

                Unit* owner = m_caster->GetCharmerOrOwner();
                Unit* targetOwner = target->GetCharmerOrOwner();
                if (owner)
                {
                    if (owner->GetTypeId() == TYPEID_PLAYER)
                    {
                        if (target == owner)
                        {
                            targetUnitMap.push_back(target);
                            break;
                        }
                        pGroup = ((Player*)owner)->GetGroup();
                    }
                }
                else if (m_caster->GetTypeId() == TYPEID_PLAYER)
                {
                    if (targetOwner == m_caster && target->GetTypeId() == TYPEID_UNIT && ((Creature*)target)->IsPet())
                    {
                        targetUnitMap.push_back(target);
                        break;
                    }
                    pGroup = ((Player*)m_caster)->GetGroup();
                }

                if (pGroup)
                {
                    // Our target can also be a player's pet who's grouped with us or our pet. But can't be controlled player
                    if (targetOwner)
                    {
                        if (targetOwner->GetTypeId() == TYPEID_PLAYER &&
                            target->GetTypeId() == TYPEID_UNIT && (((Creature*)target)->IsPet()) &&
                            target->GetOwnerGuid() == targetOwner->GetObjectGuid() &&
                            pGroup->IsMember(((Player*)targetOwner)->GetObjectGuid()))
                        {
                            targetUnitMap.push_back(target);
                        }
                    }
                    // 1Our target can be a player who is on our group
                    else if (target->GetTypeId() == TYPEID_PLAYER && pGroup->IsMember(((Player*)target)->GetObjectGuid()))
                    {
                        targetUnitMap.push_back(target);
                    }
                }
            }
            break;
        }
        case TARGET_GAMEOBJECT:
            if (m_targets.getGOTarget())
            {
                AddGOTarget(m_targets.getGOTarget(), effIndex);
            }
            break;
        case TARGET_IN_FRONT_OF_CASTER:
        {
            SpellNotifyPushType pushType = PUSH_IN_FRONT;
            switch (m_spellInfo->SpellVisual)            // Some spell require a different target fill
            {
                case 3879: pushType = PUSH_IN_BACK;     break;
                case 7441: pushType = PUSH_IN_FRONT_15; break;
            }
            FillAreaTargets(targetUnitMap, radius, pushType, SPELL_TARGETS_AOE_DAMAGE);
            break;
        }
        case TARGET_LARGE_FRONTAL_CONE:
            FillAreaTargets(targetUnitMap, radius, PUSH_IN_FRONT_90, SPELL_TARGETS_AOE_DAMAGE);
            break;
        case TARGET_NARROW_FRONTAL_CONE:
        {
            SpellTargets targetB = SPELL_TARGETS_AOE_DAMAGE;

            if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_SCRIPT_EFFECT)
            {
                targetB = SPELL_TARGETS_ALL;
            }

            UnitList tempTargetUnitMap;
            SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(m_spellInfo->Id);

            // fill real target list if no spell script target defined
            FillAreaTargets(bounds.first != bounds.second ? tempTargetUnitMap : targetUnitMap,
                radius, PUSH_IN_FRONT_15, bounds.first != bounds.second ? SPELL_TARGETS_ALL : targetB);

            if (!tempTargetUnitMap.empty())
            {
                for (UnitList::const_iterator iter = tempTargetUnitMap.begin(); iter != tempTargetUnitMap.end(); ++iter)
                {
                    if ((*iter)->GetTypeId() != TYPEID_UNIT)
                    {
                        continue;
                    }

                    for (SQLMultiStorage::SQLMultiSIterator<SpellTargetEntry> i_spellST = bounds.first; i_spellST != bounds.second; ++i_spellST)
                    {
                        if (i_spellST->CanNotHitWithSpellEffect(effIndex))
                        {
                            continue;
                        }

                        // only creature entries supported for this target type
                        if (i_spellST->type == SPELL_TARGET_TYPE_GAMEOBJECT)
                        {
                            continue;
                        }

                        if ((*iter)->GetEntry() == i_spellST->targetEntry)
                        {
                            if (i_spellST->type == SPELL_TARGET_TYPE_DEAD && ((Creature*)(*iter))->IsCorpse())
                            {
                                targetUnitMap.push_back((*iter));
                            }
                            else if (i_spellST->type == SPELL_TARGET_TYPE_CREATURE && (*iter)->IsAlive())
                            {
                                targetUnitMap.push_back((*iter));
                            }
                            break;
                        }
                    }
                }
            }
            break;
        }
        case TARGET_DUELVSPLAYER:
        {
            if (Unit* target = m_targets.getUnitTarget())
            {
                if (m_caster->IsFriendlyTo(target))
                {
                    targetUnitMap.push_back(target);
                }
                else
                {
                    if (Unit* pUnitTarget = m_caster->SelectMagnetTarget(target, this, effIndex))
                    {
                        if (target != pUnitTarget)
                        {
                            m_targets.setUnitTarget(pUnitTarget);
                        }
                        targetUnitMap.push_back(pUnitTarget);
                    }
                }
            }
            break;
        }
        case TARGET_GAMEOBJECT_ITEM:
            if (m_targets.getGOTargetGuid())
            {
                AddGOTarget(m_targets.getGOTarget(), effIndex);
            }
            else if (m_targets.getItemTarget())
            {
                AddItemTarget(m_targets.getItemTarget(), effIndex);
            }
            break;
        case TARGET_MASTER:
            if (Unit* owner = m_caster->GetCharmerOrOwner())
            {
                targetUnitMap.push_back(owner);
            }
            break;
        case TARGET_ALL_ENEMY_IN_AREA_CHANNELED:
            // targets the ground, not the units in the area
            if (m_spellInfo->Effect[effIndex] != SPELL_EFFECT_PERSISTENT_AREA_AURA)
            {
                FillAreaTargets(targetUnitMap, radius, PUSH_DEST_CENTER, SPELL_TARGETS_AOE_DAMAGE);
            }
            break;
        case TARGET_MINION:
            if (m_spellInfo->Effect[effIndex] != SPELL_EFFECT_DUEL)
            {
                targetUnitMap.push_back(m_caster);
            }
            break;
        case TARGET_AREAEFFECT_PARTY:
        {
            Unit* owner = m_caster->GetCharmerOrOwner();
            Player* pTarget = NULL;

            if (owner)
            {
                targetUnitMap.push_back(m_caster);
                if (owner->GetTypeId() == TYPEID_PLAYER)
                {
                    pTarget = (Player*)owner;
                }
            }
            else if (m_caster->GetTypeId() == TYPEID_PLAYER)
            {
                if (Unit* target = m_targets.getUnitTarget())
                {
                    if (target->GetTypeId() != TYPEID_PLAYER)
                    {
                        if (((Creature*)target)->IsPet())
                        {
                            Unit* targetOwner = target->GetOwner();
                            if (targetOwner->GetTypeId() == TYPEID_PLAYER)
                            {
                                pTarget = (Player*)targetOwner;
                            }
                        }
                    }
                    else
                    {
                        pTarget = (Player*)target;
                    }
                }
            }

            Group* pGroup = pTarget ? pTarget->GetGroup() : NULL;

            if (pGroup)
            {
                uint8 subgroup = pTarget->GetSubGroup();

                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* Target = itr->getSource();

                    // IsHostileTo check duel and controlled by enemy
                    if (Target && Target->GetSubGroup() == subgroup && !m_caster->IsHostileTo(Target))
                    {
                        if (pTarget->IsWithinDistInMap(Target, radius))
                        {
                            targetUnitMap.push_back(Target);
                        }

                        if (Pet* pet = Target->GetPet())
                        {
                            if (pTarget->IsWithinDistInMap(pet, radius))
                            {
                                targetUnitMap.push_back(pet);
                            }
                        }
                    }
                }
            }
            else if (owner)
            {
                if (m_caster->IsWithinDistInMap(owner, radius))
                {
                    targetUnitMap.push_back(owner);
                }
            }
            else if (pTarget)
            {
                targetUnitMap.push_back(pTarget);

                if (Pet* pet = pTarget->GetPet())
                {
                    if (m_caster->IsWithinDistInMap(pet, radius))
                    {
                        targetUnitMap.push_back(pet);
                    }
                }
            }
            break;
        }
        case TARGET_SCRIPT:
        {
            if (m_targets.getUnitTarget())
            {
                targetUnitMap.push_back(m_targets.getUnitTarget());
            }
            if (m_targets.getItemTarget())
            {
                AddItemTarget(m_targets.getItemTarget(), effIndex);
            }
            break;
        }
        case TARGET_SELF_FISHING:
            targetUnitMap.push_back(m_caster);
            break;
        case TARGET_CHAIN_HEAL:
        {
            Unit* pUnitTarget = m_targets.getUnitTarget();
            if (!pUnitTarget)
            {
                break;
            }

            if (EffectChainTarget <= 1)
            {
                targetUnitMap.push_back(pUnitTarget);
            }
            else
            {
                unMaxTargets = EffectChainTarget;
                float max_range = radius + unMaxTargets * CHAIN_SPELL_JUMP_RADIUS;

                UnitList tempTargetUnitMap;

                FillAreaTargets(tempTargetUnitMap, max_range, PUSH_SELF_CENTER, SPELL_TARGETS_FRIENDLY);

                if (m_caster != pUnitTarget && std::find(tempTargetUnitMap.begin(), tempTargetUnitMap.end(), m_caster) == tempTargetUnitMap.end())
                {
                    tempTargetUnitMap.push_front(m_caster);
                }

                tempTargetUnitMap.sort(TargetDistanceOrderNear(pUnitTarget));

                if (tempTargetUnitMap.empty())
                {
                    break;
                }

                if (*tempTargetUnitMap.begin() == pUnitTarget)
                {
                    tempTargetUnitMap.erase(tempTargetUnitMap.begin());
                }

                targetUnitMap.push_back(pUnitTarget);
                uint32 t = unMaxTargets - 1;
                Unit* prev = pUnitTarget;
                UnitList::iterator next = tempTargetUnitMap.begin();

                while (t && next != tempTargetUnitMap.end())
                {
                    if (!prev->IsWithinDist(*next, CHAIN_SPELL_JUMP_RADIUS))
                    {
                        break;
                    }

                    if (!DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, NULL, SPELL_DISABLE_LOS) && !prev->IsWithinLOSInMap(*next))
                    {
                        ++next;
                        continue;
                    }

                    if ((*next)->GetHealth() == (*next)->GetMaxHealth())
                    {
                        next = tempTargetUnitMap.erase(next);
                        continue;
                    }

                    prev = *next;
                    targetUnitMap.push_back(prev);
                    tempTargetUnitMap.erase(next);
                    tempTargetUnitMap.sort(TargetDistanceOrderNear(prev));
                    next = tempTargetUnitMap.begin();

                    --t;
                }
            }
            break;
        }
        case TARGET_CURRENT_ENEMY_COORDINATES:
        {
            Unit* currentTarget = m_targets.getUnitTarget();
            if (currentTarget)
            {
                targetUnitMap.push_back(currentTarget);
                m_targets.setDestination(currentTarget->GetPositionX(), currentTarget->GetPositionY(), currentTarget->GetPositionZ());
            }
            break;
        }
        case TARGET_AREAEFFECT_PARTY_AND_CLASS:
        {
            Player* targetPlayer = m_targets.getUnitTarget() && m_targets.getUnitTarget()->GetTypeId() == TYPEID_PLAYER
                ? (Player*)m_targets.getUnitTarget() : NULL;

            Group* pGroup = targetPlayer ? targetPlayer->GetGroup() : NULL;
            if (pGroup)
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* Target = itr->getSource();

                    // IsHostileTo check duel and controlled by enemy
                    if (Target && targetPlayer->IsWithinDistInMap(Target, radius) &&
                        targetPlayer->getClass() == Target->getClass() &&
                        !m_caster->IsHostileTo(Target))
                    {
                        targetUnitMap.push_back(Target);
                    }
                }
            }
            else if (m_targets.getUnitTarget())
            {
                targetUnitMap.push_back(m_targets.getUnitTarget());
            }
            break;
        }
        case TARGET_TABLE_X_Y_Z_COORDINATES:
        {
            if (SpellTargetPosition const* st = sSpellMgr.GetSpellTargetPosition(m_spellInfo->Id))
            {
                m_targets.setDestination(st->target_X, st->target_Y, st->target_Z);
                // TODO - maybe use an (internal) value for the map for neat far teleport handling

                // far-teleport spells are handled in SpellEffect, elsewise report an error about an unexpected map (spells are always locally)
                if (st->target_mapId != m_caster->GetMapId() && m_spellInfo->Effect[effIndex] != SPELL_EFFECT_TELEPORT_UNITS)
                {
                    sLog.outError("SPELL: wrong map (%u instead %u) target coordinates for spell ID %u", st->target_mapId, m_caster->GetMapId(), m_spellInfo->Id);
                }
            }
            else
            {
                sLog.outError("SPELL: unknown target coordinates for spell ID %u", m_spellInfo->Id);
            }
            break;
        }
        case TARGET_DYNAMIC_OBJECT_FRONT:
        case TARGET_DYNAMIC_OBJECT_BEHIND:
        case TARGET_DYNAMIC_OBJECT_LEFT_SIDE:
        case TARGET_DYNAMIC_OBJECT_RIGHT_SIDE:
        {
            if (!(m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION))
            {
                // General override, we don't want to use max spell range here.
                // Note: 0.0 radius is also for index 36. It is possible that 36 must be defined as
                // "at the base of", in difference to 0 which appear to be "directly in front of".
                // TODO: some summoned will make caster be half inside summoned object. Need to fix
                // that in the below code (nearpoint vs closepoint, etc).
                if (m_spellInfo->EffectRadiusIndex[effIndex] == 0)
                {
                    radius = 0.0f;
                }

                float angle = m_caster->GetOrientation();
                switch (targetMode)
                {
                    case TARGET_DYNAMIC_OBJECT_FRONT:                           break;
                    case TARGET_DYNAMIC_OBJECT_BEHIND:      angle += M_PI_F;      break;
                    case TARGET_DYNAMIC_OBJECT_LEFT_SIDE:   angle += M_PI_F / 2;  break;
                    case TARGET_DYNAMIC_OBJECT_RIGHT_SIDE:  angle -= M_PI_F / 2;  break;
                }

                float x, y;
                m_caster->GetNearPoint2D(x, y, radius + m_caster->GetObjectBoundingRadius(), angle);
                m_targets.setDestination(x, y, m_caster->GetPositionZ());
            }

            targetUnitMap.push_back(m_caster);
            break;
        }
        case TARGET_EFFECT_SELECT:
        {
            // add here custom effects that need default target.
            // FOR EVERY TARGET TYPE THERE IS A DIFFERENT FILL!!
            switch (m_spellInfo->Effect[effIndex])
            {
                case SPELL_EFFECT_DUMMY:
                {
                    switch (m_spellInfo->Id)
                    {
                        case 20577:                         // Cannibalize
                        {
                            WorldObject* result = FindCorpseUsing<MaNGOS::CannibalizeObjectCheck> ();

                            if (result)
                            {
                                switch (result->GetTypeId())
                                {
                                    case TYPEID_UNIT:
                                    case TYPEID_PLAYER:
                                        targetUnitMap.push_back((Unit*)result);
                                        break;
                                    case TYPEID_CORPSE:
                                        m_targets.setCorpseTarget((Corpse*)result);
                                        if (Player* owner = sObjectAccessor.FindPlayer(((Corpse*)result)->GetOwnerGuid()))
                                        {
                                            targetUnitMap.push_back(owner);
                                        }
                                        break;
                                }
                            }
                            else
                            {
                                // clear cooldown at fail
                                if (m_caster->GetTypeId() == TYPEID_PLAYER)
                                {
                                    ((Player*)m_caster)->RemoveSpellCooldown(m_spellInfo->Id, true);
                                }
                                SendCastResult(SPELL_FAILED_NO_EDIBLE_CORPSES);
                                finish(false);
                            }
                            break;
                        }
                        default:
                            if (m_targets.getUnitTarget())
                            {
                                targetUnitMap.push_back(m_targets.getUnitTarget());
                            }
                            break;
                    }
                    // Add AoE target-mask to self, if no target-dest provided already
                    if ((m_targets.m_targetMask & TARGET_FLAG_DEST_LOCATION) == 0)
                    {
                        m_targets.setDestination(m_caster->GetPositionX(), m_caster->GetPositionY(), m_caster->GetPositionZ());
                    }
                    break;
                }
                case SPELL_EFFECT_BIND:
                case SPELL_EFFECT_RESURRECT:
                case SPELL_EFFECT_PARRY:
                case SPELL_EFFECT_BLOCK:
                case SPELL_EFFECT_CREATE_ITEM:
                case SPELL_EFFECT_WEAPON:
                case SPELL_EFFECT_TRIGGER_SPELL:
                case SPELL_EFFECT_TRIGGER_MISSILE:
                case SPELL_EFFECT_LEARN_SPELL:
                case SPELL_EFFECT_SKILL_STEP:
                case SPELL_EFFECT_PROFICIENCY:
                case SPELL_EFFECT_SUMMON_POSSESSED:
                case SPELL_EFFECT_SUMMON_OBJECT_WILD:
                case SPELL_EFFECT_SELF_RESURRECT:
                case SPELL_EFFECT_REPUTATION:
                case SPELL_EFFECT_ADD_HONOR:
                case SPELL_EFFECT_SEND_TAXI:
                    if (m_targets.getUnitTarget())
                    {
                        targetUnitMap.push_back(m_targets.getUnitTarget());
                    }
                    // Triggered spells have additional spell targets - cast them even if no explicit unit target is given (required for spell 50516 for example)
                    else if (m_spellInfo->Effect[effIndex] == SPELL_EFFECT_TRIGGER_SPELL)
                    {
                        targetUnitMap.push_back(m_caster);
                    }
                    break;
                case SPELL_EFFECT_SUMMON_PLAYER:
                    if (m_caster->GetTypeId() == TYPEID_PLAYER && ((Player*)m_caster)->GetSelectionGuid())
                    {
                        if (Player* target = sObjectMgr.GetPlayer(((Player*)m_caster)->GetSelectionGuid()))
                        {
                            targetUnitMap.push_back(target);
                        }
                    }
                    break;
                case SPELL_EFFECT_RESURRECT_NEW:
                    if (m_targets.getUnitTarget())
                    {
                        targetUnitMap.push_back(m_targets.getUnitTarget());
                    }
                    if (m_targets.getCorpseTargetGuid())
                    {
                        if (Corpse* corpse = m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid()))
                        {
                            if (Player* owner = sObjectAccessor.FindPlayer(corpse->GetOwnerGuid()))
                            {
                                targetUnitMap.push_back(owner);
                            }
                        }
                    }
                    break;
                case SPELL_EFFECT_TELEPORT_UNITS:
                case SPELL_EFFECT_SUMMON:
                    /** [-ZERO]  if (m_spellInfo->EffectMiscValueB[effIndex] == SUMMON_TYPE_POSESSED ||
                     *              m_spellInfo->EffectMiscValueB[effIndex] == SUMMON_TYPE_POSESSED2)
                     *              {
                     *                  if (m_targets.getUnitTarget())
                     *                  {
                     *                      targetUnitMap.push_back(m_targets.getUnitTarget());
                     *                  }
                     *              }
                     *              else
                     */
                    {
                        targetUnitMap.push_back(m_caster);
                    }
                    break;
                case SPELL_EFFECT_SUMMON_CHANGE_ITEM:
                case SPELL_EFFECT_SUMMON_WILD:
                case SPELL_EFFECT_SUMMON_GUARDIAN:
                case SPELL_EFFECT_TRANS_DOOR:
                case SPELL_EFFECT_ADD_FARSIGHT:
                case SPELL_EFFECT_STUCK:
                case SPELL_EFFECT_DESTROY_ALL_TOTEMS:
                case SPELL_EFFECT_SUMMON_DEMON:
                case SPELL_EFFECT_SKILL:
                    targetUnitMap.push_back(m_caster);
                    break;
                case SPELL_EFFECT_PERSISTENT_AREA_AURA:
                    if (Unit* currentTarget = m_targets.getUnitTarget())
                    {
                        m_targets.setDestination(currentTarget->GetPositionX(), currentTarget->GetPositionY(), currentTarget->GetPositionZ());
                    }
                    break;
                case SPELL_EFFECT_LEARN_PET_SPELL:
                    if (Pet* pet = m_caster->GetPet())
                    {
                        targetUnitMap.push_back(pet);
                    }
                    break;
                case SPELL_EFFECT_ENCHANT_ITEM:
                case SPELL_EFFECT_ENCHANT_ITEM_TEMPORARY:
                case SPELL_EFFECT_DISENCHANT:
                case SPELL_EFFECT_FEED_PET:
                    if (m_targets.getItemTarget())
                    {
                        AddItemTarget(m_targets.getItemTarget(), effIndex);
                    }
                    break;
                case SPELL_EFFECT_APPLY_AURA:
                    switch (m_spellInfo->EffectApplyAuraName[effIndex])
                    {
                        case SPELL_AURA_ADD_FLAT_MODIFIER:  // some spell mods auras have 0 target modes instead expected TARGET_SELF(1) (and present for other ranks for same spell for example)
                        case SPELL_AURA_ADD_PCT_MODIFIER:
                            targetUnitMap.push_back(m_caster);
                            break;
                        default:                            // apply to target in other case
                            if (m_targets.getUnitTarget())
                            {
                                targetUnitMap.push_back(m_targets.getUnitTarget());
                            }
                            break;
                    }
                    break;
                case SPELL_EFFECT_APPLY_AREA_AURA_PARTY:
                    // AreaAura
                    if ((m_spellInfo->Attributes == (SPELL_ATTR_NOT_SHAPESHIFT | SPELL_ATTR_DONT_AFFECT_SHEATH_STATE | SPELL_ATTR_CASTABLE_WHILE_MOUNTED | SPELL_ATTR_CASTABLE_WHILE_SITTING)) || (m_spellInfo->Attributes == SPELL_ATTR_NOT_SHAPESHIFT))
                    {
                        SetTargetMap(effIndex, TARGET_AREAEFFECT_PARTY, targetUnitMap);
                    }
                    break;
                case SPELL_EFFECT_SKIN_PLAYER_CORPSE:
                    if (m_targets.getUnitTarget())
                    {
                        targetUnitMap.push_back(m_targets.getUnitTarget());
                    }
                    else if (m_targets.getCorpseTargetGuid())
                    {
                        if (Corpse* corpse = m_caster->GetMap()->GetCorpse(m_targets.getCorpseTargetGuid()))
                        {
                            if (Player* owner = sObjectAccessor.FindPlayer(corpse->GetOwnerGuid()))
                            {
                                targetUnitMap.push_back(owner);
                            }
                        }
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        default:
            // sLog.outError( "SPELL: Unknown implicit target (%u) for spell ID %u", targetMode, m_spellInfo->Id );
            break;
    }

    if (targetMode != TARGET_SELF && m_spellInfo->HasAttribute(SPELL_ATTR_EX_CANT_TARGET_SELF))
    {
        targetUnitMap.remove(m_caster);
    }

    if (unMaxTargets && targetUnitMap.size() > unMaxTargets)
    {
        // make sure one unit is always removed per iteration
        uint32 removed_utarget = 0;
        for (UnitList::iterator itr = targetUnitMap.begin(), next; itr != targetUnitMap.end(); itr = next)
        {
            next = itr;
            ++next;
            if (!*itr)
            {
                continue;
            }
            if ((*itr) == m_targets.getUnitTarget())
            {
                targetUnitMap.erase(itr);
                removed_utarget = 1;
                //        break;
            }
        }
        // remove random units from the map
        while (targetUnitMap.size() > unMaxTargets - removed_utarget)
        {
            uint32 poz = urand(0, targetUnitMap.size() - 1);
            for (UnitList::iterator itr = targetUnitMap.begin(); itr != targetUnitMap.end(); ++itr, --poz)
            {
                if (!*itr)
                {
                    continue;
                }

                if (!poz)
                {
                    targetUnitMap.erase(itr);
                    break;
                }
            }
        }
        // the player's target will always be added to the map
        if (removed_utarget && m_targets.getUnitTarget())
        {
            targetUnitMap.push_back(m_targets.getUnitTarget());
        }
    }
    if (!tempTargetGOList.empty())                          // GO CASE
    {
        if (unMaxTargets && tempTargetGOList.size() > unMaxTargets)
        {
            // make sure one go is always removed per iteration
            uint32 removed_utarget = 0;
            for (std::list<GameObject*>::iterator itr = tempTargetGOList.begin(), next; itr != tempTargetGOList.end(); itr = next)
            {
                next = itr;
                ++next;
                if (!*itr)
                {
                    continue;
                }
                if ((*itr) == m_targets.getGOTarget())
                {
                    tempTargetGOList.erase(itr);
                    removed_utarget = 1;
                    //        break;
                }
            }
            // remove random units from the map
            while (tempTargetGOList.size() > unMaxTargets - removed_utarget)
            {
                uint32 poz = urand(0, tempTargetGOList.size() - 1);
                for (std::list<GameObject*>::iterator itr = tempTargetGOList.begin(); itr != tempTargetGOList.end(); ++itr, --poz)
                {
                    if (!*itr)
                    {
                        continue;
                    }

                    if (!poz)
                    {
                        tempTargetGOList.erase(itr);
                        break;
                    }
                }
            }
        }
        // Add resulting GOs as GOTargets
        for (std::list<GameObject*>::iterator iter = tempTargetGOList.begin(); iter != tempTargetGOList.end(); ++iter)
        {
            AddGOTarget(*iter, effIndex);
        }
    }
}

/**
 * @brief Prepares the spell cast, validates conditions, and starts cast processing.
 *
 * @param targets The resolved spell cast targets.
 * @param triggeredByAura The triggering aura, if this spell was aura-triggered.
 * @param chance Optional roll chance required before proceeding.
 * @return The resulting cast status.
 */
SpellCastResult Spell::prepare(SpellCastTargets const* targets, Aura* triggeredByAura, uint32 chance)
{
    m_targets = *targets;

    m_castPositionX = m_caster->GetPositionX();
    m_castPositionY = m_caster->GetPositionY();
    m_castPositionZ = m_caster->GetPositionZ();
    m_castOrientation = m_caster->GetOrientation();

    if (triggeredByAura)
    {
        m_triggeredByAuraSpell = triggeredByAura->GetSpellProto();
    }

    // create and add update event for this spell
    SpellEvent* Event = new SpellEvent(this);
    m_caster->m_Events.AddEvent(Event, m_caster->m_Events.CalculateTime(1));

    // Prevent casting at cast another spell (ServerSide check)
    if (!m_IsTriggeredSpell && m_caster->IsNonMeleeSpellCasted(false, true, true))
    {
        SendCastResult(SPELL_FAILED_SPELL_IN_PROGRESS);
        finish(false);
        return SPELL_FAILED_SPELL_IN_PROGRESS;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, m_spellInfo->Id, m_caster))
    {
        SendCastResult(SPELL_FAILED_SPELL_UNAVAILABLE);
        finish(false);
        return SPELL_FAILED_SPELL_UNAVAILABLE;
    }

    // Fill cost data
    m_powerCost = CalculatePowerCost(m_spellInfo, m_caster, this, m_CastItem);

    SpellCastResult result = CheckCast(true);
    if (result != SPELL_CAST_OK && !IsAutoRepeat())         // always cast autorepeat dummy for triggering
    {
        if (triggeredByAura)
        {
            SendChannelUpdate(0);
            triggeredByAura->GetHolder()->SetAuraDuration(0);
        }
        SendCastResult(result);
        finish(false);
        return result;
    }

    // Roll chance to cast from spell list (must be after cast checks, this is why its here)
    if (chance)
    {
        if (!roll_chance_i(chance))
        {
            finish(false);
            return SPELL_FAILED_TRY_AGAIN;
        }
    }

    m_spellState = SPELL_STATE_PREPARING;

    // Prepare data for triggers
    prepareDataForTriggerSystem();

    // calculate cast time (calculated after first CheckCast check to prevent charge counting for first CheckCast fail)
    m_casttime = GetSpellCastTime(m_spellInfo, this);
    m_duration = CalculateSpellDuration(m_spellInfo, m_caster);

    // set timer base at cast time
    ReSetTimer();

    // stealth must be removed at cast starting (at show channel bar)
    // skip triggered spell (item equip spell casting and other not explicit character casts/item uses)
    if (!m_IsTriggeredSpell && isSpellBreakStealth(m_spellInfo))
    {
        // Sap - don't exit Stealth yet to prevent getting in combat and making Sap impossible to cast
        // Removing Stealth depends on talent later
        // Pick Pocket - don't exit Stealth at all
        if (!(m_spellInfo->SpellFamilyName == SPELLFAMILY_ROGUE && (m_spellInfo->SpellFamilyFlags & UI64LIT(0x00000080) || m_spellInfo->SpellFamilyFlags & 2147483648)))
        {
            m_caster->RemoveSpellsCausingAura(SPELL_AURA_MOD_STEALTH);
        }
        m_caster->RemoveSpellsCausingAura(SPELL_AURA_FEIGN_DEATH);
    }

    // add non-triggered (with cast time and without)
    if (!m_IsTriggeredSpell)
    {
        // add to cast type slot
        m_caster->SetCurrentCastedSpell(this);

        // will show cast bar
        SendSpellStart();

        TriggerGlobalCooldown();
    }
    else
    {
        if (m_timer == 0)
        {
            cast(true);
        }
    }

    return SPELL_CAST_OK;
}































/**
 * @brief Gets the first queued unit target guid for an effect, falling back to the explicit target guid.
 *
 * @param effIndex The effect index to inspect.
 * @return The matching unit target guid, or the explicit unit target guid when none is queued.
 */
ObjectGuid Spell::GetPrefilledOrUnitTargetGuid(SpellEffectIndex effIndex) const
{
    for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin(); itr != m_UniqueTargetInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effIndex))
        {
            return itr->targetGUID;
        }
    }

    return m_targets.getUnitTargetGuid();
}










/**
 * @brief Applies spell pushback delay to a currently casting player spell.
 */
void Spell::Delayed()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (m_spellState == SPELL_STATE_DELAYED)
    {
        return;                                              // spell is active and can't be time-backed
    }

    // spells not losing casting time ( slam, dynamites, bombs.. )
    if (!(m_spellInfo->InterruptFlags & SPELL_INTERRUPT_FLAG_DAMAGE))
    {
        return;
    }

    // check resist chance
    int32 resistChance = 100;                               // must be initialized to 100 for percent modifiers
    ((Player*)m_caster)->ApplySpellMod(m_spellInfo->Id, SPELLMOD_NOT_LOSE_CASTING_TIME, resistChance, this);
    resistChance += m_caster->GetTotalAuraModifier(SPELL_AURA_RESIST_PUSHBACK) - 100;
    if (roll_chance_i(resistChance))
    {
        return;
    }

    int32 delaytime = GetNextDelayAtDamageMsTime();

    if (int32(m_timer) + delaytime > m_casttime)
    {
        delaytime = m_casttime - m_timer;
        m_timer = m_casttime;
    }
    else
    {
        m_timer += delaytime;
    }

    DETAIL_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted for (%d) ms at damage", m_spellInfo->Id, delaytime);

    WorldPacket data(SMSG_SPELL_DELAYED, 8 + 4);
    data << ObjectGuid(m_caster->GetObjectGuid());
    data << uint32(delaytime);

    if (m_caster->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)m_caster)->SendDirectMessage(&data);
    }
}

/**
 * @brief Applies pushback to an active channeled spell and linked aura durations.
 */
void Spell::DelayedChannel()
{
    if (!m_caster || m_caster->GetTypeId() != TYPEID_PLAYER || getState() != SPELL_STATE_CASTING)
    {
        return;
    }

    // check resist chance
    int32 resistChance = 100;                               // must be initialized to 100 for percent modifiers
    ((Player*)m_caster)->ApplySpellMod(m_spellInfo->Id, SPELLMOD_NOT_LOSE_CASTING_TIME, resistChance, this);
    resistChance += m_caster->GetTotalAuraModifier(SPELL_AURA_RESIST_PUSHBACK) - 100;
    if (roll_chance_i(resistChance))
    {
        return;
    }

    int32 delaytime = GetNextDelayAtDamageMsTime();

    if (int32(m_timer) < delaytime)
    {
        delaytime = m_timer;
        m_timer = 0;
    }
    else
    {
        m_timer -= delaytime;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "Spell %u partially interrupted for %i ms, new duration: %u ms", m_spellInfo->Id, delaytime, m_timer);

    for (TargetList::const_iterator ihit = m_UniqueTargetInfo.begin(); ihit != m_UniqueTargetInfo.end(); ++ihit)
    {
        if ((*ihit).missCondition == SPELL_MISS_NONE)
        {
            if (Unit* unit = m_caster->GetObjectGuid() == ihit->targetGUID ? m_caster : sObjectAccessor.GetUnit(*m_caster, ihit->targetGUID))
            {
                unit->DelaySpellAuraHolder(m_spellInfo->Id, delaytime, unit->GetObjectGuid());
            }
        }
    }

    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
    {
        // partially interrupt persistent area auras
        if (DynamicObject* dynObj = m_caster->GetDynObject(m_spellInfo->Id, SpellEffectIndex(j)))
        {
            dynObj->Delay(delaytime);
        }
    }

    SendChannelUpdate(m_timer);
}

/**
 * @brief Refreshes the cached original caster pointer from the stored guid.
 */
void Spell::UpdateOriginalCasterPointer()
{
    if (m_originalCasterGUID == m_caster->GetObjectGuid())
    {
        m_originalCaster = m_caster;
    }
    else if (m_originalCasterGUID.IsGameObject())
    {
        GameObject* go = m_caster->IsInWorld() ? m_caster->GetMap()->GetGameObject(m_originalCasterGUID) : NULL;
        m_originalCaster = go ? go->GetOwner() : NULL;
    }
    else
    {
        Unit* unit = sObjectAccessor.GetUnit(*m_caster, m_originalCasterGUID);
        m_originalCaster = unit && unit->IsInWorld() ? unit : NULL;
    }
}

/**
 * @brief Refreshes cached caster and target pointers from stored guids.
 */
void Spell::UpdatePointers()
{
    UpdateOriginalCasterPointer();

    m_targets.Update(m_caster);
}




/**
 * @brief Checks whether this spell cast should produce client-visible packets.
 *
 * @return True if packets should be sent to clients; otherwise, false.
 */
bool Spell::IsNeedSendToClient() const
{
    return m_spellInfo->SpellVisual != 0 ||
        IsChanneledSpell(m_spellInfo) ||
        m_spellInfo->speed > 0.0f ||
        (!m_triggeredByAuraSpell && !m_IsTriggeredSpell);
}

/**
 * @brief Checks whether the triggered spell still requires redundant cast-time handling.
 *
 * @return True if redundant cast-time handling is needed; otherwise, false.
 */
bool Spell::IsTriggeredSpellWithRedundentCastTime() const
{
    return m_triggeredByAuraSpell || (m_IsTriggeredSpell && (m_spellInfo->manaCost || m_spellInfo->ManaCostPercentage));
}

/**
 * @brief Checks whether any queued target entry contains a given effect.
 *
 * @param effect The effect index to look for.
 * @return True if at least one target has the effect queued; otherwise, false.
 */
bool Spell::HaveTargetsForEffect(SpellEffectIndex effect) const
{
    for (TargetList::const_iterator itr = m_UniqueTargetInfo.begin(); itr != m_UniqueTargetInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effect))
        {
            return true;
        }
    }

    for (GOTargetList::const_iterator itr = m_UniqueGOTargetInfo.begin(); itr != m_UniqueGOTargetInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effect))
        {
            return true;
        }
    }

    for (ItemTargetList::const_iterator itr = m_UniqueItemInfo.begin(); itr != m_UniqueItemInfo.end(); ++itr)
    {
        if (itr->effectMask & (1 << effect))
        {
            return true;
        }
    }

    return false;
}

SpellEvent::SpellEvent(Spell* spell) : BasicEvent()
{
    m_Spell = spell;
}

SpellEvent::~SpellEvent()
{
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->cancel();
    }

    if (m_Spell->IsDeletable())
    {
        delete m_Spell;
    }
    else
    {
        sLog.outError("~SpellEvent: %s %u tried to delete non-deletable spell %u. Was not deleted, causes memory leak.",
            (m_Spell->GetCaster()->GetTypeId() == TYPEID_PLAYER ? "Player" : "Creature"), m_Spell->GetCaster()->GetGUIDLow(), m_Spell->m_spellInfo->Id);
    }
}

/**
 * @brief Advances spell execution within the event queue.
 *
 * @param e_time The event execution time.
 * @param p_time The elapsed update time in milliseconds.
 * @return True when the event is complete and can be removed; otherwise, false.
 */
bool SpellEvent::Execute(uint64 e_time, uint32 p_time)
{
    // update spell if it is not finished
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->update(p_time);
    }

    // check spell state to process
    switch (m_Spell->getState())
    {
        case SPELL_STATE_FINISHED:
        {
            // spell was finished, check deletable state
            if (m_Spell->IsDeletable())
            {
                // check, if we do have unfinished triggered spells
                return true;                                // spell is deletable, finish event
            }
            // event will be re-added automatically at the end of routine)
            break;
        }
        case SPELL_STATE_CASTING:
        {
            // this spell is in channeled state, process it on the next update
            // event will be re-added automatically at the end of routine)
            break;
        }
        case SPELL_STATE_DELAYED:
        {
            // first, check, if we have just started
            if (m_Spell->GetDelayStart() != 0)
            {
                // no, we aren't, do the typical update
                // check, if we have channeled spell on our hands
                if (IsChanneledSpell(m_Spell->m_spellInfo))
                {
                    // evented channeled spell is processed separately, casted once after delay, and not destroyed till finish
                    // check, if we have casting anything else except this channeled spell and autorepeat
                    if (m_Spell->GetCaster()->IsNonMeleeSpellCasted(false, true, true))
                    {
                        // another non-melee non-delayed spell is casted now, abort
                        m_Spell->cancel();
                    }
                    else
                    {
                        // do the action (pass spell to channeling state)
                        m_Spell->handle_immediate();
                    }
                    // event will be re-added automatically at the end of routine)
                }
                else
                {
                    // run the spell handler and think about what we can do next
                    uint64 t_offset = e_time - m_Spell->GetDelayStart();
                    uint64 n_offset = m_Spell->handle_delayed(t_offset);
                    if (n_offset)
                    {
                        // re-add us to the queue
                        m_Spell->GetCaster()->m_Events.AddEvent(this, m_Spell->GetDelayStart() + n_offset, false);
                        return false;                       // event not complete
                    }
                    // event complete
                    // finish update event will be re-added automatically at the end of routine)
                }
            }
            else
            {
                // delaying had just started, record the moment
                m_Spell->SetDelayStart(e_time);
                // re-plan the event for the delay moment
                m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + m_Spell->GetDelayMoment(), false);
                return false;                               // event not complete
            }
            break;
        }
        default:
        {
            // all other states
            // event will be re-added automatically at the end of routine)
            break;
        }
    }

    // spell processing not complete, plan event on the next update interval
    m_Spell->GetCaster()->m_Events.AddEvent(this, e_time + 1, false);
    return false;                                           // event not complete
}

/**
 * @brief Aborts the queued spell event and cancels the spell if needed.
 *
 * @param e_time Unused event time.
 */
void SpellEvent::Abort(uint64 /*e_time*/)
{
    // oops, the spell we try to do is aborted
    if (m_Spell->getState() != SPELL_STATE_FINISHED)
    {
        m_Spell->cancel();
    }
}

/**
 * @brief Checks whether the underlying spell can be deleted.
 *
 * @return True if the spell is deletable; otherwise, false.
 */
bool SpellEvent::IsDeletable() const
{
    return m_Spell->IsDeletable();
}

/**
 * @brief Checks whether a lockable game object is within spell range.
 *
 * @param go The game object being checked.
 * @return True if the object is within range; otherwise, false.
 */
bool Spell::IsLockInRange(GameObject* go)
{
    const SpellRangeEntry* srange = sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex);

    // This check is not related to bounding radius
    float dx = m_caster->GetPositionX() - go->GetPositionX();
    float dy = m_caster->GetPositionY() - go->GetPositionY();
    float dz = m_caster->GetPositionZ() - go->GetPositionZ();

    return (dx * dx + dy * dy + dz * dz < srange->maxRange);
}

/**
 * @brief Validates whether the caster can open a lock with this spell effect.
 *
 * @param effIndex The effect index performing the open-lock action.
 * @param lockId The lock identifier.
 * @param skillId Receives the required skill type.
 * @param reqSkillValue Receives the required skill value.
 * @param skillValue Receives the caster's effective skill value.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CanOpenLock(SpellEffectIndex effIndex, uint32 lockId, SkillType& skillId, int32& reqSkillValue, int32& skillValue)
{
    if (!lockId)                                            // possible case for GO and maybe for items.
    {
        return SPELL_CAST_OK;
    }

    // Get LockInfo
    LockEntry const* lockInfo = sLockStore.LookupEntry(lockId);

    if (!lockInfo)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    bool reqKey = false;                                    // some locks not have reqs

    for (int j = 0; j < 8; ++j)
    {
        switch (lockInfo->Type[j])
        {
            // check key item (many fit cases can be)
            case LOCK_KEY_ITEM:
            {
                if (lockInfo->Index[j] && m_CastItem && m_CastItem->GetEntry() == lockInfo->Index[j])
                {
                    return SPELL_CAST_OK;
                }
                reqKey = true;
                break;
            }
            // check key skill - only single first fit case can be
            case LOCK_KEY_SKILL:
            {
                reqKey = true;

                // wrong locktype, skip
                if (uint32(m_spellInfo->EffectMiscValue[effIndex]) != lockInfo->Index[j])
                {
                    continue;
                }

                skillId = SkillByLockType(LockType(lockInfo->Index[j]));

                if (skillId != SKILL_NONE)
                {
                    // skill bonus provided by casting spell (mostly item spells)
                    // add the damage modifier from the spell casted (cheat lock / skeleton key etc.) (use m_currentBasePoints, CalculateDamage returns wrong value)
                    uint32 spellSkillBonus = uint32(m_currentBasePoints[effIndex]);
                    reqSkillValue = lockInfo->Skill[j];

                    // castitem check: rogue using skeleton keys. the skill values should not be added in this case.
                    skillValue = m_CastItem || m_caster->GetTypeId() != TYPEID_PLAYER ? 0
                        : ((Player*)m_caster)->GetSkillValue(skillId);

                    skillValue += spellSkillBonus;

                    if (skillValue < reqSkillValue)
                    {
                        return SPELL_FAILED_SKILL_NOT_HIGH_ENOUGH;
                    }
                }

                return SPELL_CAST_OK;
            }
        }
    }

    if (reqKey)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    return SPELL_CAST_OK;
}



/**
 * @brief Gets the world object that should be used as the effective spell origin.
 *
 * @return The effective caster world object.
 */
WorldObject* Spell::GetAffectiveCasterObject() const
{
    if (!m_originalCasterGUID)
    {
        return m_caster;
    }

    if (m_originalCasterGUID.IsGameObject() && m_caster->IsInWorld())
    {
        return m_caster->GetMap()->GetGameObject(m_originalCasterGUID);
    }
    return m_originalCaster;
}

/**
 * @brief Gets the world object used for cast-position and line-of-sight calculations.
 *
 * @return The casting world object.
 */
WorldObject* Spell::GetCastingObject() const
{
    if (m_originalCasterGUID.IsGameObject())
    {
        return m_caster->IsInWorld() ? m_caster->GetMap()->GetGameObject(m_originalCasterGUID) : NULL;
    }
    else
    {
        return m_caster;
    }
}

/**
 * @brief Clears the accumulated effect damage and healing counters.
 */
void Spell::ResetEffectDamageAndHeal()
{
    m_damage = 0;
    m_healing = 0;
}

/**
 * @brief Clears the cached cast item and unlinks it from target data when necessary.
 */
void Spell::ClearCastItem()
{
    if (m_CastItem == m_targets.getItemTarget())
    {
        m_targets.setItemTarget(NULL);
    }

    m_CastItem = NULL;
}




/**
 * @brief Resolves effective radius, chain target count, and target cap modifiers for an effect.
 *
 * @param effIndex The effect index being evaluated.
 * @param radius Receives the effective radius.
 * @param EffectChainTarget Receives the effective chain target count.
 * @param unMaxTargets Receives the effective maximum affected target count.
 */
void Spell::GetSpellRangeAndRadius(SpellEffectIndex effIndex, float& radius, uint32& EffectChainTarget, uint32& unMaxTargets) const
{
    if (m_spellInfo->EffectRadiusIndex[effIndex])
    {
        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[effIndex]));
    }
    else
    {
        radius = GetSpellMaxRange(sSpellRangeStore.LookupEntry(m_spellInfo->rangeIndex));
    }

    if (Unit* realCaster = GetAffectiveCaster())
    {
        if (Player* modOwner = realCaster->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_RADIUS, radius, this);
            modOwner->ApplySpellMod(m_spellInfo->Id, SPELLMOD_JUMP_TARGETS, EffectChainTarget, this);
        }
    }

    // custom target amount cases
    switch (m_spellInfo->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->Id)
            {
                case 802:                                   // Mutate Bug (AQ40, Emperor Vek'nilash)
                case 804:                                   // Explode Bug (AQ40, Emperor Vek'lor)
                case 23138:                                 // Gate of Shazzrah (MC, Shazzrah)
                case 24781:                                 // Dream Fog (Emerald Dragons)
                case 28560:                                 // Summon Blizzard (Naxx, Sapphiron)
                    unMaxTargets = 1;
                    break;
                case 10258:                                 // Awaken Vault Warder (Uldaman)
                case 28542:                                 // Life Drain (Naxx, Sapphiron)
                    unMaxTargets = 2;
                    break;
                case 28796:                                 // Poison Bolt Volley (Naxx, Faerlina)
                    unMaxTargets = 10;
                    break;
                case 25991:                                 // Poison Bolt Volley (AQ40, Pincess Huhuran)
                    unMaxTargets = 15;
                    break;
            }
            break;
        }
        default:
            break;
    }

    // custom radius cases
    switch (m_spellInfo->SpellFamilyName)
    {
        case SPELLFAMILY_GENERIC:
        {
            switch (m_spellInfo->Id)
            {
                case 24811:                                 // Draw Spirit (Lethon)
                {
                    if (effIndex == EFFECT_INDEX_0)         // Copy range from EFF_1 to 0
                    {
                        radius = GetSpellRadius(sSpellRadiusStore.LookupEntry(m_spellInfo->EffectRadiusIndex[EFFECT_INDEX_1]));
                    }
                    break;
                }
                case 28241:                                 // Poison (Naxxramas, Grobbulus Cloud)
                {
                    if (SpellAuraHolder* auraHolder = m_caster->GetSpellAuraHolder(28158))
                    {
                        radius = 0.5f * (60000 - auraHolder->GetAuraDuration()) * 0.001f;
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        default:
            break;
    }
}

/**
 * @brief Validates whether the current target can be tamed by the caster.
 *
 * @param isGM True to allow game master override behavior.
 * @return The resulting cast status.
 */
SpellCastResult Spell::CanTameUnit(bool isGM)
{
    // Spell can be triggered, we need to check original caster prior to caster
    Unit* caster = GetAffectiveCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER ||
        !m_targets.getUnitTarget() || m_targets.getUnitTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        return SPELL_FAILED_BAD_TARGETS;
    }

    Player* plrCaster = caster->ToPlayer();

    if (plrCaster->getClass() != CLASS_HUNTER)
    {
        plrCaster->SendPetTameFailure(PETTAME_UNITSCANTTAME);
        return SPELL_FAILED_DONT_REPORT;
    }

    if (isGM && !ChatHandler(plrCaster).FindCommand("npc tame"))
    {
        plrCaster->SendPetTameFailure(PETTAME_UNKNOWNERROR);
        return SPELL_FAILED_DONT_REPORT;
    }

    Creature* target = (Creature*)m_targets.getUnitTarget();

    if (target->IsPet() || target->IsCharmed())
    {
        plrCaster->SendPetTameFailure(PETTAME_CREATUREALREADYOWNED);
        return SPELL_FAILED_DONT_REPORT;
    }

    if (target->getLevel() > plrCaster->getLevel() && !isGM)
    {
        plrCaster->SendPetTameFailure(PETTAME_TOOHIGHLEVEL);
        return SPELL_FAILED_DONT_REPORT;
    }

    if (!target->GetCreatureInfo()->isTameable())
    {
        plrCaster->SendPetTameFailure(PETTAME_NOTTAMEABLE);
        return SPELL_FAILED_DONT_REPORT;
    }

    Pet* pet = plrCaster->GetPet();
    if (pet || plrCaster->GetCharmGuid())
    {
        plrCaster->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
        return SPELL_FAILED_DONT_REPORT;
    }
    else
    {
        PetDatabaseStatus status = Pet::GetStatusFromDB(plrCaster);
        if (status != PET_DB_NO_PET)
        {
            plrCaster->SendPetTameFailure(PETTAME_ANOTHERSUMMONACTIVE);
            return SPELL_FAILED_DONT_REPORT;
        }
    }
    return SPELL_CAST_OK;
}
