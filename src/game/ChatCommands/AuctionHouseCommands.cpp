/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "Chat.h"
#include "Language.h"

 /**********************************************************************
     CommandTable : auctionCommandTable
 /***********************************************************************/

bool ChatHandler::HandleAuctionCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(0);
    m_session->SendAuctionHello(m_session->GetPlayer());

    return true;
}

bool ChatHandler::HandleAuctionItemCommand(char* args)
{
    // format: (alliance|horde|goblin) item[:count] price [buyout] [short|long|verylong]
    char* typeStr = ExtractLiteralArg(&args);
    if (!typeStr)
    {
        return false;
    }

    uint32 houseid;
    if (strncmp(typeStr, "alliance", strlen(typeStr)) == 0)
    {
        houseid = 1;
    }
    else if (strncmp(typeStr, "horde", strlen(typeStr)) == 0)
    {
        houseid = 6;
    }
    else if (strncmp(typeStr, "goblin", strlen(typeStr)) == 0)
    {
        houseid = 7;
    }
    else
    {
        return false;
    }

    // parse item str
    char* itemStr = ExtractArg(&args);
    if (!itemStr)
    {
        return false;
    }

    uint32 item_id = 0;
    uint32 item_count = 1;
    if (sscanf(itemStr, "%u:%u", &item_id, &item_count) != 2)
        if (sscanf(itemStr, "%u", &item_id) != 1)
        {
            return false;
        }

    uint32 price;
    if (!ExtractUInt32(&args, price))
    {
        return false;
    }

    uint32 buyout;
    if (!ExtractOptUInt32(&args, buyout, 0))
    {
        return false;
    }

    uint32 etime = 4 * MIN_AUCTION_TIME;
    if (char* timeStr = ExtractLiteralArg(&args))
    {
        if (strncmp(timeStr, "short", strlen(timeStr)) == 0)
        {
            etime = 1 * MIN_AUCTION_TIME;
        }
        else if (strncmp(timeStr, "long", strlen(timeStr)) == 0)
        {
            etime = 2 * MIN_AUCTION_TIME;
        }
        else if (strncmp(timeStr, "verylong", strlen(timeStr)) == 0)
        {
            etime = 4 * MIN_AUCTION_TIME;
        }
        else
        {
            return false;
        }
    }

    AuctionHouseEntry const* auctionHouseEntry = sAuctionHouseStore.LookupEntry(houseid);
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(auctionHouseEntry);

    if (!item_id)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* item_proto = ObjectMgr::GetItemPrototype(item_id);
    if (!item_proto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    if (item_count < 1 || (item_proto->MaxCount > 0 && item_count > uint32(item_proto->MaxCount)))
    {
        PSendSysMessage(LANG_COMMAND_INVALID_ITEM_COUNT, item_count, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    do
    {
        uint32 item_stack = item_count > item_proto->GetMaxStackSize() ? item_proto->GetMaxStackSize() : item_count;
        item_count -= item_stack;

        Item* newItem = Item::CreateItem(item_id, item_stack);
        MANGOS_ASSERT(newItem);

        auctionHouse->AddAuction(auctionHouseEntry, newItem, etime, price, buyout);
    }
    while (item_count);

    return true;
}

bool ChatHandler::HandleAuctionHordeCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(m_session->GetPlayer()->GetTeam() != HORDE ? -1 : 0);
    m_session->SendAuctionHello(m_session->GetPlayer());
    return true;
}

bool ChatHandler::HandleAuctionGoblinCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(1);
    m_session->SendAuctionHello(m_session->GetPlayer());
    return true;
}

bool ChatHandler::HandleAuctionAllianceCommand(char* /*args*/)
{
    m_session->GetPlayer()->SetAuctionAccessMode(m_session->GetPlayer()->GetTeam() != ALLIANCE ? -1 : 0);
    m_session->SendAuctionHello(m_session->GetPlayer());
    return true;
}
