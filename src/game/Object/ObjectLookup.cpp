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

#include "ObjectLookup.h"

#include "Creature.h"
#include "GameObject.h"
#include "Map.h"
#include "Object.h"
#include "Player.h"
#include "PlayerRegistry.h"

namespace ObjectLookup
{
    Unit* GetUnit(WorldObject const& obj, ObjectGuid guid)
    {
        if (!guid)
        {
            return nullptr;
        }

        // A player is reachable wherever they are, so the global index answers
        // before the reference object's map is even consulted.
        if (guid.IsPlayer())
        {
            return sPlayerRegistry.Find(guid);
        }

        if (!obj.IsInWorld())
        {
            return nullptr;
        }

        return obj.GetMap()->GetAnyTypeCreature(guid);
    }

    Creature* GetCreature(WorldObject const& obj, ObjectGuid guid)
    {
        if (!guid || !obj.IsInWorld())
        {
            return nullptr;
        }

        return obj.GetMap()->GetCreature(guid);
    }

    GameObject* GetGameObject(WorldObject const& obj, ObjectGuid guid)
    {
        if (!guid || !obj.IsInWorld())
        {
            return nullptr;
        }

        return obj.GetMap()->GetGameObject(guid);
    }
}
