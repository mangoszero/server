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

#include "Totem.h"
#include "Log.h"
#include "Group.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "SpellMgr.h"
#include "DBCStores.h"
#include "CreatureAI.h"
#include "InstanceData.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Initializes a totem creature instance.
 */
Totem::Totem() : Creature(CREATURE_SUBTYPE_TOTEM)
{
    m_duration = 0;
    m_type = TOTEM_PASSIVE;
}

/**
 * @brief Creates a totem from creature prototype data.
 *
 * Places the totem on the target map, adjusts its spawn height when needed, and
 * registers it with instance data before loading creature addons.
 *
 * @param guidlow The low part of the totem GUID.
 * @param cPos The desired creation position.
 * @param cinfo The creature template used for creation.
 * @param owner The unit that owns the totem.
 * @return true if creation succeeded; otherwise, false.
 */
bool Totem::Create(uint32 guidlow, CreatureCreatePos& cPos, CreatureInfo const* cinfo, Unit* owner)
{
    SetMap(cPos.GetMap());

    Team team = owner->GetTypeId() == TYPEID_PLAYER ? ((Player*)owner)->GetTeam() : TEAM_NONE;

    if (!CreateFromProto(guidlow, cinfo, team))
    {
        return false;
    }

    cPos.SelectFinalPoint(this);

    // totem must be at same Z in case swimming caster and etc.
    if (fabs(cPos.m_pos.z - owner->GetPositionZ()) > 5.0f)
    {
        cPos.m_pos.z = owner->GetPositionZ();
    }

    if (!cPos.Relocate(this))
    {
        return false;
    }

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that map.
    // Normally non-players do not teleport to other maps.
    if (InstanceData* iData = GetMap()->GetInstanceData())
    {
        iData->OnCreatureCreate(this);
    }

    LoadCreatureAddon(false);

    return true;
}

/**
 * @brief Updates the totem each server tick.
 *
 * Unsummons the totem when its owner dies, the totem dies, or its duration expires.
 * Otherwise, forwards update processing to the base creature implementation.
 *
 * @param update_diff The elapsed time since the last update in milliseconds.
 * @param time The current world update time.
 */
void Totem::Update(uint32 update_diff, uint32 time)
{
    Unit* owner = GetOwner();
    if (!owner || !owner->IsAlive() || !IsAlive())
    {
        UnSummon();                                         // remove self
        return;
    }

    if (m_duration <= update_diff)
    {
        UnSummon();                                         // remove self
        return;
    }
    else
    {
        m_duration -= update_diff;
    }

    Creature::Update(update_diff, time);
}

/**
 * @brief Finalizes a totem summon.
 *
 * Adds the totem to the map, initializes AI, sends spawn animation data, and casts
 * the configured spell for passive or statue totems.
 *
 * @param owner The unit that summoned the totem.
 */
void Totem::Summon(Unit* owner)
{
    owner->GetMap()->Add((Creature*)this);
    AIM_Initialize();

    WorldPacket data(SMSG_GAMEOBJECT_SPAWN_ANIM_OBSOLETE, 8);
    data << GetObjectGuid();
    SendMessageToSet(&data, true);

    if (owner->GetTypeId() == TYPEID_UNIT && ((Creature*)owner)->AI())
    {
        ((Creature*)owner)->AI()->JustSummoned((Creature*)this);
    }
#ifdef ENABLE_ELUNA
    if (Eluna* e = this->GetEluna())
    {
        e->OnSummoned(this, owner);
    }
#endif /* ENABLE_ELUNA */

    // there are some totems, which exist just for their visual appeareance
    if (!GetSpell())
    {
        return;
    }

    switch (m_type)
    {
        case TOTEM_PASSIVE:
            CastSpell(this, GetSpell(), true);
            break;
        case TOTEM_STATUE:
            CastSpell(GetOwner(), GetSpell(), true);
            break;
        default: break;
    }
}

/**
 * @brief Unsummons the totem and removes its effects.
 *
 * Stops combat, removes spell effects from the owner and nearby party members,
 * notifies creature AI hooks, and schedules the totem for removal.
 */
