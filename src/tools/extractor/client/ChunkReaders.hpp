#pragma once

#include "terrain/Geometry.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace world::terrain
{
    // A WMO (MODF) or M2 (MDDF) placement, in raw client coordinates.
    struct Placement
    {
        uint32_t nameIndex = 0;
        Vec3 pos{};
        Vec3 rotDeg{};
        float scale = 1.0f;
        uint16_t nameSet = 0;    ///< MODF nameSet, the WMOAreaTable "adtId"
        uint16_t doodadSet = 0;  ///< which MODS set furnishes this placement
    };

    namespace internal
    {
        inline uint32_t RdU32(const uint8_t* p)
        {
            return uint32_t(p[0]) | (uint32_t(p[1]) << 8) | (uint32_t(p[2]) << 16) |
                   (uint32_t(p[3]) << 24);
        }

        inline uint16_t RdU16(const uint8_t* p)
        {
            return uint16_t(uint16_t(p[0]) | (uint16_t(p[1]) << 8));
        }

        inline uint64_t RdU64(const uint8_t* p)
        {
            return uint64_t(RdU32(p)) | (uint64_t(RdU32(p + 4)) << 32);
        }

        inline float RdF32(const uint8_t* p)
        {
            const uint32_t u = RdU32(p);
            float f;
            std::memcpy(&f, &u, 4);
            return f;
        }

        // Chunk tags are stored reversed on disk ('MHDR' -> 'R','D','H','M').
        inline bool TagIs(const uint8_t* p, const char* tag)
        {
            return p[0] == tag[3] && p[1] == tag[2] && p[2] == tag[1] && p[3] == tag[0];
        }

        inline Placement ReadModf(const uint8_t* p)
        {
            Placement pl;
            pl.nameIndex = RdU32(p + 0);
            pl.pos = {RdF32(p + 8), RdF32(p + 12), RdF32(p + 16)};
            pl.rotDeg = {RdF32(p + 20), RdF32(p + 24), RdF32(p + 28)};
            pl.scale = 1.0f;
            pl.doodadSet = RdU16(p + 0x3A);
            pl.nameSet = RdU16(p + 0x3C);
            return pl;
        }

        inline Placement ReadMddf(const uint8_t* p)
        {
            Placement pl;
            pl.nameIndex = RdU32(p + 0);
            pl.pos = {RdF32(p + 8), RdF32(p + 12), RdF32(p + 16)};
            pl.rotDeg = {RdF32(p + 20), RdF32(p + 24), RdF32(p + 28)};
            pl.scale = RdU16(p + 32) / 1024.0f;
            return pl;
        }

        // Split a MWMO/MMDX string block, ordered by the MWID/MMID offset table so a
        // placement's nameIndex maps straight to a vector slot.
        inline std::vector<std::string> ResolveNames(const uint8_t* block, uint32_t blockSize,
                                                     const uint8_t* offs, uint32_t offsSize)
        {
            std::vector<std::string> names;
            const uint32_t n = offsSize / 4;
            names.reserve(n);
            for (uint32_t i = 0; i < n; ++i)
            {
                const uint32_t o = RdU32(offs + i * 4);
                if (o >= blockSize)
                {
                    names.emplace_back();
                    continue;
                }
                const char* s = reinterpret_cast<const char*>(block + o);
                const uint32_t maxLen = blockSize - o;
                uint32_t len = 0;
                while (len < maxLen && s[len] != '\0')
                {
                    ++len;
                }
                names.emplace_back(s, len);
            }
            return names;
        }
    }
}
