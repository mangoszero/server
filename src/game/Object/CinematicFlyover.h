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
 */

#ifndef MANGOSSERVER_CINEMATIC_FLYOVER_H
#define MANGOSSERVER_CINEMATIC_FLYOVER_H

#include "CinematicFlyoverRoute.h"
#include "ObjectGuid.h"
#include <memory>

class Player;
class Creature;
class Map;

class CinematicFlyover
{
public:
    CinematicFlyover(Player* player, uint8 raceId);
    ~CinematicFlyover();

    /**
     * Begin the flyover (summon body + bind camera). Called when the client
     * enters the cinematic on the first CMSG_NEXT_CINEMATIC_CAMERA. Guarded:
     * a no-op unless the flyover was armed at login and has not already begun.
     */
    void Begin();

    /// Update tick - called from Player::Update
    void Update(uint32 updateDiff);

    /// Stop the flyover and clean up (idempotent)
    void Stop();

    /// Check if flyover is active
    bool IsActive() const { return m_active; }

private:
    /// Resolve body from GUID (returns nullptr if body no longer exists)
    Creature* ResolveBody() const;

    /// Interpolate route position at the given route time (ms)
    bool InterpolatePosition(uint32 atMs, float& x, float& y, float& z, float& o);

    Player* m_player;
    const CinematicFlyoverRoute* m_route;
    Map* m_viewerMap;       // map the broadcast-radius viewer was registered on
    float m_viewerRadius;   // radius it was registered with (for paired removal)
    ObjectGuid m_bodyGuid;
    uint32 m_bodyEntry;
    uint32 m_elapsedMs;
    uint32 m_updateTimer;
    uint32 m_timeoutMs;
    bool m_armed;       // validated at login; waiting for the client to enter the cinematic
    bool m_begun;       // Begin() has run (guards repeated CMSG_NEXT_CINEMATIC_CAMERA)
    bool m_active;      // body summoned and camera bound
};

#endif // MANGOSSERVER_CINEMATIC_FLYOVER_H
