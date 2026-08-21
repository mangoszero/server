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

#include "EventProcessor.h"

#include <utility>

namespace Events
{

EventProcessor::~EventProcessor()
{
    KillAllEvents(true);
}

bool EventProcessor::DeliverAbort(BasicEvent& event, std::uint64_t time)
{
    if (event.m_aborted)
    {
        return false;
    }

    event.m_aborted = true;
    event.Abort(time);
    return true;
}

void EventProcessor::Update(std::uint32_t elapsed)
{
    m_time += elapsed;
    ++m_pass;

    while (!m_events.empty())
    {
        auto it = m_events.begin();
        if (it->first > m_time)
        {
            break;
        }

        // Queued during THIS pass, so it waits for the next one. An event that
        // re-adds itself for the current moment would otherwise be picked up
        // again immediately, and Update would never return.
        if (it->second && it->second->m_queuedPass == m_pass)
        {
            break;
        }

        // Ownership moves OUT of the map before the event runs. That ordering is
        // what makes re-entrancy safe: while Execute is running, the map holds
        // no pointer to this event, so anything Execute triggers -- another
        // Update, a KillAllEvents, the owner's destruction -- cannot reach it
        // and cannot destroy it twice.
        std::unique_ptr<BasicEvent> event = std::move(it->second);
        m_events.erase(it);

        if (!event)
        {
            continue;
        }

        if (event->IsAbortRequested())
        {
            DeliverAbort(*event, m_time);
            continue;   // the unique_ptr destroys it
        }

        if (!event->Execute(m_time, elapsed))
        {
            // The event re-added itself, here or elsewhere, and that insertion
            // is now its owner. Releasing is what stops this unique_ptr from
            // destroying an object the queue is holding.
            //
            // An Execute that returns false WITHOUT re-adding leaks the event.
            // That is the contract's one sharp edge and it cannot be detected
            // from here -- the event may legitimately have gone to a processor
            // this one has never heard of.
            static_cast<void>(event.release());
        }
    }
}

void EventProcessor::KillAllEvents(bool force)
{
    // Read by AddEvent and Reschedule, which refuse while it is set: an event
    // queued from an Abort handler would land in a container that is being
    // emptied two lines later.
    //
    // ===== SAVED AND RESTORED, BECAUSE THIS NESTS =====
    //
    // Abort() is virtual and runs game code, and game code can call
    // KillAllEvents again. An inner call that finished by storing `false`
    // outright would leave the OUTER call delivering aborts with the guard
    // switched off, and an AddEvent from any later handler accepted into a
    // processor that is in the middle of destroying its queue.
    //
    // Restoring the previous value instead makes the depth irrelevant: the inner
    // call puts back `true`, and only the outermost one puts back `false`.
    // ==================================================
    const bool wasAborting = m_aborting;
    m_aborting = true;

    if (force)
    {
        // Tell any non-forced pass further up the stack that its survivors are
        // not to be re-queued. Its container is out of reach from here.
        m_forceRequested = true;

        // Move the whole queue out FIRST, then destroy.
        //
        // Destroying them in place would leave the map full of dangling
        // pointers between the first destruction and the clear -- and Abort() is
        // virtual and runs game code, so anything it touches that reaches back
        // into this processor would walk them.
        std::multimap<std::uint64_t, std::unique_ptr<BasicEvent>> doomed;
        doomed.swap(m_events);

        for (auto& entry : doomed)
        {
            if (entry.second)
            {
                entry.second->RequestAbort();
                DeliverAbort(*entry.second, m_time);
            }
        }

        // doomed goes out of scope here and destroys every event.
        //
        // The request is left standing for a non-forced pass higher up to read,
        // and cleared only by the outermost call -- otherwise it would survive
        // into the next unrelated kill and drop survivors nobody asked to drop.
        if (!wasAborting)
        {
            m_forceRequested = false;
        }

        m_aborting = wasAborting;
        return;
    }

    // ===== NOT ITERATED WHILE GAME CODE RUNS =====
    //
    // Non-forced: abort everything, but keep what is not yet deletable. Abort is
    // virtual, it runs game code, and that code can call KillAllEvents(true) --
    // which swaps the whole map out from under any iterator this function is
    // holding. AddEvent is refused during an abort, so insertion is not the
    // hazard; the swap is, and no flag prevents it.
    //
    // So the queue is moved out first and put back afterwards, the same shape the
    // forced branch already uses. While the handlers run, m_events is a container
    // they are welcome to do anything to.
    // =============================================
    std::multimap<std::uint64_t, std::unique_ptr<BasicEvent>> pending;
    pending.swap(m_events);

    std::multimap<std::uint64_t, std::unique_ptr<BasicEvent>> survivors;

    // Saved and restored for the same reason m_aborting is: this nests.
    const bool hadForceRequest = m_forceRequested;
    m_forceRequested = false;

    for (auto& entry : pending)
    {
        if (!entry.second)
        {
            continue;
        }

        entry.second->RequestAbort();
        DeliverAbort(*entry.second, m_time);

        if (!entry.second->IsDeletable())
        {
            // Stays queued. A later Update will find the abort request and
            // destroy it -- WITHOUT calling Abort again, because m_aborted is
            // now set.
            survivors.emplace(entry.first, std::move(entry.second));
        }

        // Anything else is destroyed with `pending`, at the end of this scope.
    }

    // A handler asked for a forced kill while this pass was running, and it
    // found nothing to destroy because the queue was already in `pending`.
    // Honouring it here is what stops "force" from being downgraded: the
    // survivors are dropped rather than re-queued, and `survivors` destroys
    // them on the way out. They were aborted once already, and DeliverAbort
    // will not fire a second time.
    if (!m_forceRequested)
    {
        // Whatever a handler queued in the meantime keeps its place; the
        // survivors are merged back in rather than overwriting it.
        for (auto& entry : survivors)
        {
            m_events.emplace(entry.first, std::move(entry.second));
        }
    }

    m_forceRequested = hadForceRequest;
    m_aborting = wasAborting;
}

bool EventProcessor::AddEvent(std::unique_ptr<BasicEvent> event,
                              std::uint64_t executionTime,
                              bool setAddTime)
{
    if (!event)
    {
        return false;
    }

    if (m_aborting)
    {
        // Refused, and cleaned up rather than dropped. The event is aborted so
        // that whatever it was holding is released on the way out.
        event->RequestAbort();
        DeliverAbort(*event, m_time);
        return false;
    }

    if (setAddTime)
    {
        event->m_addTime = m_time;
    }
    event->m_execTime = executionTime;
    event->m_queuedPass = m_pass;

    m_events.emplace(executionTime, std::move(event));
    return true;
}

bool EventProcessor::Reschedule(BasicEvent* event,
                                std::uint64_t executionTime,
                                bool setAddTime)
{
    if (!event)
    {
        return false;
    }

    if (m_aborting)
    {
        return false;
    }

    // Already queued: adopting it a second time would put two unique_ptrs on one
    // object, and the second destruction is a double free. Linear, but the scan
    // only runs on the re-add path and these queues are short.
    for (const auto& entry : m_events)
    {
        if (entry.second.get() == event)
        {
            return false;
        }
    }

    if (setAddTime)
    {
        event->m_addTime = m_time;
    }
    event->m_execTime = executionTime;
    event->m_queuedPass = m_pass;

    m_events.emplace(executionTime, std::unique_ptr<BasicEvent>(event));
    return true;
}

} // namespace Events
