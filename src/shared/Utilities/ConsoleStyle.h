/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOSSERVER_CONSOLESTYLE_H
#define MANGOSSERVER_CONSOLESTYLE_H

/**
 * @file ConsoleStyle.h
 * @brief Terminal capability detection and the drawing primitives the startup
 *        console UI is built from.
 *
 * Two independent capabilities are probed once, at Init():
 *
 * - "fancy": stdout is an interactive console, so box-drawing and block glyphs
 *   may be used and a line may be repainted in place with '\r'. When stdout is
 *   redirected to a file or a pipe the whole UI degrades to the plain ASCII
 *   output the server has always produced.
 * - "colour": the console understands ANSI SGR escapes. On Windows this means
 *   ENABLE_VIRTUAL_TERMINAL_PROCESSING could be turned on; on a console too old
 *   for that, glyphs still work but no colour is emitted (escapes would print as
 *   literal garbage).
 *
 * Glyphs are authored in UTF-8 and restricted to the subset that also exists in
 * the DOS OEM code pages (box drawing, full block, light shade). Encode() maps
 * them to the Windows console code page through the same Utf8 -> wide -> OEM
 * path the rest of the logging uses, so they survive on a stock cmd.exe as well
 * as on a UTF-8 terminal.
 */

#include <cstddef>
#include <string>

namespace ConsoleStyle
{
    /**
     * @brief The glyph set in use. Members are UTF-8 when fancy output is on and
     *        ASCII substitutes otherwise, so the same builder code serves both.
     */
    struct Glyphs
    {
        const char* boxTopLeft;     ///< top-left corner of a panel
        const char* boxTopRight;    ///< top-right corner of a panel
        const char* boxBottomLeft;  ///< bottom-left corner of a panel
        const char* boxBottomRight; ///< bottom-right corner of a panel
        const char* boxHorizontal;  ///< panel top/bottom edge
        const char* boxVertical;    ///< panel side edge
        const char* boxTeeLeft;     ///< left junction of a panel's inner divider
        const char* boxTeeRight;    ///< right junction of a panel's inner divider
        const char* rule;           ///< section-rule fill
        const char* running;        ///< marker for the step in progress
        const char* done;           ///< marker for a completed step
        const char* separator;      ///< inline separator between fields
        const char* barFull;        ///< filled progress cell
        const char* barEmpty;       ///< empty progress cell
    };

    /**
     * @brief Probe the terminal and pick the glyph set. Safe to call once, early
     *        (before the console writer thread starts).
     * @param mode "auto" (fancy only on an interactive console), "fancy" (force
     *        it on, e.g. for a terminal we cannot detect) or "plain"/"off".
     */
    void Init(const std::string& mode);

    /// @return true when block/box glyphs and in-place line repaints may be used.
    bool Fancy();

    /// @return true when ANSI SGR colour escapes may be emitted.
    bool Colored();

    /// @return usable console width in columns, clamped to a sane range.
    int Width();

    /// @return the active glyph set.
    const Glyphs& G();

    /// SGR escapes. Each returns "" when colour is unavailable, so they can be
    /// concatenated unconditionally.
    const char* Reset();
    const char* Bold();
    const char* Dim();
    const char* Accent();
    const char* Good();

    /**
     * @brief Convert UTF-8 to the console's encoding (a no-op off Windows).
     */
    std::string Encode(const std::string& utf8);

    /**
     * @brief Encode @p utf8 and hand it to the off-thread console writer verbatim.
     *
     * The bytes carry their own '\r'/'\n'; nothing is appended. This is the only
     * way this UI reaches stdout, so it stays behind the single stdout owner.
     */
    void Emit(const std::string& utf8);

    /**
     * @brief Number of columns @p utf8 occupies: counts code points, not bytes,
     *        and skips SGR escape sequences. Used to pad and centre.
     */
    std::size_t VisibleWidth(const std::string& utf8);

    /**
     * @brief Append spaces to @p text until it is @p columns wide, or truncate it
     *        (with a trailing "..") when it is longer. Escape-aware.
     */
    std::string Fit(const std::string& text, std::size_t columns);

    /**
     * @brief Repeat @p glyph @p count times.
     */
    std::string Repeat(const char* glyph, std::size_t count);
}

#endif
