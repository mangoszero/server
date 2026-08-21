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

/**
 * @file EventProcessorHostileTest.cpp
 * @brief EventProcessor driven by events that fight back.
 *
 * Every event handler here is game code, and game code is allowed to do the
 * worst thing available to it at that moment: queue another event from inside an
 * Abort, destroy the whole queue from inside an Abort, re-add itself twice from
 * inside its own Execute, or re-add itself for the very instant that is being
 * processed. Each of those reaches the processor while it is mid-operation on
 * its own container, and each has a wrong answer that looks like nothing at all
 * until a player logs out during a spell cast.
 *
 * The happy path gets one case at the end. Everything above it is the point.
 */

#include "TestHarness.h"

#include "Utilities/EventProcessor.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace
{
    /// Fires up to a cap, then reports itself finished.
    struct CountingEvent : BasicEvent
    {
        CountingEvent(int& fires, int cap)
            : m_fires(fires)
            , m_cap(cap)
        {
        }

        bool Execute(std::uint64_t, std::uint32_t) override
        {
            ++m_fires;
            return m_fires >= m_cap;
        }

        int& m_fires;
        int m_cap;
    };

    /// Counts how many times Abort reaches it. Must never exceed one.
    struct AbortCountingEvent : BasicEvent
    {
        AbortCountingEvent(int& aborts, bool deletable)
            : m_aborts(aborts)
            , m_deletable(deletable)
        {
        }

        void Abort(std::uint64_t) override { ++m_aborts; }
        bool IsDeletable() const override { return m_deletable; }

        int& m_aborts;
        bool m_deletable;
    };

    /// Tries to adopt itself twice from one Execute.
    struct DoubleRescheduleEvent : BasicEvent
    {
        DoubleRescheduleEvent(EventProcessor& processor, bool& secondAccepted)
            : m_processor(processor)
            , m_secondAccepted(secondAccepted)
        {
        }

        bool Execute(std::uint64_t, std::uint32_t) override
        {
            const bool first = m_processor.Reschedule(this, m_processor.CalculateTime(10));
            const bool second = m_processor.Reschedule(this, m_processor.CalculateTime(20));
            m_secondAccepted = second;

            // If the second adoption succeeded, two unique_ptrs alias this
            // object and returning false would leave both of them live. The
            // contract is that the second is refused; returning "still owned by
            // the queue" is only safe because of that.
            return !first;
        }

        EventProcessor& m_processor;
        bool& m_secondAccepted;
    };

    /// Re-adds itself for the instant currently being processed.
    struct SameTickEvent : BasicEvent
    {
        SameTickEvent(EventProcessor& processor, int& fires)
            : m_processor(processor)
            , m_fires(fires)
        {
        }

        bool Execute(std::uint64_t, std::uint32_t) override
        {
            ++m_fires;
            if (m_fires >= 32)
            {
                return true;
            }

            return !m_processor.Reschedule(this, m_processor.Now());
        }

        EventProcessor& m_processor;
        int& m_fires;
    };

    /// Queues a new event from inside its own Abort.
    struct AbortAddsEvent : BasicEvent
    {
        AbortAddsEvent(EventProcessor& processor, int& accepted, int& fires)
            : m_processor(processor)
            , m_accepted(accepted)
            , m_fires(fires)
        {
        }

        void Abort(std::uint64_t) override
        {
            if (m_processor.AddEvent(
                    std::unique_ptr<BasicEvent>(new CountingEvent(m_fires, 1)),
                    m_processor.CalculateTime(1)))
            {
                ++m_accepted;
            }
        }

        EventProcessor& m_processor;
        int& m_accepted;
        int& m_fires;
    };

    /// Destroys the whole queue from inside its own Abort.
    struct AbortKillsEvent : BasicEvent
    {
        AbortKillsEvent(EventProcessor& processor, bool force)
            : m_processor(processor)
            , m_force(force)
        {
        }

        void Abort(std::uint64_t) override { m_processor.KillAllEvents(m_force); }

        EventProcessor& m_processor;
        bool m_force;
    };
}

// ===== 1. Unhappy path: refusals =====

