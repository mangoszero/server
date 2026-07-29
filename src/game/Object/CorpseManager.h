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

#ifndef MANGOS_H_CORPSEMANAGER_H
#define MANGOS_H_CORPSEMANAGER_H

#include "GridDefines.h"
#include "ObjectGuid.h"
#include "Policies/Singleton.h"
#include "Utilities/ConcurrentRegistry.h"

class Corpse;
class Map;

/**
 * @brief Owns player corpses: the indexes, and their whole lifecycle.
 *
 * This was never "access" -- it creates corpses, converts them to bones, expires
 * the old ones and keeps the object-manager's per-cell data in step. Bundling it
 * with the player lookup made a class where the name told you nothing about what
 * any given method did.
 *
 * Two distinct indexes live here, which the old code held side by side without
 * ever making the difference visible:
 *
 *   m_corpses     keyed by the CORPSE's own GUID -- what FindCorpse looks in.
 *   m_byOwner     keyed by the OWNING PLAYER's GUID -- what AddCorpse writes to,
 *                 and the one that carries the cell bookkeeping.
 *
 * They have different keys, different populations and different lifetimes.
 */
class CorpseManager : public MaNGOS::Singleton<CorpseManager>
{
        friend class MaNGOS::Singleton<CorpseManager>;

    public:

        // --- lookup ---------------------------------------------------------

        /// Find a corpse by its own GUID.
        Corpse* Find(ObjectGuid corpseGuid) const;

        /// Find a corpse by its own GUID, but only on the given map.
        Corpse* FindInMap(ObjectGuid corpseGuid, uint32 mapId) const;

        /// The active (non-bones) corpse belonging to a player, if any.
        Corpse* FindForPlayer(ObjectGuid playerGuid) const;

        // --- registry -------------------------------------------------------

        /// Register a corpse under its own GUID.
        void AddObject(Corpse* corpse);
        void RemoveObject(Corpse* corpse);

        // --- lifecycle ------------------------------------------------------

        /// Track a freshly created player corpse, and record its cell.
        void Add(Corpse* corpse);

        /// Stop tracking a player corpse and drop its cell record.
        void Remove(Corpse* corpse);

        /// Put every corpse belonging to a grid into that grid's object store.
        void AddCorpsesToGrid(GridPair const& gridpair, GridType& grid, Map* map);

        /// Turn a player's corpse into bones, optionally leaving insignia.
        Corpse* ConvertCorpseForPlayer(ObjectGuid playerGuid, bool insignia = false);

        /// Expire bones that have outlived their delay.
        void RemoveOldCorpses();

    private:

        CorpseManager() = default;
        ~CorpseManager();

        /// Cell bookkeeping that must accompany owner-index changes.
        void RecordCell(Corpse* corpse);
        void ForgetCell(Corpse* corpse);

        MaNGOS::ConcurrentRegistry<ObjectGuid, Corpse> m_corpses;
        MaNGOS::ConcurrentRegistry<ObjectGuid, Corpse> m_byOwner;
};

#define sCorpseManager MaNGOS::Singleton<CorpseManager>::Instance()

#endif
