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
 * @file ThreatManager.cpp
 * @brief Threat management system implementation
 *
 * This file implements the threat management system used by creatures/NPCs
 * to determine which target to attack in combat. The system maintains threat
 * values for all units in combat and applies the 110%/130% threat rules.
 *
 * Key components:
 * - ThreatCalcHelper: Calculates threat values with modifiers
 * - HostileReference: Individual threat relationship between units
 * - ThreatContainer: Sorted list of threatening units
 * - ThreatManager: Main threat management for a unit
 *
 * @see ThreatManager for the main manager class
 * @see HostileReference for individual threat relationships
 * @see ThreatContainer for threat list management
 */

#include "ThreatManager.h"
#include "Unit.h"
#include "Creature.h"
#include "CreatureAI.h"
#include "Map.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "UnitEvents.h"

//==============================================================
//================= ThreatCalcHelper ===========================
//==============================================================

/**
 * @brief Calculate threat value
 * @param pHatedUnit Unit being threatened
 * @param pHatingUnit Unit generating threat (unused)
 * @param threat Base threat value
 * @param crit If true, threat was from a critical hit
 * @param schoolMask Spell school mask
 * @param pThreatSpell Spell that generated threat
 * @return Calculated threat value
 *
 * Calculates the final threat value after applying all modifiers
 * including spell mods, critical threat multipliers, and aura modifiers.
 */
float ThreatCalcHelper::CalcThreat(Unit* pHatedUnit, Unit* /*pHatingUnit*/, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* pThreatSpell)
{
    // all flat mods applied early
    if (!threat)
    {
        return 0.0f;
    }

    if (pThreatSpell)
    {
        if (pThreatSpell->HasAttribute(SPELL_ATTR_EX_NO_THREAT))
        {
            return 0.0f;
        }

        if (Player* modOwner = pHatedUnit->GetSpellModOwner())
        {
            modOwner->ApplySpellMod(pThreatSpell->ID, SPELLMOD_THREAT, threat);
        }

        if (crit)
        {
            threat *= pHatedUnit->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_CRITICAL_THREAT, schoolMask);
        }
    }

    threat = pHatedUnit->ApplyTotalThreatModifier(threat, schoolMask);
    return threat;
}

//============================================================
//================= HostileReference ==========================
//============================================================

/**
 * @brief HostileReference constructor
 * @param pUnit Unit threatening the target
 * @param pThreatManager Threat manager for the target
 * @param pThreat Initial threat value
 *
 * Creates a new hostile reference linking a unit to its threat target.
 */
HostileReference::HostileReference(Unit* pUnit, ThreatManager* pThreatManager, float pThreat)
{
    iThreat = pThreat;
    iTempThreatModifyer = 0.0f;
    link(pUnit, pThreatManager);
    iUnitGuid = pUnit->GetObjectGuid();
    iOnline = true;
    iAccessible = true;
}

/**
 * @brief Called when link is established to target
 *
 * Registers this hostile reference with the target unit.
 */
void HostileReference::targetObjectBuildLink()
{
    getTarget()->AddHatedBy(this);
}

/**
 * @brief Called when target is being destroyed
 *
 * Removes this hostile reference from the target's list.
 */
void HostileReference::targetObjectDestroyLink()
{
    getTarget()->RemoveHatedBy(this);
}

/**
 * @brief Called when source is being destroyed
 *
 * Sets the reference to offline state when the source unit is destroyed.
 */
void HostileReference::sourceObjectDestroyLink()
{
    setOnlineOfflineState(false);
}

/**
 * @brief Fire status changed event
 * @param pThreatRefStatusChangeEvent Event to fire
 *
 * Notifies the source unit that the reference status has changed.
 */
void HostileReference::fireStatusChanged(ThreatRefStatusChangeEvent& pThreatRefStatusChangeEvent)
{
    if (getSource())
    {
        getSource()->processThreatEvent(&pThreatRefStatusChangeEvent);
    }
}

//============================================================

/**
 * @brief Add threat to this reference
 * @param pMod Threat modifier to add
 *
 * Adds threat to this reference and relinks if necessary.
 * Also creates threat to pet owners when pets attack.
 */
void HostileReference::addThreat(float pMod)
{
    iThreat += pMod;
    // the threat is changed. Source and target unit have to be availabe
    // if the link was cut before relink it again
    if (!isOnline())
    {
        updateOnlineStatus();
    }
    if (pMod != 0.0f)
    {
        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_THREAT_CHANGE, this, pMod);
        fireStatusChanged(event);
    }

    if (isValid() && pMod >= 0)
    {
        Unit* victim_owner = getTarget()->GetOwner();
        if (victim_owner && victim_owner->IsAlive())
        {
            getSource()->addThreat(victim_owner, 0.0f);      // create a threat to the owner of a pet, if the pet attacks
        }
    }
}

