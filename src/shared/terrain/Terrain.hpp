#pragma once

#include "terrain/Geometry.hpp"
#include "terrain/ICollisionModel.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace world::terrain
{
    constexpr float TILE_SIZE = 533.33333f;
    constexpr int GRID_PER_TILE = 128;
    constexpr int V9_SIDE = GRID_PER_TILE + 1;
    constexpr int CHUNKS = 16;
    constexpr int CELLS_PER_CHUNK = GRID_PER_TILE / CHUNKS;
    constexpr uint32_t MAP_CENTER = 32;

    constexpr int FusedTerrainGridCount = 64;

    inline float GridCoord(float c) { return GRID_PER_TILE * (MAP_CENTER - c / TILE_SIZE); }
    inline int TileIndex(float c) { return static_cast<int>(GridCoord(c)) >> 7; }

    enum class LiquidKind : uint8_t
    {
        None = 0,
        Water = 1,
        Ocean = 2,
        Magma = 3,
        Slime = 4,
    };

    struct LiquidInfo
    {
        float level = 0.f;
        LiquidKind kind = LiquidKind::None;
        uint16_t entry = 0;   ///< LiquidType.dbc row id (0 = unknown)
        bool deep = false;    ///< drives swim fatigue ("dark water")
    };

    struct StaticInstance
    {
        Transform xf;
        std::shared_ptr<const ICollisionModel> model;
        Aabb worldBounds;
        int32_t adtId = 0;
    };

    class TerrainTile
    {
    public:
        int tx = 0, ty = 0;
        bool hasTerrain = false;
        bool isGlobalWmo = false;

        std::vector<float> v9;                         ///< V9_SIDE*V9_SIDE corner heights
        std::vector<float> v8;                         ///< GRID_PER_TILE^2 centre heights
        std::array<uint16_t, CHUNKS * CHUNKS> holes{};
        std::array<uint16_t, CHUNKS * CHUNKS> areaIds{};

        bool hasLiquid = false;
        std::vector<float> liquidHeight;    ///< V9_SIDE*V9_SIDE corner grid
        std::vector<uint8_t> liquidShow;    ///< GRID_PER_TILE^2 cell mask
        std::vector<uint8_t> liquidKind;    ///< GRID_PER_TILE^2 LiquidKind
        std::vector<uint16_t> liquidEntry;  ///< GRID_PER_TILE^2 LiquidType.dbc id
        std::vector<uint8_t> liquidDeep;    ///< GRID_PER_TILE^2 dark-water mask

        std::vector<StaticInstance> instances;

        // Mirrors the reference GridMap: four triangles meeting at the V8 centre.
        std::optional<float> TerrainHeight(float x, float y) const
        {
            if (!hasTerrain || v8.empty() || v9.empty())
            {
                return std::nullopt;
            }

            const float gx = GridCoord(x), gy = GridCoord(y);
            const int ix = static_cast<int>(gx) & (GRID_PER_TILE - 1);
            const int iy = static_cast<int>(gy) & (GRID_PER_TILE - 1);
            if (IsHole(ix, iy))
            {
                return std::nullopt;
            }

            const float fx = gx - std::floor(gx);
            const float fy = gy - std::floor(gy);

            auto V9 = [&](int a, int b) { return v9[a * V9_SIDE + b]; };
            auto V8 = [&](int a, int b) { return v8[a * GRID_PER_TILE + b]; };

            float a, b, c;
            if (fx + fy < 1.f)
            {
                if (fx > fy)
                {
                    const float h1 = V9(ix, iy), h2 = V9(ix + 1, iy), h5 = 2 * V8(ix, iy);
                    a = h2 - h1;
                    b = h5 - h1 - h2;
                    c = h1;
                }
                else
                {
                    const float h1 = V9(ix, iy), h3 = V9(ix, iy + 1), h5 = 2 * V8(ix, iy);
                    a = h5 - h1 - h3;
                    b = h3 - h1;
                    c = h1;
                }
            }
            else
            {
                if (fx > fy)
                {
                    const float h2 = V9(ix + 1, iy), h4 = V9(ix + 1, iy + 1), h5 = 2 * V8(ix, iy);
                    a = h2 + h4 - h5;
                    b = h4 - h2;
                    c = h5 - h4;
                }
                else
                {
                    const float h3 = V9(ix, iy + 1), h4 = V9(ix + 1, iy + 1), h5 = 2 * V8(ix, iy);
                    a = h4 - h3;
                    b = h3 + h4 - h5;
                    c = h5 - h4;
                }
            }
            return a * fx + b * fy + c;
        }

        std::optional<LiquidInfo> LiquidAt(float x, float y) const
        {
            if (!hasLiquid || liquidHeight.empty() || liquidShow.empty())
            {
                return std::nullopt;
            }

            const float gx = GridCoord(x), gy = GridCoord(y);
            const int ix = static_cast<int>(gx) & (GRID_PER_TILE - 1);
            const int iy = static_cast<int>(gy) & (GRID_PER_TILE - 1);
            const int cell = ix * GRID_PER_TILE + iy;
            if (!liquidShow[cell])
            {
                return std::nullopt;
            }

            const float fx = gx - std::floor(gx);
            const float fy = gy - std::floor(gy);
            auto LH = [&](int a, int b) { return liquidHeight[a * V9_SIDE + b]; };
            const float top = LH(ix, iy) * (1 - fx) + LH(ix + 1, iy) * fx;
            const float bot = LH(ix, iy + 1) * (1 - fx) + LH(ix + 1, iy + 1) * fx;

            LiquidInfo info;
            info.level = top * (1 - fy) + bot * fy;
            info.kind = liquidKind.empty() ? LiquidKind::Water
                                           : static_cast<LiquidKind>(liquidKind[cell]);
            info.entry = liquidEntry.empty() ? uint16_t(0) : liquidEntry[cell];
            info.deep = !liquidDeep.empty() && liquidDeep[cell] != 0;
            return info;
        }

        bool IsHoleAt(int ix, int iy) const { return IsHole(ix, iy); }

        uint16_t AreaId(float x, float y) const
        {
            const float gx = GridCoord(x), gy = GridCoord(y);
            const int ix = static_cast<int>(gx) & (GRID_PER_TILE - 1);
            const int iy = static_cast<int>(gy) & (GRID_PER_TILE - 1);
            return areaIds[(ix / CELLS_PER_CHUNK) * CHUNKS + (iy / CELLS_PER_CHUNK)];
        }

    private:
        // Each of the mask's 16 bits covers a 2x2 block of the chunk's 8x8 height cells.
        bool IsHole(int ix, int iy) const
        {
            const int chunk = (ix / CELLS_PER_CHUNK) * CHUNKS + (iy / CELLS_PER_CHUNK);
            const uint16_t mask = holes[chunk];
            if (!mask)
            {
                return false;
            }
            const int hi = (ix % CELLS_PER_CHUNK) / 2, hj = (iy % CELLS_PER_CHUNK) / 2;
            return (mask >> (hi * 4 + hj)) & 1u;
        }
    };

    class ITileSource
    {
    public:
        virtual ~ITileSource() = default;
        virtual std::shared_ptr<TerrainTile> Load(uint32_t mapId, int tx, int ty) = 0;

        /// The one tile that covers the whole map, for a map built from a single model
        /// rather than an ADT grid. Nothing by default: most sources have a grid.
        virtual std::shared_ptr<TerrainTile> LoadGlobal(uint32_t mapId)
        {
            (void)mapId;
            return nullptr;
        }
    };

    class NullTileSource : public ITileSource
    {
    public:
        std::shared_ptr<TerrainTile> Load(uint32_t, int, int) override { return nullptr; }
    };
}
