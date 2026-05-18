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

/**
 * @addtogroup mailing
 * @{
 *
 * @file MassMailMgr.cpp
 * @brief Mass mail distribution system for server-wide announcements
 *
 * This file implements MassMailMgr, a throttled mailing system for sending
 * announcements to large numbers of players. It prevents server lag by:
 * - Processing mail delivery over multiple server ticks
 * - Limiting mails sent per tick (configurable)
 * - Using async database queries for recipient lookup
 *
 * Use cases:
 * - Event announcements to all players
 * - Compensations for server issues
 * - Seasonal greetings
 * - Account-related notifications
 *
 * Delivery is batched and spread over time to maintain server performance.
 *
 * @see MassMailMgr for the manager interface
 * @see MailDraft for the mail content template
 */

#include "MassMailMgr.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Database/DatabaseImpl.h"
#include "SharedDefines.h"
#include "World.h"
#include "ObjectMgr.h"

INSTANTIATE_SINGLETON_1(MassMailMgr);

/**
 * @brief Add mass mail task filtered by race mask
 * @param mailProto Mail template (will be cloned for each recipient)
 * @param sender Mail sender information
 * @param raceMask Bitmask of races to include (RACEMASK_* constants)
 *
 * Creates a mass mail task targeting specific races. Generates SQL query
 * based on race mask to filter character database. For all playable races,
 * uses a simpler query without race filtering.
 *
 * Example race masks:
 * - RACEMASK_ALL_PLAYABLE: All races
 * - RACEMASK_ALLIANCE: Alliance races only
 * - RACEMASK_HORDE: Horde races only
 */
void MassMailMgr::AddMassMailTask(MailDraft* mailProto, const MailSender &sender, uint32 raceMask)
{
    if (RACEMASK_ALL_PLAYABLE & ~raceMask)                  // have races not included in mask
    {
        std::ostringstream ss;
        ss << "SELECT `guid` FROM `characters` WHERE (1 << (`race` - 1)) & " << raceMask << " AND `deleteDate` IS NULL";
        AddMassMailTask(mailProto, sender, ss.str().c_str());
    }
    else
    {
        AddMassMailTask(mailProto, sender, "SELECT `guid` FROM `characters` WHERE `deleteDate` IS NULL");
    }
}

/**
 * @struct MassMailerQueryHandler
 * @brief Database query callback handler for mass mail recipient lookup
 *
 * Handles async query results containing character GUIDs for mass mailing.
 * Populates the receivers list and adds the task to MassMailMgr processing queue.
 */
struct MassMailerQueryHandler
{
    /**
     * @brief Process query results and add receivers to mass mail task
     * @param result Database query result containing character GUIDs
     * @param mailProto Mail template to send
     * @param sender Mail sender information
     *
     * Extracts all GUIDs from the query result and adds them to the
     * receivers list for the mass mail task.
     */
    void HandleQueryCallback(QueryResult* result, MailDraft* mailProto, MailSender sender)
    {
        if (!result)
        {
            return;
        }

        MassMailMgr::ReceiversList& recievers = sMassMailMgr.AddMassMailTask(mailProto, sender);

        do
        {
            Field* fields = result->Fetch();
            recievers.insert(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }
} massMailerQueryHandler;

/**
 * @brief Add mass mail task with custom SQL query
 * @param mailProto Mail template (will be cloned for each recipient)
 * @param sender Mail sender information
 * @param query SQL query to select recipient GUIDs from characters table
 *
 * Executes async database query to build recipient list. Query must return
 * character GUIDs in the first column.
 *
 * @note Query is executed asynchronously - task is added when results return
 */
void MassMailMgr::AddMassMailTask(MailDraft* mailProto, const MailSender &sender, char const* query)
{
    CharacterDatabase.AsyncPQuery(&massMailerQueryHandler, &MassMailerQueryHandler::HandleQueryCallback, mailProto, sender, "%s", query);
}

/**
 * @brief Process mass mail delivery queue
 * @param sendall If true, process entire queue regardless of per-tick limit
 *
 * Called periodically (server tick) to send queued mass mails.
 * Respects CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK to prevent lag.
 *
 * For each task:
 * - Sends up to maxcount mails (or all if sendall=true)
 * - Clones the mail draft for each recipient (except last)
 * - Removes completed tasks from queue
 * - Sets MAIL_CHECK_MASK_RETURNED to prevent mail returns
 *
 * @note This method throttles delivery to maintain server performance
 */
void MassMailMgr::Update(bool sendall /*= false*/)
{
    if (m_massMails.empty())
    {
        return;
    }

    uint32 maxcount = sWorld.getConfig(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK);

    do
    {
        MassMail& task = m_massMails.front();

        while (!task.m_receivers.empty() && (sendall || maxcount > 0))
        {
            uint32 receiver_lowguid = *task.m_receivers.begin();
            task.m_receivers.erase(task.m_receivers.begin());

            ObjectGuid receiver_guid = ObjectGuid(HIGHGUID_PLAYER, receiver_lowguid);
            Player* receiver = sObjectMgr.GetPlayer(receiver_guid);

            // last case. can be just send
            if (task.m_receivers.empty())
            {
                // prevent mail return
                task.m_protoMail->SendMailTo(MailReceiver(receiver, receiver_guid), task.m_sender, MAIL_CHECK_MASK_RETURNED);

                if (!sendall)
                {
                    --maxcount;
                }
                break;
            }

            // need clone draft
            MailDraft draft;
            draft.CloneFrom(*task.m_protoMail);

            // prevent mail return
            draft.SendMailTo(MailReceiver(receiver, receiver_guid), task.m_sender, MAIL_CHECK_MASK_RETURNED);

            if (!sendall)
            {
                --maxcount;
            }
        }

        if (task.m_receivers.empty())
        {
            m_massMails.pop_front();
        }
    }
    while (!m_massMails.empty() && (sendall || maxcount > 0));
}

/**
 * @brief Get mass mail queue statistics
 * @param tasks Output: Number of queued tasks
 * @param mails Output: Total pending mails across all tasks
 * @param needTime Output: Estimated time to clear queue (seconds)
 *
 * Calculates statistics for mass mail queue status.
 * needTime is estimated based on:
 * - 50ms tick length
 * - Mails per tick (CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK)
 * - Total pending mails
 *
 * Used for GM commands and monitoring.
 */
void MassMailMgr::GetStatistic(uint32& tasks, uint32& mails, uint32& needTime) const
{
    tasks = m_massMails.size();

    uint32 mailsCount = 0;
    for (MassMailList::const_iterator mailItr = m_massMails.begin(); mailItr != m_massMails.end(); ++mailItr)
    {
        mailsCount += mailItr->m_receivers.size();
    }

    mails = mailsCount;

    // 50 msecs is tick length
    needTime = 50 * mailsCount / sWorld.getConfig(CONFIG_UINT32_MASS_MAILER_SEND_PER_TICK) / IN_MILLISECONDS;
}


/*! @} */
