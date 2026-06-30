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
 */

#include "BrowsePending.h"
#include "ObjectGuid.h"

uint64 BrowsePendingMap::Register(const PendingBrowse& pb, uint32 nowSec)
{
    // D3: at the hard cap, refuse (return 0). A browse flood must not grow the
    // map unbounded between 1s sweeps; the caller serves this browse in-process.
    if (m_map.size() >= MAX_PENDING)
    {
        return 0u;
    }
    uint64 id = m_nextId++;   // 0 is reserved as the "refused" sentinel
    PendingBrowse e = pb;
    e.sentSec = nowSec;
    m_map[id] = e;
    return id;
}

bool BrowsePendingMap::Take(uint64 queryId, PendingBrowse& out)
{
    std::unordered_map<uint64, PendingBrowse>::iterator it = m_map.find(queryId);
    if (it == m_map.end())
    {
        return false;
    }
    out = it->second;
    m_map.erase(it);
    return true;
}

void BrowsePendingMap::Sweep(uint32 nowSec, uint32 ttlSec,
                             std::vector<PendingBrowse>& timedOut)
{
    std::unordered_map<uint64, PendingBrowse>::iterator it = m_map.begin();
    while (it != m_map.end())
    {
        if (nowSec - it->second.sentSec > ttlSec)
        {
            timedOut.push_back(it->second);
            it = m_map.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

uint32 BrowsePendingMap::NextSeqFor(uint32 guid, uint8 kind)
{
    uint32& s = m_seq[SeqKey(guid, kind)];
    return ++s;
}

bool BrowsePendingMap::IsCurrent(uint32 guid, uint8 kind, uint32 seq) const
{
    std::map<uint64, uint32>::const_iterator it = m_seq.find(SeqKey(guid, kind));
    return it != m_seq.end() && it->second == seq;
}

void AhAssembleBrowseListBody(const std::vector<BrowseEntry>& entries,
                              uint32 totalcount, ByteBuffer& out)
{
    out << uint32(entries.size());
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const BrowseEntry& e = entries[i];
        out << uint32(e.id);
        out << uint32(e.itemEntry);
        out << uint32(e.enchantId);
        out << uint32(e.randomPropId);
        out << uint32(e.suffixFactor);
        out << uint32(e.count);
        out << uint32(e.charges);                       // live writes charges as uint32
        out << ObjectGuid(HIGHGUID_PLAYER, e.ownerGuidLow);
        out << uint32(e.startbid);
        out << uint32(e.outbid);
        out << uint32(e.buyout);
        out << uint32(e.timeLeftMs);
        out << ObjectGuid(HIGHGUID_PLAYER, e.bidderGuidLow);
        out << uint32(e.curBid);
    }
    out << uint32(totalcount);
}
