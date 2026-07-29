#pragma once

// The collision hull of an M2 (MD20): its bounding-volume vertices and triangles.
// Nothing else in the model is collidable.

#include "terrain/Geometry.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace world::terrain
{
    struct M2Data
    {
        uint32_t version = 0;
        std::vector<Vec3> verts;
        std::vector<std::array<uint32_t, 3>> tris;

        bool Empty() const { return tris.empty(); }
    };

    bool ParseM2(const uint8_t* data, size_t size, M2Data& out);

    inline bool ParseM2(const std::vector<uint8_t>& bytes, M2Data& out)
    {
        return ParseM2(bytes.data(), bytes.size(), out);
    }

    // MMDX/MODN name .mdx or .mdl; the file on disk is .m2.
    std::string M2PathOf(std::string path);
}
