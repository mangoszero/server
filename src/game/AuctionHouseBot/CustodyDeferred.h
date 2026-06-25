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
 * live world state (the online receiver's in-memory mail list) or dispose of a
 * live Item* until that transaction has DURABLY committed. Those side effects
 * are queued here as ordered closures, then replayed (run()) AFTER a successful
 * checked commit.
 *
 * Item ownership model: every custody mail item is owned by the AH cache
 * (AuctionHouseMgr::mAitems), NOT by the mail. The item MUST survive a rollback
 * -- the auction stays intact and re-resolves on the next tick, where GetAItem()
 * must still return a valid pointer. An item is therefore disposed ONLY on
 * success, inside an effects closure:
 *   - online receiver        -> Player::AddMItem (ownership to the Player);
 *   - offline-with-account   -> the live Item* is deleted (it now lives only as
 *                               the DB item_instance + mail_items rows);
 *   - invalid receiver       -> the live Item* is deleted (its item_instance
 *                               DELETE is in the same transaction).
 * In every case the seam's RemoveAItem (also a success-only effect, ordered
 * BEFORE the mail's disposal) takes the item out of mAitems first. On ROLLBACK
 * none of these effects run: the item is neither freed nor RemoveAItem'd and
 * survives in mAitems for the next-tick re-resolution.
 */

#ifndef MANGOS_CUSTODY_DEFERRED_H
#define MANGOS_CUSTODY_DEFERRED_H

#include "Common.h"                                         // uint8/uint32/uint64 typedefs

#include <functional>
#include <vector>
#include <string>
#include <utility>

/// The live Item* are only stored (never dereferenced) in this header, so a
/// forward declaration is enough and Item.h need not be pulled in here.
class Item;

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
    /// Live Item* objects (owned by the AH cache, mAitems) registered via
    /// Player::AddMItem when the success push closure runs (same call stack, so
    /// still valid). On rollback the closure never runs and these items survive
    /// in mAitems for the next-tick re-resolution -- they are NOT freed here.
    std::vector<Item*> liveItems;
};

/**
 * Ordered queue of deferred side effects for a custody co-commit. The owner
 * runs the queue exactly once, AFTER the caller's checked commit reports
 * success; on rollback it does NOTHING (no effect runs).
 *
 * Caller contract: on a successful checked commit call run(); on a failed
 * commit call nothing. Every item disposal (online AddMItem, offline/invalid
 * delete) and every RemoveAItem is a success-only effect, so on rollback the
 * custody items are neither freed nor removed from mAitems -- they survive for
 * the next-tick re-resolution (the auction itself is not deleted until run()).
 * This is why there is no item-discard hook: rollback frees nothing.
 */
struct CustodyDeferred
{
    /// Ordered live effects, executed in push order on success: seam RemoveAItem
    /// (out of mAitems) THEN the mail's item disposal (online AddMItem / offline
    /// + invalid delete). On rollback none of these run and every custody item
    /// survives in mAitems.
    std::vector<std::function<void()>> effects;

    /// Executes every queued effect in order. Call once, after a successful
    /// checked commit, on the same thread/call stack that built the queue.
    void run()
    {
        for (size_t i = 0; i < effects.size(); ++i)
        {
            effects[i]();
        }
    }
};

#endif
/*! @} */
