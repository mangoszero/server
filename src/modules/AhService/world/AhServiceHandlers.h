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

#ifndef MANGOS_H_AHSERVICE_HANDLERS
#define MANGOS_H_AHSERVICE_HANDLERS

#include "AuctionHouseModule.h"

class AuctionHouseObject;
class WorldSession;
struct AuctionEntry;

/// The center flash and red chat line of an unavailable auction house.
void AhSendUnavailableMessage(WorldSession* session);
/// An empty list of @p kind (a BrowseKind) plus the unavailable message.
void AhSendBrowseUnavailable(WorldSession* session, uint8 kind);

// The AuctionHouseModule request hooks of the service.
bool AhBlocksHello(WorldSession& session);
bool AhForwardSell(WorldSession& session, AuctionSale const& sale);
bool AhSettleSell(WorldSession& session, AuctionSale const& sale, AuctionEntry*& listed);
bool AhForwardBid(WorldSession& session, uint32 auctionId, uint32 price);
bool AhSettleBid(WorldSession& session, AuctionEntry& auction, uint32 price, uint32 newOutbid);
bool AhForwardCancel(WorldSession& session, uint32 auctionId);
bool AhSettleCancel(WorldSession& session, AuctionHouseObject& house, AuctionEntry& auction);
bool AhForwardList(WorldSession& session, AuctionListRequest const& request);

#endif