TEST(EventProcessor_NullEventIsRefused)
{
    EventProcessor processor;

    CHECK(!processor.AddEvent(nullptr, 1));
    CHECK(processor.IsEmpty());

    CHECK(!processor.Reschedule(nullptr, 1));
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_EmptyUpdateStillAdvancesTheClock)
{
    EventProcessor processor;

    processor.Update(50);

    CHECK(processor.Now() == 50);
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_AddDuringForcedKillIsRefused)
{
    // An Abort handler is where a dying object queues its cleanup. Accepting it
    // would drop the event on the floor: the teardown that triggered the Abort
    // has already decided what it is destroying.
    EventProcessor processor;
    int accepted = 0;
    int fires = 0;

    processor.AddEvent(
        std::unique_ptr<BasicEvent>(new AbortAddsEvent(processor, accepted, fires)), 100);
    processor.KillAllEvents(true);

    CHECK(accepted == 0);
    CHECK(fires == 0);
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_RefusedAddStillAbortsTheEvent)
{
    // Refusing must not mean leaking: whatever the event was holding has to be
    // released, so the refusal path delivers Abort on the way out.
    EventProcessor processor;
    int aborts = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortKillsEvent(processor, true)), 10);

    // Drive a forced kill; the handler above re-enters KillAllEvents, and while
    // it runs the guard is set, so this insertion is refused.
    int accepted = 0;
    int fires = 0;
    processor.AddEvent(
        std::unique_ptr<BasicEvent>(new AbortAddsEvent(processor, accepted, fires)), 20);
    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, true)), 30);

    processor.KillAllEvents(true);

    CHECK(accepted == 0);
    CHECK(aborts == 1);
    CHECK(processor.IsEmpty());
}

// ===== 2. Hostile: handlers that re-enter the processor =====

TEST(EventProcessor_AbortIsDeliveredExactlyOnceAcrossANonForcedKill)
{
    // ===== THE DOUBLE ABORT =====
    //
    // A non-forced kill aborts everything but leaves undeletable events queued;
    // a later Update finds them and destroys them. Without a flag recording that
    // the abort was already sent, that second visit calls Abort AGAIN -- and an
    // Abort that refunds a reagent, releases a lock or notifies a player does it
    // twice, with nothing in the event's own code to suggest it could happen.
    // ============================
    EventProcessor processor;
    int aborts = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, false)), 10);

    processor.KillAllEvents(false);
    CHECK(aborts == 1);
    CHECK(!processor.IsEmpty());     // undeletable, so it stayed queued

    processor.Update(100);           // now due; destroyed here
    CHECK(aborts == 1);
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_ForcedKillFromInsideANonForcedKillDoesNotCorruptTheQueue)
{
    // Abort is virtual and runs game code, and that code may destroy the whole
    // queue. A non-forced kill that walks m_events in place is holding an
    // iterator into a container the inner call swaps out from under it.
    EventProcessor processor;
    int aborts = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortKillsEvent(processor, true)), 10);
    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, true)), 20);
    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, false)), 30);

    processor.KillAllEvents(false);

    // Both siblings were aborted, once each, and nothing was walked after the
    // container it lived in had been swapped away.
    CHECK(aborts == 2);

    // ===== AND THE FORCE IS NOT DOWNGRADED =====
    //
    // The undeletable sibling would ordinarily survive a non-forced kill. A
    // handler asked for a FORCED one, and that request found an empty queue
    // because the outer pass had already moved it aside -- so the outer pass is
    // what has to honour it. Re-queueing the survivor here would mean "force"
    // silently became "not force" whenever it arrived from an Abort.
    // ===========================================
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_ForceRequestDoesNotLeakIntoTheNextKill)
{
    // The flag above must not outlive the call that raised it: a later,
    // unrelated non-forced kill would then drop survivors nobody asked to drop.
    EventProcessor processor;
    int aborts = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortKillsEvent(processor, true)), 10);
    processor.KillAllEvents(true);
    CHECK(processor.IsEmpty());

    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, false)), 20);
    processor.KillAllEvents(false);

    CHECK(aborts == 1);
    CHECK(!processor.IsEmpty());     // undeletable, and no force was requested
}

