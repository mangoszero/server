#pragma once

// One 3.3.5a .adt tile, parsed into the grids the height engine wants. The V9/V8
// layout and the tile-local indexing match the reference map-extractor exactly, so
// TerrainTile::TerrainHeight indexes them correctly: the FIRST index is derived from
// the MCNK IndexY field and is what a world-X query resolves against.
//
// Liquid is reported as raw LiquidType.dbc row ids. Classifying a row into
// water/ocean/magma/slime needs the DBC and is deliberately not done here.

#include "ChunkReaders.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace world::terrain
{
    constexpr int ADT_CHUNKS = 16;
    constexpr int ADT_CELLS_PER_CHUNK = 8;
    constexpr int ADT_GRID = ADT_CHUNKS * ADT_CELLS_PER_CHUNK;  // 128
    constexpr int ADT_V9 = ADT_GRID + 1;                        // 129

    struct AdtData
    {
        bool hasTerrain = false;
        std::vector<float> v9;
        std::vector<float> v8;
        std::array<uint16_t, ADT_CHUNKS * ADT_CHUNKS> holes{};
        std::array<uint16_t, ADT_CHUNKS * ADT_CHUNKS> areaIds{};

        bool hasLiquid = false;
        bool hasMh2o = false;
        std::vector<float> liquidHeight;
        std::vector<uint8_t> liquidShow;
        std::vector<uint16_t> liquidEntry;
        std::vector<uint8_t> liquidDark;     ///< MCLQ per-cell dark-water bit
        std::vector<uint8_t> liquidNoLight;  ///< MH2O layer shipped no light map

        std::vector<std::string> wmoNames;
        std::vector<std::string> m2Names;
        std::vector<Placement> wmoPlacements;
        std::vector<Placement> m2Placements;
    };

    // Returns false only on a structurally broken file; a valid-but-empty tile yields
    // hasTerrain = false with no placements.
    bool ParseAdt(const uint8_t* data, size_t size, AdtData& out);

    inline bool ParseAdt(const std::vector<uint8_t>& bytes, AdtData& out)
    {
        return ParseAdt(bytes.data(), bytes.size(), out);
    }
}
