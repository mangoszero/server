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



#include "Guild.h"
#include "GuildMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "Opcodes.h"
#include "ObjectMgr.h"
#include "Chat.h"
#include "SocialMgr.h"
#include "Util.h"
#include "Language.h"
#include "World.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

/**
 * @brief Creates and persists a new guild rank.
 *
 * @param name_ The rank name.
 * @param rights The rights mask for the new rank.
 */
void Guild::CreateRank(std::string name_, uint32 rights)
{
    if (m_Ranks.size() >= GUILD_RANKS_MAX_COUNT)
    {
        return;
    }

    // ranks are sequence 0,1,2,... where 0 means guildmaster
    uint32 new_rank_id = m_Ranks.size();

    AddRank(name_, rights);

    // name now can be used for encoding to DB
    CharacterDatabase.escape_string(name_);
    CharacterDatabase.PExecute("INSERT INTO `guild_rank` (`guildid`,`rid`,`rname`,`rights`) VALUES ('%u', '%u', '%s', '%u')", m_Id, new_rank_id, name_.c_str(), rights);
}

/**
 * @brief Adds a rank to the in-memory rank list.
 *
 * @param name_ The rank name.
 * @param rights The rights mask.
 */
void Guild::AddRank(const std::string& name_, uint32 rights)
{
    m_Ranks.push_back(RankInfo(name_, rights));
}

/**
 * @brief Deletes the lowest guild rank if allowed.
 */
void Guild::DelRank()
{
    // client won't allow to have less than GUILD_RANKS_MIN_COUNT ranks in guild
    if (m_Ranks.size() <= GUILD_RANKS_MIN_COUNT)
    {
        return;
    }

    // delete lowest guild_rank
    uint32 rank = GetLowestRank();
    CharacterDatabase.PExecute("DELETE FROM `guild_rank` WHERE `rid`>='%u' AND `guildid`='%u'", rank, m_Id);

    m_Ranks.pop_back();
}

/**
 * @brief Gets the name of a guild rank.
 *
 * @param rankId The rank identifier.
 * @return The rank name, or a placeholder if the rank is invalid.
 */
std::string Guild::GetRankName(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return "<unknown>";
    }

    return m_Ranks[rankId].Name;
}

/**
 * @brief Gets the rights mask for a guild rank.
 *
 * @param rankId The rank identifier.
 * @return The rights mask for the rank.
 */
uint32 Guild::GetRankRights(uint32 rankId)
{
    if (rankId >= m_Ranks.size())
    {
        return 0;
    }

    return m_Ranks[rankId].Rights;
}

/**
 * @brief Renames a guild rank.
 *
 * @param rankId The rank identifier.
 * @param name_ The new rank name.
 */
void Guild::SetRankName(uint32 rankId, std::string name_)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    m_Ranks[rankId].Name = name_;

    // name now can be used for encoding to DB
    CharacterDatabase.escape_string(name_);
    CharacterDatabase.PExecute("UPDATE `guild_rank` SET `rname`='%s' WHERE `rid`='%u' AND `guildid`='%u'", name_.c_str(), rankId, m_Id);
}

/**
 * @brief Updates the rights mask for a guild rank.
 *
 * @param rankId The rank identifier.
 * @param rights The new rights mask.
 */
void Guild::SetRankRights(uint32 rankId, uint32 rights)
{
    if (rankId >= m_Ranks.size())
    {
        return;
    }

    m_Ranks[rankId].Rights = rights;

    CharacterDatabase.PExecute("UPDATE `guild_rank` SET `rights`='%u' WHERE `rid`='%u' AND `guildid`='%u'", rights, rankId, m_Id);
}
