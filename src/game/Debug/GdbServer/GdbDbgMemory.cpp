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

#include "GdbDbgMemory.h"

#include <cstring>
#include <mutex>
#include <vector>

#ifdef _WIN32
#  include <windows.h>
#else
#  include <cstdio>
#endif

namespace GdbDbg
{
    namespace
    {
        // Guards both the validation path and the memcpy so a concurrent
        // map change on another thread cannot be observed mid-copy.
        std::mutex g_lock;

#ifndef _WIN32
        struct Region
        {
            uintptr_t start;
            uintptr_t end;
            bool readable;
            bool writable;
        };

        std::vector<Region> g_regions;

        // (Re)parse /proc/self/maps into g_regions. Lines look like:
        //   7f...000-7f...000 rw-p 00000000 00:00 0   [heap]
        void ParseMaps()
        {
            g_regions.clear();

            FILE* f = fopen("/proc/self/maps", "r");
            if (f == nullptr)
            {
                return;
            }

            char line[512];
            while (fgets(line, sizeof(line), f) != nullptr)
            {
                unsigned long long start = 0;
                unsigned long long end = 0;
                char perms[8] = {0};
                if (sscanf(line, "%llx-%llx %7s", &start, &end, perms) != 3)
                {
                    continue;
                }

                Region r;
                r.start = static_cast<uintptr_t>(start);
                r.end = static_cast<uintptr_t>(end);
                r.readable = (perms[0] == 'r');
                r.writable = (perms[1] == 'w');
                g_regions.push_back(r);
            }
            fclose(f);
        }

        // True when [addr, addr+len) lies entirely inside one region that
        // satisfies the requested protection. Refreshes the cache once on a
        // miss (the map may have grown since the last parse).
        bool RangeOk(uintptr_t addr, size_t len, bool needWrite)
        {
            if (len == 0)
            {
                return false;
            }
            // Reject wraparound.
            if (addr + len < addr)
            {
                return false;
            }

            for (int attempt = 0; attempt < 2; ++attempt)
            {
                for (const Region& r : g_regions)
                {
                    if (addr >= r.start && (addr + len) <= r.end)
                    {
                        if (!r.readable)
                        {
                            return false;
                        }
                        if (needWrite && !r.writable)
                        {
                            return false;
                        }
                        return true;
                    }
                }
                if (attempt == 0)
                {
                    ParseMaps();
                }
            }
            return false;
        }
#else  // _WIN32
        bool RangeOk(uintptr_t addr, size_t len, bool needWrite)
        {
            if (len == 0)
            {
                return false;
            }
            if (addr + len < addr)
            {
                return false;
            }

            uintptr_t cur = addr;
            const uintptr_t last = addr + len;
            while (cur < last)
            {
                MEMORY_BASIC_INFORMATION mbi;
                if (VirtualQuery(reinterpret_cast<LPCVOID>(cur), &mbi, sizeof(mbi)) == 0)
                {
                    return false;
                }
                if (mbi.State != MEM_COMMIT)
                {
                    return false;
                }

                const DWORD prot = mbi.Protect;
                if ((prot & PAGE_GUARD) || (prot & PAGE_NOACCESS))
                {
                    return false;
                }

                const DWORD readMask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                                       PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
                if ((prot & readMask) == 0)
                {
                    return false;
                }
                if (needWrite)
                {
                    const DWORD writeMask = PAGE_READWRITE | PAGE_WRITECOPY |
                                            PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
                    if ((prot & writeMask) == 0)
                    {
                        return false;
                    }
                }

                const uintptr_t regionEnd =
                    reinterpret_cast<uintptr_t>(mbi.BaseAddress) + static_cast<uintptr_t>(mbi.RegionSize);
                if (regionEnd <= cur)
                {
                    return false;
                }
                cur = regionEnd;
            }
            return true;
        }
#endif
    } // namespace

    bool IsReadable(uintptr_t addr, size_t len)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        return RangeOk(addr, len, false);
    }

    bool IsWritable(uintptr_t addr, size_t len)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        return RangeOk(addr, len, true);
    }

    size_t MemRead(uintptr_t addr, void* out, size_t len)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (!RangeOk(addr, len, false))
        {
            return 0;
        }
        memcpy(out, reinterpret_cast<const void*>(addr), len);
        return len;
    }

    size_t MemWrite(uintptr_t addr, const void* in, size_t len)
    {
        std::lock_guard<std::mutex> guard(g_lock);
        if (!RangeOk(addr, len, true))
        {
            return 0;
        }
        memcpy(reinterpret_cast<void*>(addr), in, len);
        return len;
    }
} // namespace GdbDbg
/// @}
