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
 * @file CustodyService.h
 * @brief High-level custody primitives for the out-of-process AH service.
 *
 * Each function appends DB writes to the caller's OPEN CharacterDatabase
 * transaction and queues live-world side effects into a CustodyDeferred.
 * None of these functions open or commit a transaction; that is the seam's
 * responsibility.
 *
 * This is default-off, unused-until-gated groundwork.
 */

#ifndef MANGOS_CUSTODY_SERVICE_H
#define MANGOS_CUSTODY_SERVICE_H

#include "Common.h"
#include "CustodyDeferred.h"
#include "CustodyLedger.h"

#include <functional>
#include <string>
#include <vector>

class Player;
class MailDraft;
class MailReceiver;
class MailSender;

/**
 * @brief High-level custody primitives for auction-house co-commit operations.
 *
 * All write functions queue PExecute statements onto the caller's open
 * CharacterDatabase transaction.  Live-world side effects (money debits, mail
 * pushes) are gated on `ownerOnline != NULL` or appended to the caller's
 * CustodyDeferred for post-commit execution.
 */
namespace CustodyService
{
    /**
     * @brief Debit `amount` from the online owner's wallet (if online) and
     *        insert a CST_RESERVED gold row into the custody_ledger.
     *
     * If `ownerOnline` is non-NULL, calls ModifyMoney(-amount) and
     * SaveInventoryAndGoldToDB() before inserting the ledger row.  If NULL
     * (offline owner), only the ledger row is written (spec X1/S2).
     *
     * @param d           Deferred queue for this co-commit.
     * @param ownerGuid   Low GUID of the gold holder.
     * @param ownerOnline Pointer to the online Player, or NULL if offline.
     * @param amount      Gold amount in copper to reserve.
     * @param key         Idempotency key for this custody row.
     * @param auctionId   Associated auction entry id.
     * @param role        CustodyRole value for this row.
     */
    void ReserveGold(CustodyDeferred& d, uint32 ownerGuid, Player* ownerOnline,
                     uint32 amount, std::string const& key, uint32 auctionId,
                     uint8 role);

    /**
     * @brief Insert a CST_RESERVED gold row without debiting the wallet.
     *
     * Used by S1 (listing deposit): the handler at opcode :401 has already
     * called ModifyMoney before the seam opens the transaction.  This records
     * the already-debited amount into the ledger without a second debit
     * (spec X4).
     *
     * @param ownerGuid Low GUID of the gold holder.
     * @param amount    Gold amount in copper already debited.
     * @param key       Idempotency key for this custody row.
     * @param auctionId Associated auction entry id.
     * @param role      CustodyRole value for this row.
     */
    void ReserveGoldAlreadyDebited(uint32 ownerGuid, uint32 amount,
                                   std::string const& key, uint32 auctionId,
                                   uint8 role);

    /**
     * @brief Flip a CST_RESERVED gold row to CST_TERMINAL_OK (ledger only).
     *
     * No mail is sent; the beneficiary payment is handled by the legacy sender
     * for parity (spec R3).  Appends a SetState UPDATE to the open transaction.
     *
     * @param key Idempotency key identifying the row.
     */
    void CommitGoldLedgerOnly(std::string const& key);

    /**
     * @brief Flip a CST_RESERVED gold row to CST_TERMINAL_BACK (ledger only).
     *
     * No mail is sent.  Use RollbackGoldRefund when a refund mail is required
     * (spec R3).  Appends a SetState UPDATE to the open transaction.
     *
     * @param key Idempotency key identifying the row.
     */
    void RollbackGoldLedgerOnly(std::string const& key);

    /**
     * @brief Flip a CST_RESERVED gold row to CST_TERMINAL_BACK and send a
     *        refund mail via the deferred co-commit path.
     *
     * Used for S2 outbid and S5 cancel-refund paths that DO mail.  Appends
     * both a SetState UPDATE and the mail INSERT to the open transaction, and
     * queues the mail push into @p d (spec S2/S5).
     *
     * @param d          Deferred queue for this co-commit.
     * @param key        Idempotency key identifying the row.
     * @param refundMail Pre-populated MailDraft carrying the refund money.
     * @param to         Mail receiver descriptor.
     * @param from       Mail sender descriptor.
     */
    void RollbackGoldRefund(CustodyDeferred& d, std::string const& key,
                            MailDraft& refundMail, MailReceiver const& to,
                            MailSender const& from);

    /**
     * @brief [SP-2] Silently re-credit @p amount to @p ownerGuid's wallet for a
     *        REJECTED/aborted reservation, then flip @p key's row to
     *        CST_TERMINAL_BACK. Online: ModifyMoney(+amount) + save. Offline:
     *        a direct `UPDATE characters SET money = money + amount` appended to
     *        the caller's OPEN transaction. NO mail (legacy never mailed on a
     *        rejection). Idempotency is the caller's (the row must be RESERVED).
     */
    void ReleaseGoldToWallet(CustodyDeferred& d, uint32 ownerGuid,
                             Player* ownerOnline, uint32 amount,
                             std::string const& key);