/**
 * @brief Update online status
 *
 * Checks if the source can reach the target and updates the
 * online/accessible status accordingly.
 */
void HostileReference::updateOnlineStatus()
{
    bool online = false;
    bool accessible = false;

    if (!isValid())
    {
        if (Unit* target = sObjectAccessor.GetUnit(*getSourceUnit(), getUnitGuid()))
        {
            link(target, getSource());
        }
    }
    // only check for online status if
    // ref is valid
    // target is no player or not gamemaster
    // target is not in flight
    if (isValid() &&
        ((getTarget()->GetTypeId() != TYPEID_PLAYER || !((Player*)getTarget())->isGameMaster()) ||
        !getTarget()->IsTaxiFlying()))
    {
        Creature* creature = (Creature*) getSourceUnit();
        online = getTarget()->isInAccessablePlaceFor(creature);
        if (!online)
        {
            if (creature->AI()->canReachByRangeAttack(getTarget()))
            {
                online = true;                               // not accessable but stays online
            }
        }
        else
        {
            accessible = true;
        }
    }
    setAccessibleState(accessible);
    setOnlineOfflineState(online);
}

/**
 * @brief Set online/offline state
 * @param pIsOnline New online state
 *
 * Sets the online state and fires an event if the state changes.
 */
void HostileReference::setOnlineOfflineState(bool pIsOnline)
{
    if (iOnline != pIsOnline)
    {
        iOnline = pIsOnline;
        if (!iOnline)
        {
            setAccessibleState(false);                       // if not online that not accessable as well
        }

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ONLINE_STATUS, this);
        fireStatusChanged(event);
    }
}

//============================================================

/**
 * @brief Set accessible state
 * @param pIsAccessible New accessible state
 *
 * Sets the accessible state and fires an event if the state changes.
 */
void HostileReference::setAccessibleState(bool pIsAccessible)
{
    if (iAccessible != pIsAccessible)
    {
        iAccessible = pIsAccessible;

        ThreatRefStatusChangeEvent event(UEV_THREAT_REF_ASSECCIBLE_STATUS, this);
        fireStatusChanged(event);
    }
}

/**
 * @brief Remove reference
 *
 * Prepares the reference for deletion by invalidating it
 * and firing a removal event.
 */
void HostileReference::removeReference()
{
    invalidate();

    ThreatRefStatusChangeEvent event(UEV_THREAT_REF_REMOVE_FROM_LIST, this);
    fireStatusChanged(event);
}

//============================================================

/**
 * @brief Get source unit
 * @return Source unit pointer
 *
 * Returns the unit that owns this reference's source.
 */
Unit* HostileReference::getSourceUnit()
{
    return (getSource()->getOwner());
}

//============================================================
//================ ThreatContainer ===========================
//============================================================

/**
 * @brief Clear all references
 *
 * Removes and deletes all hostile references from the container.
 */
void ThreatContainer::clearReferences()
{
    for (ThreatList::const_iterator i = iThreatList.begin(); i != iThreatList.end(); ++i)
    {
        (*i)->unlink();
        delete(*i);
    }
    iThreatList.clear();
}

/**
 * @brief Get reference by target unit
 * @param pVictim Target unit to find
 * @return HostileReference or NULL if not found
 *
 * Searches for a hostile reference to the specified unit.
 */
HostileReference* ThreatContainer::getReferenceByTarget(Unit* pVictim)
{
    HostileReference* result = NULL;
    ObjectGuid guid = pVictim->GetObjectGuid();
    for (ThreatList::const_iterator i = iThreatList.begin(); i != iThreatList.end(); ++i)
    {
        if ((*i)->getUnitGuid() == guid)
        {
            result = (*i);
            break;
        }
    }

    return result;
}

/**
 * @brief Add threat to target
 * @param pVictim Target unit
 * @param pThreat Threat to add
 * @return HostileReference or NULL if not found
 *
 * Adds threat to the specified unit if a reference exists.
 */
HostileReference* ThreatContainer::addThreat(Unit* pVictim, float pThreat)
{
    HostileReference* ref = getReferenceByTarget(pVictim);
    if (ref)
    {
        ref->addThreat(pThreat);
    }
    return ref;
}

//============================================================

