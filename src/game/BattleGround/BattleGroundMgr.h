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

#ifndef MANGOS_H_BATTLEGROUNDMGR
#define MANGOS_H_BATTLEGROUNDMGR

#include "Common.h"
#include "Policies/Singleton.h"
#include "BattleGround.h"
#include <ace/Recursive_Thread_Mutex.h>
#include "Utilities/EventProcessor.h"

/**
 * @brief Container for storing battleground instances.
 */
typedef std::map<uint32, BattleGround*> BattleGroundSet;

/**
 * @brief Queue for free battleground slots.
 * This container can't be deque, because deque doesn't like removing the last element - if you remove it, it invalidates next iterator and crash appears.
 */
typedef std::list<BattleGround*> BGFreeSlotQueueType;

/**
 * @brief Map for storing battle master entries.
 */
typedef UNORDERED_MAP<uint32, BattleGroundTypeId> BattleMastersMap;

/**
 * @brief Map for storing creature battle event indexes.
 */
typedef UNORDERED_MAP<uint32, BattleGroundEventIdx> CreatureBattleEventIndexesMap;

/**
 * @brief Map for storing game object battle event indexes.
 */
typedef UNORDERED_MAP<uint32, BattleGroundEventIdx> GameObjectBattleEventIndexesMap;

#define COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME 10

struct GroupQueueInfo; // type predefinition

/**
 * @brief Stores information for players in queue.
 */
struct PlayerQueueInfo
{
    uint32  LastOnlineTime; /**< For tracking and removing offline players from queue after 5 minutes */
    GroupQueueInfo* GroupInfo; /**< Pointer to the associated groupqueueinfo */
};

/**
 * @brief Container for storing player queue info in a group.
 */
typedef std::map<ObjectGuid, PlayerQueueInfo*> GroupQueueInfoPlayers;

/**
 * @brief Stores information about the group in queue (also used when joined as solo!).
 */
struct GroupQueueInfo
{
    GroupQueueInfoPlayers Players; /**< Player queue info map */
    Team  GroupTeam; /**< Player team (ALLIANCE/HORDE) */
    BattleGroundTypeId BgTypeId; /**< Battleground type id */
    uint32  JoinTime; /**< Time when group was added */
    uint32  RemoveInviteTime; /**< Time when we will remove invite for players in group */
    uint32  IsInvitedToBGInstanceGUID; /**< Was invited to certain BG */
};

/**
 * @brief Enum for battleground queue group types.
 */
enum BattleGroundQueueGroupTypes
{
    BG_QUEUE_PREMADE_ALLIANCE   = 0,
    BG_QUEUE_PREMADE_HORDE      = 1,
    BG_QUEUE_NORMAL_ALLIANCE    = 2,
    BG_QUEUE_NORMAL_HORDE       = 3
};
#define BG_QUEUE_GROUP_TYPES_COUNT 4

/**
 * @brief Enum for battleground group join status.
 */
enum BattleGroundGroupJoinStatus
{
    BG_GROUPJOIN_DESERTERS      = -2,
    BG_GROUPJOIN_FAILED         = -1    // actually, any negative except 2
    // any other value is a MapID meaning successful join
};

class BattleGround;

/**
 * @brief Class for managing battleground queues.
 */
class BattleGroundQueue
{
    public:
        /**
         * @brief Constructor for BattleGroundQueue.
         */
        BattleGroundQueue();
        /**
         * @brief Destructor for BattleGroundQueue.
         */
        ~BattleGroundQueue();

