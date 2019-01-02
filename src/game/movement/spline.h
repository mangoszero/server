/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOSSERVER_SPLINE_H
#define MANGOSSERVER_SPLINE_H

#include "typedefs.h"
#include <G3D/Vector3.h>
#include <limits>

namespace Movement
{

    /**
     * @brief
     *
     */
    class SplineBase
    {
        public:
            /**
             * @brief
             *
             */
            typedef int index_type;
            /**
             * @brief
             *
             */
            typedef std::vector<Vector3> ControlArray;

            /**
             * @brief
             *
             */
            enum EvaluationMode
            {
                ModeLinear,
                ModeCatmullrom,
                ModeBezier3_Unused,
                UninitializedMode,
                ModesEnd
            };

        protected:
            ControlArray points; /**< TODO */

            index_type index_lo; /**< TODO */
            index_type index_hi; /**< TODO */

            uint8 m_mode; /**< TODO */
            bool cyclic; /**< TODO */

            /**
             * @brief
             *
             */
            enum
            {
                // could be modified, affects segment length evaluation precision
                // lesser value saves more performance in cost of lover precision
                // minimal value is 1
                // client's value is 20, blizzs use 2-3 steps to compute length
                STEPS_PER_SEGMENT = 3
            };
            static_assert(STEPS_PER_SEGMENT > 0, "shouldn't be lesser than 1");

        protected:
            /**
             * @brief
             *
             * @param index_type
             * @param float
             * @param
             */
            void EvaluateLinear(index_type, float, Vector3&) const;
            /**
             * @brief
             *
             * @param index_type
             * @param float
             * @param
             */
            void EvaluateCatmullRom(index_type, float, Vector3&) const;
            /**
             * @brief
             *
             * @param index_type
             * @param float
             * @param
             */
            void EvaluateBezier3(index_type, float, Vector3&) const;
            /**
             * @brief
             *
             */
            typedef void (SplineBase::*EvaluationMethtod)(index_type, float, Vector3&) const;
            static EvaluationMethtod evaluators[ModesEnd]; /**< TODO */

            /**
             * @brief
             *
             * @param index_type
             * @param float
             * @param
             */
            void EvaluateDerivativeLinear(index_type, float, Vector3&) const;
            /**
             * @brief
             *
             * @param index_type
             * @param float
             * @param
             */
            void EvaluateDerivativeCatmullRom(index_type, float, Vector3&) const;
            /**
             * @brief
             *
             * @param index_type
             * @param float
             * @param
             */
            void EvaluateDerivativeBezier3(index_type, float, Vector3&) const;
            static EvaluationMethtod derivative_evaluators[ModesEnd]; /**< TODO */

            /**
             * @brief
             *
             * @param index_type
             * @return float
             */
            float SegLengthLinear(index_type) const;
            /**
             * @brief
             *
             * @param index_type
             * @return float
             */
            float SegLengthCatmullRom(index_type) const;
            /**
             * @brief
             *
             * @param index_type
             * @return float
             */
            float SegLengthBezier3(index_type) const;
            /**
             * @brief
             *
             */
            typedef float(SplineBase::*SegLenghtMethtod)(index_type) const;
            static SegLenghtMethtod seglengths[ModesEnd]; /**< TODO */

            /**
             * @brief
             *
             * @param
             * @param index_type
             * @param bool
             * @param index_type
             */
            void InitLinear(const Vector3*, index_type, bool, index_type);
            /**
             * @brief
             *
             * @param
             * @param index_type
             * @param bool
             * @param index_type
             */
            void InitCatmullRom(const Vector3*, index_type, bool, index_type);
            /**
             * @brief
             *
             * @param
             * @param index_type
             * @param bool
             * @param index_type
             */
            void InitBezier3(const Vector3*, index_type, bool, index_type);
            /**
             * @brief
             *
             */
            typedef void (SplineBase::*InitMethtod)(const Vector3*, index_type, bool, index_type);
            static InitMethtod initializers[ModesEnd]; /**< TODO */

            /**
             * @brief
             *
             */
            void UninitializedSpline() const { MANGOS_ASSERT(false);}

        public:

            /**
             * @brief
             *
             */
            explicit SplineBase() : index_lo(0), index_hi(0), m_mode(UninitializedMode), cyclic(false) {}

            /**
             * @brief Calculates the position for given segment Idx, and percent of segment length t
             *
             * @param Idx spline segment index, should be in range [first, last)
             * @param u percent of segment length, assumes that t in range [0, 1]
             * @param c
             */
            void evaluate_percent(index_type Idx, float u, Vector3& c) const {(this->*evaluators[m_mode])(Idx, u, c);}

            /**
             * @brief Calculates derivation in index Idx, and percent of segment length t
             *
             * @param Idx spline segment index, should be in range [first, last)
             * @param u percent of spline segment length, assumes that t in range [0, 1]
             * @param hermite
             */
            void evaluate_derivative(index_type Idx, float u, Vector3& hermite) const {(this->*derivative_evaluators[m_mode])(Idx, u, hermite);}

            /**
             * @brief Bounds for spline indexes. All indexes should be in range [first, last).
             *
             * @return index_type
             */
            index_type first() const { return index_lo;}
            /**
             * @brief
             *
             * @return index_type
             */
            index_type last()  const { return index_hi;}

            /**
             * @brief
             *
             * @return bool
             */
            bool empty() const { return index_lo == index_hi;}
            /**
             * @brief
             *
             * @return EvaluationMode
             */
            EvaluationMode mode() const { return (EvaluationMode)m_mode;}
            /**
             * @brief
             *
             * @return bool
             */
            bool isCyclic() const { return cyclic;}

