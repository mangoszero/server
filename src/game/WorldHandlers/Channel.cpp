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
 * @file Channel.cpp
 * @brief Chat channel implementation
 *
 * This file implements the Channel class which manages chat channels
 * including built-in channels (General, Trade, etc.) and custom player-created
 * channels. It handles:
 *
 * - Channel creation and initialization
 * - Player join/leave operations
 * - Channel moderation (kick, ban, mute)
 * - Message broadcasting
 * - Channel announcements
 * - Built-in vs custom channel behavior
 *
 * @see Channel for the channel class
 * @see ChannelMgr for channel management
 */

#include "Channel.h"
#include "ObjectMgr.h"
#include "World.h"
#include "SocialMgr.h"
#include "Chat.h"

Channel::Channel(const std::string& name)
    : m_announce(true), m_moderate(false), m_name(name), m_flags(0), m_channelId(0)
{
    // set special flags if built-in channel
    ChatChannelsEntry const* ch = GetChannelEntryFor(name);
    if (ch)                                                 // it's built-in channel
    {
        m_channelId = ch->ID;                                // only built-in channel have channel id != 0
        m_announce = false;                                 // no join/leave announces

        m_flags |= CHANNEL_FLAG_GENERAL;                    // for all built-in channels

        if (ch->Flags & CHANNEL_DBC_FLAG_TRADE)             // for trade channel
        {
            m_flags |= CHANNEL_FLAG_TRADE;
        }

        if (ch->Flags & CHANNEL_DBC_FLAG_CITY_ONLY2)        // for city only channels
        {
            m_flags |= CHANNEL_FLAG_CITY;
        }

        if (ch->Flags & CHANNEL_DBC_FLAG_LFG)               // for LFG channel
        {
            m_flags |= CHANNEL_FLAG_LFG;
        }
        else                                                // for all other channels
        {
            m_flags |= CHANNEL_FLAG_NOT_LFG;
        }
    }
    else                                                    // it's custom channel
    {
        m_flags |= CHANNEL_FLAG_CUSTOM;
        m_announce = (name.compare("world") != 0);
    }
}

/**
 * @brief Adds a player to the channel after validating membership, bans, and password.
 *
 * @param player The player attempting to join.
 * @param password The supplied channel password.
 */
void Channel::Join(Player* player, const char* password)
{
    ObjectGuid guid = player->GetObjectGuid();

    WorldPacket data;
    if (IsOn(guid))
    {
        if (!IsConstant())                                  // non send error message for built-in channels
        {
            MakePlayerAlreadyMember(&data, guid);
            SendToOne(&data, guid);
        }
        return;
    }

    if (IsBanned(guid))
    {
        MakeBanned(&data);
        SendToOne(&data, guid);
        return;
    }

    if (m_password.length() > 0 && strcmp(password, m_password.c_str()))
    {
        MakeWrongPassword(&data);
        SendToOne(&data, guid);
        return;
    }

    if (player->GetGuildId() && (GetFlags() == 0x38))
    {
        return;
    }

    // join channel
    player->JoinedChannel(this);

    if (m_announce && (player->GetSession()->GetSecurity() < SEC_GAMEMASTER || !sWorld.getConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL)))
    {
        MakeJoined(&data, guid);
        SendToAll(&data);
    }

    data.clear();

    PlayerInfo& pinfo = m_players[guid];
    pinfo.player = guid;
    pinfo.flags = MEMBER_FLAG_NONE;

    MakeYouJoined(&data);
    SendToOne(&data, guid);

    JoinNotify(guid);

    // if no owner first logged will become
    if (!IsConstant() && !m_ownerGuid)
    {
        SetOwner(guid, (m_players.size() > 1 ? true : false));
        m_players[guid].SetModerator(true);
    }
}

/**
 * @brief Removes a player from the channel and updates ownership if needed.
 *
 * @param player The player leaving the channel.
 * @param send True to send leave notifications to the player.
 */
