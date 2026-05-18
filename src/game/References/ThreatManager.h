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

#ifndef _THREATMANAGER
#define _THREATMANAGER

#include "Common.h"
#include "SharedDefines.h"
#include "Utilities/LinkedReference/Reference.h"
#include "UnitEvents.h"
#include "ObjectGuid.h"
#include <list>

//==============================================================

class Unit;
class Creature;
class ThreatManager;
struct SpellEntry;

//==============================================================
/**
 * @brief Threat calculation helper class
 *
 * Class to calculate the real threat based on various factors.
 */
class ThreatCalcHelper
{
    public:
        /**
         * @brief Calculate threat
         * @param pHatedUnit Unit being hated
         * @param pHatingUnit Unit doing the hating
         * @param threat Base threat value
         * @param crit Whether the attack was a critical hit
         * @param schoolMask Spell school mask
         * @param threatSpell Spell causing threat
         * @return Calculated threat value
         */
        static float CalcThreat(Unit* pHatedUnit, Unit* pHatingUnit, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell);
};

//==============================================================
/**
 * @brief Hostile reference class
 *
 * Manages a hostile reference between a Unit and a ThreatManager.
 */
class HostileReference : public Reference<Unit, ThreatManager>
{
    public:
        /**
         * @brief Constructor
         * @param pUnit Unit reference
         * @param pThreatManager Threat manager
         * @param pThreat Initial threat value
         */
        HostileReference(Unit* pUnit, ThreatManager* pThreatManager, float pThreat);

        /**
         * @brief Add threat
         * @param pMod Threat modifier
         */
        void addThreat(float pMod);

        /**
         * @brief Set threat
         * @param pThreat Threat value to set
         */
        void setThreat(float pThreat) { addThreat(pThreat - getThreat()); }

        /**
         * @brief Add threat percentage
         * @param pPercent Percentage to add
         */
        void addThreatPercent(int32 pPercent)
        {
            // For special -100 case avoid rounding
            addThreat(pPercent == -100 ? -iThreat : iThreat * pPercent / 100.0f);
        }

        /**
         * @brief Get threat
         * @return Current threat value
         */
        float getThreat() const { return iThreat; }

        /**
         * @brief Check if online
         * @return True if online
         */
        bool isOnline() const { return iOnline; }

        /**
         * @brief Check if accessible
         *
         * The Unit might be in water and the creature cannot enter the water,
         * but has range attack. In this case online = true, but accessible = false.
         *
         * @return True if accessible
         */
        bool isAccessable() const { return iAccessible; }

        /**
         * @brief Set temporary threat
         *
         * Used for temporarily setting a threat and reducing it later again.
         * The threat modification is stored.
         *
         * @param pThreat Temporary threat value
         */
        void setTempThreat(float pThreat) { iTempThreatModifyer = pThreat - getThreat(); if (iTempThreatModifyer != 0.0f) { addThreat(iTempThreatModifyer); } }

        /**
         * @brief Reset temporary threat
         */
        void resetTempThreat()
        {
            if (iTempThreatModifyer != 0.0f)
            {
                addThreat(-iTempThreatModifyer); iTempThreatModifyer = 0.0f;
            }
        }

        /**
         * @brief Get temporary threat modifier
         * @return Temporary threat modifier
         */
        float getTempThreatModifyer() { return iTempThreatModifyer; }

        /**
         * @brief Update online status
         *
         * Check if source can reach target and set the status.
         */
        void updateOnlineStatus();

        /**
         * @brief Set online/offline state
         * @param pIsOnline Online state
         */
        void setOnlineOfflineState(bool pIsOnline);

        /**
         * @brief Set accessible state
         * @param pIsAccessible Accessible state
         */
        void setAccessibleState(bool pIsAccessible);

        /**
         * @brief Equality operator
         * @param pHostileReference Reference to compare
         * @return True if equal
         */
        bool operator ==(const HostileReference& pHostileReference) const { return pHostileReference.getUnitGuid() == getUnitGuid(); }

