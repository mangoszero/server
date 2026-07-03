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



#include "Player.h"
#include "Language.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Opcodes.h"
#include "SpellMgr.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "UpdateMask.h"
#include "QuestDef.h"
#include "GossipDef.h"
#include "UpdateData.h"
#include "Channel.h"
#include "ChannelMgr.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CreatureAI.h"
#include "Formulas.h"
#include "Group.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Pet.h"
#include "Util.h"
#include "Transports.h"
#include "Weather.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundMgr.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Chat.h"
#include "revision_data.h"
#include "Database/DatabaseImpl.h"
#include "Spell.h"
#include "ScriptMgr.h"
#include "SocialMgr.h"
#include "Mail.h"
#include "SpellAuras.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "DisableMgr.h"
#include "CinematicFlyover.h"
#include <cmath>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

/**
 * @brief Registers a joined chat channel on the player.
 *
 * @param c The channel that was joined.
 */
void Player::JoinedChannel(Channel* c)
{
    m_channels.push_back(c);
}

/**
 * @brief Removes a chat channel from the player's joined channel list.
 *
 * @param c The channel that was left.
 */
void Player::LeftChannel(Channel* c)
{
    m_channels.remove(c);
}

/**
 * @brief Leaves and cleans up all channels currently joined by the player.
 */
void Player::CleanupChannels()
{
    while (!m_channels.empty())
    {
        Channel* ch = *m_channels.begin();
        m_channels.erase(m_channels.begin());               // remove from player's channel list
        ch->Leave(this, false);                             // not send to client, not remove from player's channel list
        if (ChannelMgr* cMgr = channelMgr(GetTeam()))
        {
            cMgr->LeftChannel(ch->GetName()); // deleted channel if empty
        }
    }
    DEBUG_LOG("Player: channels cleaned up!");
}

/**
 * @brief Updates built-in local channels after a zone change.
 *
 * @param newZone The new area identifier used for localized channel names.
 */
void Player::UpdateLocalChannels(uint32 newZone)
{
    if (m_channels.empty())
    {
        return;
    }

    AreaTableEntry const* current_zone = GetAreaEntryByAreaID(newZone);
    if (!current_zone)
    {
        return;
    }

    ChannelMgr* cMgr = channelMgr(GetTeam());
    if (!cMgr)
    {
        return;
    }

    std::string current_zone_name = current_zone->AreaName_lang[GetSession()->GetSessionDbcLocale()];

    for (JoinedChannelsList::iterator i = m_channels.begin(), next; i != m_channels.end(); i = next)
    {
        next = i; ++next;

        // skip non built-in channels
        if (!(*i)->IsConstant())
        {
            continue;
        }

        ChatChannelsEntry const* ch = GetChannelEntryFor((*i)->GetChannelId());
        if (!ch)
        {
            continue;
        }

        if ((ch->Flags & 4) == 4)                           // global channel without zone name in pattern
        {
            continue;
        }

        //  new channel
        char new_channel_name_buf[100];
        snprintf(new_channel_name_buf, 100, ch->Name_lang[m_session->GetSessionDbcLocale()], current_zone_name.c_str());
        Channel* new_channel = cMgr->GetJoinChannel(new_channel_name_buf);

        if ((*i) != new_channel)
        {
            new_channel->Join(this, "");                    // will output Changed Channel: N. Name

            // leave old channel
            (*i)->Leave(this, false);                       // not send leave channel, it already replaced at client
            std::string name = (*i)->GetName();             // store name, (*i)erase in LeftChannel
            LeftChannel(*i);                                // remove from player's channel list
            cMgr->LeftChannel(name);                        // delete if empty
        }
    }
    DEBUG_LOG("Player: channels cleaned up!");
}

/**
 * @brief Leaves the currently joined looking-for-group channel, if any.
 */
void Player::LeaveLFGChannel()
{
    for (JoinedChannelsList::iterator i = m_channels.begin(); i != m_channels.end(); ++i)
    {
        if ((*i)->IsLFG())
        {
            (*i)->Leave(this);
            break;
        }
    }
}
