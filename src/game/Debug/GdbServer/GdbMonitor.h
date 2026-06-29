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

#ifndef MANGOS_H_GDB_MONITOR
#define MANGOS_H_GDB_MONITOR

#include "Common.h"

/*
 * GDB `monitor` (qRcmd) command surface for mangosd, ported from the
 * DuetOS `duet` monitor. The generic GDB remote-serial server (GdbRsp)
 * speaks raw registers / memory; stock GDB cannot express mangos-specific
 * state (online players, the world tick, config values, server commands).
 * This TU implements a `mangos <verb> [args]` command surface delivered
 * over the standard GDB `qRcmd,<hex>` ("monitor") packet, so a specialised
 * client OR stock `gdb` (`monitor mangos ...`) — OR any AI over the plain
 * monitor bridge — gets full introspection + control.
 *
 * EXECUTION CONTEXT
 *   GdbMonitorDispatch only ever runs on the world thread (from the
 *   GdbServer per-tick service or stop loop), so verbs may touch game
 *   state directly without locking.
 */
namespace GdbMon
{
    /**
     * Bounded text builder for monitor replies. Stops writing on overflow
     * and latches Truncated(); never writes out of bounds. NUL-terminates
     * on every mutation so Data() is always a valid C string.
     */
    class MonitorWriter
    {
        public:
            MonitorWriter(char* buf, uint32 cap);

            void Str(const char* s);
            void Char(char c);
            void U64(uint64 v);                       // decimal
            void Hex(uint64 v, uint32 min_digits = 0); // lowercase, no "0x"
            void Line();                              // emits '\n'

            bool Truncated() const { return m_truncated; }
            uint32 Len() const { return m_pos; }
            const char* Data() const { return m_buf; }

        private:
            char* m_buf;
            uint32 m_cap;
            uint32 m_pos = 0;
            bool m_truncated = false;
    };

    /**
     * Execute one decoded monitor command line. Returns true when @p cmd was
     * a recognised `mangos ...` line (the reply is in @p out, even for an
     * unknown subcommand — a friendly usage hint). Returns false ONLY when
     * @p cmd is not a `mangos` line at all, so the RSP caller can answer the
     * GDB packet with the empty "unsupported" reply.
     */
    bool GdbMonitorDispatch(const char* cmd, uint32 cmd_len, MonitorWriter& out);

    /// Boot-time self-test. Exercises the dispatcher directly (no socket
    /// I/O) and logs a grep-able "[gdb-monitor-selftest] PASS" line.
    /// Returns false on failure (caller logs an error; never aborts).
    bool GdbMonitorSelfTest();

    // Mangos-specific read verbs. Defined in GdbMonitorVerbs.cpp; the
    // dispatch table in GdbMonitor.cpp routes here.
    namespace verbs
    {
        void CmdStatus(MonitorWriter& out);
        void CmdPlayers(MonitorWriter& out);
        void CmdTick(MonitorWriter& out);
        void CmdSession(uint64 accountId, MonitorWriter& out);
        void CmdConfig(const char* key, MonitorWriter& out);
        void CmdCmd(const char* serverCommand, MonitorWriter& out);
    }
}

#endif
/// @}
