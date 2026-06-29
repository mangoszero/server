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

/// \addtogroup mangosd
/// @{
/// \file

#include "GdbMonitor.h"

#include "Log.h"

namespace GdbMon
{
    // -----------------------------------------------------------------------
    // MonitorWriter
    // -----------------------------------------------------------------------

    MonitorWriter::MonitorWriter(char* buf, uint32 cap) : m_buf(buf), m_cap(cap)
    {
        if (m_buf != nullptr && m_cap != 0)
        {
            m_buf[0] = '\0';
        }
    }

    void MonitorWriter::Char(char c)
    {
        if (m_buf == nullptr || m_pos + 1 >= m_cap)
        {
            m_truncated = true;
            return;
        }
        m_buf[m_pos++] = c;
        m_buf[m_pos] = '\0';
    }

    void MonitorWriter::Str(const char* s)
    {
        if (s == nullptr)
        {
            return;
        }
        for (uint32 i = 0; s[i] != '\0'; ++i)
        {
            if (m_pos + 1 >= m_cap)
            {
                m_truncated = true;
                return;
            }
            m_buf[m_pos++] = s[i];
        }
        if (m_buf != nullptr && m_pos < m_cap)
        {
            m_buf[m_pos] = '\0';
        }
    }

    void MonitorWriter::U64(uint64 v)
    {
        char tmp[24];
        uint32 n = 0;
        if (v == 0)
        {
            tmp[n++] = '0';
        }
        while (v != 0 && n < sizeof(tmp))
        {
            tmp[n++] = static_cast<char>('0' + (v % 10));
            v /= 10;
        }
        while (n != 0)
        {
            Char(tmp[--n]);
        }
    }

    void MonitorWriter::Hex(uint64 v, uint32 min_digits)
    {
        char tmp[16];
        uint32 n = 0;
        if (v == 0)
        {
            tmp[n++] = '0';
        }
        while (v != 0 && n < sizeof(tmp))
        {
            const uint32 nib = static_cast<uint32>(v & 0xF);
            tmp[n++] = static_cast<char>(nib < 10 ? ('0' + nib) : ('a' + nib - 10));
            v >>= 4;
        }
        while (n < min_digits && n < sizeof(tmp))
        {
            tmp[n++] = '0';
        }
        while (n != 0)
        {
            Char(tmp[--n]);
        }
    }

    void MonitorWriter::Line()
    {
        Char('\n');
    }

    // -----------------------------------------------------------------------
    // Tokenizer + small parse helpers
    // -----------------------------------------------------------------------

    namespace
    {
        constexpr uint32 kMaxArgs = 8;

        // Split into whitespace-delimited tokens (runs collapsed), pointing
        // into the caller-owned mutable `line` copy. Returns argc.
        uint32 Tokenize(char* line, uint32 len, const char** argv)
        {
            uint32 argc = 0;
            uint32 i = 0;
            while (i < len && argc < kMaxArgs)
            {
                while (i < len && (line[i] == ' ' || line[i] == '\t'))
                {
                    ++i;
                }
                if (i >= len || line[i] == '\0')
                {
                    break;
                }
                argv[argc++] = &line[i];
                while (i < len && line[i] != ' ' && line[i] != '\t' && line[i] != '\0')
                {
                    ++i;
                }
                if (i < len)
                {
                    line[i++] = '\0';
                }
            }
            return argc;
        }

        // Pointer to the substring of `cmd` that follows the first `skip`
        // whitespace-delimited tokens (used for verbs whose tail can itself
        // contain spaces, e.g. `config` keys or `cmd` command lines).
        const char* RestAfter(const char* cmd, uint32 skip)
        {
            uint32 i = 0;
            for (uint32 t = 0; t < skip; ++t)
            {
                while (cmd[i] == ' ' || cmd[i] == '\t')
                {
                    ++i;
                }
                if (cmd[i] == '\0')
                {
                    return &cmd[i];
                }
                while (cmd[i] != '\0' && cmd[i] != ' ' && cmd[i] != '\t')
                {
                    ++i;
                }
            }
            while (cmd[i] == ' ' || cmd[i] == '\t')
            {
                ++i;
            }
            return &cmd[i];
        }

        bool Eq(const char* a, const char* b)
        {
            if (a == nullptr || b == nullptr)
            {
                return false;
            }
            uint32 i = 0;
            while (a[i] != '\0' && b[i] != '\0')
            {
                if (a[i] != b[i])
                {
                    return false;
                }
                ++i;
            }
            return a[i] == b[i];
        }

        bool Contains(const char* hay, const char* needle)
        {
            if (hay == nullptr || needle == nullptr || needle[0] == '\0')
            {
                return false;
            }
            for (uint32 i = 0; hay[i] != '\0'; ++i)
            {
                uint32 j = 0;
                while (needle[j] != '\0' && hay[i + j] == needle[j])
                {
                    ++j;
                }
                if (needle[j] == '\0')
                {
                    return true;
                }
            }
            return false;
        }

