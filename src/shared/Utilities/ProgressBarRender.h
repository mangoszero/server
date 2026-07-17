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

#ifndef MANGOSSERVER_PROGRESSBARRENDER_H
#define MANGOSSERVER_PROGRESSBARRENDER_H

/**
 * @file ProgressBarRender.h
 * @brief Pure, dependency-free byte rendering for the console progress bar.
 *
 * Builds the exact byte sequence that BarGoLink::init/step/~BarGoLink used to
 * emit with their per-fragment printf() calls, but as a single std::string per
 * visual update. Coalescing the redraw into one string lets the bar be handed
 * to the off-thread console writer as ONE atomic record, so it can no longer
 * tear against concurrently-drained log lines.
 *
 * This header is intentionally free of any Log / stdio dependency so the
 * rendering can be unit-tested in isolation and so offline tools that use a
 * progress bar do not gain a logging dependency.
 *
 * Byte-fidelity is asserted by test_progressbar_render.cpp.
 */

#include <cstddef>
#include <string>

namespace ProgressBarRender
{
    /**
     * @brief Initial bar: left edge, an empty field of @p indic_len cells, the
     *        right edge, " 0%", then a carriage-return that repositions the
     *        cursor and reprints the left edge.
     *
     * Legacy equivalent (Windows): printf("=") + indic_len*printf(" ") + printf("= 0%\r=").
     */
    inline std::string buildInit(int indic_len)
    {
        std::string s;
        s.reserve((std::size_t)indic_len + 8);
#ifdef _WIN32
        s += '=';
        s.append((std::size_t)indic_len, ' ');
        s += "= 0%\r=";
#else
        s += '[';
        s.append((std::size_t)indic_len, ' ');
        s += "] 0%\r[";
#endif
        return s;
    }

    /**
     * @brief One bar redraw at fill position @p n (of @p indic_len): carriage
     *        return, left edge, @p n filled cells, the remaining empty cells,
     *        the right edge, the percentage, then CR + left edge to reposition.
     *
     * The percentage is computed exactly as the legacy code did
     * ((int)((float)n / indic_len * 100)).
     *
     * Legacy equivalent (Windows):
     *   printf("\r=") + n*printf("=") + (indic_len-n)*printf(" ")
     *   + printf("= %i%%  \r=", percent).
     */
    inline std::string buildStep(int n, int indic_len)
    {
        const int percent = (int)(((float)n / (float)indic_len) * 100.0f);
        // Guard the empty-cell count: legacy code tolerated n > indic_len (its
        // second printf loop simply did not run), so clamp to 0 here to avoid a
        // std::size_t underflow in append(). For the normal range (0..indic_len) this
        // is identical to (indic_len - n).
        const std::size_t fills  = (n > 0) ? (std::size_t)n : 0;
        const std::size_t empties = (indic_len > n) ? (std::size_t)(indic_len - n) : 0;
        std::string s;
        s.reserve((std::size_t)indic_len + 16);
#ifdef _WIN32
        s += "\r=";
        s.append(fills, '=');
        s.append(empties, ' ');
        s += "= ";
        s += std::to_string(percent);
        s += "%  \r=";
#else
        s += "\r[";
        s.append(fills, '*');
        s.append(empties, ' ');
        s += "] ";
        s += std::to_string(percent);
        s += "%  \r[";
#endif
        return s;
    }

    /**
     * @brief Bar terminator: a single newline so subsequent output starts on a
     *        fresh line. Legacy equivalent: printf("\n").
     */
    inline std::string buildEnd()
    {
        return std::string("\n");
    }
}

#endif
