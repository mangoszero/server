#pragma once

// Flat binary form of an assembled TerrainTile: the height and liquid grids, the
// collidable geometry of every model on it, and the BVH the baker built over each.
// Reading the client MPQs -- decompression, hundreds of WMO group opens -- and
// building the acceleration structures happens once, offline; a load is then a
// single sequential read with none of it.
//
// Native-endian, same-machine cache, not a portable archive. Magic and version guard
// the format: ReadTile returns nullptr on any mismatch or truncation, which the
// caller treats as a miss and rebakes.

#include "terrain/Terrain.hpp"

#include <memory>
#include <string>

namespace world::terrain
{
    bool WriteTile(const TerrainTile& tile, const std::string& path);

    std::shared_ptr<TerrainTile> ReadTile(const std::string& path);

    // Names a tile file. `tx` is the tile index from world X, `ty` from world Y.
    std::string TileFileName(uint32_t mapId, int tx, int ty);
    std::string GlobalWmoFileName(uint32_t mapId);

    // Blizzard named a map for most vessels but not all, and a hull has a model either
    // way. One is minted per GAME OBJECT entry -- not per display id, since two ships of
    // the same model are still two decks -- clear of Map.dbc, whose ids end in the
    // hundreds.
    constexpr uint32_t VESSEL_MAP_BASE = 1000000;

    inline uint32_t MintedVesselMapId(uint32_t goEntry) { return VESSEL_MAP_BASE + goEntry; }

    // A game object's collision, per GameObjectDisplayInfo.dbc id. It is stored as an
    // ordinary one-instance tile so the writer and reader above are the only ones,
    // rather than a second format that could drift from them.
    std::string GoModelFileName(uint32_t displayId);
}
