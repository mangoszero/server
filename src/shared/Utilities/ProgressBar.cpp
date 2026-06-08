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

/**
 * @file ProgressBar.cpp
 * @brief Console progress bar implementation
 *
 * This file implements BarGoLink, a lightweight console progress bar
 * for displaying operation progress during long-running tasks like
 * database loading, map initialization, or file processing.
 *
 * Features:
 * - Visual progress indication with percentage
 * - Cross-platform (Windows/Unix) character rendering
 * - Optional output suppression for non-interactive modes
 * - Automatic cleanup on destruction
 *
 * Usage pattern:
 *   BarGoLink bar(total_items);
 *   for (each item) {
 *       process(item);
 *       bar.step();
 *   }
 */

#include <stdio.h>

#include "ProgressBar.h"
#include "Errors.h"

/**
 * @var BarGoLink::m_showOutput
 * @brief Global flag controlling progress bar visibility
 *
 * When false, all progress bar output is suppressed. Used for
 * non-interactive modes or when logging to file.
 */
bool BarGoLink::m_showOutput = true;

char const* const BarGoLink::empty = " ";
#ifdef _WIN32
char const* const BarGoLink::full  = "\x3D";
#else
char const* const BarGoLink::full  = "*";
#endif

/**
 * @brief Construct progress bar
 * @param row_count Total number of items to process
 *
 * Creates a progress bar and immediately displays the empty bar
 * at 0% progress. The bar is 50 characters wide with percentage display.
 *
 * @note Does nothing if m_showOutput is false
 */
BarGoLink::BarGoLink(int row_count)
{
    init(row_count);
}

/**
 * @brief Destroy progress bar
 *
 * Outputs a final newline to complete the progress display.
 * This ensures subsequent console output appears on a fresh line.
 */
BarGoLink::~BarGoLink()
{
    if (!m_showOutput)
    {
        return;
    }

    printf("\n");
    fflush(stdout);
}

/**
 * @brief Initialize progress bar state and display
 * @param row_count Total number of items to process
 *
 * Sets up internal counters and renders the initial empty progress bar.
 * Platform-specific characters are used:
 * - Windows: Uses '=' for bar edges and fill
 * - Unix: Uses '[', ']', '*' for bar display
 */
void BarGoLink::init(int row_count)
{
    rec_no    = 0;
    rec_pos   = 0;
    indic_len = 50;
    num_rec   = row_count;

    if (!m_showOutput)
    {
        return;
    }

#ifdef _WIN32
    printf("\x3D");
#else
    printf("[");
#endif

    for (int i = 0; i < indic_len; ++i)
    {
        printf(empty);
    }
#ifdef _WIN32
    printf("\x3D 0%%\r\x3D");
#else
    printf("] 0%%\r[");
#endif

    fflush(stdout);
}

/**
 * @brief Advance progress by one step
 *
 * Increments the internal counter and updates the display if the
 * progress bar position has changed. Called once per processed item.
 *
 * The display only updates when the visual position changes to
 * minimize console output overhead.
 *
 * @note Safe to call even when m_showOutput is false (no-op)
 */
void BarGoLink::step()
{
    if (!m_showOutput)
    {
        return;
    }

    int i, n;

    if (num_rec == 0)
    {
        return;
    }
    ++rec_no;
    n = rec_no * indic_len / num_rec;
    if (n != rec_pos)
    {
#ifdef _WIN32
        printf("\r\x3D");
#else
        printf("\r[");
#endif

        for (i = 0; i < n; ++i)
        {
            printf(full);
        }
        for (; i < indic_len; ++i)
        {
            printf(empty);
        }
        float percent = (((float)n / (float)indic_len) * 100);
#ifdef _WIN32
        printf("\x3D %i%%  \r\x3D", (int)percent);
#else
        printf("] %i%%  \r[", (int)percent);
#endif

        fflush(stdout);

        rec_pos = n;
    }
}

/**
 * @brief Enable or disable progress bar output globally
 * @param on true to enable output, false to suppress
 *
 * Controls whether all BarGoLink instances produce console output.
 * Used for server modes where console feedback is not desired.
 *
 * @note This is a static method affecting all progress bars
 */
void BarGoLink::SetOutputState(bool on)
{
    m_showOutput = on;
}
