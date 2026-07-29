#pragma once

// Homogeneous 4D float vector. Self-contained replacement for G3D::Vector4, ported to the
// subset the game core used (notably Vector4 * Matrix4, used by the spline evaluator).
// See [[project_g3d_removal]].

#include "Geometry/Vector3.h"
#include "Geometry/Matrix4.h"

#include <cmath>

namespace Geometry
{
    class Vector4
    {
        public:
            float x, y, z, w;

            Vector4() : x(0.0f), y(0.0f), z(0.0f), w(0.0f) {}
            Vector4(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}
            Vector4(const Vector3& v, float _w) : x(v.x), y(v.y), z(v.z), w(_w) {}

            float& operator[](int i) { return reinterpret_cast<float*>(this)[i]; }
            const float& operator[](int i) const { return reinterpret_cast<const float*>(this)[i]; }

            bool operator==(const Vector4& v) const { return x == v.x && y == v.y && z == v.z && w == v.w; }
            bool operator!=(const Vector4& v) const { return x != v.x || y != v.y || z != v.z || w != v.w; }

            Vector4 operator+(const Vector4& v) const { return Vector4(x + v.x, y + v.y, z + v.z, w + v.w); }
            Vector4 operator-(const Vector4& v) const { return Vector4(x - v.x, y - v.y, z - v.z, w - v.w); }
            Vector4 operator*(const Vector4& v) const { return Vector4(x * v.x, y * v.y, z * v.z, w * v.w); }
            Vector4 operator*(float s) const { return Vector4(x * s, y * s, z * s, w * s); }
            Vector4 operator-() const { return Vector4(-x, -y, -z, -w); }

            Vector4& operator+=(const Vector4& v) { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
            Vector4& operator-=(const Vector4& v) { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
            Vector4& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }

            /// Row-vector times matrix: result[i] = sum_j this[j] * M[j][i].
            Vector4 operator*(const Matrix4& M) const
            {
                Vector4 result;
                for (int i = 0; i < 4; ++i)
                {
                    result[i] = 0.0f;
                    for (int j = 0; j < 4; ++j)
                    {
                        result[i] += (*this)[j] * M[j][i];
                    }
                }
                return result;
            }

            float dot(const Vector4& v) const { return x * v.x + y * v.y + z * v.z + w * v.w; }
            float squaredLength() const { return x * x + y * y + z * z + w * w; }
            float length() const { return std::sqrt(squaredLength()); }

            Vector4 lerp(const Vector4& v, float alpha) const { return (*this) + (v - *this) * alpha; }

            static const Vector4& zero() { static const Vector4 v(0, 0, 0, 0); return v; }
    };

    inline Vector4 operator*(float s, const Vector4& v) { return v * s; }
}
