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

#ifndef _HOSTILEREFMANAGER
#define _HOSTILEREFMANAGER

#include "Common.h"
#include "Utilities/LinkedReference/RefManager.h"

class Unit;
class ThreatManager;
class HostileReference;
struct SpellEntry;

//=================================================

/**
 * @brief Hostile reference manager class
 *
 * Manages hostile references for threat management.
 */
class HostileRefManager : public RefManager<Unit, ThreatManager>
{
    public:
        /**
         * @brief Constructor
         * @param pOwner Owner unit
         */
        explicit HostileRefManager(Unit* pOwner);

        /**
         * @brief Destructor
         */
        ~HostileRefManager();

        /**
         * @brief Get owner unit
         * @return Owner unit
         */
        Unit* getOwner() { return iOwner; }

        /**
         * @brief Send threat to all haters for the victim
         *
         * The victim is hated by them as well.
         * Used for buffs and healing threat functionality.
         *
         * @param pVictim Victim unit
         * @param threat Threat amount
         * @param threatSpell Spell causing threat (optional)
         * @param pSingleTarget Single target only
         */
        void threatAssist(Unit* pVictim, float threat, SpellEntry const* threatSpell = 0, bool pSingleTarget = false);

        /**
         * @brief Add threat percentage
         * @param pValue Percentage value to add
         */
        void addThreatPercent(int32 pValue);

        /**
         * @brief Delete all references
         *
         * The references are not needed anymore.
         * Tell the source to remove them from the list and free the memory.
         */
        void deleteReferences();

        /**
         * @brief Remove specific faction references
         * @param faction Faction ID
         */
        void deleteReferencesForFaction(uint32 faction);

        /**
         * @brief Get first hostile reference
         * @return First hostile reference
         */
        HostileReference* getFirst() { return ((HostileReference*) RefManager<Unit, ThreatManager>::getFirst()); }

        /**
         * @brief Update threat tables
         */
        void updateThreatTables();

        /**
         * @brief Set online/offline state for all references
         * @param pIsOnline Online state
         */
        void setOnlineOfflineState(bool pIsOnline);

        /**
         * @brief Set online/offline state for one reference
         * @param pCreature Unit reference
         * @param pIsOnline Online state
         */
        void setOnlineOfflineState(Unit* pCreature, bool pIsOnline);

        /**
         * @brief Delete one reference
         * @param pCreature Unit reference
         */
        void deleteReference(Unit* pCreature);

    private:
        Unit* iOwner; ///< Owner of manager variable (back reference, always exists)
};
//=================================================
#endif
