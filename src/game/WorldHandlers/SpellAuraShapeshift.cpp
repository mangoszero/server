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
 * @file SpellAuras.cpp
 * @brief Spell aura implementation
 *
 * This file implements the SpellAura class which handles spell auras:
 * - Aura application and removal
 * - Aura effect processing (stat modifiers, DoTs, HoTs, etc.)
 * - Aura stacking rules
 * - Aura dispelling mechanics
 * - Aura periodic effects
 * - Aura duration management
 * - Aura visual effects
 *
 * Auras are persistent effects applied by spells that modify
 * unit stats, deal damage over time, or provide other benefits.
 *
 * @see SpellAura for the aura class
 * @see Spell for spell casting
 */



#include "SpellAuras.h"
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "WorldPacket.h"
#include "WorldSession.h"
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
#include "Group.h"
#include "UpdateData.h"
#include "ObjectAccessor.h"
#include "Policies/Singleton.h"
#include "Totem.h"
#include "Creature.h"
#include "Formulas.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "CreatureAI.h"
#include "ScriptMgr.h"
#include "Util.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "Language.h"
#include "TemporarySummon.h"
#include "MapManager.h"

enum SpellCreatedItems {
    ITEM_SOUL_SHARD = 6265
};

/**
 * @brief Applies or removes a mounted display from the target.
 *
 * @param apply True to mount; false to unmount.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraMounted(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    Unit* target = GetTarget();

    if (apply)
    {
        CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
        if (!ci)
        {
            sLog.outErrorDb("AuraMounted: `creature_template`='%u' not found in database (only need it modelid)", m_modifier.m_miscvalue);
            return;
        }

        uint32 display_id = Creature::ChooseDisplayId(ci);
        CreatureModelInfo const* minfo = sObjectMgr.GetCreatureModelRandomGender(display_id);
        if (minfo)
        {
            display_id = minfo->modelid;
        }

        target->Mount(display_id, GetId());
    }
    else
    {
        target->Unmount(true);
    }
}

/**
 * @brief Applies or removes water walking on the target.
 *
 * @param apply True to enable water walking; false to disable it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraWaterWalk(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    GetTarget()->SetWaterWalk(apply);
}

/**
 * @brief Applies or removes feather fall on the target.
 *
 * @param apply True to enable feather fall; false to disable it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraFeatherFall(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    GetTarget()->SetFeatherFall(apply);
}

/**
 * @brief Applies or removes hovering movement state.
 *
 * @param apply True to enable hover; false to disable it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraHover(bool apply, bool Real)
{
    // only at real add/remove aura
    if (!Real)
    {
        return;
    }

    GetTarget()->SetHover(apply);
}

/**
 * @brief Refreshes client breathing timers for the target.
 *
 * @param apply Unused.
 * @param Real Unused.
 */
void Aura::HandleWaterBreathing(bool /*apply*/, bool /*Real*/)
{
    // update timers in client
    if (GetTarget()->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)GetTarget())->UpdateMirrorTimers();
    }
}