TEST(EventProcessor_NestedKillDoesNotClearTheGuardEarly)
{
    // An inner KillAllEvents that finishes by storing `false` leaves the OUTER
    // call delivering aborts with the guard down. The probe has to be an Abort
    // that tries to queue something, ordered AFTER the nested killer -- by then
    // the inner call has returned, which is exactly the open window.
    EventProcessor processor;
    int accepted = 0;
    int fires = 0;
    int aborts = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortKillsEvent(processor, true)), 100);
    processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, true)), 200);
    processor.AddEvent(
        std::unique_ptr<BasicEvent>(new AbortAddsEvent(processor, accepted, fires)), 300);

    processor.KillAllEvents(true);

    CHECK(processor.IsEmpty());
    CHECK(aborts == 1);
    CHECK(accepted == 0);
    CHECK(fires == 0);
}

TEST(EventProcessor_SecondRescheduleOfTheSameEventIsRefused)
{
    // Two adoptions of one object put two unique_ptrs on it; the second
    // destruction is a double free. The refusal is what makes returning "the
    // queue owns me" safe for the caller.
    EventProcessor processor;
    bool secondAccepted = true;

    processor.AddEvent(
        std::unique_ptr<BasicEvent>(new DoubleRescheduleEvent(processor, secondAccepted)), 0);
    processor.Update(1);

    CHECK(!secondAccepted);

    // Destroying the processor here is itself the check: with two owners this
    // would double-free.
    processor.KillAllEvents(true);
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_SameTickRescheduleIsNotAnUnboundedLoop)
{
    // Re-adding for the instant being processed must be deferred to the next
    // pass. Picking it up again immediately makes Update never return, which on
    // a live server is a hung world thread rather than a crash.
    EventProcessor processor;
    int fires = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new SameTickEvent(processor, fires)), 0);
    processor.Update(1);

    CHECK(fires == 1);
    CHECK(!processor.IsEmpty());

    processor.Update(1);
    CHECK(fires == 2);
}

// ===== 3. Lifetime =====

TEST(EventProcessor_DestructorAbortsAndDestroysWhatIsStillQueued)
{
    int aborts = 0;
    int deletableAborts = 0;

    {
        EventProcessor processor;
        processor.AddEvent(std::unique_ptr<BasicEvent>(new AbortCountingEvent(aborts, false)), 10);
        processor.AddEvent(
            std::unique_ptr<BasicEvent>(new AbortCountingEvent(deletableAborts, true)), 20);
    }

    // Undeletable is not un-destroyable: a forced teardown takes everything.
    CHECK(aborts == 1);
    CHECK(deletableAborts == 1);
}

TEST(EventProcessor_AbortRequestedEventIsNotExecuted)
{
    EventProcessor processor;
    int fires = 0;

    auto event = std::unique_ptr<BasicEvent>(new CountingEvent(fires, 1));
    BasicEvent* raw = event.get();
    processor.AddEvent(std::move(event), 0);
    raw->RequestAbort();

    processor.Update(1);

    CHECK(fires == 0);
    CHECK(processor.IsEmpty());
}

// ===== 4. Ordering, and the one happy path =====

TEST(EventProcessor_RunsInDueOrderAndOnlyWhatIsDue)
{
    EventProcessor processor;
    int early = 0;
    int late = 0;

    processor.AddEvent(std::unique_ptr<BasicEvent>(new CountingEvent(early, 1)), 10);
    processor.AddEvent(std::unique_ptr<BasicEvent>(new CountingEvent(late, 1)), 100);

    processor.Update(50);
    CHECK(early == 1);
    CHECK(late == 0);
    CHECK(!processor.IsEmpty());

    processor.Update(100);
    CHECK(late == 1);
    CHECK(processor.IsEmpty());
}

TEST(EventProcessor_CalculateTimeIsRelativeToTheProcessorClock)
{
    EventProcessor processor;

    CHECK(processor.CalculateTime(10) == 10);
    processor.Update(40);
    CHECK(processor.CalculateTime(10) == 50);
}
