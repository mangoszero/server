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
 * @file ChannelMgr.cpp
 * @brief Chat channel management system
 *
 * This file implements the ChannelMgr which manages custom chat channels
 * (like "General", "Trade", or player-created channels). Features:
 * - Separate channel managers per faction (Alliance/Horde)
 * - Cross-faction channel support via configuration
 * - Channel creation and lookup by name
 * - Case-insensitive channel name handling
 *
 * Channel names are normalized to lowercase for consistent lookup.
 *
 * @see Channel for individual channel management
 * @see ChannelMgr for the manager interface
 */

#include "ChannelMgr.h"
#include "Policies/Singleton.h"
#include "World.h"

INSTANTIATE_SINGLETON_1(AllianceChannelMgr);
INSTANTIATE_SINGLETON_1(HordeChannelMgr);

/**
 * @brief Get the appropriate channel manager for a team
 * @param team Player's faction (ALLIANCE or HORDE)
 * @return Channel manager for that faction, or NULL for invalid team
 *
 * Returns the channel manager instance for the specified faction.
 * If cross-faction channels are enabled in configuration, all
 * teams use the Alliance channel manager.
 *
 * @note Player-created channels are faction-specific by default
 */
ChannelMgr* channelMgr(Team team)
{
    if (sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_CHANNEL))
    {
        return &MaNGOS::Singleton<AllianceChannelMgr>::Instance();         // cross-faction
    }

    if (team == ALLIANCE)
    {
        return &MaNGOS::Singleton<AllianceChannelMgr>::Instance();
    }
    if (team == HORDE)
    {
        return &MaNGOS::Singleton<HordeChannelMgr>::Instance();
    }

    return NULL;
}

/**
 * @brief Destroy channel manager
 *
 * Cleans up all channels managed by this instance.
 * Each Channel object is deleted and the map is cleared.
 *
 * @note Called on server shutdown
 */
ChannelMgr::~ChannelMgr()
{
    for (ChannelMap::iterator itr = channels.begin(); itr != channels.end(); ++itr)
    {
        delete itr->second;
    }

    channels.clear();
}

/**
 * @brief Get or create a channel for joining
 * @param name Channel name (case-insensitive)
 * @return Channel instance (existing or newly created)
 *
 * Looks up a channel by name and creates it if it doesn't exist.
 * This is used when a player attempts to join a channel.
 *
 * Channel names are normalized to lowercase for consistent lookup.
 *
 * @note Creates new Channel object if not found
 */
Channel* ChannelMgr::GetJoinChannel(const std::string &name)
{
    std::wstring wname;
    Utf8toWStr(name, wname);
    wstrToLower(wname);

    if (channels.find(wname) == channels.end())
    {
        Channel* nchan = new Channel(name);
        channels[wname] = nchan;
        return nchan;
    }

    return channels[wname];
}

/**
 * @brief Get an existing channel
 * @param name Channel name (case-insensitive)
 * @param p Player requesting the channel (for error packet)
 * @param pkt If true, send "not on channel" error packet when channel not found
 * @return Channel instance, or NULL if not found
 *
 * Looks up a channel by name. Unlike GetJoinChannel(), this does NOT
 * create the channel if it doesn't exist.
 *
 * @param pkt controls whether an error packet is sent to the player
 * @return NULL if channel doesn't exist, otherwise the Channel pointer
 */
Channel* ChannelMgr::GetChannel(const std::string &name, Player* p, bool pkt)
{
    std::wstring wname;
    Utf8toWStr(name, wname);
    wstrToLower(wname);

    ChannelMap::const_iterator i = channels.find(wname);

    if (i == channels.end())
    {
        if (pkt)
        {
            WorldPacket data;
            Channel::MakeNotOnPacket(&data, name);
            p->GetSession()->SendPacket(&data);
        }

        return NULL;
    }
    else
    {
        return i->second;
    }
}

/**
 * @brief Removes an empty non-constant channel after a player leaves it.
 *
 * @param name The channel name.
 */
void ChannelMgr::LeftChannel(const std::string &name)
{
    std::wstring wname;
    Utf8toWStr(name, wname);
    wstrToLower(wname);

    ChannelMap::const_iterator i = channels.find(wname);

    if (i == channels.end())
    {
        return;
    }

    Channel* channel = i->second;

    if (channel->GetNumPlayers() == 0 && !channel->IsConstant())
    {
        channels.erase(wname);
        delete channel;
    }
}