        /**
         * @brief Updates the battleground queue.
         * @param bgTypeId The battleground type id.
         * @param bracket_id The bracket id.
         */
        void Update(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        /**
         * @brief Fills players to the battleground.
         * @param bg The battleground.
         * @param bracket_id The bracket id.
         */
        void FillPlayersToBG(BattleGround* bg, BattleGroundBracketId bracket_id);

        /**
         * @brief Checks for premade match.
         * @param bracket_id The bracket id.
         * @param MinPlayersPerTeam Minimum players per team.
         * @param MaxPlayersPerTeam Maximum players per team.
         * @return bool True if premade match is found, false otherwise.
         */
        bool CheckPremadeMatch(BattleGroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam);

        /**
         * @brief Checks for normal match.
         * @param bracket_id The bracket id.
         * @param minPlayers Minimum players.
         * @param maxPlayers Maximum players.
         * @return bool True if normal match is found, false otherwise.
         */
        bool CheckNormalMatch(BattleGroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers);

        /**
         * @brief Adds a group to the battleground queue.
         * @param leader The group leader.
         * @param group The group.
         * @param bgTypeId The battleground type id.
         * @param bracketId The bracket id.
         * @param isPremade True if the group is premade, false otherwise.
         * @return GroupQueueInfo* Pointer to the group queue info.
         */
        GroupQueueInfo* AddGroup(Player* leader, Group* group, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId, bool isPremade);

        /**
         * @brief Removes a player from the battleground queue.
         * @param guid The player's GUID.
         * @param decreaseInvitedCount True if the invited count should be decreased, false otherwise.
         */
        void RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount);

        /**
         * @brief Checks if a player is invited to the battleground.
         * @param pl_guid The player's GUID.
         * @param bgInstanceGuid The battleground instance GUID.
         * @param removeTime The remove time.
         * @return bool True if the player is invited, false otherwise.
         */
        bool IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime);

        /**
         * @brief Gets the player group info data.
         * @param guid The player's GUID.
         * @param ginfo Pointer to the group queue info.
         * @return bool True if the player group info data is found, false otherwise.
         */
        bool GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo);

        /**
         * @brief Updates the average wait time for a player invited to the battleground.
         * @param ginfo Pointer to the group queue info.
         * @param bracket_id The bracket id.
         */
        void PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id);

        /**
         * @brief Gets the average queue wait time.
         * @param ginfo Pointer to the group queue info.
         * @param bracket_id The bracket id.
         * @return uint32 The average queue wait time.
         */
        uint32 GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id);

    private:
        ACE_Recursive_Thread_Mutex  m_Lock; /**< Mutex that should not allow changing private data, nor allowing to update Queue during private data change. */

        /**
         * @brief Map for storing queued players.
         */
        typedef std::map<ObjectGuid, PlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_QueuedPlayers; /**< Map for storing queued players. */

        /**
         * @brief List for storing queued groups.
         * We need constant add to begin and constant remove / add from the end, therefore deque suits our problem well.
         */
        typedef std::list<GroupQueueInfo*> GroupsQueueType;

        /**
         * @brief Two dimensional array for storing all queued groups.
         * First dimension specifies the bgTypeId.
         * Second dimension specifies the player's group types.
             BG_QUEUE_PREMADE_ALLIANCE  is used for premade alliance groups and alliance rated arena teams
             BG_QUEUE_PREMADE_HORDE     is used for premade horde groups and horde rated arena teams
             BG_QUEUE_NORMAL_ALLIANCE   is used for normal (or small) alliance groups or non-rated arena matches
             BG_QUEUE_NORMAL_HORDE      is used for normal (or small) horde groups or non-rated arena matches
         */
        GroupsQueueType m_QueuedGroups[MAX_BATTLEGROUND_BRACKETS][BG_QUEUE_GROUP_TYPES_COUNT]; /**< Two dimensional array for storing all queued groups. */

        /**
         * @brief Class to select and invite groups to battleground.
         */
        class SelectionPool
        {
            public:
                /**
                 * @brief Constructor for SelectionPool.
                 */
                SelectionPool() : PlayerCount(0) {}

                /**
                 * @brief Initializes the selection pool.
                 */
                void Init();

                /**
                 * @brief Adds a group to the selection pool.
                 * @param ginfo Pointer to the group queue info.
                 * @param desiredCount The desired count.
                 * @return bool True if the group is added, false otherwise.
                 */
                bool AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount);

                /**
                 * @brief Kicks a group from the selection pool.
                 * @param size The size of the group.
                 * @return bool True if the group is kicked, false otherwise.
                 */
                bool KickGroup(uint32 size);

                /**
                 * @brief Gets the player count in the selection pool.
                 * @return uint32 The player count.
                 */
                uint32 GetPlayerCount() const {return PlayerCount;}

            public:
                GroupsQueueType SelectedGroups; /**< List of selected groups. */

            private:
                uint32 PlayerCount; /**< Player count in the selection pool. */
        };

        SelectionPool m_SelectionPools[PVP_TEAM_COUNT]; /**< One selection pool for horde, other one for alliance. */

        /**
         * @brief Invites a group to the battleground.
         * @param ginfo Pointer to the group queue info.
         * @param bg Pointer to the battleground.
         * @param side The team side.
         * @return bool True if the group is invited, false otherwise.
         */
        bool InviteGroupToBG(GroupQueueInfo* ginfo, BattleGround* bg, Team side);

        uint32 m_WaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME]; /**< Array for storing wait times. */
        uint32 m_WaitTimeLastPlayer[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS]; /**< Array for storing last player wait times. */
        uint32 m_SumOfWaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS]; /**< Array for storing sum of wait times. */
};