void Totem::UnSummon()
{
    SendObjectDeSpawnAnim(GetObjectGuid());

    CombatStop();
    RemoveAurasDueToSpell(GetSpell());

    if (Unit* owner = GetOwner())
    {
        owner->_RemoveTotem(this);
        owner->RemoveAurasDueToSpell(GetSpell());

        // remove aura all party members too
        if (owner->GetTypeId() == TYPEID_PLAYER)
        {
            // Not only the player can summon the totem (scripted AI)
            if (Group* pGroup = ((Player*)owner)->GetGroup())
            {
                for (GroupReference* itr = pGroup->GetFirstMember(); itr != NULL; itr = itr->next())
                {
                    Player* Target = itr->getSource();
                    if (Target && pGroup->SameSubGroup((Player*)owner, Target))
                    {
                        Target->RemoveAurasDueToSpell(GetSpell());
                    }
                }
            }
        }

        if (owner->GetTypeId() == TYPEID_UNIT && ((Creature*)owner)->AI())
        {
            ((Creature*)owner)->AI()->SummonedCreatureDespawn((Creature*)this);
        }
    }

    // any totem unsummon look like as totem kill, req. for proper animation
    if (IsAlive())
    {
        SetDeathState(DEAD);
    }

    AddObjectToRemoveList();
}

/**
 * @brief Assigns the totem owner data.
 *
 * Copies creator, owner, faction, and level information from the summoning unit.
 *
 * @param owner The unit that owns the totem.
 */
void Totem::SetOwner(Unit* owner)
{
    SetCreatorGuid(owner->GetObjectGuid());
    SetOwnerGuid(owner->GetObjectGuid());
    setFaction(owner->getFaction());
    SetLevel(owner->getLevel());
}

/**
 * @brief Retrieves the unit that owns this totem.
 *
 * @return Pointer to the owning unit, or NULL if the owner cannot be found.
 */
Unit* Totem::GetOwner()
{
    if (ObjectGuid ownerGuid = GetOwnerGuid())
    {
        return sObjectAccessor.GetUnit(*this, ownerGuid);
    }

    return NULL;
}

/**
 * @brief Determines the totem type from the summon spell.
 *
 * Uses the totem spell cast time and special icon handling to classify the totem
 * as active, passive, or statue.
 *
 * @param spellProto The summon spell that created the totem.
 */
void Totem::SetTypeBySummonSpell(SpellEntry const* spellProto)
{
    // Get spell casted by totem
    SpellEntry const* totemSpell = sSpellStore.LookupEntry(GetSpell());
    if (totemSpell)
    {
        // If spell have cast time -> so its active totem
        if (GetSpellCastTime(totemSpell))
        {
            m_type = TOTEM_ACTIVE;
        }
    }
    if (spellProto->SpellIconID == 2056)
    {
        m_type = TOTEM_STATUE; // Jewelery statue
    }
}

/**
 * @brief Checks whether a spell effect is ignored by the totem.
 *
 * Totems reject most healing, energizing, and periodic regeneration effects, while
 * allowing specific shaman spell interactions.
 *
 * @param spellInfo The spell being evaluated.
 * @param index The effect index within the spell.
 * @param castOnSelf Indicates whether the spell is cast on the totem itself.
 * @return true if the effect is immune on the totem; otherwise, false.
 */
bool Totem::IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const
{
    // Totem may affected by some specific spells
    // Mana Spring, Healing stream, Mana tide
    // Flags : 0x00000002000 | 0x00000004000 | 0x00004000000 -> 0x00004006000
    if (spellInfo->SpellFamilyName == SPELLFAMILY_SHAMAN && spellInfo->IsFitToFamilyMask(UI64LIT(0x00004006000)))
    {
        return false;
    }

    switch (spellInfo->Effect[index])
    {
        case SPELL_EFFECT_ATTACK_ME:
            // immune to any type of regeneration effects hp/mana etc.
        case SPELL_EFFECT_HEAL:
        case SPELL_EFFECT_HEAL_MAX_HEALTH:
        case SPELL_EFFECT_HEAL_MECHANICAL:
        case SPELL_EFFECT_ENERGIZE:
            return true;
        default:
            break;
    }

    if (!IsPositiveSpell(spellInfo))
    {
        // immune to all negative auras
        if (IsAuraApplyEffect(spellInfo, index))
        {
            return true;
        }
    }
    else
    {
        // immune to any type of regeneration auras hp/mana etc.
        if (IsPeriodicRegenerateEffect(spellInfo, index))
        {
            return true;
        }
    }

    return Creature::IsImmuneToSpellEffect(spellInfo, index, castOnSelf);
}
