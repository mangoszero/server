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
 * @file GMTicketHandler.cpp
 * @brief GM ticket system handlers
 *
 * This file implements player-side GM ticket management:
 * - Get current ticket status
 * - Create new tickets
 * - Update existing ticket text
 * - Delete/close tickets
 * - System status toggle
 *
 * Tickets are stored in GMTicketMgr and notify online GMs of changes.
 *
 * @see GMTicketMgr for ticket storage and management
 * @see GMTicket for ticket data structure
 */

#include "Common.h"
#include "Language.h"
#include "WorldPacket.h"
#include "Log.h"
#include "GMTicketMgr.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "Chat.h"

/**
 * @brief Send ticket status to client
 * @param status Response status code:
 *               0x0A = No ticket
 *               0x06 = Ticket exists (includes ticket data)
 * @param ticket Active ticket pointer (required if status == 6)
 *
 * Constructs and sends SMSG_GMTICKET_GETTICKET with ticket information.
 * When status is 0x06, includes ticket text and queue position info.
 */
void WorldSession::SendGMTicketGetTicket(uint32 status, GMTicket* ticket /*= NULL*/)
{
    std::string text = ticket ? ticket->GetText() : "";

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

/**
 * @brief Handle ticket status request (CMSG_GMTICKET_GETTICKET)
 * @param recv_data World packet (empty)
 *
 * Player requests current ticket status. Responds with either:
 * - Status 0x06 + ticket data if player has active ticket
 * - Status 0x0A if no ticket exists
 *
 * Also sends server time via SendQueryTimeResponse().
 */
void WorldSession::HandleGMTicketGetTicketOpcode(WorldPacket& /*recv_data*/)
{
    SendQueryTimeResponse();

    GMTicket* ticket = sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid());
    if (ticket)
    {
        SendGMTicketGetTicket(0x06, ticket);
    }
    else
    {
        SendGMTicketGetTicket(0x0A);
    }
}

/**
 * @brief Handle ticket text update (CMSG_GMTICKET_UPDATETEXT)
 * @param recv_data World packet containing new ticket text
 *
 * Updates the text of an existing ticket. Performs text cleanup:
 * - Removes invisible characters (e.g., '\a' added by client)
 * - Trims leading whitespace
 *
 * Notifies all online GMs of the update.
 */
void WorldSession::HandleGMTicketUpdateTextOpcode(WorldPacket& recv_data)
{
    std::string ticketText;
    recv_data >> ticketText;

    // When updating the ticket, the client adds a leading '\a' char - remove it
    stripLineInvisibleChars(ticketText);

    // Trim leading spaces that may result from invisible char removal
    ltrim(ticketText);

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

    GMTicket * ticket = sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid());

    // Notify all GM that the ticket has been changed
    sObjectAccessor.DoForAllPlayers([ticket, this](Player* player)
        {
            if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            {
                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETUPDATED, GetPlayer()->GetName(), ticket->GetId());

            }
        }
    );
}

/**
 * @brief Send ticket status update to client
 * @param statusCode Status code to send
 *
 * Sends SMSG_GM_TICKET_STATUS_UPDATE to notify the player of ticket status changes.
 * Status codes:
 * - 0 = Ticket updated
 * - 1 = Ticket closed
 * - 2 = Ticket being handled by GM
 * - 3 = Survey available (show survey dialog)
 */
void WorldSession::SendGMTicketStatusUpdate(GMTicketStatus statusCode)
{
    WorldPacket data(SMSG_GM_TICKET_STATUS_UPDATE, 4);
    data << uint32(statusCode);
    SendPacket(&data);
}

/**
 * @brief Handle ticket deletion by player (CMSG_GMTICKET_DELETETICKET)
 * @param recv_data World packet (empty)
 *
 * Player requests to close/delete their ticket. If ticket exists:
 * 1. Marks it as closed by client
 * 2. Removes from ticket manager
 * 3. Sends confirmation to client
 * 4. Updates status to show no ticket
 */
