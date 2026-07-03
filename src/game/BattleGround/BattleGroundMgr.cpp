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
 * @file BattleGroundMgr.cpp
 * @brief Implementation of the battleground manager and queue system.
 *
 * This file contains the implementation of the BattleGroundMgr singleton class and
 * the BattleGroundQueue class, which handle:
 * - Battleground instance creation and management
 * - Player queue management and matching
 * - Team balancing for battleground invitations
 * - Average wait time calculations
 * - Bracket-based queue organization
 * - Premade group matching
 */

#include "Common.h"
#include "SharedDefines.h"
#include "Player.h"
#include "BattleGroundMgr.h"
#include "BattleGroundAV.h"
#include "BattleGroundAB.h"
#include "BattleGroundWS.h"
#include "MapManager.h"
#include "Map.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "Chat.h"
#include "World.h"
#include "WorldPacket.h"
#include "Language.h"
#include "GameEventMgr.h"
#include "DisableMgr.h"
#include "GameTime.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#include "Policies/Singleton.h"

INSTANTIATE_SINGLETON_1(BattleGroundMgr);

/*********************************************************/
/***            BATTLEGROUND QUEUE SYSTEM              ***/
/*********************************************************/

/**
 * @brief Constructor for BattleGroundQueue.
 *
 * Initializes the queue system by zeroing out all wait time tracking arrays
 * for each team and bracket combination.
 */
BattleGroundQueue::BattleGroundQueue()
{
    for (uint8 i = 0; i < PVP_TEAM_COUNT; ++i)
    {
        for (uint8 j = 0; j < MAX_BATTLEGROUND_BRACKETS; ++j)
        {
            m_SumOfWaitTimes[i][j] = 0;
            m_WaitTimeLastPlayer[i][j] = 0;
            for (uint8 k = 0; k < COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME; ++k)
            {
                m_WaitTimes[i][j][k] = 0;
            }
        }
    }
}

/**
 * @brief Destructor for BattleGroundQueue.
 *
 * Cleans up all queued players and group information, deallocating memory
 * for all group queue info structures across all brackets and queue types.
 */
BattleGroundQueue::~BattleGroundQueue()
{
    m_QueuedPlayers.clear();
    for (uint8 i = 0; i < MAX_BATTLEGROUND_BRACKETS; ++i)
    {
        for (uint8 j = 0; j < BG_QUEUE_GROUP_TYPES_COUNT; ++j)
        {
            for (GroupsQueueType::iterator itr = m_QueuedGroups[i][j].begin(); itr != m_QueuedGroups[i][j].end(); ++itr)
            {
                delete(*itr);
            }
            m_QueuedGroups[i][j].clear();
        }
    }
}

/*********************************************************/
/***      BATTLEGROUND QUEUE SELECTION POOLS           ***/
/*********************************************************/




/*********************************************************/
/***               BATTLEGROUND QUEUES                 ***/
/*********************************************************/












/*********************************************************/
/***            BATTLEGROUND QUEUE EVENTS              ***/
/*********************************************************/



/**
 * This event has many possibilities when it is executed:
 * 1. player is in battleground ( he clicked enter on invitation window )
 * 2. player left battleground queue and he isn't there any more
 * 3. player left battleground queue and he joined it again and IsInvitedToBGInstanceGUID = 0
 * 4. player left queue and he joined again and he has been invited to same battleground again -> we should not remove him from queue yet
 * 5. player is invited to bg and he didn't choose what to do and timer expired - only in this condition we should call queue::RemovePlayer
 * We must remove player in the 5. case even if battleground object doesn't exist!
 */



/*********************************************************/
/***            BATTLEGROUND MANAGER                   ***/
/*********************************************************/

/**
 * @brief Constructor for BattleGroundMgr.
 *
 * Initializes all battleground containers and sets testing mode to false.
 */