            /**
             * @brief
             *
             * @return const ControlArray
             */
            const ControlArray& getPoints() const { return points;}
            /**
             * @brief
             *
             * @return index_type
             */
            index_type getPointCount() const { return points.size();}
            /**
             * @brief
             *
             * @param i
             * @return const Vector3
             */
            const Vector3& getPoint(index_type i) const { return points[i];}

            /**
             * @brief Initializes spline. Don't call other methods while spline not initialized.
             *
             * @param controls
             * @param count
             * @param m
             */
            void init_spline(const Vector3* controls, index_type count, EvaluationMode m);
            /**
             * @brief
             *
             * @param controls
             * @param count
             * @param m
             * @param cyclic_point
             */
            void init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point);

            /**
             * @brief As i can see there are a lot of ways how spline can be
             * initialized would be no harm to have some custom initializers.
             *
             * @param initializer
             */
            template<class Init> inline void init_spline(Init& initializer)
            {
                initializer(m_mode, cyclic, points, index_lo, index_hi);
            }

            /**
             * @brief
             *
             */
            void clear();

            /**
             * @brief Calculates distance between [i; i+1] points, assumes that index i is in bounds.
             *
             * @param i
             * @return float
             */
            float SegLength(index_type i) const { return (this->*seglengths[m_mode])(i);}

            /**
             * @brief
             *
             * @return std::string
             */
            std::string ToString() const;
    };

    template<typename length_type>
    /**
     * @brief
     *
     */
    class Spline : public SplineBase
    {
        public:
            /**
             * @brief
             *
             */
            typedef length_type LengthType;
            /**
             * @brief
             *
             */
            typedef std::vector<length_type> LengthArray;
        protected:

            LengthArray lengths; /**< TODO */

            /**
             * @brief
             *
             * @param length
             * @return index_type
             */
            index_type computeIndexInBounds(length_type length) const;
        public:

            /**
             * @brief
             *
             */
            explicit Spline() {}

            /**
             * @brief Calculates the position for given t
             *
             * @param t percent of spline's length, assumes that t in range [0, 1].
             * @param c
             */
            void evaluate_percent(float t, Vector3& c) const;

            /**
             * @brief Calculates derivation for given t
             *
             * @param t percent of spline's length, assumes that t in range [0, 1].
             * @param hermite
             */
            void evaluate_derivative(float t, Vector3& hermite) const;

            /**
             * @brief Calculates the position for given segment Idx, and percent of segment length t
             *
             * @param Idx spline segment index, should be in range [first, last).
             * @param u partial_segment_length / whole_segment_length
             * @param c
             */
            void evaluate_percent(index_type Idx, float u, Vector3& c) const { SplineBase::evaluate_percent(Idx, u, c);}

            /**
             * @brief Caclulates derivation for index Idx, and percent of segment length t
             *
             * @param Idx spline segment index, should be in range [first, last)
             * @param u percent of spline segment length, assumes that t in range [0, 1].
             * @param c
             */
            void evaluate_derivative(index_type Idx, float u, Vector3& c) const { SplineBase::evaluate_derivative(Idx, u, c);}

            /**
             * @brief
             *
             * @param t Assumes that t in range [0, 1]
             * @return index_type
             */
            index_type computeIndexInBounds(float t) const;
            /**
             * @brief
             *
             * @param t
             * @param out_idx
             * @param out_u
             */
            void computeIndex(float t, index_type& out_idx, float& out_u) const;

            /**
             * @brief Initializes spline. Don't call other methods while spline not initialized.
             *
             * @param controls
             * @param count
             * @param m
             */
            void init_spline(const Vector3* controls, index_type count, EvaluationMode m) { SplineBase::init_spline(controls, count, m);}
            /**
             * @brief
             *
             * @param controls
             * @param count
             * @param m
             * @param cyclic_point
             */
            void init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point) { SplineBase::init_cyclic_spline(controls, count, m, cyclic_point);}

            /**
             * @brief Initializes lengths with SplineBase::SegLength method.
             *
             */
            void initLengths();

            /**
             * @brief Initializes lengths in some custom way
             * Note that value returned by cacher must be greater or equal to previous value.
             *
             * @param cacher
             */
            template<class T> inline void initLengths(T& cacher)
            {
                index_type i = index_lo;
                lengths.resize(index_hi + 1);
                length_type prev_length = 0, new_length = 0;
                while (i < index_hi)
                {
                    new_length = cacher(*this, i);

                    if (new_length < 0)         // length overflowed, assign to max positive value (stop case only?)
                        new_length = std::numeric_limits<length_type>::max();

                    lengths[++i] = new_length;

                    MANGOS_ASSERT(prev_length <= new_length);
                    prev_length = new_length;
                }
            }

            /**
             * @brief Returns length of the whole spline.
             *
             * @return length_type
             */
            length_type length() const { return lengths[index_hi];}
            /**
             * @brief Returns length between given nodes.
             *
             * @param first
             * @param last
             * @return length_type
             */
            length_type length(index_type first, index_type last) const { return lengths[last] - lengths[first];}
            /**
             * @brief
             *
             * @param Idx
             * @return length_type
             */
            length_type length(index_type Idx) const { return lengths[Idx];}

            /**
             * @brief
             *
             * @param i
             * @param length
             */
            void set_length(index_type i, length_type length) { lengths[i] = length;}
            /**
             * @brief
             *
             */
            void clear();
    };
}

#include "spline.impl.h"

#endif // MANGOSSERVER_SPLINE_H
