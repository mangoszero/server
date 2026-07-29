#pragma once

// A WMO's collision. Geometry from every group is folded into one model-wide soup and
// each triangle remembers which group it came from, which is all "which room am I
// standing in" needs -- the answer falls out of the same traversal that found the floor.
//
// Blizzard's authored MOBN/MOBR BSP is deliberately not used: its nodes carry a split
// plane and no bounding box, so a ray that misses a group is still walked through it, and
// it indexes every face when only about 40% of a WMO's faces are collidable.

#include "terrain/CollisionModel.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace world::terrain
{
    class WmoModel : public CollisionModel
    {
    public:
        struct Liquid
        {
            uint32_t tilesX = 0, tilesY = 0;
            Vec3 corner;
            uint16_t entry = 0;
            uint8_t kind = 0;
            std::vector<float> heights;
            std::vector<uint8_t> flags;
        };

        struct Group
        {
            uint32_t mogpFlags = 0;
            uint32_t groupWmoId = 0;
            bool hasLiquid = false;
            Liquid liquid;
        };

        WmoModel() = default;

        WmoModel(TriSoup soup, std::vector<uint16_t> triGroup, std::vector<Group> groups,
                 uint32_t rootWmoId, Bvh bvh = Bvh{});

        ModelKind Kind() const override { return ModelKind::Wmo; }

        std::optional<LocalLiquid> LiquidLocal(const Vec3& pModel) const override;

        uint32_t RootId() const { return m_rootId; }
        const std::vector<Group>& Groups() const { return m_groups; }
        const std::vector<uint16_t>& TriGroups() const { return m_triGroup; }

        struct AreaResult
        {
            uint32_t groupId = 0;
            uint32_t mogpFlags = 0;
            float t = 0.f;
        };

        std::optional<AreaResult> AreaInfo(const Vec3& origin, const Vec3& dir,
                                           float tMax) const;

    private:
        void DeriveWmoBounds();

        std::vector<uint16_t> m_triGroup;
        std::vector<Group> m_groups;
        uint32_t m_rootId = 0;
    };
}
