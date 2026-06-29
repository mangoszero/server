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

#include "GdbRsp.h"

#include "GdbDbgMemory.h"
#include "GdbMonitor.h"

namespace GdbRsp
{
    namespace
    {
        // GDB's maximum packet — 4 KiB matches the advertised PacketSize and
        // leaves room for a full target.xml chunk and multi-KiB memory reads.
        constexpr uint32 kPacketMax = 4096;

        enum class State : uint8
        {
            Idle,  // waiting for '$'
            Body,  // accumulating packet body until '#'
            Csum1, // first checksum hex digit
            Csum2, // second checksum hex digit
        };

        State g_state = State::Idle;
        uint8 g_packet[kPacketMax];
        uint32 g_packet_len = 0;
        uint8 g_csum_calc = 0;
        uint8 g_csum_recv = 0;

        WriteByte g_sink = nullptr;
        bool g_allow_mem_write = false;

        ResumeAction g_resume_pending = ResumeAction::None;

        uint64 g_packets_received = 0;
        uint64 g_packets_bad_csum = 0;
        uint64 g_packets_handled = 0;

        bool IsHexDigit(uint8 c)
        {
            return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        }

        uint8 HexDigitValue(uint8 c)
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F')
                return 10 + (c - 'A');
            return 0;
        }

        uint8 HexDigitChar(uint8 v)
        {
            return (v < 10) ? uint8('0' + v) : uint8('a' + (v - 10));
        }

        void EmitByte(uint8 b)
        {
            if (g_sink != nullptr)
            {
                g_sink(b);
            }
        }

        bool MatchPrefix(const uint8* body, uint32 len, const char* prefix)
        {
            for (uint32 i = 0; prefix[i] != '\0'; ++i)
            {
                if (i >= len || body[i] != static_cast<uint8>(prefix[i]))
                {
                    return false;
                }
            }
            return true;
        }

        // Send a `$<payload>#<csum>` reply: frame + checksum the raw bytes.
        void SendReply(const char* payload, uint32 len)
        {
            EmitByte('$');
            uint8 csum = 0;
            for (uint32 i = 0; i < len; ++i)
            {
                const uint8 b = static_cast<uint8>(payload[i]);
                EmitByte(b);
                csum = static_cast<uint8>(csum + b);
            }
            EmitByte('#');
            EmitByte(HexDigitChar((csum >> 4) & 0xF));
            EmitByte(HexDigitChar(csum & 0xF));
        }

        void SendCStr(const char* payload)
        {
            uint32 len = 0;
            while (payload[len] != '\0')
            {
                ++len;
            }
            SendReply(payload, len);
        }

        // 24-register x86_64 target description matching the `g` reply
        // exactly, so gdb's amd64-tdep validator accepts our register count
        // instead of falling back to its 154-register default. Ported
        // verbatim from the DuetOS stub.
        const char* TargetXml()
        {
            static const char kTargetXml[] =
                "l<?xml version=\"1.0\"?>"
                "<!DOCTYPE target SYSTEM \"gdb-target.dtd\">"
                "<target>"
                "<architecture>i386:x86-64</architecture>"
                "<feature name=\"org.gnu.gdb.i386.core\">"
                "<reg name=\"rax\" bitsize=\"64\"/>"
                "<reg name=\"rbx\" bitsize=\"64\"/>"
                "<reg name=\"rcx\" bitsize=\"64\"/>"
                "<reg name=\"rdx\" bitsize=\"64\"/>"
                "<reg name=\"rsi\" bitsize=\"64\"/>"
                "<reg name=\"rdi\" bitsize=\"64\"/>"
                "<reg name=\"rbp\" bitsize=\"64\"/>"
                "<reg name=\"rsp\" bitsize=\"64\"/>"
                "<reg name=\"r8\" bitsize=\"64\"/>"
                "<reg name=\"r9\" bitsize=\"64\"/>"
                "<reg name=\"r10\" bitsize=\"64\"/>"
                "<reg name=\"r11\" bitsize=\"64\"/>"
                "<reg name=\"r12\" bitsize=\"64\"/>"
                "<reg name=\"r13\" bitsize=\"64\"/>"
                "<reg name=\"r14\" bitsize=\"64\"/>"
                "<reg name=\"r15\" bitsize=\"64\"/>"
                "<reg name=\"rip\" bitsize=\"64\" type=\"code_ptr\"/>"
                "<reg name=\"eflags\" bitsize=\"32\"/>"
                "<reg name=\"cs\" bitsize=\"32\"/>"
                "<reg name=\"ss\" bitsize=\"32\"/>"
                "<reg name=\"ds\" bitsize=\"32\"/>"
                "<reg name=\"es\" bitsize=\"32\"/>"
                "<reg name=\"fs\" bitsize=\"32\"/>"
                "<reg name=\"gs\" bitsize=\"32\"/>"
                "</feature>"
                "</target>";
            return kTargetXml;
        }

