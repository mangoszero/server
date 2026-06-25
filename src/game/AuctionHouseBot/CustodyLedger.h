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
 * @file CustodyLedger.h
 * @brief Persistent custody_ledger model: CRUD for gold/item custody rows.
 *
 * The custody_ledger table records every gold or item custody event for the
 * out-of-process AH service.  All write functions (Insert, SetState, SetAmount,
 * DeleteTerminalOlderThan) append PExecute statements to an OPEN
 * CharacterDatabase transaction managed by the caller, mirroring the
 * AuctionEntry::SaveToDB idiom.  The single exception is
 * DeleteTerminalOlderThan, which also runs safely outside a transaction (the
 * TTL sweep issues it as its own standalone statement).
 *
 * This is default-off, unused-until-gated groundwork.
 */

#ifndef MANGOS_CUSTODY_LEDGER_H
#define MANGOS_CUSTODY_LEDGER_H

#include "Common.h"

#include <string>
#include <vector>

/// Discriminator: gold movement vs. item movement.
enum CustodyKind : uint8
{
    CUSTODY_GOLD = 0, ///< A gold-amount custody row.
    CUSTODY_ITEM = 1, ///< An item-guid custody row.
};

/// Role of this custody row within its auction lifecycle.
enum CustodyRole : uint8
{
    ROLE_DEPOSIT  = 0, ///< Listing deposit held from seller.
    ROLE_BID      = 1, ///< Bidder's held bid amount.
    ROLE_PROCEEDS = 2, ///< Sale proceeds to be paid to seller.
    ROLE_ITEM     = 3, ///< Item custody (held for delivery).
};

/// Lifecycle state of a custody row.
enum CustodyState : uint8
{
    CST_RESERVED      = 0, ///< Funds/item held; not yet released.
    CST_TERMINAL_OK   = 1, ///< Released successfully (auction ended ok).
    CST_TERMINAL_BACK = 2, ///< Returned to original holder (cancelled/expired).
};

/**
 * @brief One row from the custody_ledger table.
 *
 * Mirrors the 12 columns in the table definition:
 *   id (AUTO_INCREMENT PK), idem_key (uk), kind, role, state,
 *   owner_guid, beneficiary_guid, amount, item_guid,
 *   auction_id, created_time, resolved_time.
 */
struct CustodyRow
{
    uint32      id;               ///< AUTO_INCREMENT PK (0 when not yet persisted).
    std::string idemKey;          ///< Idempotency key (unique; e.g. "bid:999:12").
    uint8       kind;             ///< CustodyKind value.
    uint8       role;             ///< CustodyRole value.
    uint8       state;            ///< CustodyState value.
    uint32      ownerGuid;        ///< Low GUID of the custody holder (seller/bidder).
    uint32      beneficiaryGuid;  ///< Low GUID of eventual recipient (0 = none yet).
    uint32      amount;           ///< Gold amount in copper, or 0 for CUSTODY_ITEM.
    uint32      itemGuid;         ///< Item low GUID for CUSTODY_ITEM, else 0.
    uint32      auctionId;        ///< Associated auction entry id.
    uint64      createdTime;      ///< Unix timestamp at row creation.
    uint64      resolvedTime;     ///< Unix timestamp at resolution (0 = unresolved).
};

/**
 * @brief Persistent CRUD namespace for the custody_ledger table.
 *
 * All write functions queue PExecute calls onto the caller's open
 * CharacterDatabase transaction.  Read functions issue synchronous queries.
 */
namespace CustodyLedger
{
    /**
     * @brief Append an INSERT for @p row to the caller's open transaction.
     *
     * The @c id field is ignored (AUTO_INCREMENT); all other fields are
     * written.  The @c idemKey is escape-sanitised before use.
     *
     * @param row Fully populated CustodyRow (id field unused).
     */
    void Insert(CustodyRow const& row);

    /**
     * @brief Append an UPDATE setting state and resolved_time for @p idemKey.
     *
     * @param idemKey      Idempotency key identifying the row.
     * @param newState     New CustodyState value.
     * @param resolvedTime Unix timestamp to record as resolved_time.
     */
    void SetState(std::string const& idemKey, uint8 newState, uint64 resolvedTime);

    /**
     * @brief Append an UPDATE setting amount for @p idemKey (same-bidder raise).
     *
     * @param idemKey   Idempotency key identifying the row.
     * @param newAmount New gold amount in copper.
     */
    void SetAmount(std::string const& idemKey, uint32 newAmount);

    /**
     * @brief Return true if at least one row exists for @p auctionId.
     *
     * Issues a synchronous SELECT; safe to call outside a transaction.
     *
     * @param auctionId Auction entry id to probe.
     * @return true if any row with that auction_id exists.
     */
    bool HasRows(uint32 auctionId);

    /**
     * @brief Load all non-terminal rows (state == CST_RESERVED) into @p out.
     *
     * Issues a synchronous SELECT; safe to call outside a transaction.  Used
     * at boot/sweep to reconstruct in-flight custody state.
     *
     * @param out Destination vector; rows are appended (not replaced).
     */
    void LoadNonTerminal(std::vector<CustodyRow>& out);

    /**
     * @brief Fetch a single row by @p idemKey.
     *
     * Issues a synchronous SELECT; safe to call outside a transaction.
     *
     * @param idemKey Idempotency key identifying the row.
     * @param out     Populated on success.
     * @return true if the row was found.
     */
    bool Get(std::string const& idemKey, CustodyRow& out);

    /**
     * @brief Delete all terminal rows older than @p cutoffTime.
     *
     * Issues a synchronous PExecute; intended for use both inside an open
     * transaction (append) and standalone (the TTL sweep).
     *
     * @param cutoffTime Rows with resolved_time < cutoffTime and a terminal
     *                   state are deleted.
     */
    void DeleteTerminalOlderThan(uint64 cutoffTime);
}

#endif // MANGOS_CUSTODY_LEDGER_H