BattleGroundMgr::BattleGroundMgr()
{
    for (uint8 i = BATTLEGROUND_TYPE_NONE; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
    {
        m_BattleGrounds[i].clear();
    }
    m_Testing = false;
}

/**
 * @brief Destructor for BattleGroundMgr.
 *
 * Cleans up all active and template battlegrounds.
 */
BattleGroundMgr::~BattleGroundMgr()
{
    DeleteAllBattleGrounds();
}

/**
 * @brief Deletes all battleground instances.
 *
 * Safely removes all active battlegrounds and template battlegrounds from memory.
 * This includes template battlegrounds that are only used as templates for creating instances.
 */
void BattleGroundMgr::DeleteAllBattleGrounds()
{
    // will also delete template bgs:
    for (uint8 i = BATTLEGROUND_TYPE_NONE; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
    {
        for (BattleGroundSet::iterator itr = m_BattleGrounds[i].begin(); itr != m_BattleGrounds[i].end();)
        {
            BattleGround* bg = itr->second;
            ++itr;                                          // step from invalidate iterator pos in result element remove in ~BattleGround call
            delete bg;
        }
    }
}

/**
 * @brief Updates all active battlegrounds and processes queue operations.
 *
 * Performs the main update loop for all active battleground instances, processes
 * scheduled queue updates based on the update scheduler, and removes finished
 * battlegrounds from memory. Called once per world tick.
 *
 * @param diff The time elapsed since the last update in milliseconds (unused).
 */
void BattleGroundMgr::Update(uint32 /*diff*/)
{
    // update scheduled queues
    if (!m_QueueUpdateScheduler.empty())
    {
        std::vector<uint32> scheduled;
        {
            // create mutex
            // ACE_Guard<ACE_Thread_Mutex> guard(SchedulerLock);
            // copy vector and clear the other
            scheduled = std::vector<uint32>(m_QueueUpdateScheduler);
            m_QueueUpdateScheduler.clear();
            // release lock
        }

        for (uint8 i = 0; i < scheduled.size(); ++i)
        {
            BattleGroundQueueTypeId bgQueueTypeId = BattleGroundQueueTypeId(scheduled[i] >> 16 & 255);
            BattleGroundTypeId bgTypeId = BattleGroundTypeId((scheduled[i] >> 8) & 255);
            BattleGroundBracketId bracket_id = BattleGroundBracketId(scheduled[i] & 255);
            m_BattleGroundQueues[bgQueueTypeId].Update(bgTypeId, bracket_id);
        }
    }
}

/**
 * @brief Builds a battlefield status packet for sending to the player.
 *
 * Constructs the network packet for SMSG_BATTLEFIELD_STATUS that informs the player
 * of their queue status, position, estimated wait time, and other relevant information.
 * Handles different status types: waiting in queue, invited to join, and in progress.
 *
 * @param data Pointer to the WorldPacket to write data to.
 * @param bg Pointer to the battleground (may be NULL for status clear).
 * @param QueueSlot The queue slot index (0-2, player can be in multiple queues).
 * @param StatusID The status identifier (0=clear, STATUS_WAIT_QUEUE, STATUS_WAIT_JOIN, STATUS_IN_PROGRESS).
 * @param Time1 Status-specific time value (wait time, invitation timeout, or auto-leave time).
 * @param Time2 Secondary time value (queue time or elapsed battle time).
 */
void BattleGroundMgr::BuildBattleGroundStatusPacket(WorldPacket* data, BattleGround* bg, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2)
{
    // we can be in 3 queues in same time...

    if (StatusID == 0 || !bg)
    {
        data->Initialize(SMSG_BATTLEFIELD_STATUS, 4 * 2);
        *data << uint32(QueueSlot);                         // queue id (0...2)
        *data << uint32(0);
        return;
    }

    data->Initialize(SMSG_BATTLEFIELD_STATUS, (4 + 8 + 4 + 1 + 4 + 4 + 4));
    *data << uint32(QueueSlot);                             // queue id (0...2) - player can be in 3 queues in time
    *data << uint32(bg->GetMapId());                        // MapID
    *data << uint8(0);                                      // Unknown
    *data << uint32(bg->GetClientInstanceID());
    *data << uint32(StatusID);                              // status
    switch (StatusID)
    {
        case STATUS_WAIT_QUEUE:                             // status_in_queue
            *data << uint32(Time1);                         // average wait time, milliseconds
            *data << uint32(Time2);                         // time in queue, updated every minute!, milliseconds
            break;
        case STATUS_WAIT_JOIN:                              // status_invite
            *data << uint32(Time1);                         // time to remove from queue, milliseconds
            break;
        case STATUS_IN_PROGRESS:                            // status_in_progress
            *data << uint32(Time1);                         // time to bg auto leave, 0 at bg start, 120000 after bg end, milliseconds
            *data << uint32(Time2);                         // time from bg start, milliseconds
            break;
        default:
            sLog.outError("Unknown BG status!");
            break;
    }
}

/**
 * @brief Builds a PvP log data packet with player statistics.
 *
 * Constructs the network packet for MSG_PVP_LOG_DATA that contains the battleground
 * statistics for all players, including scores, kills, deaths, and battleground-specific
 * objective data. Indicates whether the battleground has finished.
 *
 * @param data Pointer to the WorldPacket to write data to.
 * @param bg Pointer to the battleground instance.
 */
void BattleGroundMgr::BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg)
{
    data->Initialize(MSG_PVP_LOG_DATA, (1 + 4 + 40 * bg->GetPlayerScoresSize()));

    if (bg->GetStatus() != STATUS_WAIT_LEAVE)
    {
        *data << uint8(0);                                  // bg not ended
    }
    else
    {
        *data << uint8(1);                                  // bg ended
        *data << uint8(bg->GetWinner());                    // who win
    }

    *data << (uint32)(bg->GetPlayerScoresSize());

    for (BattleGround::BattleGroundScoreMap::const_iterator itr = bg->GetPlayerScoresBegin(); itr != bg->GetPlayerScoresEnd(); ++itr)
    {
        const BattleGroundScore* score = itr->second;

        *data << ObjectGuid(itr->first);

        Player* plr = sObjectMgr.GetPlayer(itr->first);

        *data << uint32(plr ? plr->GetHonorRankInfo().visualRank : 0);
        *data << uint32(itr->second->KillingBlows);
        *data << uint32(itr->second->HonorableKills);
        *data << uint32(itr->second->Deaths);
        *data << uint32(itr->second->BonusHonor);

        switch (bg->GetTypeID())                            // battleground specific things
        {
            case BATTLEGROUND_AV:
                *data << (uint32)0x00000007;                // count of next fields
                *data << (uint32)((BattleGroundAVScore*)score)->GraveyardsAssaulted;  // GraveyardsAssaulted
                *data << (uint32)((BattleGroundAVScore*)score)->GraveyardsDefended;   // GraveyardsDefended
                *data << (uint32)((BattleGroundAVScore*)score)->TowersAssaulted;      // TowersAssaulted
                *data << (uint32)((BattleGroundAVScore*)score)->TowersDefended;       // TowersDefended
                *data << (uint32)((BattleGroundAVScore*)score)->SecondaryObjectives;  // Mines Taken
                *data << (uint32)((BattleGroundAVScore*)score)->LieutnantCount;       // Lieutnant kills
                *data << (uint32)((BattleGroundAVScore*)score)->SecondaryNPC;         // Secondary unit summons
                break;
            case BATTLEGROUND_WS:
                *data << (uint32)0x00000002;                // count of next fields
                *data << (uint32)((BattleGroundWGScore*)score)->FlagCaptures;         // flag captures
                *data << (uint32)((BattleGroundWGScore*)score)->FlagReturns;          // flag returns
                break;
            case BATTLEGROUND_AB:
                *data << (uint32)0x00000002;                // count of next fields
                *data << (uint32)((BattleGroundABScore*)score)->BasesAssaulted;       // bases asssulted
                *data << (uint32)((BattleGroundABScore*)score)->BasesDefended;        // bases defended
                break;
            default:
                DEBUG_LOG("Unhandled MSG_PVP_LOG_DATA for BG id %u", bg->GetTypeID());
                *data << (uint32)0;
                break;
        }
    }
}

/**
 * @brief Builds the group battleground join result packet.
 *
 * Writes the battleground join status code returned to grouped players after a
 * join request is processed.
 *
 * @param data Pointer to the packet being filled.
 * @param status The battleground group join status code.
 */
void BattleGroundMgr::BuildGroupJoinedBattlegroundPacket(WorldPacket* data, int32 status)
{
    data->Initialize(SMSG_GROUP_JOINED_BATTLEGROUND, 4);
    // for status, see enum BattleGroundGroupJoinStatus
    *data << int32(status);
}

/**
 * @brief Builds a world state update packet.
 *
 * Populates a packet with a world state field identifier and its new value so
 * clients can refresh battleground UI state.
 *
 * @param data Pointer to the packet being filled.
 * @param field The world state field identifier.
 * @param value The value to assign to the field.
 */
void BattleGroundMgr::BuildUpdateWorldStatePacket(WorldPacket* data, uint32 field, uint32 value)
{
    data->Initialize(SMSG_UPDATE_WORLD_STATE, 4 + 4);
    *data << uint32(field);
    *data << uint32(value);
}

/**
 * @brief Builds a packet to play a sound effect.
 *
 * Constructs the SMSG_PLAY_SOUND packet that instructs clients to play a specific sound.
 *
 * @param data Pointer to the WorldPacket to write data to.
 * @param soundid The sound ID to play.
 */
void BattleGroundMgr::BuildPlaySoundPacket(WorldPacket* data, uint32 soundid)
{
    data->Initialize(SMSG_PLAY_SOUND, 4);
    *data << uint32(soundid);
}

/**
 * @brief Builds a packet for when a player leaves a battleground.
 *
 * Constructs the SMSG_BATTLEGROUND_PLAYER_LEFT packet that notifies other players
 * about a player leaving the battleground.
 *
 * @param data Pointer to the WorldPacket to write data to.
 * @param guid The GUID of the player who left.
 */
void BattleGroundMgr::BuildPlayerLeftBattleGroundPacket(WorldPacket* data, ObjectGuid guid)
{
    data->Initialize(SMSG_BATTLEGROUND_PLAYER_LEFT, 8);
    *data << ObjectGuid(guid);
}

/**
 * @brief Builds a packet for when a player joins a battleground.
 *
 * Constructs the SMSG_BATTLEGROUND_PLAYER_JOINED packet that notifies other players
 * about a new player joining the battleground.
 *
 * @param data Pointer to the WorldPacket to write data to.
 * @param plr Pointer to the player who joined.
 */
void BattleGroundMgr::BuildPlayerJoinedBattleGroundPacket(WorldPacket* data, Player* plr)
{
    data->Initialize(SMSG_BATTLEGROUND_PLAYER_JOINED, 8);
    *data << plr->GetObjectGuid();
}

/**
 * @brief Retrieves a battleground instance by client instance ID.
 *
 * Searches for a battleground instance using the client-side instance ID that was
 * sent in the SMSG_BATTLEFIELD_LIST packet. This is used when a player joins from the UI.
 *
 * @param instanceId The client-side instance ID.
 * @param bgTypeId The battleground type to search in.
 * @return Pointer to the battleground instance, or NULL if not found.
 */
BattleGround* BattleGroundMgr::GetBattleGroundThroughClientInstance(uint32 instanceId, BattleGroundTypeId bgTypeId)
{
    // cause at HandleBattleGroundJoinOpcode the clients sends the instanceid he gets from
    // SMSG_BATTLEFIELD_LIST we need to find the battleground with this clientinstance-id
    BattleGround* bg = GetBattleGroundTemplate(bgTypeId);
    if (!bg)
    {
        return NULL;
    }

    for (BattleGroundSet::iterator itr = m_BattleGrounds[bgTypeId].begin(); itr != m_BattleGrounds[bgTypeId].end(); ++itr)
    {
        if (itr->second->GetClientInstanceID() == instanceId)
        {
            return itr->second;
        }
    }
    return NULL;
}

/**
 * @brief Retrieves a battleground instance by instance ID.
 *
 * Searches for an active battleground instance by its server instance ID. If bgTypeId
 * is BATTLEGROUND_TYPE_NONE, searches across all battleground types.
 *
 * @param InstanceID The server instance ID.
 * @param bgTypeId The battleground type to search in, or BATTLEGROUND_TYPE_NONE for all types.
 * @return Pointer to the battleground instance, or NULL if not found.
 */
BattleGround* BattleGroundMgr::GetBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId)
{
    // search if needed
    BattleGroundSet::iterator itr;
    if (bgTypeId == BATTLEGROUND_TYPE_NONE)
    {
        for (uint8 i = BATTLEGROUND_AV; i < MAX_BATTLEGROUND_TYPE_ID; ++i)
        {
            itr = m_BattleGrounds[i].find(InstanceID);
            if (itr != m_BattleGrounds[i].end())
            {
                return itr->second;
            }
        }
        return NULL;
    }
    itr = m_BattleGrounds[bgTypeId].find(InstanceID);
    return ((itr != m_BattleGrounds[bgTypeId].end()) ? itr->second : NULL);
}

