#include <string>
#include <vector>
#include "nav/NavMeshBuilder.hpp"

#include "terrain/TileSerializer.hpp"
#include "terrain/WmoModel.hpp"

// The on-disk header and the NAV_* area bits are the SERVER's, included rather than
// copied. A struct that is written here and read there must have exactly one
// declaration, or the two drift and the drift is silent.
#include "MoveMapSharedDefines.h"

#include "DetourNavMesh.h"
#include "DetourNavMeshBuilder.h"
#include "Recast.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

namespace world::nav
{
    namespace
    {
        using world::terrain::TerrainTile;
        using Vec3 = Geometry::Vector3;

        constexpr float GRID_SIZE = 533.33333f;
        constexpr int V9_SIDE = 129;
        constexpr int V8_SIDE = 128;
        constexpr float GRID_PART = GRID_SIZE / float(V8_SIDE);

        std::mutex g_bakeLogMutex;

        // Triangle soup in RECAST space. Vertices go in as world (x, y, z) and come out
        // as (y, z, x) -- the same permutation the server's PathFinder applies.
        struct Soup
        {
            std::vector<float> verts;
            std::vector<int> tris;
            std::vector<unsigned char> areas;

            int AddVertex(const Vec3& w)
            {
                const int index = int(verts.size() / 3);
                verts.push_back(w.y);
                verts.push_back(w.z);
                verts.push_back(w.x);
                return index;
            }

            void AddTriangle(int a, int b, int c, unsigned char area)
            {
                tris.push_back(a);
                tris.push_back(b);
                tris.push_back(c);
                areas.push_back(area);
            }

            int VertexCount() const { return int(verts.size() / 3); }
            int TriangleCount() const { return int(tris.size() / 3); }
            bool Empty() const { return tris.empty(); }
        };

        // World X of a V9 column on grid gx. Both axes FALL as the index grows.
        inline float WorldX(int gx, float ix) { return (32.0f - float(gx)) * GRID_SIZE - ix * GRID_PART; }
        inline float WorldY(int gy, float iy) { return (32.0f - float(gy)) * GRID_SIZE - iy * GRID_PART; }

        void TileBoundsXZ(int navTileX, int navTileY, float* bmin, float* bmax)
        {
            bmax[0] = (32.0f - float(navTileX)) * GRID_SIZE;
            bmax[2] = (32.0f - float(navTileY)) * GRID_SIZE;
            bmin[0] = bmax[0] - GRID_SIZE;
            bmin[2] = bmax[2] - GRID_SIZE;
        }

