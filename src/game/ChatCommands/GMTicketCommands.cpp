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
#include "World.h"
#include "GMTicketMgr.h"
#include "Mail.h"


// show ticket (helper)
void ChatHandler::ShowTicket(GMTicket const* ticket)
{
    std::string lastupdated = TimeToTimestampStr(ticket->GetLastUpdate());

    std::string name;
    if (!sObjectMgr.GetPlayerNameByGUID(ticket->GetPlayerGuid(), name))
    {
        name = GetMangosString(LANG_UNKNOWN);
    }

    std::string nameLink = playerLink(name);

    char const* response = ticket->GetResponse();

    PSendSysMessage(LANG_COMMAND_TICKETVIEW, nameLink.c_str(), lastupdated.c_str(), ticket->GetText());
    if (strlen(response))
    {
        PSendSysMessage(LANG_COMMAND_TICKETRESPONSE, ticket->GetResponse());
    }
}

// ticket commands
bool ChatHandler::HandleTicketAcceptCommand(char* args)
{
    char* px = ExtractLiteralArg(&args);

    // ticket<end>
    if (!px)
    {
        return false;
    }

    // ticket accept on
    if (strncmp(px, "on", 3) == 0)
    {
        sTicketMgr.SetAcceptTickets(true);
        SendSysMessage(LANG_COMMAND_TICKETS_SYSTEM_ON);
    }
    // ticket accept off
    else if (strncmp(px, "off", 4) == 0)
    {
        sTicketMgr.SetAcceptTickets(false);
        SendSysMessage(LANG_COMMAND_TICKETS_SYSTEM_OFF);
    }
    else
    {
        return false;
    }

    return true;
}

bool ChatHandler::HandleTicketCloseCommand(char* args)
{
    GMTicket* ticket = NULL;

    uint32 num;
    if (ExtractUInt32(&args, num))
    {
        if (num == 0)
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        ObjectGuid target_guid;
        std::string target_name;
        if (!ExtractPlayerTarget(&args, NULL, &target_guid, &target_name))
        {
            return false;
        }

        // ticket respond $char_name
        ticket = sTicketMgr.GetGMTicket(target_guid);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
            SetSentErrorMessage(true);
            return false;
        }
    }

    ObjectGuid target_guid = ticket->GetPlayerGuid();

    // Get Player
    // Can be nullptr if player is offline
    Player* pPlayer = sObjectMgr.GetPlayer(target_guid);

    // Get Player name
    std::string target_name;
    sObjectMgr.GetPlayerNameByGUID(target_guid, target_name);


    if (!pPlayer && !sWorld.getConfig(CONFIG_BOOL_GM_TICKET_OFFLINE_CLOSING))
    {
        SendSysMessage(LANG_COMMAND_TICKET_CANT_CLOSE);
        return false;
    }

    if (*args) {
        ticket->SetResponseText(args);
    }

    // Set reponse text if not existing
    if (!*ticket->GetResponse())
    {
        const uint32 responseBufferSize = 256;
        char response[responseBufferSize];

        if (m_session)
        {
            const char* format = "[System Message] This ticket was closed by <GM> %s without any written response, perhaps it was resolved by direct chat.";
            const char* buffer;
            snprintf(response, responseBufferSize, format, m_session->GetPlayer()->GetName());
        }
        else
        {
            strcpy(response, "[System Message] this ticket was closed using CLI console.");
        }

        ticket->SetResponseText(response);
    }

    ticket->Close();

    // Define ticketId variable because we need ticket id after deleting it from TicketMgr
    uint32 ticketId = ticket->GetId();

    //This logic feels misplaced, but you can't have it in GMTicket?
    // here, ticket become invalidated and should not be used below
    sTicketMgr.Delete(ticket->GetPlayerGuid());

    const char* gmNameReplacementWhenUsingCLI = "ADMIN";


    // Send system Message to All Connected GMs to inform them the ticket has been closed
    sObjectAccessor.DoForAllPlayers([&](Player* player)
        {
            if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            {
                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);
            }
        });

    if (!m_session)
    {
        // In order to have message in CLI otherwise above code will only display to connected gms but not in console
        PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), gmNameReplacementWhenUsingCLI);
    }

    return true;
}

// del tickets
bool ChatHandler::HandleTicketDeleteCommand(char* args)
{
    char* px = ExtractLiteralArg(&args);
    if (!px)
    {
        return false;
    }

    // ticket delete all
    if (strncmp(px, "all", 4) == 0)
    {
        sTicketMgr.DeleteAll();
        SendSysMessage(LANG_COMMAND_ALLTICKETDELETED);
        return true;
    }

    uint32 num;

    // ticket delete #num
    if (ExtractUInt32(&px, num))
    {
        if (num == 0)
        {
            return false;
        }

        // mgr numbering tickets start from 0
        GMTicket* ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }

        ObjectGuid guid = ticket->GetPlayerGuid();

        sTicketMgr.Delete(guid);

        // notify player
        if (Player* pl = sObjectMgr.GetPlayer(guid))
        {
            pl->GetSession()->SendGMTicketGetTicket(0x0A);
            PSendSysMessage(LANG_COMMAND_TICKETPLAYERDEL, GetNameLink(pl).c_str());
        }
        else
        {
            PSendSysMessage(LANG_COMMAND_TICKETDEL);
        }

        return true;
    }

    // ticket delete $charName
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&px, &target, &target_guid, &target_name))
    {
        return false;
    }

    // ticket delete $charName
    sTicketMgr.Delete(target_guid);

    // notify players about ticket deleting
    if (target)
    {
        target->GetSession()->SendGMTicketGetTicket(0x0A);
    }

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_COMMAND_TICKETPLAYERDEL, nameLink.c_str());
    return true;
}

