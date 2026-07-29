/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

/**
 * @file HeightCheck.cpp
 * @brief DOES THE BAKED FLOOR AGREE WITH WHERE THE WORLD DATABASE PUTS ITS CREATURES?
 *
 * A clean parse is not correct geometry, and a baker that produces tiles proves nothing
 * about what is in them. This is the test that does: the world database says where a
 * creature stands, and a creature whose `creature_template.InhabitType` is ground-only
 * cannot stand anywhere but on the floor. So the baked floor under its spawn must agree
 * with its `position_z`.
 *
 *   SELECT CONCAT('c', c.guid, '_', c.id), c.map, c.position_x, c.position_y,
 *          c.position_z, c.position_z
 *   FROM   creature c JOIN creature_template t ON t.entry = c.id
 *   WHERE  t.InhabitType & 1 AND (t.InhabitType & 6) = 0;
 *
 * feeds this tool directly: one probe per line, `name,map,x,y,z,expectedFloor`.
 *
 * It reports a DISTRIBUTION rather than a verdict, because a single pass/fail threshold
 * hides the two failures that matter and look nothing alike: a handful of spawns metres
 * out (authored drift, or a model the baker placed wrong) and a whole map with no floor
 * at all (a format the reader did not understand). The second is invisible in a pass rate.
 *
 * `--verbose` restores the other use this tool has always had, from mangos_one's version
 * of it: one line per probe with the floor, the liquid column and the tile it landed in,
 * for feeding the coordinates out of a "creature falls through the floor" report. Useless
 * over thirty thousand spawns, which is why it is not the default.
 */

