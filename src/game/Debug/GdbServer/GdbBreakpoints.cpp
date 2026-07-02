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

#include "GdbBreakpoints.h"

#include "Debug/DebugBreakHook.h"
#include "GdbMonitor.h"
#include "GdbServer.h"

#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace GdbBp
{
    std::atomic<uint64> g_armedMask{0};

    namespace
    {
        // One armed breakpoint: an event and an optional filter (0 = any).
        struct Entry
        {
            Event ev;
            uint64 filter;
        };

        std::vector<Entry> g_entries;
        uint64 g_hits = 0;

        // Canonical lower-case name per Event, indexed by enum value. Keep in
        // lockstep with the GdbEvent enum in shared/Debug/GdbEvents.h.
        const char* const kEventNames[] = {
            "opcode", "login", "logout", "mapenter", "mapleave",
            "spellcast", "spellprepare", "death", "damage", "levelup",
            "loot", "questaccept", "questcomplete", "questreward",
            "chat", "itemuse", "gossip", "creaturecreate",
            "gobjectuse", "gmcmd", "worldtick",
            "netaccept", "netclose", "authsession", "packetrecv", "packetsend",
            "dbquery", "dbexecute", "dbasyncquery",
            "wardencheck", "wardenviolation",
            "scriptai", "eluna", "sd3",
            "aicombat", "aicombatend", "aiupdate", "aispawn",
            "mapcreate", "gridload", "instancecreate", "instancereset",
            "mailsend", "mailrecv", "auctionadd", "auctionbuy", "trade",
            "bgstart", "bgend", "petsummon", "itemequip", "itemdestroy",
            "groupjoin", "pvpkill", "movementinform",
        };
        static_assert(sizeof(kEventNames) / sizeof(kEventNames[0]) ==
                          static_cast<size_t>(Event::Count),
                      "kEventNames must match the Event enum");

        void Recount()
        {
            uint64 mask = 0;
            for (const Entry& e : g_entries)
            {
                mask |= (1ULL << static_cast<uint32>(e.ev));
            }
            g_armedMask.store(mask, std::memory_order_relaxed);
        }

        bool StrEq(const char* a, const char* b)
        {
            return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
        }
    } // namespace

    bool Matches(Event e, uint64 detail)
    {
        for (const Entry& entry : g_entries)
        {
            if (entry.ev == e && (entry.filter == 0 || entry.filter == detail))
            {
                return true;
            }
        }
        return false;
    }

    bool Arm(Event e, uint64 filter)
    {
        for (const Entry& entry : g_entries)
        {
            if (entry.ev == e && entry.filter == filter)
            {
                return false; // already armed
            }
        }
        g_entries.push_back(Entry{e, filter});
        Recount();
        return true;
    }

    bool Disarm(Event e, uint64 filter)
    {
        for (size_t i = 0; i < g_entries.size(); ++i)
        {
            if (g_entries[i].ev == e && g_entries[i].filter == filter)
            {
                g_entries.erase(g_entries.begin() + i);
                Recount();
                return true;
            }
        }
        return false;
    }

    void DisarmAll()
    {
        g_entries.clear();
        Recount();
    }

    void List(GdbMon::MonitorWriter& out)
    {
        if (g_entries.empty())
        {
            out.Str("  (no breakpoints armed)\n");
        }
        for (const Entry& e : g_entries)
        {
            out.Str("  ");
            out.Str(EventName(e.ev));
            if (e.filter != 0)
            {
                out.Str(" filter=");
                out.U64(e.filter);
            }
            else
            {
                out.Str(" (any)");
            }
            out.Line();
        }
        out.Str("  hits=");
        out.U64(g_hits);
        out.Line();
    }

    bool ParseEvent(const char* name, Event& out)
    {
        for (uint32 i = 0; i < static_cast<uint32>(Event::Count); ++i)
        {
            if (StrEq(name, kEventNames[i]))
            {
                out = static_cast<Event>(i);
                return true;
            }
        }
        return false;
    }

    const char* EventName(Event e)
    {
        const uint32 i = static_cast<uint32>(e);
        return (i < static_cast<uint32>(Event::Count)) ? kEventNames[i] : "?";
    }

    void ListEventNames(GdbMon::MonitorWriter& out)
    {
        for (uint32 i = 0; i < static_cast<uint32>(Event::Count); ++i)
        {
            out.Str(i == 0 ? "  " : " ");
            out.Str(kEventNames[i]);
        }
        out.Line();
    }

    void Hit(Event e, uint64 detail)
    {
        ++g_hits;
        char reason[96];
        snprintf(reason, sizeof(reason), "breakpoint %s %llu", EventName(e),
            static_cast<unsigned long long>(detail));
        // No-op unless a debugger is attached (EnterBreak guards that), so an
        // armed-but-unattended breakpoint never hangs the server.
        sGdbServer.EnterBreak(reason);
    }

    namespace
    {
        // Bridge entry points handed to the shared-layer shim (DbgBreak).
        std::uint64_t CurrentMask()
        {
            return static_cast<std::uint64_t>(g_armedMask.load(std::memory_order_relaxed));
        }

        void SharedHit(std::uint32_t ev, std::uint64_t detail)
        {
            const Event e = static_cast<Event>(ev);
            if (Matches(e, detail))
            {
                Hit(e, detail);
            }
        }
    } // namespace

    void Init()
    {
        DbgBreak::Register(&CurrentMask, &SharedHit);
    }
} // namespace GdbBp
/// @}
