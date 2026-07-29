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

#ifndef MANGOS_H_CONSOLE_CONSOLEUI
#define MANGOS_H_CONSOLE_CONSOLEUI

#include <atomic>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace MaNGOS
{
namespace Console
{
    /**
     * @brief Semantic weight of a line, resolved to colour by the theme.
     *
     * Callers name intent, never a colour, so the palette is decided in exactly
     * one place and stays coherent across the three regions.
     */
    enum Style
    {
        STYLE_NORMAL,
        STYLE_DETAIL,
        STYLE_DEBUG,
        STYLE_SUCCESS,
        STYLE_WARN,
        STYLE_ERROR,
        STYLE_ACCENT
    };

    enum
    {
        MAX_STATUS_FIELDS   = 12,
        DEFAULT_SCROLLBACK  = 20000
    };

    /**
     * @brief Full-screen console: status header, scrollback log, command line.
     *
     * Three regions on one alternate screen. Producers (any thread) only push
     * state; the single render thread turns that state into a frame and writes
     * the rows that actually changed, so nothing flickers and no producer ever
     * blocks on the terminal.
     *
     * Game-agnostic on purpose: it takes strings and numbers and knows nothing
     * about worlds, players or maps. What to display is the caller's decision.
     */
    class ConsoleUI
    {
        public:
            static ConsoleUI& Instance();

            /**
             * @brief Take over the terminal.
             * @return false when the process has no terminal to take over, and
             *         the caller must keep using line-oriented output.
             */
            bool Start(const std::string& title, const std::string& subtitle);

            /// Restore the terminal and replay the log tail onto the normal screen.
            void Stop();

            bool Active() const { return m_active; }

            void PushLog(const std::string& text, Style style);

            /**
             * @brief Push verbatim console bytes, split into lines.
             *
             * For output that was written for a line-oriented console and still
             * carries its own newlines (CLI command results). Embedded control
             * bytes are dropped rather than replayed.
             */
            void PushRaw(const std::string& bytes, Style style);

            void SetStatus(unsigned slot, const std::string& label,
                           const std::string& value, Style style = STYLE_NORMAL);

            /// Current long-running operation shown next to the progress bar.
            void SetActivity(const std::string& text);

            /// 0..100 draws the bar; a negative value hides it.
            void SetProgress(int percent);

            void SetPrompt(const std::string& prompt);
            void SetHint(const std::string& hint);

            /// Show the code of each keystroke in the input row. Diagnostic only.
            void SetKeyEcho(bool on);

            /// Lines of history kept. Trims immediately when lowered.
            void SetScrollback(unsigned lines);
            void SetHeaderRight(const std::string& text);

            /**
             * @brief Drain the keyboard and hand back one finished command line.
             *
             * Non-blocking, and the only entry point that touches stdin. Call it
             * from the one thread that consumes commands.
             *
             * @return true when @p line was filled with a submitted command.
             */
            bool PollInput(std::string& line);

            /// Terminal width in columns, for a caller that wants to lay text out
            /// itself. 0 before Start().
            int Columns() const;

            /// Build a frame and emit the changed rows. Render thread only.
            void Render();

        private:
            ConsoleUI();

            struct LogLine
            {
                std::string text;
                Style       style;
            };

            struct StatusField
            {
                std::string label;
                std::string value;
                Style       style;
                bool        set;

                StatusField() : style(STYLE_NORMAL), set(false) {}
            };

            /// Append to the scrollback. Caller must hold m_mutex.
            void AppendLog(const std::string& text, Style style);

            void HandleKey(int key);
            void InsertCodePoint(int cp);
            void EraseBefore();
            void EraseAt();
            void MoveLeft();
            void MoveRight();
            void RecallHistory(int delta);
            void Submit();

            void ComposeFrame(std::vector<std::string>& frame, int rows, int cols);
            void ComposeHeader(std::vector<std::string>& frame, int cols);
            void ComposeStatus(std::vector<std::string>& frame, int cols);
            void ComposeLog(std::vector<std::string>& frame, int cols, int height);
            void ComposeInput(std::vector<std::string>& frame, int cols);

            mutable std::mutex      m_mutex;
            /// Read without m_mutex by the log writer and the CLI thread, while
            /// Stop() clears it from the main thread -- so it has to be atomic.
            std::atomic<bool>       m_active;
            bool                    m_forceRedraw;
            int                     m_rows;
            int                     m_cols;

            std::string             m_title;
            std::string             m_subtitle;
            std::string             m_headerRight;
            std::string             m_activity;
            std::string             m_hint;
            std::string             m_prompt;
            int                     m_progress;
            bool                    m_keyEcho;
            int                     m_lastKey;

            StatusField             m_status[MAX_STATUS_FIELDS];
            std::deque<LogLine>     m_log;
            std::size_t             m_scrollback;
            int                     m_scroll;

            std::string             m_input;
            std::size_t             m_cursor;
            std::vector<std::string> m_history;
            int                     m_historyPos;
            std::deque<std::string> m_submitted;

            std::vector<std::string> m_frame;
            int                     m_cursorCol;
    };
}
}

#endif
