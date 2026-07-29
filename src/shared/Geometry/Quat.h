#pragma once

// Quaternion (not necessarily unit). Self-contained replacement for G3D::Quat, ported to
// the subset the game core used: game-object world rotation storage, quaternion
// composition (rotate a GO about Z), and unitize(). The multiply follows Watt & Watt
// exactly so results match the former g3dlite dependency. See [[project_g3d_removal]].
//
// Layout note: x,y,z,w must stay the first data members with no virtuals so imag()'s
// reinterpret_cast onto a Vector3 is valid.

#include "Geometry/GeometryMath.h"
#include "Geometry/Vector3.h"

#include <cstdint>

#include <cmath>

namespace Geometry
{
    class Quat
    {
        public:
            /// q = [sin(angle/2) * axis, cos(angle/2)]; (x,y,z) imaginary, w real.
            float x, y, z, w;

            /// Identity rotation (0, 0, 0, 1).
            Quat() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
            Quat(float _x, float _y, float _z, float _w) : x(_x), y(_y), z(_z), w(_w) {}

            /// Defaults to a pure-vector quaternion.
            Quat(const Vector3& v, float _w = 0.0f) : x(v.x), y(v.y), z(v.z), w(_w) {}

            const float& operator[](int i) const { return reinterpret_cast<const float*>(this)[i]; }
            float& operator[](int i) { return reinterpret_cast<float*>(this)[i]; }

            /// The imaginary part (x, y, z) aliased as a Vector3.
            const Vector3& imag() const { return *reinterpret_cast<const Vector3*>(this); }
            Vector3& imag() { return *reinterpret_cast<Vector3*>(this); }

            const float& real() const { return w; }
            float& real() { return w; }

            Quat operator-() const { return Quat(-x, -y, -z, -w); }
            Quat conj() const { return Quat(-x, -y, -z, w); }

            Quat operator*(float s) const { return Quat(x * s, y * s, z * s, w * s); }
            Quat operator/(float s) const { return Quat(x / s, y / s, z / s, w / s); }
            Quat& operator*=(float s) { x *= s; y *= s; z *= s; w *= s; return *this; }

            float dot(const Quat& other) const
            {
                return (x * other.x) + (y * other.y) + (z * other.z) + (w * other.w);
            }

            /// Quaternion multiplication (composition of rotations); does not commute.
            /// @cite Watt & Watt, page 360.
            Quat operator*(const Quat& other) const
            {
                const Vector3& v1 = imag();
                const Vector3& v2 = other.imag();
                float s1 = w;
                float s2 = other.w;

                return Quat(s1 * v2 + s2 * v1 + v1.cross(v2), s1 * s2 - v1.dot(v2));
            }

            /// Make unit length in place.
            void unitize() { *this *= rsq(dot(*this)); }

            float magnitude() const { return std::sqrt(dot(*this)); }

            bool isUnit(float tolerance = 1e-5f) const { return std::fabs(dot(*this) - 1.0f) < tolerance; }
    };

    inline Quat operator*(float s, const Quat& q) { return q * s; }

    /// The yaw a rotation carries, in [0, 2*PI) -- the only component a server-side
    /// facing has, since everything the core places stands upright.
    inline float YawOf(const Quat& q)
    {
        const double t1 = +2.0 * (double(q.w) * q.z + double(q.x) * q.y);
        const double t2 = +1.0 - 2.0 * (double(q.y) * q.y + double(q.z) * q.z);
        const float yaw = float(std::atan2(t1, t2));
        return wrap(yaw, 0.0f, 2.0f * pif());
    }

    /// The rotation of `angle` radians about `axis`. Standard convention:
    /// (axis.unit() * sin(angle/2), cos(angle/2)).
    inline Quat FromAxisAngle(const Vector3& axis, float angle)
    {
        const Vector3 u = axis.unit();
        const float s = std::sin(angle * 0.5f);
        return Quat(u.x * s, u.y * s, u.z * s, std::cos(angle * 0.5f));
    }

    /**
     * @brief The rotation Rz(z) * Ry(y) * Rx(x), as G3D's Matrix3-to-Quat produced it.
     *
     * A direct port, including which REPRESENTATIVE it picks. A quaternion and its
     * negation are the same rotation, so the composed product -(qz*qy*qx) is equally
     * correct geometrically -- but G3D chooses its sign from the largest diagonal
     * element of the matrix, and that disagrees with the product's sign for some inputs
     * (the identity is one). Measured against g3dlite before this was written, not
     * derived: two of seven sample angles differed.
     *
     * It does not change what the client draws, because PackRotation folds the sign of
     * w into each axis. It does change the raw components anything else reads, and
     * "probably equivalent" is not a thing to leave in a wire format.
     */
    inline Quat FromEulerAnglesZYX(float z, float y, float x)
    {
        const float cz = std::cos(z), sz = std::sin(z);
        const float cy = std::cos(y), sy = std::sin(y);
        const float cx = std::cos(x), sx = std::sin(x);

        // Rz * (Ry * Rx), row-major, exactly as Matrix3::fromEulerAnglesZYX builds it.
        const float r[3][3] = {
            { cz * cy,  cz * sy * sx - sz * cx,  cz * sy * cx + sz * sx },
            { sz * cy,  sz * sy * sx + cz * cx,  sz * sy * cx - cz * sx },
            { -sy,      cy * sx,                 cy * cx                },
        };

        static const int plus1mod3[] = { 1, 2, 0 };
        int i = (r[1][1] > r[0][0]) ? 1 : 0;
        i = (r[2][2] > r[i][i]) ? 2 : i;
        const int j = plus1mod3[i];
        const int k = plus1mod3[j];

        Quat q;
        float* v = &q.x;
        v[i] = float(((r[j][j] + r[k][k]) - r[i][i]) - 1.0);
        q.w  = (r[j][k] - r[k][j]);
        v[j] = -(r[i][j] + r[j][i]);
        v[k] = -(r[i][k] + r[k][i]);

        const float len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
        if (len > 0.00001f)
        {
            q *= 1.0f / len;
        }
        else
        {
            q = Quat(0.0f, 0.0f, 0.0f, 1.0f);
        }
        return q;
    }

    /// A rotation squeezed into the 3x21-bit field the 3.3.5 client reads
    /// (GAMEOBJECT_ROTATION). The sign of w is folded into each axis.
    inline int64_t PackRotation(const Quat& q)
    {
        enum
        {
            PACK_COEFF_YZ = 1 << 20,
            PACK_COEFF_X = 1 << 21,
        };

        const int sign = (q.w >= 0.0f ? 1 : -1);
        const int64_t x = int64_t(int32_t(q.x * PACK_COEFF_X) * sign & ((1 << 22) - 1));
        const int64_t y = int64_t(int32_t(q.y * PACK_COEFF_YZ) * sign & ((1 << 21) - 1));
        const int64_t z = int64_t(int32_t(q.z * PACK_COEFF_YZ) * sign & ((1 << 21) - 1));
        return z | (y << 21) | (x << 42);
    }
}
