#pragma once

// Every surface found under one point, in one answer. The engine reports what is there;
// which surface a QUESTION means -- the floor beneath a unit, the water it swims in, the
// ground below a fall -- is a SELECTION over this. One gather, many selections, so no
// caller can be handed a differently pre-selected answer by a differently shaped entry.

#include "terrain/Terrain.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

namespace world::terrain
{
    enum class SurfaceKind : uint8_t
    {
        Terrain,   ///< the ADT heightmap
        Static,    ///< a baked model: a building, a bridge
        Live,      ///< a body posed at runtime: a door, a lift
        Liquid,
    };

    struct Surface
    {
        float z = 0.f;
        SurfaceKind kind = SurfaceKind::Terrain;
        LiquidKind liquid = LiquidKind::None;
        uint16_t liquidEntry = 0;
        bool deep = false;

        bool Solid() const { return kind != SurfaceKind::Liquid; }

        LiquidInfo AsLiquid() const
        {
            LiquidInfo info;
            info.level = z;
            info.kind = liquid;
            info.entry = liquidEntry;
            info.deep = deep;
            return info;
        }
    };

    class Column
    {
        public:
            void AddSolid(float z, SurfaceKind kind)
            {
                Surface s;
                s.z = z;
                s.kind = kind;
                m_surfaces.push_back(s);
            }

            void AddLiquid(const LiquidInfo& info)
            {
                Surface s;
                s.z = info.level;
                s.kind = SurfaceKind::Liquid;
                s.liquid = info.kind;
                s.liquidEntry = info.entry;
                s.deep = info.deep;
                m_surfaces.push_back(s);
            }

            void Clear() { m_surfaces.clear(); }
            bool Empty() const { return m_surfaces.empty(); }
            const std::vector<Surface>& Surfaces() const { return m_surfaces; }

            std::optional<float> HighestSolidAtOrBelow(float z) const
            {
                float best = -std::numeric_limits<float>::max();
                bool found = false;
                for (const Surface& s : m_surfaces)
                {
                    if (s.Solid() && s.z <= z && s.z > best)
                    {
                        best = s.z;
                        found = true;
                    }
                }
                return found ? std::optional<float>(best) : std::nullopt;
            }

            std::optional<float> LowestSolidAbove(float z) const
            {
                float best = std::numeric_limits<float>::max();
                bool found = false;
                for (const Surface& s : m_surfaces)
                {
                    if (s.Solid() && s.z > z && s.z < best)
                    {
                        best = s.z;
                        found = true;
                    }
                }
                return found ? std::optional<float>(best) : std::nullopt;
            }

            std::optional<float> HighestSolid() const
            {
                return HighestSolidAtOrBelow(std::numeric_limits<float>::max());
            }

            /// The floor a point stands on. A query point often sits a little UNDER the
            /// surface -- a spawn buried a yard into a hillside -- so when nothing is
            /// below, the nearest surface ABOVE is the floor it would stand on once
            /// freed. Answering with the highest surface anywhere instead hands back the
            /// roof of whatever the hill is under.
            std::optional<float> Floor(float z, float tolerance = 2.0f) const
            {
                if (auto below = HighestSolidAtOrBelow(z + tolerance))
                {
                    return below;
                }
                return LowestSolidAbove(z + tolerance);
            }

            std::optional<Surface> HighestLiquid() const
            {
                std::optional<Surface> best;
                for (const Surface& s : m_surfaces)
                {
                    if (s.kind == SurfaceKind::Liquid && (!best || s.z > best->z))
                    {
                        best = s;
                    }
                }
                return best;
            }

        private:
            std::vector<Surface> m_surfaces;
    };
}