/**
 * @brief Modify threat by percentage
 * @param pVictim Target unit
 * @param pPercent Percentage to modify (negative to reduce)
 *
 * Modifies the threat for the specified unit by a percentage.
 * If reduction is more than 100%, the reference is removed.
 */
void ThreatContainer::modifyThreatPercent(Unit* pVictim, int32 pPercent)
{
    if (HostileReference* ref = getReferenceByTarget(pVictim))
    {
        if (pPercent < -100)
        {
            ref->removeReference();
            delete ref;
        }
        else
        {
            ref->addThreatPercent(pPercent);
        }
    }
}

//============================================================

/**
 * @brief Hostile reference sort predicate
 * @param lhs First reference
 * @param rhs Second reference
 * @return True if lhs has higher threat
 *
 * Sort predicate for ordering hostile references by threat (descending).
 */
bool HostileReferenceSortPredicate(const HostileReference* lhs, const HostileReference* rhs)
{
    // std::list::sort ordering predicate must be: (Pred(x,y)&&Pred(y,x))==false
    return lhs->getThreat() > rhs->getThreat();             // reverse sorting
}

/**
 * @brief Update threat container
 *
 * Sorts the threat list if it has been modified (dirty flag set).
 */
void ThreatContainer::update()
{
    if (iDirty && iThreatList.size() > 1)
    {
        iThreatList.sort(HostileReferenceSortPredicate);
    }
    iDirty = false;
}

/**
 * @brief Select next victim to attack
 * @param pAttacker Creature selecting victim
 * @param pCurrentVictim Current victim
 * @return Next victim reference or NULL
 *
 * Selects the next victim based on threat values and the
 * 110%/130% threat rules. Handles second choice targets and
 * melee/ranged threat thresholds.
 */
HostileReference* ThreatContainer::selectNextVictim(Creature* pAttacker, HostileReference* pCurrentVictim)
{
    HostileReference* pCurrentRef = NULL;
    bool found = false;
    bool onlySecondChoiceTargetsFound = false;
    bool checkedCurrentVictim = false;

    ThreatList::const_iterator lastRef = iThreatList.end();
    --lastRef;

    for (ThreatList::const_iterator iter = iThreatList.begin(); iter != iThreatList.end() && !found;)
    {
        pCurrentRef = (*iter);

        Unit* pTarget = pCurrentRef->getTarget();
        MANGOS_ASSERT(pTarget);                             // if the ref has status online the target must be there!

        // some units are prefered in comparison to others
        // if (checkThreatArea) consider IsOutOfThreatArea - expected to be only set for pCurrentVictim
        //     This prevents dropping valid targets due to 1.1 or 1.3 threat rule vs invalid current target
        if (!onlySecondChoiceTargetsFound && pAttacker->IsSecondChoiceTarget(pTarget, pCurrentRef == pCurrentVictim))
        {
            if (iter != lastRef)
            {
                ++iter;
            }
            else
            {
                // if we reached to this point, everyone in the threatlist is a second choice target. In such a situation the target with the highest threat should be attacked.
                onlySecondChoiceTargetsFound = true;
                iter = iThreatList.begin();
            }

            // current victim is a second choice target, so don't compare threat with it below
            if (pCurrentRef == pCurrentVictim)
            {
                pCurrentVictim = NULL;
            }

            // second choice targets are only handled threat dependend if we have only have second choice targets
            continue;
        }

        if (!pAttacker->IsOutOfThreatArea(pTarget))         // skip non attackable currently targets
        {
            if (pCurrentVictim)                             // select 1.3/1.1 better target in comparison current target
            {
                // normal case: pCurrentRef is still valid and most hated
                if (pCurrentVictim == pCurrentRef)
                {
                    found = true;
                    break;
                }

                // we found a valid target, but only compare its threat if the currect victim is also a valid target
                // Additional check to prevent unneeded comparision in case of valid current victim
                if (!checkedCurrentVictim)
                {
                    Unit* pCurrentTarget = pCurrentVictim->getTarget();
                    MANGOS_ASSERT(pCurrentTarget);
                    if (pAttacker->IsSecondChoiceTarget(pCurrentTarget, true))
                    {
                        // CurrentVictim is invalid, so return CurrentRef
                        found = true;
                        break;
                    }
                    checkedCurrentVictim = true;
                }

                // list sorted and and we check current target, then this is best case
                if (pCurrentRef->getThreat() <= 1.1f * pCurrentVictim->getThreat())
                {
                    pCurrentRef = pCurrentVictim;
                    found = true;
                    break;
                }

                if (pCurrentRef->getThreat() > 1.3f * pCurrentVictim->getThreat() ||
                    (pCurrentRef->getThreat() > 1.1f * pCurrentVictim->getThreat() && pAttacker->CanReachWithMeleeAttack(pTarget)))
                {
                    // implement 110% threat rule for targets in melee range
                    found = true;                           // and 130% rule for targets in ranged distances
                    break;                                  // for selecting alive targets
                }
            }
            else                                            // select any
            {
                found = true;
                break;
            }
        }
        ++iter;
    }
    if (!found)
    {
        pCurrentRef = NULL;
    }

    return pCurrentRef;
}