        // Recast slope-filters on the triangle normal's Y. In recast space x is world Y
        // and z is world X, and BOTH fall as the cell index grows, so the ring
        // a->b->c->d runs clockwise seen from +Y. Emitting (a, m, b) rather than
        // (a, b, m) is what makes n.y positive -- get it backwards and every ground
        // triangle is discarded as a ceiling, leaving an empty navmesh.
        void AddTerrain(const TerrainTile& tile, int gx, int gy, Soup& out)
        {
            if (!tile.hasTerrain || tile.v9.empty() || tile.v8.empty())
            {
                return;
            }

            const auto v9 = [&](int ix, int iy) { return tile.v9[ix * V9_SIDE + iy]; };
            const auto v8 = [&](int ix, int iy) { return tile.v8[ix * V8_SIDE + iy]; };

            const int base9 = out.VertexCount();
            for (int ix = 0; ix < V9_SIDE; ++ix)
            {
                for (int iy = 0; iy < V9_SIDE; ++iy)
                {
                    out.AddVertex(Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy)), v9(ix, iy)});
                }
            }

            const auto corner = [&](int ix, int iy) { return base9 + ix * V9_SIDE + iy; };

            for (int ix = 0; ix < V8_SIDE; ++ix)
            {
                for (int iy = 0; iy < V8_SIDE; ++iy)
                {
                    if (tile.IsHoleAt(ix, iy))
                    {
                        continue;
                    }

                    const int m = out.AddVertex(Vec3{WorldX(gx, float(ix) + 0.5f),
                                                     WorldY(gy, float(iy) + 0.5f),
                                                     v8(ix, iy)});
                    const int a = corner(ix, iy);
                    const int b = corner(ix, iy + 1);
                    const int c = corner(ix + 1, iy + 1);
                    const int d = corner(ix + 1, iy);

                    out.AddTriangle(a, m, b, NAV_GROUND);
                    out.AddTriangle(b, m, c, NAV_GROUND);
                    out.AddTriangle(c, m, d, NAV_GROUND);
                    out.AddTriangle(d, m, a, NAV_GROUND);
                }
            }
        }

        // Recast's ledge filter and walkable-radius erosion need geometry outside the
        // tile's core bounds. The legacy generator supplied one terrain cell from each
        // orthogonal neighbour. Emit only that requested cell range here; importing a
        // neighbour's models as well would duplicate placements which already overlap
        // the current tile.
        void AddTerrainCells(const TerrainTile& tile, int gx, int gy,
                             const CellRect& cells, Soup& out)
        {
            if (!tile.hasTerrain || tile.v9.empty() || tile.v8.empty())
            {
                return;
            }

            const auto v9 = [&](int ix, int iy) { return tile.v9[ix * V9_SIDE + iy]; };
            const auto v8 = [&](int ix, int iy) { return tile.v8[ix * V8_SIDE + iy]; };

            for (int ix = cells.ixFirst; ix <= cells.ixLast; ++ix)
            {
                for (int iy = cells.iyFirst; iy <= cells.iyLast; ++iy)
                {
                    if (tile.IsHoleAt(ix, iy))
                    {
                        continue;
                    }

                    const int a = out.AddVertex(
                        Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy)), v9(ix, iy)});
                    const int b = out.AddVertex(
                        Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy) + 1.f),
                             v9(ix, iy + 1)});
                    const int c = out.AddVertex(
                        Vec3{WorldX(gx, float(ix) + 1.f), WorldY(gy, float(iy) + 1.f),
                             v9(ix + 1, iy + 1)});
                    const int d = out.AddVertex(
                        Vec3{WorldX(gx, float(ix) + 1.f), WorldY(gy, float(iy)),
                             v9(ix + 1, iy)});
                    const int m = out.AddVertex(
                        Vec3{WorldX(gx, float(ix) + 0.5f),
                             WorldY(gy, float(iy) + 0.5f), v8(ix, iy)});

                    out.AddTriangle(a, m, b, NAV_GROUND);
                    out.AddTriangle(b, m, c, NAV_GROUND);
                    out.AddTriangle(c, m, d, NAV_GROUND);
                    out.AddTriangle(d, m, a, NAV_GROUND);
                }
            }
        }

        unsigned char LiquidArea(world::terrain::LiquidKind kind)
        {
            switch (kind)
            {
                case world::terrain::LiquidKind::Magma: return NAV_MAGMA;
                case world::terrain::LiquidKind::Slime: return NAV_SLIME;
                case world::terrain::LiquidKind::Water:
                case world::terrain::LiquidKind::Ocean: return NAV_WATER;
                default: return NAV_EMPTY;
            }
        }

        // The liquid surface, per cell, and only where a swimmer could actually be.
        //
        // Two rules, both taken from the reference generator and both load-bearing:
        //
        //   * a surface that lies UNDER the terrain of its own cell is dropped. Emitting
        //     it anyway buries swimmable polygons inside a hillside, and creatures then
        //     path through solid ground.
        //   * DEEP ("dark") water is dropped outright. Players take fatigue there and are
        //     not meant to be in it, so neither is anything pathing.
        void AddLiquid(const TerrainTile& tile, int gx, int gy, Soup& out)
        {
            if (!tile.hasLiquid || tile.liquidHeight.empty() || tile.liquidShow.empty())
            {
                return;
            }

            const auto lh = [&](int ix, int iy) { return tile.liquidHeight[ix * V9_SIDE + iy]; };
            const auto v9 = [&](int ix, int iy) { return tile.v9[ix * V9_SIDE + iy]; };
            const auto v8 = [&](int ix, int iy) { return tile.v8[ix * V8_SIDE + iy]; };
            const bool haveTerrain = tile.hasTerrain && !tile.v9.empty() && !tile.v8.empty();

            for (int ix = 0; ix < V8_SIDE; ++ix)
            {
                for (int iy = 0; iy < V8_SIDE; ++iy)
                {
                    const size_t cell = size_t(ix) * V8_SIDE + iy;
                    if (!tile.liquidShow[cell])
                    {
                        continue;
                    }
                    if (!tile.liquidDeep.empty() && tile.liquidDeep[cell])
                    {
                        continue;
                    }

                    const auto kind = tile.liquidKind.empty()
                                          ? world::terrain::LiquidKind::Water
                                          : world::terrain::LiquidKind(tile.liquidKind[cell]);
                    const unsigned char area = LiquidArea(kind);
                    if (area == NAV_EMPTY)
                    {
                        continue;
                    }

                    const float h00 = lh(ix, iy), h01 = lh(ix, iy + 1);
                    const float h11 = lh(ix + 1, iy + 1), h10 = lh(ix + 1, iy);

                    if (haveTerrain && !tile.IsHoleAt(ix, iy))
                    {
                        const float maxLiquid = std::max(std::max(h00, h01), std::max(h11, h10));
                        const float minTerrain =
                            std::min(std::min(std::min(v9(ix, iy), v9(ix, iy + 1)),
                                              std::min(v9(ix + 1, iy + 1), v9(ix + 1, iy))),
                                     v8(ix, iy));
                        if (minTerrain > maxLiquid)
                        {
                            continue;
                        }
                    }

                    const int a = out.AddVertex(Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy)), h00});
                    const int b = out.AddVertex(Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy) + 1.f), h01});
                    const int c = out.AddVertex(Vec3{WorldX(gx, float(ix) + 1.f), WorldY(gy, float(iy) + 1.f), h11});
                    const int d = out.AddVertex(Vec3{WorldX(gx, float(ix) + 1.f), WorldY(gy, float(iy)), h10});

                    out.AddTriangle(a, c, b, area);
                    out.AddTriangle(a, d, c, area);
                }
            }
        }

        void AddLiquidCells(const TerrainTile& tile, int gx, int gy,
                            const CellRect& cells, Soup& out)
        {
            if (!tile.hasLiquid || tile.liquidHeight.empty() || tile.liquidShow.empty())
            {
                return;
            }

            const auto lh = [&](int ix, int iy) { return tile.liquidHeight[ix * V9_SIDE + iy]; };
            const auto v9 = [&](int ix, int iy) { return tile.v9[ix * V9_SIDE + iy]; };
            const auto v8 = [&](int ix, int iy) { return tile.v8[ix * V8_SIDE + iy]; };
            const bool haveTerrain = tile.hasTerrain && !tile.v9.empty() && !tile.v8.empty();

            for (int ix = cells.ixFirst; ix <= cells.ixLast; ++ix)
            {
                for (int iy = cells.iyFirst; iy <= cells.iyLast; ++iy)
                {
                    const size_t cell = size_t(ix) * V8_SIDE + iy;
                    if (!tile.liquidShow[cell])
                    {
                        continue;
                    }
                    if (!tile.liquidDeep.empty() && tile.liquidDeep[cell])
                    {
                        continue;
                    }

                    const auto kind = tile.liquidKind.empty()
                                          ? world::terrain::LiquidKind::Water
                                          : world::terrain::LiquidKind(tile.liquidKind[cell]);
                    const unsigned char area = LiquidArea(kind);
                    if (area == NAV_EMPTY)
                    {
                        continue;
                    }

                    const float h00 = lh(ix, iy), h01 = lh(ix, iy + 1);
                    const float h11 = lh(ix + 1, iy + 1), h10 = lh(ix + 1, iy);

                    if (haveTerrain && !tile.IsHoleAt(ix, iy))
                    {
                        const float maxLiquid =
                            std::max(std::max(h00, h01), std::max(h11, h10));
                        const float minTerrain =
                            std::min(std::min(std::min(v9(ix, iy), v9(ix, iy + 1)),
                                              std::min(v9(ix + 1, iy + 1),
                                                       v9(ix + 1, iy))),
                                     v8(ix, iy));
                        if (minTerrain > maxLiquid)
                        {
                            continue;
                        }
                    }

                    const int a = out.AddVertex(
                        Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy)), h00});
                    const int b = out.AddVertex(
                        Vec3{WorldX(gx, float(ix)), WorldY(gy, float(iy) + 1.f), h01});
                    const int c = out.AddVertex(
                        Vec3{WorldX(gx, float(ix) + 1.f), WorldY(gy, float(iy) + 1.f),
                             h11});
                    const int d = out.AddVertex(
                        Vec3{WorldX(gx, float(ix) + 1.f), WorldY(gy, float(iy)), h10});

                    out.AddTriangle(a, c, b, area);
                    out.AddTriangle(a, d, c, area);
                }
            }
        }

        // Every static WMO/M2 instance, pushed through its placement. The recast
        // conversion is a cyclic permutation, so the authored winding survives.
        void AddModels(const TerrainTile& tile, Soup& out)
        {
            for (const world::terrain::StaticInstance& inst : tile.instances)
            {
                if (!inst.model || inst.model->Empty())
                {
                    continue;
                }

                const world::terrain::TriSoup* soup = nullptr;
                if (inst.model->Kind() == world::terrain::ModelKind::Wmo)
                {
                    soup = &static_cast<const world::terrain::WmoModel*>(inst.model.get())->Soup();
                }
                else
                {
                    soup = &static_cast<const world::terrain::CollisionModel*>(inst.model.get())->Soup();
                }

                const int base = out.VertexCount();
                for (const Vec3& v : soup->verts)
                {
                    out.AddVertex(inst.xf.localToWorld(v));
                }
                for (const auto& tri : soup->tris)
                {
                    out.AddTriangle(base + int(tri[0]), base + int(tri[1]),
                                    base + int(tri[2]), NAV_GROUND);
                }
            }
        }

        // One jump link, in recast space and already filtered to its tile.
        struct OffMeshLink
        {
            float verts[6];
            float radius;
        };

        // offmesh.txt, one link per line, in the reference generator's format:
        //   <mapId> <tileX>,<tileY> (x y z) (x y z) <radius>
        // The tile pair is the FILE-NAME pair, which is the grid pair swapped, so it is
        // compared against (navTileX, navTileY) rather than (gx, gy).
        std::vector<OffMeshLink> LoadOffMesh(const std::string& path, uint32_t mapId,
                                             int navTileX, int navTileY)
        {
            std::vector<OffMeshLink> links;
            if (path.empty())
            {
                return links;
            }

            std::FILE* f = std::fopen(path.c_str(), "rb");
            if (!f)
            {
                return links;
            }

            char line[512];
            while (std::fgets(line, sizeof(line), f))
            {
                float p0[3], p1[3], size;
                int mid, tx, ty;
                if (std::sscanf(line, "%d %d,%d (%f %f %f) (%f %f %f) %f", &mid, &tx, &ty,
                                &p0[0], &p0[1], &p0[2], &p1[0], &p1[1], &p1[2],
                                &size) != 10)
                {
                    continue;
                }
                if (uint32_t(mid) != mapId || tx != navTileX || ty != navTileY)
                {
                    continue;
                }

                OffMeshLink link;
                link.verts[0] = p0[1];
                link.verts[1] = p0[2];
                link.verts[2] = p0[0];
                link.verts[3] = p1[1];
                link.verts[4] = p1[2];
                link.verts[5] = p1[0];
                link.radius = size;
                links.push_back(link);
            }

            std::fclose(f);
            return links;
        }

        bool WriteFile(const std::string& path, const void* head, size_t headSize,
                       const void* body, size_t bodySize)
        {
            std::FILE* f = std::fopen(path.c_str(), "wb");
            if (!f)
            {
                return false;
            }
            bool ok = headSize == 0 || std::fwrite(head, headSize, 1, f) == 1;
            ok = ok && (bodySize == 0 || std::fwrite(body, bodySize, 1, f) == 1);
            std::fclose(f);
            if (!ok)
            {
                std::remove(path.c_str());
            }
            return ok;
        }

        struct MapBake
        {
            uint32_t mapId = 0;
            std::string tileDir;
            std::string outDir;
            NavConfig cfg;
            float orig[3] = {0.f, 0.f, 0.f};
            int subTileSize = 0;
            int subTilesPerTile = 0;
            int borderSize = 0;

            // Set only for a WMO-only map (a WDT with no ADT grid, e.g. Deeprun Tram):
            // there is no per-grid t_ tile to read, so every grid this WMO spans bakes
            // from this one shared tile instead. Null for an ordinary ADT grid.
            std::shared_ptr<const TerrainTile> globalWmo;
        };

        // Grid tiles a global WMO's world footprint touches. Both axes fall as the index
        // grows -- WorldX(gx,0) = (32 - gx) * GRID_SIZE -- so the high world corner gives
        // the low index and vice versa. A rectangle over the union bbox can name a grid
        // the WMO never reaches; that tile just bakes empty and writes nothing.
        std::vector<std::pair<int, int>> GlobalWmoGrids(const TerrainTile& tile)
        {
            Geometry::Aabb box;
            for (const world::terrain::StaticInstance& inst : tile.instances)
            {
                if (inst.worldBounds.valid())
                {
                    box.expand(inst.worldBounds);
                }
            }
            if (!box.valid())
            {
                return {};
            }

            const auto gridIndex = [](float world)
            {
                const int i = int(std::floor(32.0f - world / GRID_SIZE));
                return std::max(0, std::min(63, i));
            };

            std::vector<std::pair<int, int>> grids;
            for (int gx = gridIndex(box.hi.x); gx <= gridIndex(box.lo.x); ++gx)
            {
                for (int gy = gridIndex(box.hi.y); gy <= gridIndex(box.lo.y); ++gy)
                {
                    grids.emplace_back(gx, gy);
                }
            }
            return grids;
        }

        bool AddNeighbourGeometry(const MapBake& mb, int gx, int gy,
                                  Soup& solid, Soup& liquid)
        {
            constexpr int neighbours[][2] = {
                {-1, 0},
                {1, 0},
                {0, -1},
                {0, 1},
            };

            for (const auto& offset : neighbours)
            {
                const int neighbourGx = gx + offset[0];
                const int neighbourGy = gy + offset[1];
                const std::string path =
                    mb.tileDir + "/" +
                    world::terrain::TileFileName(mb.mapId, neighbourGx, neighbourGy);

                std::error_code ec;
                const bool exists = std::filesystem::exists(path, ec);
                if (ec)
                {
                    std::lock_guard<std::mutex> lock(g_bakeLogMutex);
                    std::fprintf(stderr,
                                 "nav: map %u tile (%d,%d): cannot inspect neighbour "
                                 "(%d,%d): %s\n",
                                 mb.mapId, gx, gy, neighbourGx, neighbourGy,
                                 ec.message().c_str());
                    return false;
                }
                if (!exists)
                {
                    continue;
                }

                std::shared_ptr<TerrainTile> neighbour =
                    world::terrain::ReadTile(path);
                if (!neighbour)
                {
                    std::lock_guard<std::mutex> lock(g_bakeLogMutex);
                    std::fprintf(stderr,
                                 "nav: map %u tile (%d,%d): existing neighbour "
                                 "(%d,%d) is unreadable: %s\n",
                                 mb.mapId, gx, gy, neighbourGx, neighbourGy,
                                 path.c_str());
                    return false;
                }

                CellRect cells;
                if (!NeighbourCellRect(offset[0], offset[1], cells))
                {
                    return false;
                }
                AddTerrainCells(*neighbour, neighbourGx, neighbourGy, cells, solid);
                AddLiquidCells(*neighbour, neighbourGx, neighbourGy, cells, liquid);
            }
            return true;
        }

        // Which triangles each sub-tile has to look at.
        //
        // A grid tile is 25x25 sub-tiles, and the obvious loop hands the WHOLE tile soup
        // -- 131k terrain triangles plus every model -- to each of the 625. Recast then
        // early-outs on a bounds test per triangle, so it is correct, but it is 82
        // million tests where a few hundred thousand would do, and the slope filter runs
        // that many times over as well.
        //
        // Binning once per tile turns that into: one pass to file each triangle under the
        // sub-tiles its XZ box spans, then each sub-tile walks only its own list. A
        // terrain triangle spans one bin, so the pass is O(triangles).
        //
        // The span is padded by the sub-tile border, because a triangle just outside a
        // sub-tile's core still contributes to its border ring. Padding can only ever add
        // a bin, never drop one -- and dropping one would silently punch a hole in the
        // navmesh.
        struct TriBins
        {
            int side = 0;
            std::vector<std::vector<int>> bins;

            const std::vector<int>& At(int sx, int sy) const
            {
                return bins[size_t(sy) * side + sx];
            }
        };

        TriBins BinTriangles(const Soup& soup, const rcConfig& cfg, int subTileSize,
                             int borderSize, int side)
        {
            TriBins out;
            out.side = side;
            out.bins.resize(size_t(side) * side);
            if (soup.Empty() || side <= 0)
            {
                return out;
            }

            const float width = float(subTileSize) * cfg.cs;
            const float pad = float(borderSize) * cfg.cs;

            for (int t = 0; t < soup.TriangleCount(); ++t)
            {
                float lo[2] = {FLT_MAX, FLT_MAX};
                float hi[2] = {-FLT_MAX, -FLT_MAX};
                for (int k = 0; k < 3; ++k)
                {
                    const float* v = &soup.verts[size_t(soup.tris[t * 3 + k]) * 3];
                    lo[0] = std::min(lo[0], v[0]);
                    hi[0] = std::max(hi[0], v[0]);
                    lo[1] = std::min(lo[1], v[2]);
                    hi[1] = std::max(hi[1], v[2]);
                }

                int sxFirst, sxLast, syFirst, syLast;
                SubTileSpan(lo[0], hi[0], cfg.bmin[0], width, pad, side, sxFirst, sxLast);
                SubTileSpan(lo[1], hi[1], cfg.bmin[2], width, pad, side, syFirst, syLast);

                for (int sy = syFirst; sy <= syLast; ++sy)
                {
                    for (int sx = sxFirst; sx <= sxLast; ++sx)
                    {
                        out.bins[size_t(sy) * side + sx].push_back(t);
                    }
                }
            }
            return out;
        }

        // Gathers one bin's triangles into the flat arrays Recast wants. Vertices are
        // shared: only the index list is subset, so nothing is copied per sub-tile.
        void GatherBin(const Soup& soup, const std::vector<unsigned char>& areas,
                       const std::vector<int>& bin, std::vector<int>& outTris,
                       std::vector<unsigned char>& outAreas)
        {
            outTris.clear();
            outAreas.clear();
            outTris.reserve(bin.size() * 3);
            outAreas.reserve(bin.size());
            for (int t : bin)
            {
                outTris.push_back(soup.tris[t * 3 + 0]);
                outTris.push_back(soup.tris[t * 3 + 1]);
                outTris.push_back(soup.tris[t * 3 + 2]);
                outAreas.push_back(areas[t]);
            }
        }

        struct SubTileResult
        {
            std::unique_ptr<rcPolyMesh, void (*)(rcPolyMesh*)> pmesh{nullptr, rcFreePolyMesh};
            std::unique_ptr<rcPolyMeshDetail, void (*)(rcPolyMeshDetail*)> dmesh{
                nullptr, rcFreePolyMeshDetail};
        };

        bool BuildSubTile(rcContext& ctx, const rcConfig& cfg, int borderSize,
                          int subTileSize, int sx, int sy, const Soup& solid,
                          const std::vector<unsigned char>& solidAreas,
                          const TriBins& solidBins, const Soup& liquid,
                          const TriBins& liquidBins, SubTileResult& out)
        {
            rcConfig tcfg = cfg;
            tcfg.width = subTileSize + borderSize * 2;
            tcfg.height = subTileSize + borderSize * 2;
            tcfg.bmin[0] = cfg.bmin[0] + float(sx * subTileSize - borderSize) * cfg.cs;
            tcfg.bmin[2] = cfg.bmin[2] + float(sy * subTileSize - borderSize) * cfg.cs;
            tcfg.bmax[0] = cfg.bmin[0] + float((sx + 1) * subTileSize + borderSize) * cfg.cs;
            tcfg.bmax[2] = cfg.bmin[2] + float((sy + 1) * subTileSize + borderSize) * cfg.cs;

            std::unique_ptr<rcHeightfield, void (*)(rcHeightfield*)> hf(rcAllocHeightfield(),
                                                                       rcFreeHeightField);
            if (!hf || !rcCreateHeightfield(&ctx, *hf, tcfg.width, tcfg.height, tcfg.bmin,
                                            tcfg.bmax, tcfg.cs, tcfg.ch))
            {
                return false;
            }

            std::vector<int> binTris;
            std::vector<unsigned char> binAreas;

            GatherBin(solid, solidAreas, solidBins.At(sx, sy), binTris, binAreas);
            if (binTris.empty())
            {
                return false;   // nothing of the world reaches this sub-tile
            }
            rcRasterizeTriangles(&ctx, solid.verts.data(), solid.VertexCount(),
                                 binTris.data(), binAreas.data(),
                                 int(binAreas.size()), *hf, cfg.walkableClimb);

            rcFilterLowHangingWalkableObstacles(&ctx, cfg.walkableClimb, *hf);
            rcFilterLedgeSpans(&ctx, tcfg.walkableHeight, tcfg.walkableClimb, *hf);
            rcFilterWalkableLowHeightSpans(&ctx, tcfg.walkableHeight, *hf);

            // Liquid is rasterised AFTER the walkability filters so its area id survives
            // into the query filter -- a swimmable span must not be filtered as a ledge.
            if (!liquid.Empty())
            {
                GatherBin(liquid, liquid.areas, liquidBins.At(sx, sy), binTris, binAreas);
                if (!binTris.empty())
                {
                    rcRasterizeTriangles(&ctx, liquid.verts.data(), liquid.VertexCount(),
                                         binTris.data(), binAreas.data(),
                                         int(binAreas.size()), *hf, cfg.walkableClimb);
                }
            }

            std::unique_ptr<rcCompactHeightfield, void (*)(rcCompactHeightfield*)> chf(
                rcAllocCompactHeightfield(), rcFreeCompactHeightfield);
            if (!chf || !rcBuildCompactHeightfield(&ctx, tcfg.walkableHeight,
                                                   tcfg.walkableClimb, *hf, *chf))
            {
                return false;
            }
            if (!rcErodeWalkableArea(&ctx, cfg.walkableRadius, *chf))
            {
                return false;
            }
            if (!rcBuildDistanceField(&ctx, *chf) ||
                !rcBuildRegions(&ctx, *chf, tcfg.borderSize, tcfg.minRegionArea,
                                tcfg.mergeRegionArea))
            {
                return false;
            }

            std::unique_ptr<rcContourSet, void (*)(rcContourSet*)> cset(rcAllocContourSet(),
                                                                       rcFreeContourSet);
            if (!cset || !rcBuildContours(&ctx, *chf, tcfg.maxSimplificationError,
                                          tcfg.maxEdgeLen, *cset))
            {
                return false;
            }
            if (cset->nconts == 0)
            {
                return false;
            }

            out.pmesh.reset(rcAllocPolyMesh());
            if (!out.pmesh || !rcBuildPolyMesh(&ctx, *cset, tcfg.maxVertsPerPoly, *out.pmesh))
            {
                return false;
            }

            out.dmesh.reset(rcAllocPolyMeshDetail());
            if (!out.dmesh || !rcBuildPolyMeshDetail(&ctx, *out.pmesh, *chf,
                                                     tcfg.detailSampleDist,
                                                     tcfg.detailSampleMaxError, *out.dmesh))
            {
                return false;
            }
            return true;
        }

        // Bakes one grid cell into one .mmtile. Workers share no mutable state: each
        // reads its own tile plus up to four neighbour borders and writes its own file,
        // which keeps the map bake safe to run one tile per core without a cache lock.
        bool BakeTile(const MapBake& mb, int gx, int gy, bool& tileError)
        {
            tileError = false;

            Soup solid;
            Soup liquid;

            if (mb.globalWmo)
            {
                // No terrain, no neighbours: the one WMO is the whole map. Each grid it
                // spans rasterises the shared soup; Recast clips it to the grid bounds.
                AddModels(*mb.globalWmo, solid);
                if (solid.Empty())
                {
                    return false;
                }
            }
            else
            {
                const std::string tilePath =
                    mb.tileDir + "/" + world::terrain::TileFileName(mb.mapId, gx, gy);
                std::shared_ptr<TerrainTile> tile = world::terrain::ReadTile(tilePath);
                if (!tile)
                {
                    std::lock_guard<std::mutex> lock(g_bakeLogMutex);
                    std::fprintf(stderr, "nav: map %u tile (%d,%d) is unreadable: %s\n",
                                 mb.mapId, gx, gy, tilePath.c_str());
                    tileError = true;
                    return false;
                }

                AddTerrain(*tile, gx, gy, solid);
                AddModels(*tile, solid);
                AddLiquid(*tile, gx, gy, liquid);
                if (solid.Empty())
                {
                    return false;  // ocean tile: nothing to stand on
                }
                if (!AddNeighbourGeometry(mb, gx, gy, solid, liquid))
                {
                    tileError = true;
                    return false;
                }
            }

            const int navTileX = gy;
            const int navTileY = gx;

            float bmin[3], bmax[3];
            rcCalcBounds(solid.verts.data(), solid.VertexCount(), bmin, bmax);
            if (!liquid.Empty())
            {
                float lmin[3], lmax[3];
                rcCalcBounds(liquid.verts.data(), liquid.VertexCount(), lmin, lmax);
                bmin[1] = std::min(bmin[1], lmin[1]);
                bmax[1] = std::max(bmax[1], lmax[1]);
            }
            TileBoundsXZ(navTileX, navTileY, bmin, bmax);

            rcConfig cfg{};
            rcVcopy(cfg.bmin, bmin);
            rcVcopy(cfg.bmax, bmax);
            cfg.cs = mb.cfg.cellSize;
            cfg.ch = mb.cfg.cellSize;
            cfg.walkableSlopeAngle = mb.cfg.maxWalkableAngle;
            cfg.walkableHeight = mb.cfg.walkableHeight;
            cfg.walkableClimb = mb.cfg.walkableClimb;
            cfg.walkableRadius = mb.cfg.walkableRadius;
            cfg.borderSize = mb.borderSize;
            cfg.maxVertsPerPoly = DT_VERTS_PER_POLYGON;
            cfg.maxEdgeLen = mb.subTileSize + 1;
            cfg.minRegionArea = rcSqr(60);
            cfg.mergeRegionArea = rcSqr(50);
            cfg.maxSimplificationError = 2.0f;
            cfg.detailSampleDist = cfg.cs * 64.0f;
            cfg.detailSampleMaxError = cfg.ch * 2.0f;
            cfg.tileSize = mb.subTileSize;

            rcContext ctx(false);

            // The slope test depends on the triangle alone, not on which sub-tile is
            // being built, so it runs once for the tile instead of once per sub-tile.
            std::vector<unsigned char> solidAreas(size_t(solid.TriangleCount()), NAV_GROUND);
            rcClearUnwalkableTriangles(&ctx, cfg.walkableSlopeAngle, solid.verts.data(),
                                       solid.VertexCount(), solid.tris.data(),
                                       solid.TriangleCount(), solidAreas.data());

            const TriBins solidBins =
                BinTriangles(solid, cfg, mb.subTileSize, mb.borderSize, mb.subTilesPerTile);
            const TriBins liquidBins =
                BinTriangles(liquid, cfg, mb.subTileSize, mb.borderSize, mb.subTilesPerTile);

            std::vector<SubTileResult> parts;
            for (int sy = 0; sy < mb.subTilesPerTile; ++sy)
            {
                for (int sx = 0; sx < mb.subTilesPerTile; ++sx)
                {
                    SubTileResult part;
                    if (BuildSubTile(ctx, cfg, mb.borderSize, mb.subTileSize, sx, sy,
                                     solid, solidAreas, solidBins, liquid, liquidBins,
                                     part))
                    {
                        parts.push_back(std::move(part));
                    }
                }
            }
            if (parts.empty())
            {
                return false;
            }

            std::vector<rcPolyMesh*> pmeshes;
            std::vector<rcPolyMeshDetail*> dmeshes;
            pmeshes.reserve(parts.size());
            dmeshes.reserve(parts.size());
            for (SubTileResult& p : parts)
            {
                pmeshes.push_back(p.pmesh.get());
                dmeshes.push_back(p.dmesh.get());
            }

            std::unique_ptr<rcPolyMesh, void (*)(rcPolyMesh*)> merged(rcAllocPolyMesh(),
                                                                     rcFreePolyMesh);
            std::unique_ptr<rcPolyMeshDetail, void (*)(rcPolyMeshDetail*)> mergedDetail(
                rcAllocPolyMeshDetail(), rcFreePolyMeshDetail);
            if (!merged || !mergedDetail ||
                !rcMergePolyMeshes(&ctx, pmeshes.data(), int(pmeshes.size()), *merged) ||
                !rcMergePolyMeshDetails(&ctx, dmeshes.data(), int(dmeshes.size()),
                                        *mergedDetail))
            {
                return false;
            }

            // Every walkable poly must carry a flag or the query filter rejects all of
            // them; the area id is what the server's filter then reads.
            for (int i = 0; i < merged->npolys; ++i)
            {
                if (merged->areas[i] == RC_WALKABLE_AREA)
                {
                    merged->areas[i] = NAV_GROUND;
                }
                merged->flags[i] = merged->areas[i] ? 1 : 0;
            }

            const std::vector<OffMeshLink> links =
                LoadOffMesh(mb.cfg.offMeshFile, mb.mapId, navTileX, navTileY);
            std::vector<float> offVerts;
            std::vector<float> offRads;
            std::vector<unsigned char> offDirs;
            std::vector<unsigned char> offAreas;
            std::vector<unsigned short> offFlags;
            for (const OffMeshLink& l : links)
            {
                offVerts.insert(offVerts.end(), l.verts, l.verts + 6);
                offRads.push_back(l.radius);
                offDirs.push_back(1);           // bidirectional
                offAreas.push_back(NAV_GROUND);
                offFlags.push_back(0xFFFF);     // usable by every movement mask
            }

            dtNavMeshCreateParams np{};
            np.verts = merged->verts;
            np.vertCount = merged->nverts;
            np.polys = merged->polys;
            np.polyAreas = merged->areas;
            np.polyFlags = merged->flags;
            np.polyCount = merged->npolys;
            np.nvp = merged->nvp;
            np.detailMeshes = mergedDetail->meshes;
            np.detailVerts = mergedDetail->verts;
            np.detailVertsCount = mergedDetail->nverts;
            np.detailTris = mergedDetail->tris;
            np.detailTriCount = mergedDetail->ntris;
            np.offMeshConVerts = offVerts.empty() ? nullptr : offVerts.data();
            np.offMeshConRad = offRads.empty() ? nullptr : offRads.data();
            np.offMeshConDir = offDirs.empty() ? nullptr : offDirs.data();
            np.offMeshConAreas = offAreas.empty() ? nullptr : offAreas.data();
            np.offMeshConFlags = offFlags.empty() ? nullptr : offFlags.data();
            np.offMeshConCount = int(offRads.size());
            np.walkableHeight = mb.cfg.cellSize * float(cfg.walkableHeight);
            np.walkableRadius = mb.cfg.cellSize * float(cfg.walkableRadius);
            np.walkableClimb = mb.cfg.cellSize * float(cfg.walkableClimb);
            // NOT navTileX/navTileY. Detour finds a tile with calcTileLoc, which is
            // floor((pos - orig) / tileWidth) -- an index relative to the navmesh ORIGIN,
            // and the origin is the min corner of the highest-indexed tile, so the two
            // run in opposite directions. Storing the raw index makes every lookup miss:
            // 687 tiles load without complaint and every query returns no polygon.
            // The FILE NAME still uses the raw pair; only this does not.
            np.tileX = int(((cfg.bmin[0] + cfg.bmax[0]) * 0.5f - mb.orig[0]) / GRID_SIZE);
            np.tileY = int(((cfg.bmin[2] + cfg.bmax[2]) * 0.5f - mb.orig[2]) / GRID_SIZE);
            np.tileLayer = 0;
            rcVcopy(np.bmin, merged->bmin);
            rcVcopy(np.bmax, merged->bmax);
            np.cs = cfg.cs;
            np.ch = cfg.ch;
            np.buildBvTree = true;

            unsigned char* navData = nullptr;
            int navDataSize = 0;
            if (!dtCreateNavMeshData(&np, &navData, &navDataSize))
            {
                return false;
            }

            MmapTileHeader header;
            header.size = uint32(navDataSize);
            header.usesLiquids = !liquid.Empty();

            char name[64];
            std::snprintf(name, sizeof(name), "%04u%02i%02i.mmtile", mb.mapId, navTileX,
                          navTileY);
            const bool ok = WriteFile(mb.outDir + "/" + name, &header, sizeof(header),
                                      navData, size_t(navDataSize));
            dtFree(navData);
            return ok;
        }
    }

    void SubTileSpan(float lo, float hi, float origin, float width, float pad, int side,
                     int& first, int& last)
    {
        first = int(std::floor((lo - pad - origin) / width));
        last = int(std::floor((hi + pad - origin) / width));
        first = std::max(0, first);
        last = std::min(side - 1, last);
    }

    bool NeighbourCellRect(int deltaGx, int deltaGy, CellRect& out)
    {
        constexpr int last = V8_SIDE - 1;
        if (deltaGx == -1 && deltaGy == 0)
        {
            out = {last, last, 0, last};
        }
        else if (deltaGx == 1 && deltaGy == 0)
        {
            out = {0, 0, 0, last};
        }
        else if (deltaGx == 0 && deltaGy == -1)
        {
            out = {0, last, last, last};
        }
        else if (deltaGx == 0 && deltaGy == 1)
        {
            out = {0, last, 0, 0};
        }
        else
        {
            return false;
        }
        return true;
    }

    NavMeshBuilder::NavMeshBuilder(std::string tileDir, std::string outDir, NavConfig cfg)
        : m_tileDir(std::move(tileDir)), m_outDir(std::move(outDir)), m_cfg(cfg)
    {
    }

    void NavMeshBuilder::SetProgress(ProgressFn fn, void* context)
    {
        m_progress = fn;
        m_progressContext = context;
    }

    void NavMeshBuilder::SetMapDone(MapDoneFn fn)
    {
        m_mapDone = fn;
    }

    int NavMeshBuilder::BakeMap(uint32_t mapId, const std::string& mapName,
                                const std::vector<std::pair<int, int>>& grids,
                                std::shared_ptr<const world::terrain::TerrainTile> globalWmo)
    {
        if (grids.empty())
        {
            return 0;
        }

        int navTileXMax = 0;
        int navTileYMax = 0;
        for (const auto& g : grids)
        {
            navTileXMax = std::max(navTileXMax, g.second);
            navTileYMax = std::max(navTileYMax, g.first);
        }

        // The origin is the min corner of the highest-indexed tile, which is the
        // convention the server's dtNavMesh is initialised with. Y is unused by Detour
        // -- it locates tiles in XZ only -- but the reference writes FLT_MIN there and
        // the file must stay byte-compatible with what the server expects to read.
        float orig[3], originMax[3];
        TileBoundsXZ(navTileXMax, navTileYMax, orig, originMax);
        orig[1] = FLT_MIN;

        dtNavMeshParams params{};
        rcVcopy(params.orig, orig);
        params.tileWidth = GRID_SIZE;
        params.tileHeight = GRID_SIZE;
        params.maxTiles = int(grids.size());
        params.maxPolys = 1 << DT_POLY_BITS;

        char name[32];
        std::snprintf(name, sizeof(name), "%04u.mmap", mapId);
        if (!WriteFile(m_outDir + "/" + name, &params, sizeof(params), nullptr, 0))
        {
            return 0;
        }

        MapBake mb;
        mb.mapId = mapId;
        mb.tileDir = m_tileDir;
        mb.outDir = m_outDir;
        mb.cfg = m_cfg;
        rcVcopy(mb.orig, orig);
        mb.subTileSize = m_cfg.subTileSize;
        mb.subTilesPerTile = int(GRID_SIZE / m_cfg.cellSize + 0.5f) / m_cfg.subTileSize;
        mb.borderSize = m_cfg.walkableRadius + 3;
        mb.globalWmo = std::move(globalWmo);

        unsigned workers = m_cfg.threads > 0 ? unsigned(m_cfg.threads)
                                             : std::thread::hardware_concurrency();
        if (workers == 0)
        {
            workers = 1;
        }
        workers = std::min<unsigned>(workers, unsigned(grids.size()));

        std::atomic<size_t> next{0};
        std::atomic<int> written{0};
        std::atomic<int> tileErrors{0};

        // Only the main-thread worker touches the console, so the report reads the shared
        // dispatch counter rather than adding a lock the pool would contend on. `next`
        // counts tiles STARTED, which is a fine progress proxy and never stalls at the end.
        auto report = [&](size_t started)
        {
            if (m_progress)
            {
                m_progress(m_progressContext, mapId, mapName.c_str(),
                           std::min(started, grids.size()), grids.size());
            }
        };

        auto worker = [&](bool isMain)
        {
            for (;;)
            {
                const size_t i = next.fetch_add(1);
                if (i >= grids.size())
                {
                    return;
                }
                bool tileError = false;
                if (BakeTile(mb, grids[i].first, grids[i].second, tileError))
                {
                    ++written;
                }
                else if (tileError)
                {
                    ++tileErrors;
                }
                if (isMain)
                {
                    report(next.load());
                }
            }
        };

        report(0);
        std::vector<std::thread> pool;
        pool.reserve(workers);
        for (unsigned i = 1; i < workers; ++i)
        {
            pool.emplace_back(worker, false);
        }
        worker(true);
        for (std::thread& t : pool)
        {
            t.join();
        }
        report(grids.size());

        if (tileErrors.load() != 0)
        {
            std::lock_guard<std::mutex> lock(g_bakeLogMutex);
            std::fprintf(stderr, "nav: map %u failed: %d tile input error(s)\n",
                         mapId, tileErrors.load());
            return -1;
        }
        return written.load();
    }

    int NavMeshBuilder::BakeAll(long mapFilter)
    {
        std::error_code ec;
        std::filesystem::create_directories(m_outDir, ec);

        // Which maps and grids exist is read off the baked tiles themselves, so the
        // navmesh can only ever cover ground the collision engine also has. A map is
        // either an ADT grid (t_ tiles) or a single global WMO (a lone w_ tile); the
        // extractor writes one or the other, never both.
        std::map<uint32_t, std::vector<std::pair<int, int>>> byMap;
        std::set<uint32_t> globalWmoMaps;
        for (const auto& entry : std::filesystem::directory_iterator(m_tileDir, ec))
        {
            const std::string leaf = entry.path().filename().string();
            unsigned mapId = 0;
            int gx = 0, gy = 0;
            if (std::sscanf(leaf.c_str(), "t_%u_%d_%d.tile", &mapId, &gx, &gy) == 3)
            {
                if (mapFilter < 0 || uint32_t(mapFilter) == mapId)
                {
                    byMap[mapId].emplace_back(gx, gy);
                }
            }
            else if (std::sscanf(leaf.c_str(), "w_%u.tile", &mapId) == 1)
            {
                if (mapFilter < 0 || uint32_t(mapFilter) == mapId)
                {
                    globalWmoMaps.insert(mapId);
                }
            }
        }
        if (ec)
        {
            return -1;
        }

        const size_t mapCount = byMap.size() + globalWmoMaps.size();
        int total = 0;
        size_t done = 0;
        for (auto& entry : byMap)
        {
            char label[48];
            std::snprintf(label, sizeof(label), "map %u  [%zu/%zu]", entry.first,
                          done + 1, mapCount);
            const int written = BakeMap(entry.first, label, entry.second);
            if (written < 0)
            {
                return -1;
            }
            if (m_mapDone)
            {
                m_mapDone(m_progressContext, entry.first, label, written,
                          entry.second.size());
            }
            total += written;
            ++done;
        }

        for (uint32_t mapId : globalWmoMaps)
        {
            char label[48];
            std::snprintf(label, sizeof(label), "map %u  [%zu/%zu]", mapId, done + 1,
                          mapCount);

            std::shared_ptr<TerrainTile> tile = world::terrain::ReadTile(
                m_tileDir + "/" + world::terrain::GlobalWmoFileName(mapId));
            if (tile)
            {
                const std::vector<std::pair<int, int>> grids = GlobalWmoGrids(*tile);
                const int written = BakeMap(mapId, label, grids, tile);
                if (written < 0)
                {
                    return -1;
                }
                if (m_mapDone)
                {
                    m_mapDone(m_progressContext, mapId, label, written, grids.size());
                }
                total += written;
            }
            ++done;
        }
        return total;
    }
}