/**
 * @brief Retrieves the template battleground for a given type.
 *
 * Returns the template battleground for the specified type. The template is the lowest-ID
 * battleground in the container and is used as a reference for creating new instances.
 *
 * @param bgTypeId The battleground type.
 * @return Pointer to the template battleground, or NULL if none exists.
 */
BattleGround* BattleGroundMgr::GetBattleGroundTemplate(BattleGroundTypeId bgTypeId)
{
    // map is sorted and we can be sure that lowest instance id has only BG template
    return m_BattleGrounds[bgTypeId].empty() ? NULL : m_BattleGrounds[bgTypeId].begin()->second;
}

/**
 * @brief Creates a unique client-visible instance ID for a battleground.
 *
 * Generates a new unique client-facing instance ID for the specified battleground type and bracket.
 * Client IDs are sequential starting from 1, filling any gaps in the ID sequence. These IDs are
 * sent to clients in the battleground list packet and used when players join via the UI.
 *
 * @param bgTypeId The battleground type.
 * @param bracket_id The bracket level.
 * @return A unique client-visible instance ID.
 */
uint32 BattleGroundMgr::CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{
    // we create here an instanceid, which is just for
    // displaying this to the client and without any other use..
    // the client-instanceIds are unique for each battleground-type
    // the instance-id just needs to be as low as possible, beginning with 1
    // the following works, because std::set is default ordered with "<"
    // the optimalization would be to use as bitmask std::vector<uint32> - but that would only make code unreadable
    uint32 lastId = 0;
    ClientBattleGroundIdSet& ids = m_ClientBattleGroundIds[bgTypeId][bracket_id];
    for (ClientBattleGroundIdSet::const_iterator itr = ids.begin(); itr != ids.end();)
    {
        if ((++lastId) != *itr)                             // if there is a gap between the ids, we will break..
        {
            break;
        }
        lastId = *itr;
    }
    ids.insert(lastId + 1);
    return lastId + 1;
}

