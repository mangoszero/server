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

#ifndef MANGOS_H_GDB_DBGMEMORY
#define MANGOS_H_GDB_DBGMEMORY

#include "Common.h"

#include <cstddef>
#include <cstdint>

/**
 * Guarded access to the mangosd process' own address space, used by the
 * GDB-server `m` / `M` packets. The debugger is in-process, so a read is
 * simply a memcpy of our own memory — but a typo'd address from the
 * debugger must NOT crash the server. Each access is therefore validated
 * against the live memory map first:
 *   - Linux: a cached parse of /proc/self/maps (refresh-on-miss).
 *   - Windows: VirtualQuery walked across the requested range.
 * A SIGSEGV-trampoline is deliberately avoided: other server threads keep
 * running during a debug stop and a process-global fault handler would
 * fight with them.
 */
namespace GdbDbg
{
    /// Validate that [addr, addr+len) is fully mapped and readable.
    bool IsReadable(uintptr_t addr, size_t len);

    /// Validate that [addr, addr+len) is fully mapped and writable.
    bool IsWritable(uintptr_t addr, size_t len);

    /// Read up to @p len bytes from @p addr into @p out. Returns the number
    /// of bytes copied (0 when the range is not fully readable).
    size_t MemRead(uintptr_t addr, void* out, size_t len);

    /// Write @p len bytes from @p in to @p addr. Returns the number of bytes
    /// written (0 when the range is not fully writable).
    size_t MemWrite(uintptr_t addr, const void* in, size_t len);
}

#endif
/// @}