        // Reply to a `g` (read registers) packet. Phase 1: a zeroed 24-reg
        // x86_64 block (16 GPR + rip + eflags + 6 segments) in the canonical
        // little-endian hex order our target.xml declares.
        void HandleReadRegisters()
        {
            // 16 GPR (16 hex) + rip (16) + eflags (8) + 6 segs (8) = 328.
            constexpr uint32 kReplyChars = 16 * 16 + 16 + 8 + 6 * 8;
            char buf[kReplyChars + 1];
            for (uint32 i = 0; i < kReplyChars; ++i)
            {
                buf[i] = '0';
            }
            buf[kReplyChars] = '\0';
            SendReply(buf, kReplyChars);
        }

        // Parse the `m<addr>,<len>` arguments out of the packet starting at
        // index 1. Returns false on a malformed header.
        bool ParseAddrLen(uint32 start, uint64& addr, uint64& len, uint32& consumed)
        {
            addr = 0;
            len = 0;
            uint32 i = start;
            while (i < g_packet_len && g_packet[i] != ',')
            {
                if (!IsHexDigit(g_packet[i]))
                {
                    return false;
                }
                addr = (addr << 4) | HexDigitValue(g_packet[i]);
                ++i;
            }
            if (i >= g_packet_len || g_packet[i] != ',')
            {
                return false;
            }
            ++i;
            while (i < g_packet_len && g_packet[i] != ':')
            {
                if (!IsHexDigit(g_packet[i]))
                {
                    break;
                }
                len = (len << 4) | HexDigitValue(g_packet[i]);
                ++i;
            }
            consumed = i;
            return true;
        }

        void HandleReadMemory()
        {
            uint64 addr = 0;
            uint64 len = 0;
            uint32 consumed = 0;
            if (!ParseAddrLen(1, addr, len, consumed))
            {
                SendCStr("E01");
                return;
            }

            constexpr uint64 kMaxLen = (kPacketMax / 2) - 8;
            if (len > kMaxLen)
            {
                len = kMaxLen;
            }
            if (len == 0)
            {
                SendCStr("00");
                return;
            }

            static uint8 raw[kPacketMax / 2];
            const size_t got = GdbDbg::MemRead(static_cast<uintptr_t>(addr), raw, static_cast<size_t>(len));
            if (got == 0)
            {
                SendCStr("E14"); // EFAULT-style
                return;
            }

            char buf[kPacketMax];
            uint32 off = 0;
            for (size_t k = 0; k < got && off + 2 <= sizeof(buf); ++k)
            {
                buf[off++] = HexDigitChar((raw[k] >> 4) & 0xF);
                buf[off++] = HexDigitChar(raw[k] & 0xF);
            }
            SendReply(buf, off);
        }

