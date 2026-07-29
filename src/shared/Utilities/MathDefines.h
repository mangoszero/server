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

#ifndef MANGOS_MATHDEFINES_H
#define MANGOS_MATHDEFINES_H

#include <cmath>

// M_PI is POSIX, not ISO C++, so MSVC does not define it without _USE_MATH_DEFINES.
#ifndef M_PI
#  define M_PI 3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F float(M_PI)
#endif

/**
 * @brief Replace a non-finite float with zero.
 *
 * Positions and orientations arriving from the client are trusted nowhere else;
 * this keeps a NaN or infinity out of the movement maths, where it would spread
 * to everything it touches and be very hard to trace back.
 *
 * std::isfinite replaces the old finite()/_finite() pair, which needed a
 * per-compiler #define to paper over the naming difference.
 */
inline float finiteAlways(float f)
{
    return std::isfinite(f) ? f : 0.0f;
}

#endif
