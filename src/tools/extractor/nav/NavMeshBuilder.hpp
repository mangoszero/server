#pragma once

// Offline navmesh bake: turns the fused terrain+collision tiles into the Detour navmesh
// the server's MMAP layer loads (mmaps/<map>.mmap + .mmtile).
//
// The input is the BAKED TILE, not the client MPQs, so the surface the pathfinder walks
// is exactly the surface FusedTerrain collides against and the two cannot disagree.
//
// Coordinate spaces. WoW is Z-up, Recast is Y-up, and the server converts a world point
// (x, y, z) -> (y, z, x) (see PathFinder). That is a cyclic permutation, so it preserves
// orientation and triangle winding -- hence face normals -- carries over untouched.
//
// Tile indices. Recast X is world Y, so navmesh tile coordinates are SWAPPED relative to
// the grid: navTileX = gy, navTileY = gx. That is why the runtime opens
// mmaps/%04u%02i%02i.mmtile with (mapId, y, x); see MMapManager::loadMap.

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace world::terrain { class TerrainTile; }

namespace world::nav
{
    // Defaults reproduce the values the server's PathFinder was tuned against.
    struct NavConfig
    {
        float cellSize = 0.266666f;      ///< voxel size in yards; must divide 533.33333
        float maxWalkableAngle = 60.0f;  ///< steepest walkable slope, degrees
        int walkableHeight = 6;          ///< agent height, in cells
        int walkableClimb = 4;           ///< max step up; keep below walkableHeight
        int walkableRadius = 2;          ///< agent radius, in cells
        int subTileSize = 80;            ///< recast sub-tile edge, in cells
        int threads = 0;                 ///< 0 asks the hardware
        std::string offMeshFile;         ///< optional offmesh.txt
    };

    // Range of sub-tiles a world-space interval [lo, hi] touches on one axis.
    //
    // The bake bins triangles into sub-tiles so each sub-tile rasterises only its own
    // list rather than the whole tile's. That makes THIS function the thing deciding
    // what Recast ever sees: a range one too narrow drops geometry and punches a silent
    // hole in the navmesh, so it must be conservative -- padded by the sub-tile border,
    // and rounded outwards at both ends. Returning one bin too many costs a bounds test.
    //
    // `origin` is the tile's min corner on that axis, `width` a sub-tile's world size,
    // `pad` the border in world units. Results are clamped to [0, side - 1].
    void SubTileSpan(float lo, float hi, float origin, float width, float pad, int side,
                     int& first, int& last);

    // Inclusive terrain-cell range contributed by one orthogonal neighbour. Tile
    // indices and height-cell indices both increase toward falling world coordinates,
    // so the neighbour at gx-1 contributes its final X row, while gx+1 contributes
    // its first. Diagonal, current-tile and non-adjacent offsets are rejected.
    struct CellRect
    {
        int ixFirst = 0;
        int ixLast = 0;
        int iyFirst = 0;
        int iyLast = 0;
    };

    bool NeighbourCellRect(int deltaGx, int deltaGy, CellRect& out);

    class NavMeshBuilder
    {
    public:
        NavMeshBuilder(std::string tileDir, std::string outDir, NavConfig cfg = {});

        // Live progress WITHIN a map: `done` of `total` tiles started, so the console
        // header moves tile by tile rather than once per map. Called only from the main
        // thread, so the callback needs no locking of its own.
        using ProgressFn = void (*)(void* context, uint32_t mapId, const char* mapName,
                                    size_t done, size_t total);
        void SetProgress(ProgressFn fn, void* context);

        // One durable line per finished map (`written` of `total` tiles produced a
        // navmesh). Unlike ProgressFn this is meant to survive in a piped log, where the
        // moving header renders nothing.
        using MapDoneFn = void (*)(void* context, uint32_t mapId, const char* mapName,
                                   int written, size_t total);
        void SetMapDone(MapDoneFn fn);

        /// Bakes every map that has tiles, or only `mapFilter` when >= 0.
        /// Returns the number of .mmtile files written, or -1 on a fatal error.
        int BakeAll(long mapFilter = -1);

    private:
        // `globalWmo`, when set, is a WMO-only map's single shared tile: every grid in
        // `grids` bakes from it instead of reading a per-grid tile off disk.
        int BakeMap(uint32_t mapId, const std::string& mapName,
                    const std::vector<std::pair<int, int>>& grids,
                    std::shared_ptr<const world::terrain::TerrainTile> globalWmo = nullptr);

        std::string m_tileDir;
        std::string m_outDir;
        NavConfig m_cfg;
        ProgressFn m_progress = nullptr;
        void* m_progressContext = nullptr;
        MapDoneFn m_mapDone = nullptr;
    };
}