        void HandleWriteMemory()
        {
            if (!g_allow_mem_write)
            {
                SendCStr("E01"); // write disabled by config
                return;
            }

            uint64 addr = 0;
            uint64 len = 0;
            uint32 consumed = 0;
            if (!ParseAddrLen(1, addr, len, consumed) || consumed >= g_packet_len || g_packet[consumed] != ':')
            {
                SendCStr("E01");
                return;
            }
            uint32 i = consumed + 1; // skip ':'

            static uint8 raw[kPacketMax / 2];
            uint64 n = 0;
            for (; n < len && n < sizeof(raw); ++n)
            {
                if (i + 1 >= g_packet_len)
                {
                    break;
                }
                const uint8 hi = HexDigitValue(g_packet[i]);
                const uint8 lo = HexDigitValue(g_packet[i + 1]);
                raw[n] = static_cast<uint8>((hi << 4) | lo);
                i += 2;
            }

            const size_t wrote = GdbDbg::MemWrite(static_cast<uintptr_t>(addr), raw, static_cast<size_t>(n));
            SendCStr(wrote == n && n > 0 ? "OK" : "E14");
        }

        // Decode a qRcmd hex payload, dispatch through the mangos monitor,
        // and hex-encode the text reply back to gdb.
        void HandleMonitor()
        {
            static char mon_cmd[1024];
            static char mon_txt[2048];
            static char mon_hex[2 * sizeof(mon_txt)];

            const uint32 hoff = 6; // strlen("qRcmd,")
            uint32 dn = 0;
            for (uint32 i = hoff; i + 1 < g_packet_len && dn + 1 < sizeof(mon_cmd); i += 2)
            {
                mon_cmd[dn++] = static_cast<char>((HexDigitValue(g_packet[i]) << 4) | HexDigitValue(g_packet[i + 1]));
            }
            mon_cmd[dn] = '\0';

            GdbMon::MonitorWriter w(mon_txt, sizeof(mon_txt));
            if (!GdbMon::GdbMonitorDispatch(mon_cmd, dn, w))
            {
                SendCStr(""); // not a "mangos" line — unsupported
                return;
            }

            uint32 ho = 0;
            for (uint32 i = 0; i < w.Len() && ho + 2 < sizeof(mon_hex); ++i)
            {
                const uint8 b = static_cast<uint8>(w.Data()[i]);
                mon_hex[ho++] = HexDigitChar((b >> 4) & 0xF);
                mon_hex[ho++] = HexDigitChar(b & 0xF);
            }
            SendReply(mon_hex, ho);
        }

        // Pick the first action ('c' or 's') out of a vCont; packet.
        void HandleVCont()
        {
            uint32 i = 6; // skip "vCont;"
            while (i < g_packet_len)
            {
                const uint8 act = g_packet[i];
                if (act == 'c' || act == 'C')
                {
                    g_resume_pending = ResumeAction::Continue;
                    break;
                }
                if (act == 's' || act == 'S')
                {
                    g_resume_pending = ResumeAction::Step;
                    break;
                }
                ++i;
            }
            // No explicit reply: gdb expects a stop reply once the target
            // halts again, which the cooperative stop loop emits.
        }

