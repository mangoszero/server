#pragma once

// 3D float vector. Self-contained replacement for G3D::Vector3, ported so the arithmetic
// (dot/cross/length/lerp/...) matches the former g3dlite behavior bit-for-bit. Only the
// subset the game core actually used is provided. See [[project_g3d_removal]].
//
// Layout note: x,y,z must stay the first (and only) data members with no virtuals.
// operator[] and Quat::imag() reinterpret_cast over them, and -- the one that is
// invisible if it breaks -- packet_builder bulk-appends a whole spline straight into
// a movement packet with data.append<Vector3>(&spline.getPoint(2), count). That is a
// memcpy of count * sizeof(Vector3) onto the wire. Add a member, a base or a virtual
// and it still compiles, still links, and the client receives garbage paths.
// The assertions at the bottom of this file are what make that unwritable.

#include "Geometry/GeometryMath.h"
#include "Geometry/Vector2.h"

#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <type_traits>

namespace Geometry
{
    class Vector3
    {
        public:
            float x, y, z;

            Vector3() : x(0.0f), y(0.0f), z(0.0f) {}
            Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}
            explicit Vector3(const float coordinate[3]) : x(coordinate[0]), y(coordinate[1]), z(coordinate[2]) {}
            explicit Vector3(const Vector2& v, float _z) : x(v.x), y(v.y), z(_z) {}

            const float& operator[](int i) const { return reinterpret_cast<const float*>(this)[i]; }
            float& operator[](int i) { return reinterpret_cast<float*>(this)[i]; }

            // comparison
            bool operator==(const Vector3& v) const { return x == v.x && y == v.y && z == v.z; }
            bool operator!=(const Vector3& v) const { return x != v.x || y != v.y || z != v.z; }
            bool fuzzyEq(const Vector3& other) const { return Geometry::fuzzyEq((*this - other).squaredMagnitude(), 0); }
            bool fuzzyNe(const Vector3& other) const { return Geometry::fuzzyNe((*this - other).squaredMagnitude(), 0); }

            // arithmetic
            Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
            Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
            Vector3 operator*(const Vector3& v) const { return Vector3(x * v.x, y * v.y, z * v.z); }
            Vector3 operator/(const Vector3& v) const { return Vector3(x / v.x, y / v.y, z / v.z); }
            Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
            Vector3 operator/(float s) const { return *this * (1.0f / s); }
            Vector3 operator-() const { return Vector3(-x, -y, -z); }

            Vector3& operator+=(const Vector3& v) { x += v.x; y += v.y; z += v.z; return *this; }
            Vector3& operator-=(const Vector3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
            Vector3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
            Vector3& operator/=(float s) { return *this *= (1.0f / s); }
            Vector3& operator*=(const Vector3& v) { x *= v.x; y *= v.y; z *= v.z; return *this; }
            Vector3& operator/=(const Vector3& v) { x /= v.x; y /= v.y; z /= v.z; return *this; }

            float dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }

            Vector3 cross(const Vector3& v) const
            {
                return Vector3(y * v.z - z * v.y,
                               z * v.x - x * v.z,
                               x * v.y - y * v.x);
            }

            float squaredMagnitude() const { return x * x + y * y + z * z; }
            float squaredLength() const { return squaredMagnitude(); }
            float magnitude() const { return std::sqrt(x * x + y * y + z * z); }
            float length() const { return magnitude(); }

            Vector3 direction() const
            {
                const float invSqrt = 1.0f / std::sqrt(squaredMagnitude());
                return Vector3(x * invSqrt, y * invSqrt, z * invSqrt);
            }
            Vector3 unit() const { return direction(); }

            bool isZero() const { return Geometry::fuzzyEq(squaredMagnitude(), 0.0f); }
            bool isUnit() const { return Geometry::fuzzyEq(squaredMagnitude(), 1.0f); }
            bool isFinite() const { return Geometry::isFinite(x) && Geometry::isFinite(y) && Geometry::isFinite(z); }

            Vector3 lerp(const Vector3& v, float alpha) const { return (*this) + (v - *this) * alpha; }

            Vector3 min(const Vector3& v) const
            {
                return Vector3(v.x < x ? v.x : x, v.y < y ? v.y : y, v.z < z ? v.z : z);
            }

            Vector3 max(const Vector3& v) const
            {
                return Vector3(v.x > x ? v.x : x, v.y > y ? v.y : y, v.z > z ? v.z : z);
            }

            float sum() const { return x + y + z; }
            float average() const { return sum() / 3.0f; }

            std::string toString() const
            {
                char buffer[64];
                snprintf(buffer, sizeof(buffer), "(%g, %g, %g)", x, y, z);
                return std::string(buffer);
            }

            // Special values.
            static const Vector3& zero()  { static const Vector3 v(0, 0, 0); return v; }
            static const Vector3& one()   { static const Vector3 v(1, 1, 1); return v; }
            static const Vector3& unitX() { static const Vector3 v(1, 0, 0); return v; }
            static const Vector3& unitY() { static const Vector3 v(0, 1, 0); return v; }
            static const Vector3& unitZ() { static const Vector3 v(0, 0, 1); return v; }
    };

    inline Vector3 operator*(float s, const Vector3& v) { return v * s; }

    static_assert(sizeof(Vector3) == 3 * sizeof(float),
                  "Vector3 is memcpy'd into movement packets; it must be three bare floats");
    static_assert(std::is_standard_layout<Vector3>::value,
                  "Vector3 is memcpy'd into movement packets; it must be standard layout");
    static_assert(offsetof(Vector3, x) == 0 && offsetof(Vector3, y) == sizeof(float) &&
                  offsetof(Vector3, z) == 2 * sizeof(float),
                  "Vector3 members must stay in x,y,z order with no padding");
}
