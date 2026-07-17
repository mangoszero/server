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

#include "ConsoleStyle.h"

#include "Common.h"
#include "Log.h"
#include "Util.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if PLATFORM == PLATFORM_WINDOWS
#include <io.h>
#else
#include <sys/ioctl.h>
#include <unistd.h>
#include <string>
#endif

namespace
{
    /// Box drawing, light shade and full block. Every glyph here also exists in
    /// the DOS OEM code pages, so Encode() can map it onto a stock cmd.exe.
    const ConsoleStyle::Glyphs FancyGlyphs =
    {
        "\xE2\x95\x94", // U+2554 box drawings double down and right
        "\xE2\x95\x97", // U+2557 box drawings double down and left
        "\xE2\x95\x9A", // U+255A box drawings double up and right
        "\xE2\x95\x9D", // U+255D box drawings double up and left
        "\xE2\x95\x90", // U+2550 box drawings double horizontal
        "\xE2\x95\x91", // U+2551 box drawings double vertical
        "\xE2\x95\x9F", // U+255F box drawings vertical double and right single
        "\xE2\x95\xA2", // U+2562 box drawings vertical double and left single
        "\xE2\x94\x80", // U+2500 box drawings light horizontal
        "\xC2\xBB",     // U+00BB right double angle quote; the "in progress" marker
        "\xE2\x88\x9A", // U+221A square root; the OEM stand-in for a check mark
        "\xC2\xB7",     // U+00B7 middle dot
        "\xE2\x96\x88", // U+2588 full block
        "\xE2\x96\x91"  // U+2591 light shade
    };

    /// ASCII fallback: what a redirected log or a dumb terminal gets.
    const ConsoleStyle::Glyphs PlainGlyphs =
    {
        "+", "+", "+", "+", "-", "|", "+", "+",
        "-", ">", "*", "-", "#", " "
    };

    bool s_fancy   = false;
    bool s_colored = false;
    int  s_width   = 100;

    /// Ask the terminal how wide it is; fall back to a comfortable default.
    int QueryWidth()
    {
#if PLATFORM == PLATFORM_WINDOWS
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
        {
            const int cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
            if (cols > 0)
            {
                return cols;
            }
        }
#else
        struct winsize ws;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
        {
            return (int)ws.ws_col;
        }
#endif
        return 100;
    }

    bool StdoutIsConsole()
    {
#if PLATFORM == PLATFORM_WINDOWS
        return _isatty(_fileno(stdout)) != 0;
#else
        return isatty(fileno(stdout)) != 0;
#endif
    }

    /**
     * @brief Try to put the console into ANSI mode.
     *
     * On Windows 10+ this flips ENABLE_VIRTUAL_TERMINAL_PROCESSING; on an older
     * console the call fails and we simply never emit an escape. Elsewhere any
     * terminal that is not "dumb" is assumed to understand SGR.
     */
    bool EnableAnsi()
    {
#if PLATFORM == PLATFORM_WINDOWS
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
        HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
        DWORD  mode   = 0;
        if (handle == INVALID_HANDLE_VALUE || !GetConsoleMode(handle, &mode))
        {
            return false;
        }
        if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        {
            return true;
        }
        return SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING) != 0;
#else
        const char* term = getenv("TERM");
        return term && strcmp(term, "dumb") != 0;
#endif
    }
}

void ConsoleStyle::Init(const std::string& mode)
{
    const bool force = (mode == "fancy" || mode == "force" || mode == "1");
    const bool off   = (mode == "plain" || mode == "off" || mode == "0" || mode == "none");

    s_fancy = off ? false : (force || StdoutIsConsole());

    // NO_COLOR is the de-facto opt-out (https://no-color.org): honour it, but keep
    // the glyphs, which carry the layout.
    const char* noColor = getenv("NO_COLOR");
    s_colored = s_fancy && (noColor == NULL || *noColor == '\0') && EnableAnsi();

    s_width = QueryWidth();
    if (s_width < 60)
    {
        s_width = 60;
    }
    else if (s_width > 120)
    {
        s_width = 120;
    }
}

