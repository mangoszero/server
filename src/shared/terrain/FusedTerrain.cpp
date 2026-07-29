#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <shared_mutex>
#include "terrain/FusedTerrain.hpp"
#include "terrain/TileSerializer.hpp"
#include "terrain/WmoModel.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <utility>

namespace world::terrain
{
    namespace
    {
        // The idle window is comfortably longer than the sweep interval so a tile on the
        // edge of an active grid -- pinned by nobody, but hit constantly by queries
        // reaching across the boundary -- is never evicted from under a live query.
        constexpr uint32_t SWEEP_INTERVAL_MS = 60u * 1000u;
        constexpr uint32_t TILE_IDLE_MS = 5u * 60u * 1000u;

        std::string g_tileDir;

        float SegmentHitFrac(const std::vector<const StaticInstance*>& instances,
                             const Vec3& a, const Vec3& b)
        {
            const Vec3 seg = b - a;
            if (dot(seg, seg) < 1e-6f)
            {
                return 2.0f;
            }

            auto inv = [](float d) { return std::fabs(d) > 1e-9f ? 1.0f / d : 1e30f; };
            const Vec3 invDir{inv(seg.x), inv(seg.y), inv(seg.z)};

            float best = 2.0f;
            for (const StaticInstance* inst : instances)
            {
                if (!inst->model || inst->model->Empty() ||
                    !inst->worldBounds.intersectsRay(a, invDir, 1.0f))
                {
                    continue;
                }

                const Vec3 originLocal = inst->xf.worldToLocal(a);
                const Vec3 dirLocal = inst->xf.worldToLocal(b) - originLocal;
                if (auto t = inst->model->RaycastNearest(originLocal, dirLocal, 1.0f))
                {
                    if (*t >= 0.f && *t < best)
                    {
                        best = *t;
                    }
                }
            }
            return best;
        }
    }

    void FusedTerrain::SetTileDir(const std::string& dir) { g_tileDir = dir; }
    const std::string& FusedTerrain::TileDir() { return g_tileDir; }

    FusedTerrain::FusedTerrain(uint32_t mapId, std::shared_ptr<ITileSource> source)
        : m_mapId(mapId), m_source(std::move(source))
    {
    }

    bool FusedTerrain::HasTile(uint32_t mapId, int tx, int ty)
    {
        if (g_tileDir.empty())
        {
            return false;
        }
        if (std::ifstream(g_tileDir + "/" + TileFileName(mapId, tx, ty),
                          std::ios::binary).good())
        {
            return true;
        }
        // A map built from one global WMO carries no ADT grid tiles at all.
        return std::ifstream(g_tileDir + "/" + GlobalWmoFileName(mapId),
                             std::ios::binary).good();
    }

    FusedTerrain::TilePtr FusedTerrain::LoadCell(int tx, int ty) const
    {
        if (m_source)
        {
            return m_source->Load(m_mapId, tx, ty);
        }
        if (g_tileDir.empty())
        {
            return nullptr;
        }
        return ReadTile(g_tileDir + "/" + TileFileName(m_mapId, tx, ty));
    }

    FusedTerrain::TilePtr FusedTerrain::TileAt(float x, float y) const
    {
        const int tx = TileIndex(x);
        const int ty = TileIndex(y);
        if (tx < 0 || tx >= GRID_COUNT || ty < 0 || ty >= GRID_COUNT)
        {
            return nullptr;
        }

        const uint32_t now = m_clockMs.load(std::memory_order_relaxed);
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (m_loaded[tx][ty])
            {
                m_tileLastUse[tx][ty].store(now, std::memory_order_relaxed);
                return m_tiles[tx][ty];
            }
        }

