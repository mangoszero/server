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

namespace Movement
{
    /**
     * @brief Evaluates the spline at a given percentage of its length.
     * @param t The percentage of the spline's length (0.0 to 1.0).
     * @param c The resulting position on the spline.
     */
    template<typename length_type> void Spline<length_type>::evaluate_percent(float t, Vector3& c) const
    {
        index_type Index;
        float u;
        computeIndex(t, Index, u);
        evaluate_percent(Index, u, c);
    }

    /**
     * @brief Evaluates the derivative of the spline at a given percentage of its length.
     * @param t The percentage of the spline's length (0.0 to 1.0).
     * @param hermite The resulting derivative on the spline.
     */
    template<typename length_type> void Spline<length_type>::evaluate_derivative(float t, Vector3& hermite) const
    {
        index_type Index;
        float u;
        computeIndex(t, Index, u);
        evaluate_derivative(Index, u, hermite);
    }

    /**
     * @brief Computes the index of the segment that contains the given length.
     * @param length_ The length along the spline.
     * @return SplineBase::index_type The index of the segment.
     */
    template<typename length_type> SplineBase::index_type Spline<length_type>::computeIndexInBounds(length_type length_) const
    {
        index_type i = index_lo;
        index_type N = index_hi;
        while (i + 1 < N && lengths[i + 1] < length_)
        {
            ++i;
        }

        return i;
    }

    /**
     * @brief Computes the index and the local parameter for a given percentage of the spline's length.
     * @param t The percentage of the spline's length (0.0 to 1.0).
     * @param index The resulting index of the segment.
     * @param u The resulting local parameter within the segment.
     */
    template<typename length_type> void Spline<length_type>::computeIndex(float t, index_type& index, float& u) const
    {
        MANGOS_ASSERT(t >= 0.f && t <= 1.f);
        length_type length_ = t * length();
        index = computeIndexInBounds(length_);
        MANGOS_ASSERT(index < index_hi);
        u = (length_ - length(index)) / (float)length(index, index + 1);
    }

    /**
     * @brief Computes the index of the segment that contains the given percentage of the spline's length.
     * @param t The percentage of the spline's length (0.0 to 1.0).
     * @return SplineBase::index_type The index of the segment.
     */
    template<typename length_type> SplineBase::index_type Spline<length_type>::computeIndexInBounds(float t) const
    {
        MANGOS_ASSERT(t >= 0.f && t <= 1.f);
        return computeIndexInBounds(t * length());
    }

    /**
     * @brief Initializes the lengths of the segments of the spline.
     */
    template<typename length_type> void Spline<length_type>::initLengths()
    {
        index_type i = index_lo;
        length_type length = 0;
        lengths.resize(index_hi + 1);
        while (i < index_hi)
        {
            length += SegLength(i);
            lengths[++i] = length;
        }
    }

    /**
     * @brief Clears the spline data.
     */
    template<typename length_type> void Spline<length_type>::clear()
    {
        SplineBase::clear();
        lengths.clear();
    }
}

