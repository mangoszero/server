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
#include "ProgressBarRender.h"
#include "Errors.h"

#include <chrono>
#include <string>

namespace
{
    /// Monotonic millisecond clock, independent of any game/world timer so the
    /// bar keeps working in the offline tools.
    uint64 NowMs()
    {
        using namespace std::chrono;
        return (uint64)duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
    }
}

/**
 * @var BarGoLink::m_showOutput
 * @brief Global flag controlling progress bar visibility
 *
 * When false, all progress bar output is suppressed. Used for
 * non-interactive modes or when logging to file.
 */
bool BarGoLink::m_showOutput = true;

/**
 * @brief Default console sink: write the redraw bytes straight to stdout and
 *        flush, exactly as the legacy printf()+fflush() path did. Used by
 *        offline tools and any caller before mangosd installs the async sink.
 */
void BarGoLink::DefaultSink(char const* bytes, size_t len)
{
    fwrite(bytes, 1, len, stdout);
    fflush(stdout);
}

BarGoLink::ConsoleSink BarGoLink::m_sink = &BarGoLink::DefaultSink;
BarGoLink::Renderer BarGoLink::m_renderer = NULL;

void BarGoLink::SetConsoleSink(ConsoleSink sink)
{
    m_sink = sink ? sink : &BarGoLink::DefaultSink;
}

void BarGoLink::SetRenderer(Renderer renderer)
{
    m_renderer = renderer;
}

void BarGoLink::emit(std::string const& bytes)
{
    if (!bytes.empty())
    {
        m_sink(bytes.data(), bytes.size());
    }
}

uint32 BarGoLink::elapsedMs() const
{
    return (uint32)(NowMs() - start_ms);
}

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

    if (m_renderer)
    {
        // The styled bar shares its line with the step it belongs to: the final
        // redraw is the startup UI's business, not a bare newline.
        emit(m_renderer(rec_no, num_rec, elapsedMs(), true));
        return;
    }

    emit(ProgressBarRender::buildEnd());
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
    start_ms  = NowMs();

    if (!m_showOutput)
    {
        return;
    }

    if (m_renderer)
    {
        emit(m_renderer(0, num_rec, 0, false));
        return;
    }

    emit(ProgressBarRender::buildInit(indic_len));
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

    if (num_rec == 0)
    {
        return;
    }
    ++rec_no;
    int n = rec_no * indic_len / num_rec;
    if (n != rec_pos)
    {
        if (m_renderer)
        {
            emit(m_renderer(rec_no, num_rec, elapsedMs(), false));
        }
        else
        {
            emit(ProgressBarRender::buildStep(n, indic_len));
        }
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
