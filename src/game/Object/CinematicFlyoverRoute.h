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

#ifndef MANGOSSERVER_CINEMATIC_FLYOVER_ROUTE_H
#define MANGOSSERVER_CINEMATIC_FLYOVER_ROUTE_H

#include "Common.h"

struct CinematicFlyoverKeyframe
{
    uint32 timestampMs;  // Time from route start in milliseconds
    float x, y, z;       // World position
    float orientation;   // Facing in radians
};

struct CinematicFlyoverRoute
{
    uint32 raceId;          // ChrRaces race ID
    uint32 cinematicId;     // CinematicCamera.dbc ID
    uint32 mapId;           // Map ID
    uint32 durationMs;      // Total route duration in milliseconds
    uint32 keyframeCount;
    const CinematicFlyoverKeyframe* keyframes;
};

/// Route accessor function - returns nullptr if no route exists for the race
const CinematicFlyoverRoute* GetCinematicFlyoverRouteForRace(uint8 raceId);

#endif // MANGOSSERVER_CINEMATIC_FLYOVER_ROUTE_H
