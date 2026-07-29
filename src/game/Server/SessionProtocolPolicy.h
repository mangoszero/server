/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOS_H_SESSIONPROTOCOLPOLICY
#define MANGOS_H_SESSIONPROTOCOLPOLICY

#include "Platform/Define.h"

#include <chrono>

class SessionPingTracker
{
    public:
        using Clock = std::chrono::steady_clock;

        uint32 Record(Clock::time_point now)
        {
            if (!m_hadPing)
            {
                m_hadPing = true;
                m_lastPing = now;
                return 0;
            }

            bool fast = now - m_lastPing < std::chrono::seconds(27);
            m_lastPing = now;
            if (fast)
                return ++m_fastRun;

            m_fastRun = 0;
            return 0;
        }

        bool ShouldKick(uint32 maximum, bool ordinaryPlayer) const
        {
            return maximum != 0 && m_fastRun > maximum && ordinaryPlayer;
        }

    private:
        Clock::time_point m_lastPing{};
        bool m_hadPing = false;
        uint32 m_fastRun = 0;
};

#endif
