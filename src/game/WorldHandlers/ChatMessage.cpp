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
 * @file Chat.cpp
 * @brief Chat system implementation
 *
 * This file implements the chat system including:
 * - Message formatting and color codes
 * - Shift-link parsing (item, spell, quest links)
 * - Channel message routing
 * - Whisper, say, yell, emote handling
 * - GM command parsing and execution
 * - Language filtering
 *
 * The chat system supports various message types with different
 * visibility ranges and formatting requirements.
 *
 * @see ChatHandler for command handling
 * @see Channel for channel chat
 */



#include "Chat.h"
#include "Language.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "World.h"
#include "Player.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "AccountMgr.h"
#include "SpellMgr.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "AuctionHouseBot/AuctionHouseBot.h"
#include "CommandMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Sends a system message to the current session, splitting multiline text.
 *
 * @param str The message text to send.
 */
void ChatHandler::SendSysMessage(const char* str)
{
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = mangos_strdup(str);
    char* pos = buf;

    while (char* line = LineFromMessage(pos))
    {
        // m_session == null when we're accessing these command from the console.
        ObjectGuid senderGuid;
        if (m_session)
        {
            senderGuid = m_session->GetPlayer()->GetObjectGuid();
        }

        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, senderGuid);
        m_session->SendPacket(&data);
    }

    delete[] buf;
}

/**
 * @brief Sends a global system message to all sessions meeting the minimum security level.
 *
 * @param str The message text to send.
 * @param minSec The minimum security level required to receive the message.
 */
void ChatHandler::SendGlobalSysMessage(const char* str, AccountTypes minSec)
{
    // Chat output
    WorldPacket data;

    // need copy to prevent corruption by strtok call in LineFromMessage original string
    char* buf = mangos_strdup(str);
    char* pos = buf;
    ObjectGuid senderGuid = m_session ? m_session->GetPlayer()->GetObjectGuid() : ObjectGuid();

    while (char* line = LineFromMessage(pos))
    {
        ChatHandler::BuildChatPacket(data, CHAT_MSG_SYSTEM, line, LANG_UNIVERSAL, CHAT_TAG_NONE, senderGuid);
        sWorld.SendGlobalMessage(&data, minSec);
    }

    delete[] buf;
}

/**
 * @brief Sends a localized system message by string table entry.
 *
 * @param entry The string table entry identifier.
 */
void ChatHandler::SendSysMessage(int32 entry)
{
    SendSysMessage(GetMangosString(entry));
}

/**
 * @brief Formats and sends a localized system message.
 *
 * @param entry The string table entry identifier.
 */
void ChatHandler::PSendSysMessage(int32 entry, ...)
{
    const char* format = GetMangosString(entry);
    va_list ap;
    char str [2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

/**
 * @brief Formats and sends a localized multiline system message using @@ separators.
 *
 * @param entry The string table entry identifier.
 */
void  ChatHandler::PSendSysMessageMultiline(int32 entry, ...)
{
    uint32 linecount = 0;

    const char* format = GetMangosString(entry);
    va_list ap;
    char str[2048];
    va_start(ap, entry);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);

    std::string mangosString(str);

    /* Used for tracking our position within the string while iterating through it */
    std::string::size_type pos = 0, nextpos;

    /** Find the next occurance of @ in the string
     * This is how newlines are represented */
    while ((nextpos = mangosString.find("@@", pos)) != std::string::npos)
    {
        /** If these are not equal, it means a '@@' was found
         * These are used to represent newlines in the string
         * It is set by the code above here */
        if (nextpos != pos)
        {
            /* Send the player a system message containing the substring from pos to nextpos - pos */
            PSendSysMessage("%s", mangosString.substr(pos, nextpos - pos).c_str());
            ++linecount;
        }
        pos = nextpos + 2; // +2 because there are two @ as delimiter
    }

    /* There are no more newlines in our mangosString, so we send whatever is left */
    if (pos < mangosString.length())
    {
        PSendSysMessage("%s", mangosString.substr(pos).c_str());
    }
}

/**
 * @brief Formats and sends a raw system message.
 *
 * @param format The printf-style format string.
 */
void ChatHandler::PSendSysMessage(const char* format, ...)
{
    va_list ap;
    char str [2048];
    va_start(ap, format);
    vsnprintf(str, 2048, format, ap);
    va_end(ap);
    SendSysMessage(str);
}

void ChatHandler::BuildChatPacket(WorldPacket& data, ChatMsg msgtype, char const* message, Language language /*= LANG_UNIVERSAL*/, ChatTagFlags chatTag /*= CHAT_TAG_NONE*/,
    ObjectGuid const& senderGuid /*= ObjectGuid()*/, char const* senderName /*= NULL*/,
    ObjectGuid const& targetGuid /*= ObjectGuid()*/, char const* /*targetName*/ /*= NULL*/,
    char const* channelName /*= NULL*/, uint8 playerRank /*= 0*/)
{
    data.Initialize(SMSG_MESSAGECHAT);
    data << uint8(msgtype);
    data << uint32(language);

    switch (msgtype)
    {
        case CHAT_MSG_MONSTER_WHISPER:
        case CHAT_MSG_RAID_BOSS_WHISPER:
        case CHAT_MSG_RAID_BOSS_EMOTE:
        case CHAT_MSG_MONSTER_EMOTE:
            MANGOS_ASSERT(senderName);
            data << uint32(strlen(senderName) + 1);
            data << senderName;
            data << ObjectGuid(targetGuid);                         // Unit Target
            break;

        case CHAT_MSG_SAY:
        case CHAT_MSG_PARTY:
        case CHAT_MSG_YELL:
            data << ObjectGuid(senderGuid);
            data << ObjectGuid(senderGuid);
            break;

        case CHAT_MSG_MONSTER_SAY:
        case CHAT_MSG_MONSTER_YELL:
            MANGOS_ASSERT(senderName);
            data << ObjectGuid(senderGuid);
            data << uint32(strlen(senderName) + 1);
            data << senderName;
            data << ObjectGuid(targetGuid);                         // Unit Target
            break;

        case CHAT_MSG_CHANNEL:
            MANGOS_ASSERT(channelName);
            data << channelName;
            data << uint32(playerRank);
            data << ObjectGuid(senderGuid);
            break;

        default:
            data << ObjectGuid(senderGuid);
            break;
    }

    MANGOS_ASSERT(message);
    data << uint32(strlen(message) + 1);
    data << message;
    data << uint8(chatTag);
}