        /**
         * @brief Get unit GUID
         * @return Unit GUID
         */
        ObjectGuid const& getUnitGuid() const { return iUnitGuid; }

        /**
         * @brief Get source unit
         * @return Source unit
         */
        Unit* getSourceUnit();

        /**
         * @brief Remove reference
         *
         * Reference is not needed anymore, really delete it.
         */
        void removeReference();

        /**
         * @brief Get next reference
         * @return Next hostile reference
         */
        HostileReference* next() { return ((HostileReference*) Reference<Unit, ThreatManager>::next()); }

        /**
         * @brief Build link to target object
         *
         * Tell our refTo (target) object that we have a link.
         */
        void targetObjectBuildLink() override;

        /**
         * @brief Destroy link to target object
         *
         * Tell our refTo (target) object that the link is cut.
         */
        void targetObjectDestroyLink() override;

        /**
         * @brief Destroy link from source object
         *
         * Tell our refFrom (source) object that the link is cut (Target destroyed).
         */
        void sourceObjectDestroyLink() override;

    private:
        /**
         * @brief Fire status changed event
         *
         * Inform the source that the status of that reference was changed.
         *
         * @param pThreatRefStatusChangeEvent Status change event
         */
        void fireStatusChanged(ThreatRefStatusChangeEvent& pThreatRefStatusChangeEvent);

    private:
        float iThreat; ///< Current threat
        float iTempThreatModifyer; ///< Temporary threat modifier (used for taunt)
        ObjectGuid iUnitGuid; ///< Unit GUID
        bool iOnline; ///< Online status
        bool iAccessible; ///< Accessible status
};

//==============================================================
class ThreatManager;

typedef std::list<HostileReference*> ThreatList;

/**
 * @brief Threat container class
 *
 * Manages a list of hostile references and provides threat-related operations.
 */
class ThreatContainer
{
    private:
        ThreatList iThreatList; ///< Threat list
        bool iDirty; ///< Dirty flag (needs sorting)

    protected:
        friend class ThreatManager;

        /**
         * @brief Remove reference
         * @param pRef Reference to remove
         */
        void remove(HostileReference* pRef) { iThreatList.remove(pRef); }

        /**
         * @brief Add reference
         * @param pHostileReference Reference to add
         */
        void addReference(HostileReference* pHostileReference) { iThreatList.push_back(pHostileReference); }

        /**
         * @brief Clear all references
         */
        void clearReferences();

        /**
         * @brief Update container
         *
         * Sort the list if necessary.
         */
        void update();

    public:
        /**
         * @brief Constructor
         */
        ThreatContainer() { iDirty = false; }

        /**
         * @brief Destructor
         */
        ~ThreatContainer() { clearReferences(); }

        /**
         * @brief Add threat
         * @param pVictim Victim unit
         * @param pThreat Threat amount
         * @return Hostile reference
         */
        HostileReference* addThreat(Unit* pVictim, float pThreat);

        /**
         * @brief Modify threat percentage
         * @param pVictim Victim unit
         * @param percent Percentage to modify
         */
        void modifyThreatPercent(Unit* pVictim, int32 percent);

        /**
         * @brief Select next victim
         * @param pAttacker Attacking creature
         * @param pCurrentVictim Current victim
         * @return Next victim reference
         */
        HostileReference* selectNextVictim(Creature* pAttacker, HostileReference* pCurrentVictim);

        /**
         * @brief Set dirty flag
         * @param pDirty Dirty state
         */
        void setDirty(bool pDirty) { iDirty = pDirty; }

        /**
         * @brief Check if dirty
         * @return True if dirty
         */
        bool isDirty() const { return iDirty; }

        /**
         * @brief Check if empty
         * @return True if empty
         */
        bool empty() const { return(iThreatList.empty()); }

        /**
         * @brief Get most hated reference
         * @return Most hated reference
         */
        HostileReference* getMostHated() { return iThreatList.empty() ? NULL : iThreatList.front(); }