#include "terrain/FusedTerrain.hpp"
#include "terrain/GoModelStore.hpp"
#include "terrain/Column.hpp"
#include "terrain/Terrain.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    /// How far above a spawn to start looking, and how far below to give up. A creature
    /// authored slightly inside the floor still has to find it, and one authored on a
    /// bridge must not fall through to the canyon under it.
    constexpr float SEARCH_UP = 5.0f;
    constexpr float SEARCH_DOWN = 60.0f;

    const char* LiquidName(world::terrain::LiquidKind k)
    {
        switch (k)
        {
            case world::terrain::LiquidKind::None:  return "none";
            case world::terrain::LiquidKind::Water: return "water";
            case world::terrain::LiquidKind::Ocean: return "ocean";
            case world::terrain::LiquidKind::Magma: return "magma";
            case world::terrain::LiquidKind::Slime: return "slime";
        }
        return "?";
    }

    struct Probe
    {
        std::string name;
        uint32_t map = 0;
        float x = 0.0f, y = 0.0f, z = 0.0f, expected = 0.0f;
    };

    struct MapScore
    {
        uint32_t probes = 0;
        uint32_t noFloor = 0;
        uint32_t within[4] = {0, 0, 0, 0};   ///< 0.5, 2, 5, 20 yards
        uint32_t beyond = 0;
        double sumAbs = 0.0;
        float worst = 0.0f;
        std::string worstName;
    };

    bool ReadProbes(const std::string& path, std::vector<Probe>& out)
    {
        std::ifstream in(path);
        if (!in)
        {
            std::fprintf(stderr, "cannot open probe file: %s\n", path.c_str());
            return false;
        }

        std::string line;
        while (std::getline(in, line))
        {
            if (line.empty() || line[0] == '#')
            {
                continue;
            }

            std::istringstream fields(line);
            std::string cell;
            Probe p;

            if (!std::getline(fields, p.name, ',')) { continue; }
            if (!std::getline(fields, cell, ',')) { continue; }
            p.map = static_cast<uint32_t>(std::strtoul(cell.c_str(), nullptr, 10));
            if (!std::getline(fields, cell, ',')) { continue; }
            p.x = std::strtof(cell.c_str(), nullptr);
            if (!std::getline(fields, cell, ',')) { continue; }
            p.y = std::strtof(cell.c_str(), nullptr);
            if (!std::getline(fields, cell, ',')) { continue; }
            p.z = std::strtof(cell.c_str(), nullptr);
            p.expected = std::getline(fields, cell, ',')
                             ? std::strtof(cell.c_str(), nullptr)
                             : p.z;

            out.push_back(p);
        }

        return true;
    }
}

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf("usage: mangos-height-check <tileDir> <probes.csv> [gomodelDir] [--verbose]\n"
                    "\n"
                    "  probes.csv: one per line, name,map,x,y,z,expectedFloor\n"
                    "  gomodelDir: baked gomodels, so WMO and M2 floors resolve too\n"
                    "              (default: <tileDir>/../gomodels)\n"
                    "  --verbose : one line per probe -- floor, liquid, tile. For chasing\n"
                    "              a single report, not for scoring a bake.\n");
        return 2;
    }

    const std::string tileDir = argv[1];
    const std::string probePath = argv[2];

    bool verbose = false;
    std::string goDir = tileDir + "/../gomodels";
    for (int i = 3; i < argc; ++i)
    {
        if (std::string(argv[i]) == "--verbose")
        {
            verbose = true;
        }
        else
        {
            goDir = argv[i];
        }
    }

    world::terrain::FusedTerrain::SetTileDir(tileDir);
    world::terrain::GoModelStore::Instance().SetDirectory(goDir);

    std::vector<Probe> probes;
    if (!ReadProbes(probePath, probes))
    {
        return 2;
    }

    std::printf("%zu probes, tiles from %s\n\n", probes.size(), tileDir.c_str());

    std::map<uint32_t, std::unique_ptr<world::terrain::FusedTerrain>> engines;
    std::map<uint32_t, MapScore> scores;
    MapScore total;

    for (const Probe& p : probes)
    {
        auto& engine = engines[p.map];
        if (!engine)
        {
            engine.reset(new world::terrain::FusedTerrain(p.map));
        }

        MapScore& s = scores[p.map];
        ++s.probes;
        ++total.probes;

        const world::terrain::Column column =
            engine->ColumnAt(p.x, p.y, p.z + SEARCH_UP, p.z - SEARCH_DOWN);
        const auto floor = column.HighestSolidAtOrBelow(p.z + SEARCH_UP);

        if (!floor)
        {
            ++s.noFloor;
            ++total.noFloor;

            if (verbose)
            {
                std::printf("  [NO FLOOR] %-28s map=%-4u tile=(%d,%d)\n", p.name.c_str(),
                            p.map, world::terrain::TileIndex(p.x),
                            world::terrain::TileIndex(p.y));
            }
            continue;
        }

        const float delta = std::fabs(*floor - p.expected);

        if (verbose)
        {
            const auto liquid = column.HighestLiquid();
            char liq[80] = "none";
            if (liquid)
            {
                std::snprintf(liq, sizeof liq, "%s@%.2f (depth %+.2f)",
                              LiquidName(liquid->liquid), liquid->z, liquid->z - *floor);
            }

            std::printf("  [%s] %-28s map=%-4u tile=(%d,%d)  floor=%.3f exp=%.3f d=%.3f  "
                        "liquid=%s\n",
                        delta <= 2.0f ? "PASS" : "FAIL", p.name.c_str(), p.map,
                        world::terrain::TileIndex(p.x), world::terrain::TileIndex(p.y),
                        *floor, p.expected, delta, liq);
        }

        s.sumAbs += delta;
        total.sumAbs += delta;

        if (delta > s.worst)
        {
            s.worst = delta;
            s.worstName = p.name;
        }
        if (delta > total.worst)
        {
            total.worst = delta;
            total.worstName = p.name;
        }

        const float bands[4] = {0.5f, 2.0f, 5.0f, 20.0f};
        int band = 4;
        for (int i = 0; i < 4; ++i)
        {
            if (delta <= bands[i])
            {
                band = i;
                break;
            }
        }

        if (band == 4)
        {
            ++s.beyond;
            ++total.beyond;
        }
        else
        {
            ++s.within[band];
            ++total.within[band];
        }
    }

    std::printf("%-6s %8s %8s %8s %8s %8s %8s %8s %9s\n",
                "map", "probes", "<=0.5", "<=2", "<=5", "<=20", ">20", "NO FLOOR", "mean|d|");

    for (const auto& entry : scores)
    {
        const MapScore& s = entry.second;
        const uint32_t scored = s.probes - s.noFloor;
        std::printf("%-6u %8u %8u %8u %8u %8u %8u %8u %9.2f\n",
                    entry.first, s.probes, s.within[0], s.within[1], s.within[2],
                    s.within[3], s.beyond, s.noFloor,
                    scored ? s.sumAbs / scored : 0.0);
    }

    const uint32_t scored = total.probes - total.noFloor;
    std::printf("\n%-6s %8u %8u %8u %8u %8u %8u %8u %9.2f\n",
                "ALL", total.probes, total.within[0], total.within[1], total.within[2],
                total.within[3], total.beyond, total.noFloor,
                scored ? total.sumAbs / scored : 0.0);

    if (scored)
    {
        const double within2 =
            100.0 * double(total.within[0] + total.within[1]) / double(scored);
        std::printf("\nwithin 2 yards of the authored Z: %.2f%% of the %u that found a floor\n",
                    within2, scored);
    }

    if (total.noFloor)
    {
        std::printf("NO FLOOR for %u of %u probes (%.2f%%) -- a map with a high share here "
                    "is a map the reader did not understand\n",
                    total.noFloor, total.probes,
                    100.0 * double(total.noFloor) / double(total.probes));
    }

    if (!total.worstName.empty())
    {
        std::printf("worst single probe: %s off by %.2f yards\n",
                    total.worstName.c_str(), total.worst);
    }

    return 0;
}