/**
 * @brief This class is used to invite player to BG again, when minute lasts from his first invitation it is capable to solve all possibilities.
 */
class BGQueueInviteEvent : public BasicEvent
{
    public:
        /**
         * @brief Constructor for BGQueueInviteEvent.
         * @param pl_guid The player's GUID.
         * @param BgInstanceGUID The battleground instance GUID.
         * @param BgTypeId The battleground type id.
         * @param removeTime The remove time.
         */
        BGQueueInviteEvent(ObjectGuid pl_guid, uint32 BgInstanceGUID, BattleGroundTypeId BgTypeId, uint32 removeTime) :
            m_PlayerGuid(pl_guid), m_BgInstanceGUID(BgInstanceGUID), m_BgTypeId(BgTypeId), m_RemoveTime(removeTime)
        {};

        /**
         * @brief Destructor for BGQueueInviteEvent.
         */
        virtual ~BGQueueInviteEvent() {};

        /**
         * @brief Executes the event.
         * @param e_time The event time.
         * @param p_time The process time.
         * @return bool True if the event is executed, false otherwise.
         */
        bool Execute(uint64 e_time, uint32 p_time) override;

        /**
         * @brief Aborts the event.
         * @param e_time The event time.
         */
        void Abort(uint64 e_time) override;

    private:
        ObjectGuid m_PlayerGuid; /**< The player's GUID. */
        uint32 m_BgInstanceGUID; /**< The battleground instance GUID. */
        BattleGroundTypeId m_BgTypeId; /**< The battleground type id. */
        uint32 m_RemoveTime; /**< The remove time. */
};

/**
 * @brief This class is used to remove player from BG queue after 1 minute 20 seconds from first invitation.
 * We must store removeInvite time in case player left queue and joined and is invited again.
 * We must store bgQueueTypeId, because battleground can be deleted already, when player entered it.
 */
class BGQueueRemoveEvent : public BasicEvent
{
    public:
        /**
         * @brief Constructor for BGQueueRemoveEvent.
         * @param plGuid The player's GUID.
         * @param bgInstanceGUID The battleground instance GUID.
         * @param BgTypeId The battleground type id.
         * @param bgQueueTypeId The battleground queue type id.
         * @param removeTime The remove time.
         */
        BGQueueRemoveEvent(ObjectGuid plGuid, uint32 bgInstanceGUID, BattleGroundTypeId BgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 removeTime)
            : m_PlayerGuid(plGuid), m_BgInstanceGUID(bgInstanceGUID), m_RemoveTime(removeTime), m_BgTypeId(BgTypeId), m_BgQueueTypeId(bgQueueTypeId)
        {}

        /**
         * @brief Destructor for BGQueueRemoveEvent.
         */
        virtual ~BGQueueRemoveEvent() {}

        /**
         * @brief Executes the event.
         * @param e_time The event time.
         * @param p_time The process time.
         * @return bool True if the event is executed, false otherwise.
         */
        bool Execute(uint64 e_time, uint32 p_time) override;

        /**
         * @brief Aborts the event.
         * @param e_time The event time.
         */
        void Abort(uint64 e_time) override;

    private:
        ObjectGuid m_PlayerGuid; /**< The player's GUID. */
        uint32 m_BgInstanceGUID; /**< The battleground instance GUID. */
        uint32 m_RemoveTime; /**< The remove time. */
        BattleGroundTypeId m_BgTypeId; /**< The battleground type id. */
        BattleGroundQueueTypeId m_BgQueueTypeId; /**< The battleground queue type id. */
};

