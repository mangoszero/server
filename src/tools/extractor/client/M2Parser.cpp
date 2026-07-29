#include <string>
#include "M2Parser.hpp"
#include "ChunkReaders.hpp"

#include <cctype>
#include <cstring>

namespace world::terrain
{
    namespace
    {
        using namespace world::terrain::internal;

        // The MD20 header shrank twice on the way to 3.3.5a: version 264 dropped both
        // playable_animation_lookup (8 bytes) and ofsViews (4), moving the bounding
        // block from 236 to 216. Reading a 3.3.5a model at the 2.4.3 offset yields
        // plausible-looking counts and a hull made of garbage, with no error anywhere.
        constexpr size_t BOUND_BLOCK_WOTLK = 216;
        constexpr size_t BOUND_BLOCK_LEGACY = 236;
        constexpr uint32_t M2_VERSION_WOTLK = 264;

        bool EndsWith(const std::string& s, const char* ext)
        {
            const size_t n = std::strlen(ext);
            if (s.size() < n)
            {
                return false;
            }
            for (size_t i = 0; i < n; ++i)
            {
                if (std::tolower(static_cast<unsigned char>(s[s.size() - n + i])) != ext[i])
                {
                    return false;
                }
            }
            return true;
        }
    }

    std::string M2PathOf(std::string path)
    {
        if (EndsWith(path, ".mdx") || EndsWith(path, ".mdl"))
        {
            path.replace(path.size() - 4, 4, ".m2");
        }
        return path;
    }

    bool ParseM2(const uint8_t* data, size_t size, M2Data& out)
    {
        out = M2Data{};
        if (!data || size < 8 || std::memcmp(data, "MD20", 4) != 0)
        {
            return false;
        }

        out.version = RdU32(data + 4);
        const size_t block = out.version >= M2_VERSION_WOTLK ? BOUND_BLOCK_WOTLK
                                                             : BOUND_BLOCK_LEGACY;
        if (size < block + 16)
        {
            return false;
        }

        const uint32_t nTriIdx = RdU32(data + block + 0);
        const uint32_t ofsTri = RdU32(data + block + 4);
        const uint32_t nVerts = RdU32(data + block + 8);
        const uint32_t ofsVerts = RdU32(data + block + 12);

        const bool vertsOk = nVerts && uint64_t(ofsVerts) + uint64_t(nVerts) * 12 <= size;
        const bool trisOk = nTriIdx && uint64_t(ofsTri) + uint64_t(nTriIdx) * 2 <= size;
        if (!vertsOk || !trisOk)
        {
            return true;
        }

        out.verts.reserve(nVerts);
        for (uint32_t v = 0; v < nVerts; ++v)
        {
            const uint8_t* p = data + ofsVerts + v * 12;
            // Negating Y is the net of the reference extractor's fixCoordSystem plus its
            // y/z swap, and lands these vertices in the same model space a WMO uses.
            out.verts.push_back({RdF32(p + 0), -RdF32(p + 4), RdF32(p + 8)});
        }

        out.tris.reserve(nTriIdx / 3);
        for (uint32_t i = 0; i + 2 < nTriIdx; i += 3)
        {
            const uint32_t a = RdU16(data + ofsTri + (i + 0) * 2);
            const uint32_t b = RdU16(data + ofsTri + (i + 1) * 2);
            const uint32_t c = RdU16(data + ofsTri + (i + 2) * 2);
            if (a < nVerts && b < nVerts && c < nVerts)
            {
                out.tris.push_back({a, b, c});
            }
        }
        return true;
    }
}
