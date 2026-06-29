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

/// \addtogroup shared
/// @{
/// \file

#ifndef MANGOS_H_DEBUG_BREAK_HOOK
#define MANGOS_H_DEBUG_BREAK_HOOK

#include "Debug/GdbEvents.h"

#include <cstdint>

/*
 * Lower-layer bridge to the game-level breakpoint engine.
 *
 * The breakpoint engine (GdbBp) lives in the game library, but some systems
 * worth breaking on — most notably the database — live in the shared library,
 * which must not depend on game. This shim lets those shared subsystems raise
 * a breakpoint through two function pointers the game registers at startup:
 * one that returns the current armed-event bitmask (the cheap hot-path gate)
 * and one that handles a matched hit. When nothing is registered (game not
 * built, or endpoint disabled) the call sites are inert.
 */
namespace DbgBreak
{
    /// Returns the current armed-event bitmask (bit per GdbEvent).
    typedef std::uint64_t (*MaskFn)();
    /// Handles a raised event (filter matching + cooperative stop live here).
    typedef void (*HitFn)(std::uint32_t ev, std::uint64_t detail);

    extern MaskFn g_maskFn;
    extern HitFn g_hitFn;

    /// Wire the shim to the game engine. Called once at startup.
    void Register(MaskFn maskFn, HitFn hitFn);

    inline bool Armed(GdbEvent e)
    {
        const MaskFn m = g_maskFn;
        return m != nullptr && ((m() >> static_cast<std::uint32_t>(e)) & 1ULL) != 0;
    }

    inline void Hit(GdbEvent e, std::uint64_t detail)
    {
        if (g_hitFn != nullptr)
        {
            g_hitFn(static_cast<std::uint32_t>(e), detail);
        }
    }
}

/// Shared-layer call-site macro — mirrors GDB_BREAK but routes through the
/// registered bridge. Negligible cost when the event is not armed.
#define GDB_BREAK_SHARED(ev, detail)                                            \
    do {                                                                        \
        if (DbgBreak::Armed(GdbEvent::ev))                                      \
            DbgBreak::Hit(GdbEvent::ev, static_cast<std::uint64_t>(detail));    \
    } while (0)

#endif
/// @}