void Channel::Leave(Player* player, bool send)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        if (send)
        {
            WorldPacket data;
            MakeNotMember(&data);
            SendToOne(&data, guid);
        }
        return;
    }

    // leave channel
    if (send)
    {
        WorldPacket data;
        MakeYouLeft(&data);
        SendToOne(&data, guid);
        player->LeftChannel(this);
        data.clear();
    }

    bool changeowner = m_players[guid].IsOwner();

    m_players.erase(guid);
    if (m_announce && (player->GetSession()->GetSecurity() < SEC_GAMEMASTER || !sWorld.getConfig(CONFIG_BOOL_SILENTLY_GM_JOIN_TO_CHANNEL)))
    {
        WorldPacket data;
        MakeLeft(&data, guid);
        SendToAll(&data);
    }

    LeaveNotify(guid);

    if (changeowner)
    {
        ObjectGuid newowner = !m_players.empty() ? m_players.begin()->second.player : ObjectGuid();
        SetOwner(newowner);
    }
}

/**
 * @brief Kicks or bans a target player from the channel.
 *
 * @param player The moderator issuing the command.
 * @param targetName The target player name.
 * @param ban True to ban the player in addition to removing them.
 */
void Channel::KickOrBan(Player* player, const char* targetName, bool ban)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    bool changeowner = m_ownerGuid == targetGuid;

    if (player->GetSession()->GetSecurity() < SEC_GAMEMASTER && changeowner && guid != m_ownerGuid)
    {
        WorldPacket data;
        MakeNotOwner(&data);
        SendToOne(&data, guid);
        return;
    }

    // kick or ban player
    WorldPacket data;

    if (ban && !IsBanned(targetGuid))
    {
        m_banned.insert(targetGuid);
        MakePlayerBanned(&data, targetGuid, guid);
    }
    else
    {
        MakePlayerKicked(&data, targetGuid, guid);
    }

    SendToAll(&data);
    m_players.erase(targetGuid);
    target->LeftChannel(this);

    if (changeowner)
    {
        ObjectGuid newowner = !m_players.empty() ? guid : ObjectGuid();
        SetOwner(newowner);
    }
}

/**
 * @brief Removes a ban from the specified player.
 *
 * @param player The moderator issuing the unban.
 * @param targetName The target player name.
 */
