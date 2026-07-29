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

#include <string>
#include "Terminal.h"

#include <cstdio>

#ifdef _WIN32
#  include <windows.h>
#  include <io.h>
#else
#  include <termios.h>
#  include <unistd.h>
#  include <poll.h>
#  include <sys/ioctl.h>
#endif

namespace MaNGOS
{
namespace Console
{

namespace
{
    bool s_active = false;

    const char ENTER_SEQ[] = "\x1b[?1049h\x1b[?25l\x1b[2J";
    const char LEAVE_SEQ[] = "\x1b[0m\x1b[?25h\x1b[?1049l";

#ifdef _WIN32
    HANDLE  s_out       = INVALID_HANDLE_VALUE;
    HANDLE  s_in        = INVALID_HANDLE_VALUE;
    DWORD   s_outMode   = 0;
    DWORD   s_inMode    = 0;
    UINT    s_outCp     = 0;

    int MapVirtualKeyCode(const KEY_EVENT_RECORD& ev)
    {
        switch (ev.wVirtualKeyCode)
        {
        case VK_RETURN: return KEY_ENTER;
        case VK_BACK:   return KEY_BACKSPACE;
        case VK_DELETE: return KEY_DELETE;
        case VK_LEFT:   return KEY_LEFT;
        case VK_RIGHT:  return KEY_RIGHT;
        case VK_UP:     return KEY_UP;
        case VK_DOWN:   return KEY_DOWN;
        case VK_HOME:   return KEY_HOME;
        case VK_END:    return KEY_END;
        case VK_PRIOR:  return KEY_PAGEUP;
        case VK_NEXT:   return KEY_PAGEDOWN;
        case VK_TAB:    return KEY_TAB;
        default:        break;
        }

        const wchar_t ch = ev.uChar.UnicodeChar;
        if (ev.dwControlKeyState & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED))
        {
            switch (ch)
            {
            case 21: return KEY_KILLLINE;
            case 23: return KEY_KILLWORD;
            case 12: return KEY_REDRAW;
            case 4:  return KEY_EOF;
            default: break;
            }
        }

        return (ch >= 32 || ch == 0) ? int(ch) : KEY_NONE;
    }
#else
    termios s_orig;
    bool    s_origSaved = false;

    /// Read one more byte of a multi-byte key, waiting up to @p waitMs for it.
    int NextByte(int waitMs)
    {
        pollfd pfd;
        pfd.fd     = STDIN_FILENO;
        pfd.events = POLLIN;

        if (poll(&pfd, 1, waitMs) <= 0)
        {
            return -1;
        }

        unsigned char c = 0;
        return (read(STDIN_FILENO, &c, 1) == 1) ? int(c) : -1;
    }

    int DecodeEscape()
    {
        const int b1 = NextByte(25);
        if (b1 < 0)
        {
            return KEY_NONE;
        }
        if (b1 != '[' && b1 != 'O')
        {
            return KEY_NONE;
        }

        const int b2 = NextByte(25);
        switch (b2)
        {
        case 'A': return KEY_UP;
        case 'B': return KEY_DOWN;
        case 'C': return KEY_RIGHT;
        case 'D': return KEY_LEFT;
        case 'H': return KEY_HOME;
        case 'F': return KEY_END;
        default:  break;
        }

        if (b2 < '0' || b2 > '9')
        {
            return KEY_NONE;
        }

        int value = b2 - '0';
        for (int b = NextByte(25); b >= 0 && b != '~'; b = NextByte(25))
        {
            if (b >= '0' && b <= '9')
            {
                value = value * 10 + (b - '0');
            }
            else
            {
                return KEY_NONE;
            }
        }

        switch (value)
        {
        case 1:
        case 7:  return KEY_HOME;
        case 3:  return KEY_DELETE;
        case 4:
        case 8:  return KEY_END;
        case 5:  return KEY_PAGEUP;
        case 6:  return KEY_PAGEDOWN;
        default: return KEY_NONE;
        }
    }

    int DecodeUtf8(unsigned char lead)
    {
        int extra = 0;
        int cp    = 0;

        if ((lead & 0xE0) == 0xC0)      { extra = 1; cp = lead & 0x1F; }
        else if ((lead & 0xF0) == 0xE0) { extra = 2; cp = lead & 0x0F; }
        else if ((lead & 0xF8) == 0xF0) { extra = 3; cp = lead & 0x07; }
        else                            { return KEY_NONE; }

        while (extra-- > 0)
        {
            const int b = NextByte(25);
            if (b < 0 || (b & 0xC0) != 0x80)
            {
                return KEY_NONE;
            }
            cp = (cp << 6) | (b & 0x3F);
        }

        return cp;
    }
#endif
}

bool Terminal::Active()
{
    return s_active;
}

void Terminal::Write(const std::string& s)
{
    Write(s.data(), s.size());
}

#ifdef _WIN32

bool Terminal::IsInteractive()
{
    if (!_isatty(_fileno(stdout)) || !_isatty(_fileno(stdin)))
    {
        return false;
    }

    DWORD mode = 0;
    return GetConsoleMode(GetStdHandle(STD_OUTPUT_HANDLE), &mode) != 0;
}

bool Terminal::Enter()
{
    if (s_active || !IsInteractive())
    {
        return false;
    }

    s_out = GetStdHandle(STD_OUTPUT_HANDLE);
    s_in  = GetStdHandle(STD_INPUT_HANDLE);

    if (!GetConsoleMode(s_out, &s_outMode) || !GetConsoleMode(s_in, &s_inMode))
    {
        return false;
    }

    DWORD wanted = s_outMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING
                             | ENABLE_PROCESSED_OUTPUT;
    if (!SetConsoleMode(s_out, wanted))
    {
        return false;                                   // pre-1511 console host
    }

    s_outCp = GetConsoleOutputCP();
    SetConsoleOutputCP(CP_UTF8);

    // ENABLE_VIRTUAL_TERMINAL_INPUT MUST be off: with it on, the console reports
    // arrows and page keys as the bytes of an escape sequence spread over several
    // KEY_EVENTs, which this decoder would take for literal text and type into the
    // command line. Windows Terminal leaves it on, so clearing it is not optional.
    // ENABLE_PROCESSED_INPUT stays on so Ctrl+C keeps reaching the console control
    // handler that drives the existing clean-shutdown path.
    SetConsoleMode(s_in, (s_inMode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT
                                       | ENABLE_VIRTUAL_TERMINAL_INPUT))
                         | ENABLE_PROCESSED_INPUT | ENABLE_WINDOW_INPUT);

