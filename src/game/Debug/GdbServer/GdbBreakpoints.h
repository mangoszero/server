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

#include <atomic>

namespace GdbMon { class MonitorWriter; }

/*
 * Game-level breakpoints for the GDB-server debug endpoint.
 *
 * Unlike native instruction breakpoints, these fire at semantically
 * meaningful points in the server (a received opcode, a player entering a
 * map, a named code label). When one fires AND a debugger is attached, the
 * world thread enters the cooperative stop inline at the call site — so the
 * attached debugger sees the real, live call stack and can inspect game
 * state via the `monitor` surface, then resume.
 *
 * The hot-path guard AnyArmed() is a single relaxed atomic load, so call
 * sites cost effectively nothing when no breakpoint is armed. All arming and
 * checking happens on the world thread (monitor dispatch + game code), so the
 * registry needs no locking.
 */
namespace GdbBp
{
    /// Number of armed breakpoints; the hot-path gate. Defined in the .cpp.
    extern std::atomic<int> g_armedCount;

    /// Cheap gate for call sites — true when any breakpoint is armed.
    inline bool AnyArmed() { return g_armedCount.load(std::memory_order_relaxed) != 0; }

    // Match tests (world thread).
    bool OpcodeArmed(uint32 opcode);
    bool MapEnterArmed(uint32 mapId);
    bool LabelArmed(const char* name);

    // Arming / management (world thread, from the monitor surface).
    bool ArmOpcode(uint32 opcode);
    bool ArmMapEnter(uint32 mapId);
    bool ArmLabel(const char* name);
    bool Disarm(const char* spec); ///< "opcode:<id>" | "map:<id>" | "<label>"
    void DisarmAll();
    void List(GdbMon::MonitorWriter& out);

    /// A breakpoint matched: record the hit and, if a debugger is attached,
    /// enter the cooperative stop. @p kind is a short category, @p detail an
    /// optional numeric (opcode / map id).
    void Hit(const char* kind, uint64 detail);
}

/// Call-site macros — negligible cost when nothing is armed.
#define GDB_BREAK_OPCODE(op)                                                    \
    do {                                                                       \
        if (GdbBp::AnyArmed() && GdbBp::OpcodeArmed(static_cast<uint32>(op)))  \
            GdbBp::Hit("opcode", static_cast<uint64>(op));                     \
    } while (0)

#define GDB_BREAK_MAP_ENTER(mapId)                                             \
    do {                                                                       \
        if (GdbBp::AnyArmed() && GdbBp::MapEnterArmed(static_cast<uint32>(mapId))) \
            GdbBp::Hit("map-enter", static_cast<uint64>(mapId));              \
    } while (0)

#define GDB_BREAK_LABEL(name)                                                  \
    do {                                                                       \
        if (GdbBp::AnyArmed() && GdbBp::LabelArmed(name))                      \
            GdbBp::Hit("label", 0);                                            \
    } while (0)

#endif
/// @}