//============================================================
//=================== ThreatManager ==========================
//============================================================

/**
 * @brief ThreatManager constructor
 * @param owner Unit that owns this threat manager
 *
 * Initializes the threat manager for the specified unit.
 */
ThreatManager::ThreatManager(Unit* owner)
    : iCurrentVictim(NULL), iOwner(owner)
{
}

//============================================================

/**
 * @brief Clear all threat references
 *
 * Removes all threat references from both online and offline containers.
 */
void ThreatManager::clearReferences()
{
    iThreatContainer.clearReferences();
    iThreatOfflineContainer.clearReferences();
    iCurrentVictim = NULL;
}

//============================================================

/**
 * @brief Add threat from a unit
 * @param pVictim Unit generating threat
 * @param pThreat Base threat value
 * @param crit If true, threat was from a critical hit
 * @param schoolMask Spell school mask
 * @param pThreatSpell Spell that generated threat
 *
 * Adds threat from the specified unit after calculating the final
 * threat value with all modifiers applied.
 */
void ThreatManager::addThreat(Unit* pVictim, float pThreat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* pThreatSpell)
{
    // function deals with adding threat and adding players and pets into ThreatList
    // mobs, NPCs, guards have ThreatList and HateOfflineList
    // players and pets have only InHateListOf
    // HateOfflineList is used co contain unattackable victims (in-flight, in-water, GM etc.)

    // not to self
    if (pVictim == getOwner())
    {
        return;
    }

    // not to GM
    if (!pVictim || (pVictim->GetTypeId() == TYPEID_PLAYER && ((Player*)pVictim)->isGameMaster()))
    {
        return;
    }

    // not to dead and not for dead
    if (!pVictim->IsAlive() || !getOwner()->IsAlive())
    {
        return;
    }

    MANGOS_ASSERT(getOwner()->GetTypeId() == TYPEID_UNIT);

    float threat = ThreatCalcHelper::CalcThreat(pVictim, iOwner, pThreat, crit, schoolMask, pThreatSpell);

#if !defined(CLASSIC)
    if (threat > 0.0f)
    {
        if (Unit* redirectedTarget = pVictim->GetHostileRefManager().GetThreatRedirectionTarget())
        {
            if (redirectedTarget != getOwner() && redirectedTarget->IsAlive())
            {
                addThreatDirectly(redirectedTarget, threat);
                threat = 0;                                 // but still need add to threat list
            }
        }
    }
#endif
    addThreatDirectly(pVictim, threat);
}

/**
 * @brief Add threat directly without calculation
 * @param pVictim Unit generating threat
 * @param threat Threat value to add
 *
 * Adds threat directly to the specified unit, creating a new
 * reference if one doesn't exist.
 */
void ThreatManager::addThreatDirectly(Unit* pVictim, float threat)
{
    HostileReference* ref = iThreatContainer.addThreat(pVictim, threat);
    // Ref is not in the online refs, search the offline refs next
    if (!ref)
    {
        ref = iThreatOfflineContainer.addThreat(pVictim, threat);
    }

    if (!ref)                                               // there was no ref => create a new one
    {
        // threat has to be 0 here
        HostileReference* hostileReference = new HostileReference(pVictim, this, 0);
        iThreatContainer.addReference(hostileReference);
        hostileReference->addThreat(threat);                // now we add the real threat
        if (pVictim->GetTypeId() == TYPEID_PLAYER && ((Player*)pVictim)->isGameMaster())
        {
            hostileReference->setOnlineOfflineState(false);  // GM is always offline
        }
    }
}

//============================================================

/**
 * @brief Modify threat by percentage
 * @param pVictim Target unit
 * @param pPercent Percentage to modify
 *
 * Modifies the threat for the specified unit by a percentage.
 */
void ThreatManager::modifyThreatPercent(Unit* pVictim, int32 pPercent)
{
    iThreatContainer.modifyThreatPercent(pVictim, pPercent);
}