bool ChatHandler::HandleTicketInfoCommand(char* args)
{
    size_t count = sTicketMgr.GetTicketCount();

    if (m_session)
    {
        PSendSysMessage(LANG_COMMAND_TICKETCOUNT, count, GetOnOffStr(m_session->GetPlayer()->isAcceptTickets()));
    }
    else
    {
        PSendSysMessage(LANG_COMMAND_TICKETCOUNT_CONSOLE, count);
    }

    return true;
}

bool ChatHandler::HandleTicketListCommand(char* args)
{
    uint16 numToShow = std::min(uint16(sTicketMgr.GetTicketCount()), uint16(sWorld.getConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE)));
    for (uint16 i = 0; i < numToShow; ++i)
    {
        GMTicket* ticket = sTicketMgr.GetGMTicketByOrderPos(i);
        time_t lastChanged = time_t(ticket->GetLastUpdate());
        PSendSysMessage(LANG_COMMAND_TICKET_OFFLINE_INFO, ticket->GetId(), ticket->GetPlayerGuid().GetCounter(), ticket->HasResponse() ? "+" : "-", ctime(&lastChanged));
    }

    PSendSysMessage(LANG_COMMAND_TICKET_COUNT_ALL, numToShow, sTicketMgr.GetTicketCount());
    return true;
}

bool ChatHandler::HandleTicketOnlineListCommand(char* args)
{
    uint16 count = 0;
    for (uint16 i = 0; i < sTicketMgr.GetTicketCount(); ++i)
    {
        GMTicket* ticket = sTicketMgr.GetGMTicketByOrderPos(i);
        if (Player* player = sObjectMgr.GetPlayer(ticket->GetPlayerGuid(), true))
        {
            ++count;
            if (i < sWorld.getConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE))
            {
                time_t lastChanged = time_t(ticket->GetLastUpdate());
                PSendSysMessage(LANG_COMMAND_TICKET_BRIEF_INFO, ticket->GetId(), player->GetName(), ticket->HasResponse() ? "+" : "-", ctime(&lastChanged));
            }
        }
    }

    PSendSysMessage(LANG_COMMAND_TICKET_COUNT_ONLINE, std::min(count, uint16(sWorld.getConfig(CONFIG_UINT32_GM_TICKET_LIST_SIZE))), count);
    return true;
}

bool ChatHandler::HandleTicketMeAcceptCommand(char* args)
{
    char* px = ExtractLiteralArg(&args);
    if (!px)
    {
        PSendSysMessage(LANG_COMMAND_TICKET_ACCEPT_STATE, m_session->GetPlayer()->isAcceptTickets() ? "on" : "off");
        return true;
    }

    if (!m_session)
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    // ticket on
    if (strncmp(px, "on", 3) == 0)
    {
        m_session->GetPlayer()->SetAcceptTicket(true);
        SendSysMessage(LANG_COMMAND_TICKETON);
    }
    // ticket off
    else if (strncmp(px, "off", 4) == 0)
    {
        m_session->GetPlayer()->SetAcceptTicket(false);
        SendSysMessage(LANG_COMMAND_TICKETOFF);
    }
    else
    {
        return false;
    }

    return true;
}

