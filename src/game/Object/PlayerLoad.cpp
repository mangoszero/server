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
#include "CinematicFlyover.h"
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
#include "LFGMgr.h"
#include "DisableMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif /* ENABLE_PLAYERBOTS */

// corpse reclaim times
#define DEATH_EXPIRE_STEP (5*MINUTE)

#define MAX_DEATH_COUNT 3

/**
 * @brief Loads battleground return and participation data from the database.
 *
 * @param result The query result containing battleground data.
 */
void Player::_LoadBGData(QueryResult* result)
{
    if (!result)
    {
        return;
    }

    // Expecting only one row
    Field* fields = result->Fetch();
    /* bgInstanceID, bgTeam, x, y, z, o, map */
    m_bgData.bgInstanceID = fields[0].GetUInt32();
    m_bgData.bgTeam       = Team(fields[1].GetUInt32());
    m_bgData.joinPos      = WorldLocation(fields[6].GetUInt32(),    // Map
        fields[2].GetFloat(),     // X
        fields[3].GetFloat(),     // Y
        fields[4].GetFloat(),     // Z
        fields[5].GetFloat());    // Orientation

    delete result;
}

/**
 * @brief Loads a character position directly from the database.
 *
 * @param guid The player GUID to query.
 * @param mapid Output map identifier.
 * @param x Output X coordinate.
 * @param y Output Y coordinate.
 * @param z Output Z coordinate.
 * @param o Output orientation.
 * @param in_flight Output flag indicating whether the character was in flight.
 * @return True if the position was loaded successfully; otherwise, false.
 */
bool Player::LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight)
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT `position_x`,`position_y`,`position_z`,`orientation`,`map`,`taxi_path` FROM `characters` WHERE `guid` = '%u'", guid.GetCounter());
    if (!result)
    {
        return false;
    }

    Field* fields = result->Fetch();

    x = fields[0].GetFloat();
    y = fields[1].GetFloat();
    z = fields[2].GetFloat();
    o = fields[3].GetFloat();
    mapid = fields[4].GetUInt32();
    in_flight = !fields[5].GetCppString().empty();

    delete result;
    return true;
}

/**
 * @brief Loads serialized uint32 values into the player's update field array.
 *
 * @param data The serialized space-separated field data.
 * @param startOffset The first update-field offset to populate.
 * @param count The number of uint32 values expected.
 */
void Player::_LoadIntoDataField(const char* data, uint32 startOffset, uint32 count)
{
    if (!data)
    {
        return;
    }

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != count)
    {
        return;
    }

    Tokens::iterator iter;
    uint32 index;
    for (iter = tokens.begin(), index = 0; index < count; ++iter, ++index)
    {
        m_uint32Values[startOffset + index] = atol((*iter).c_str());
    }
}

/**
 * @brief Loads the player's persisted state from the database and initializes runtime data.
 *
 * @param guid The player GUID to load.
 * @param holder The query holder containing login query results.
 * @return True if the player was loaded successfully; otherwise, false.
 */