/**
 * @brief Class for managing battlegrounds.
 */
class BattleGroundMgr
{
    public:
        /**
         * @brief Constructor for BattleGroundMgr.
         */
        BattleGroundMgr();

        /**
         * @brief Destructor for BattleGroundMgr.
         */
        ~BattleGroundMgr();

        /**
         * @brief Updates the battleground manager.
         * @param diff The time difference.
         */
        void Update(uint32 diff);

        /* Packet Building */

        /**
         * @brief Builds a packet for player joined battleground.
         * @param data The packet data.
         * @param plr The player.
         */
        void BuildPlayerJoinedBattleGroundPacket(WorldPacket* data, Player* plr);

        /**
         * @brief Builds a packet for player left battleground.
         * @param data The packet data.
         * @param guid The player's GUID.
         */
        void BuildPlayerLeftBattleGroundPacket(WorldPacket* data, ObjectGuid guid);

        /**
         * @brief Builds a packet for battleground list.
         * @param data The packet data.
         * @param guid The player's GUID.
         * @param plr The player.
         * @param bgTypeId The battleground type id.
         */
        void BuildBattleGroundListPacket(WorldPacket* data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId);

        /**
         * @brief Builds a packet for group joined battleground.
         * @param data The packet data.
         * @param status The status.
         */
        void BuildGroupJoinedBattlegroundPacket(WorldPacket* data, int32 status);

        /**
         * @brief Builds a packet for updating world state.
         * @param data The packet data.
         * @param field The field.
         * @param value The value.
         */
        void BuildUpdateWorldStatePacket(WorldPacket* data, uint32 field, uint32 value);