    /**
     * @brief [SP-2] Append the applied-record for a worker resolution:
     *        a CST_TERMINAL_OK ROLE_RESOLUTION CUSTODY_GOLD row keyed
     *        "resolve:<uuid>". Written INSIDE the finalize txn so applying value
     *        and recording "done" commit atomically (spec 4.3-C1).
     */
    void WriteResolutionApplied(uint32 auctionId, uint64 uuid);

    /**
     * @brief [SP-2] True if the applied-record for @p uuid exists (the
     *        RESOLVE_ACK DUPLICATE test; DUPLICATE == APPLIED).
     */
    bool ResolutionApplied(uint64 uuid);

    /**
     * @brief Raise the held bid amount for an existing CST_RESERVED gold row.
     *
     * If `bidderOnline` is non-NULL, debits `delta` copper from the bidder's
     * wallet via ModifyMoney + SaveInventoryAndGoldToDB, then updates the
     * ledger row's amount to `newAmount` (spec S2 same-bidder raise).
     *
     * @param key          Idempotency key identifying the row.
     * @param newAmount    New total gold amount in copper.
     * @param delta        Incremental gold amount to debit from the bidder.
     * @param bidderOnline Pointer to the online bidder Player, or NULL.
     */
    void TopUpBid(std::string const& key, uint32 newAmount, uint32 delta,
                  Player* bidderOnline);

    /**
     * @brief Insert a CST_RESERVED CUSTODY_ITEM row into the custody_ledger.
     *
     * Records the item as held by the service.  Moving the item out of the
     * seller's inventory (AddAItem / MoveItemFromInventory) is the seam's
     * responsibility before the transaction opens.
     *
     * @param ownerGuid Low GUID of the item's current holder (seller).
     * @param itemGuid  Low GUID of the item being escrowed.
     * @param key       Idempotency key for this custody row.
     * @param auctionId Associated auction entry id.
     */
    void EscrowItem(uint32 ownerGuid, uint32 itemGuid, std::string const& key,
                    uint32 auctionId);

    /**
     * @brief Flip a CST_RESERVED item row to CST_TERMINAL_OK and send the
     *        delivery mail via the deferred co-commit path.
     *
     * Appends a SetState UPDATE and the mail INSERT to the open transaction,
     * and queues the mail push into @p d (spec S4/S6 item delivery).
     *
     * @param d        Deferred queue for this co-commit.
     * @param key      Idempotency key identifying the row.
     * @param itemMail Pre-populated MailDraft carrying the item.
     * @param to       Mail receiver descriptor.
     * @param from     Mail sender descriptor.
     * @param checked  Mail checked mask to persist with the mail row.
     */
    void DeliverItem(CustodyDeferred& d, std::string const& key,
                     MailDraft& itemMail, MailReceiver const& to,
                     MailSender const& from, uint32 checked = 0);

    /**
     * @brief Append a generic live-world effect at the current call position.
     *
     * Effects are executed in push order by CustodyDeferred::run() after the
     * checked commit succeeds.  Use this for notify calls (e.g. SMSG_AUCTION_*)
     * that must fire at the legacy call position within the seam.
     *
     * @param d      Deferred queue for this co-commit.
     * @param effect Closure to execute post-commit.
     */
    void DeferEffect(CustodyDeferred& d, std::function<void()> effect);

    /// TEST ONLY. Returns the AH.Service.CustodyCrashAt config string
    /// (empty = off, "pre-commit", "pre-deferred").  Never set on a live realm.
    std::string CrashPhase();

    /// TEST ONLY. If AH.Service.CustodyCrashAt == @p phase, flush + _exit(3) to
    /// simulate process death at that custody-seam transition. No-op when the
    /// config is empty (the live default), so it is inert on a real realm.
    void MaybeCrash(std::string const& phase);

    /// TEST ONLY. Checked-commit wrapper for a finalize transaction. If
    /// AH.Service.CustodyFailCommitAt == @p phase, rolls the caller's OPEN
    /// CharacterDatabase transaction back and returns false ONCE (one-shot per
    /// process) to simulate a failed finalize checked-commit, exercising the
    /// redrive-without-rollback path (spec 4.1 step 4). Otherwise -- and always
    /// once the one-shot has fired -- it just delegates to
    /// CommitTransactionChecked(). Inert when the config is empty (live default).
    bool CommitCheckedOrForcedFail(std::string const& phase);

    /**
     * @brief Audit custody-ledger drift.
     *
     * Scans non-terminal custody rows and loaded auction maps. Drift means a
     * non-terminal row with no live auction, or a live custody auction missing
     * one of its required non-terminal rows (`item:<id>`, `dep:<id>`, and a
     * live bid row when `bidder != 0`). This is audit-only: @p dryRun is kept
     * for the repair command's interface but this function never mutates state.
     *
     * @param dryRun  Audit-only flag; no mutations happen in either mode.
     * @param orphans Destination vector; drift rows are appended.
     */
    void ReconcileScan(bool dryRun, std::vector<CustodyRow>& orphans);
}

#endif // MANGOS_CUSTODY_SERVICE_H
