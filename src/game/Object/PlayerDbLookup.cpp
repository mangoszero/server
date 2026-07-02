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
 * @brief Loads a player's guild identifier from the database.
 *
 * @param guid The player GUID to query.
 * @return The guild identifier, or zero if none is found.
 */
uint32 Player::GetGuildIdFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `guildid` FROM `guild_member` WHERE `guid`='%u'", lowguid);
    if (!result)
    {
        return 0;
    }

    uint32 id = result->Fetch()[0].GetUInt32();
    delete result;
    return id;
}

/**
 * @brief Loads a player's guild rank from the database.
 *
 * @param guid The player GUID to query.
 * @return The guild rank, or zero if none is found.
 */
uint32 Player::GetRankFromDB(ObjectGuid guid)
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT `rank` FROM `guild_member` WHERE `guid`='%u'", guid.GetCounter());
    if (result)
    {
        uint32 v = result->Fetch()[0].GetUInt32();
        delete result;
        return v;
    }
    else
    {
        return 0;
    }
}

/**
 * @brief Loads or derives a player's saved zone identifier from the database.
 *
 * @param guid The player GUID to query.
 * @return The resolved zone identifier, or zero on failure.
 */
uint32 Player::GetZoneIdFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();
    QueryResult* result = CharacterDatabase.PQuery("SELECT `zone` FROM `characters` WHERE `guid`='%u'", lowguid);
    if (!result)
    {
        return 0;
    }
    Field* fields = result->Fetch();
    uint32 zone = fields[0].GetUInt32();
    delete result;

    if (!zone)
    {
        // stored zone is zero, use generic and slow zone detection
        result = CharacterDatabase.PQuery("SELECT `map`,`position_x`,`position_y`,`position_z` FROM `characters` WHERE `guid`='%u'", lowguid);
        if (!result)
        {
            return 0;
        }
        fields = result->Fetch();
        uint32 map = fields[0].GetUInt32();
        float posx = fields[1].GetFloat();
        float posy = fields[2].GetFloat();
        float posz = fields[3].GetFloat();
        delete result;

        zone = sTerrainMgr.GetZoneId(map, posx, posy, posz);

        if (zone > 0)
        {
            CharacterDatabase.PExecute("UPDATE `characters` SET `zone`='%u' WHERE `guid`='%u'", zone, lowguid);
        }
    }

    return zone;
}

/**
 * @brief Loads a player's level from the database.
 *
 * @param guid The player GUID to query.
 * @return The stored level, or zero on failure.
 */
uint32 Player::GetLevelFromDB(ObjectGuid guid)
{
    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `level` FROM `characters` WHERE `guid`='%u'", lowguid);
    if (!result)
    {
        return 0;
    }

    Field* fields = result->Fetch();
    uint32 level = fields[0].GetUInt32();
    delete result;

    return level;
}
