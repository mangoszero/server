#pragma once

#include "ChunkReaders.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace world::terrain
{
    struct WdtData
    {
        bool hasGlobalWmo = false;
        bool hasMainChunk = false;
        std::string globalWmoName;
        std::optional<Placement> globalWmoPlacement;
        uint32_t mphdFlags = 0;

        // Only ever reached through HasAdt below. MAIN's 4096 entries are row-major in
        // the tile index derived from world X, so the raw array is [tx][ty] -- the
        // opposite order to the "<ty>_<tx>" the ADT file name uses. Indexing it directly
        // reads the transpose, which on a map whose coverage is nearly symmetric loses
        // about half the tiles and reports nothing at all.
        std::array<std::array<bool, 64>, 64> adtGrid{};

        bool HasAdt(int tx, int ty) const
        {
            if (tx < 0 || tx > 63 || ty < 0 || ty > 63)
            {
                return false;
            }
            return adtGrid[size_t(tx)][size_t(ty)];
        }

        bool HasAnyAdt() const
        {
            for (const auto& row : adtGrid)
            {
                for (bool v : row)
                {
                    if (v)
                    {
                        return true;
                    }
                }
            }
            return false;
        }
    };

    bool ParseWdt(const uint8_t* data, size_t size, WdtData& out);

    inline bool ParseWdt(const std::vector<uint8_t>& bytes, WdtData& out)
    {
        return ParseWdt(bytes.data(), bytes.size(), out);
    }
}
