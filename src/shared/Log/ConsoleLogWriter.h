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

#ifndef MANGOS_H_CONSOLELOGWRITER
#define MANGOS_H_CONSOLELOGWRITER

#include <atomic>

#include "LockedQueue/LockedQueue.h"
#include "Threading/Threading.h"
#include "Log.h"

/**
 * @brief Off-thread console writer
 *
 * Drains formatted console log records on a dedicated thread and renders
 * them (SetConsoleTextAttribute + OEM transform already applied by the
 * producer) so that the world/map-update threads never stall on the
 * per-call console flush the Windows CRT performs. Producers format the
 * line and enqueue a ConsoleLogRecord; this single writer thread performs
 * the SetColor + write + ResetColor.
 *
 * The writer MUST NEVER call sLog (no out*): it only ever touches
 * stdout/stderr via Log::SetColor / Log::ResetColor and fwrite, so it is
 * safe to run alongside (and shut down after) the rest of logging.
 */
class ConsoleLogWriter : public MaNGOS::Runnable
{
    public:
        /**
         * @brief Construct the writer in the running state with empty counters
         */
        ConsoleLogWriter();

        /**
         * @brief Producer entry point: enqueue a formatted record
         *
         * Drops and counts the record when the queue is at capacity so a
         * producer is never blocked behind a slow console.
         *
         * @param rec Formatted record to render on the writer thread
         */
        void Enqueue(ConsoleLogRecord& rec);

        /**
         * @brief Cooperative stop: poll target for the drain loop
         */
        void Stop() { m_running = false; }

        /**
         * @brief Drain+render loop; runs on the dedicated writer thread
         */
        virtual void run() override;

    private:
        /**
         * @brief Drain everything currently queued once
         *
         * @return bool true if any record (or drop note) was emitted
         */
        bool DrainOnce();

        /**
         * @brief Render a single record to its target stream
         *
         * @param rec Record to emit
         */
        void Emit(const ConsoleLogRecord& rec);

        typedef MaNGOS::LockedQueue<ConsoleLogRecord> Queue;
        Queue m_queue; /**< Unbounded backing queue (capacity enforced via m_depth) */
        std::atomic<long> m_depth; /**< Approximate pending count */
        std::atomic<long> m_dropped; /**< Records dropped on overflow */
        std::atomic<bool> m_running; /**< Cooperative stop flag (cross-thread: set by Stop(), read by run()) */
        static const long MAX_CONSOLE_QUEUE = 16384; /**< Overflow threshold */
};

#endif