        void HandlePacket()
        {
            ++g_packets_handled;

            if (g_packet_len == 0)
            {
                SendCStr("");
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "qSupported"))
            {
                SendCStr("PacketSize=1000;swbreak+;qXfer:features:read+;vContSupported+");
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "qXfer:features:read:target.xml:"))
            {
                SendCStr(TargetXml());
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "qRcmd,"))
            {
                HandleMonitor();
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "qfThreadInfo"))
            {
                SendCStr("m1"); // one logical thread: the world thread
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "qsThreadInfo"))
            {
                SendCStr("l");
                return;
            }
            if (g_packet_len == 2 && g_packet[0] == 'q' && g_packet[1] == 'C')
            {
                SendCStr("QC1");
                return;
            }
            if (g_packet[0] == 'T' && g_packet_len > 1)
            {
                SendCStr("OK"); // single logical thread is always alive
                return;
            }
            if (g_packet[0] == '?')
            {
                SendCStr("S05"); // SIGTRAP
                return;
            }
            if (g_packet[0] == 'g')
            {
                HandleReadRegisters();
                return;
            }
            if (g_packet[0] == 'G')
            {
                SendCStr("OK"); // Phase 1: synthetic registers, accept + ignore
                return;
            }
            if (g_packet[0] == 'm')
            {
                HandleReadMemory();
                return;
            }
            if (g_packet[0] == 'M')
            {
                HandleWriteMemory();
                return;
            }
            if (g_packet[0] == 'H')
            {
                SendCStr("OK"); // single logical thread — accept any selection
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "vCont?"))
            {
                SendCStr("vCont;c;C;s;S");
                return;
            }
            if (MatchPrefix(g_packet, g_packet_len, "vCont;"))
            {
                HandleVCont();
                return;
            }
            if (g_packet[0] == 'c')
            {
                g_resume_pending = ResumeAction::Continue;
                return;
            }
            if (g_packet[0] == 's')
            {
                g_resume_pending = ResumeAction::Step;
                return;
            }
            if (g_packet[0] == 'D')
            {
                g_resume_pending = ResumeAction::Detached;
                SendCStr("OK");
                return;
            }
            if (g_packet[0] == 'k')
            {
                g_resume_pending = ResumeAction::Killed;
                return; // no reply expected
            }
            // Z / z (breakpoints) are Phase 2 — report unsupported so gdb
            // falls back to its own software breakpoints.
            SendCStr("");
        }

        void ResetParser()
        {
            g_state = State::Idle;
            g_packet_len = 0;
            g_csum_calc = 0;
            g_csum_recv = 0;
        }
    } // namespace

    void SetSink(WriteByte sink)
    {
        g_sink = sink;
    }

    void SetAllowMemWrite(bool allow)
    {
        g_allow_mem_write = allow;
    }

    void ResetSession()
    {
        ResetParser();
        g_resume_pending = ResumeAction::None;
    }

    void ReceiveByte(uint8 byte)
    {
        switch (g_state)
        {
            case State::Idle:
                if (byte == '$')
                {
                    g_packet_len = 0;
                    g_csum_calc = 0;
                    g_state = State::Body;
                }
                break;
            case State::Body:
                if (byte == '#')
                {
                    g_state = State::Csum1;
                }
                else if (g_packet_len < kPacketMax)
                {
                    g_packet[g_packet_len++] = byte;
                    g_csum_calc = static_cast<uint8>(g_csum_calc + byte);
                }
                break;
            case State::Csum1:
                if (IsHexDigit(byte))
                {
                    g_csum_recv = static_cast<uint8>(HexDigitValue(byte) << 4);
                    g_state = State::Csum2;
                }
                else
                {
                    ResetParser();
                }
                break;
            case State::Csum2:
                if (IsHexDigit(byte))
                {
                    g_csum_recv |= HexDigitValue(byte);
                    ++g_packets_received;
                    if (g_csum_recv == g_csum_calc)
                    {
                        EmitByte('+');
                        HandlePacket();
                    }
                    else
                    {
                        ++g_packets_bad_csum;
                        EmitByte('-');
                    }
                }
                ResetParser();
                break;
        }
    }

    ResumeAction TakeResume()
    {
        const ResumeAction r = g_resume_pending;
        g_resume_pending = ResumeAction::None;
        return r;
    }

    void SendStopReply()
    {
        SendCStr("S05");
    }

    uint64 PacketsReceived()
    {
        return g_packets_received;
    }

    uint64 PacketsBadChecksum()
    {
        return g_packets_bad_csum;
    }

    uint64 PacketsHandled()
    {
        return g_packets_handled;
    }
} // namespace GdbRsp
/// @}
