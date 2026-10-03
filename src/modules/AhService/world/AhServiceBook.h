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

#ifndef MANGOS_H_AHSERVICE_BOOK
#define MANGOS_H_AHSERVICE_BOOK

#include "AuctionHouseMgr.h"
#include "AuctionHouseModule.h"
#include "CustodyDeferred.h"
#include "Item.h"

#include <string>
#include <vector>

class Item;
class Player;
class WorldPacket;
class WorldSession;

// Packets built from values, for effects that run after the auction is gone.
void AhSendAuctionCommandResultData(WorldSession* session, uint32 aucId, AuctionAction action, AuctionError errorCode, InventoryResult invError, uint32 newOutbid);
void AhSendAuctionBidderNotificationData(WorldSession* session, uint32 houseId, uint32 id, uint32 bidder, uint32 bid, uint32 outbid, uint32 itemTemplate, int32 itemRand, bool won);
void AhSendAuctionOwnerNotificationData(WorldSession* session, uint32 houseId, uint32 id, uint32 bid, uint32 outbid, uint32 bidderGuidLow, uint32 itemTemplate, int32 itemRand, bool sold);
void AhSendAuctionRemovedNotificationData(WorldSession* session, uint32 id, uint32 itemTemplate, int32 itemRand);

// The settlement mails, written into the caller's open transaction.
void AhSendAuctionOutbiddedMailInTransaction(AuctionEntry* auction, CustodyDeferred& def);
void AhSendAuctionCancelledToBidderMailInTransaction(AuctionEntry* auction, CustodyDeferred& def);
void AhSendAuctionExpiredMailInTransaction(AuctionEntry* auction, CustodyDeferred& def);
void AhSendAuctionSuccessfulMailInTransaction(AuctionEntry* auction, CustodyDeferred& def);
void AhSendAuctionWonMailInTransaction(AuctionEntry* auction, CustodyDeferred& def);

// The custody variants of the book's settlements.
void AhAuctionBidWinningCustody(AuctionEntry& a, Player* newbidder, CustodyDeferred& def,
                                bool usesPlayerSellerCustody,
                                bool hasLiveBidCustody,
                                std::string const& knownBidKey = "");
void AhExpireUnsoldCustody(AuctionEntry& a, CustodyDeferred& def);
void AhPrepareCancelCustody(AuctionEntry& a, Player* seller, CustodyDeferred& def,
                            bool usesPlayerSellerCustody,
                            bool hasLiveBidCustody,
                            std::string const& liveBidKey,
                            uint32 auctionCut);
bool AhUpdateBidCustody(AuctionEntry& a, uint32 newbid, Player* newbidder, CustodyDeferred& def,
                        bool usesPlayerSellerCustody,
                        bool hadLiveBidCustody,
                        std::string const& liveBidKey);

AuctionExpiry AhExpireAuction(AuctionEntry& auction);
/// True when custody settled or refused the bid; @p applied says which.
bool AhSettleGeneratedBid(AuctionEntry& auction, uint32 newbid, bool& stillActive, bool& applied);
/// A bot bid through custody when the auction has it, through the core otherwise.
void AhPlaceGeneratedBid(AuctionEntry& auction, uint32 newbid, bool& applied);
/// AuctionHouseObject::AddAuction, its writes appended to the caller's open transaction.
AuctionEntry* AhAddAuctionInTransaction(AuctionHouseObject& house, AuctionHouseEntry const* auctionHouseEntry,
                                        Item* newItem, uint32 etime, uint32 bid, uint32 buyout,
                                        uint32 deposit, Player* pl);

void AhBuildListForKind(AuctionHouseObject& house, uint8 kind, WorldPacket& data, Player* player,
    const std::wstring& wname, uint32 listfrom, uint32 levelmin, uint32 levelmax,
    uint32 usable, uint32 invType, uint32 itemClass, uint32 itemSubClass,
    uint32 quality, const std::vector<uint32>& clientOutbidIds,
    uint32& count, uint32& totalcount);
/// Records @p clientIds in client order, as AhBuildListForKind prepends them.
void AhAppendClientOutbidsForTest(const std::vector<uint32>& clientIds,
                                  std::vector<uint32>& outOrder);

void AhAuditCustodyReconcile(char const* phase);

#endif
