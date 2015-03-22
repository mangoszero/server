/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef VEC3D_H
#define VEC3D_H

#include <iostream>
#include <cmath>

/**
 * @brief
 *
 */
class Vec3D
{
    public:
        float x, y, z; /**< TODO */

        /**
         * @brief
         *
         * @param x0
         * @param y0
         * @param z0
         */
        Vec3D(float x0 = 0.0f, float y0 = 0.0f, float z0 = 0.0f) : x(x0), y(y0), z(z0) {}

        /**
         * @brief
         *
         * @param v
         */
        Vec3D(const Vec3D& v) : x(v.x), y(v.y), z(v.z) {}

        /**
         * @brief
         *
         * @param v
         * @return Vec3D &operator
         */
        Vec3D& operator= (const Vec3D& v)
        {
            x = v.x;
            y = v.y;
            z = v.z;
            return *this;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec3D operator
         */
        Vec3D operator+ (const Vec3D& v) const
        {
            Vec3D r(x + v.x, y + v.y, z + v.z);
            return r;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec3D operator
         */
        Vec3D operator- (const Vec3D& v) const
        {
            Vec3D r(x - v.x, y - v.y, z - v.z);
            return r;
        }

        /**
         * @brief
         *
         * @param v
         * @return float operator
         */
        float operator* (const Vec3D& v) const
        {
            return x * v.x + y * v.y + z * v.z;
        }

        /**
         * @brief
         *
         * @param d
         * @return Vec3D operator
         */
        Vec3D operator* (float d) const
        {
            Vec3D r(x * d, y * d, z * d);
            return r;
        }

        /**
         * @brief
         *
         * @param d
         * @param v
         * @return Vec3D operator
         */
        friend Vec3D operator* (float d, const Vec3D& v)
        {
            return v * d;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec3D operator
         */
        Vec3D operator% (const Vec3D& v) const
        {
            Vec3D r(y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x);
            return r;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec3D &operator
         */
        Vec3D& operator+= (const Vec3D& v)
        {
            x += v.x;
            y += v.y;
            z += v.z;
            return *this;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec3D &operator
         */
        Vec3D& operator-= (const Vec3D& v)
        {
            x -= v.x;
            y -= v.y;
            z -= v.z;
            return *this;
        }

        /**
         * @brief
         *
         * @param d
         * @return Vec3D &operator
         */
        Vec3D& operator*= (float d)
        {
            x *= d;
            y *= d;
            z *= d;
            return *this;
        }

        /**
         * @brief
         *
         * @return float
         */
        float lengthSquared() const
        {
            return x * x + y * y + z * z;
        }

        /**
         * @brief
         *
         * @return float
         */
        float length() const
        {
            return sqrt(x * x + y * y + z * z);
        }

        /**
         * @brief
         *
         * @return Vec3D
         */
        Vec3D& normalize()
        {
            this->operator*= (1.0f / length());
            return *this;
        }

        /**
         * @brief
         *
         * @return Vec3D operator
         */
        Vec3D operator~() const
        {
            Vec3D r(*this);
            r.normalize();
            return r;
        }

        /**
         * @brief
         *
         * @param in
         * @param v
         * @return std::istream &operator >>
         */
        friend std::istream& operator>>(std::istream& in, Vec3D& v)
        {
            in >> v.x >> v.y >> v.z;
            return in;
        }

        /**
         * @brief
         *
         * @param out
         * @param v
         * @return std::ostream &operator
         */
        friend std::ostream& operator<<(std::ostream& out, const Vec3D& v)
        {
            out << v.x << " " << v.y << " " << v.z;
            return out;
        }

        /**
         * @brief
         *
         * @return operator float
         */
        operator float* ()
        {
            return (float*)this;
        }
};


/**
 * @brief
 *
 */
class Vec2D
{
    public:
        float x, y; /**< TODO */

        /**
         * @brief
         *
         * @param x0
         * @param y0
         */
        Vec2D(float x0 = 0.0f, float y0 = 0.0f) : x(x0), y(y0) {}

        /**
         * @brief
         *
         * @param v
         */
        Vec2D(const Vec2D& v) : x(v.x), y(v.y) {}

        /**
         * @brief
         *
         * @param v
         * @return Vec2D &operator
         */
        Vec2D& operator= (const Vec2D& v)
        {
            x = v.x;
            y = v.y;
            return *this;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec2D operator
         */
        Vec2D operator+ (const Vec2D& v) const
        {
            Vec2D r(x + v.x, y + v.y);
            return r;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec2D operator
         */
        Vec2D operator- (const Vec2D& v) const
        {
            Vec2D r(x - v.x, y - v.y);
            return r;
        }

        /**
         * @brief
         *
         * @param v
         * @return float operator
         */
        float operator* (const Vec2D& v) const
        {
            return x * v.x + y * v.y;
        }

        /**
         * @brief
         *
         * @param d
         * @return Vec2D operator
         */
        Vec2D operator* (float d) const
        {
            Vec2D r(x * d, y * d);
            return r;
        }

        /**
         * @brief
         *
         * @param d
         * @param v
         * @return Vec2D operator
         */
        friend Vec2D operator* (float d, const Vec2D& v)
        {
            return v * d;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec2D &operator
         */
        Vec2D& operator+= (const Vec2D& v)
        {
            x += v.x;
            y += v.y;
            return *this;
        }

        /**
         * @brief
         *
         * @param v
         * @return Vec2D &operator
         */
        Vec2D& operator-= (const Vec2D& v)
        {
            x -= v.x;
            y -= v.y;
            return *this;
        }

        /**
         * @brief
         *
         * @param d
         * @return Vec2D &operator
         */
        Vec2D& operator*= (float d)
        {
            x *= d;
            y *= d;
            return *this;
        }

        /**
         * @brief
         *
         * @return float
         */
        float lengthSquared() const
        {
            return x * x + y * y;
        }

        /**
         * @brief
         *
         * @return float
         */
        float length() const
        {
            return sqrt(x * x + y * y);
        }

        /**
         * @brief
         *
         * @return Vec2D
         */
        Vec2D& normalize()
        {
            this->operator*= (1.0f / length());
            return *this;
        }

        /**
         * @brief
         *
         * @return Vec2D operator
         */
        Vec2D operator~() const
        {
            Vec2D r(*this);
            r.normalize();
            return r;
        }


        /**
         * @brief
         *
         * @param in
         * @param v
         * @return std::istream &operator >>
         */
        friend std::istream& operator>>(std::istream& in, Vec2D& v)
        {
            in >> v.x >> v.y;
            return in;
        }

        /**
         * @brief
         *
         * @return operator float
         */
        operator float* ()
        {
            return (float*)this;
        }
};

/**
 * @brief
 *
 * @param x0
 * @param y0
 * @param x
 * @param y
 * @param angle
 */
inline void rotate(float x0, float y0, float* x, float* y, float angle)
{
    float xa = *x - x0, ya = *y - y0;
    *x = xa * cosf(angle) - ya * sinf(angle) + x0;
    *y = xa * sinf(angle) + ya * cosf(angle) + y0;
}

#endif