void WorldSession::HandleGMTicketDeleteTicketOpcode(WorldPacket& /*recv_data*/)
{
    // Mark ticket as closed if it exists
    GMTicket *ticket = sTicketMgr.GetGMTicket(_player->GetObjectGuid());
    if (ticket)
    {
        ticket->CloseByClient();
    }
    sTicketMgr.Delete(GetPlayer()->GetObjectGuid());

    WorldPacket data(SMSG_GMTICKET_DELETETICKET, 4);
    data << uint32(GMTICKET_RESPONSE_TICKET_DELETED);
    SendPacket(&data);

    SendGMTicketGetTicket(0x0A);
}

/**
 * @brief Handle new ticket creation (CMSG_GMTICKET_CREATE)
 * @param recv_data World packet containing ticket details
 *
 * Creates a new GM ticket with:
 * - Category (harassment, bug report, etc.)
 * - Player location (map, coordinates)
 * - Ticket message text
 * - Optional chat log data (for harassment reports)
 *
 * Fails if player already has an open ticket.
 * Notifies all online GMs of the new ticket.
 */
void WorldSession::HandleGMTicketCreateOpcode(WorldPacket& recv_data)
{
    uint32 mapId;
    uint8 category;
    float x, y, z;
    std::string ticketText = "";
    recv_data >> category;
    recv_data >> mapId >> x >> y >> z;                      // Player position at ticket creation
    recv_data >> ticketText;

    std::string reserved;
    recv_data >> reserved;                                  // Legacy: "Reserved for future use"

    // Category 2 = Behavior/Harassment - includes chat log data
    if (category == 2)
    {
        uint32 chatDataLineCount;
        recv_data >> chatDataLineCount;

        uint32 chatDataSizeInflated;
        recv_data >> chatDataSizeInflated;

        // Skip compressed chat log data (not stored in this implementation)
        if (size_t chatDataSizeDeflated = (recv_data.size() - recv_data.rpos()))
        {
            recv_data.read_skip(chatDataSizeDeflated);
        }
    }


    DEBUG_LOG("TicketCreate: map %u, x %f, y %f, z %f, text %s", mapId, x, y, z, ticketText.c_str());

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

    GMTicket * ticket = sTicketMgr.GetGMTicket(_player->GetObjectGuid());

    sObjectAccessor.DoForAllPlayers([ticket, this](Player* player)
        {
            if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            {
                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETNEW, GetPlayer()->GetName(), ticket->GetId());
            }
        }
    );
}

/**
 * @brief Handle ticket system status request (CMSG_GMTICKET_SYSTEMSTATUS)
 * @param recv_data World packet (empty)
 *
 * Player queries whether the GM ticket system is currently enabled.
 * Controlled by GM command .ticket system_on/off via sTicketMgr.
 *
 * Response: 1 = System enabled, 0 = System disabled
 */
void WorldSession::HandleGMTicketSystemStatusOpcode(WorldPacket& /*recv_data*/)
{
    WorldPacket data(SMSG_GMTICKET_SYSTEMSTATUS, 4);
    // Controlled by GM command .ticket system_on/off
    data << uint32(sTicketMgr.WillAcceptTickets() ? 1 : 0);
    SendPacket(&data);
}

/**
 * @brief Handle ticket survey submission (CMSG_GMTICKET_SURVEY)
 * @param recv_data World packet containing survey responses
 *
 * Player submits satisfaction survey after ticket resolution.
 * Sent in response to SMSG_GM_TICKET_STATUS_UPDATE with status = 3.
 *
 * Survey data is saved to the ticket for GM/admin review.
 */
void WorldSession::HandleGMTicketSurveySubmitOpcode(WorldPacket& recv_data)
{
    // Sent after SMSG_GM_TICKET_STATUS_UPDATE with status = 3 (survey available)
    GMTicket* ticket = sTicketMgr.GetGMTicket(GetPlayer()->GetObjectGuid());
    if (!ticket)
    {
        return;
    }

    ticket->SaveSurveyData(recv_data);
}
