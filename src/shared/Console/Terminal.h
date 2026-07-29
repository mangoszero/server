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

#ifndef MANGOS_H_CONSOLE_TERMINAL
#define MANGOS_H_CONSOLE_TERMINAL

#include <cstddef>
#include <string>

namespace MaNGOS
{
namespace Console
{
    /**
     * @brief Decoded keystroke.
     *
     * Values > 0 are Unicode code points. Values < 0 are the named keys below,
     * so a caller can switch on the negative set and treat everything else as
     * text.
     */
    enum Key
    {
        KEY_NONE      = 0,
        KEY_ENTER     = -1,
        KEY_BACKSPACE = -2,
        KEY_DELETE    = -3,
        KEY_LEFT      = -4,
        KEY_RIGHT     = -5,
        KEY_UP        = -6,
        KEY_DOWN      = -7,
        KEY_HOME      = -8,
        KEY_END       = -9,
        KEY_PAGEUP    = -10,
        KEY_PAGEDOWN  = -11,
        KEY_TAB       = -12,
        KEY_KILLLINE  = -13,
        KEY_KILLWORD  = -14,
        KEY_REDRAW    = -15,
        KEY_EOF       = -16,
        KEY_RESIZE    = -17
    };

    /**
     * @brief Raw terminal control: VT output, unbuffered keystroke input, size.
     *
     * The whole platform split of the console UI lives here; everything above
     * this class is plain VT100/ECMA-48 byte pushing and is identical on
     * Windows, Linux and the BSDs. Windows 10 1511+ renders those sequences
     * once ENABLE_VIRTUAL_TERMINAL_PROCESSING is set, which Enter() does.
     *
     * Not thread-safe by itself: ConsoleUI owns the instance and serialises
     * Write() (render thread) against ReadKey() (input thread) by only ever
     * calling them from their one designated thread.
     */
    class Terminal
    {
        public:
            /// True when both stdin and stdout are attached to a real terminal.
            static bool IsInteractive();

            /**
             * @brief Switch the terminal into full-screen VT mode.
             *
             * Enables VT rendering, turns off line buffering and local echo,
             * and moves to the alternate screen so Leave() restores whatever
             * the operator had on screen before. Returns false and changes
             * nothing when the process is not interactive (service, daemon,
             * redirected output), which is the caller's cue to stay on the
             * plain line-oriented logger.
             */
            static bool Enter();

            /// Undo Enter(). Safe to call when Enter() failed or never ran.
            static void Leave();

            static bool Active();

            /// Current window size; falls back to 80x24 if the query fails.
            static void Size(int& rows, int& cols);

            /// Non-blocking: returns KEY_NONE when nothing is buffered.
            static int ReadKey();

            static void Write(const char* bytes, std::size_t len);
            static void Write(const std::string& s);
    };
}
}

#endif