        /**
         * @brief Builds a packet for PvP log data.
         * @param data The packet data.
         * @param bg The battleground.
         */
        void BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg);

        /**
         * @brief Builds a packet for battleground status.
         * @param data The packet data.
         * @param bg The battleground.
         * @param QueueSlot The queue slot.
         * @param StatusID The status ID.
         * @param Time1 The first time value.
         * @param Time2 The second time value.
         */
        void BuildBattleGroundStatusPacket(WorldPacket* data, BattleGround* bg, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2);

        /**
         * @brief Builds a packet for playing sound.
         * @param data The packet data.
         * @param soundid The sound ID.
         */
        void BuildPlaySoundPacket(WorldPacket* data, uint32 soundid);

        /* Battlegrounds */

        /**
         * @brief Gets a battleground through client instance.
         * @param instanceId The instance ID.
         * @param bgTypeId The battleground type id.
         * @return BattleGround* Pointer to the battleground.
         */
        BattleGround* GetBattleGroundThroughClientInstance(uint32 instanceId, BattleGroundTypeId bgTypeId);

        /**
         * @brief Gets a battleground.
         * @param InstanceID The instance ID.
         * @param bgTypeId The battleground type id.
         * @return BattleGround* Pointer to the battleground.
         */
        BattleGround* GetBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId); // there must be uint32 because MAX_BATTLEGROUND_TYPE_ID means unknown

        /**
         * @brief Gets a battleground template.
         * @param bgTypeId The battleground type id.
         * @return BattleGround* Pointer to the battleground template.
         */
        BattleGround* GetBattleGroundTemplate(BattleGroundTypeId bgTypeId);
        /**
         * @brief Creates a new battleground instance.
         *
         * @param bgTypeId The battleground type id.
         * @param bracket_id The bracket id.
         * @return BattleGround* Pointer to the created battleground instance.
         */
        BattleGround* CreateNewBattleGround(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        /**
         * @brief Creates a battleground with specified parameters.
         *
         * @param bgTypeId The battleground type id.
         * @param MinPlayersPerTeam Minimum players per team.
         * @param MaxPlayersPerTeam Maximum players per team.
         * @param LevelMin Minimum level required.
         * @param LevelMax Maximum level allowed.
         * @param BattleGroundName Name of the battleground.
         * @param MapID The map ID.
         * @param Team1StartLocX Team 1 start location X coordinate.
         * @param Team1StartLocY Team 1 start location Y coordinate.
         * @param Team1StartLocZ Team 1 start location Z coordinate.
         * @param Team1StartLocO Team 1 start location orientation.
         * @param Team2StartLocX Team 2 start location X coordinate.
         * @param Team2StartLocY Team 2 start location Y coordinate.
         * @param Team2StartLocZ Team 2 start location Z coordinate.
         * @param Team2StartLocO Team 2 start location orientation.
         * @param StartMaxDist Maximum start distance.
         * @return uint32 The instance ID of the created battleground.
         */
        uint32 CreateBattleGround(BattleGroundTypeId bgTypeId, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam, uint32 LevelMin, uint32 LevelMax, char const* BattleGroundName, uint32 MapID, float Team1StartLocX, float Team1StartLocY, float Team1StartLocZ, float Team1StartLocO, float Team2StartLocX, float Team2StartLocY, float Team2StartLocZ, float Team2StartLocO, float StartMaxDist);

        /**
         * @brief Adds a battleground instance to the manager.
         *
         * @param InstanceID The instance ID.
         * @param bgTypeId The battleground type id.
         * @param BG Pointer to the battleground instance.
         */
        void AddBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId, BattleGround* BG) { m_BattleGrounds[bgTypeId][InstanceID] = BG; };

        /**
         * @brief Removes a battleground instance from the manager.
         *
         * @param instanceID The instance ID.
         * @param bgTypeId The battleground type id.
         */
        void RemoveBattleGround(uint32 instanceID, BattleGroundTypeId bgTypeId) { m_BattleGrounds[bgTypeId].erase(instanceID); }

        /**
         * @brief Creates a client-visible instance ID for a battleground.
         *
         * @param bgTypeId The battleground type id.
         * @param bracket_id The bracket id.
         * @return uint32 The client-visible instance ID.
         */
        uint32 CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        /**
         * @brief Deletes a client-visible instance ID for a battleground.
         *
         * @param bgTypeId The battleground type id.
         * @param bracket_id The bracket id.
         * @param clientInstanceID The client-visible instance ID.
         */
        void DeleteClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id, uint32 clientInstanceID)
        {
            m_ClientBattleGroundIds[bgTypeId][bracket_id].erase(clientInstanceID);
        }

        /**
         * @brief Creates initial battlegrounds.
         */
        void CreateInitialBattleGrounds();

        /**
         * @brief Deletes all battlegrounds.
         */
        void DeleteAllBattleGrounds();

        /**
         * @brief Sends a player to a battleground.
         *
         * @param pl Pointer to the player.
         * @param InstanceID The instance ID.
         * @param bgTypeId The battleground type id.
         */
        void SendToBattleGround(Player* pl, uint32 InstanceID, BattleGroundTypeId bgTypeId);

        /* Battleground queues */
        // these queues are instantiated when creating BattlegroundMrg
        BattleGroundQueue m_BattleGroundQueues[MAX_BATTLEGROUND_QUEUE_TYPES]; /**< public, because we need to access them in BG handler code */

        BGFreeSlotQueueType BGFreeSlotQueue[MAX_BATTLEGROUND_TYPE_ID]; /**< Queue for free battleground slots. */

        /**
         * @brief Schedules a queue update for a battleground.
         *
         * @param bgQueueTypeId The battleground queue type id.
         * @param bgTypeId The battleground type id.
         * @param bracket_id The bracket id.
         */
        void ScheduleQueueUpdate(BattleGroundQueueTypeId bgQueueTypeId, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        /**
         * @brief Gets the premature finish time for a battleground.
         *
         * @return uint32 The premature finish time.
         */
        uint32 GetPrematureFinishTime() const;

        /**
         * @brief Toggles testing mode.
         */
        void ToggleTesting();

        /**
         * @brief Loads battle master entries.
         */
        void LoadBattleMastersEntry();

        /**
         * @brief Gets the battleground type id for a battle master entry.
         *
         * @param entry The battle master entry.
         * @return BattleGroundTypeId The battleground type id.
         */
        BattleGroundTypeId GetBattleMasterBG(uint32 entry) const
        {
            BattleMastersMap::const_iterator itr = mBattleMastersMap.find(entry);
            if (itr != mBattleMastersMap.end())
            {
                return itr->second;
            }
            return BATTLEGROUND_TYPE_NONE;
        }

        /**
         * @brief Loads battle event indexes.
         */
        void LoadBattleEventIndexes();

        /**
         * @brief Gets the creature event index for a given GUID.
         *
         * @param dbTableGuidLow The GUID.
         * @return const BattleGroundEventIdx The event index.
         */
        const BattleGroundEventIdx GetCreatureEventIndex(uint32 dbTableGuidLow) const
        {
            CreatureBattleEventIndexesMap::const_iterator itr = m_CreatureBattleEventIndexMap.find(dbTableGuidLow);
            if (itr != m_CreatureBattleEventIndexMap.end())
            {
                return itr->second;
            }
            return m_CreatureBattleEventIndexMap.find(-1)->second;
        }

        /**
         * @brief Gets the game object event index for a given GUID.
         *
         * @param dbTableGuidLow The GUID.
         * @return const BattleGroundEventIdx The event index.
         */
        const BattleGroundEventIdx GetGameObjectEventIndex(uint32 dbTableGuidLow) const
        {
            GameObjectBattleEventIndexesMap::const_iterator itr = m_GameObjectBattleEventIndexMap.find(dbTableGuidLow);
            if (itr != m_GameObjectBattleEventIndexMap.end())
            {
                return itr->second;
            }
            return m_GameObjectBattleEventIndexMap.find(-1)->second;
        }

        /**
         * @brief Checks if testing mode is enabled.
         *
         * @return bool True if testing mode is enabled, false otherwise.
         */
        bool isTesting() const { return m_Testing; }

        /**
         * @brief Gets the battleground queue type id for a given battleground type id.
         *
         * @param bgTypeId The battleground type id.
         * @return BattleGroundQueueTypeId The battleground queue type id.
         */
        static BattleGroundQueueTypeId BGQueueTypeId(BattleGroundTypeId bgTypeId);

        /**
         * @brief Gets the battleground type id for a given battleground queue type id.
         *
         * @param bgQueueTypeId The battleground queue type id.
         * @return BattleGroundTypeId The battleground type id.
         */
        static BattleGroundTypeId BGTemplateId(BattleGroundQueueTypeId bgQueueTypeId);

        /**
         * @brief Gets the holiday id for a battleground type id.
         *
         * @param bgTypeId The battleground type id.
         * @return HolidayIds The holiday id.
         */
        static HolidayIds BGTypeToWeekendHolidayId(BattleGroundTypeId bgTypeId);

        /**
         * @brief Gets the battleground type id for a holiday id.
         *
         * @param holiday The holiday id.
         * @return BattleGroundTypeId The battleground type id.
         */
        static BattleGroundTypeId WeekendHolidayIdToBGType(HolidayIds holiday);

        /**
         * @brief Checks if it is a battleground weekend for a given battleground type id.
         *
         * @param bgTypeId The battleground type id.
         * @return bool True if it is a battleground weekend, false otherwise.
         */
        static bool IsBGWeekend(BattleGroundTypeId bgTypeId);
    private:
        ACE_Thread_Mutex    SchedulerLock; /**< Mutex to protect the scheduler from concurrent access. */
        BattleMastersMap    mBattleMastersMap; /**< Map storing battle master entries. */
        CreatureBattleEventIndexesMap m_CreatureBattleEventIndexMap; /**< Map storing creature battle event indexes. */
        GameObjectBattleEventIndexesMap m_GameObjectBattleEventIndexMap; /**< Map storing game object battle event indexes. */

        /* Battlegrounds */
        BattleGroundSet m_BattleGrounds[MAX_BATTLEGROUND_TYPE_ID]; /**< Array of maps storing battleground instances by type ID. */
        std::vector<uint32> m_QueueUpdateScheduler; /**< Vector for scheduling queue updates. */
        /**
         * @brief Set of client-visible battleground instance IDs.
         * The first dimension specifies the battleground type ID.
         * The second dimension specifies the bracket ID.
         */
        typedef std::set<uint32> ClientBattleGroundIdSet;
        ClientBattleGroundIdSet m_ClientBattleGroundIds[MAX_BATTLEGROUND_TYPE_ID][MAX_BATTLEGROUND_BRACKETS]; /**< Array of sets storing client-visible battleground instance IDs. */
        bool   m_Testing; /**< Flag indicating if testing mode is enabled. */
};

#define sBattleGroundMgr MaNGOS::Singleton<BattleGroundMgr>::Instance()

#endif
