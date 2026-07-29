#pragma once

// Static collision geometry, queried in its own MODEL-LOCAL space: the world ray is
// transformed in, never the geometry.

#include "terrain/Geometry.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace world::terrain
{
    // What the serializer has to write. A virtual tag rather than a dynamic_cast: WmoModel
    // derives from CollisionModel, so a cast chain would silently depend on its order.
    enum class ModelKind : uint8_t
    {
        Mesh = 0,
        Wmo = 1
    };

    class ICollisionModel
    {
    public:
        virtual ~ICollisionModel() = default;

        virtual ModelKind Kind() const = 0;

        virtual std::optional<float> RaycastNearest(const Vec3& origin, const Vec3& dir,
                                                    float tMax) const = 0;

        // Every surface the ray crosses, appended in no particular order. What the
        // nearest hit cannot answer: which floor a point is standing on when the point
        // sits under one, and how many more lie beneath it.
        virtual void RaycastAll(const Vec3& origin, const Vec3& dir, float tMax,
                                std::vector<float>& out) const = 0;

        virtual const Aabb& Bounds() const = 0;

        virtual bool Empty() const = 0;

        // Both fields are resolved by the baker. The liquid identity lives in
        // MOGP.groupLiquid, never in MLIQ's trailing uint16 (that is a materialId).
        struct LocalLiquid
        {
            float z = 0.f;
            uint16_t entry = 0;
            uint8_t kind = 0;
            bool deep = false;
        };

        virtual std::optional<LocalLiquid> LiquidLocal(const Vec3& pModel) const
        {
            (void)pModel;
            return std::nullopt;
        }
    };
}