/**
 * @brief Applies or removes a shapeshift form and its related state changes.
 *
 * @param apply True to enter the form; false to leave it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraModShapeshift(bool apply, bool Real)
{
    if (!Real)
    {
        return;
    }

    ShapeshiftForm form = ShapeshiftForm(m_modifier.m_miscvalue);

    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(form);
    if (!ssEntry)
    {
        sLog.outError("Unknown shapeshift form %u in spell %u", form, GetId());
        return;
    }

    uint32 modelid = 0;
    Powers PowerType = POWER_MANA;
    Unit* target = GetTarget();

    // remove SPELL_AURA_EMPATHY
    target->RemoveSpellsCausingAura(SPELL_AURA_EMPATHY);

    switch (form)
    {
        case FORM_CAT:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 892;
            }
            else
            {
                modelid = 8571;
            }
            PowerType = POWER_ENERGY;
            break;
        case FORM_TRAVEL:
            modelid = 632;
            break;
        case FORM_AQUA:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 2428;
            }
            else
            {
                modelid = 2428;
            }
            break;
        case FORM_BEAR:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 2281;
            }
            else
            {
                modelid = 2289;
            }
            PowerType = POWER_RAGE;
            break;
        case FORM_GHOUL:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 10045;
            }
            break;
        case FORM_DIREBEAR:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 2281;
            }
            else
            {
                modelid = 2289;
            }
            PowerType = POWER_RAGE;
            break;
        case FORM_CREATUREBEAR:
            modelid = 902;
            break;
        case FORM_GHOSTWOLF:
            modelid = 4613;
            break;
        case FORM_MOONKIN:
            if (Player::TeamForRace(target->getRace()) == ALLIANCE)
            {
                modelid = 15374;
            }
            else
            {
                modelid = 15375;
            }
            break;
        case FORM_AMBIENT:
        case FORM_SHADOW:
        case FORM_STEALTH:
            break;
        case FORM_TREE:
            modelid = 864;
            break;
        case FORM_BATTLESTANCE:
        case FORM_BERSERKERSTANCE:
        case FORM_DEFENSIVESTANCE:
            PowerType = POWER_RAGE;
            break;
        case FORM_SPIRITOFREDEMPTION:
            modelid = 16031;
            break;
        default:
            break;
    }

    // remove polymorph before changing display id to keep new display id
    switch (form)
    {
        case FORM_CAT:
        case FORM_TREE:
        case FORM_TRAVEL:
        case FORM_AQUA:
        case FORM_BEAR:
        case FORM_DIREBEAR:
        case FORM_MOONKIN:
        {
            // remove movement affects
            target->RemoveSpellsCausingAura(SPELL_AURA_MOD_ROOT, GetHolder());
            Unit::AuraList const& slowingAuras = target->GetAurasByType(SPELL_AURA_MOD_DECREASE_SPEED);
            for (Unit::AuraList::const_iterator iter = slowingAuras.begin(); iter != slowingAuras.end();)
            {
                SpellEntry const* aurSpellInfo = (*iter)->GetSpellProto();

                uint32 aurMechMask = GetAllSpellMechanicMask(aurSpellInfo);

                // If spell that caused this aura has Croud Control or Daze effect
                if ((aurMechMask & MECHANIC_NOT_REMOVED_BY_SHAPESHIFT) ||
                    // some Daze spells have these parameters instead of MECHANIC_DAZE (skip snare spells)
                    (aurSpellInfo->SpellIconID == 15 && aurSpellInfo->Dispel == 0 &&
                    (aurMechMask & (1 << (MECHANIC_SNARE - 1))) == 0))
                {
                    ++iter;
                    continue;
                }

                // All OK, remove aura now
                target->RemoveAurasDueToSpellByCancel(aurSpellInfo->Id);
                iter = slowingAuras.begin();
            }

            // and polymorphic affects
            if (target->IsPolymorphed())
            {
                target->RemoveAurasDueToSpell(target->GetTransform());
            }

            //no break here
        }
        case FORM_GHOSTWOLF:
        {
            // remove water walk aura. TODO:: there is probably better way to do this
            target->RemoveSpellsCausingAura(SPELL_AURA_WATER_WALK);

            break;
        }
        default:
            break;
    }

    if (apply)
    {
        // remove other shapeshift before applying a new one
        target->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT, GetHolder());

        if (modelid > 0)
        {
            target->SetObjectScale(DEFAULT_OBJECT_SCALE * target->GetObjectScaleMod());
            target->SetDisplayId(modelid);
        }

        if (PowerType != POWER_MANA)
        {
            // reset power to default values only at power change
            if (target->GetPowerType() != PowerType)
            {
                target->SetPowerType(PowerType);
            }

            switch (form)
            {
                case FORM_CAT:
                case FORM_BEAR:
                case FORM_DIREBEAR:
                {
                    // get furor proc chance
                    int32 furorChance = 0;
                    Unit::AuraList const& mDummy = target->GetAurasByType(SPELL_AURA_DUMMY);
                    for (Unit::AuraList::const_iterator i = mDummy.begin(); i != mDummy.end(); ++i)
                    {
                        if ((*i)->GetSpellProto()->SpellIconID == 238)
                        {
                            furorChance = (*i)->GetModifier()->m_amount;
                            break;
                        }
                    }

                    if (m_modifier.m_miscvalue == FORM_CAT)
                    {
                        target->SetPower(POWER_ENERGY, 0);
                        if (irand(1, 100) <= furorChance)
                        {
                            target->CastSpell(target, 17099, true, NULL, this);
                        }
                    }
                    else
                    {
                        target->SetPower(POWER_RAGE, 0);
                        if (irand(1, 100) <= furorChance)
                        {
                            target->CastSpell(target, 17057, true, NULL, this);
                        }
                    }
                    break;
                }
                case FORM_BATTLESTANCE:
                case FORM_DEFENSIVESTANCE:
                case FORM_BERSERKERSTANCE:
                {
                    uint32 Rage_val = 0;
                    // Tactical mastery
                    if (target->GetTypeId() == TYPEID_PLAYER)
                    {
                        Unit::AuraList const& aurasOverrideClassScripts = target->GetAurasByType(SPELL_AURA_OVERRIDE_CLASS_SCRIPTS);
                        for (Unit::AuraList::const_iterator iter = aurasOverrideClassScripts.begin(); iter != aurasOverrideClassScripts.end(); ++iter)
                        {
                            // select by script id
                            switch ((*iter)->GetModifier()->m_miscvalue)
                            {
                                case 831: Rage_val =  50; break;
                                case 832: Rage_val = 100; break;
                                case 833: Rage_val = 150; break;
                                case 834: Rage_val = 200; break;
                                case 835: Rage_val = 250; break;
                            }
                            if (Rage_val != 0)
                            {
                                break;
                            }
                        }
                    }
                    if (target->GetPower(POWER_RAGE) > Rage_val)
                    {
                        target->SetPower(POWER_RAGE, Rage_val);
                    }
                    break;
                }
                default:
                    break;
            }
        }

        target->SetShapeshiftForm(form);
    }
    else
    {
        if (modelid > 0)
        {
            // workaround for tauren scale appear too big
            if (target->getRace() == RACE_TAUREN)
            {
                if (target->getGender() == GENDER_MALE)
                {
                    target->SetObjectScale(DEFAULT_TAUREN_MALE_SCALE * target->GetObjectScaleMod());
                }
                else
                {
                    target->SetObjectScale(DEFAULT_TAUREN_FEMALE_SCALE * target->GetObjectScaleMod());
                }
            }

            target->SetDisplayId(target->GetNativeDisplayId());
        }

        if (target->getClass() == CLASS_DRUID)
        {
            target->SetPowerType(POWER_MANA);
        }

        target->SetShapeshiftForm(FORM_NONE);
    }

    // adding/removing linked auras
    // add/remove the shapeshift aura's boosts
    HandleShapeshiftBoosts(apply);

    if (target->GetTypeId() == TYPEID_PLAYER)
    {
        ((Player*)target)->InitDataForForm();
    }
}

/**
 * @brief Applies or removes a transform model effect.
 *
 * @param apply True to apply the transform; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleAuraTransform(bool apply, bool Real)
{
    Unit* target = GetTarget();
    if (apply)
    {
        // special case (spell specific functionality)
        if (m_modifier.m_miscvalue == 0)
        {
            switch (GetId())
            {
                case 16739:                                 // Orb of Deception
                {
                    uint32 orb_model = target->GetNativeDisplayId();
                    switch (orb_model)
                    {
                        // Troll Female
                        case 1479: target->SetDisplayId(10134); break;
                        // Troll Male
                        case 1478: target->SetDisplayId(10135); break;
                        // Tauren Male
                        case 59:   target->SetDisplayId(10136); break;
                        // Human Male
                        case 49:   target->SetDisplayId(10137); break;
                        // Human Female
                        case 50:   target->SetDisplayId(10138); break;
                        // Orc Male
                        case 51:   target->SetDisplayId(10139); break;
                        // Orc Female
                        case 52:   target->SetDisplayId(10140); break;
                        // Dwarf Male
                        case 53:   target->SetDisplayId(10141); break;
                        // Dwarf Female
                        case 54:   target->SetDisplayId(10142); break;
                        // NightElf Male
                        case 55:   target->SetDisplayId(10143); break;
                        // NightElf Female
                        case 56:   target->SetDisplayId(10144); break;
                        // Undead Female
                        case 58:   target->SetDisplayId(10145); break;
                        // Undead Male
                        case 57:   target->SetDisplayId(10146); break;
                        // Tauren Female
                        case 60:   target->SetDisplayId(10147); break;
                        // Gnome Male
                        case 1563: target->SetDisplayId(10148); break;
                        // Gnome Female
                        case 1564: target->SetDisplayId(10149); break;
                        default: break;
                    }
                    break;
                }
                default:
                    sLog.outError("Aura::HandleAuraTransform, spell %u does not have creature entry defined, need custom defined model.", GetId());
                    break;
            }
        }
        else                                                // m_modifier.m_miscvalue != 0
        {
            uint32 model_id;

            CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_modifier.m_miscvalue);
            if (!ci)
            {
                model_id = 16358;                           // pig pink ^_^
                sLog.outError("Auras: unknown creature id = %d (only need its modelid) Form Spell Aura Transform in Spell ID = %d", m_modifier.m_miscvalue, GetId());
            }
            else
            {
                model_id = Creature::ChooseDisplayId(ci);    // Will use the default model here
            }

            target->SetDisplayId(model_id);

            // creature case, need to update equipment if additional provided
            if (ci && target->GetTypeId() == TYPEID_UNIT)
            {
                ((Creature*)target)->LoadEquipment(ci->EquipmentTemplateId, false);
            }
        }

        // update active transform spell only not set or not overwriting negative by positive case
        if (!target->GetTransform() || !IsPositiveSpell(GetId()) || IsPositiveSpell(target->GetTransform()))
        {
            target->SetTransform(GetId());
        }
    }
    else                                                    // !apply
    {
        // ApplyModifier(true) will reapply it if need
        target->SetTransform(0);
        target->SetDisplayId(target->GetNativeDisplayId());

        // apply default equipment for creature case
        if (target->GetTypeId() == TYPEID_UNIT)
        {
            ((Creature*)target)->LoadEquipment(((Creature*)target)->GetCreatureInfo()->EquipmentTemplateId, true);
        }

        // re-apply some from still active with preference negative cases
        Unit::AuraList const& otherTransforms = target->GetAurasByType(SPELL_AURA_TRANSFORM);
        if (!otherTransforms.empty())
        {
            // look for other transform auras
            Aura* handledAura = *otherTransforms.begin();
            for (Unit::AuraList::const_iterator i = otherTransforms.begin(); i != otherTransforms.end(); ++i)
            {
                // negative auras are preferred
                if (!IsPositiveSpell((*i)->GetSpellProto()->Id))
                {
                    handledAura = *i;
                    break;
                }
            }
            handledAura->ApplyModifier(true);
        }
    }
}

/**
 * @brief Applies or removes a forced reputation reaction for a player.
 *
 * @param apply True to apply the forced reaction; false to remove it.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleForceReaction(bool apply, bool Real)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (!Real)
    {
        return;
    }

    Player* player = (Player*)GetTarget();

    uint32 faction_id = m_modifier.m_miscvalue;
    ReputationRank faction_rank = ReputationRank(m_modifier.m_amount);

    player->GetReputationMgr().ApplyForceReaction(faction_id, faction_rank, apply);
    player->GetReputationMgr().SendForceReactions();

    // stop fighting if at apply forced rank friendly or at remove real rank friendly
    if ((apply && faction_rank >= REP_FRIENDLY) || (!apply && player->GetReputationRank(faction_id) >= REP_FRIENDLY))
    {
        player->StopAttackFaction(faction_id);
    }
}

/**
 * @brief Applies or removes a player skill bonus from the aura.
 *
 * @param apply True to apply the bonus; false to remove it.
 * @param Real Unused.
 */
