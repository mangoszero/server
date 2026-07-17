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

#ifndef MANGOSSERVER_PROGRESSBAR_H
#define MANGOSSERVER_PROGRESSBAR_H

#include "Platform/Define.h"

#include <cstddef>
#include <string>

/**
 * @brief
 *
 */
class BarGoLink
{
    public:
        /**
         * @brief constructors
         *
         * @param row_count
         */
        explicit BarGoLink(int row_count);

        /**
         * @brief
         *
         */
        ~BarGoLink();

    public:
        /**
         * @brief modifiers
         *
         */
        void step();

        /**
         * @brief
         *
         * @param on
         */
        static void SetOutputState(bool on);

        /**
         * @brief Console output sink for one fully-built bar redraw.
         *
         * Receives the exact bytes of a single visual update (built by
         * ProgressBarRender, carrying their own '\r'/'\n'). The default sink
         * does a synchronous fwrite(stdout)+fflush, byte-identical to the
         * legacy per-fragment printf path, so offline tools and any caller
         * before the logging system is up behave exactly as before. mangosd
         * installs a sink that forwards the bytes to the off-thread console
         * writer (sLog.ConsoleEmitRaw), so the bar shares one serialized stdout
         * with the log lines and can no longer tear against them.
         */
        typedef void (*ConsoleSink)(char const* bytes, size_t len);

        /**
         * @brief Install the console sink. Passing NULL restores the default
         *        synchronous sink.
         * @param sink
         */
        static void SetConsoleSink(ConsoleSink sink);

        /**
         * @brief Optional renderer for one bar redraw.
         *
         * mangosd's startup UI installs one so the bar is drawn as part of the
         * step line it belongs to (label, blocks, percentage, rows and elapsed
         * time on a single line that is repainted in place). It also lets the UI
         * learn each step's row count, which is what @p total carries.
         *
         * When no renderer is installed -- realmd, the offline tools -- the
         * legacy ASCII bar in ProgressBarRender is emitted, unchanged.
         *
         * @param done      rows processed so far
         * @param total     rows expected in this bar
         * @param elapsedMs milliseconds since the bar was created
         * @param finished  true for the final call, made when the bar is torn down
         * @return the exact bytes of this redraw (carrying their own '\r'/'\n');
         *         may be empty to draw nothing
         */
        typedef std::string (*Renderer)(int done, int total, uint32 elapsedMs, bool finished);

        /**
         * @brief Install the styled renderer. Passing NULL restores the legacy bar.
         * @param renderer
         */
        static void SetRenderer(Renderer renderer);
    private:
        /**
         * @brief
         *
         * @param row_count
         */
        void init(int row_count);

        /// Hand one built redraw to the active sink.
        void emit(std::string const& bytes);

        /// Milliseconds since this bar was constructed.
        uint32 elapsedMs() const;

        /// Default synchronous sink: fwrite(stdout)+fflush (legacy behaviour).
        static void DefaultSink(char const* bytes, size_t len);

        static ConsoleSink m_sink; /**< active console sink for built bar redraws */
        static Renderer m_renderer; /**< active styled renderer, or NULL for the legacy bar */
        static bool m_showOutput; /**< not recommended change with existed active bar */

        int rec_no; /**< TODO */
        int rec_pos; /**< TODO */
        int num_rec; /**< TODO */
        int indic_len; /**< TODO */
        uint64 start_ms; /**< construction time, for the elapsed-time readout */
};
#endif