        /**
         * @brief Get reference by target
         * @param pVictim Victim unit
         * @return Hostile reference
         */
        HostileReference* getReferenceByTarget(Unit* pVictim);

        /**
         * @brief Get threat list
         * @return Threat list
         */
        ThreatList const& getThreatList() const { return iThreatList; }
};

//=================================================
/**
 * @brief Threat manager class
 *
 * Manages threat for a unit, including threat calculation and target selection.
 */
class ThreatManager
{
    public:
        friend class HostileReference;

        /**
         * @brief Constructor
         * @param pOwner Owner unit
         */
        explicit ThreatManager(Unit* pOwner);

        /**
         * @brief Destructor
         */
        ~ThreatManager() { clearReferences(); }

        /**
         * @brief Clear all references
         */
        void clearReferences();

        /**
         * @brief Add threat
         * @param pVictim Victim unit
         * @param threat Threat amount
         * @param crit Whether the attack was a critical hit
         * @param schoolMask Spell school mask
         * @param threatSpell Spell causing threat
         */
        void addThreat(Unit* pVictim, float threat, bool crit, SpellSchoolMask schoolMask, SpellEntry const* threatSpell);

        /**
         * @brief Add threat (simplified)
         * @param pVictim Victim unit
         * @param threat Threat amount
         */
        void addThreat(Unit* pVictim, float threat) { addThreat(pVictim, threat, false, SPELL_SCHOOL_MASK_NONE, NULL); }

        /**
         * @brief Add threat directly
         *
         * Add threat as raw value (ignore redirections and expect all mods applied already to it).
         *
         * @param pVictim Victim unit
         * @param threat Threat amount
         */
        void addThreatDirectly(Unit* pVictim, float threat);

        /**
         * @brief Modify threat percentage
         * @param pVictim Victim unit
         * @param pPercent Percentage to modify
         */
        void modifyThreatPercent(Unit* pVictim, int32 pPercent);

        /**
         * @brief Get threat
         * @param pVictim Victim unit
         * @param pAlsoSearchOfflineList Also search offline list
         * @return Threat value
         */
        float getThreat(Unit* pVictim, bool pAlsoSearchOfflineList = false);

        /**
         * @brief Check if threat list is empty
         * @return True if empty
         */
        bool isThreatListEmpty() const { return iThreatContainer.empty(); }

        /**
         * @brief Process threat event
         * @param threatRefStatusChangeEvent Threat reference status change event
         */
        void processThreatEvent(ThreatRefStatusChangeEvent* threatRefStatusChangeEvent);

        /**
         * @brief Get current victim
         * @return Current victim reference
         */
        HostileReference* getCurrentVictim() { return iCurrentVictim; }

        /**
         * @brief Get owner unit
         * @return Owner unit
         */
        Unit* getOwner() const
        {
            return iOwner;
        }

        /**
         * @brief Get hostile target
         * @return Hostile target unit
         */
        Unit* getHostileTarget();

        /**
         * @brief Apply taunt
         * @param pTaunter Taunting unit
         */
        void tauntApply(Unit* pTaunter);

        /**
         * @brief Fade out taunt
         * @param pTaunter Taunting unit
         */
        void tauntFadeOut(Unit* pTaunter);

        /**
         * @brief Set current victim
         * @param pHostileReference Hostile reference to set as current victim
         */
        void setCurrentVictim(HostileReference* pHostileReference);

        /**
         * @brief Set dirty flag
         * @param pDirty Dirty state
         */
        void setDirty(bool pDirty) { iThreatContainer.setDirty(pDirty); }

        /**
         * @brief Get threat list
         *
         * Don't use for explicit modify threat values in iterator return pointers.
         *
         * @return Threat list
         */
        ThreatList const& getThreatList() const { return iThreatContainer.getThreatList(); }

    private:
        HostileReference* iCurrentVictim; ///< Current victim
        Unit* iOwner; ///< Owner unit
        ThreatContainer iThreatContainer; ///< Threat container
        ThreatContainer iThreatOfflineContainer; ///< Offline threat container
};

//=================================================
#endif