    s_active = true;
    Write(ENTER_SEQ, sizeof(ENTER_SEQ) - 1);
    return true;
}

void Terminal::Leave()
{
    if (!s_active)
    {
        return;
    }

    Write(LEAVE_SEQ, sizeof(LEAVE_SEQ) - 1);
    s_active = false;

    SetConsoleMode(s_out, s_outMode);
    SetConsoleMode(s_in, s_inMode);
    if (s_outCp)
    {
        SetConsoleOutputCP(s_outCp);
    }
}

void Terminal::Size(int& rows, int& cols)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
    if (s_out != INVALID_HANDLE_VALUE && GetConsoleScreenBufferInfo(s_out, &info))
    {
        cols = info.srWindow.Right  - info.srWindow.Left + 1;
        rows = info.srWindow.Bottom - info.srWindow.Top  + 1;
    }
    else
    {
        rows = 24;
        cols = 80;
    }
}

int Terminal::ReadKey()
{
    if (!s_active)
    {
        return KEY_NONE;
    }

    for (;;)
    {
        DWORD pending = 0;
        if (!GetNumberOfConsoleInputEvents(s_in, &pending) || pending == 0)
        {
            return KEY_NONE;
        }

        INPUT_RECORD rec;
        DWORD read = 0;
        if (!ReadConsoleInputW(s_in, &rec, 1, &read) || read == 0)
        {
            return KEY_NONE;
        }

        if (rec.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            return KEY_RESIZE;
        }
        if (rec.EventType != KEY_EVENT || !rec.Event.KeyEvent.bKeyDown)
        {
            continue;
        }

        const int key = MapVirtualKeyCode(rec.Event.KeyEvent);
        if (key != KEY_NONE)
        {
            return key;
        }
    }
}

void Terminal::Write(const char* bytes, std::size_t len)
{
    if (!len)
    {
        return;
    }

    // WriteFile, not fwrite: stdout is a text-mode CRT stream on Windows and
    // would rewrite '\n' into "\r\n" inside cursor-positioning sequences.
    DWORD written = 0;
    WriteFile(s_out, bytes, DWORD(len), &written, NULL);
}

#else

bool Terminal::IsInteractive()
{
    return isatty(STDOUT_FILENO) != 0 && isatty(STDIN_FILENO) != 0;
}

bool Terminal::Enter()
{
    if (s_active || !IsInteractive())
    {
        return false;
    }

    if (tcgetattr(STDIN_FILENO, &s_orig) != 0)
    {
        return false;
    }
    s_origSaved = true;

    termios raw = s_orig;
    raw.c_lflag &= ~(ECHO | ICANON);                    // ISIG kept: Ctrl+C still signals
    raw.c_cc[VMIN]  = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0)
    {
        return false;
    }

    s_active = true;
    Write(ENTER_SEQ, sizeof(ENTER_SEQ) - 1);
    return true;
}

void Terminal::Leave()
{
    if (!s_active)
    {
        return;
    }

    Write(LEAVE_SEQ, sizeof(LEAVE_SEQ) - 1);
    s_active = false;

    if (s_origSaved)
    {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &s_orig);
    }
}

void Terminal::Size(int& rows, int& cols)
{
    winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0)
    {
        rows = ws.ws_row;
        cols = ws.ws_col;
    }
    else
    {
        rows = 24;
        cols = 80;
    }
}

int Terminal::ReadKey()
{
    if (!s_active)
    {
        return KEY_NONE;
    }

    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) != 1)
    {
        return KEY_NONE;
    }

    switch (c)
    {
    case '\r':
    case '\n': return KEY_ENTER;
    case 127:
    case 8:    return KEY_BACKSPACE;
    case '\t': return KEY_TAB;
    case 21:   return KEY_KILLLINE;
    case 23:   return KEY_KILLWORD;
    case 12:   return KEY_REDRAW;
    case 4:    return KEY_EOF;
    case 27:   return DecodeEscape();
    default:   break;
    }

    if (c < 32)
    {
        return KEY_NONE;
    }

    return (c < 0x80) ? int(c) : DecodeUtf8(c);
}

void Terminal::Write(const char* bytes, std::size_t len)
{
    while (len > 0)
    {
        const ssize_t n = write(STDOUT_FILENO, bytes, len);
        if (n <= 0)
        {
            return;                                     // EPIPE / closed console
        }
        bytes += n;
        len   -= std::size_t(n);
    }
}

#endif

}
}