/**
 * @brief Creates a new battleground instance.
 *
 * Creates a new playable battleground instance by copying the template and initializing
 * it with a new instance ID, bracket ID, and game map. The new battleground is placed in
 * queue waiting for players to join.
 *
 * @param bgTypeId The type of battleground to create.
 * @param bracket_id The bracket the battleground belongs to.
 * @return Pointer to the newly created battleground, or NULL if creation failed.
 */
BattleGround* BattleGroundMgr::CreateNewBattleGround(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id)
{
    // get the template BG
    BattleGround* bg_template = GetBattleGroundTemplate(bgTypeId);
    if (!bg_template)
    {
        sLog.outError("BattleGround: CreateNewBattleGround - bg template not found for %u", bgTypeId);
        return NULL;
    }

    BattleGround* bg = NULL;
    // create a copy of the BG template
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV:
            bg = new BattleGroundAV(*(BattleGroundAV*)bg_template);
            break;
        case BATTLEGROUND_WS:
            bg = new BattleGroundWS(*(BattleGroundWS*)bg_template);
            break;
        case BATTLEGROUND_AB:
            bg = new BattleGroundAB(*(BattleGroundAB*)bg_template);
            break;
        default:
            // error, but it is handled few lines above
            return 0;
    }

    // will also set m_bgMap, instanceid
    sMapMgr.CreateBgMap(bg->GetMapId(), bg);

    bg->SetClientInstanceID(CreateClientVisibleInstanceId(bgTypeId, bracket_id));

    // reset the new bg (set status to status_wait_queue from status_none)
    bg->Reset();

    // start the joining of the bg
    bg->SetStatus(STATUS_WAIT_JOIN);
    bg->SetBracketId(bracket_id);

    return bg;
}

