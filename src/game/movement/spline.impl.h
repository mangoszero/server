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

namespace Movement
{
    /**
     * @brief
     *
     * @param t
     * @param c
     */
    template<typename length_type> void Spline<length_type>::evaluate_percent(float t, Vector3& c) const
    {
        index_type Index;
        float u;
        computeIndex(t, Index, u);
        evaluate_percent(Index, u, c);
    }

    /**
     * @brief
     *
     * @param t
     * @param hermite
     */
    template<typename length_type> void Spline<length_type>::evaluate_derivative(float t, Vector3& hermite) const
    {
        index_type Index;
        float u;
        computeIndex(t, Index, u);
        evaluate_derivative(Index, u, hermite);
    }

    /**
     * @brief
     *
     * @param length_
     * @return SplineBase::index_type Spline<length_type>
     */
    template<typename length_type> SplineBase::index_type Spline<length_type>::computeIndexInBounds(length_type length_) const
    {
        // Temporary disabled: causes infinite loop with t = 1.f
        /*
            index_type hi = index_hi;
            index_type lo = index_lo;

            index_type i = lo + (float)(hi - lo) * t;

            while ((lengths[i] > length) || (lengths[i + 1] <= length))
            {
                if (lengths[i] > length)
                    hi = i - 1; // too big
                else if (lengths[i + 1] <= length)
                    lo = i + 1; // too small

                i = (hi + lo) / 2;
            }*/

        index_type i = index_lo;
        index_type N = index_hi;
        while (i + 1 < N && lengths[i + 1] < length_)
            { ++i; }

        return i;
    }

    /**
     * @brief
     *
     * @param t
     * @param index
     * @param u
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
     * @brief
     *
     * @param t
     * @return SplineBase::index_type Spline<length_type>
     */
    template<typename length_type> SplineBase::index_type Spline<length_type>::computeIndexInBounds(float t) const
    {
        MANGOS_ASSERT(t >= 0.f && t <= 1.f);
        return computeIndexInBounds(t * length());
    }

    /**
     * @brief
     *
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
     * @brief
     *
     */
    template<typename length_type> void Spline<length_type>::clear()
    {
        SplineBase::clear();
        lengths.clear();
    }
}
