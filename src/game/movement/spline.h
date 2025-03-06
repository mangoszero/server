/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
     * @brief Base class for handling splines.
     */
    class SplineBase
    {
        public:
            /**
             * @brief Type definition for index.
             */
            typedef int index_type;
            /**
             * @brief Type definition for control points array.
             */
            typedef std::vector<Vector3> ControlArray;

            /**
             * @brief Enumeration for spline evaluation modes.
             */
            enum EvaluationMode
            {
                ModeLinear,         /**< Linear interpolation mode */
                ModeCatmullrom,     /**< Catmull-Rom spline mode */
                ModeBezier3_Unused, /**< Bezier curve mode (unused) */
                UninitializedMode,  /**< Uninitialized mode */
                ModesEnd            /**< End of modes */
            };

        protected:
            ControlArray points; /**< Control points of the spline */

            index_type index_lo; /**< Lower bound index */
            index_type index_hi; /**< Upper bound index */

            uint8 m_mode; /**< Evaluation mode */
            bool cyclic; /**< Indicates if the spline is cyclic */

            /**
             * @brief Constants for segment length evaluation precision.
             */
            enum
            {
                // could be modified, affects segment length evaluation precision
                // lesser value saves more performance in cost of lower precision
                // minimal value is 1
                // client's value is 20, blizzs use 2-3 steps to compute length
                STEPS_PER_SEGMENT = 3
            };
            static_assert(STEPS_PER_SEGMENT > 0, "shouldn't be lesser than 1");

        protected:
            /**
             * @brief Evaluates the spline linearly.
             * @param index Index of the segment.
             * @param t Parameter for interpolation.
             * @param out Output vector for the evaluated point.
             */
            void EvaluateLinear(index_type index, float t, Vector3& out) const;
            /**
             * @brief Evaluates the spline using Catmull-Rom interpolation.
             * @param index Index of the segment.
             * @param t Parameter for interpolation.
             * @param out Output vector for the evaluated point.
             */
            void EvaluateCatmullRom(index_type index, float t, Vector3& out) const;
            /**
             * @brief Evaluates the spline using Bezier interpolation.
             * @param index Index of the segment.
             * @param t Parameter for interpolation.
             * @param out Output vector for the evaluated point.
             */
            void EvaluateBezier3(index_type index, float t, Vector3& out) const;
            /**
             * @brief Type definition for evaluation method pointers.
             */
            typedef void (SplineBase::*EvaluationMethtod)(index_type, float, Vector3&) const;
            static EvaluationMethtod evaluators[ModesEnd]; /**< Array of evaluation methods */

            /**
             * @brief Evaluates the derivative of the spline linearly.
             * @param index Index of the segment.
             * @param t Parameter for interpolation.
             * @param out Output vector for the evaluated derivative.
             */
            void EvaluateDerivativeLinear(index_type index, float t, Vector3& out) const;
            /**
             * @brief Evaluates the derivative of the spline using Catmull-Rom interpolation.
             * @param index Index of the segment.
             * @param t Parameter for interpolation.
             * @param out Output vector for the evaluated derivative.
             */
            void EvaluateDerivativeCatmullRom(index_type index, float t, Vector3& out) const;
            /**
             * @brief Evaluates the derivative of the spline using Bezier interpolation.
             * @param index Index of the segment.
             * @param t Parameter for interpolation.
             * @param out Output vector for the evaluated derivative.
             */
            void EvaluateDerivativeBezier3(index_type index, float t, Vector3& out) const;
            static EvaluationMethtod derivative_evaluators[ModesEnd]; /**< Array of derivative evaluation methods */

            /**
             * @brief Calculates the length of a linear segment.
             * @param index Index of the segment.
             * @return Length of the segment.
             */
            float SegLengthLinear(index_type index) const;
            /**
             * @brief Calculates the length of a Catmull-Rom segment.
             * @param index Index of the segment.
             * @return Length of the segment.
             */
            float SegLengthCatmullRom(index_type index) const;
            /**
             * @brief Calculates the length of a Bezier segment.
             * @param index Index of the segment.
             * @return Length of the segment.
             */
            float SegLengthBezier3(index_type index) const;
            /**
             * @brief Type definition for segment length method pointers.
             */
            typedef float(SplineBase::*SegLenghtMethtod)(index_type) const;
            static SegLenghtMethtod seglengths[ModesEnd]; /**< Array of segment length methods */

            /**
             * @brief Initializes the spline linearly.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param cyclic Indicates if the spline is cyclic.
             * @param cyclic_point Index of the cyclic point.
             */
            void InitLinear(const Vector3* controls, index_type count, bool cyclic, index_type cyclic_point);
            /**
             * @brief Initializes the spline using Catmull-Rom interpolation.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param cyclic Indicates if the spline is cyclic.
             * @param cyclic_point Index of the cyclic point.
             */
            void InitCatmullRom(const Vector3* controls, index_type count, bool cyclic, index_type cyclic_point);
            /**
             * @brief Initializes the spline using Bezier interpolation.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param cyclic Indicates if the spline is cyclic.
             * @param cyclic_point Index of the cyclic point.
             */
            void InitBezier3(const Vector3* controls, index_type count, bool cyclic, index_type cyclic_point);
            /**
             * @brief Type definition for initialization method pointers.
             */
            typedef void (SplineBase::*InitMethtod)(const Vector3*, index_type, bool, index_type);
            static InitMethtod initializers[ModesEnd]; /**< Array of initialization methods */

            /**
             * @brief Uninitialized spline handler.
             */
            void UninitializedSpline() const { MANGOS_ASSERT(false);}

        public:

            /**
             * @brief Constructor for SplineBase.
             */
            explicit SplineBase() : index_lo(0), index_hi(0), m_mode(UninitializedMode), cyclic(false) {}

            /**
             * @brief Calculates the position for given segment Idx, and percent of segment length t.
             * @param Idx Spline segment index, should be in range [first, last).
             * @param u Percent of segment length, assumes that t in range [0, 1].
             * @param c Output vector for the evaluated point.
             */
            void evaluate_percent(index_type Idx, float u, Vector3& c) const {(this->*evaluators[m_mode])(Idx, u, c);}

            /**
             * @brief Calculates derivation in index Idx, and percent of segment length t.
             * @param Idx Spline segment index, should be in range [first, last).
             * @param u Percent of spline segment length, assumes that t in range [0, 1].
             * @param hermite Output vector for the evaluated derivative.
             */
            void evaluate_derivative(index_type Idx, float u, Vector3& hermite) const {(this->*derivative_evaluators[m_mode])(Idx, u, hermite);}

            /**
             * @brief Bounds for spline indexes. All indexes should be in range [first, last).
             * @return Lower bound index.
             */
            index_type first() const { return index_lo;}
            /**
             * @brief Upper bound index.
             * @return Upper bound index.
             */
            index_type last()  const { return index_hi;}

            /**
             * @brief Checks if the spline is empty.
             * @return True if the spline is empty, false otherwise.
             */
            bool empty() const { return index_lo == index_hi;}
            /**
             * @brief Gets the evaluation mode of the spline.
             * @return Evaluation mode.
             */
            EvaluationMode mode() const { return (EvaluationMode)m_mode;}
            /**
             * @brief Checks if the spline is cyclic.
             * @return True if the spline is cyclic, false otherwise.
             */
            bool isCyclic() const { return cyclic;}

            /**
             * @brief Gets the control points of the spline.
             * @return Control points array.
             */
            const ControlArray& getPoints() const { return points;}
            /**
             * @brief Gets the number of control points.
             * @return Number of control points.
             */
            index_type getPointCount() const { return points.size();}
            /**
             * @brief Gets a specific control point.
             * @param i Index of the control point.
             * @return Control point at the specified index.
             */
            const Vector3& getPoint(index_type i) const { return points[i];}

            /**
             * @brief Initializes the spline. Don't call other methods while spline not initialized.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param m Evaluation mode.
             */
            void init_spline(const Vector3* controls, index_type count, EvaluationMode m);
            /**
             * @brief Initializes a cyclic spline.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param m Evaluation mode.
             * @param cyclic_point Index of the cyclic point.
             */
            void init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point);

            /**
             * @brief Initializes the spline with a custom initializer.
             * @param initializer Custom initializer.
             */
            template<class Init> inline void init_spline(Init& initializer)
            {
                initializer(m_mode, cyclic, points, index_lo, index_hi);
            }

            /**
             * @brief Clears the spline.
             */
            void clear();

            /**
             * @brief Calculates the length of a segment.
             * @param i Index of the segment.
             * @return Length of the segment.
             */
            float SegLength(index_type i) const { return (this->*seglengths[m_mode])(i);}

            /**
             * @brief Converts the spline to a string representation.
             * @return String representation of the spline.
             */
            std::string ToString() const;
    };

    template<typename length_type>
    /**
     * @brief Template class for handling splines with length information.
     * @tparam length_type Type for length information.
     */
    class Spline : public SplineBase
    {
        public:
            /**
             * @brief Type definition for length.
             */
            typedef length_type LengthType;
            /**
             * @brief Type definition for length array.
             */
            typedef std::vector<length_type> LengthArray;
        protected:

            LengthArray lengths; /**< Array of segment lengths */

            /**
             * @brief Computes the index within bounds for a given length.
             * @param length Length to compute the index for.
             * @return Computed index.
             */
            index_type computeIndexInBounds(length_type length) const;
        public:

            /**
             * @brief Constructor for Spline.
             */
            explicit Spline() {}

            /**
             * @brief Calculates the position for a given t.
             * @param t Percent of spline's length, assumes that t in range [0, 1].
             * @param c Output vector for the evaluated point.
             */
            void evaluate_percent(float t, Vector3& c) const;

            /**
             * @brief Calculates the derivative for a given t.
             * @param t Percent of spline's length, assumes that t in range [0, 1].
             * @param hermite Output vector for the evaluated derivative.
             */
            void evaluate_derivative(float t, Vector3& hermite) const;

            /**
             * @brief Calculates the position for a given segment Idx, and percent of segment length t.
             * @param Idx Spline segment index, should be in range [first, last).
             * @param u Partial segment length / whole segment length.
             * @param c Output vector for the evaluated point.
             */
            void evaluate_percent(index_type Idx, float u, Vector3& c) const { SplineBase::evaluate_percent(Idx, u, c);}

            /**
             * @brief Calculates the derivative for a given index Idx, and percent of segment length t.
             * @param Idx Spline segment index, should be in range [first, last).
             * @param u Percent of spline segment length, assumes that t in range [0, 1].
             * @param c Output vector for the evaluated derivative.
             */
            void evaluate_derivative(index_type Idx, float u, Vector3& c) const { SplineBase::evaluate_derivative(Idx, u, c);}

            /**
             * @brief Computes the index within bounds for a given t.
             * @param t Percent of spline's length, assumes that t in range [0, 1].
             * @return Computed index.
             */
            index_type computeIndexInBounds(float t) const;
            /**
             * @brief Computes the index and partial segment length for a given t.
             * @param t Percent of spline's length, assumes that t in range [0, 1].
             * @param out_idx Output index.
             * @param out_u Output partial segment length.
             */
            void computeIndex(float t, index_type& out_idx, float& out_u) const;

            /**
             * @brief Initializes the spline. Don't call other methods while spline not initialized.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param m Evaluation mode.
             */
            void init_spline(const Vector3* controls, index_type count, EvaluationMode m) { SplineBase::init_spline(controls, count, m);}
            /**
             * @brief Initializes a cyclic spline.
             * @param controls Array of control points.
             * @param count Number of control points.
             * @param m Evaluation mode.
             * @param cyclic_point Index of the cyclic point.
             */
            void init_cyclic_spline(const Vector3* controls, index_type count, EvaluationMode m, index_type cyclic_point) { SplineBase::init_cyclic_spline(controls, count, m, cyclic_point);}

            /**
             * @brief Initializes lengths with SplineBase::SegLength method.
             */
            void initLengths();

            /**
             * @brief Initializes lengths in a custom way.
             * Note that value returned by cacher must be greater or equal to previous value.
             * @param cacher Custom length initializer.
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
                    {
                        new_length = std::numeric_limits<length_type>::max();
                    }

                    lengths[++i] = new_length;

                    MANGOS_ASSERT(prev_length <= new_length);
                    prev_length = new_length;
                }
            }

            /**
             * @brief Returns the length of the whole spline.
             * @return Length of the spline.
             */
            length_type length() const { return lengths[index_hi];}
            /**
             * @brief Returns the length between given nodes.
             * @param first Index of the first node.
             * @param last Index of the last node.
             * @return Length between the nodes.
             */
            length_type length(index_type first, index_type last) const { return lengths[last] - lengths[first];}
            /**
             * @brief Returns the length of a specific segment.
             * @param Idx Index of the segment.
             * @return Length of the segment.
             */
            length_type length(index_type Idx) const { return lengths[Idx];}

            /**
             * @brief Sets the length of a specific segment.
             * @param i Index of the segment.
             * @param length Length to set.
             */
            void set_length(index_type i, length_type length) { lengths[i] = length;}
            /**
             * @brief Clears the spline.
             */
            void clear();
    };
}

#include "spline.impl.h"

#endif // MANGOSSERVER_SPLINE_H
