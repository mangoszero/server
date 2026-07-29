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

#ifndef MANGOS_H_PLAYERREGISTRY_H
#define MANGOS_H_PLAYERREGISTRY_H

#include <utility>
#include "ObjectGuid.h"
#include "Policies/Singleton.h"
#include "Utilities/ConcurrentRegistry.h"

class Player;

/**
 * @brief The server-wide index of logged-in players.
 *
 * This is the one part of the old ObjectAccessor that genuinely was a global
 * registry. Everything else it carried -- corpse ownership and lifecycle, and a
 * handful of stateless per-map lookups -- has moved to CorpseManager and
 * ObjectLookup respectively, because none of it shared any state with this.
 *
 * The index is held by composition rather than inheritance, which is what closes
 * the hazard in the old HashMapHolder/Player2Corpse pair: with no virtual
 * functions anywhere, the derived Insert/Remove only hid the base ones.
 */
class PlayerRegistry : public MaNGOS::Singleton<PlayerRegistry>
{
        friend class MaNGOS::Singleton<PlayerRegistry>;

    public:

        /**
         * @brief Find a logged-in player by GUID.
         *
         * @param guid    Player GUID.
         * @param inWorld When true (the default) only players actually in the
         *                world are returned; a player still loading is skipped.
         */
        Player* Find(ObjectGuid guid, bool inWorld = true) const;

        /// Find by exact name. Linear; callers are rare (commands, mail, trade).
        Player* FindByName(const char* name) const;

        /// Disconnect a player by GUID, if online.
        void Kick(ObjectGuid guid) const;

        /// Persist every online player. Used by the periodic save and shutdown.
        void SaveAll() const;

        void Add(Player* player);
        void Remove(Player* player);

        /// Run work(Player*) over everyone online, under the shared lock.
        template <typename F>
        void ForEach(F&& work) const
        {
            m_players.ForEach(std::forward<F>(work));
        }

        size_t Count() const { return m_players.Size(); }

    private:

        PlayerRegistry() = default;
        ~PlayerRegistry() = default;

        MaNGOS::ConcurrentRegistry<ObjectGuid, Player> m_players;
};

#define sPlayerRegistry MaNGOS::Singleton<PlayerRegistry>::Instance()

#endif
