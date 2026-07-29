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

#ifndef MANGOS_H_ELUNAOBJECTACCESSORSHIM_H
#define MANGOS_H_ELUNAOBJECTACCESSORSHIM_H

// ===========================================================================
//                             TEMPORARY
//
// This file exists for exactly one reason: the Eluna submodule still reaches
// for the old ObjectAccessor through its own `eObjectAccessor()` macro, which
// expands to `sObjectAccessor.` and therefore needs an object of that name to
// exist. Eluna is shared upstream between several cores (AzerothCore, CMangos,
// TrinityCore, VMangos, MaNGOS) and absorbs their differences here, so it has
// not followed this fork's split of ObjectAccessor into PlayerRegistry,
// CorpseManager and ObjectLookup.
//
// Nothing in the core may include this header. It is included by Eluna alone.
//
// Named ElunaObjectAccessorShim rather than ElunaCompat on purpose: Eluna ships
// its own ElunaCompat.h, and a quoted include from ElunaIncludes.h searches that
// file's own directory first -- so the shorter name silently resolved to Eluna's
// header and this one was never included at all.
//
// DELETE THIS FILE, and the include of it in ElunaIncludes.h, as soon as Eluna
// is updated to call sPlayerRegistry / ObjectLookup directly. There is nothing
// here worth keeping -- it is a name-compatibility layer and no more.
// ===========================================================================

#include <utility>
#include "ObjectGuid.h"
#include "ObjectLookup.h"
#include "PlayerRegistry.h"
#include "Policies/Singleton.h"

class Player;
class Unit;
class WorldObject;

/**
 * @brief Old ObjectAccessor method names, forwarding to their real homes.
 *
 * Only the five methods the MaNGOS branch of Eluna actually calls are here.
 * FindPlayerByLowGUID, which also appears in Eluna's sources, is inside a
 * TrinityCore-only branch and is deliberately absent.
 */
class ElunaObjectAccessorCompat
{
    public:

        Player* FindPlayer(ObjectGuid guid, bool inWorld = true) const
        {
            return sPlayerRegistry.Find(guid, inWorld);
        }

        Player* FindPlayerByName(const char* name) const
        {
            return sPlayerRegistry.FindByName(name);
        }

        void SaveAllPlayers() const
        {
            sPlayerRegistry.SaveAll();
        }

        template <typename F>
        void DoForAllPlayers(F&& work) const
        {
            sPlayerRegistry.ForEach(std::forward<F>(work));
        }

        Unit* GetUnit(WorldObject const& obj, ObjectGuid guid) const
        {
            return ObjectLookup::GetUnit(obj, guid);
        }
};

/// Satisfies Eluna's `#define eObjectAccessor() sObjectAccessor.`
#define sObjectAccessor MaNGOS::Singleton<ElunaObjectAccessorCompat>::Instance()

#endif