        // Read outside the lock so I/O does not stall other columns. A racing thread may
        // load the same cell; either result describes the same tile.
        TilePtr tile = LoadCell(tx, ty);

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (!m_loaded[tx][ty])
        {
            m_tiles[tx][ty] = std::move(tile);
            m_loaded[tx][ty] = 1;
        }
        m_tileLastUse[tx][ty].store(now, std::memory_order_relaxed);
        return m_tiles[tx][ty];
    }

    FusedTerrain::TilePtr FusedTerrain::GlobalWmo() const
    {
        {
            std::shared_lock<std::shared_mutex> lock(m_mutex);
            if (m_globalWmoProbed)
            {
                return m_globalWmo;
            }
        }

        TilePtr tile;
        if (m_source)
        {
            tile = m_source->LoadGlobal(m_mapId);
        }
        else if (!g_tileDir.empty())
        {
            tile = ReadTile(g_tileDir + "/" + GlobalWmoFileName(m_mapId));
        }

        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (!m_globalWmoProbed)
        {
            m_globalWmo = std::move(tile);
            m_globalWmoProbed = 1;
        }
        return m_globalWmo;
    }

    void FusedTerrain::EvictTile(int tx, int ty) const
    {
        // m_loaded goes back to 0 so the next query re-probes. The absent-tile memo is
        // not kept here: its whole value is recording that the file is missing, and this
        // tile plainly exists.
        m_tiles[tx][ty].reset();
        m_loaded[tx][ty] = 0;
        m_tileLastUse[tx][ty].store(0, std::memory_order_relaxed);
    }

    void FusedTerrain::Update(uint32_t diff)
    {
        const uint32_t now = m_clockMs.load(std::memory_order_relaxed) + diff;
        m_clockMs.store(now, std::memory_order_relaxed);

        m_sweepAccumMs += diff;
        if (m_sweepAccumMs < SWEEP_INTERVAL_MS)
        {
            return;
        }
        m_sweepAccumMs = 0;

        // Lock order is cell-ref then tile cache; nothing else takes both.
        std::lock_guard<std::mutex> refLock(m_cellRefMutex);
        std::unique_lock<std::shared_mutex> lock(m_mutex);

        for (int tx = 0; tx < GRID_COUNT; ++tx)
        {
            for (int ty = 0; ty < GRID_COUNT; ++ty)
            {
                if (!m_tiles[tx][ty] || m_cellRef[tx][ty] > 0)
                {
                    continue;
                }
                // Unsigned subtraction, so this stays correct across the counter's wrap.
                if (now - m_tileLastUse[tx][ty].load(std::memory_order_relaxed) <
                    TILE_IDLE_MS)
                {
                    continue;
                }
                EvictTile(tx, ty);
            }
        }
    }

    void FusedTerrain::PinCell(int tx, int ty)
    {
        if (tx < 0 || tx >= GRID_COUNT || ty < 0 || ty >= GRID_COUNT)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(m_cellRefMutex);
        ++m_cellRef[tx][ty];
    }

    void FusedTerrain::UnpinCell(int tx, int ty)
    {
        if (tx < 0 || tx >= GRID_COUNT || ty < 0 || ty >= GRID_COUNT)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(m_cellRefMutex);
        if (m_cellRef[tx][ty] > 0)
        {
            --m_cellRef[tx][ty];
        }
    }

    size_t FusedTerrain::ResidentTiles() const
    {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        size_t n = 0;
        for (int tx = 0; tx < GRID_COUNT; ++tx)
        {
            for (int ty = 0; ty < GRID_COUNT; ++ty)
            {
                n += m_tiles[tx][ty] ? 1 : 0;
            }
        }
        return n;
    }

    Column FusedTerrain::ColumnAt(float x, float y, float zTop, float zBottom,
                                  const ILiveGeometry* live, uint32_t filter) const
    {
        Column column;

        TilePtr tile = TileAt(x, y);
        TilePtr global = GlobalWmo();
        if (!tile && !global)
        {
            return column;
        }

        // Deliberately not clipped to zBottom. A heightmap sample is a single value the
        // tile already holds, so there is nothing to gain by hiding it, and a caller
        // probing from far above (MAX_HEIGHT) would otherwise get an empty column on a
        // map whose only surface is terrain.
        if (tile)
        {
            if (auto h = tile->TerrainHeight(x, y))
            {
                if (*h <= zTop)
                {
                    column.AddSolid(*h, SurfaceKind::Terrain);
                }
            }
        }

        const float span = zTop - zBottom;
        const Vec3 originWorld{x, y, zTop};
        const Vec3 downWorld{0.0f, 0.0f, -1.0f};

        std::vector<float> hits;

        auto probe = [&](const std::vector<StaticInstance>& instances)
        {
            for (const StaticInstance& inst : instances)
            {
                if (!inst.model || inst.model->Empty())
                {
                    continue;
                }
                const Aabb& wb = inst.worldBounds;
                if (!wb.coversColumn(x, y) || wb.hi.z < zBottom || wb.lo.z > zTop + 0.1f)
                {
                    continue;
                }

                // Every instance -- a map's global WMO included -- stores its model in
                // model space plus a placement. A global WMO's placement is NOT identity:
                // it carries the half-turn about Z, so raycasting the raw model in world
                // space misses the floor on every global-WMO map but the one whose
                // placement happens to be identity.
                const Vec3 originLocal = inst.xf.worldToLocal(originWorld);
                const Vec3 dirLocal = inst.xf.worldToLocalDirection(downWorld);

                // localToWorld(o + t*d) == originWorld + t*downWorld, so t is already a
                // world distance whatever the instance scale.
                hits.clear();
                inst.model->RaycastAll(originLocal, dirLocal, span, hits);
                for (const float t : hits)
                {
                    column.AddSolid(zTop - t, SurfaceKind::Static);
                }

                // The ray origin doubles as the liquid probe: MLIQ is indexed by local X
                // and Y alone, and every real placement is a Z-rotation plus a
                // translation, so which height along the column it is taken from cannot
                // change the pair those come out as.
                const Vec3 pointLocal = inst.xf.worldToLocal(originWorld);
                if (auto local = inst.model->LiquidLocal(pointLocal))
                {
                    const LiquidKind kind = static_cast<LiquidKind>(local->kind);
                    if (kind != LiquidKind::None)
                    {
                        // Lift the surface back through the placement itself: it sits
                        // directly over the query column, so transforming that exact
                        // point is exact. Reconstructing the lift by hand applies the
                        // placement scale twice and assumes the model's local Z is
                        // parallel to world Z.
                        const Vec3 surfaceLocal{pointLocal.x, pointLocal.y, local->z};
                        LiquidInfo info;
                        info.level = inst.xf.localToWorld(surfaceLocal).z;
                        info.kind = kind;
                        info.entry = local->entry;
                        info.deep = local->deep;
                        column.AddLiquid(info);
                    }
                }
            }
        };

        if (tile)
        {
            probe(tile->instances);
        }
        if (global && global != tile)
        {
            probe(global->instances);
        }

        if (tile)
        {
            if (auto adt = tile->LiquidAt(x, y))
            {
                column.AddLiquid(*adt);
            }
        }

        if (live)
        {
            live->AddSurfaces(x, y, zTop, zBottom, filter, column);
        }

        return column;
    }

    void FusedTerrain::CollectSegmentInstances(const Vec3& a, const Vec3& b,
                                               std::vector<const StaticInstance*>& out,
                                               std::vector<TilePtr>& keepAlive) const
    {
        const float minx = std::min(a.x, b.x), maxx = std::max(a.x, b.x);
        const float miny = std::min(a.y, b.y), maxy = std::max(a.y, b.y);

        const float dx = b.x - a.x, dy = b.y - a.y;
        const float lengthXY = std::sqrt(dx * dx + dy * dy);
        const int samples = std::max(2, int(lengthXY / (TILE_SIZE * 0.5f)) + 2);

        auto gather = [&](const TilePtr& tile)
        {
            if (!tile)
            {
                return;
            }
            keepAlive.push_back(tile);
            for (const StaticInstance& inst : tile->instances)
            {
                const Aabb& wb = inst.worldBounds;
                if (wb.hi.x < minx || wb.lo.x > maxx || wb.hi.y < miny || wb.lo.y > maxy)
                {
                    continue;
                }
                out.push_back(&inst);
            }
        };

        int lastTx = 0, lastTy = 0;
        bool seen = false;
        for (int i = 0; i < samples; ++i)
        {
            const float f = float(i) / float(samples - 1);
            const float px = a.x + dx * f, py = a.y + dy * f;
            const int tx = TileIndex(px), ty = TileIndex(py);
            if (seen && tx == lastTx && ty == lastTy)
            {
                continue;
            }
            seen = true;
            lastTx = tx;
            lastTy = ty;
            if (tx >= 0 && tx < GRID_COUNT && ty >= 0 && ty < GRID_COUNT)
            {
                gather(TileAt(px, py));
            }
        }

        gather(GlobalWmo());
    }

    float FusedTerrain::NearestHitFraction(float x1, float y1, float z1, float x2,
                                           float y2, float z2) const
    {
        const Vec3 a{x1, y1, z1}, b{x2, y2, z2};
        std::vector<const StaticInstance*> instances;
        std::vector<TilePtr> keepAlive;
        CollectSegmentInstances(a, b, instances, keepAlive);
        return SegmentHitFrac(instances, a, b);
    }

    bool FusedTerrain::IsInLineOfSight(float x1, float y1, float z1, float x2, float y2,
                                       float z2) const
    {
        return NearestHitFraction(x1, y1, z1, x2, y2, z2) > 1.0f;
    }

    uint16_t FusedTerrain::GetAreaId(float x, float y) const
    {
        TilePtr tile = TileAt(x, y);
        if (!tile || !tile->hasTerrain)
        {
            return 0;
        }
        return tile->AreaId(x, y);
    }

    bool FusedTerrain::GetAreaInfo(float x, float y, float z, uint32_t& mogpFlags,
                                   int32_t& adtId, int32_t& rootId, int32_t& groupId,
                                   float& groundZ) const
    {
        TilePtr tile = TileAt(x, y);
        TilePtr global = GlobalWmo();
        if (!tile && !global)
        {
            return false;
        }

        constexpr float SEARCH_UP = 2.0f;
        constexpr float MAX_DROP = 300.0f;
        const float ceiling = z + SEARCH_UP;
        const Vec3 originWorld{x, y, ceiling};
        const Vec3 downWorld{0.0f, 0.0f, -1.0f};

        bool found = false;
        float bestZ = -std::numeric_limits<float>::max();
        uint32_t bestMogp = 0;
        int32_t bestAdt = 0, bestRoot = 0, bestGroup = 0;

        auto scan = [&](const std::vector<StaticInstance>& instances)
        {
            for (const StaticInstance& inst : instances)
            {
                if (!inst.model || inst.model->Kind() != ModelKind::Wmo ||
                    inst.model->Empty())
                {
                    continue;
                }
                const Aabb& wb = inst.worldBounds;
                if (!wb.coversColumn(x, y) || wb.hi.z < ceiling - MAX_DROP ||
                    wb.lo.z > ceiling + 0.1f)
                {
                    continue;
                }

                const auto* wmo = static_cast<const WmoModel*>(inst.model.get());
                const Vec3 originLocal = inst.xf.worldToLocal(originWorld);
                const Vec3 dirLocal = inst.xf.worldToLocalDirection(downWorld);

                if (auto area = wmo->AreaInfo(originLocal, dirLocal, MAX_DROP))
                {
                    const float hitZ = ceiling - area->t;
                    if (hitZ <= ceiling && hitZ > bestZ)
                    {
                        bestZ = hitZ;
                        found = true;
                        bestMogp = area->mogpFlags;
                        bestGroup = int32_t(area->groupId);
                        bestRoot = int32_t(wmo->RootId());
                        bestAdt = inst.adtId;
                    }
                }
            }
        };

        if (tile)
        {
            scan(tile->instances);
        }
        if (global && global != tile)
        {
            scan(global->instances);
        }

        // Roof guard: terrain lying between the query point and the WMO floor means the
        // querier stands on the terrain above the building, not inside it.
        if (found && tile)
        {
            if (auto th = tile->TerrainHeight(x, y))
            {
                if (z + 2.0f > *th && *th > bestZ)
                {
                    found = false;
                }
            }
        }

        if (found)
        {
            mogpFlags = bestMogp;
            adtId = bestAdt;
            rootId = bestRoot;
            groupId = bestGroup;
            groundZ = bestZ;
        }
        return found;
    }
}
