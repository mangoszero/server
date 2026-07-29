#pragma once

// 2D float vector. A minimal, self-contained replacement for G3D::Vector2 (the server
// only ever aliased the type through Movement::Vector2). See [[project_g3d_removal]].

#include <cmath>

namespace Geometry
{
    class Vector2
    {
        public:
            float x, y;

            Vector2() : x(0.0f), y(0.0f) {}
            Vector2(float _x, float _y) : x(_x), y(_y) {}

            const float& operator[](int i) const { return reinterpret_cast<const float*>(this)[i]; }
            float& operator[](int i) { return reinterpret_cast<float*>(this)[i]; }

            bool operator==(const Vector2& v) const { return x == v.x && y == v.y; }
            bool operator!=(const Vector2& v) const { return x != v.x || y != v.y; }

            Vector2 operator+(const Vector2& v) const { return Vector2(x + v.x, y + v.y); }
            Vector2 operator-(const Vector2& v) const { return Vector2(x - v.x, y - v.y); }
            Vector2 operator*(float s) const { return Vector2(x * s, y * s); }
            Vector2 operator-() const { return Vector2(-x, -y); }

            float dot(const Vector2& v) const { return x * v.x + y * v.y; }
            float squaredLength() const { return x * x + y * y; }
            float length() const { return std::sqrt(x * x + y * y); }

            static const Vector2& zero() { static const Vector2 v(0.0f, 0.0f); return v; }
    };

    inline Vector2 operator*(float s, const Vector2& v) { return v * s; }
}
