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

#ifndef MANGOS_H_EVENTPROCESSOR
#define MANGOS_H_EVENTPROCESSOR

#include <cstdint>
#include <map>
#include <memory>

namespace Events
{

class EventProcessor;

/**
 * @brief Something to happen later. All times are milliseconds.
 *
 * Subclass it, override Execute, hand it to an EventProcessor. The processor
 * owns it from that moment until one of two things happens:
 *
 *   Execute returns TRUE   the event is finished and the processor destroys it.
 *
 *   Execute returns FALSE  the event has RE-INSERTED ITSELF, into this
 *                          processor or another one, and the processor gives up
 *                          ownership without destroying it.
 *
 * The second is not a hypothetical: a spell in flight re-adds itself on every
 * update until it lands, sometimes into a different caster's processor. It is
 * also the sharp edge -- an Execute that returns false without re-adding leaks
 * the event, silently and forever, and nothing can detect it from here.
 */
class BasicEvent
{
    public:

        BasicEvent() = default;
        virtual ~BasicEvent() = default;

        // Events are owned through pointers and identified by address; copying
        // one would produce a second object the processor knows nothing about.
        BasicEvent(const BasicEvent&) = delete;
        BasicEvent& operator=(const BasicEvent&) = delete;

        /**
         * @param executionTime The processor's clock at the moment of execution.
         * @param elapsed       Milliseconds since the previous update.
         * @return true to be destroyed, false if the event has re-added itself.
         */
        virtual bool Execute(std::uint64_t /*executionTime*/, std::uint32_t /*elapsed*/)
        {
            return true;
        }

        /// False to survive a non-forced KillAllEvents.
        virtual bool IsDeletable() const { return true; }

        /// Called instead of Execute when the event is cancelled. Exactly once.
        virtual void Abort(std::uint64_t /*executionTime*/) {}

        /**
         * @brief Ask for this event to be cancelled rather than executed.
         *
         * A request from outside, so it is writable from outside -- but paired
         * with a separate flag the processor owns, so "someone asked" and "the
         * abort has been delivered" stay distinct.
         */
        void RequestAbort() { m_abortRequested = true; }
        bool IsAbortRequested() const { return m_abortRequested; }

        /// When the event was queued, and when it is due.
        std::uint64_t AddedAt() const { return m_addTime; }
        std::uint64_t ScheduledFor() const { return m_execTime; }

    private:

        friend class EventProcessor;

        bool m_abortRequested = false;

        /**
         * @brief Whether Abort() has already been delivered.
         *
         * A non-forced KillAllEvents aborts every event but leaves the ones that
         * are not yet deletable in the queue; a later Update finds them and
         * destroys them. Without this flag it would abort them a SECOND time --
         * and an Abort that releases a resource, refunds a cost or notifies a
         * player would do it twice, with nothing in the event's own code to
         * suggest that could happen.
         */
        bool m_aborted = false;

        std::uint64_t m_addTime = 0;
        std::uint64_t m_execTime = 0;

        /// Which Update pass queued this, so a same-tick re-add waits for the
        /// next one instead of spinning inside the current loop.
        std::uint64_t m_queuedPass = 0;
};

/**
 * @brief A time-ordered queue of events, driven by the owner's update tick.
 *
 * One of these lives on every Unit and every Player. It is not thread-safe and
 * does not need to be: it is driven from the thread that updates its owner.
 */
class EventProcessor
{
    public:

        EventProcessor() = default;
        ~EventProcessor();

        EventProcessor(const EventProcessor&) = delete;
        EventProcessor& operator=(const EventProcessor&) = delete;

        /// Advance the clock and run everything now due.
        void Update(std::uint32_t elapsed);

        /**
         * @brief Cancel everything.
         *
         * @param force true destroys every event regardless of IsDeletable().
         *              false leaves undeletable events queued -- they will be
         *              destroyed by a later Update, without a second Abort.
         */
        void KillAllEvents(bool force);

        /**
         * @brief Queue an event, taking ownership.
         *
         * @return false if the processor is tearing down, in which case the
         *         event is aborted and destroyed rather than queued.
         *
         * An Abort handler is exactly where a dying object queues its cleanup,
         * so insertion during teardown is a real path and must be refused --
         * anything accepted then would be dropped by the teardown that is
         * already in progress. The refusal is reported rather than swallowed, so
         * a caller that queued something important finds out.
         */
        bool AddEvent(std::unique_ptr<BasicEvent> event,
                      std::uint64_t executionTime,
                      bool setAddTime = true);

        /**
         * @brief Re-queue an event that is executing right now.
         *
         * ONLY legal from inside that event's own Execute, which must then
         * return false. That pair is the contract: Execute says "I did not
         * finish", and this says where the still-living event went.
         *
         * Takes a raw pointer BECAUSE the event is mid-Execute and cannot hand
         * over a unique_ptr to itself. The processor running it releases
         * ownership precisely when Execute returns false, so exactly one owner
         * exists at every moment.
         *
         * @return false if the target processor is tearing down, or if the event
         *         is already queued; the event is then NOT adopted and the
         *         caller still owns it -- which means Execute must return true.
         */
        bool Reschedule(BasicEvent* event,
                        std::uint64_t executionTime,
                        bool setAddTime = false);

        /// The processor's clock plus an offset -- how callers name a due time.
        std::uint64_t CalculateTime(std::uint64_t offset) const { return m_time + offset; }

        std::uint64_t Now() const { return m_time; }

        bool IsEmpty() const { return m_events.empty(); }

    private:

        /// Deliver Abort exactly once. Returns false if it had already been sent.
        static bool DeliverAbort(BasicEvent& event, std::uint64_t time);

        std::multimap<std::uint64_t, std::unique_ptr<BasicEvent>> m_events;

        std::uint64_t m_time = 0;
        std::uint64_t m_pass = 0;
        bool m_aborting = false;

        /**
         * @brief A forced kill was asked for while a non-forced one was running.
         *
         * The non-forced pass moves the queue into a local before running any
         * Abort handler, so a forced kill reaching the processor from inside one
         * finds m_events already empty and destroys nothing. Left at that, the
         * outer pass would then re-queue its undeletable survivors and the
         * caller's "force" would have been silently downgraded. This flag is how
         * the outer pass learns it must drop them instead.
         */
        bool m_forceRequested = false;
};

} // namespace Events

// The server declares its events and its processors unqualified, in game headers
// that have nothing else to say about namespaces. Hoisting the two names keeps
// the structure aligned with the shared tree without renaming every call site.
using Events::BasicEvent;
using Events::EventProcessor;

#endif
