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

/*
    All commands related to discussions
*/

 /**********************************************************************
     CommandTable : commandTable
 /***********************************************************************/

 // global announce
bool ChatHandler::HandleAnnounceCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    sWorld.SendWorldText(LANG_SYSTEMMESSAGE, args);
    return true;
}

// notification player at the screen
bool ChatHandler::HandleNotifyCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    std::string str = GetMangosString(LANG_GLOBAL_NOTIFY);
    str += args;

    WorldPacket data(SMSG_NOTIFICATION, (str.size() + 1));
    data << str;
    sWorld.SendGlobalMessage(&data);

    return true;
}

// mute player for some times
bool ChatHandler::HandleMuteCommand(char* args)
{
    char* nameStr = ExtractOptNotLastArg(&args);

    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&nameStr, &target, &target_guid, &target_name))
    {
        return false;
    }

    uint32 notspeaktime;
    if (!ExtractUInt32(&args, notspeaktime))
    {
        return false;
    }

    uint32 account_id = target ? target->GetSession()->GetAccountId() : sObjectMgr.GetPlayerAccountIdByGUID(target_guid);

    // find only player from same account if any
    if (!target)
    {
        if (WorldSession* session = sWorld.FindSession(account_id))
        {
            target = session->GetPlayer();
        }
    }

    // must have strong lesser security level
    if (HasLowerSecurity(target, target_guid, true))
    {
        return false;
    }

    time_t mutetime = time(NULL) + notspeaktime * 60;

    if (target)
    {
        target->GetSession()->m_muteTime = mutetime;
    }

    LoginDatabase.PExecute("UPDATE `account` SET `mutetime` = " UI64FMTD " WHERE `id` = '%u'", uint64(mutetime), account_id);

    if (target)
    {
        ChatHandler(target).PSendSysMessage(LANG_YOUR_CHAT_DISABLED, notspeaktime);
    }

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_YOU_DISABLE_CHAT, nameLink.c_str(), notspeaktime);
    return true;
}

// unmute player
bool ChatHandler::HandleUnmuteCommand(char* args)
{
    Player* target;
    ObjectGuid target_guid;
    std::string target_name;
    if (!ExtractPlayerTarget(&args, &target, &target_guid, &target_name))
    {
        return false;
    }

    uint32 account_id = target ? target->GetSession()->GetAccountId() : sObjectMgr.GetPlayerAccountIdByGUID(target_guid);

    // find only player from same account if any
    if (!target)
    {
        if (WorldSession* session = sWorld.FindSession(account_id))
        {
            target = session->GetPlayer();
        }
    }

    // must have strong lesser security level
    if (HasLowerSecurity(target, target_guid, true))
    {
        return false;
    }

    if (target)
    {
        if (target->CanSpeak())
        {
            SendSysMessage(LANG_CHAT_ALREADY_ENABLED);
            SetSentErrorMessage(true);
            return false;
        }

        target->GetSession()->m_muteTime = 0;
    }

    LoginDatabase.PExecute("UPDATE `account` SET `mutetime` = '0' WHERE `id` = '%u'", account_id);

    if (target)
    {
        ChatHandler(target).PSendSysMessage(LANG_YOUR_CHAT_ENABLED);
    }

    std::string nameLink = playerLink(target_name);

    PSendSysMessage(LANG_YOU_ENABLE_CHAT, nameLink.c_str());
    return true;
}

// Enable\Dissable accept whispers (for GM)
bool ChatHandler::HandleWhispersCommand(char* args)
{
    if (!*args)
    {
        PSendSysMessage(LANG_COMMAND_WHISPERACCEPTING, GetOnOffStr(m_session->GetPlayer()->isAcceptWhispers()));
        return true;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    // whisper on
    if (value)
    {
        m_session->GetPlayer()->SetAcceptWhispers(true);
        SendSysMessage(LANG_COMMAND_WHISPERON);
    }
    // whisper off
    else
    {
        m_session->GetPlayer()->SetAcceptWhispers(false);
        SendSysMessage(LANG_COMMAND_WHISPEROFF);
    }

    return true;
}

// Enables or disables hiding of the staff badge
bool ChatHandler::HandleGMChatCommand(char* args)
{
    if (!*args)
    {
        if (m_session->GetPlayer()->isGMChat())
        {
            m_session->SendNotification(LANG_GM_CHAT_ON);
        }
        else
        {
            m_session->SendNotification(LANG_GM_CHAT_OFF);
        }
        return true;
    }

    bool value;
    if (!ExtractOnOff(&args, value))
    {
        SendSysMessage(LANG_USE_BOL);
        SetSentErrorMessage(true);
        return false;
    }

    if (value)
    {
        m_session->GetPlayer()->SetGMChat(true);
        m_session->SendNotification(LANG_GM_CHAT_ON);
    }
    else
    {
        m_session->GetPlayer()->SetGMChat(false);
        m_session->SendNotification(LANG_GM_CHAT_OFF);
    }

    return true;
}


/**********************************************************************
    CommandTable : npcCommandTable
/***********************************************************************/

bool ChatHandler::HandleNpcSayCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterSay(args, LANG_UNIVERSAL, m_session->GetPlayer());

    return true;
}

bool ChatHandler::HandleNpcYellCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Creature* pCreature = getSelectedCreature();
    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterYell(args, LANG_UNIVERSAL, m_session->GetPlayer());

    return true;
}

// show text emote by creature in chat
bool ChatHandler::HandleNpcTextEmoteCommand(char* args)
{
    if (!*args)
    {
        return false;
    }

    Creature* pCreature = getSelectedCreature();

    if (!pCreature)
    {
        SendSysMessage(LANG_SELECT_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    pCreature->MonsterTextEmote(args, NULL);

    return true;
}

// make npc whisper to player
bool ChatHandler::HandleNpcWhisperCommand(char* args)
{
    Player* target;
    if (!ExtractPlayerTarget(&args, &target))
    {
        return false;
    }

    ObjectGuid guid = m_session->GetPlayer()->GetSelectionGuid();
    if (!guid)
    {
        return false;
    }

    Creature* pCreature = m_session->GetPlayer()->GetMap()->GetCreature(guid);

    if (!pCreature || !target || !*args)
    {
        return false;
    }

    // check online security
    if (HasLowerSecurity(target))
    {
        return false;
    }

    pCreature->MonsterWhisper(args, target);

    return true;
}

/// Send a message to a player in game
bool ChatHandler::HandleSendMessageCommand(char* args)
{
    ///- Find the player
    Player* rPlayer;
    if (!ExtractPlayerTarget(&args, &rPlayer))
    {
        return false;
    }

    ///- message
    if (!*args)
    {
        return false;
    }

    WorldSession* rPlayerSession = rPlayer->GetSession();

    ///- Check that he is not logging out.
    if (rPlayerSession->isLogingOut())
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }

    ///- Send the message
    // Use SendAreaTriggerMessage for fastest delivery.
    rPlayerSession->SendAreaTriggerMessage("%s", args);
    rPlayerSession->SendAreaTriggerMessage("|cffff0000[Message from administrator]:|r");

    // Confirmation message
    std::string nameLink = GetNameLink(rPlayer);
    PSendSysMessage(LANG_SENDMESSAGE, nameLink.c_str(), args);
    return true;
}