void Aura::HandleAuraModSkill(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    uint32 prot = GetSpellProto()->EffectMiscValue[m_effIndex];
    int32 points = GetModifier()->m_amount;

    ((Player*)GetTarget())->ModifySkillBonus(prot, (apply ? points : -points), m_modifier.m_auraname == SPELL_AURA_MOD_SKILL_TALENT);
    if (prot == SKILL_DEFENSE)
    {
        ((Player*)GetTarget())->UpdateDefenseBonusesMod();
    }
}

/**
 * @brief Awards the configured item when a channel-death aura ends by death.
 *
 * @param apply True on application; false on removal.
 * @param Real True when processing the real aura state change.
 */
void Aura::HandleChannelDeathItem(bool apply, bool Real)
{
    if (Real && !apply)
    {
        if (m_removeMode != AURA_REMOVE_BY_DEATH)
        {
            return;
        }
        // Item amount
        if (m_modifier.m_amount <= 0)
        {
            return;
        }

        SpellEntry const* spellInfo = GetSpellProto();
        if (spellInfo->EffectItemType[m_effIndex] == 0)
        {
            return;
        }

        Unit* caster = GetCaster();
        if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
        {
            return;
        }

        uint32 createdItemId = spellInfo->EffectItemType[m_effIndex];

        // Soul Shard (target req.)
        if (createdItemId == ITEM_SOUL_SHARD)
        {
            Unit* victim = GetTarget();

            // Only from non-grey units
            if (!((Player*)caster)->isHonorOrXPTarget(victim) ||
                (victim->GetTypeId() == TYPEID_UNIT && !((Creature*)victim)->IsTappedBy((Player*)caster)))
            {
                return;
            }

            // Avoid awarding multiple souls on the same target
            // 1.11.0: If you cast Drain Soul while shadowburn is on the victim, you will no longer receive two soul shards upon the victim's death.
            for (auto const& aura : victim->GetAurasByType(SPELL_AURA_CHANNEL_DEATH_ITEM))
            {
                if (aura != this && caster->GetObjectGuid() == aura->GetCasterGuid() && aura->GetSpellProto()->EffectItemType[aura->GetEffIndex()] == ITEM_SOUL_SHARD)
                {
                    return;
                }
            }

        }

        // Adding items
        uint32 noSpaceForCount = 0;
        uint32 count = m_modifier.m_amount;

        ItemPosCountVec dest;
        InventoryResult msg = ((Player*)caster)->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, createdItemId, count, &noSpaceForCount);
        if (msg != EQUIP_ERR_OK)
        {
            count -= noSpaceForCount;
            ((Player*)caster)->SendEquipError(msg, NULL, NULL, createdItemId);
            if (count == 0)
            {
                return;
            }
        }

        Item* newitem = ((Player*)caster)->StoreNewItem(dest, createdItemId, true);
        ((Player*)caster)->SendNewItem(newitem, count, true, true);
    }
}

