#pragma once

// WMO root and group files, parsed to plain data: the collidable faces and the
// interior liquid surface. No acceleration structure is built here.

#include "terrain/Geometry.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace world::terrain
{
    struct WmoLiquid
    {
        uint32_t tilesX = 0;
        uint32_t tilesY = 0;
        Vec3 corner{};
        std::vector<float> heights;  ///< (tilesX+1)*(tilesY+1)
        std::vector<uint8_t> flags;  ///< tilesX*tilesY, low nibble 15 = dry
        uint16_t entry = 0;          ///< LiquidType.dbc row id
    };

    struct WmoGroupData
    {
        uint32_t mogpFlags = 0;
        uint32_t groupWmoId = 0;
        std::vector<Vec3> verts;
        std::vector<std::array<uint16_t, 3>> tris;
        bool hasLiquid = false;
        WmoLiquid liquid;
    };

    struct WmoDoodad
    {
        std::string name;
        Vec3 pos{};
        float quat[4] = {0.f, 0.f, 0.f, 1.f};
        float scale = 1.f;
    };

    struct WmoDoodadSet
    {
        uint32_t start = 0;
        uint32_t count = 0;
    };

    struct WmoRootData
    {
        uint32_t nGroups = 0;
        uint32_t wmoId = 0;
        uint32_t flags = 0;
        std::vector<WmoDoodadSet> sets;
        std::vector<WmoDoodad> doodads;
    };

    bool ParseWmoRoot(const uint8_t* data, size_t size, WmoRootData& out);

    // rootFlags is WmoRootData::flags; its bit 0x4 decides whether MOGP.groupLiquid is
    // already a LiquidType.dbc row id. Returns false when the group carries neither
    // collidable geometry nor liquid.
    bool ParseWmoGroup(const uint8_t* data, size_t size, uint32_t rootFlags,
                       WmoGroupData& out);

    // "X.wmo" -> "X_003.wmo"
    std::string WmoGroupPath(const std::string& root, uint32_t index);

    inline bool ParseWmoRoot(const std::vector<uint8_t>& b, WmoRootData& out)
    {
        return ParseWmoRoot(b.data(), b.size(), out);
    }

    inline bool ParseWmoGroup(const std::vector<uint8_t>& b, uint32_t rootFlags,
                              WmoGroupData& out)
    {
        return ParseWmoGroup(b.data(), b.size(), rootFlags, out);
    }
}
