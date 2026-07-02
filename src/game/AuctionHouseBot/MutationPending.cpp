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

#include "MutationPending.h"
#include "Log.h"

#include <ctime>

const size_t MutationPendingMap::MAX_PER_PLAYER;
const size_t MutationPendingMap::MAX_TOTAL;

bool MutationPendingMap::CanRegister(uint32 playerGuidLow) const
{
    if (m_map.size() >= MAX_TOTAL)
    {
        return false;
    }

    std::unordered_map<uint32, size_t>::const_iterator itr = m_perPlayer.find(playerGuidLow);
    if (itr != m_perPlayer.end() && itr->second >= MAX_PER_PLAYER)
    {
        return false;
    }

    return true;
}

void MutationPendingMap::Register(PendingMutation const& pm)
{
    std::pair<std::unordered_map<uint64, PendingMutation>::iterator, bool> ins =
        m_map.insert(std::make_pair(pm.uuid, pm));
    if (!ins.second)
    {
        // uuids are mint-unique; a duplicate Register is a caller bug.
        sLog.outError("SP-2 MutationPending: duplicate Register for uuid " UI64FMTD
                      " (player %u, op 0x%04X); replacing", pm.uuid,
                      pm.playerGuidLow, uint32(pm.op));
        ins.first->second = pm;
        return;
    }

    ++m_perPlayer[pm.playerGuidLow];
}

bool MutationPendingMap::Take(uint64 uuid, PendingMutation& out)
{
    std::unordered_map<uint64, PendingMutation>::iterator itr = m_map.find(uuid);
    if (itr == m_map.end())
    {
        return false;
    }

    out = itr->second;

    std::unordered_map<uint32, size_t>::iterator pc = m_perPlayer.find(out.playerGuidLow);
    if (pc != m_perPlayer.end())
    {
        if (pc->second <= 1u)
        {
            m_perPlayer.erase(pc);
        }
        else
        {
            --pc->second;
        }
    }

    m_map.erase(itr);
    return true;
}

bool MutationPendingMap::Tombstone(uint64 uuid)
{
    std::unordered_map<uint64, PendingMutation>::iterator itr = m_map.find(uuid);
    if (itr == m_map.end())
    {
        return false;
    }

    itr->second.state = uint8(PMUT_TOMBSTONE);
    return true;
}

void MutationPendingMap::RearmConfirm(uint64 uuid, uint32 nowSec)
{
    std::unordered_map<uint64, PendingMutation>::iterator itr = m_map.find(uuid);
    if (itr == m_map.end())
    {
        return;
    }

    itr->second.state   = uint8(PMUT_AWAIT_CONFIRM);
    itr->second.sentSec = nowSec;
}

void MutationPendingMap::SweepToTombstones(uint32 nowSec, uint32 ttlSec,
                                           std::vector<uint64>& newlyInDoubt)
{
    for (std::unordered_map<uint64, PendingMutation>::iterator itr = m_map.begin();
         itr != m_map.end(); ++itr)
    {
        if (itr->second.state == uint8(PMUT_TOMBSTONE))
        {
            continue;
        }

        if (nowSec - itr->second.sentSec <= ttlSec)
        {
            continue;
        }

        itr->second.state = uint8(PMUT_TOMBSTONE);
        newlyInDoubt.push_back(itr->first);
    }
}

bool MutationPendingMap::Peek(uint64 uuid, PendingMutation& out) const
{
    std::unordered_map<uint64, PendingMutation>::const_iterator itr = m_map.find(uuid);
    if (itr == m_map.end())
    {
        return false;
    }

    out = itr->second;
    return true;
}

bool MutationPendingMap::SetReserve(uint64 uuid, uint32 amount, std::string const& key)
{
    std::unordered_map<uint64, PendingMutation>::iterator itr = m_map.find(uuid);
    if (itr == m_map.end())
    {
        return false;
    }

    itr->second.reservedAmount = amount;
    itr->second.reserveKey     = key;
    return true;
}

void MutationPendingMap::SnapshotInflight(std::vector<PendingMutation>& out) const
{
    out.reserve(out.size() + m_map.size());
    for (std::unordered_map<uint64, PendingMutation>::const_iterator itr = m_map.begin();
         itr != m_map.end(); ++itr)
    {
        out.push_back(itr->second);
    }
}

uint64 AhMintMutationUuid()
{
    // World-thread only. See header note on the composition.
    static uint64 s_base = uint64(uint32(time(NULL))) << 32;
    static uint32 s_seq  = 0u;
    ++s_seq;
    return s_base | uint64(s_seq);
}