bool ChatHandler::HandleTicketRespondCommand(char* args)
{
    GMTicket* ticket = NULL;

    // ticket respond #num
    uint32 num;
    if (ExtractUInt32(&args, num))
    {
        if (num == 0)
        {
            return false;
        }

        // mgr numbering tickets start from 0
        ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {
        ObjectGuid target_guid;
        std::string target_name;
        if (!ExtractPlayerTarget(&args, NULL, &target_guid, &target_name))
        {
            return false;
        }

        // ticket respond $char_name
        ticket = sTicketMgr.GetGMTicket(target_guid);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
            SetSentErrorMessage(true);
            return false;
        }
    }

    // no response text?
    if (!*args)
    {
        return false;
    }

    // Set the response text to the ticket
    ticket->SetResponseText(args);

    // Send in-game email with ticket answer
    MailDraft draft;

    const char* signatureFormat = GetMangosString(LANG_COMMAND_TICKET_RESPOND_MAIL_SIGNATURE);
    const uint32 signatureBufferSize = 256;
    char signature[signatureBufferSize];

    if (m_session)
    {
        snprintf(signature, signatureBufferSize, signatureFormat, m_session->GetPlayer()->GetName());
    }
    else
    {
        // Used when the command is used via CLI console
        strcpy(signature, "$B$BBest regards, $B$BThe Server Admin");
    }


    std::string  mailText = args;
    mailText = mailText + signature;

    draft.SetSubjectAndBody(GetMangosString(LANG_COMMAND_TICKET_RESPOND_MAIL_SUBJECT), mailText);

    uint32 senderGuidLow = 0;
    if (m_session)
    {
        senderGuidLow = m_session->GetPlayer()->GetGUIDLow();
    }

    MailSender sender(MAIL_NORMAL, senderGuidLow, MAIL_STATIONERY_GM);

    ObjectGuid target_guid = ticket->GetPlayerGuid();

    // Get Player
    // Can be nullptr if player is offline
    Player* target = sObjectMgr.GetPlayer(target_guid);

    // Get Player name
    std::string target_name;
    sObjectMgr.GetPlayerNameByGUID(target_guid, target_name);

    // Find player to send, hopefully we have his guid if target is nullpt
    // Todo set MailDraft sent by GM and handle 90 day delay
    draft.SendMailTo(MailReceiver(target, target_guid), sender);

    const char* gmNameReplacementWhenUsingCLI = "ADMIN";

    // If player is online, notify with a system message  that the ticket was handled.
    if (target && target->IsInWorld())
    {
        ChatHandler(target).PSendSysMessageMultiline(LANG_COMMAND_TICKETCLOSED_PLAYER_NOTIF, m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);
    }

    // Define ticketId variable because we need ticket id after deleting it from TicketMgr in notification formated string
    uint32 ticketId = ticket->GetId();

    // Close the ticket
    ticket->Close();


    // Remove ticket from ticket manager
    // Otherwise ticket will reappear in player UI if teleported or logout/login !
    sTicketMgr.Delete(ticket->GetPlayerGuid());

    // Send system Message to All Connected GMs to informe them the ticket has been closed
    sObjectAccessor.DoForAllPlayers([&](Player* player)
        {
            if (player->GetSession()->GetSecurity() >= SEC_GAMEMASTER && player->isAcceptTickets())
            {
                ChatHandler(player).PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);
            }
        });


    if (!m_session)
    {
        // In order to have message in CLI otherwise above code will only display to connected gms but not in console
        PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), gmNameReplacementWhenUsingCLI);
    }

    return true;
}

bool ChatHandler::HandleTicketShowCommand(char* args)
{
    // ticket #num
    char* px = ExtractLiteralArg(&args);
    if (!px)
    {
        return false;
    }

    uint32 num;
    if (ExtractUInt32(&px, num))
    {
        if (num == 0)
        {
            return false;
        }

        // mgr numbering tickets start from 0
        GMTicket* ticket = sTicketMgr.GetGMTicket(num);
        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }

        ShowTicket(ticket);
        return true;
    }

    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&px, NULL, &target_guid, &target_name))
    {
        return false;
    }

    // ticket $char_name
    GMTicket* ticket = sTicketMgr.GetGMTicket(target_guid);
    if (!ticket)
    {
        PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    ShowTicket(ticket);

    return true;
}

bool ChatHandler::HandleTickerSurveyClose(char* args)
{
    GMTicket* ticket = NULL;
    std::string target_name;
    ObjectGuid target_guid;
    uint32 num;
    if (ExtractUInt32(&args, num))
    {
        if (num == 0)
        {
            return false;
        }

        ticket = sTicketMgr.GetGMTicket(num);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST, num);
            SetSentErrorMessage(true);
            return false;
        }
    }
    else
    {

        if (!ExtractPlayerTarget(&args, NULL, &target_guid, &target_name))
        {
            return false;
        }

        // ticket respond $char_name
        ticket = sTicketMgr.GetGMTicket(target_guid);

        if (!ticket)
        {
            PSendSysMessage(LANG_COMMAND_TICKETNOTEXIST_NAME, target_name.c_str());
            SetSentErrorMessage(true);
            return false;
        }
    }

    uint32 ticketId = ticket->GetId();
    ticket->CloseWithSurvey();

    //This needs to be before we delete the ticket
    Player* pPlayer = sObjectMgr.GetPlayer(ticket->GetPlayerGuid());

    //For now we can't close tickets for offline players, TODO
    if (!pPlayer)
    {
        SendSysMessage(LANG_COMMAND_TICKET_CANT_CLOSE);
        return false;
    }

    //This logic feels misplaced, but you can't have it in GMTicket?
    sTicketMgr.Delete(ticket->GetPlayerGuid());
    ticket = NULL;

    const char* gmNameReplacementWhenUsingCLI = "ADMIN";

    PSendSysMessage(LANG_COMMAND_TICKETCLOSED_NAME, ticketId, target_name.c_str(), m_session ? m_session->GetPlayer()->GetName() : gmNameReplacementWhenUsingCLI);

    return true;
}