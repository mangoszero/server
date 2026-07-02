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



#include "Unit.h"
#include "Log.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "Spell.h"
#include "Group.h"
#include "SpellAuras.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Formulas.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "MapPersistentStateMgr.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "VMapFactory.h"
#include "MovementGenerator.h"
#include "movement/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include "DynamicObject.h"
#include "GameObject.h"
#include <math.h>
#include <stdarg.h>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaConfig.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_ELUNA
#include "ElunaEventMgr.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Registers a dynamic object owned by the unit.
 *
 * @param dynObj The dynamic object to register.
 */
void Unit::AddDynObject(DynamicObject* dynObj)
{
    m_dynObjGUIDs.push_back(dynObj->GetObjectGuid());
}

/**
 * @brief Removes owned dynamic objects for a spell or all of them.
 *
 * @param spellid The spell identifier to match, or 0 for all.
 */
void Unit::RemoveDynObject(uint32 spellid)
{
    if (m_dynObjGUIDs.empty())
    {
        return;
    }
    for (GuidList::iterator i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
        }
        else if (spellid == 0 || dynObj->GetSpellId() == spellid)
        {
            dynObj->Delete();
            i = m_dynObjGUIDs.erase(i);
        }
        else
        {
            ++i;
        }
    }
}

/**
 * @brief Removes all dynamic objects owned by the unit.
 */
void Unit::RemoveAllDynObjects()
{
    while (!m_dynObjGUIDs.empty())
    {
        if (DynamicObject* dynObj = GetMap()->GetDynamicObject(*m_dynObjGUIDs.begin()))
        {
            dynObj->Delete();
        }
        m_dynObjGUIDs.erase(m_dynObjGUIDs.begin());
    }
}

/**
 * @brief Gets an owned dynamic object by spell id and effect index.
 *
 * @param spellId The spell identifier.
 * @param effIndex The effect index.
 * @return The matching dynamic object, or NULL if none exists.
 */
DynamicObject* Unit::GetDynObject(uint32 spellId, SpellEffectIndex effIndex)
{
    for (GuidList::iterator i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId && dynObj->GetEffIndex() == effIndex)
        {
            return dynObj;
        }
        ++i;
    }
    return NULL;
}

/**
 * @brief Gets an owned dynamic object by spell id.
 *
 * @param spellId The spell identifier.
 * @return The matching dynamic object, or NULL if none exists.
 */
DynamicObject* Unit::GetDynObject(uint32 spellId)
{
    for (GuidList::iterator i = m_dynObjGUIDs.begin(); i != m_dynObjGUIDs.end();)
    {
        DynamicObject* dynObj = GetMap()->GetDynamicObject(*i);
        if (!dynObj)
        {
            i = m_dynObjGUIDs.erase(i);
            continue;
        }

        if (dynObj->GetSpellId() == spellId)
        {
            return dynObj;
        }
        ++i;
    }
    return NULL;
}

/**
 * @brief Gets an owned or tracked gameobject created by a spell.
 *
 * @param spellId The spell identifier.
 * @return The matching gameobject, or NULL if none exists.
 */
GameObject* Unit::GetGameObject(uint32 spellId) const
{
    for (GameObjectList::const_iterator i = m_gameObj.begin(); i != m_gameObj.end(); ++i)
    {
        if ((*i)->GetSpellId() == spellId)
        {
            return *i;
        }
    }

    WildGameObjectMap::const_iterator find = m_wildGameObjs.find(spellId);
    if (find != m_wildGameObjs.end())
    {
        return GetMap()->GetGameObject(find->second); // Can be NULL
    }

    return NULL;
}

/**
 * @brief Registers an owned gameobject created by the unit.
 *
 * @param gameObj The gameobject to register.
 */