bool Player::LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder)
{
    //        0     1        2     3     4      5       6      7   8      9            10            11
    // SELECT guid, account, name, race, class, gender, level, xp, money, playerBytes, playerBytes2, playerFlags,
    // 12          13          14          15   16           17        18         19         20         21          22           23                 24
    // position_x, position_y, position_z, map, orientation, taximask, cinematic, totaltime, leveltime, rest_bonus, logout_time, is_logout_resting, resettalents_cost,
    // 25                 26       27       28       29       30         31           32            33        34    35      36                 37
    // resettalents_time, trans_x, trans_y, trans_z, trans_o, transguid, extra_flags, stable_slots, at_login, zone, online, death_expire_time, taxi_path,
    // 38                  39              40                   41                        42
    // honor_highest_rank, honor_standing, stored_honor_rating, stored_dishonorablekills, stored_honorable_kills,
    // 43               44
    // watchedFaction,  drunk,
    // 45      46      47      48      49      50      51             52              53      54          55
    // health, power1, power2, power3, power4, power5, exploredZones, equipmentCache, ammoId, actionBars, createdDate  FROM characters WHERE guid = '%u'", GUID_LOPART(m_guid));
    QueryResult* result = holder->GetResult(PLAYER_LOGIN_QUERY_LOADFROM);

    if (!result)
    {
        sLog.outError("%s not found in table `characters`, can't load. ", guid.GetString().c_str());
        return false;
    }

    Field* fields = result->Fetch();

    uint32 dbAccountId = fields[1].GetUInt32();

    // check if the character's account in the db and the logged in account match.
    // player should be able to load/delete character only with correct account!
    if (dbAccountId != GetSession()->GetAccountId())
    {
        sLog.outError("%s loading from wrong account (is: %u, should be: %u)",
            guid.GetString().c_str(), GetSession()->GetAccountId(), dbAccountId);
        delete result;
        return false;
    }

    Object::_Create(guid.GetCounter(), 0, HIGHGUID_PLAYER);

    m_name = fields[2].GetCppString();

    // check name limitations
    if (ObjectMgr::CheckPlayerName(m_name) != CHAR_NAME_SUCCESS ||
        (GetSession()->GetSecurity() == SEC_PLAYER && sObjectMgr.IsReservedName(m_name)))
    {
        delete result;
        CharacterDatabase.PExecute("UPDATE `characters` SET `at_login` = `at_login` | '%u' WHERE `guid` ='%u'",
            uint32(AT_LOGIN_RENAME), guid.GetCounter());
        return false;
    }

    // overwrite possible wrong/corrupted guid
    SetGuidValue(OBJECT_FIELD_GUID, guid);

    // overwrite some data fields
    SetByteValue(UNIT_FIELD_BYTES_0, 0, fields[3].GetUInt8()); // race
    SetByteValue(UNIT_FIELD_BYTES_0, 1, fields[4].GetUInt8()); // class

    uint8 gender = fields[5].GetUInt8() & 0x01;             // allowed only 1 bit values male/female cases (for fit drunk gender part)
    SetByteValue(UNIT_FIELD_BYTES_0, 2, gender);            // gender

    SetByteValue(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_UNK3 | UNIT_BYTE2_FLAG_UNK5);

    // check if race/class combination is valid
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "Player (GUID: %u) has wrong race/class (%u/%u), can't be loaded.",
            guid.GetCounter(), getRace(), getClass());
        return false;
    }

    SetUInt32Value(UNIT_FIELD_LEVEL, fields[6].GetUInt8());
    SetUInt32Value(PLAYER_XP, fields[7].GetUInt32());

    _LoadIntoDataField(fields[51].GetString(), PLAYER_EXPLORED_ZONES_1, PLAYER_EXPLORED_ZONES_SIZE);

    InitDisplayIds();                                       // model, scale and model data

    uint32 money = fields[8].GetUInt32();
    if (money > MAX_MONEY_AMOUNT)
    {
        money = MAX_MONEY_AMOUNT;
    }
    SetMoney(money);

    SetUInt32Value(PLAYER_BYTES, fields[9].GetUInt32());
    SetUInt32Value(PLAYER_BYTES_2, fields[10].GetUInt32());

    m_drunk = fields[44].GetUInt16();

    SetUInt16Value(PLAYER_BYTES_3, 0, (m_drunk & 0xFFFE) | gender);

    SetUInt32Value(PLAYER_FLAGS, fields[11].GetUInt32());
    SetInt32Value(PLAYER_FIELD_WATCHED_FACTION_INDEX, fields[43].GetInt32());

    SetUInt32Value(PLAYER_AMMO_ID, fields[53].GetUInt32());

    // Action bars state
    SetByteValue(PLAYER_FIELD_BYTES, 2, fields[54].GetUInt8());

    // cleanup inventory related item value fields (its will be filled correctly in _LoadInventory)
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        SetGuidValue(PLAYER_FIELD_INV_SLOT_HEAD + (slot * 2), ObjectGuid());
        SetVisibleItemSlot(slot, NULL);

        delete m_items[slot];
        m_items[slot] = NULL;
    }

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "Load Basic value of player %s is: ", m_name.c_str());
    outDebugStatsValues();

    // Need to call it to initialize m_team (m_team can be calculated from race)
    // Other way is to saves m_team into characters table.
    setFactionForRace(getRace());
    SetCharm(NULL);

    // load home bind and check in same time class/race pair, it used later for restore broken positions
    if (!_LoadHomeBind(holder->GetResult(PLAYER_LOGIN_QUERY_LOADHOMEBIND)))
    {
        return false;
    }

    InitPrimaryProfessions();                               // to max set before any spell loaded

    // init saved position, and fix it later if problematic
    uint32 transGUID = fields[30].GetUInt32();
    Relocate(fields[12].GetFloat(), fields[13].GetFloat(), fields[14].GetFloat(), fields[16].GetFloat());
    SetLocationMapId(fields[15].GetUInt32());

    _LoadGroup(holder->GetResult(PLAYER_LOGIN_QUERY_LOADGROUP));

    m_highest_rank.rank        = fields[38].GetUInt32();
    m_highest_rank  = MaNGOS::Honor::CalculateRankInfo(m_highest_rank);
    m_standing_pos             = fields[39].GetUInt32();
    m_stored_honor             = fields[40].GetFloat();
    m_stored_dishonorableKills = fields[41].GetUInt32();
    m_stored_honorableKills    = fields[42].GetUInt32();

    _LoadHonorCP(holder->GetResult(PLAYER_LOGIN_QUERY_LOADHONORCP));

    _LoadBoundInstances(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES));

    if (!IsPositionValid())
    {
        sLog.outError("%s have invalid coordinates (X: %f Y: %f Z: %f O: %f). Teleport to default race/class locations.",
            guid.GetString().c_str(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation());
        RelocateToHomebind();

        transGUID = 0;

        m_movementInfo.ClearTransportData();
    }

    _LoadBGData(holder->GetResult(PLAYER_LOGIN_QUERY_LOADBGDATA));

    if (m_bgData.bgInstanceID)                              // saved in BattleGround
    {
        BattleGround* currentBg = sBattleGroundMgr.GetBattleGround(m_bgData.bgInstanceID, BATTLEGROUND_TYPE_NONE);

        bool player_at_bg = currentBg && currentBg->IsPlayerInBattleGround(GetObjectGuid());

        if (player_at_bg && currentBg->GetStatus() != STATUS_WAIT_LEAVE)
        {
            BattleGroundQueueTypeId bgQueueTypeId = sBattleGroundMgr.BGQueueTypeId(currentBg->GetTypeID());
            AddBattleGroundQueueId(bgQueueTypeId);

            m_bgData.bgTypeID = currentBg->GetTypeID();     // bg data not marked as modified

            // join player to battleground group
            currentBg->EventPlayerLoggedIn(this);
            currentBg->AddOrSetPlayerToCorrectBgGroup(this, GetObjectGuid(), m_bgData.bgTeam);

            SetInviteForBattleGroundQueueType(bgQueueTypeId, currentBg->GetInstanceID());
        }
        else
        {
            // leave bg
            if (player_at_bg)
            {
                currentBg->RemovePlayerAtLeave(GetObjectGuid(), false, true);
            }

            // move to bg enter point
            const WorldLocation& _loc = GetBattleGroundEntryPoint();
            SetLocationMapId(_loc.mapid);
            Relocate(_loc.coord_x, _loc.coord_y, _loc.coord_z, _loc.orientation);

            // We are not in BG anymore
            SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // remove outdated DB data in DB
            _SaveBGData();
        }
    }
    else
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(GetMapId());
        // if server restart after player save in BG or area
        // player can have current coordinates in to BG map, fix this
        if (!mapEntry || mapEntry->IsBattleGround())
        {
            const WorldLocation& _loc = GetBattleGroundEntryPoint();
            SetLocationMapId(_loc.mapid);
            Relocate(_loc.coord_x, _loc.coord_y, _loc.coord_z, _loc.orientation);

            // We are not in BG anymore
            SetBattleGroundId(0, BATTLEGROUND_TYPE_NONE);
            // remove outdated DB data in DB
            _SaveBGData();
        }
    }

    if (transGUID != 0)
    {
        m_movementInfo.SetTransportData(ObjectGuid(HIGHGUID_MO_TRANSPORT, transGUID), fields[26].GetFloat(), fields[27].GetFloat(), fields[28].GetFloat(), fields[29].GetFloat(), 0);

        Position const* transportPosition = m_movementInfo.GetTransportPos();

        if (!MaNGOS::IsValidMapCoord(GetPositionX() + transportPosition->x, GetPositionY() + transportPosition->y,
            GetPositionZ() + transportPosition->z, GetOrientation() + transportPosition->o) ||
            // transport size limited
            transportPosition->x > 50 || transportPosition->y > 50 || transportPosition->z > 50)
        {
            sLog.outError("%s have invalid transport coordinates (X: %f Y: %f Z: %f O: %f). Teleport to default race/class locations.",
                guid.GetString().c_str(), GetPositionX() + transportPosition->x, GetPositionY() + transportPosition->y,
                GetPositionZ() + transportPosition->z, GetOrientation() + transportPosition->o);

            RelocateToHomebind();

            m_movementInfo.ClearTransportData();

            transGUID = 0;
        }
    }

    if (transGUID != 0)
    {
        for (MapManager::TransportSet::const_iterator iter = sMapMgr.m_Transports.begin(); iter != sMapMgr.m_Transports.end(); ++iter)
        {
            if ((*iter)->GetGUIDLow() == transGUID)
            {
                m_transport = *iter;
                m_transport->AddPassenger(this);
                SetLocationMapId(m_transport->GetMapId());
                break;
            }
        }

        if (!m_transport)
        {
            sLog.outError("%s have problems with transport guid (%u). Teleport to default race/class locations.",
                guid.GetString().c_str(), transGUID);

            RelocateToHomebind();

            m_movementInfo.ClearTransportData();

            transGUID = 0;
        }
    }

    // player bounded instance saves loaded in _LoadBoundInstances, group versions at group loading
    DungeonPersistentState* state = GetBoundInstanceSaveForSelfOrGroup(GetMapId());

    // load the player's map here if it's not already loaded
    SetMap(sMapMgr.CreateMap(GetMapId(), this));

    // if the player is in an instance and it has been reset in the meantime teleport him to the entrance
    if (GetInstanceId() && !state)
    {
        AreaTrigger const* at = sObjectMgr.GetMapEntranceTrigger(GetMapId());
        if (at)
        {
            Relocate(at->target_X, at->target_Y, at->target_Z, at->target_Orientation);

            // Update group members' positions in DB so they also appear at the entrance
            if (Group* group = GetGroup())
            {
                uint32 zoneId = sTerrainMgr.GetZoneId(GetMapId(), at->target_X, at->target_Y, at->target_Z);

                std::ostringstream guidList;
                bool first = true;
                for (Group::MemberSlotList::const_iterator itr = group->GetMemberSlots().begin(); itr != group->GetMemberSlots().end(); ++itr)
                {
                    if (itr->guid != GetObjectGuid())
                    {
                        if (!first)
                        {
                            guidList << ',';
                        }
                        guidList << itr->guid.GetCounter();
                        first = false;
                    }
                }

                if (!first)
                {
                    CharacterDatabase.PExecute(
                            "UPDATE `characters` SET `position_x`='%f',`position_y`='%f',`position_z`='%f',"
                            "`orientation`='%f',`map`='%u',`zone`='%u',`trans_x`='0',`trans_y`='0',`trans_z`='0',"
                            "`transguid`='0',`taxi_path`='' WHERE `guid` IN (%s) AND `map`='%u'",
                        at->target_X, at->target_Y, at->target_Z, at->target_Orientation,
                        GetMapId(), zoneId,
                        guidList.str().c_str(), GetMapId());
                }
            }
        }
        else
        {
            sLog.outError("Player %s(GUID: %u) logged in to a reset instance (map: %u) and there is no area-trigger leading to this map. Thus he can't be ported back to the entrance. This _might_ be an exploit attempt.", GetName(), GetGUIDLow(), GetMapId());
        }
    }

    SaveRecallPosition();

    time_t now = time(NULL);
    time_t logoutTime = time_t(fields[22].GetUInt64());

    // since last logout (in seconds)
    uint32 time_diff = uint32(now - logoutTime);

    // set value, including drunk invisibility detection
    // calculate sobering. after 15 minutes logged out, the player will be sober again
    float soberFactor;
    if (time_diff > 15 * MINUTE)
    {
        soberFactor = 0;
    }
    else
    {
        soberFactor = 1 - time_diff / (15.0f * MINUTE);
    }
    uint16 newDrunkenValue = uint16(soberFactor * m_drunk);
    SetDrunkValue(newDrunkenValue);

    m_cinematic = fields[18].GetUInt32();
    m_Played_time[PLAYED_TIME_TOTAL] = fields[19].GetUInt32();
    m_Played_time[PLAYED_TIME_LEVEL] = fields[20].GetUInt32();

    m_resetTalentsCost = fields[24].GetUInt32();
    m_resetTalentsTime = time_t(fields[25].GetUInt64());

    // reserve some flags
    uint32 old_safe_flags = GetUInt32Value(PLAYER_FLAGS) & (PLAYER_FLAGS_HIDE_CLOAK | PLAYER_FLAGS_HIDE_HELM);

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GM))
    {
        SetUInt32Value(PLAYER_FLAGS, 0 | old_safe_flags);
    }

    m_taxi.LoadTaxiMask(fields[17].GetString());            // must be before InitTaxiNodesForLevel

    uint32 extraflags = fields[31].GetUInt32();

    m_petMgr.LoadStableSlotsFromField(fields[32].GetUInt32());

    m_atLoginFlags = fields[33].GetUInt32();

    m_deathExpireTime = (time_t)fields[36].GetUInt64();
    if (m_deathExpireTime > now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP)
    {
        m_deathExpireTime = now + MAX_DEATH_COUNT * DEATH_EXPIRE_STEP - 1;
    }

    std::string taxi_nodes = fields[37].GetCppString();

    // clear channel spell data (if saved at channel spell casting)
    SetChannelObjectGuid(ObjectGuid());
    SetUInt32Value(UNIT_CHANNEL_SPELL, 0);

    // clear charm/summon related fields
    SetCharm(NULL);
    SetPet(NULL);
    SetTargetGuid(ObjectGuid());
    SetCharmerGuid(ObjectGuid());
    SetOwnerGuid(ObjectGuid());
    SetCreatorGuid(ObjectGuid());

    // reset some aura modifiers before aura apply

    SetGuidValue(PLAYER_FARSIGHT, ObjectGuid());
    SetUInt32Value(PLAYER_TRACK_CREATURES, 0);
    SetUInt32Value(PLAYER_TRACK_RESOURCES, 0);

    // cleanup aura list explicitly before skill load where some spells can be applied
    RemoveAllAuras();

    // make sure the unit is considered out of combat for proper loading
    ClearInCombat();

    // make sure the unit is considered not in duel for proper loading
    SetGuidValue(PLAYER_DUEL_ARBITER, ObjectGuid());
    SetUInt32Value(PLAYER_DUEL_TEAM, 0);

    // reset stats before loading any modifiers
    InitStatsForLevel();

    // is it need, only in pre-2.x used and field byte removed later?
    if (GetPowerType() == POWER_RAGE || GetPowerType() == POWER_MANA)
    {
        SetByteValue(UNIT_FIELD_BYTES_1, 1, 0xEE);
    }

    // rest bonus can only be calculated after InitStatsForLevel()
    m_rest_bonus = fields[21].GetFloat();

    if (time_diff > 0)
    {
        SetRestBonus(GetRestBonus() + ComputeRest(time_diff, true, (fields[23].GetInt32() > 0)));
    }

    // load skills after InitStatsForLevel because it triggering aura apply also
    _LoadSkills(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSKILLS));

    // apply original stats mods before spell loading or item equipment that call before equip _RemoveStatsMods()

    // Mail
    _LoadMails(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILS));
    _LoadMailedItems(holder->GetResult(PLAYER_LOGIN_QUERY_LOADMAILEDITEMS));
    UpdateNextMailTimeAndUnreads();

    _LoadAuras(holder->GetResult(PLAYER_LOGIN_QUERY_LOADAURAS), time_diff);

    // add ghost flag (must be after aura load: PLAYER_FLAGS_GHOST set in aura)
    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        m_deathState = DEAD;
    }

    _LoadSpells(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLS));

    // after spell load
    InitTalentForLevel();
    learnDefaultSpells();

    // after spell load, learn rewarded spell if need also
    _LoadQuestStatus(holder->GetResult(PLAYER_LOGIN_QUERY_LOADQUESTSTATUS));

    // must be before inventory (some items required reputation check)
    m_reputationMgr.LoadFromDB(holder->GetResult(PLAYER_LOGIN_QUERY_LOADREPUTATION));

    _LoadInventory(holder->GetResult(PLAYER_LOGIN_QUERY_LOADINVENTORY), time_diff);
    _LoadItemLoot(holder->GetResult(PLAYER_LOGIN_QUERY_LOADITEMLOOT));

    // update items with duration and realtime
    UpdateItemDuration(time_diff, true);

    _LoadActions(holder->GetResult(PLAYER_LOGIN_QUERY_LOADACTIONS));

    m_social = sSocialMgr.LoadFromDB(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSOCIALLIST), GetObjectGuid());

    if (!m_taxi.LoadTaxiDestinationsFromString(taxi_nodes, GetTeam()))
    {
        // problems with taxi path loading
        TaxiNodesEntry const* nodeEntry = NULL;
        if (uint32 node_id = m_taxi.GetTaxiSource())
        {
            nodeEntry = sTaxiNodesStore.LookupEntry(node_id);
        }

        if (!nodeEntry)                                     // don't know taxi start node, to homebind
        {
            sLog.outError("Character %u have wrong data in taxi destination list, teleport to homebind.", GetGUIDLow());
            RelocateToHomebind();
        }
        else                                                // have start node, to it
        {
            sLog.outError("Character %u have too short taxi destination list, teleport to original node.", GetGUIDLow());
            SetLocationMapId(nodeEntry->map_id);
            Relocate(nodeEntry->x, nodeEntry->y, nodeEntry->z, 0.0f);
        }

        // we can be relocated from taxi and still have an outdated Map pointer!
        // so we need to get a new Map pointer!
        SetMap(sMapMgr.CreateMap(GetMapId(), this));
        SaveRecallPosition();                           // save as recall also to prevent recall and fall from sky

        m_taxi.ClearTaxiDestinations();
    }

    if (uint32 node_id = m_taxi.GetTaxiSource())
    {
        // save source node as recall coord to prevent recall and fall from sky
        TaxiNodesEntry const* nodeEntry = sTaxiNodesStore.LookupEntry(node_id);
        MANGOS_ASSERT(nodeEntry);                           // checked in m_taxi.LoadTaxiDestinationsFromString
        m_recallMap = nodeEntry->map_id;
        m_recallX = nodeEntry->x;
        m_recallY = nodeEntry->y;
        m_recallZ = nodeEntry->z;

        // flight will started later
    }

    // has to be called after last Relocate() in Player::LoadFromDB
    SetFallInformation(0, GetPositionZ());

    _LoadSpellCooldowns(holder->GetResult(PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS));

    // Spell code allow apply any auras to dead character in load time in aura/spell/item loading
    // Do now before stats re-calculation cleanup for ghost state unexpected auras
    if (!IsAlive())
    {
        RemoveAllAurasOnDeath();
    }

    // apply all stat bonuses from items and auras
    SetCanModifyStats(true);
    UpdateAllStats();

    // restore remembered power/health values (but not more max values)
    uint32 savedhealth = fields[45].GetUInt32();
    SetHealth(savedhealth > GetMaxHealth() ? GetMaxHealth() : savedhealth);
    for (uint32 i = 0; i < MAX_POWERS; ++i)
    {
        uint32 savedpower = fields[46 + i].GetUInt32();
        SetPower(Powers(i), savedpower > GetMaxPower(Powers(i)) ? GetMaxPower(Powers(i)) : savedpower);
    }

    uint32 createdDate = fields[55].GetUInt32();
    SetCreatedDate(createdDate);

    DEBUG_FILTER_LOG(LOG_FILTER_PLAYER_STATS, "The value of player %s after load item and aura is: ", m_name.c_str());
    outDebugStatsValues();

    // all fields read
    delete result;

    // GM state
    if (GetSession()->GetSecurity() > SEC_PLAYER)
    {
        switch (sWorld.getConfig(CONFIG_UINT32_GM_LOGIN_STATE))
        {
            default:
            case 0:                      break;             // disable
            case 1: SetGameMaster(true); break;             // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_ON)
                {
                    SetGameMaster(true);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_VISIBLE_STATE))
        {
            default:
            case 0: SetGMVisible(false); break;             // invisible
            case 1:                      break;             // visible
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_INVISIBLE)
                {
                    SetGMVisible(false);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_ACCEPT_TICKETS))
        {
            default:
            case 0:                        break;           // disable
            case 1: SetAcceptTicket(true); break;           // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_ACCEPT_TICKETS)
                {
                    SetAcceptTicket(true);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_CHAT))
        {
            default:
            case 0:                  break;                 // disable
            case 1: SetGMChat(true); break;                 // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_GM_CHAT)
                {
                    SetGMChat(true);
                }
                break;
        }

        switch (sWorld.getConfig(CONFIG_UINT32_GM_WISPERING_TO))
        {
            default:
            case 0:                          break;         // disable
            case 1: SetAcceptWhispers(true); break;         // enable
            case 2:                                         // save state
                if (extraflags & PLAYER_EXTRA_ACCEPT_WHISPERS)
                {
                    SetAcceptWhispers(true);
                }
                break;
        }
    }

    return true;
}

