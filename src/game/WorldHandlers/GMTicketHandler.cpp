/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "Common.h"
#include "Language.h"
#include "WorldPacket.h"
#include "Log.h"
#include "GMTicketMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"

void WorldSession::SendGMTicketGetTicket(uint32 status, GMTicket* ticket /*= NULL*/)
{
    std::string text = ticket ? ticket->GetText() : "";

    if (ticket && ticket->HasResponse())
    {
        text += "\n\n";

        std::string textFormat = GetMangosString(LANG_COMMAND_TICKETRESPONSE);
        char textBuf[1024];
        snprintf(textBuf, 1024, textFormat.c_str(), ticket->GetResponse());

        text += textBuf;
    }

    int len = text.size() + 1;
    WorldPacket data(SMSG_GMTICKET_GETTICKET, (4 + len + 1 + 4 + 2 + 4 + 4));
    data << uint32(status);                                 // standard 0x0A, 0x06 if text present
    if (status == 6)
    {
        data << text;                                       // ticket text
        data << uint8(0x7);                                 // ticket category
        data << float(0);                                   // tickets in queue?
        data << float(0);                                   // if > "tickets in queue" then "We are currently experiencing a high volume of petitions."
        data << float(0);                                   // 0 - "Your ticket will be serviced soon", 1 - "Wait time currently unavailable"
        data << uint8(0);                                   // if == 2 and next field == 1 then "Your ticket has been escalated"
        data << uint8(0);                                   // const
    }
    SendPacket(&data);
}

void WorldSession::HandleGMTicketGetTicketOpcode(WorldPacket& /*recv_data*/)
{
    SendQueryTimeResponse();

    GMTicket* ticket = sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid());
    if (ticket)
        { SendGMTicketGetTicket(0x06, ticket); }
    else
        { SendGMTicketGetTicket(0x0A); }
}

void WorldSession::HandleGMTicketUpdateTextOpcode(WorldPacket& recv_data)
{
    std::string ticketText;
    recv_data >> ticketText;

    GMTicketResponse responce = GMTICKET_RESPONSE_UPDATE_SUCCESS;
    if (GMTicket* ticket = sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid()))
    {
        ticket->SetText(ticketText.c_str());
    }
    else
    {
        sLog.outError("Ticket update: Player %s (GUID: %u) doesn't have active ticket", GetPlayer()->GetName(), GetPlayer()->GetGUIDLow());
        responce = GMTICKET_RESPONSE_UPDATE_ERROR;
    }

    WorldPacket data(SMSG_GMTICKET_UPDATETEXT, 4);
    data << uint32(responce);
    SendPacket(&data);
}

//A statusCode of 3 would mean that the client should show the survey now
void WorldSession::SendGMTicketStatusUpdate(GMTicketStatus statusCode)
{
    WorldPacket data(SMSG_GM_TICKET_STATUS_UPDATE, 4);
    data << uint32(statusCode);
    SendPacket(&data);
}

void WorldSession::HandleGMTicketDeleteTicketOpcode(WorldPacket& /*recv_data*/)
{
    //Some housekeeping, this could be cleaner
    GMTicket *ticket = sTicketMgr.GetGMTicket(_player->GetObjectGuid());
    if (ticket)
        ticket->CloseByClient();
    sTicketMgr.Delete(GetPlayer()->GetObjectGuid());

    WorldPacket data(SMSG_GMTICKET_DELETETICKET, 4);
    data << uint32(GMTICKET_RESPONSE_TICKET_DELETED);
    SendPacket(&data);

    SendGMTicketGetTicket(0x0A);
}

void WorldSession::HandleGMTicketCreateOpcode(WorldPacket& recv_data)
{
    uint32 map;
    float x, y, z;
    std::string ticketText = "";

    recv_data >> map >> x >> y >> z;                        // last check 2.4.3
    recv_data >> ticketText;

    recv_data.read_skip<uint32>();                          // unk1, 0
    recv_data.read_skip<uint32>();                          // unk2, 1
    recv_data.read_skip<uint32>();                          // unk3, 0

    DEBUG_LOG("TicketCreate: map %u, x %f, y %f, z %f, text %s", map, x, y, z, ticketText.c_str());

    if (sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid()))
    {
        WorldPacket data(SMSG_GMTICKET_CREATE, 4);
        data << uint32(GMTICKET_RESPONSE_ALREADY_EXIST);    // 1 - You already have GM ticket
        SendPacket(&data);
        return;
    }

    sTicketMgr.Create(_player->GetObjectGuid(), ticketText.c_str());

    SendQueryTimeResponse();

    WorldPacket data(SMSG_GMTICKET_CREATE, 4);
    data << uint32(GMTICKET_RESPONSE_CREATE_SUCCESS);       // 2 - nothing appears (3-error creating, 5-error updating)
    SendPacket(&data);

    sObjectAccessor.DoForAllPlayers([this](Player* player)
        {
        if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            { ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETNEW, GetPlayer()->GetName()); }
        });
}

void WorldSession::HandleGMTicketSystemStatusOpcode(WorldPacket& /*recv_data*/)
{
    WorldPacket data(SMSG_GMTICKET_SYSTEMSTATUS, 4);
    //Handled by using .ticket system_on/off
    data << uint32(sTicketMgr.WillAcceptTickets() ? 1 : 0);
    SendPacket(&data);
}

void WorldSession::HandleGMTicketSurveySubmitOpcode(WorldPacket& recv_data)
{
    // This will be sent after SMSG_GM_TICKET_STATUS_UPDATE with the status = 3
    GMTicket* ticket = sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid());
    if (!ticket)
        //Should we send GM_TICKET_STATUS_CLOSE here aswell?
        return;
    
    ticket->SaveSurveyData(recv_data);
    //Here something needs to be done to inform the client that the ticket is closed
}
