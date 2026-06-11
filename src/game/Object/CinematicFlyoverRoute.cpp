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

// GENERATED FILE - do not hand-edit. Regenerate with:
//     python .ai_tools/bake_cinematic_routes.py --out <this file>
//
// World-space camera routes for the eight starting-race intro cinematics.
// Derived offline from client data: CinematicCamera.dbc provides a world
// origin and facing per cinematic; the referenced camera-only M2 model
// provides the flyby as keyframed position/target tracks relative to the
// camera base vector. world = origin + RotZ(facing) * (base + key value).
// Orientation per keyframe faces the interpolated target track position.
// Only derived coordinates are stored here; no client assets are shipped.
// Each route's final keyframe lands at that race's player spawn point
// (verified against playercreateinfo, 5.6-12.2 yd, camera ending on the
// newly created character).

// Human (race 1, cinematic 142, map 0)
static const CinematicFlyoverKeyframe s_humanKeyframes[] = {
    { 0, -8922.42f, 543.34f, 144.58f, 0.7977f },
    { 3533, -8941.30f, 529.32f, 125.54f, 0.7752f },
    { 7300, -8953.98f, 513.38f, 116.89f, 0.8840f },
    { 10200, -8965.95f, 507.06f, 111.10f, 0.8158f },
    { 17333, -8990.03f, 488.07f, 104.71f, 0.7667f },
    { 24033, -9018.13f, 468.53f, 104.25f, 0.7247f },
    { 29067, -9052.48f, 442.88f, 105.50f, 0.7221f },
    { 33067, -9097.85f, 412.25f, 112.38f, 0.6513f },
    { 36767, -9142.12f, 365.84f, 113.04f, 0.7278f },
    { 39600, -9148.07f, 303.22f, 108.50f, 1.3275f },
    { 43533, -9148.65f, 276.84f, 103.41f, 3.0569f },
    { 50433, -9214.34f, 200.05f, 77.81f, 4.5261f },
    { 54133, -9210.40f, 120.50f, 86.67f, 5.4820f },
    { 58400, -9127.28f, 44.61f, 89.23f, 5.4862f },
    { 61100, -9062.21f, 9.39f, 119.98f, 5.4434f },
    { 64000, -9029.00f, -37.70f, 119.79f, 5.4475f },
    { 70100, -9012.78f, -80.13f, 93.79f, 5.7676f },
    { 75567, -9017.59f, -112.65f, 91.61f, 6.1206f },
    { 79967, -9001.97f, -142.92f, 91.18f, 0.0921f },
    { 87467, -8956.36f, -132.94f, 84.80f, 6.2808f },
};

