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

#include "PlayerRegistry.h"

#include "Player.h"
#include "World.h"
#include "WorldSession.h"

#include <cstring>


Player* PlayerRegistry::Find(ObjectGuid guid, bool inWorld /* = true */) const
{
    if (!guid)
    {
        return nullptr;
    }

    // A hashed lookup, which is what the container was always for. The version
    // this replaces called FindWith() and walked the entire map comparing every
    // key to `guid` -- a linear scan of every online player, on the single
    // hottest lookup in the server, purely so the inWorld filter could be folded
    // into the predicate. Fetch first, then filter.
    Player* player = m_players.Find(guid);
    if (!player)
    {
        return nullptr;
    }

    return (player->IsInWorld() || !inWorld) ? player : nullptr;
}

Player* PlayerRegistry::FindByName(const char* name) const
{
    if (!name)
    {
        return nullptr;
    }

    // Genuinely linear: names are not indexed. Callers are commands, mail and
    // trade, none of them hot.
    return m_players.FindWith([name](const ObjectGuid&, Player* player) -> bool
    {
        return player->IsInWorld() && std::strcmp(name, player->GetName()) == 0;
    });
}

void PlayerRegistry::Kick(ObjectGuid guid) const
{
    // inWorld = false: a player still loading is exactly the sort we may need to
    // kick, and would be invisible to the filtered lookup.
    if (Player* player = Find(guid, false))
    {
        WorldSession* session = player->GetSession();
        session->KickPlayer();          // mark for removal at the next session update
        session->LogoutPlayer(false);   // and log out now rather than waiting for it
    }
}

void PlayerRegistry::SaveAll() const
{
    // Driven from the session list rather than this index: a session can exist
    // without its player being registered here yet, and those still need saving.
    for (const auto& iter : sWorld.GetAllSessions())
    {
        if (Player* player = iter.second->GetPlayer())
        {
            if (player->IsInWorld())
            {
                player->SaveToDB();
            }
        }
    }
}

void PlayerRegistry::Add(Player* player)
{
    m_players.Insert(player->GetObjectGuid(), player);
}

void PlayerRegistry::Remove(Player* player)
{
    m_players.Remove(player->GetObjectGuid());
}