/**
 * @brief Checks whether the player is currently allowed to loot a creature.
 *
 * @param creature The creature to test.
 * @return True if the player may loot the creature; otherwise, false.
 */
bool Player::isAllowedToLoot(Creature* creature)
{
    /* Nobody tapped the monster (kill either solo or mostly by another NPC) */
    if (!creature->HasFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_TAPPED) || !creature->IsDamageEnoughForLootingAndReward())
    {
        return false;
    }

    /* If we there is a loot recipient, assign it to recipient */
    if (Player* recipient = creature->GetLootRecipient())
    {
        /* See if we're in a group */
        if (Group* plr_group = recipient->GetGroup())
        {
            /* Recipient is in a group... but is it ours? */
            if (Group* my_group = GetGroup())
            {
                /* Check groups are the same */
                if (plr_group != my_group)
                {
                    return false; // Cheater, deny loot
                }
            }
            else
            {
                return false; // We're not in a group, probably cheater
            }

            /* If the player has joined the group after the creature has been killed, doesn't show up. */
            if (creature->GetKilledTime() < plr_group->GetMemberSlotJoinedTime(GetObjectGuid()))
            {
                return false;
            }

            /* We're in a group, get the loot type */
            switch (plr_group->GetLootMethod())
            {
                /* Free for all or Master Loot let everyone loot it */
                case MASTER_LOOT:
                case FREE_FOR_ALL:
                    return true;

                /** These 3 systems all use the same kind of check to display loot,
                 *  which is what we're doing here. Threshold checks are done elsewhere. */
                case GROUP_LOOT:
                case ROUND_ROBIN:
                case NEED_BEFORE_GREED:
                {
                    uint32 loot_id = creature->GetCreatureInfo()->LootId;

                    /* Checking there's some loot defined. */
                    bool hasLoot = loot_id;
                    /* Checking if the creature may drop any quest loot for us. */
                    bool hasSharedLoot = LootTemplates_Creature.HaveSharedQuestLootForPlayer(loot_id, this);
                    /* Checking if there's any starting quest item available for this player. */
                    bool hasStartingQuestLoot = LootTemplates_Creature.HaveStartingQuestLootForPlayer(loot_id,  this);

                    /* If there's no loot, we return false. */
                    if (!hasLoot)
                    {
                        return false;
                    }
                    /** This is set to true after the looter (chosen below) has closed their loot window
                     * If this is true, allow everyone else in the group to loot the corpse */
                    else if (creature->hasBeenLootedOnce)
                    {
                        return true;
                    }
                    /* If the assigned looter's GUID is equal to ours */
                    else if (creature->assignedLooter == GetGUIDLow())
                    {
                        return true;
                    }
                    /* If the creature already has an assigned looter and that looter isn't us */
                    else if (creature->assignedLooter != 0 && !hasSharedLoot && !hasStartingQuestLoot)
                    {
                        return false;
                    }

                    /* If we've reached here, there is only one exclusive, undecided looter */

                    /* This is the player that will be given permission to loot */
                    Player* final_looter = recipient;

                    /* Iterate through the valid party members */
                    Group::MemberSlotList slots = plr_group->GetMemberSlots();

                    for (Group::MemberSlotList::iterator itr = slots.begin(); itr != slots.end(); ++itr)
                    {
                        /* Get the player data */
                        if (Player* grp_plr = sObjectMgr.GetPlayer(itr->guid))
                        {
                            /* Player is disconnected */
                            if (!grp_plr->IsInWorld())
                            {
                                continue;
                            }

                            /* Player is too far from the creature. */
                            if (!grp_plr->IsWithinDist(creature, sWorld.getConfig(CONFIG_FLOAT_GROUP_XP_DISTANCE), false))
                            {
                                continue;
                            }

                            /** Check if the last time the player looted is less than the current final looter
                             * If the value is lower, it means it happened longer ago */
                            if (final_looter->lastTimeLooted > grp_plr->lastTimeLooted)
                            {
                                final_looter = grp_plr;
                            }
                        }
                    }

                    /* We have our looter, update their loot time */
                    final_looter->lastTimeLooted = time(NULL);

                    /* Update the creature with the looter that has been assigned to them */
                    creature->assignedLooter = final_looter->GetGUIDLow();
                    final_looter->GetGroup()->SetLooterGuid(final_looter->GetObjectGuid());

                    /* Finally, return if we are the assigned looter */
                    return (final_looter->GetGUIDLow() == GetGUIDLow() || hasSharedLoot || hasStartingQuestLoot);
                }
                default:
                    // Something went wrong, avoid crash
                    return false;
            }
        }
        /* We're not in a group, check to make sure we're the recipient (prevent cheaters) */
        else if (recipient == this)
        {
            return true;
        }
    }
    else
        // prevent other players from looting if the recipient got disconnected
    {
        return !creature->HasLootRecipient();
    }

    return false;
}

