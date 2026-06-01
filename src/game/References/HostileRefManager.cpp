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
 * @file HostileRefManager.cpp
 * @brief Hostile threat reference management
 *
 * This file implements HostileRefManager which tracks all units that have
 * threat (hostility) against a unit. It maintains the list of HostileReference
 * objects representing entities in combat with the owner.
 *
 * Key responsibilities:
 * - Track all units threatening the owner
 * - Distribute threat (e.g., healing aggro, buff threat)
 * - Manage online/offline state for threat calculations
 * - Cleanup references when units are destroyed
 *
 * Used primarily by creatures/NPCs to manage their threat table and
 * determine who to attack.
 *
 * @see HostileRefManager for the manager class
 * @see HostileReference for individual threat relationships
 * @see ThreatManager for threat value management
 */

#include "HostileRefManager.h"
#include "ThreatManager.h"
#include "Unit.h"
#include "DBCStructure.h"
#include "SpellMgr.h"

/**
 * @brief Construct HostileRefManager
 * @param pOwner Unit that owns this manager (the one being threatened)
 */
HostileRefManager::HostileRefManager(Unit* pOwner) : iOwner(pOwner)
{
}

/**
 * @brief Destroy HostileRefManager
 *
 * Cleans up all remaining hostile references.
 */
HostileRefManager::~HostileRefManager()
{
    deleteReferences();
}

/**
 * @brief Distribute threat to all hostile units (threat assist)
 * @param pVictim Unit generating the threat (e.g., healer, buffer)
 * @param pThreat Base threat amount
 * @param pThreatSpell Spell that generated the threat (optional)
 * @param pSingleTarget If true, don't divide threat among targets
 *
 * Used for healing/buff threat distribution. When a unit heals or buffs
 * someone in combat, threat is distributed to all units hostile to the
 * healer/buffer.
 *
 * Threat is divided equally among all hostile units unless pSingleTarget
 * is set.
 */
void HostileRefManager::threatAssist(Unit* pVictim, float pThreat, SpellEntry const* pThreatSpell, bool pSingleTarget)
{
    uint32 size = pSingleTarget ? 1 : getSize();            // if pSingleTarget do not divide threat
    float threat = pThreat / size;
    HostileReference* ref = getFirst();
    while (ref)
    {
        ref->getSource()->addThreat(pVictim, threat, false, (pThreatSpell ? GetSpellSchoolMask(pThreatSpell) : SPELL_SCHOOL_MASK_NORMAL), pThreatSpell);

        ref = ref->next();
    }
}

/**
 * @brief Modify all threat by percentage
 * @param pValue Percentage to modify (can be negative for reduction)
 *
 * Applies a percentage modifier to all threat values.
 * Used for threat-reduction abilities like Feint or threat-modifying auras.
 */
void HostileRefManager::addThreatPercent(int32 pValue)
{
    HostileReference* ref;

    ref = getFirst();
    while (ref != NULL)
    {
        ref->addThreatPercent(pValue);
        ref = ref->next();
    }
}

/**
 * @brief Set online/offline state for all references
 * @param pIsOnline New online state
 *
 * Sets the online state for all hostile references. Online state affects
 * whether threat decays and whether the unit is considered for attack.
 *
 * @note Caller must calculate the appropriate state before calling.
 */
void HostileRefManager::setOnlineOfflineState(bool pIsOnline)
{
    HostileReference* ref;

    ref = getFirst();
    while (ref != NULL)
    {
        ref->setOnlineOfflineState(pIsOnline);
        ref = ref->next();
    }
}

/**
 * @brief Update online status for all references
 *
 * Recalculates and updates the online/offline status for all hostile
 * references. Called periodically to refresh threat table validity.
 */
void HostileRefManager::updateThreatTables()
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        ref->updateOnlineStatus();
        ref = ref->next();
    }
}

/**
 * @brief Delete all hostile references
 *
 * Removes and deletes all hostile references. Called when the manager
 * is destroyed or when clearing the threat table completely.
 *
 * Notifies each source to remove the reference from its list.
 */
void HostileRefManager::deleteReferences()
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        ref->removeReference();
        delete ref;
        ref = nextRef;
    }
}

/**
 * @brief Delete references for a specific faction
 * @param faction Faction ID to match
 *
 * Removes all hostile references from units of the specified faction.
 * Used when faction reputation changes make units non-hostile.
 */
void HostileRefManager::deleteReferencesForFaction(uint32 faction)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner()->getFactionTemplateEntry()->faction == faction)
        {
            ref->removeReference();
            delete ref;
        }
        ref = nextRef;
    }
}

/**
 * @brief Delete reference for a specific unit
 * @param pCreature Unit whose reference should be removed
 *
 * Removes the hostile reference for the specified unit from the manager.
 * Used when a unit leaves combat or is destroyed.
 */
void HostileRefManager::deleteReference(Unit* pCreature)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner() == pCreature)
        {
            ref->removeReference();
            delete ref;
            break;
        }
        ref = nextRef;
    }
}

/**
 * @brief Set online/offline state for a specific unit's reference
 * @param pCreature Unit whose reference to modify
 * @param pIsOnline New online state
 *
 * Sets the online/offline state for the hostile reference of the
 * specified unit only.
 */
void HostileRefManager::setOnlineOfflineState(Unit* pCreature, bool pIsOnline)
{
    HostileReference* ref = getFirst();
    while (ref)
    {
        HostileReference* nextRef = ref->next();
        if (ref->getSource()->getOwner() == pCreature)
        {
            ref->setOnlineOfflineState(pIsOnline);
            break;
        }
        ref = nextRef;
    }
}

//=================================================