void Unit::AddGameObject(GameObject* gameObj)
{
    MANGOS_ASSERT(gameObj && !gameObj->GetOwnerGuid());
    m_gameObj.push_back(gameObj);
    gameObj->SetOwnerGuid(GetObjectGuid());

    if (GetTypeId() == TYPEID_PLAYER && gameObj->GetSpellId())
    {
        SpellEntry const* createBySpell = sSpellStore.LookupEntry(gameObj->GetSpellId());
        // Need disable spell use for owner
        if (createBySpell && createBySpell->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
            // note: item based cooldowns and cooldown spell mods with charges ignored (unknown existing cases)
        {
            ((Player*)this)->AddSpellAndCategoryCooldowns(createBySpell, 0, NULL, true);
        }
    }
}

/**
 * @brief Registers a wild summoned gameobject tracked by spell id.
 *
 * @param gameObj The wild gameobject to register.
 */
void Unit::AddWildGameObject(GameObject* gameObj)
{
    MANGOS_ASSERT(gameObj && gameObj->GetOwnerGuid().IsEmpty());
    m_wildGameObjs[gameObj->GetSpellId()] = gameObj->GetObjectGuid();

    // As of 335 there are no wild-summon spells with SPELL_ATTR_DISABLED_WHILE_ACTIVE

    // Remove outdated wild summoned GOs
    for (WildGameObjectMap::iterator itr = m_wildGameObjs.begin(); itr != m_wildGameObjs.end();)
    {
        GameObject* pGo = GetMap()->GetGameObject(itr->second);
        if (pGo)
        {
            ++itr;
        }
        else
        {
            m_wildGameObjs.erase(itr++);
        }
    }
}

/**
 * @brief Unregisters an owned gameobject and optionally deletes it.
 *
 * @param gameObj The gameobject to remove.
 * @param del True to delete the gameobject from the world.
 */
void Unit::RemoveGameObject(GameObject* gameObj, bool del)
{
    MANGOS_ASSERT(gameObj && gameObj->GetOwnerGuid() == GetObjectGuid());

    gameObj->SetOwnerGuid(ObjectGuid());

    // GO created by some spell
    if (uint32 spellid = gameObj->GetSpellId())
    {
        RemoveAurasDueToSpell(spellid);

        if (GetTypeId() == TYPEID_PLAYER)
        {
            SpellEntry const* createBySpell = sSpellStore.LookupEntry(spellid);
            // Need activate spell use for owner
            if (createBySpell && createBySpell->HasAttribute(SPELL_ATTR_DISABLED_WHILE_ACTIVE))
                // note: item based cooldowns and cooldown spell mods with charges ignored (unknown existing cases)
            {
                ((Player*)this)->SendCooldownEvent(createBySpell);
            }
        }
    }

    m_gameObj.remove(gameObj);

    if (del)
    {
        gameObj->SetRespawnTime(0);
        gameObj->Delete();
    }
}

/**
 * @brief Removes owned gameobjects created by a spell or all of them.
 *
 * @param spellid The spell identifier to match, or 0 for all.
 * @param del True to delete matching gameobjects from the world.
 */
void Unit::RemoveGameObject(uint32 spellid, bool del)
{
    if (m_gameObj.empty())
    {
        return;
    }

    GameObjectList::iterator i, next;
    for (i = m_gameObj.begin(); i != m_gameObj.end(); i = next)
    {
        next = i;
        if (spellid == 0 || (*i)->GetSpellId() == spellid)
        {
            (*i)->SetOwnerGuid(ObjectGuid());
            if (del)
            {
                (*i)->SetRespawnTime(0);
                (*i)->Delete();
            }

            next = m_gameObj.erase(i);
        }
        else
        {
            ++next;
        }
    }
}

/**
 * @brief Removes all owned gameobjects and clears wild summon tracking.
 */
void Unit::RemoveAllGameObjects()
{
    // remove references to unit
    for (GameObjectList::iterator i = m_gameObj.begin(); i != m_gameObj.end();)
    {
        (*i)->SetOwnerGuid(ObjectGuid());
        (*i)->SetRespawnTime(0);
        (*i)->Delete();
        i = m_gameObj.erase(i);
    }

    // wild summoned GOs - only remove references, do not remove GOs
    m_wildGameObjs.clear();
}