//============================================================

/**
 * @brief Get current hostile target
 * @return Target unit or NULL
 *
 * Updates the threat container and returns the current victim
 * based on threat values.
 */
Unit* ThreatManager::getHostileTarget()
{
    iThreatContainer.update();
    HostileReference* nextVictim = iThreatContainer.selectNextVictim((Creature*) getOwner(), getCurrentVictim());
    setCurrentVictim(nextVictim);
    return getCurrentVictim() != NULL ? getCurrentVictim()->getTarget() : NULL;
}

//============================================================

/**
 * @brief Get threat value for unit
 * @param pVictim Target unit
 * @param pAlsoSearchOfflineList If true, also search offline list
 * @return Threat value
 *
 * Returns the threat value for the specified unit.
 */
float ThreatManager::getThreat(Unit* pVictim, bool pAlsoSearchOfflineList)
{
    float threat = 0.0f;
    HostileReference* ref = iThreatContainer.getReferenceByTarget(pVictim);
    if (!ref && pAlsoSearchOfflineList)
    {
        ref = iThreatOfflineContainer.getReferenceByTarget(pVictim);
    }
    if (ref)
    {
        threat = ref->getThreat();
    }
    return threat;
}

//============================================================

/**
 * @brief Apply taunt effect
 * @param pTaunter Unit using taunt
 *
 * Sets the taunter's temporary threat to match the current victim's
 * threat if it's higher, forcing the creature to attack the taunter.
 */
void ThreatManager::tauntApply(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        if (getCurrentVictim() && (ref->getThreat() < getCurrentVictim()->getThreat()))
        {
            // Ok, temp threat is unused
            if (ref->getTempThreatModifyer() == 0.0f)
            {
                ref->setTempThreat(getCurrentVictim()->getThreat());
            }
        }
    }
}

//============================================================

/**
 * @brief Remove taunt effect
 * @param pTaunter Unit whose taunt is fading
 *
 * Resets the temporary threat modifier for the taunter.
 */
void ThreatManager::tauntFadeOut(Unit* pTaunter)
{
    if (HostileReference* ref = iThreatContainer.getReferenceByTarget(pTaunter))
    {
        ref->resetTempThreat();
    }
}

//============================================================

/**
 * @brief Set current victim
 * @param pHostileReference New victim reference
 *
 * Sets the current victim for threat management.
 */
void ThreatManager::setCurrentVictim(HostileReference* pHostileReference)
{
    iCurrentVictim = pHostileReference;
}

/**
 * @brief Process threat reference status change event
 * @param threatRefStatusChangeEvent Event to process
 *
 * Handles threat reference status changes including threat modifications,
 * online/offline status changes, and reference removal.
 */
void ThreatManager::processThreatEvent(ThreatRefStatusChangeEvent* threatRefStatusChangeEvent)
{
    threatRefStatusChangeEvent->setThreatManager(this);     // now we can set the threat manager

    HostileReference* hostileReference = threatRefStatusChangeEvent->getReference();

    switch (threatRefStatusChangeEvent->getType())
    {
        case UEV_THREAT_REF_THREAT_CHANGE:
            if ((getCurrentVictim() == hostileReference && threatRefStatusChangeEvent->getFValue() < 0.0f) ||
                (getCurrentVictim() != hostileReference && threatRefStatusChangeEvent->getFValue() > 0.0f))
            {
                setDirty(true);                              // the order in the threat list might have changed
            }
            break;
        case UEV_THREAT_REF_ONLINE_STATUS:
            if (!hostileReference->isOnline())
            {
                if (hostileReference == getCurrentVictim())
                {
                    setCurrentVictim(NULL);
                    setDirty(true);
                }
                iThreatContainer.remove(hostileReference);
                iThreatOfflineContainer.addReference(hostileReference);
            }
            else
            {
                if (getCurrentVictim() && hostileReference->getThreat() > (1.1f * getCurrentVictim()->getThreat()))
                {
                    setDirty(true);
                }
                iThreatContainer.addReference(hostileReference);
                iThreatOfflineContainer.remove(hostileReference);
            }
            break;
        case UEV_THREAT_REF_REMOVE_FROM_LIST:
            if (hostileReference == getCurrentVictim())
            {
                setCurrentVictim(NULL);
                setDirty(true);
            }
            if (hostileReference->isOnline())
            {
                iThreatContainer.remove(hostileReference);
            }
            else
            {
                iThreatOfflineContainer.remove(hostileReference);
            }
            break;
    }
}