/**
 * @brief Creates a battleground template.
 *
 * Creates a template battleground that serves as the prototype for all instances of this type.
 * The template stores configuration like player limits, level requirements, and spawn locations.
 * New instances are created by copying this template.
 *
 * @param bgTypeId The battleground type.
 * @param MinPlayersPerTeam Minimum players required per team.
 * @param MaxPlayersPerTeam Maximum players allowed per team.
 * @param LevelMin Minimum level to queue for this battleground.
 * @param LevelMax Maximum level for this battleground.
 * @param BattleGroundName The name of the battleground.
 * @param MapID The map ID for this battleground.
 * @param Team1StartLocX Alliance spawn location X coordinate.
 * @param Team1StartLocY Alliance spawn location Y coordinate.
 * @param Team1StartLocZ Alliance spawn location Z coordinate.
 * @param Team1StartLocO Alliance spawn location orientation.
 * @param Team2StartLocX Horde spawn location X coordinate.
 * @param Team2StartLocY Horde spawn location Y coordinate.
 * @param Team2StartLocZ Horde spawn location Z coordinate.
 * @param Team2StartLocO Horde spawn location orientation.
 * @param StartMaxDist Maximum distance from spawn location for initial positioning.
 * @return The instance ID of the created template battleground.
 */
uint32 BattleGroundMgr::CreateBattleGround(BattleGroundTypeId bgTypeId, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam, uint32 LevelMin, uint32 LevelMax, char const* BattleGroundName, uint32 MapID, float Team1StartLocX, float Team1StartLocY, float Team1StartLocZ, float Team1StartLocO, float Team2StartLocX, float Team2StartLocY, float Team2StartLocZ, float Team2StartLocO, float StartMaxDist)
{
    // Create the BG
    BattleGround* bg;
    switch (bgTypeId)
    {
        case BATTLEGROUND_AV: bg = new BattleGroundAV; break;
        case BATTLEGROUND_WS: bg = new BattleGroundWS; break;
        case BATTLEGROUND_AB: bg = new BattleGroundAB; break;
        default:              bg = new BattleGround;   break;                           // placeholder for non implemented BG
    }

    bg->SetMapId(MapID);
    bg->SetTypeID(bgTypeId);
    bg->SetMinPlayersPerTeam(MinPlayersPerTeam);
    bg->SetMaxPlayersPerTeam(MaxPlayersPerTeam);
    bg->SetMinPlayers(MinPlayersPerTeam * 2);
    bg->SetMaxPlayers(MaxPlayersPerTeam * 2);
    bg->SetName(BattleGroundName);
    bg->SetTeamStartLoc(ALLIANCE, Team1StartLocX, Team1StartLocY, Team1StartLocZ, Team1StartLocO);
    bg->SetTeamStartLoc(HORDE,    Team2StartLocX, Team2StartLocY, Team2StartLocZ, Team2StartLocO);
    bg->SetStartMaxDist(StartMaxDist);
    bg->SetLevelRange(LevelMin, LevelMax);

    // add bg to update list
    AddBattleGround(bg->GetInstanceID(), bg->GetTypeID(), bg);

    // return some not-null value, bgTypeId is good enough for me
    return bgTypeId;
}

