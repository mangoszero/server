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

#include "Chat.h"
#include "Config.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Util.h"
#include "World.h"
#include "WorldSession.h"

#include <cstdlib>
#if defined(_WIN32)
#  include <windows.h>
#elif defined(__linux__)
#  include <execinfo.h>
#endif

namespace GdbMon
{
    namespace
    {
        // CliHandler print adapter: appends each emitted line to the bound
        // MonitorWriter so a server command's console output flows back to
        // the debugger / monitor client verbatim.
        void MonitorPrint(void* arg, char const* txt)
        {
            if (txt != nullptr && arg != nullptr)
            {
                static_cast<MonitorWriter*>(arg)->Str(txt);
            }
        }

        void WriteSessionLine(WorldSession* sess, MonitorWriter& out)
        {
            out.Str("  acc=");
            out.U64(sess->GetAccountId());
            out.Str("  sec=");
            out.U64(static_cast<uint64>(sess->GetSecurity()));
            out.Str("  lat=");
            out.U64(sess->GetLatency());

            Player* p = sess->GetPlayer();
            if (p != nullptr)
            {
                out.Str("  name=");
                out.Str(p->GetName());
                out.Str("  lvl=");
                out.U64(p->getLevel());
                out.Str("  map=");
                out.U64(p->GetMapId());
                out.Str("  zone=");
                out.U64(p->GetZoneId());
                out.Str("  guid=");
                out.Str(p->GetObjectGuid().GetString().c_str());
            }
            else
            {
                const char* n = sess->GetPlayerName();
                out.Str("  name=");
                out.Str((n != nullptr && n[0] != '\0') ? n : "(char-select)");
            }
            out.Line();
        }
    } // namespace

    namespace verbs
    {
        void CmdStatus(MonitorWriter& out)
        {
            out.Str("motd: ");
            out.Str(sWorld.GetMotd());
            out.Line();
            out.Str("uptime: ");
            out.Str(secsToTimeString(sWorld.GetUptime()).c_str());
            out.Line();
            out.Str("sessions: active=");
            out.U64(sWorld.GetActiveSessionCount());
            out.Str("  total=");
            out.U64(sWorld.GetAllSessions().size());
            out.Line();
            out.Str("world-tick: ");
            out.U64(static_cast<uint64>(World::m_worldLoopCounter.value()));
            out.Line();
        }

        void CmdPlayers(MonitorWriter& out)
        {
            const SessionMap& sessions = sWorld.GetAllSessions();
            if (sessions.empty())
            {
                out.Str("  (no sessions)\n");
                return;
            }
            for (SessionMap::const_iterator it = sessions.begin(); it != sessions.end(); ++it)
            {
                if (it->second != nullptr)
                {
                    WriteSessionLine(it->second, out);
                }
            }
        }

        void CmdTick(MonitorWriter& out)
        {
            out.Str("world-tick=");
            out.U64(static_cast<uint64>(World::m_worldLoopCounter.value()));
            out.Str("  uptime-sec=");
            out.U64(sWorld.GetUptime());
            out.Line();
        }

        void CmdSession(uint64 accountId, MonitorWriter& out)
        {
            WorldSession* sess = sWorld.FindSession(static_cast<uint32>(accountId));
            if (sess == nullptr)
            {
                out.Str("session: no online session for account ");
                out.U64(accountId);
                out.Line();
                return;
            }
            WriteSessionLine(sess, out);
        }

        void CmdConfig(const char* key, MonitorWriter& out)
        {
            std::string val = sConfig.GetStringDefault(key, "<unset>");
            out.Str(key);
            out.Str(" = ");
            out.Str(val.c_str());
            out.Line();
        }

        void CmdCmd(const char* serverCommand, MonitorWriter& out)
        {
            // Runs on the world thread (monitor dispatch context), so the
            // command executes synchronously and inline — no cliCmdQueue
            // round-trip is needed. Account id 0 + SEC_CONSOLE grants the
            // full command surface, matching the local console.
            CliHandler handler(0, SEC_CONSOLE, &out, &MonitorPrint);
            handler.ParseCommands(serverCommand);
        }

        void CmdDump(MonitorWriter& out)
        {
            out.Str("world-thread backtrace:\n");
#if defined(__linux__)
            void* frames[64];
            const int n = backtrace(frames, 64);
            char** syms = backtrace_symbols(frames, n);
            for (int i = 0; i < n; ++i)
            {
                out.Str("  #");
                out.U64(static_cast<uint64>(i));
                out.Str("  ");
                if (syms != nullptr && syms[i] != nullptr)
                {
                    out.Str(syms[i]);
                }
                else
                {
                    out.Str("0x");
                    out.Hex(reinterpret_cast<uint64>(frames[i]));
                }
                out.Line();
            }
            if (syms != nullptr)
            {
                free(syms);
            }
#elif defined(_WIN32)
            void* frames[64];
            const USHORT n = CaptureStackBackTrace(0, 64, frames, nullptr);
            for (USHORT i = 0; i < n; ++i)
            {
                out.Str("  #");
                out.U64(i);
                out.Str("  0x");
                out.Hex(reinterpret_cast<uint64>(frames[i]));
                out.Line();
            }
            out.Str("  (symbol resolution: see the crash report written on fault)\n");
#else
            out.Str("  (backtrace not supported on this platform)\n");
#endif
        }
    } // namespace verbs
} // namespace GdbMon
/// @}
