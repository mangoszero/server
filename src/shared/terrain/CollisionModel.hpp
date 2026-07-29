#pragma once

// Static collision geometry held once and shared by every placement of that model:
// a WMO instanced fifty times stores its triangles once, and the placements differ
// only by Transform. Queries run in model space -- the world ray is transformed in,
// never the geometry out.

#include "terrain/Accelerators.hpp"
#include "terrain/Geometry.hpp"
#include "terrain/ICollisionModel.hpp"

#include <optional>
#include <vector>

namespace world::terrain
{
    class CollisionModel : public ICollisionModel
    {
    public:
        CollisionModel() = default;

        // An empty `bvh` is built here; the baker hands one already built, in which case
        // soup.tris must already be in that BVH's permuted order.
        explicit CollisionModel(TriSoup soup, Bvh bvh = Bvh{});

        ModelKind Kind() const override { return ModelKind::Mesh; }

        std::optional<float> RaycastNearest(const Vec3& origin, const Vec3& dir,
                                            float tMax) const override;

        void RaycastAll(const Vec3& origin, const Vec3& dir, float tMax,
                        std::vector<float>& out) const override;

        const Aabb& Bounds() const override { return m_bounds; }
        bool Empty() const override { return m_empty; }

        size_t TriangleCount() const { return m_soup.tris.size(); }
        const TriSoup& Soup() const { return m_soup; }
        const Bvh& GetBvh() const { return m_bvh; }

    protected:
        void DeriveBounds();

        TriSoup m_soup;
        Bvh m_bvh;
        Aabb m_bounds;
        bool m_empty = true;
    };
}