        // Parse a decimal or 0x-hex unsigned. Returns false on empty /
        // malformed input so callers can reject it explicitly.
        bool ParseU64(const char* s, uint64* out)
        {
            if (s == nullptr || s[0] == '\0')
            {
                return false;
            }
            uint64 v = 0;
            uint32 i = 0;
            if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
            {
                i = 2;
                if (s[i] == '\0')
                {
                    return false;
                }
                for (; s[i] != '\0'; ++i)
                {
                    const char c = s[i];
                    uint32 d;
                    if (c >= '0' && c <= '9')
                        d = static_cast<uint32>(c - '0');
                    else if (c >= 'a' && c <= 'f')
                        d = static_cast<uint32>(10 + c - 'a');
                    else if (c >= 'A' && c <= 'F')
                        d = static_cast<uint32>(10 + c - 'A');
                    else
                        return false;
                    v = (v << 4) | d;
                }
            }
            else
            {
                for (; s[i] != '\0'; ++i)
                {
                    if (s[i] < '0' || s[i] > '9')
                    {
                        return false;
                    }
                    v = v * 10 + static_cast<uint64>(s[i] - '0');
                }
            }
            *out = v;
            return true;
        }

        void Usage(MonitorWriter& out)
        {
            out.Str("MaNGOS GDB monitor - verb namespace 'mangos'\n"
                    "  mangos help               this text\n"
                    "  mangos status             world summary (uptime, sessions, tick)\n"
                    "  mangos players            list online players\n"
                    "  mangos tick               world loop counter + uptime\n"
                    "  mangos session <accId>    session detail for an account id\n"
                    "  mangos config <key>       read a mangosd.conf value\n"
                    "  mangos cmd <command>      run a server/GM command (e.g. .server info)\n");
        }
    } // namespace

    // -----------------------------------------------------------------------
    // Dispatch
    // -----------------------------------------------------------------------

    bool GdbMonitorDispatch(const char* cmd, uint32 cmd_len, MonitorWriter& out)
    {
        if (cmd == nullptr)
        {
            return false;
        }

        static char line[1024];
        uint32 ln = 0;
        for (uint32 i = 0; i < cmd_len && cmd[i] != '\0' && ln + 1 < sizeof(line); ++i)
        {
            line[ln++] = cmd[i];
        }
        line[ln] = '\0';

        const char* argv[kMaxArgs] = {};
        const uint32 argc = Tokenize(line, ln, argv);
        if (argc == 0 || !Eq(argv[0], "mangos"))
        {
            return false;
        }
        if (argc == 1 || Eq(argv[1], "help"))
        {
            Usage(out);
            return true;
        }

        const char* sub = argv[1];

        if (Eq(sub, "status"))
        {
            verbs::CmdStatus(out);
        }
        else if (Eq(sub, "players") || Eq(sub, "ps"))
        {
            verbs::CmdPlayers(out);
        }
        else if (Eq(sub, "tick"))
        {
            verbs::CmdTick(out);
        }
        else if (Eq(sub, "session"))
        {
            uint64 accId = 0;
            if (argc < 3 || !ParseU64(argv[2], &accId))
            {
                out.Str("session: usage: mangos session <accountId>\n");
                return true;
            }
            verbs::CmdSession(accId, out);
        }
        else if (Eq(sub, "config"))
        {
            if (argc < 3)
            {
                out.Str("config: usage: mangos config <key>\n");
                return true;
            }
            verbs::CmdConfig(RestAfter(cmd, 2), out);
        }
        else if (Eq(sub, "cmd"))
        {
            const char* rest = RestAfter(cmd, 2);
            if (rest[0] == '\0')
            {
                out.Str("cmd: usage: mangos cmd <server command>\n");
                return true;
            }
            verbs::CmdCmd(rest, out);
        }
        else
        {
            out.Str("mangos: unknown command '");
            out.Str(sub);
            out.Str("' - try 'mangos help'\n");
        }

        if (out.Truncated())
        {
            out.Str("\n[truncated]\n");
        }
        return true;
    }

    // -----------------------------------------------------------------------
    // Self-test
    // -----------------------------------------------------------------------

    bool GdbMonitorSelfTest()
    {
        char buf[2048];

        // help must list the verb surface.
        {
            MonitorWriter w(buf, sizeof(buf));
            if (!GdbMonitorDispatch("mangos help", 11, w))
            {
                sLog.outError("[gdb-monitor-selftest] FAIL: 'mangos help' not recognized");
                return false;
            }
            if (!Contains(w.Data(), "players") || !Contains(w.Data(), "config"))
            {
                sLog.outError("[gdb-monitor-selftest] FAIL: help text missing expected verbs");
                return false;
            }
        }

        // tick always produces output.
        {
            MonitorWriter w(buf, sizeof(buf));
            GdbMonitorDispatch("mangos tick", 11, w);
            if (w.Len() == 0)
            {
                sLog.outError("[gdb-monitor-selftest] FAIL: 'mangos tick' produced no output");
                return false;
            }
        }

        // non-"mangos" lines must fall through to the unsupported reply.
        {
            MonitorWriter w(buf, sizeof(buf));
            if (GdbMonitorDispatch("info registers", 14, w))
            {
                sLog.outError("[gdb-monitor-selftest] FAIL: non-mangos line wrongly accepted");
                return false;
            }
        }

        sLog.outString("[gdb-monitor-selftest] PASS");
        return true;
    }
} // namespace GdbMon
/// @}