static const CinematicFlyoverRoute s_humanRoute = {
    1,     // raceId (Human)
    142,   // cinematicId
    0,     // mapId (Eastern Kingdoms)
    87467, // durationMs
    sizeof(s_humanKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_humanKeyframes
};

// Orc (race 2, cinematic 235, map 1)
static const CinematicFlyoverKeyframe s_orcKeyframes[] = {
    { 0, 369.84f, -4705.95f, 76.67f, 2.6119f },
    { 1600, 373.16f, -4708.63f, 70.90f, 2.5974f },
    { 3400, 378.57f, -4712.84f, 58.99f, 2.6198f },
    { 6567, 381.45f, -4714.62f, 45.46f, 2.8160f },
    { 9833, 384.15f, -4717.90f, 26.67f, 2.9839f },
    { 13000, 363.23f, -4722.84f, 17.41f, 4.1009f },
    { 15733, 323.57f, -4742.88f, 33.60f, 3.8427f },
    { 19033, 229.49f, -4765.07f, 45.82f, 3.7114f },
    { 21033, 135.46f, -4764.45f, 36.57f, 3.9419f },
    { 24000, 44.56f, -4756.36f, 31.67f, 3.9671f },
    { 27567, -38.87f, -4741.07f, 33.46f, 3.8122f },
    { 31933, -198.15f, -4713.90f, 52.55f, 3.1440f },
    { 37633, -425.56f, -4687.64f, 75.69f, 2.9397f },
    { 42700, -569.95f, -4696.60f, 75.17f, 1.7464f },
    { 46433, -597.56f, -4617.07f, 72.05f, 1.5875f },
    { 50567, -586.16f, -4531.13f, 65.73f, 1.6111f },
    { 56700, -589.86f, -4345.27f, 52.91f, 2.3445f },
    { 58800, -612.26f, -4309.83f, 50.93f, 1.9667f },
    { 61833, -633.80f, -4286.64f, 48.93f, 1.1573f },
    { 65933, -633.80f, -4261.25f, 42.95f, 0.3849f },
    { 68833, -624.65f, -4251.78f, 40.34f, 0.0513f },
    { 70167, -624.65f, -4251.78f, 40.34f, 0.0513f },
};

static const CinematicFlyoverRoute s_orcRoute = {
    2,     // raceId (Orc)
    235,   // cinematicId
    1,     // mapId (Kalimdor)
    70167, // durationMs
    sizeof(s_orcKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_orcKeyframes
};

// Dwarf (race 3, cinematic 234, map 0)
static const CinematicFlyoverKeyframe s_dwarfKeyframes[] = {
    { 0, -5041.88f, -824.64f, 541.27f, 5.7394f },
    { 800, -5043.23f, -823.65f, 541.25f, 5.7390f },
    { 1533, -5044.98f, -822.21f, 541.13f, 5.7321f },
    { 4300, -5054.95f, -811.14f, 538.55f, 5.6794f },
    { 7900, -5065.12f, -792.77f, 529.51f, 6.0822f },
    { 9900, -5072.87f, -767.79f, 513.70f, 5.9606f },
    { 11800, -5077.14f, -743.65f, 505.35f, 5.4452f },
    { 13467, -5083.64f, -714.06f, 501.03f, 4.9922f },
    { 15767, -5101.49f, -672.80f, 495.80f, 3.9885f },
    { 17533, -5125.16f, -639.94f, 493.15f, 3.1822f },
    { 19167, -5162.07f, -597.95f, 487.92f, 2.6637f },
    { 23800, -5318.70f, -485.39f, 475.43f, 3.2049f },
    { 27200, -5521.58f, -423.47f, 474.28f, 3.4079f },
    { 30333, -5681.49f, -424.95f, 474.90f, 3.1404f },
    { 33767, -5814.69f, -407.16f, 513.43f, 2.7082f },
    { 35367, -5868.52f, -358.63f, 534.68f, 2.6946f },
    { 37633, -5900.69f, -247.71f, 529.26f, 2.6679f },
    { 40567, -5945.46f, -83.15f, 492.76f, 2.3964f },
    { 42833, -6032.51f, 42.09f, 498.74f, 2.6732f },
    { 44167, -6099.08f, 100.78f, 503.74f, 2.6615f },
    { 45633, -6182.01f, 163.14f, 503.46f, 2.4462f },
    { 47033, -6250.13f, 220.72f, 482.83f, 1.8580f },
    { 47900, -6282.28f, 251.90f, 467.12f, 1.3071f },
    { 49733, -6316.47f, 300.18f, 439.28f, 0.4613f },
    { 51900, -6315.71f, 336.33f, 412.24f, 6.2206f },
    { 54667, -6279.36f, 347.81f, 391.95f, 5.8281f },
    { 57867, -6252.41f, 337.92f, 384.56f, 5.7974f },
    { 59600, -6246.93f, 333.76f, 384.19f, 5.8478f },
};

static const CinematicFlyoverRoute s_dwarfRoute = {
    3,     // raceId (Dwarf)
    234,   // cinematicId
    0,     // mapId (Eastern Kingdoms)
    59600, // durationMs
    sizeof(s_dwarfKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_dwarfKeyframes
};

// Night Elf (race 4, cinematic 122, map 1)
static const CinematicFlyoverKeyframe s_nightelfKeyframes[] = {
    { 333, 9693.22f, 1041.14f, 1393.94f, 4.2989f },
    { 10367, 9691.12f, 1019.42f, 1350.81f, 5.3342f },
    { 17300, 9716.12f, 982.35f, 1318.08f, 5.9063f },
    { 19533, 9753.58f, 959.94f, 1319.51f, 6.1623f },
    { 21600, 9782.41f, 953.20f, 1320.45f, 0.1629f },
    { 24033, 9811.59f, 957.13f, 1318.86f, 0.2258f },
    { 25633, 9841.09f, 963.53f, 1321.16f, 5.7182f },
    { 29667, 9874.11f, 878.63f, 1328.70f, 4.8474f },
    { 31467, 9879.71f, 807.91f, 1329.75f, 5.1920f },
    { 35467, 9940.37f, 712.05f, 1346.75f, 6.1709f },
    { 39967, 10014.92f, 684.91f, 1366.94f, 0.0707f },
    { 47000, 10129.80f, 696.27f, 1376.56f, 0.0180f },
    { 49933, 10171.91f, 695.51f, 1373.12f, 0.0452f },
    { 51533, 10204.61f, 696.87f, 1371.61f, 0.1603f },
    { 57400, 10290.99f, 680.87f, 1377.97f, 0.5911f },
    { 65733, 10447.70f, 745.10f, 1413.47f, 0.7906f },
    { 66600, 10474.13f, 746.54f, 1412.25f, 1.2065f },
    { 67467, 10500.47f, 749.50f, 1410.34f, 1.8142f },
    { 68400, 10521.71f, 758.30f, 1408.37f, 2.1291f },
    { 69267, 10536.11f, 769.94f, 1405.73f, 2.2685f },
    { 71367, 10545.04f, 809.58f, 1393.81f, 2.5289f },
    { 75000, 10519.78f, 856.94f, 1376.42f, 3.0315f },
    { 77700, 10470.49f, 864.40f, 1366.97f, 3.2788f },
    { 81933, 10411.79f, 857.43f, 1360.77f, 3.4541f },
    { 85200, 10384.59f, 859.39f, 1356.52f, 3.7031f },
    { 90267, 10329.66f, 857.95f, 1343.87f, 4.1234f },
    { 91567, 10320.73f, 853.79f, 1340.49f, 4.3748f },
    { 93000, 10313.06f, 849.07f, 1337.10f, 4.6489f },
    { 94033, 10309.04f, 846.16f, 1335.05f, 4.8525f },
    { 95100, 10306.58f, 843.96f, 1333.48f, 5.0684f },
    { 97200, 10305.52f, 840.74f, 1331.45f, 5.3677f },
    { 102333, 10306.38f, 835.08f, 1328.14f, 5.6178f },
};

static const CinematicFlyoverRoute s_nightelfRoute = {
    4,     // raceId (Night Elf)
    122,   // cinematicId
    1,     // mapId (Kalimdor)
    102333, // durationMs
    sizeof(s_nightelfKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_nightelfKeyframes
};

// Undead (race 5, cinematic 2, map 0)
static const CinematicFlyoverKeyframe s_undeadKeyframes[] = {
    { 0, 2163.21f, 1524.32f, 78.97f, 3.1378f },
    { 4333, 2154.60f, 1524.34f, 79.16f, 3.3887f },
    { 16467, 2091.54f, 1520.06f, 73.78f, 2.9674f },
    { 26700, 2041.42f, 1525.22f, 79.27f, 2.7145f },
    { 40500, 1978.07f, 1542.62f, 94.21f, 2.7002f },
    { 46733, 1951.31f, 1553.02f, 99.55f, 2.7786f },
    { 46767, 1951.17f, 1553.08f, 99.57f, 2.7791f },
    { 56700, 1911.06f, 1571.75f, 102.60f, 2.6928f },
    { 56733, 1910.86f, 1571.84f, 102.62f, 2.6924f },
    { 70100, 1787.81f, 1622.27f, 116.46f, 2.7482f },
    { 74233, 1753.71f, 1637.17f, 126.52f, 2.9382f },
    { 80100, 1718.94f, 1653.86f, 137.80f, 3.0263f },
    { 86733, 1677.05f, 1663.10f, 143.88f, 3.1516f },
    { 88733, 1658.69f, 1663.10f, 143.74f, 3.1044f },
    { 90000, 1645.39f, 1662.98f, 135.27f, 2.3564f },
    { 90633, 1642.45f, 1663.94f, 134.55f, 1.5306f },
    { 90900, 1642.01f, 1664.57f, 134.26f, 1.3927f },
    { 92967, 1642.17f, 1676.90f, 129.66f, 6.2629f },
    { 94167, 1643.77f, 1678.40f, 129.36f, 6.0749f },
    { 97733, 1655.55f, 1677.86f, 123.66f, 0.0419f },
    { 99700, 1660.48f, 1677.74f, 123.38f, 0.0198f },
    { 102033, 1664.48f, 1677.93f, 123.34f, 0.0185f },
};

static const CinematicFlyoverRoute s_undeadRoute = {
    5,     // raceId (Undead)
    2,     // cinematicId
    0,     // mapId (Eastern Kingdoms)
    102033, // durationMs
    sizeof(s_undeadKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_undeadKeyframes
};

// Tauren (race 6, cinematic 202, map 1)
static const CinematicFlyoverKeyframe s_taurenKeyframes[] = {
    { 0, -1175.48f, 116.46f, 251.14f, 4.4463f },
    { 4533, -1175.61f, 124.15f, 226.10f, 4.4463f },
    { 7867, -1175.93f, 142.58f, 194.69f, 4.2287f },
    { 11633, -1224.73f, 152.22f, 186.29f, 3.7218f },
    { 13733, -1304.54f, 112.88f, 150.89f, 3.5927f },
    { 15433, -1458.90f, 43.94f, 81.49f, 3.5349f },
    { 16900, -1596.22f, -23.12f, 53.28f, 3.5913f },
    { 22033, -1861.18f, -224.36f, 31.34f, 3.4821f },
    { 32067, -2238.87f, -349.49f, 50.35f, 3.2686f },
    { 38767, -2537.91f, -462.27f, 191.74f, 2.6810f },
    { 43433, -2744.34f, -398.82f, 103.71f, 2.4239f },
    { 44933, -2804.51f, -368.68f, 97.52f, 2.3613f },
    { 48433, -2894.65f, -320.07f, 85.26f, 1.7269f },
    { 51133, -2938.43f, -314.45f, 85.79f, 1.0692f },
    { 60200, -2936.63f, -261.27f, 63.80f, 0.2326f },
    { 63667, -2931.07f, -259.26f, 56.61f, 0.0969f },
    { 66633, -2927.40f, -258.45f, 55.41f, 0.0441f },
    { 69867, -2924.08f, -258.70f, 55.12f, 0.0853f },
    { 70333, -2924.08f, -258.70f, 55.12f, 0.0853f },
};

static const CinematicFlyoverRoute s_taurenRoute = {
    6,     // raceId (Tauren)
    202,   // cinematicId
    1,     // mapId (Kalimdor)
    70333, // durationMs
    sizeof(s_taurenKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_taurenKeyframes
};

// Gnome (race 7, cinematic 162, map 0)
static const CinematicFlyoverKeyframe s_gnomeKeyframes[] = {
    { 0, -5080.12f, 445.40f, 414.29f, 5.7097f },
    { 4233, -5087.95f, 450.21f, 414.11f, 5.6720f },
    { 5500, -5091.62f, 450.50f, 414.29f, 5.7786f },
    { 8667, -5103.97f, 439.57f, 415.48f, 0.1732f },
    { 10567, -5115.03f, 434.65f, 416.51f, 1.0770f },
    { 13033, -5134.21f, 431.66f, 422.13f, 1.6108f },
    { 15200, -5155.08f, 435.48f, 423.11f, 1.7125f },
    { 18367, -5184.83f, 458.28f, 420.35f, 1.7802f },
    { 22300, -5213.33f, 493.78f, 414.49f, 2.3603f },
    { 26633, -5294.24f, 541.26f, 409.62f, 2.6892f },
    { 33233, -5382.49f, 529.37f, 397.75f, 3.0529f },
    { 36400, -5464.75f, 482.47f, 396.09f, 2.8346f },
    { 37633, -5500.51f, 474.26f, 396.18f, 2.8284f },
    { 39733, -5554.60f, 455.24f, 402.80f, 2.7215f },
    { 40867, -5598.08f, 448.10f, 414.02f, 3.1007f },
    { 43733, -5683.91f, 407.83f, 451.58f, 3.2692f },
    { 48000, -5776.93f, 353.68f, 502.19f, 2.8915f },
    { 51367, -5877.27f, 426.92f, 544.34f, 2.6707f },
    { 53067, -5900.80f, 462.29f, 576.36f, 3.1244f },
    { 54467, -5959.48f, 528.08f, 585.57f, 3.8494f },
    { 58100, -6121.56f, 532.14f, 555.88f, 4.4539f },
    { 61767, -6236.54f, 447.61f, 466.25f, 4.9579f },
    { 66667, -6268.55f, 377.40f, 400.98f, 5.2366f },
    { 69633, -6262.66f, 348.82f, 387.52f, 5.5570f },
    { 72633, -6253.13f, 336.61f, 384.10f, 5.8110f },
    { 74000, -6249.58f, 334.20f, 383.82f, 5.8713f },
    { 76667, -6246.32f, 332.67f, 384.04f, 5.9216f },
};

static const CinematicFlyoverRoute s_gnomeRoute = {
    7,     // raceId (Gnome)
    162,   // cinematicId
    0,     // mapId (Eastern Kingdoms)
    76667, // durationMs
    sizeof(s_gnomeKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_gnomeKeyframes
};

// Troll (race 8, cinematic 182, map 1)
static const CinematicFlyoverKeyframe s_trollKeyframes[] = {
    { 0, -791.36f, -4945.88f, 31.06f, 3.6349f },
    { 9867, -780.53f, -4941.30f, 30.85f, 3.4950f },
    { 15567, -778.03f, -4940.81f, 30.80f, 2.2935f },
    { 17300, -779.57f, -4936.37f, 26.99f, 1.6300f },
    { 18867, -780.75f, -4926.56f, 26.89f, 1.7407f },
    { 20633, -782.58f, -4910.28f, 26.99f, 1.2977f },
    { 24233, -756.56f, -4835.33f, 30.08f, 1.3490f },
    { 26133, -739.23f, -4746.16f, 39.90f, 1.2295f },
    { 28700, -695.58f, -4676.44f, 49.32f, 0.8377f },
    { 29900, -663.93f, -4652.10f, 62.15f, 0.7796f },
    { 31367, -617.14f, -4630.65f, 84.44f, 1.1770f },
    { 32133, -605.36f, -4611.29f, 83.80f, 1.3046f },
    { 33467, -593.84f, -4565.59f, 72.10f, 1.3951f },
    { 34900, -580.49f, -4517.03f, 54.68f, 1.6357f },
    { 37633, -583.26f, -4453.29f, 52.08f, 1.8775f },
    { 40900, -628.65f, -4366.43f, 50.12f, 1.2729f },
    { 43167, -623.86f, -4330.44f, 55.86f, 1.3071f },
    { 45400, -621.25f, -4306.21f, 64.19f, 1.3772f },
    { 48433, -631.23f, -4282.52f, 62.70f, 1.0080f },
    { 51867, -635.92f, -4263.28f, 52.26f, 0.4592f },
    { 57767, -625.33f, -4251.64f, 40.44f, 0.0406f },
};

static const CinematicFlyoverRoute s_trollRoute = {
    8,     // raceId (Troll)
    182,   // cinematicId
    1,     // mapId (Kalimdor)
    57767, // durationMs
    sizeof(s_trollKeyframes) / sizeof(CinematicFlyoverKeyframe),
    s_trollKeyframes
};

/// Returns the baked intro flyover route for a race, or nullptr if none exists.
const CinematicFlyoverRoute* GetCinematicFlyoverRouteForRace(uint8 raceId)
{
    switch (raceId)
    {
        case RACE_HUMAN:
        {
            return &s_humanRoute;
        }
        case RACE_ORC:
        {
            return &s_orcRoute;
        }
        case RACE_DWARF:
        {
            return &s_dwarfRoute;
        }
        case RACE_NIGHTELF:
        {
            return &s_nightelfRoute;
        }
        case RACE_UNDEAD:
        {
            return &s_undeadRoute;
        }
        case RACE_TAUREN:
        {
            return &s_taurenRoute;
        }
        case RACE_GNOME:
        {
            return &s_gnomeRoute;
        }
        case RACE_TROLL:
        {
            return &s_trollRoute;
        }
        default:
        {
            return nullptr;
        }
    }
}
