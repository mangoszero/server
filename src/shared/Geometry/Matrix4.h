#pragma once

// 4x4 row-major float matrix. Self-contained replacement for G3D::Matrix4, ported to the
// subset the game core used: 16-float construction and row indexing (the spline evaluator
// stores the Catmull-Rom / Bezier coefficient matrices and multiplies them by a Vector4).
// See [[project_g3d_removal]].

namespace Geometry
{
    class Matrix4
    {
        private:
            float elt[4][4];

        public:
            Matrix4()
            {
                for (int r = 0; r < 4; ++r)
                {
                    for (int c = 0; c < 4; ++c)
                    {
                        elt[r][c] = 0.0f;
                    }
                }
            }

            Matrix4(
                float r1c1, float r1c2, float r1c3, float r1c4,
                float r2c1, float r2c2, float r2c3, float r2c4,
                float r3c1, float r3c2, float r3c3, float r3c4,
                float r4c1, float r4c2, float r4c3, float r4c4)
            {
                elt[0][0] = r1c1; elt[0][1] = r1c2; elt[0][2] = r1c3; elt[0][3] = r1c4;
                elt[1][0] = r2c1; elt[1][1] = r2c2; elt[1][2] = r2c3; elt[1][3] = r2c4;
                elt[2][0] = r3c1; elt[2][1] = r3c2; elt[2][2] = r3c3; elt[2][3] = r3c4;
                elt[3][0] = r4c1; elt[3][1] = r4c2; elt[3][2] = r4c3; elt[3][3] = r4c4;
            }

            float* operator[](int r) { return elt[r]; }
            const float* operator[](int r) const { return elt[r]; }
    };
}
