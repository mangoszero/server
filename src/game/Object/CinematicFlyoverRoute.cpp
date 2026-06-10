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

#include "CinematicFlyoverRoute.h"

// NOTE: This is placeholder route data for MVP compilation/testing.
// The full keyframe data should be generated from offline M2 parsing tooling
// (.ai_tools/m2_camera.py) using the CMaNGOS TranslateLocation formula:
// local = position_base + spline_value, then polar rotation.
// Origins below are from CinematicCamera.dbc (verified in audit).

// Human (race 1, cinematic 142, map 0)
static const CinematicFlyoverKeyframe s_humanKeyframes[] = {
    { 0, -8945.52f, -118.79f, 82.93f, 0.1920f },
    { 2000, -8940.00f, -115.00f, 83.00f, 0.1950f },
    { 4000, -8935.00f, -110.00f, 83.50f, 0.2000f },
    { 6000, -8930.00f, -105.00f, 84.00f, 0.2050f },
    { 87500, -8925.00f, -100.00f, 84.50f, 0.2100f }, // Placeholder end
};

static const CinematicFlyoverRoute s_humanRoute = {
    1,      // raceId (Human)
    142,    // cinematicId
    0,      // mapId (Eastern Kingdoms)
    87500,  // durationMs (placeholder ~87.5s)
    sizeof(s_humanKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_humanKeyframes
};

// Orc (race 2, cinematic 235, map 1)
static const CinematicFlyoverKeyframe s_orcKeyframes[] = {
    { 0, -603.41f, -4193.41f, 41.09f, 4.6775f },
    { 2000, -600.00f, -4190.00f, 41.50f, 4.6800f },
    { 4000, -595.00f, -4185.00f, 42.00f, 4.6850f },
    { 70200, -590.00f, -4180.00f, 42.50f, 4.6900f }, // Placeholder end
};

static const CinematicFlyoverRoute s_orcRoute = {
    2,      // raceId (Orc)
    235,    // cinematicId
    1,      // mapId (Kalimdor)
    70200,  // durationMs (placeholder ~70.2s)
    sizeof(s_orcKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_orcKeyframes
};

// Dwarf (race 3, cinematic 234, map 0)
static const CinematicFlyoverKeyframe s_dwarfKeyframes[] = {
    { 0, -5579.16f, -455.78f, 406.48f, 4.7124f },
    { 2000, -5575.00f, -450.00f, 407.00f, 4.7150f },
    { 4000, -5570.00f, -445.00f, 407.50f, 4.7200f },
    { 59600, -5565.00f, -440.00f, 408.00f, 4.7250f }, // Placeholder end
};

static const CinematicFlyoverRoute s_dwarfRoute = {
    3,      // raceId (Dwarf)
    234,    // cinematicId
    0,      // mapId (Eastern Kingdoms)
    59600,  // durationMs (placeholder ~59.6s)
    sizeof(s_dwarfKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_dwarfKeyframes
};

// Night Elf (race 4, cinematic 122, map 1)
static const CinematicFlyoverKeyframe s_nightelfKeyframes[] = {
    { 0, 10474.00f, 813.13f, 1318.66f, 3.7350f },
    { 2000, 10478.00f, 815.00f, 1319.00f, 3.7400f },
    { 4000, 10482.00f, 817.00f, 1319.50f, 3.7450f },
    { 102300, 10486.00f, 819.00f, 1320.00f, 3.7500f }, // Placeholder end
};

static const CinematicFlyoverRoute s_nightelfRoute = {
    4,      // raceId (Night Elf)
    122,    // cinematicId
    1,      // mapId (Kalimdor)
    102300, // durationMs (placeholder ~102.3s)
    sizeof(s_nightelfKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_nightelfKeyframes
};

// Undead (race 5, cinematic 2, map 0)
static const CinematicFlyoverKeyframe s_undeadKeyframes[] = {
    { 0, 1658.58f, 1662.91f, 141.23f, 3.1416f },
    { 2000, 1662.00f, 1665.00f, 141.50f, 3.1450f },
    { 4000, 1666.00f, 1668.00f, 142.00f, 3.1500f },
    { 102000, 1670.00f, 1671.00f, 142.50f, 3.1550f }, // Placeholder end
};

static const CinematicFlyoverRoute s_undeadRoute = {
    5,      // raceId (Undead)
    2,      // cinematicId
    0,      // mapId (Eastern Kingdoms)
    102000, // durationMs (placeholder ~102s)
    sizeof(s_undeadKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_undeadKeyframes
};

// Tauren (race 6, cinematic 202, map 1)
static const CinematicFlyoverKeyframe s_taurenKeyframes[] = {
    { 0, -2873.26f, -266.82f, 53.92f, 3.1590f },
    { 2000, -2870.00f, -263.00f, 54.50f, 3.1620f },
    { 4000, -2865.00f, -260.00f, 55.00f, 3.1650f },
    { 70300, -2860.00f, -257.00f, 55.50f, 3.1680f }, // Placeholder end
};

static const CinematicFlyoverRoute s_taurenRoute = {
    6,      // raceId (Tauren)
    202,    // cinematicId
    1,      // mapId (Kalimdor)
    70300,  // durationMs (placeholder ~70.3s)
    sizeof(s_taurenKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_taurenKeyframes
};

// Gnome (race 7, cinematic 162, map 0)
static const CinematicFlyoverKeyframe s_gnomeKeyframes[] = {
    { 0, -5784.35f, 424.74f, 426.59f, 0.9250f },
    { 2000, -5780.00f, 428.00f, 427.00f, 0.9300f },
    { 4000, -5775.00f, 432.00f, 427.50f, 0.9350f },
    { 76700, -5770.00f, 436.00f, 428.00f, 0.9400f }, // Placeholder end
};

static const CinematicFlyoverRoute s_gnomeRoute = {
    7,      // raceId (Gnome)
    162,    // cinematicId
    0,      // mapId (Eastern Kingdoms)
    76700,  // durationMs (placeholder ~76.7s)
    sizeof(s_gnomeKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_gnomeKeyframes
};

// Troll (race 8, cinematic 182, map 1)
static const CinematicFlyoverKeyframe s_trollKeyframes[] = {
    { 0, -774.70f, -4911.21f, 19.61f, 2.9496f },
    { 2000, -770.00f, -4908.00f, 20.00f, 2.9520f },
    { 4000, -765.00f, -4905.00f, 20.50f, 2.9550f },
    { 57800, -760.00f, -4902.00f, 21.00f, 2.9580f }, // Placeholder end
};

static const CinematicFlyoverRoute s_trollRoute = {
    8,      // raceId (Troll)
    182,    // cinematicId
    1,      // mapId (Kalimdor)
    57800,  // durationMs (placeholder ~57.8s)
    sizeof(s_trollKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_trollKeyframes
};

const CinematicFlyoverRoute* GetCinematicFlyoverRouteForRace(uint8 raceId)
{
    switch (raceId)
    {
        case RACE_HUMAN:
            return &s_humanRoute;
        case RACE_ORC:
            return &s_orcRoute;
        case RACE_DWARF:
            return &s_dwarfRoute;
        case RACE_NIGHTELF:
            return &s_nightelfRoute;
        case RACE_UNDEAD:
            return &s_undeadRoute;
        case RACE_TAUREN:
            return &s_taurenRoute;
        case RACE_GNOME:
            return &s_gnomeRoute;
        case RACE_TROLL:
            return &s_trollRoute;
        default:
            return nullptr;
    }
}