/**
 * @brief Loads action bar bindings from the database.
 *
 * @param result The query result containing action bindings.
 */
void Player::_LoadActions(QueryResult* result)
{
    m_actionButtons.clear();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT `button`,`action`,`type` FROM `character_action` WHERE `guid` = '%u' ORDER BY `button`",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint8 button = fields[0].GetUInt8();
            uint32 action = fields[1].GetUInt32();
            uint8 type = fields[2].GetUInt8();

            if (ActionButton* ab = addActionButton(button, action, type))
            {
                ab->uState = ACTIONBUTTON_UNCHANGED;
            }
            else
            {
                sLog.outError("  ...at loading, and will deleted in DB also");

                // Will deleted in DB at next save (it can create data until save but marked as deleted)
                m_actionButtons[button].uState = ACTIONBUTTON_DELETED;
            }
        }
        while (result->NextRow());

        delete result;
    }
}

/**
 * @brief Loads saved auras from the database and restores their remaining durations.
 *
 * @param result The query result containing aura data.
 * @param timediff The elapsed offline time used to age aura durations.
 */
void Player::_LoadAuras(QueryResult* result, uint32 timediff)
{
    // RemoveAllAuras(); -- some spells casted before aura load, for example in LoadSkills, aura list explicitly cleaned early

    // all aura related fields
    for (int i = UNIT_FIELD_AURA; i <= UNIT_FIELD_AURASTATE; ++i)
    {
        SetUInt32Value(i, 0);
    }

    // QueryResult *result = CharacterDatabase.PQuery("SELECT `caster_guid`,`item_guid`,`spell`,`stackcount`,`remaincharges`,`basepoints0`,`basepoints1`,`basepoints2`,`periodictime0`,`periodictime1`,`periodictime2`,`maxduration`,`remaintime`,`effIndexMask` FROM `character_aura` WHERE `guid` = '%u'",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            ObjectGuid caster_guid = ObjectGuid(fields[0].GetUInt64());
            uint32 item_lowguid = fields[1].GetUInt32();
            uint32 spellid = fields[2].GetUInt32();
            uint32 stackcount = fields[3].GetUInt32();
            uint32 remaincharges = fields[4].GetUInt32();
            int32  damage[MAX_EFFECT_INDEX];
            uint32 periodicTime[MAX_EFFECT_INDEX];

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                damage[i] = fields[i + 5].GetInt32();
                periodicTime[i] = fields[i + 8].GetUInt32();
            }

            int32 maxduration = fields[11].GetInt32();
            int32 remaintime = fields[12].GetInt32();
            uint32 effIndexMask = fields[13].GetUInt32();

            SpellEntry const* spellproto = sSpellStore.LookupEntry(spellid);
            if (!spellproto)
            {
                sLog.outError("Unknown spell (spellid %u), ignore.", spellid);
                continue;
            }

            if (remaintime != -1 && !IsPositiveSpell(spellproto))
            {
                if (remaintime / IN_MILLISECONDS <= int32(timediff))
                {
                    continue;
                }

                remaintime -= timediff * IN_MILLISECONDS;
            }

            // prevent wrong values of remaincharges
            if (spellproto->ProcCharges == 0)
            {
                remaincharges = 0;
            }

            if (!spellproto->CumulativeAura)
            {
                stackcount = 1;
            }
            else if (spellproto->CumulativeAura < stackcount)
            {
                stackcount = spellproto->CumulativeAura;
            }
            else if (!stackcount)
            {
                stackcount = 1;
            }

            SpellAuraHolder* holder = CreateSpellAuraHolder(spellproto, this, NULL);
            holder->SetLoadedState(caster_guid, ObjectGuid(HIGHGUID_ITEM, item_lowguid), stackcount, remaincharges, maxduration, remaintime);

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if ((effIndexMask & (1 << i)) == 0)
                {
                    continue;
                }

                Aura* aura = CreateAura(spellproto, SpellEffectIndex(i), NULL, holder, this);
                if (!damage[i])
                {
                    damage[i] = aura->GetModifier()->m_amount;
                }

                aura->SetLoadedState(damage[i], periodicTime[i]);
                holder->AddAura(aura, SpellEffectIndex(i));
            }

            if (!holder->IsEmptyHolder())
            {
                // reset stolen single target auras
                if (caster_guid != GetObjectGuid() && holder->GetTrackedAuraType() == TRACK_AURA_TYPE_SINGLE_TARGET)
                {
                    holder->SetTrackedAuraType(TRACK_AURA_TYPE_NOT_TRACKED);
                }

                AddSpellAuraHolder(holder);
                DETAIL_LOG("Added auras from spellid %u", spellproto->ID);
            }
            else
            {
                delete holder;
            }
        }
        while (result->NextRow());
        delete result;
    }

    if (getClass() == CLASS_WARRIOR && !HasAuraType(SPELL_AURA_MOD_SHAPESHIFT))
    {
        CastSpell(this, SPELL_ID_PASSIVE_BATTLE_STANCE, true);
    }
}

