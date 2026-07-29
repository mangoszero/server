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

#ifndef MANGOS_H_OBJECTLOOKUP_H
#define MANGOS_H_OBJECTLOOKUP_H

#include "ObjectGuid.h"

class Creature;
class GameObject;
class Unit;
class WorldObject;

/**
 * @brief Resolve a GUID relative to some object's map.
 *
 * These are free functions because they hold no state -- they never did. They
 * lived on a singleton purely because they were filed under "getting an object",
 * next to the player index and the corpse store, and inherited the singleton by
 * association. GetUnit alone accounts for 35 of the old class's call sites while
 * touching none of its members.
 *
 * All of them need a reference object only to reach its Map; when the caller
 * already has the Map, calling Map's own methods directly is cheaper and clearer.
 */
namespace ObjectLookup
{
    /**
     * @brief Find a unit (player or creature) by GUID.
     *
     * Players are resolved through the global registry, since a player is
     * reachable regardless of which map the reference object is on. Everything
     * else is looked up on the reference object's map.
     *
     * @param obj  Reference object, used for its map.
     * @param guid GUID to resolve.
     * @return The unit, or nullptr.
     */
    Unit* GetUnit(WorldObject const& obj, ObjectGuid guid);

    /// Find a creature on the reference object's map.
    Creature* GetCreature(WorldObject const& obj, ObjectGuid guid);

    /// Find a game object on the reference object's map.
    GameObject* GetGameObject(WorldObject const& obj, ObjectGuid guid);
}

#endif
