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
#include "Database/DatabaseEnv.h"
#include "SQLStorages.h"
#include "GMTicketMgr.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "ProgressBar.h"
#include "Policies/Singleton.h"
#include "Player.h"

INSTANTIATE_SINGLETON_1(GMTicketMgr);

void GMTicket::SaveSurveyData(WorldPacket& recvData) const
{
    uint32 x;
    recvData >> x;                                         // answer range? (6 = 0-5?)
    DEBUG_LOG("SURVEY: X = %u", x);
    
    uint8 result[10];
    memset(result, 0, sizeof(result));
    for (int i = 0; i < 10; ++i)
    {
        uint32 questionID;
        recvData >> questionID;                            // GMSurveyQuestions.dbc
        if (!questionID)
            break;
        
        uint8 value;
        std::string unk_text;
        recvData >> value;                                 // answer
        recvData >> unk_text;                              // always empty?
        
        result[i] = value;
        DEBUG_LOG("SURVEY: ID %u, value %u, text %s", questionID, value, unk_text.c_str());
    }
    
    std::string comment;
    recvData >> comment;                                   // addional comment
    DEBUG_LOG("SURVEY: comment %s", comment.c_str());
    
    // TODO: chart this data in some way in DB
}

void GMTicket::Init(ObjectGuid guid, const std::string& text, const std::string& responseText, time_t update, uint32 ticketId)
{
    m_guid = guid;
    m_ticketId = ticketId;
    m_text = text;
    m_responseText = responseText;
    m_lastUpdate = update;
}

void GMTicket::SetText(const char* text)
{
    m_text = text ? text : "";
    m_lastUpdate = time(NULL);

    std::string escapedString = m_text;
    CharacterDatabase.escape_string(escapedString);
    CharacterDatabase.PExecute("UPDATE character_ticket SET ticket_text = '%s' "
                               "WHERE guid = '%u'",
                               escapedString.c_str(), m_guid.GetCounter());
}

void GMTicket::SetResponseText(const char* text)
{
    m_responseText = text ? text : "";
    m_lastUpdate = time(NULL);

    std::string escapedString = m_responseText;
    CharacterDatabase.escape_string(escapedString);
    CharacterDatabase.PExecute("UPDATE character_ticket SET response_text = '%s' "
                               "WHERE guid = '%u'",
                               escapedString.c_str(), m_guid.GetCounter());
}

void GMTicket::CloseWithSurvey() const
{
    _Close(GM_TICKET_STATUS_SURVEY);
}

void GMTicket::CloseByClient() const
{
    _Close(GM_TICKET_STATUS_DO_NOTHING);
}

void GMTicket::Close() const
{
    _Close(GM_TICKET_STATUS_CLOSE);
}

void GMTicket::_Close(GMTicketStatus statusCode) const
{
    Player* pPlayer = sObjectMgr.GetPlayer(m_guid);
    
    CharacterDatabase.PExecute("UPDATE character_ticket "
                               "SET resolved = 1 "
                               "WHERE guid = %u AND resolved = 0",
                               m_guid.GetCounter());
    
    if (pPlayer && statusCode != GM_TICKET_STATUS_DO_NOTHING)
        { pPlayer->GetSession()->SendGMTicketStatusUpdate(statusCode); }
}

void GMTicketMgr::LoadGMTickets()
{
    m_GMTicketMap.clear();                                  // For reload case

    QueryResult* result = CharacterDatabase.Query(
    //      0     1            2              3                                  4
    "SELECT guid, ticket_text, response_text, UNIX_TIMESTAMP(ticket_lastchange), ticket_id "
    "FROM character_ticket "
    "WHERE resolved = 0 "
    "ORDER BY ticket_id ASC");
    
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded `character_ticket`, table is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 guidlow = fields[0].GetUInt32();
        if (!guidlow)
            { continue; }

        ObjectGuid guid = ObjectGuid(HIGHGUID_PLAYER, guidlow);
        GMTicket& ticket = m_GMTicketMap[guid];
        
        ticket.Init(guid, fields[1].GetCppString(), fields[2].GetCppString(), time_t(fields[3].GetUInt64()), fields[4].GetUInt32());
        m_GMTicketIdMap[ticket.GetId()] = &ticket;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " GM tickets", GetTicketCount());
    sLog.outString();
}

void GMTicketMgr::Create(ObjectGuid guid, const char* text)
{
    std::string escapedText = text;
    CharacterDatabase.escape_string(escapedText);
    CharacterDatabase.BeginTransaction();
    //This needs to be Direct (not placed in queue) as we need the id of it soon afterwards
    CharacterDatabase.DirectPExecute("INSERT INTO character_ticket "
                                     "(guid, ticket_text) "
                                     "VALUES "
                                     "(%u,   '%s')",
                                     guid.GetCounter(), escapedText.c_str());
    
    //Get the id of the ticket, needed for logging whispers
    QueryResult* result = CharacterDatabase.PQuery("SELECT ticket_id, guid, resolved "
                                                   "FROM character_ticket "
                                                   "WHERE guid = %u AND resolved = 0;",
                                                   guid.GetCounter());
    
    CharacterDatabase.CommitTransaction();
    
    if (!result)
        return;
    
    Field* fields = result->Fetch();
    uint32 ticketId = fields[0].GetUInt32();
    
    //This implicitly creates a new instance since we're using operator[]
    GMTicket& ticket = m_GMTicketMap[guid];
    if (ticket.GetPlayerGuid())
        m_GMTicketIdMap.erase(ticketId);
            
    //Lets reinitialize with new data
    ticket.Init(guid, text, "", time(NULL), ticketId);
    m_GMTicketIdMap[ticketId] = &ticket;
}

void GMTicketMgr::DeleteAll()
{
    for (GMTicketMap::const_iterator itr = m_GMTicketMap.begin(); itr != m_GMTicketMap.end(); ++itr)
    {
        if (Player* owner = sObjectMgr.GetPlayer(itr->first))
            { owner->GetSession()->SendGMTicketGetTicket(0x0A); }
    }
    CharacterDatabase.Execute("DELETE FROM character_ticket");
    m_GMTicketIdMap.clear();
    m_GMTicketMap.clear();
}
