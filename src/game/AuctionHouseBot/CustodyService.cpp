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

#include "CustodyService.h"

#include "Config/Config.h"
#include "CustodyLedger.h"
#include "Mail.h"
#include "Player.h"

#include <ctime>
#include <functional>
#include <string>

void CustodyService::ReserveGold(CustodyDeferred& d, uint32 ownerGuid,
                                 Player* ownerOnline, uint32 amount,
                                 std::string const& key, uint32 auctionId,
                                 uint8 role)
{
    if (ownerOnline)
    {
        ownerOnline->ModifyMoney(-int32(amount));
        ownerOnline->SaveInventoryAndGoldToDB();
    }

    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_GOLD;
    r.role            = role;
    r.state           = CST_RESERVED;
    r.ownerGuid       = ownerGuid;
    r.beneficiaryGuid = 0;
    r.amount          = amount;
    r.itemGuid        = 0;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = 0;
    CustodyLedger::Insert(r);

    // Suppress unused-parameter warning: d is the ordered deferred context;
    // ReserveGold does not itself push a mail effect, but callers may chain
    // DeferEffect() calls on the same d immediately after.
    (void)d;
}

void CustodyService::ReserveGoldAlreadyDebited(uint32 ownerGuid, uint32 amount,
                                               std::string const& key,
                                               uint32 auctionId, uint8 role)
{
    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_GOLD;
    r.role            = role;
    r.state           = CST_RESERVED;
    r.ownerGuid       = ownerGuid;
    r.beneficiaryGuid = 0;
    r.amount          = amount;
    r.itemGuid        = 0;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = 0;
    CustodyLedger::Insert(r);
}

void CustodyService::CommitGoldLedgerOnly(std::string const& key)
{
    CustodyLedger::SetState(key, CST_TERMINAL_OK,
                            static_cast<uint64>(time(NULL)));
}

void CustodyService::RollbackGoldLedgerOnly(std::string const& key)
{
    CustodyLedger::SetState(key, CST_TERMINAL_BACK,
                            static_cast<uint64>(time(NULL)));
}

void CustodyService::RollbackGoldRefund(CustodyDeferred& d,
                                        std::string const& key,
                                        MailDraft& refundMail,
                                        MailReceiver const& to,
                                        MailSender const& from)
{
    CustodyLedger::SetState(key, CST_TERMINAL_BACK,
                            static_cast<uint64>(time(NULL)));
    refundMail.SendMailToInTransaction(to, from, d);
}

void CustodyService::TopUpBid(std::string const& key, uint32 newAmount,
                              uint32 delta, Player* bidderOnline)
{
    if (bidderOnline)
    {
        bidderOnline->ModifyMoney(-int32(delta));
        bidderOnline->SaveInventoryAndGoldToDB();
    }
    CustodyLedger::SetAmount(key, newAmount);
}

void CustodyService::EscrowItem(uint32 ownerGuid, uint32 itemGuid,
                                std::string const& key, uint32 auctionId)
{
    CustodyRow r;
    r.id              = 0;
    r.idemKey         = key;
    r.kind            = CUSTODY_ITEM;
    r.role            = ROLE_ITEM;
    r.state           = CST_RESERVED;
    r.ownerGuid       = ownerGuid;
    r.beneficiaryGuid = 0;
    r.amount          = 0;
    r.itemGuid        = itemGuid;
    r.auctionId       = auctionId;
    r.createdTime     = static_cast<uint64>(time(NULL));
    r.resolvedTime    = 0;
    CustodyLedger::Insert(r);
}

void CustodyService::DeliverItem(CustodyDeferred& d, std::string const& key,
                                 MailDraft& itemMail, MailReceiver const& to,
                                 MailSender const& from)
{
    CustodyLedger::SetState(key, CST_TERMINAL_OK,
                            static_cast<uint64>(time(NULL)));
    itemMail.SendMailToInTransaction(to, from, d);
}

void CustodyService::DeferEffect(CustodyDeferred& d,
                                 std::function<void()> effect)
{
    d.effects.push_back(effect);
}

std::string CustodyService::CrashPhase()
{
    return sConfig.GetStringDefault("AH.Service.CustodyCrashAt", "");
}
