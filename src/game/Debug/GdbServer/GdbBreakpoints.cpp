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

#include "GdbMonitor.h"
#include "GdbServer.h"

#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>

namespace GdbBp
{
    std::atomic<int> g_armedCount{0};

    namespace
    {
        std::set<uint32> g_opcodes;
        std::set<uint32> g_maps;
        std::set<std::string> g_labels;
        uint64 g_hits = 0;

        void Recount()
        {
            g_armedCount.store(static_cast<int>(g_opcodes.size() + g_maps.size() + g_labels.size()),
                std::memory_order_relaxed);
        }
    } // namespace

    bool OpcodeArmed(uint32 opcode) { return g_opcodes.find(opcode) != g_opcodes.end(); }
    bool MapEnterArmed(uint32 mapId) { return g_maps.find(mapId) != g_maps.end(); }
    bool LabelArmed(const char* name) { return name != nullptr && g_labels.find(name) != g_labels.end(); }

    bool ArmOpcode(uint32 opcode)
    {
        const bool inserted = g_opcodes.insert(opcode).second;
        Recount();
        return inserted;
    }

    bool ArmMapEnter(uint32 mapId)
    {
        const bool inserted = g_maps.insert(mapId).second;
        Recount();
        return inserted;
    }

    bool ArmLabel(const char* name)
    {
        if (name == nullptr || name[0] == '\0')
        {
            return false;
        }
        const bool inserted = g_labels.insert(name).second;
        Recount();
        return inserted;
    }

    bool Disarm(const char* spec)
    {
        if (spec == nullptr)
        {
            return false;
        }
        bool removed = false;
        const std::string s(spec);
        if (s.rfind("opcode:", 0) == 0)
        {
            removed = g_opcodes.erase(static_cast<uint32>(strtoul(s.c_str() + 7, nullptr, 0))) != 0;
        }
        else if (s.rfind("map:", 0) == 0)
        {
            removed = g_maps.erase(static_cast<uint32>(strtoul(s.c_str() + 4, nullptr, 0))) != 0;
        }
        else
        {
            removed = g_labels.erase(s) != 0;
        }
        Recount();
        return removed;
    }

    void DisarmAll()
    {
        g_opcodes.clear();
        g_maps.clear();
        g_labels.clear();
        Recount();
    }

    void List(GdbMon::MonitorWriter& out)
    {
        if (g_armedCount.load(std::memory_order_relaxed) == 0)
        {
            out.Str("  (no breakpoints armed)\n");
        }
        for (uint32 op : g_opcodes)
        {
            out.Str("  opcode:");
            out.U64(op);
            out.Line();
        }
        for (uint32 m : g_maps)
        {
            out.Str("  map:");
            out.U64(m);
            out.Line();
        }
        for (const std::string& l : g_labels)
        {
            out.Str("  label:");
            out.Str(l.c_str());
            out.Line();
        }
        out.Str("  hits=");
        out.U64(g_hits);
        out.Line();
    }

    void Hit(const char* kind, uint64 detail)
    {
        ++g_hits;
        char reason[80];
        snprintf(reason, sizeof(reason), "breakpoint %s %llu", kind, static_cast<unsigned long long>(detail));
        // No-op unless a debugger is attached (EnterBreak guards that), so an
        // armed-but-unattended breakpoint never hangs the server.
        sGdbServer.EnterBreak(reason);
    }
} // namespace GdbBp
/// @}
