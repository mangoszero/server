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

#ifndef MANGOS_H_GDB_RSP
#define MANGOS_H_GDB_RSP

#include "Common.h"

/*
 * GDB Remote Serial Protocol (RSP) engine for mangosd. Transport-agnostic:
 * bytes arrive via ReceiveByte() and replies leave through a byte-sink
 * callback, so the engine is fully decoupled from the TCP transport that
 * carries it.
 *
 * mangosd is debugged in-process, which shapes a few packets:
 *   - Registers (g/G): Phase 1 reports a synthetic (zeroed) x86_64 set —
 *     enough for stock gdb's attach handshake; the value for users and AI
 *     is the `monitor` surface + memory reads, not register-level debugging.
 *   - Memory (m/M): reads/writes are guarded accesses to the server's own
 *     address space (see GdbDbg). Writes are gated behind config.
 *   - Breakpoints (Z/z) and real single-step are Phase 2.
 *
 * SINGLE SESSION: one attached debugger at a time. A new connection calls
 * ResetSession() which re-initialises the parser + resume state.
 */
namespace GdbRsp
{
    /// Snapshot of the x86_64 integer register file in GDB's canonical
    /// order, as returned by the `g` packet. When a real snapshot is
    /// published (e.g. captured at a breakpoint or stop point), gdb can
    /// unwind the live stack — it reads stack memory through the `m` packets.
    struct RegSnapshot
    {
        uint64 rax, rbx, rcx, rdx;
        uint64 rsi, rdi, rbp, rsp;
        uint64 r8, r9, r10, r11;
        uint64 r12, r13, r14, r15;
        uint64 rip, rflags;
        uint32 cs, ss, ds, es, fs, gs;
    };

    /// Publish the register snapshot the next `g` packet should report.
    /// Pass nullptr to revert to a zeroed reply. The caller owns the storage
    /// and must keep it alive until a different snapshot is published.
    void PublishRegisters(const RegSnapshot* snap);

    /// Output sink — invoked once per byte the engine wants to send.
    using WriteByte = void (*)(uint8 byte);

    /// Configure the output sink (the active socket writer).
    void SetSink(WriteByte sink);

    /// Allow / forbid the `M` (write memory) packet. Default: forbidden.
    void SetAllowMemWrite(bool allow);

    /// Re-initialise parser + resume state for a freshly attached debugger.
    void ResetSession();

    /// Feed one received byte to the parser. On a complete, checksum-valid
    /// packet this ACKs and dispatches to the relevant handler, emitting the
    /// reply through the sink.
    void ReceiveByte(uint8 byte);

    /// Resume action requested by the debugger via c / s / D / k.
    enum class ResumeAction : uint8
    {
        None,
        Continue,
        Step,
        Detached,
        Killed,
    };

    /// Return and clear any resume action requested since the last call.
    ResumeAction TakeResume();

    /// Emit an unsolicited stop reply ("S05" = SIGTRAP). Used when entering
    /// the cooperative world stop so the attached debugger learns the target
    /// has halted.
    void SendStopReply();

    /// Diagnostic counters.
    uint64 PacketsReceived();
    uint64 PacketsBadChecksum();
    uint64 PacketsHandled();
}

#endif
/// @}
