/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace ai
{
    struct PlayerbotPerformanceSnapshot
    {
        std::uint64_t builtPacketCount = 0;
        std::uint64_t builtPacketBytes = 0;
        std::uint64_t eventQueryCount = 0;
        std::uint64_t eventQueryTotalMicros = 0;
        std::uint64_t eventQueryMaxMicros = 0;
        std::uint64_t aiEvaluationCount = 0;
        std::uint64_t aiDeferredCount = 0;
        std::uint64_t aiEvaluationTotalMicros = 0;
        std::uint64_t aiEvaluationMaxMicros = 0;
    };

    class PlayerbotPerformanceMonitor
    {
        public:
            PlayerbotPerformanceMonitor()
                : m_nextReportAtMicros(0),
                m_builtPacketCount(0),
                m_builtPacketBytes(0),
                m_eventQueryCount(0),
                m_eventQueryTotalMicros(0),
                m_eventQueryMaxMicros(0),
                m_aiEvaluationCount(0),
                m_aiDeferredCount(0),
                m_aiEvaluationTotalMicros(0),
                m_aiEvaluationMaxMicros(0)
            {
            }

            void RecordBuiltPacket(std::size_t bytes)
            {
                m_builtPacketCount.fetch_add(1, std::memory_order_relaxed);
                m_builtPacketBytes.fetch_add(bytes, std::memory_order_relaxed);
            }

            void RecordEventQuery(std::uint64_t durationMicros)
            {
                m_eventQueryCount.fetch_add(1, std::memory_order_relaxed);
                m_eventQueryTotalMicros.fetch_add(durationMicros, std::memory_order_relaxed);
                UpdateMax(m_eventQueryMaxMicros, durationMicros);
            }

            void RecordAiEvaluation(std::uint64_t durationMicros)
            {
                m_aiEvaluationCount.fetch_add(1, std::memory_order_relaxed);
                m_aiEvaluationTotalMicros.fetch_add(durationMicros, std::memory_order_relaxed);
                UpdateMax(m_aiEvaluationMaxMicros, durationMicros);
            }

            void RecordAiDeferred()
            {
                m_aiDeferredCount.fetch_add(1, std::memory_order_relaxed);
            }

            bool TakeSnapshotIfDue(std::uint64_t nowMicros, std::uint64_t intervalMicros,
                PlayerbotPerformanceSnapshot& snapshot)
            {
                if (!intervalMicros)
                {
                    return false;
                }

                std::uint64_t nextReportAtMicros = m_nextReportAtMicros.load(std::memory_order_relaxed);
                if (!nextReportAtMicros)
                {
                    m_nextReportAtMicros.compare_exchange_strong(nextReportAtMicros,
                        nowMicros + intervalMicros, std::memory_order_relaxed);
                    return false;
                }

                // Re-arm from observation time so a stalled tick yields one wider nominal
                // window instead of a burst of catch-up reports.
                if (nowMicros < nextReportAtMicros ||
                    !m_nextReportAtMicros.compare_exchange_strong(nextReportAtMicros,
                        nowMicros + intervalMicros, std::memory_order_acq_rel, std::memory_order_relaxed))
                {
                    return false;
                }

                snapshot.builtPacketCount = m_builtPacketCount.exchange(0, std::memory_order_acq_rel);
                snapshot.builtPacketBytes = m_builtPacketBytes.exchange(0, std::memory_order_acq_rel);
                snapshot.eventQueryCount = m_eventQueryCount.exchange(0, std::memory_order_acq_rel);
                snapshot.eventQueryTotalMicros = m_eventQueryTotalMicros.exchange(0, std::memory_order_acq_rel);
                snapshot.eventQueryMaxMicros = m_eventQueryMaxMicros.exchange(0, std::memory_order_acq_rel);
                snapshot.aiEvaluationCount = m_aiEvaluationCount.exchange(0, std::memory_order_acq_rel);
                snapshot.aiDeferredCount = m_aiDeferredCount.exchange(0, std::memory_order_acq_rel);
                snapshot.aiEvaluationTotalMicros = m_aiEvaluationTotalMicros.exchange(0, std::memory_order_acq_rel);
                snapshot.aiEvaluationMaxMicros = m_aiEvaluationMaxMicros.exchange(0, std::memory_order_acq_rel);
                return true;
            }

        private:
            static void UpdateMax(std::atomic<std::uint64_t>& maximum, std::uint64_t value)
            {
                std::uint64_t current = maximum.load(std::memory_order_relaxed);
                while (current < value &&
                    !maximum.compare_exchange_weak(current, value, std::memory_order_relaxed))
                {
                }
            }

            std::atomic<std::uint64_t> m_nextReportAtMicros;
            std::atomic<std::uint64_t> m_builtPacketCount;
            std::atomic<std::uint64_t> m_builtPacketBytes;
            std::atomic<std::uint64_t> m_eventQueryCount;
            std::atomic<std::uint64_t> m_eventQueryTotalMicros;
            std::atomic<std::uint64_t> m_eventQueryMaxMicros;
            std::atomic<std::uint64_t> m_aiEvaluationCount;
            std::atomic<std::uint64_t> m_aiDeferredCount;
            std::atomic<std::uint64_t> m_aiEvaluationTotalMicros;
            std::atomic<std::uint64_t> m_aiEvaluationMaxMicros;
    };

    inline std::uint64_t PlayerbotPerformanceNowMicros()
    {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }

    extern PlayerbotPerformanceMonitor sPlayerbotPerformanceMonitor;

    void ReportPlayerbotPerformanceIfDue(std::uint32_t intervalSeconds);
}