bool ConsoleStyle::Fancy()
{
    return s_fancy;
}

bool ConsoleStyle::Colored()
{
    return s_colored;
}

int ConsoleStyle::Width()
{
    return s_width;
}

const ConsoleStyle::Glyphs& ConsoleStyle::G()
{
    return s_fancy ? FancyGlyphs : PlainGlyphs;
}

const char* ConsoleStyle::Reset()
{
    return s_colored ? "\x1b[0m" : "";
}

const char* ConsoleStyle::Bold()
{
    return s_colored ? "\x1b[1m" : "";
}

const char* ConsoleStyle::Dim()
{
    return s_colored ? "\x1b[90m" : "";
}

const char* ConsoleStyle::Accent()
{
    return s_colored ? "\x1b[96m" : "";
}

const char* ConsoleStyle::Good()
{
    return s_colored ? "\x1b[92m" : "";
}

std::string ConsoleStyle::Encode(const std::string& utf8)
{
#if PLATFORM == PLATFORM_WINDOWS
    // Same conversion the formatted log path uses (vutf8format): UTF-8 -> wide ->
    // the console's OEM code page. Escapes and the rest of ASCII pass through
    // unchanged; the box/block glyphs land on their OEM code points.
    std::wstring wide;
    if (!Utf8toWStr(utf8, wide) || wide.empty())
    {
        return utf8;
    }

    std::string out;
    out.resize(wide.size());
    CharToOemBuffW(wide.c_str(), &out[0], (DWORD)wide.size());
    return out;
#else
    return utf8;
#endif
}

void ConsoleStyle::Emit(const std::string& utf8)
{
    sLog.ConsoleEmitRaw(Encode(utf8));
}

std::size_t ConsoleStyle::VisibleWidth(const std::string& utf8)
{
    std::size_t columns = 0;

    for (std::size_t i = 0; i < utf8.size(); ++i)
    {
        const unsigned char c = (unsigned char)utf8[i];

        if (c == 0x1B)
        {
            // Skip a CSI sequence: ESC '[' ... final byte in 0x40..0x7E.
            ++i;
            if (i < utf8.size() && utf8[i] == '[')
            {
                while (i + 1 < utf8.size() &&
                       ((unsigned char)utf8[i + 1] < 0x40 || (unsigned char)utf8[i + 1] > 0x7E))
                {
                    ++i;
                }
                ++i;
            }
            continue;
        }

        // One column per code point: continuation bytes (0b10xxxxxx) do not count.
        if ((c & 0xC0) != 0x80)
        {
            ++columns;
        }
    }

    return columns;
}

std::string ConsoleStyle::Fit(const std::string& text, std::size_t columns)
{
    const std::size_t visible = VisibleWidth(text);

    if (visible <= columns)
    {
        return text + std::string(columns - visible, ' ');
    }

    // Too long: keep the leading columns-2 code points and mark the cut. Walk by
    // code point so a multi-byte glyph is never sliced in half.
    const std::size_t keep = (columns > 2) ? (columns - 2) : 0;
    std::size_t seen  = 0;
    std::size_t bytes = 0;

    while (bytes < text.size() && seen < keep)
    {
        ++bytes;
        while (bytes < text.size() && ((unsigned char)text[bytes] & 0xC0) == 0x80)
        {
            ++bytes;
        }
        ++seen;
    }

    return text.substr(0, bytes) + std::string(columns - seen, '.');
}

std::string ConsoleStyle::Repeat(const char* glyph, std::size_t count)
{
    std::string out;
    const std::size_t len = strlen(glyph);
    out.reserve(len * count);

    for (std::size_t i = 0; i < count; ++i)
    {
        out += glyph;
    }

    return out;
}