/**
 * @brief Redirects the caster camera to the target while the aura is active.
 *
 * @param apply True to bind sight; false to restore normal view.
 * @param Real Unused.
 */
void Aura::HandleBindSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
    {
        camera.SetView(GetTarget());
    }
    else
    {
        camera.ResetView();
    }
}

/**
 * @brief Redirects the caster camera for farsight while the aura is active.
 *
 * @param apply True to enable farsight; false to restore normal view.
 * @param Real Unused.
 */
void Aura::HandleFarSight(bool apply, bool /*Real*/)
{
    Unit* caster = GetCaster();
    if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    Camera& camera = ((Player*)caster)->GetCamera();
    if (apply)
    {
        camera.SetView(GetTarget());
    }
    else
    {
        camera.ResetView();
    }
}

/**
 * @brief Applies or removes creature tracking flags on a player.
 *
 * @param apply True to enable tracking; false to disable it.
 * @param Real Unused.
 */
void Aura::HandleAuraTrackCreatures(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (apply)
    {
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());
    }

    if (apply)
    {
        GetTarget()->SetFlag(PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
    else
    {
        GetTarget()->RemoveFlag(PLAYER_TRACK_CREATURES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
}

/**
 * @brief Applies or removes resource tracking flags on a player.
 *
 * @param apply True to enable tracking; false to disable it.
 * @param Real Unused.
 */
void Aura::HandleAuraTrackResources(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (apply)
    {
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());
    }

    if (apply)
    {
        GetTarget()->SetFlag(PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
    else
    {
        GetTarget()->RemoveFlag(PLAYER_TRACK_RESOURCES, uint32(1) << (m_modifier.m_miscvalue - 1));
    }
}

/**
 * @brief Applies or removes stealthed-unit tracking on a player.
 *
 * @param apply True to enable tracking; false to disable it.
 * @param Real Unused.
 */
void Aura::HandleAuraTrackStealthed(bool apply, bool /*Real*/)
{
    if (GetTarget()->GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }

    if (apply)
    {
        GetTarget()->RemoveNoStackAurasDueToAuraHolder(GetHolder());
    }

    GetTarget()->ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_TRACK_STEALTHED, apply);
}

/**
 * @brief Applies or removes a scale modifier and refreshes model data.
 *
 * @param apply True to apply the scale change; false to remove it.
 * @param Real Unused.
 */
void Aura::HandleAuraModScale(bool apply, bool /*Real*/)
{
    GetTarget()->ApplyPercentModFloatValue(OBJECT_FIELD_SCALE_X, float(m_modifier.m_amount), apply);
    GetTarget()->UpdateModelData();
}