void Channel::UnBan(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsBanned(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotBanned(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    // unban player
    m_banned.erase(targetGuid);

    WorldPacket data;
    MakePlayerUnbanned(&data, targetGuid, guid);
    SendToAll(&data);
}

/**
 * @brief Changes the channel password.
 *
 * @param player The moderator changing the password.
 * @param password The new password string.
 */
void Channel::Password(Player* player, const char* password)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    // set channel password
    m_password = password;

    WorldPacket data;
    MakePasswordChanged(&data, guid);
    SendToAll(&data);
}

/**
 * @brief Sets or clears moderator or mute mode for a channel member.
 *
 * @param player The moderator issuing the change.
 * @param targetName The target player name.
 * @param moderator True to change moderator state, false to change mute state.
 * @param set True to enable the mode, false to clear it.
 */
void Channel::SetMode(Player* player, const char* targetName, bool moderator, bool set)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (moderator && guid == m_ownerGuid && targetGuid == m_ownerGuid)
    {
        return;
    }

    if (!IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    // allow make moderator from another team only if both is GMs
    // at this moment this only way to show channel post for GM from another team
    if ((player->GetSession()->GetSecurity() < SEC_GAMEMASTER || target->GetSession()->GetSecurity() < SEC_GAMEMASTER) &&
        player->GetTeam() != target->GetTeam() && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    if (m_ownerGuid == targetGuid && m_ownerGuid != guid)
    {
        WorldPacket data;
        MakeNotOwner(&data);
        SendToOne(&data, guid);
        return;
    }

    // set channel moderator
    if (moderator)
    {
        SetModerator(targetGuid, set);
    }
    else
    {
        SetMute(targetGuid, set);
    }
}

/**
 * @brief Transfers channel ownership to another member.
 *
 * @param player The current owner or GM issuing the command.
 * @param targetName The target player name.
 */
void Channel::SetOwner(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (player->GetSession()->GetSecurity() < SEC_GAMEMASTER && guid != m_ownerGuid)
    {
        WorldPacket data;
        MakeNotOwner(&data);
        SendToOne(&data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (!IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    if (target->GetTeam() != player->GetTeam() && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    // set channel owner
    m_players[targetGuid].SetModerator(true);
    SetOwner(targetGuid);
}

/**
 * @brief Sends the current channel owner information to a member.
 *
 * @param player The player requesting the owner information.
 */
void Channel::SendWhoOwner(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    // send channel owner
    WorldPacket data;
    MakeChannelOwner(&data);
    SendToOne(&data, guid);
}

/**
 * @brief Sends the visible channel member list to a player.
 *
 * @param player The player requesting the member list.
 */
void Channel::List(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    // list players in channel
    WorldPacket data(SMSG_CHANNEL_LIST, (GetName().size() + 1) + 1 + 4 + m_players.size() * (8 + 1));   // guess size
    data << GetName();                                      // channel name
    data << uint8(GetFlags());                              // channel flags?

    size_t pos = data.wpos();
    data << int32(0);                                       // size of list, placeholder

    AccountTypes gmLevelInWhoList = (AccountTypes)sWorld.getConfig(CONFIG_UINT32_GM_LEVEL_IN_WHO_LIST);

    uint32 count  = 0;
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
    {
        Player* plr = sObjectMgr.GetPlayer(i->first);

        // PLAYER can't see MODERATOR, GAME MASTER, ADMINISTRATOR characters
        // MODERATOR, GAME MASTER, ADMINISTRATOR can see all
        if (plr && (player->GetSession()->GetSecurity() > SEC_PLAYER || plr->GetSession()->GetSecurity() <= gmLevelInWhoList) &&
            plr->IsVisibleGloballyFor(player))
        {
            data << ObjectGuid(i->first);
            data << uint8(i->second.flags);                 // flags seems to be changed...
            ++count;
        }
    }

    data.put<int32>(pos, count);

    SendToOne(&data, guid);
}

/**
 * @brief Toggles join and leave announcements for the channel.
 *
 * @param player The moderator issuing the toggle.
 */
void Channel::Announce(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    // toggle channel announcement
    m_announce = !m_announce;

    WorldPacket data;
    if (m_announce)
    {
        MakeAnnouncementsOn(&data, guid);
    }
    else
    {
        MakeAnnouncementsOff(&data, guid);
    }
    SendToAll(&data);
}

/**
 * @brief Toggles moderated mode for the channel.
 *
 * @param player The moderator issuing the toggle.
 */
void Channel::Moderate(Player* player)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    if (!m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    // toggle channel moderation
    m_moderate = !m_moderate;

    WorldPacket data;
    if (m_moderate)
    {
        MakeModerationOn(&data, guid);
    }
    else
    {
        MakeModerationOff(&data, guid);
    }
    SendToAll(&data);
}

/**
 * @brief Sends a chat message to all eligible channel members.
 *
 * @param player The player speaking in the channel.
 * @param text The message text.
 * @param lang The chat language identifier.
 */
void Channel::Say(Player* player, const char* text, uint32 lang)
{
    if (!text)
    {
        return;
    }

    ObjectGuid guid = player->GetObjectGuid();
    Player* plr = sObjectMgr.GetPlayer(guid);
    bool speakInLocalDef = false;
    bool speakInWorldDef = false;
    if (plr)
    {
        if (plr->isGameMaster())
        {
            speakInLocalDef = true;
            speakInWorldDef = true;
        }

        // Only Applicable for Vanilla
        HonorRankInfo honorInfo = plr->GetHonorRankInfo();
        //We can speak in local defense if we're above this rank (see .h file)
        if (honorInfo.rank >= SPEAK_IN_LOCALDEFENSE_RANK)
        {
            speakInLocalDef = true;
        }
    }

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }
    else if (m_players[guid].IsMuted() ||
        (GetChannelId() == CHANNEL_ID_LOCAL_DEFENSE && !speakInLocalDef) ||
        (GetChannelId() == CHANNEL_ID_WORLD_DEFENSE && !speakInWorldDef))
    {
        WorldPacket data;
        MakeMuted(&data);
        SendToOne(&data, guid);
        return;
    }

    if (m_moderate && !m_players[guid].IsModerator() && player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        WorldPacket data;
        MakeNotModerator(&data);
        SendToOne(&data, guid);
        return;
    }

    // send channel message
    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        lang = LANG_UNIVERSAL;
    }
    WorldPacket data;
    ChatHandler::BuildChatPacket(data, CHAT_MSG_CHANNEL, text, Language(lang), player->GetChatTag(), guid, player->GetName(), ObjectGuid(), "", m_name.c_str(), player->GetHonorRankInfo().rank);
    SendToAll(&data, !m_players[guid].IsModerator() ? guid : ObjectGuid());
}

/**
 * @brief Invites another player to the channel.
 *
 * @param player The player sending the invitation.
 * @param targetName The target player name.
 */
void Channel::Invite(Player* player, const char* targetName)
{
    ObjectGuid guid = player->GetObjectGuid();

    if (!IsOn(guid))
    {
        WorldPacket data;
        MakeNotMember(&data);
        SendToOne(&data, guid);
        return;
    }

    Player* target = sObjectMgr.GetPlayer(targetName);
    if (!target)
    {
        WorldPacket data;
        MakePlayerNotFound(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    ObjectGuid targetGuid = target->GetObjectGuid();
    if (IsOn(targetGuid))
    {
        WorldPacket data;
        MakePlayerAlreadyMember(&data, targetGuid);
        SendToOne(&data, guid);
        return;
    }

    if (IsBanned(targetGuid))
    {
        WorldPacket data;
        MakePlayerInviteBanned(&data, targetName);
        SendToOne(&data, guid);
        return;
    }

    if (target->GetTeam() != player->GetTeam() && !sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        WorldPacket data;
        MakeInviteWrongFaction(&data);
        SendToOne(&data, guid);
        return;
    }

    // invite player
    WorldPacket data;
    if (!target->GetSocial()->HasIgnore(guid))
    {
        MakeInvite(&data, guid);
        SendToOne(&data, targetGuid);
        data.clear();
    }

    MakePlayerInvited(&data, targetName);
    SendToOne(&data, guid);
}

/**
 * @brief Updates the stored owner GUID and broadcasts ownership state changes.
 *
 * @param guid The new owner GUID.
 * @param exclaim True to broadcast the owner changed notice.
 */
void Channel::SetOwner(ObjectGuid guid, bool exclaim)
{
    if (m_ownerGuid)
    {
        // [] will re-add player after it possible removed
        PlayerList::iterator p_itr = m_players.find(m_ownerGuid);
        if (p_itr != m_players.end())
        {
            p_itr->second.SetOwner(false);
        }
    }

    m_ownerGuid = guid;

    if (m_ownerGuid)
    {
        uint8 oldFlag = GetPlayerFlags(m_ownerGuid);
        m_players[m_ownerGuid].SetOwner(true);

        WorldPacket data;
        MakeModeChange(&data, m_ownerGuid, oldFlag);
        SendToAll(&data);

        if (exclaim)
        {
            MakeOwnerChanged(&data, m_ownerGuid);
            SendToAll(&data);
        }
    }
}

/**
 * @brief Sends a packet to all channel members, respecting ignores for an optional sender.
 *
 * @param data The packet to send.
 * @param guid The optional sender GUID used for ignore filtering.
 */
void Channel::SendToAll(WorldPacket* data, ObjectGuid guid)
{
    for (PlayerList::const_iterator i = m_players.begin(); i != m_players.end(); ++i)
    {
        if (Player* plr = sObjectMgr.GetPlayer(i->first))
        {
            if (!guid || !plr->GetSocial()->HasIgnore(guid))
            {
                plr->GetSession()->SendPacket(data);
            }
        }
    }
}

/**
 * @brief Sends a packet to a specific player.
 *
 * @param data The packet to send.
 * @param who The recipient player GUID.
 */
void Channel::SendToOne(WorldPacket* data, ObjectGuid who)
{
    if (Player* plr = ObjectMgr::GetPlayer(who))
    {
        plr->GetSession()->SendPacket(data);
    }
}

/**
 * @brief Placeholder handler for granting channel voice.
 *
 * @param guid1 Unused source GUID.
 * @param guid2 Unused target GUID.
 */
void Channel::Voice(ObjectGuid /*guid1*/, ObjectGuid /*guid2*/)
{
}

/**
 * @brief Placeholder handler for removing channel voice.
 *
 * @param guid1 Unused source GUID.
 * @param guid2 Unused target GUID.
 */
void Channel::DeVoice(ObjectGuid /*guid1*/, ObjectGuid /*guid2*/)
{
}

/**
 * @brief Initializes a generic channel notify packet.
 *
 * @param data The packet to initialize.
 * @param notify_type The channel notification opcode subtype.
 */
void Channel::MakeNotifyPacket(WorldPacket* data, uint8 notify_type)
{
    data->Initialize(SMSG_CHANNEL_NOTIFY, 1 + m_name.size() + 1);
    *data << uint8(notify_type);
    *data << m_name;
}

/**
 * @brief Builds a packet announcing that a player joined the channel.
 *
 * @param data The packet to fill.
 * @param guid The joining player GUID.
 */
void Channel::MakeJoined(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_JOINED_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds a packet announcing that a player left the channel.
 *
 * @param data The packet to fill.
 * @param guid The leaving player GUID.
 */
void Channel::MakeLeft(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_LEFT_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the self-notification packet for joining the channel.
 *
 * @param data The packet to fill.
 */
void Channel::MakeYouJoined(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_YOU_JOINED_NOTICE);
    *data << uint32(GetFlags());
    *data << uint32(0);                                     // the non-zero number will be appended to the channel name
    *data << uint8(0);                                      // CString max length 512, conditional read
}

/**
 * @brief Builds the self-notification packet for leaving the channel.
 *
 * @param data The packet to fill.
 */
void Channel::MakeYouLeft(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_YOU_LEFT_NOTICE);
}

/**
 * @brief Builds the wrong-password notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeWrongPassword(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_WRONG_PASSWORD_NOTICE);
}

/**
 * @brief Builds the not-a-member notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeNotMember(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_NOT_MEMBER_NOTICE);
}

/**
 * @brief Builds a not-on-channel notification packet using a channel name.
 *
 * @param data The packet to fill.
 * @param name The channel name.
 */
void Channel::MakeNotOnPacket(WorldPacket* data, const std::string &name)
{
    data->Initialize(SMSG_CHANNEL_NOTIFY, (1 + name.length() + 1));
    (*data) << (uint8)CHAT_NOT_MEMBER_NOTICE << name;
}

/**
 * @brief Builds the not-a-moderator notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeNotModerator(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_NOT_MODERATOR_NOTICE);
}

/**
 * @brief Builds the password-changed notification packet.
 *
 * @param data The packet to fill.
 * @param guid The GUID of the player who changed the password.
 */
void Channel::MakePasswordChanged(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_PASSWORD_CHANGED_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the owner-changed notification packet.
 *
 * @param data The packet to fill.
 * @param guid The new owner GUID.
 */
void Channel::MakeOwnerChanged(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_OWNER_CHANGED_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the player-not-found notification packet.
 *
 * @param data The packet to fill.
 * @param name The unresolved player name.
 */
void Channel::MakePlayerNotFound(WorldPacket* data, const std::string& name)
{
    MakeNotifyPacket(data, CHAT_PLAYER_NOT_FOUND_NOTICE);
    *data << name;
}

/**
 * @brief Builds the not-owner notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeNotOwner(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_NOT_OWNER_NOTICE);
}

/**
 * @brief Builds the packet that reports the current channel owner.
 *
 * @param data The packet to fill.
 */
void Channel::MakeChannelOwner(WorldPacket* data)
{
    std::string name = "";

    if (!sObjectMgr.GetPlayerNameByGUID(m_ownerGuid, name) || name.empty())
    {
        name = "PLAYER_NOT_FOUND";
    }

    MakeNotifyPacket(data, CHAT_CHANNEL_OWNER_NOTICE);
    *data << ((IsConstant() || !m_ownerGuid) ? "Nobody" : name);
}

/**
 * @brief Builds the packet describing a member flag change.
 *
 * @param data The packet to fill.
 * @param guid The affected member GUID.
 * @param oldflags The previous member flags.
 */
void Channel::MakeModeChange(WorldPacket* data, ObjectGuid guid, uint8 oldflags)
{
    MakeNotifyPacket(data, CHAT_MODE_CHANGE_NOTICE);
    *data << ObjectGuid(guid);
    *data << uint8(oldflags);
    *data << uint8(GetPlayerFlags(guid));
}

/**
 * @brief Builds the announcements-enabled notification packet.
 *
 * @param data The packet to fill.
 * @param guid The GUID of the player who enabled announcements.
 */
void Channel::MakeAnnouncementsOn(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_ANNOUNCEMENTS_ON_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the announcements-disabled notification packet.
 *
 * @param data The packet to fill.
 * @param guid The GUID of the player who disabled announcements.
 */
void Channel::MakeAnnouncementsOff(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_ANNOUNCEMENTS_OFF_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the moderation-enabled notification packet.
 *
 * @param data The packet to fill.
 * @param guid The GUID of the player who enabled moderation.
 */
void Channel::MakeModerationOn(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_MODERATION_ON_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the moderation-disabled notification packet.
 *
 * @param data The packet to fill.
 * @param guid The GUID of the player who disabled moderation.
 */
void Channel::MakeModerationOff(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_MODERATION_OFF_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the muted notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeMuted(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_MUTED_NOTICE);
}

/**
 * @brief Builds the player-kicked notification packet.
 *
 * @param data The packet to fill.
 * @param target The removed player GUID.
 * @param source The moderator GUID.
 */
void Channel::MakePlayerKicked(WorldPacket* data, ObjectGuid target, ObjectGuid source)
{
    MakeNotifyPacket(data, CHAT_PLAYER_KICKED_NOTICE);
    *data << ObjectGuid(target);
    *data << ObjectGuid(source);
}

/**
 * @brief Builds the banned-from-channel notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeBanned(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_BANNED_NOTICE);
}

/**
 * @brief Builds the player-banned notification packet.
 *
 * @param data The packet to fill.
 * @param target The banned player GUID.
 * @param source The moderator GUID.
 */
void Channel::MakePlayerBanned(WorldPacket* data, ObjectGuid target, ObjectGuid source)
{
    MakeNotifyPacket(data, CHAT_PLAYER_BANNED_NOTICE);
    *data << ObjectGuid(target);
    *data << ObjectGuid(source);
}

/**
 * @brief Builds the player-unbanned notification packet.
 *
 * @param data The packet to fill.
 * @param target The unbanned player GUID.
 * @param source The moderator GUID.
 */
void Channel::MakePlayerUnbanned(WorldPacket* data, ObjectGuid target, ObjectGuid source)
{
    MakeNotifyPacket(data, CHAT_PLAYER_UNBANNED_NOTICE);
    *data << ObjectGuid(target);
    *data << ObjectGuid(source);
}

/**
 * @brief Builds the player-not-banned notification packet.
 *
 * @param data The packet to fill.
 * @param name The target player name.
 */
void Channel::MakePlayerNotBanned(WorldPacket* data, const std::string& name)
{
    MakeNotifyPacket(data, CHAT_PLAYER_NOT_BANNED_NOTICE);
    *data << name;
}

/**
 * @brief Builds the already-a-member notification packet.
 *
 * @param data The packet to fill.
 * @param guid The member GUID already in the channel.
 */
void Channel::MakePlayerAlreadyMember(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_PLAYER_ALREADY_MEMBER_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the invitation packet sent to the invited player.
 *
 * @param data The packet to fill.
 * @param guid The inviter GUID.
 */
void Channel::MakeInvite(WorldPacket* data, ObjectGuid guid)
{
    MakeNotifyPacket(data, CHAT_INVITE_NOTICE);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds the wrong-faction invitation failure packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeInviteWrongFaction(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_INVITE_WRONG_FACTION_NOTICE);
}

/**
 * @brief Builds the wrong-faction notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeWrongFaction(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_WRONG_FACTION_NOTICE);
}

/**
 * @brief Builds the invalid-channel-name notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeInvalidName(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_INVALID_NAME_NOTICE);
}

/**
 * @brief Builds the not-moderated notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeNotModerated(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_NOT_MODERATED_NOTICE);
}

/**
 * @brief Builds the invitation-confirmation packet for the inviter.
 *
 * @param data The packet to fill.
 * @param name The invited player name.
 */
void Channel::MakePlayerInvited(WorldPacket* data, const std::string& name)
{
    MakeNotifyPacket(data, CHAT_PLAYER_INVITED_NOTICE);
    *data << name;
}

/**
 * @brief Builds the invite-banned failure packet.
 *
 * @param data The packet to fill.
 * @param name The banned player name.
 */
void Channel::MakePlayerInviteBanned(WorldPacket* data, const std::string& name)
{
    MakeNotifyPacket(data, CHAT_PLAYER_INVITE_BANNED_NOTICE);
    *data << name;
}

/**
 * @brief Builds the throttled notification packet.
 *
 * @param data The packet to fill.
 */
void Channel::MakeThrottled(WorldPacket* data)
{
    MakeNotifyPacket(data, CHAT_THROTTLED_NOTICE);
}

/**
 * @brief Placeholder join notification hook for client versions that support it.
 *
 * @param guid Unused joining player GUID.
 */
void Channel::JoinNotify(ObjectGuid /*guid*/)
{
    // [-ZERO] Feature doesn't exist in 1.x.
}

/**
 * @brief Placeholder leave notification hook for client versions that support it.
 *
 * @param guid Unused leaving player GUID.
 */
void Channel::LeaveNotify(ObjectGuid /*guid*/)
{
    // [-ZERO] Feature doesn't exist in 1.x.
}
