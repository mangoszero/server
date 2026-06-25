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
 * @file CustodyDeferred.h
 * @brief Ordered deferred-effects queue for custody co-commit operations.
 *
 * A custody co-commit (e.g. MailDraft::SendMailToInTransaction) appends its DB
 * writes to the CALLER'S open CharacterDatabase transaction and must NOT mutate
 * live world state (the online receiver's in-memory mail list) or destroy live
 * Item* objects until that transaction has DURABLY committed. Those side effects
 * are queued here as ordered closures and live Item* pointers, then either
 * replayed (run()) after a successful checked commit or discarded
 * (discardItems()) on rollback.
 */

#ifndef MANGOS_CUSTODY_DEFERRED_H
#define MANGOS_CUSTODY_DEFERRED_H

#include <functional>
#include <vector>
#include <string>
#include <utility>

/**
 * Value-captured snapshot of everything needed to replay the online receiver's
 * in-memory mail push, built by VALUE so it never aliases a MailDraft (whose
 * copy/assign are trapped, see Mail.h) and stays valid across the deferral.
 */
struct DeferredMailPush
{
    uint32 mailId;                                          ///< generated mail id
    uint32 receiverGuid;                                    ///< receiver low guid
    uint32 senderId;                                        ///< sender low guid / entry
    uint8 messageType;                                      ///< MailMessageType
    uint8 stationery;                                       ///< MailStationery
    uint32 mailTemplateId;                                  ///< mail template id
    std::string subject;                                    ///< mail subject (raw)
    std::string body;                                       ///< mail body (raw)
    uint32 money;                                           ///< attached money
    uint32 cod;                                             ///< cash on delivery
    uint32 checked;                                         ///< MailCheckMask
    uint64 deliverTime;                                     ///< delivery time
    uint64 expireTime;                                      ///< expiry time
    /// (item guidLow, item entry) pairs to register on the in-memory Mail.
    std::vector<std::pair<uint32, uint32>> items;
    /// Live Item* objects owned by the draft, registered via Player::AddMItem
    /// when the push runs (same call stack, so still valid).
    std::vector<class Item*> liveItems;
};

/**
 * Ordered queue of deferred side effects for a custody co-commit. The owner
 * runs the queue exactly once, AFTER the caller's checked commit reports
 * success; on rollback it discards any live Item* objects that will never be
 * mailed instead.
 */
struct CustodyDeferred
{
    /// One ordered effect queue (executed in push order on success).
    std::vector<std::function<void()>> effects;
    /// Live Item* objects to destroy if the transaction rolls back.
    std::vector<class Item*> itemsToDestroyOnRollback;

    /// Executes every queued effect in order. Call once, after a successful
    /// checked commit, on the same thread/call stack that built the queue.
    void run()
    {
        for (std::vector<std::function<void()>>::iterator it = effects.begin();
             it != effects.end(); ++it)
        {
            (*it)();
        }
    }

    /// Destroys the live Item* objects that were to be mailed but are now
    /// orphaned because the transaction rolled back.
    void discardItems();
};

#endif
/*! @} */
