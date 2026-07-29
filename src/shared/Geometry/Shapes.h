#pragma once

// Collision / placement primitives for the terrain engine, part of the one server-wide
// geometry module (namespace Geometry): an axis-aligned box, a 3x3 rotation matrix, a
// rigid+uniform-scale Transform (what WMO/M2 placements are), a triangle, and a
// Moller-Trumbore ray/triangle test — all over the shared Geometry::Vector3.
//
// These were historically the collision engine's own world::terrain::{Aabb,Mat3,
// Transform,Tri} types; they now live here beside Vector2/3/4, Matrix4 and Quat so the
// whole server draws geometry from a single module. terrain/Geometry.hpp re-exports them
// under world::terrain for the existing engine spellings. See [[project_g3d_removal]].

#include "Geometry/Vector3.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace Geometry
{
    // Free (functional-style) vector helpers used by the collision engine. They delegate
    // to the Vector3 methods so the math is defined in exactly one place.
    inline float dot(const Vector3& a, const Vector3& b) { return a.dot(b); }
    inline Vector3 cross(const Vector3& a, const Vector3& b) { return a.cross(b); }

    // Vector3 is serialized by raw bytes in the on-disk .tile ("TBCX") format
    // (TileSerializer), and the baker (writer) and server (reader) share these sources,
    // so its size and layout must never change.
    static_assert(sizeof(Vector3) == 3 * sizeof(float), "Vector3 must stay 3 packed floats (on-disk .tile format)");
    static_assert(std::is_trivially_copyable<Vector3>::value, "Vector3 must stay trivially copyable (raw tile I/O)");

    // Axis-aligned bounding box.
    struct Aabb
    {
        Vector3 lo{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        Vector3 hi{-std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max()};

        void expand(const Vector3& p)
        {
            lo.x = std::min(lo.x, p.x);
            lo.y = std::min(lo.y, p.y);
            lo.z = std::min(lo.z, p.z);
            hi.x = std::max(hi.x, p.x);
            hi.y = std::max(hi.y, p.y);
            hi.z = std::max(hi.z, p.z);
        }
        void expand(const Aabb& b)
        {
            expand(b.lo);
            expand(b.hi);
        }
        bool valid() const { return lo.x <= hi.x; }

        // Does the vertical line at (px,py) pass through this box's XY footprint?
        bool coversColumn(float px, float py) const
        {
            return px >= lo.x && px <= hi.x && py >= lo.y && py <= hi.y;
        }
        // Slab test for a ray; returns true if [0,tMax] overlaps the box.
        //
        // The box is PADDED by kSlabEps first, and that padding is load-bearing, not
        // cosmetic. rayTri() accepts a hit up to kUVEps (1e-5) outside a triangle's
        // barycentric edge -- deliberately, so a ray cannot slip through the crack
        // between two adjacent triangles -- whereas this test is exact. A BVH node's box
        // is the TIGHT union of its triangles' bounds, so a ray grazing a triangle's rim
        // is accepted by rayTri yet rejected by the very box enclosing it: the traversal
        // never visits that leaf, misses the nearest surface, and returns the next one
        // behind it. That is a creature falling through the floor onto the level below,
        // and it is exactly what mangos-accelbench caught (map 543: a downward floor
        // probe whose x sat 0.141 mm outside a node's x-slab, with dx == 0, so the ray
        // never entered the slab at all -- 10 such misses per 20k rays on map 0).
        //
        // Padding can only ever ADD leaves to the walk, never remove them, and rayTri
        // remains the final arbiter, so it cannot manufacture a false hit. Making the
        // broadphase at least as permissive as the narrowphase is the only safe
        // direction; never tighten this below rayTri's tolerance.
        bool intersectsRay(const Vector3& o, const Vector3& invDir, float tMax) const
        {
            // Yards, comfortably above rayTri's world-space slop (kUVEps scaled by the
            // longest edge a WMO triangle realistically has).
            constexpr float kSlabEps = 1e-2f;

            float t0 = 0.f, t1 = tMax;
            for (int a = 0; a < 3; ++a)
            {
                const float oa = (&o.x)[a], id = (&invDir.x)[a];
                const float loa = (&lo.x)[a] - kSlabEps, hia = (&hi.x)[a] + kSlabEps;
                float ta = (loa - oa) * id, tb = (hia - oa) * id;
                if (ta > tb)
                {
                    std::swap(ta, tb);
                }
                t0 = std::max(t0, ta);
                t1 = std::min(t1, tb);
                if (t0 > t1)
                {
                    return false;
                }
            }
            return true;
        }
    };

    // Column-major-agnostic 3x3 (row-stored). Only what a placement needs.
    struct Mat3
    {
        std::array<float, 9> m{1, 0, 0, 0, 1, 0, 0, 0, 1}; // identity

        Vector3 mul(const Vector3& v) const
        {
            return {m[0] * v.x + m[1] * v.y + m[2] * v.z,
                    m[3] * v.x + m[4] * v.y + m[5] * v.z,
                    m[6] * v.x + m[7] * v.y + m[8] * v.z};
        }
        Mat3 transpose() const
        {
            return Mat3{{m[0], m[3], m[6], m[1], m[4], m[7], m[2], m[5], m[8]}};
        }
        static Mat3 mulm(const Mat3& a, const Mat3& b)
        {
            Mat3 r;
            for (int i = 0; i < 3; ++i)
            {
                for (int j = 0; j < 3; ++j)
                {
                    r.m[i * 3 + j] = a.m[i * 3 + 0] * b.m[0 * 3 + j] +
                                     a.m[i * 3 + 1] * b.m[1 * 3 + j] +
                                     a.m[i * 3 + 2] * b.m[2 * 3 + j];
                }
            }
            return r;
        }
        // Rotation from a quaternion (x, y, z, w). WMO doodad placements (MODD) store
        // their rotation this way, unlike the Euler degrees ADT placements use.
        static Mat3 fromQuat(float x, float y, float z, float w)
        {
            const float n = std::sqrt(x * x + y * y + z * z + w * w);
            if (n < 1e-8f)
            {
                return Mat3{}; // degenerate -> identity
            }
            const float s = 1.0f / n;
            x *= s;
            y *= s;
            z *= s;
            w *= s;
            return Mat3{{1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y),
                         2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
                         2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)}};
        }

        // R = Rz * Ry * Rx, angles in radians.
        static Mat3 fromEuler(float rx, float ry, float rz)
        {
            const float cx = std::cos(rx), sx = std::sin(rx);
            const float cy = std::cos(ry), sy = std::sin(ry);
            const float cz = std::cos(rz), sz = std::sin(rz);
            Mat3 Rx{{1, 0, 0, 0, cx, -sx, 0, sx, cx}};
            Mat3 Ry{{cy, 0, sy, 0, 1, 0, -sy, 0, cy}};
            Mat3 Rz{{cz, -sz, 0, sz, cz, 0, 0, 0, 1}};
            return mulm(Rz, mulm(Ry, Rx));
        }
    };

    // A static object's placement: world = pos + scale * (R * local).
    struct Transform
    {
        Vector3 pos;
        Mat3 rot;
        float scale = 1.f;

        Transform() = default;

        /// A non-positive or non-finite scale is nonsense, and the damage it does is
        /// silent: worldToLocal() divides by it, so the point becomes inf or NaN, and
        /// every later comparison against a NaN is false -- a ray simply misses the
        /// model instead of failing. Clamped once here, where a model is placed, rather
        /// than tested on every query.
        Transform(const Vector3& pos, const Mat3& rot, float scale)
            : pos(pos), rot(rot),
              scale((scale > 0.0f && Geometry::isFinite(scale)) ? scale : 1.0f) {}

        Vector3 localToWorld(const Vector3& p) const
        {
            return pos + rot.mul(p) * scale;
        }

        Vector3 worldToLocal(const Vector3& p) const
        {
            return rot.transpose().mul(p - pos) * (1.f / scale);
        }

        // Transform a direction (vector) from world space to local space.
        // No translation -- rotation plus inverse scale only.
        Vector3 worldToLocalDirection(const Vector3& dir) const
        {
            return rot.transpose().mul(dir) * (1.f / scale);
        }

        // Inverse of the above (kept for symmetry).
        Vector3 localToWorldDirection(const Vector3& dir) const
        {
            return rot.mul(dir) * scale;
        }
    };

    // Triangle by three points.
    struct Tri
    {
        Vector3 a, b, c;
    };

    // Moller-Trumbore. Returns the ray parameter t (>= 0) of the hit, or nullopt.
    // Two-sided (we hit floors and ceilings alike); the caller decides what to keep.
    inline std::optional<float> rayTri(const Vector3& o, const Vector3& d, const Tri& t)
    {
        constexpr float kEps = 1e-6f;   // ray/triangle parallel tolerance
        constexpr float kUVEps = 1e-5f; // tolerance for barycentric coords

        const Vector3 e1 = t.b - t.a;
        const Vector3 e2 = t.c - t.a;
        const Vector3 p = cross(d, e2);
        const float det = dot(e1, p);

        if (std::fabs(det) < kEps)
        {
            return std::nullopt; // near-parallel
        }

        const float invDet = 1.0f / det;
        const Vector3 tv = o - t.a;

        float u = dot(tv, p) * invDet;
        if (u < -kUVEps || u > 1.0f + kUVEps)
        {
            return std::nullopt;
        }

        const Vector3 q = cross(tv, e1);
        float v = dot(d, q) * invDet;
        if (v < -kUVEps || u + v > 1.0f + kUVEps)
        {
            return std::nullopt;
        }

        float tt = dot(e2, q) * invDet;

        // Reject hits behind the ray origin.
        if (tt < -kEps)
        {
            return std::nullopt;
        }

        return tt;
    }
}