/**
 * @brief Creates initial battleground templates from the database.
 *
 * Loads battleground template configurations from the database table `battleground_template`
 * and creates the template instances for each configured battleground type. These templates
 * are used as prototypes for all new battleground instances.
 */
void BattleGroundMgr::CreateInitialBattleGrounds()
{
    uint32 count = 0;

    //                                                0   1                 2                 3      4      5                6              7             8            9
    QueryResult* result = WorldDatabase.Query("SELECT `id`, `MinPlayersPerTeam`,`MaxPlayersPerTeam`,`MinLvl`,`MaxLvl`,`AllianceStartLoc`,`AllianceStartO`,`HordeStartLoc`,`HordeStartO`, `StartMaxDist` FROM `battleground_template`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 battlegrounds. DB table `battleground_template` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 bgTypeID_ = fields[0].GetUInt32();

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, bgTypeID_))
        {
            continue;
        }

        BattleGroundTypeId bgTypeID = BattleGroundTypeId(bgTypeID_);

        uint32 MinPlayersPerTeam = fields[1].GetUInt32();
        uint32 MaxPlayersPerTeam = fields[2].GetUInt32();
        uint32 MinLvl = fields[3].GetUInt32();
        uint32 MaxLvl = fields[4].GetUInt32();

        float AStartLoc[4];
        float HStartLoc[4];

        uint32 start1 = fields[5].GetUInt32();

        WorldSafeLocsEntry const* start = sWorldSafeLocsStore.LookupEntry(start1);
        if (start)
        {
            AStartLoc[0] = start->x;
            AStartLoc[1] = start->y;
            AStartLoc[2] = start->z;
            AStartLoc[3] = fields[6].GetFloat();
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u have nonexistent WorldSafeLocs.dbc id %u in field `AllianceStartLoc`. BG not created.", bgTypeID, start1);
            continue;
        }

        uint32 start2 = fields[7].GetUInt32();

        start = sWorldSafeLocsStore.LookupEntry(start2);
        if (start)
        {
            HStartLoc[0] = start->x;
            HStartLoc[1] = start->y;
            HStartLoc[2] = start->z;
            HStartLoc[3] = fields[8].GetFloat();
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u refers to a non-existing WorldSafeLocs.dbc id %u in field `HordeStartLoc`. BG not created.", bgTypeID, start2);
            continue;
        }

        uint32 mapId = GetBattleGrounMapIdByTypeId(bgTypeID);
        char const* name;

        if (MapEntry const* mapEntry = sMapStore.LookupEntry(mapId))
        {
            name = mapEntry->MapName_lang[sWorld.GetDefaultDbcLocale()];
        }
        else
        {
            sLog.outErrorDb("Table `battleground_template` for id %u associated with nonexistent map id %u.", bgTypeID, mapId);
            continue;
        }

        float startMaxDist = fields[9].GetFloat();
        // sLog.outDetail("Creating battleground %s, %u-%u", bl->name[sWorld.GetDBClang()], MinLvl, MaxLvl);
        if (!CreateBattleGround(bgTypeID, MinPlayersPerTeam, MaxPlayersPerTeam, MinLvl, MaxLvl, name, mapId, AStartLoc[0], AStartLoc[1], AStartLoc[2], AStartLoc[3], HStartLoc[0], HStartLoc[1], HStartLoc[2], HStartLoc[3], startMaxDist))
        {
            continue;
        }

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u battlegrounds", count);
    sLog.outString();
}

