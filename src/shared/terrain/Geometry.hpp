#pragma once

// Umbrella that re-exports the server-wide Geometry primitives under the names the
// terrain collision engine spells them. New code should prefer the Geometry:: names.

#include "Geometry/Vector3.h"
#include "Geometry/Shapes.h"

namespace world::terrain
{
    using Vec3 = ::Geometry::Vector3;
    using Aabb = ::Geometry::Aabb;
    using Mat3 = ::Geometry::Mat3;
    using Transform = ::Geometry::Transform;
    using Tri = ::Geometry::Tri;

    using ::Geometry::cross;
    using ::Geometry::dot;
    using ::Geometry::rayTri;
}
