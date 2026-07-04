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

#ifndef MANGOS_H_GDB_BREAKPOINTS
#define MANGOS_H_GDB_BREAKPOINTS

#include "Common.h"
#include "Debug/GdbEvents.h"

#include <atomic>
#include <cstdint>

namespace GdbMon { class MonitorWriter; }

/*
 * Game-level breakpoints for the GDB-server debug endpoint.
 *
 * Unlike native instruction breakpoints, these fire at semantically
 * meaningful points across the server (a received opcode, a spell cast, a
 * death, a quest step, a player entering a map, ...). Each breakpoint is an
 * (event, filter) pair: the filter is an optional numeric (spell id, map id,
 * quest id, ...); filter 0 means "any". When an armed breakpoint matches AND
 * a debugger is attached, the world thread enters the cooperative stop inline
 * at the call site, so the attached debugger sees the real call stack and can
 * inspect game state via the `monitor` surface, then resume.
 *
 * The hot-path guard is a single relaxed atomic load of a per-event bitmask,
 * so call sites cost effectively nothing when their event is not armed. All
 * arming and matching happens on the world thread (monitor dispatch + game
 * code), so the registry needs no locking.
 */
namespace GdbBp
{
    /// The event catalogue lives in the shared layer (Debug/GdbEvents.h) so
    /// shared subsystems and game code share one set of ids.
    using Event = ::GdbEvent;

    /// Per-event armed bitmask; the hot-path gate. Defined in the .cpp.
    extern std::atomic<uint64> g_armedMask;

    /// Register the shared-layer break bridge (DbgBreak) with this engine so
    /// shared subsystems (e.g. the database) can raise breakpoints. Called
    /// once at startup.
    void Init();

    /// Cheap gate for call sites — true when @p e has any breakpoint armed.
    inline bool Armed(Event e)
    {
        return (g_armedMask.load(std::memory_order_relaxed) >> static_cast<uint32>(e)) & 1ULL;
    }

    /// True when an armed breakpoint for @p e matches @p detail (filter 0 or
    /// filter == detail). Only called after Armed(e) passes.
    bool Matches(Event e, uint64 detail);

    // Management (world thread, from the monitor surface).
    bool Arm(Event e, uint64 filter);    ///< filter 0 = any
    bool Disarm(Event e, uint64 filter);
    void DisarmAll();
    void List(GdbMon::MonitorWriter& out);

    // Name <-> event mapping for the monitor command surface.
    bool ParseEvent(const char* name, Event& out);
    const char* EventName(Event e);
    void ListEventNames(GdbMon::MonitorWriter& out);

    /// A breakpoint matched: record the hit and, if a debugger is attached,
    /// enter the cooperative stop.
    void Hit(Event e, uint64 detail);
}

/// Call-site macro — negligible cost when the event is not armed.
#define GDB_BREAK(ev, detail)                                                   \
    do {                                                                        \
        if (GdbBp::Armed(GdbBp::Event::ev) &&                                   \
            GdbBp::Matches(GdbBp::Event::ev, static_cast<uint64>(detail)))      \
            GdbBp::Hit(GdbBp::Event::ev, static_cast<uint64>(detail));          \
    } while (0)

#endif
/// @}