/**
 * @brief Restores corpse state for a dead player or cleans it up for a living one.
 */
void Player::LoadCorpse()
{
    if (IsAlive())
    {
        sObjectAccessor.ConvertCorpseForPlayer(GetObjectGuid());
    }
    else
    {
        if (Corpse* corpse = GetCorpse())
        {
            ApplyModByteFlag(PLAYER_FIELD_BYTES, 0, PLAYER_FIELD_BYTE_RELEASE_TIMER, corpse && !sMapStore.LookupEntry(corpse->GetMapId())->Instanceable());
        }
        else
        {
            // Prevent Dead Player login without corpse
            ResurrectPlayer(0.5f);
        }
    }
}

/**
 * @brief Loads inventory, bank, and equipped items from the database.
 *
 * @param result The query result containing item inventory rows.
 * @param timediff The elapsed offline time used for time-sensitive item checks.
 */
void Player::_LoadInventory(QueryResult* result, uint32 timediff)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT `data`,`bag`,`slot`,`item`,`item_template` FROM `character_inventory` JOIN `item_instance` ON `character_inventory`.`item` = `item_instance`.`guid` WHERE `character_inventory`.`guid` = '%u' ORDER BY `bag`,`slot`", GetGUIDLow());
    std::map<uint32, Bag*> bagMap;                          // fast guid lookup for bags
    // NOTE: the "order by `bag`" is important because it makes sure
    // the bagMap is filled before items in the bags are loaded
    // NOTE2: the "order by `slot`" is needed because mainhand weapons are (wrongly?)
    // expected to be equipped before offhand items (TODO: fixme)

    uint32 zone = GetZoneId();

    if (result)
    {
        std::list<Item*> problematicItems;

        // prevent items from being added to the queue when stored
        m_itemUpdateQueueBlocked = true;
        do
        {
            Field* fields = result->Fetch();
            uint32 bag_guid  = fields[1].GetUInt32();
            uint8  slot      = fields[2].GetUInt8();
            uint32 item_lowguid = fields[3].GetUInt32();
            uint32 item_id   = fields[4].GetUInt32();

            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_id);

            if (!proto)
            {
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", item_lowguid);
                sLog.outError("Player::_LoadInventory: Player %s has an unknown item (id: #%u) in inventory, deleted.", GetName(), item_id);
                continue;
            }

            Item* item = NewItemOrBag(proto);

            if (!item->LoadFromDB(item_lowguid, fields, GetObjectGuid()))
            {
                sLog.outError("Player::_LoadInventory: Player %s has broken item (id: #%u) in inventory, deleted.", GetName(), item_id);
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object !
                continue;
            }

            // not allow have in alive state item limited to another map/zone
            if (IsAlive() && item->IsLimitedToAnotherMapOrZone(GetMapId(), zone))
            {
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object !
                continue;
            }

            // "Conjured items disappear if you are logged out for more than 15 minutes"
            if (timediff > 15 * MINUTE && (item->GetProto()->Flags & ITEM_FLAG_CONJURED))
            {
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                item->FSetState(ITEM_REMOVED);
                item->SaveToDB();                           // it also deletes item object !
                continue;
            }

            bool success = true;

            // the item/bag is not in a bag
            if (!bag_guid)
            {
                item->SetContainer(NULL);
                item->SetSlot(slot);

                if (IsInventoryPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (CanStoreItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false) == EQUIP_ERR_OK)
                    {
                        item = StoreItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (IsEquipmentPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    uint16 dest;
                    if (CanEquipItem(slot, dest, item, false, false) == EQUIP_ERR_OK)
                    {
                        QuickEquipItem(dest, item);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else if (IsBankPos(INVENTORY_SLOT_BAG_0, slot))
                {
                    ItemPosCountVec dest;
                    if (CanBankItem(INVENTORY_SLOT_BAG_0, slot, dest, item, false, false) == EQUIP_ERR_OK)
                    {
                        item = BankItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }

                if (success)
                {
                    // store bags that may contain items in them
                    if (item->IsBag() && IsBagPos(item->GetPos()))
                    {
                        bagMap[item_lowguid] = (Bag*)item;
                    }
                }
            }
            // the item/bag in a bag
            else
            {
                item->SetSlot(NULL_SLOT);
                // the item is in a bag, find the bag
                std::map<uint32, Bag*>::const_iterator itr = bagMap.find(bag_guid);
                if (itr != bagMap.end() && slot < itr->second->GetBagSize())
                {
                    ItemPosCountVec dest;
                    if (CanStoreItem(itr->second->GetSlot(), slot, dest, item, false) == EQUIP_ERR_OK)
                    {
                        item = StoreItem(dest, item, true);
                    }
                    else
                    {
                        success = false;
                    }
                }
                else
                {
                    success = false;
                }
            }

            // item's state may have changed after stored
            if (success)
            {
                item->SetState(ITEM_UNCHANGED, this);

                // restore container unchanged state also
                if (item->GetContainer())
                {
                    item->GetContainer()->SetState(ITEM_UNCHANGED, this);
                }
            }
            else
            {
                sLog.outError("Player::_LoadInventory: Player %s has item (GUID: %u Entry: %u) can't be loaded to inventory (Bag GUID: %u Slot: %u) by some reason, will send by mail.", GetName(), item_lowguid, item_id, bag_guid, slot);
                CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` = '%u'", item_lowguid);
                problematicItems.push_back(item);
            }
        }
        while (result->NextRow());

        delete result;
        m_itemUpdateQueueBlocked = false;

        // send by mail problematic items
        while (!problematicItems.empty())
        {
            std::string subject = "Item could not be loaded to inventory.";
            std::string content = GetSession()->GetMangosString(LANG_NOT_EQUIPPED_ITEM);
            // fill mail
            MailDraft draft(subject,"");
            draft.SetSubjectAndBody(subject,content);
            for (int i = 0; !problematicItems.empty() && i < MAX_MAIL_ITEMS; ++i)
            {
                Item* item = problematicItems.front();
                problematicItems.pop_front();

                draft.AddItem(item);
            }

            draft.SendMailTo(this, MailSender(this, MAIL_STATIONERY_GM), MAIL_CHECK_MASK_COPIED);
        }
    }

    // if (IsAlive())
    _ApplyAllItemMods();
}

/**
 * @brief Loads persisted item loot contents for the player's items.
 *
 * @param result The query result containing saved item loot rows.
 */
void Player::_LoadItemLoot(QueryResult* result)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT `guid`,`itemid`,`amount`,`property` FROM `item_loot` WHERE `guid` = '%u'", GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            uint32 item_guid   = fields[0].GetUInt32();

            Item* item = GetItemByGuid(ObjectGuid(HIGHGUID_ITEM, item_guid));

            if (!item)
            {
                CharacterDatabase.PExecute("DELETE FROM `item_loot` WHERE `guid` = '%u'", item_guid);
                sLog.outError("Player::_LoadItemLoot: Player %s has loot for nonexistent item (GUID: %u) in `item_loot`, deleted.", GetName(), item_guid);
                continue;
            }

            item->LoadLootFromDB(fields);
        }
        while (result->NextRow());

        delete result;
    }
}

// load mailed item which should receive current player
void Player::_LoadMailedItems(QueryResult* result)
{
    // data needs to be at first place for Item::LoadFromDB
    //         0     1     2        3          4
    // "SELECT data, text, mail_id, item_guid, item_template FROM mail_items JOIN item_instance ON item_guid = guid WHERE receiver = '%u'", GUID_LOPART(m_guid)
    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        uint32 mail_id       = fields[1].GetUInt32();
        uint32 item_guid_low = fields[2].GetUInt32();
        uint32 item_template = fields[3].GetUInt32();

        Mail* mail = GetMail(mail_id);
        if (!mail)
        {
            continue;
        }
        mail->AddItem(item_guid_low, item_template);

        ItemPrototype const* proto = ObjectMgr::GetItemPrototype(item_template);

        if (!proto)
        {
            sLog.outError("Player %u has unknown item_template (ProtoType) in mailed items(GUID: %u template: %u) in mail (%u), deleted.", GetGUIDLow(), item_guid_low, item_template, mail->messageID);
            CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` = '%u'", item_guid_low);
            CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", item_guid_low);
            continue;
        }

        Item* item = NewItemOrBag(proto);

        if (!item->LoadFromDB(item_guid_low, fields, GetObjectGuid()))
        {
            sLog.outError("Player::_LoadMailedItems - Item in mail (%u) doesn't exist !!!! - item guid: %u, deleted from mail", mail->messageID, item_guid_low);
            CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` = '%u'", item_guid_low);
            item->FSetState(ITEM_REMOVED);
            item->SaveToDB();                               // it also deletes item object !
            continue;
        }

        AddMItem(item);
    }
    while (result->NextRow());

    delete result;
}

/**
 * @brief Loads the player's mailbox headers from the database.
 *
 * @param result The query result containing mail records.
 */
void Player::_LoadMails(QueryResult* result)
{
    m_mail.clear();
    //        0  1           2      3        4       5    6           7            8     9   10      11         12             13
    //"SELECT id,messageType,sender,receiver,subject,body,expire_time,deliver_time,money,cod,checked,stationery,mailTemplateId,has_items FROM mail WHERE receiver = '%u' ORDER BY id DESC", GetGUIDLow()
    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        Mail* m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        m->subject = fields[4].GetCppString();
        m->body = fields[5].GetCppString();
        m->expire_time = (time_t)fields[6].GetUInt64();
        m->deliver_time = (time_t)fields[7].GetUInt64();
        m->money = fields[8].GetUInt32();
        m->COD = fields[9].GetUInt32();
        m->checked = fields[10].GetUInt32();
        m->stationery = fields[11].GetUInt8();
        m->mailTemplateId = fields[12].GetInt16();
        m->has_items = fields[13].GetBool();                // true, if mail have items or mail have template and items generated (maybe none)

        if (m->mailTemplateId && !sMailTemplateStore.LookupEntry(m->mailTemplateId))
        {
            sLog.outError("Player::_LoadMail - Mail (%u) have nonexistent MailTemplateId (%u), remove at load", m->messageID, m->mailTemplateId);
            m->mailTemplateId = 0;
        }

        m->state = MAIL_STATE_UNCHANGED;

        m_mail.push_back(m);

        if (m->mailTemplateId && !m->has_items)
        {
            m->prepareTemplateItems(this);
        }
    }
    while (result->NextRow());
    delete result;
}

/**
 * @brief Loads the player's currently active pet from the database when possible.
 */
void Player::LoadPet()
{
    // fixme: the pet should still be loaded if the player is not in world
    // just not added to the map
    if (IsInWorld())
    {
        Pet* pet = new Pet;
        if (!pet->LoadPetFromDB(this, 0, 0, true))
        {
            delete pet;
        }
    }
}

/**
 * @brief Loads quest status records and rebuilds the in-memory quest log state.
 *
 * @param result The query result containing quest status rows.
 */
void Player::_LoadQuestStatus(QueryResult* result)
{
    mQuestStatus.clear();

    uint32 slot = 0;

    ////                                                       0        1         2           3           4        5            6            7            8            9             10            11            12
    // QueryResult *result = CharacterDatabase.PQuery("SELECT `quest`, `status`, `rewarded`, `explored`, `timer`, `mobcount1`, `mobcount2`, `mobcount3`, `mobcount4`, `itemcount1`, `itemcount2`, `itemcount3`, `itemcount4` FROM `character_queststatus` WHERE `guid` = '%u'", GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 quest_id = fields[0].GetUInt32();
            // used to be new, no delete?
            Quest const* pQuest = sObjectMgr.GetQuestTemplate(quest_id);
            if (pQuest)
            {
                // find or create
                QuestStatusData& questStatusData = mQuestStatus[quest_id];

                uint32 qstatus = fields[1].GetUInt32();
                if (qstatus < MAX_QUEST_STATUS)
                {
                    questStatusData.m_status = QuestStatus(qstatus);
                }
                else
                {
                    questStatusData.m_status = QUEST_STATUS_NONE;
                    sLog.outError("Player %s have invalid quest %d status (%d), replaced by QUEST_STATUS_NONE(0).", GetName(), quest_id, qstatus);
                }

                questStatusData.m_rewarded = (fields[2].GetUInt8() > 0);
                questStatusData.m_explored = (fields[3].GetUInt8() > 0);

                time_t quest_time = time_t(fields[4].GetUInt64());

                if (pQuest->HasSpecialFlag(QUEST_SPECIAL_FLAG_TIMED) && !GetQuestRewardStatus(quest_id) && questStatusData.m_status != QUEST_STATUS_NONE)
                {
                    AddTimedQuest(quest_id);

                    if (quest_time <= sWorld.GetGameTime())
                    {
                        questStatusData.m_timer = 1;
                    }
                    else
                    {
                        questStatusData.m_timer = uint32(quest_time - sWorld.GetGameTime()) * IN_MILLISECONDS;
                    }
                }
                else
                {
                    quest_time = 0;
                }

                questStatusData.m_creatureOrGOcount[0] = fields[5].GetUInt32();
                questStatusData.m_creatureOrGOcount[1] = fields[6].GetUInt32();
                questStatusData.m_creatureOrGOcount[2] = fields[7].GetUInt32();
                questStatusData.m_creatureOrGOcount[3] = fields[8].GetUInt32();
                questStatusData.m_itemcount[0] = fields[9].GetUInt32();
                questStatusData.m_itemcount[1] = fields[10].GetUInt32();
                questStatusData.m_itemcount[2] = fields[11].GetUInt32();
                questStatusData.m_itemcount[3] = fields[12].GetUInt32();

                questStatusData.uState = QUEST_UNCHANGED;

                // add to quest log
                if (slot < MAX_QUEST_LOG_SIZE &&
                    ((questStatusData.m_status == QUEST_STATUS_INCOMPLETE ||
                    questStatusData.m_status == QUEST_STATUS_COMPLETE ||
                    questStatusData.m_status == QUEST_STATUS_FAILED) &&
                    (!questStatusData.m_rewarded || pQuest->IsRepeatable())))
                {
                    SetQuestSlot(slot, quest_id, uint32(quest_time));

                    if (questStatusData.m_explored)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    }

                    if (questStatusData.m_status == QUEST_STATUS_COMPLETE)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_COMPLETE);
                    }

                    if (questStatusData.m_status == QUEST_STATUS_FAILED)
                    {
                        SetQuestSlotState(slot, QUEST_STATE_FAIL);
                    }

                    for (uint8 idx = 0; idx < QUEST_OBJECTIVES_COUNT; ++idx)
                    {
                        if (questStatusData.m_creatureOrGOcount[idx])
                        {
                            SetQuestSlotCounter(slot, idx, questStatusData.m_creatureOrGOcount[idx]);
                        }
                    }
                    ++slot;
                }

                if (questStatusData.m_rewarded)
                {
                    // learn rewarded spell if unknown
                    learnQuestRewardedSpells(pQuest);
                }

                DEBUG_LOG("Quest status is {%u} for quest {%u} for player (GUID: %u)", questStatusData.m_status, quest_id, GetGUIDLow());
            }
        }
        while (result->NextRow());

        delete result;
    }

    // clear quest log tail
    for (uint16 i = slot; i < MAX_QUEST_LOG_SIZE; ++i)
    {
        SetQuestSlot(i, 0);
    }
}

/**
 * @brief Loads known spells from the database.
 *
 * @param result The query result containing learned spell rows.
 */
void Player::_LoadSpells(QueryResult* result)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT `spell`,`active`,`disabled` FROM `character_spell` WHERE `guid` = '%u'",GetGUIDLow());

    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 spell_id = fields[0].GetUInt32();

            addSpell(spell_id, fields[1].GetBool(), false, false, fields[2].GetBool());
        }
        while (result->NextRow());

        delete result;
    }
}

/**
 * @brief Loads the player's current group membership from the database.
 *
 * @param result The query result containing the group identifier.
 */
void Player::_LoadGroup(QueryResult* result)
{
    // QueryResult *result = CharacterDatabase.PQuery("SELECT `groupId` FROM `group_member` WHERE `memberGuid`='%u'", GetGUIDLow());
    if (result)
    {
        uint32 groupId = (*result)[0].GetUInt32();
        delete result;

        if (Group* group = sObjectMgr.GetGroupById(groupId))
        {
            uint8 subgroup = group->GetMemberGroup(GetObjectGuid());
            SetGroup(group, subgroup);
        }
    }
}

/**
 * @brief Loads the player's saved dungeon and raid instance bindings.
 *
 * @param result The query result containing character instance bind rows.
 */
void Player::_LoadBoundInstances(QueryResult* result)
{
    m_boundInstances.clear();

    Group* group = GetGroup();

    // QueryResult *result = CharacterDatabase.PQuery("SELECT `id`, `permanent`, `map`, `resettime` FROM `character_instance` LEFT JOIN `instance` ON `instance` = `id` WHERE `guid` = '%u'", GUID_LOPART(m_guid));
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();
            bool perm = fields[1].GetBool();
            uint32 mapId = fields[2].GetUInt32();
            uint32 instanceId = fields[0].GetUInt32();
            time_t resetTime = (time_t)fields[3].GetUInt64();
            // the resettime for normal instances is only saved when the InstanceSave is unloaded
            // so the value read from the DB may be wrong here but only if the InstanceSave is loaded
            // and in that case it is not used

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry || !mapEntry->IsDungeon())
            {
                sLog.outError("_LoadBoundInstances: player %s(%d) has bind to nonexistent or not dungeon map %d", GetName(), GetGUIDLow(), mapId);
                CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'", GetGUIDLow(), instanceId);
                continue;
            }

            if (!perm && group)
            {
                sLog.outError("_LoadBoundInstances: %s is in group (Id: %d) but has a non-permanent character bind to map %d,%d",
                    GetGuidStr().c_str(), group->GetId(), mapId, instanceId);
                CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'",
                    GetGUIDLow(), instanceId);
                continue;
            }

            // since non permanent binds are always solo bind, they can always be reset
            DungeonPersistentState* state = (DungeonPersistentState*)sMapPersistentStateMgr.AddPersistentState(mapEntry, instanceId, resetTime, !perm, true);
            if (state)
            {
                BindToInstance(state, perm, true);
            }
        }
        while (result->NextRow());
        delete result;
    }
}

/**
 * @brief Finds the player's instance bind for a specific map.
 *
 * @param mapid The map identifier to look up.
 * @return The matching bind entry, or NULL if none exists.
 */
InstancePlayerBind* Player::GetBoundInstance(uint32 mapid)
{

    BoundInstancesMap::iterator itr = m_boundInstances.find(mapid);
    if (itr != m_boundInstances.end())
    {
        return &itr->second;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Removes the player's instance bind for a map.
 *
 * @param mapid The bound map identifier.
 * @param unload True when called during load or unload paths that should skip DB deletion.
 */
void Player::UnbindInstance(uint32 mapid, bool unload)
{
    BoundInstancesMap::iterator itr = m_boundInstances.find(mapid);
    UnbindInstance(itr, unload);
}

/**
 * @brief Removes the instance bind referenced by an iterator.
 *
 * @param itr Iterator pointing at the bind to remove.
 * @param unload True when called during load or unload paths that should skip DB deletion.
 */
void Player::UnbindInstance(BoundInstancesMap::iterator& itr, bool unload)
{
    if (itr != m_boundInstances.end())
    {
        if (!unload)
        {
            CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `instance` = '%u'",
                GetGUIDLow(), itr->second.state->GetInstanceId());
        }
        itr->second.state->RemovePlayer(this);              // state can become invalid
        m_boundInstances.erase(itr++);
    }
}

/**
 * @brief Binds the player to a dungeon persistent state.
 *
 * @param state The persistent instance state to bind.
 * @param permanent True for a permanent lock, false for a temporary one.
 * @param load True when rebuilding the bind from saved data.
 * @return The resulting bind entry, or NULL if the state was invalid.
 */
InstancePlayerBind* Player::BindToInstance(DungeonPersistentState* state, bool permanent, bool load)
{
    if (state)
    {
        InstancePlayerBind& bind = m_boundInstances[state->GetMapId()];
        if (bind.state)
        {
            // update the state when the group kills a boss
            if (permanent != bind.perm || state != bind.state)
            {
                if (!load)
                {
                    CharacterDatabase.PExecute("UPDATE `character_instance` SET `instance` = '%u', `permanent` = '%u' WHERE `guid` = '%u' AND `instance` = '%u'",
                        state->GetInstanceId(), permanent, GetGUIDLow(), bind.state->GetInstanceId());
                }
            }
        }
        else
        {
            if (!load)
            {
                CharacterDatabase.PExecute("INSERT INTO `character_instance` (`guid`, `instance`, `permanent`) VALUES ('%u', '%u', '%u')",
                    GetGUIDLow(), state->GetInstanceId(), permanent);
            }
        }

        if (bind.state != state)
        {
            if (bind.state)
            {
                bind.state->RemovePlayer(this);
            }
            state->AddPlayer(this);
        }

        if (permanent)
        {
            state->SetCanReset(false);
        }

        bind.state = state;
        bind.perm = permanent;
        if (!load)
        {
            DEBUG_LOG("Player::BindToInstance: %s(%d) is now bound to map %d, instance %d",
                GetName(), GetGUIDLow(), state->GetMapId(), state->GetInstanceId());
        }
        // Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = GetEluna())
        {
            e->OnBindToInstance(this, (Difficulty)0, state->GetMapId(), permanent);
        }
#endif /* ENABLE_ELUNA */

        return &bind;
    }
    else
    {
        return NULL;
    }
}

/**
 * @brief Resolves the instance save that applies to the player or the player's group.
 *
 * @param mapid The map identifier to inspect.
 * @return The applicable persistent state, or NULL if none exists.
 */
DungeonPersistentState* Player::GetBoundInstanceSaveForSelfOrGroup(uint32 mapid)
{
    MapEntry const* mapEntry = sMapStore.LookupEntry(mapid);
    if (!mapEntry)
    {
        return NULL;
    }

    InstancePlayerBind* pBind = GetBoundInstance(mapid);
    DungeonPersistentState* state = pBind ? pBind->state : NULL;

    // the player's permanent player bind is taken into consideration first
    // then the player's group bind and finally the solo bind.
    if (!pBind || !pBind->perm)
    {
        InstanceGroupBind* groupBind = NULL;
        if (Group* group = GetGroup())
        {
            if ((groupBind = group->GetBoundInstance(mapid)))
            {
                state = groupBind->state;
            }
        }
    }

    return state;
}

/**
 * @brief Sends the player's permanent raid instance lock information to the client.
 */
void Player::SendRaidInfo()
{
    uint32 counter = 0;

    WorldPacket data(SMSG_RAID_INSTANCE_INFO, 4);

    size_t p_counter = data.wpos();
    data << uint32(counter);                                // placeholder

    for (BoundInstancesMap::const_iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end(); ++itr)
    {
        if (itr->second.perm)
        {
            DungeonPersistentState* state = itr->second.state;
            data << uint32(state->GetMapId());              // map id
            data << uint32(state->GetResetTime() - time(NULL));
            data << uint32(state->GetInstanceId());         // instance id
            counter++;
        }
    }

    data.put<uint32>(p_counter, counter);
    GetSession()->SendPacket(&data);
}

/**
 - called on every successful teleportation to a map
 */
void Player::SendSavedInstances()
{
    bool hasBeenSaved = false;
    WorldPacket data;

    for (BoundInstancesMap::const_iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end(); ++itr)
    {
        if (itr->second.perm)                               // only permanent binds are sent
        {
            hasBeenSaved = true;
            break;
        }
    }

    // Send opcode 811. true or false means, whether you have current raid instances
    data.Initialize(SMSG_UPDATE_INSTANCE_OWNERSHIP, 4);
    data << uint32(hasBeenSaved);
    GetSession()->SendPacket(&data);

    if (!hasBeenSaved)
    {
        return;
    }

    for (BoundInstancesMap::const_iterator itr = m_boundInstances.begin(); itr != m_boundInstances.end(); ++itr)
    {
        if (itr->second.perm)
        {
            data.Initialize(SMSG_UPDATE_LAST_INSTANCE, 4);
            data << uint32(itr->second.state->GetMapId());
            GetSession()->SendPacket(&data);
        }
    }
}

/// convert the player's binds to the group
void Player::ConvertInstancesToGroup(Player* player, Group* group, ObjectGuid player_guid)
{
    bool has_binds = false;
    bool has_solo = false;

    if (player)
    {
        player_guid = player->GetObjectGuid();
        if (!group)
        {
            group = player->GetGroup();
        }
    }

    MANGOS_ASSERT(player_guid);

    // copy all binds to the group, when changing leader it's assumed the character
    // will not have any solo binds

    if (player)
    {
        for (BoundInstancesMap::iterator itr = player->m_boundInstances.begin(); itr != player->m_boundInstances.end();)
        {
            has_binds = true;

            if (group)
            {
                group->BindToInstance(itr->second.state, itr->second.perm, true);
            }

            // permanent binds are not removed
            if (!itr->second.perm)
            {
                // increments itr in call
                player->UnbindInstance(itr, true);
                has_solo = true;
            }
            else
            {
                ++itr;
            }
        }
    }

    uint32 player_lowguid = player_guid.GetCounter();

    // if the player's not online we don't know what binds it has
    if (!player || !group || has_binds)
    {
        CharacterDatabase.PExecute("INSERT INTO `group_instance` SELECT `guid`, `instance`, `permanent` FROM `character_instance` WHERE `guid` = '%u'", player_lowguid);
    }

    // the following should not get executed when changing leaders
    if (!player || has_solo)
    {
        CharacterDatabase.PExecute("DELETE FROM `character_instance` WHERE `guid` = '%u' AND `permanent` = 0", player_lowguid);
    }
}

/**
 * @brief Loads and validates the player's home bind location.
 *
 * @param result The query result containing home bind data.
 * @return True if a valid home bind was loaded or defaulted successfully; otherwise, false.
 */
bool Player::_LoadHomeBind(QueryResult* result)
{
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(getRace(), getClass());
    if (!info)
    {
        sLog.outError("Player have incorrect race/class pair. Can't be loaded.");
        return false;
    }

    bool ok = false;
    // QueryResult *result = CharacterDatabase.PQuery("SELECT `map`,`zone`,`position_x`,`position_y`,`position_z` FROM `character_homebind` WHERE `guid` = '%u'", GUID_LOPART(playerGuid));
    if (result)
    {
        Field* fields = result->Fetch();
        m_homebindMapId = fields[0].GetUInt32();
        m_homebindAreaId = fields[1].GetUInt16();
        m_homebindX = fields[2].GetFloat();
        m_homebindY = fields[3].GetFloat();
        m_homebindZ = fields[4].GetFloat();
        delete result;

        MapEntry const* bindMapEntry = sMapStore.LookupEntry(m_homebindMapId);

        // accept saved data only for valid position (and non instanceable), and accessable
        if (MapManager::IsValidMapCoord(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ) &&
            !bindMapEntry->Instanceable())
        {
            ok = true;
        }
        else
        {
            CharacterDatabase.PExecute("DELETE FROM `character_homebind` WHERE `guid` = '%u'", GetGUIDLow());
        }
    }

    if (!ok)
    {
        m_homebindMapId = info->mapId;
        m_homebindAreaId = info->areaId;
        m_homebindX = info->positionX;
        m_homebindY = info->positionY;
        m_homebindZ = info->positionZ;

        CharacterDatabase.PExecute("INSERT INTO `character_homebind` (`guid`,`map`,`zone`,`position_x`,`position_y`,`position_z`) VALUES ('%u', '%u', '%u', '%f', '%f', '%f')", GetGUIDLow(), m_homebindMapId, (uint32)m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ);
    }

    DEBUG_LOG("Setting player home position: mapid is: %u, zoneid is %u, X is %f, Y is %f, Z is %f",
        m_homebindMapId, m_homebindAreaId, m_homebindX, m_homebindY, m_homebindZ);

    return true;
}