/**
 * @brief Builds the battleground instance list packet for a player.
 *
 * Enumerates the client-visible battleground instances available for the player's
 * bracket and writes them into the battlefield list response.
 *
 * @param data Pointer to the packet being filled.
 * @param guid The battlemaster GUID associated with the request.
 * @param plr The player receiving the list.
 * @param bgTypeId The battleground type being listed.
 */
void BattleGroundMgr::BuildBattleGroundListPacket(WorldPacket* data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId)
{
    if (!plr)
    {
        return;
    }

    uint32 mapId = GetBattleGrounMapIdByTypeId(bgTypeId);

    data->Initialize(SMSG_BATTLEFIELD_LIST);
    *data << guid;                                          // battlemaster guid
    *data << uint32(mapId);                                 // battleground id
    *data << uint8(0x00);                                   // unk

    // battleground
    {
        //*data << uint8(0x00);                               // [-ZERO] unk

        //size_t count_pos = data->wpos();
        //uint32 count = 0;

        uint32 bracket_id = plr->GetBattleGroundBracketIdFromLevel(bgTypeId);
        ClientBattleGroundIdSet const& ids = m_ClientBattleGroundIds[bgTypeId][bracket_id];
        *data << uint32(ids.size());                        // number of bg instances
        for (std::set<uint32>::const_iterator itr = ids.begin(); itr != ids.end(); ++itr)
        {
            *data << uint32(*itr);
            //++count;
        }
        //data->put<uint32>(count_pos , count);
    }
}

/**
 * @brief Teleports a player to their assigned battleground location.
 *
 * Moves the player to the battleground map and their team's spawn location. Handles
 * retrieving the correct start location for the player's team.
 *
 * @param pl Pointer to the player to teleport.
 * @param instanceId The battleground instance ID.
 * @param bgTypeId The battleground type.
 */
void BattleGroundMgr::SendToBattleGround(Player* pl, uint32 instanceId, BattleGroundTypeId bgTypeId)
{
    BattleGround* bg = GetBattleGround(instanceId, bgTypeId);
    if (bg)
    {
        uint32 mapid = bg->GetMapId();
        float x, y, z, O;
        Team team = pl->GetBGTeam();
        if (team == 0)
        {
            team = pl->GetTeam();
        }
        bg->GetTeamStartLoc(team, x, y, z, O);

        DETAIL_LOG("BATTLEGROUND: Sending %s to map %u, X %f, Y %f, Z %f, O %f", pl->GetName(), mapid, x, y, z, O);
        pl->TeleportTo(mapid, x, y, z, O);
    }
    else
    {
        sLog.outError("player %u trying to port to nonexistent bg instance %u", pl->GetGUIDLow(), instanceId);
    }
}










