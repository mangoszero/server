/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

/**
 * @brief
 *
 */
typedef std::map<uint32, BattleGround*> BattleGroundSet;

/**
 * @brief this container can't be deque, because deque doesn't like removing the last element - if you remove it, it invalidates next iterator and crash appears
 *
 */
typedef std::list<BattleGround*> BGFreeSlotQueueType;

/**
 * @brief
 *
 */
typedef UNORDERED_MAP<uint32, BattleGroundTypeId> BattleMastersMap;
/**
 * @brief
 *
 */
typedef UNORDERED_MAP<uint32, BattleGroundEventIdx> CreatureBattleEventIndexesMap;
/**
 * @brief
 *
 */
typedef UNORDERED_MAP<uint32, BattleGroundEventIdx> GameObjectBattleEventIndexesMap;

#define COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME 10

struct GroupQueueInfo;                                      // type predefinition
/**
 * @brief stores information for players in queue
 *
 */
struct PlayerQueueInfo
{
    uint32  LastOnlineTime;                                 /**< for tracking and removing offline players from queue after 5 minutes */
    GroupQueueInfo* GroupInfo;                              /**< pointer to the associated groupqueueinfo */
};

/**
 * @brief
 *
 */
typedef std::map<ObjectGuid, PlayerQueueInfo*> GroupQueueInfoPlayers;

/**
 * @brief stores information about the group in queue (also used when joined as solo!)
 *
 */
struct GroupQueueInfo
{
    GroupQueueInfoPlayers Players;                          /**< player queue info map */
    Team  GroupTeam;                                        /**< Player team (ALLIANCE/HORDE) */
    BattleGroundTypeId BgTypeId;                            /**< battleground type id */
    uint32  JoinTime;                                       /**< time when group was added */
    uint32  RemoveInviteTime;                               /**< time when we will remove invite for players in group */
    uint32  IsInvitedToBGInstanceGUID;                      /**< was invited to certain BG */
};

/**
 * @brief
 *
 */
enum BattleGroundQueueGroupTypes
{
    BG_QUEUE_PREMADE_ALLIANCE   = 0,
    BG_QUEUE_PREMADE_HORDE      = 1,
    BG_QUEUE_NORMAL_ALLIANCE    = 2,
    BG_QUEUE_NORMAL_HORDE       = 3
};
#define BG_QUEUE_GROUP_TYPES_COUNT 4

enum BattleGroundGroupJoinStatus
{
    BG_GROUPJOIN_DESERTERS      = -2,
    BG_GROUPJOIN_FAILED         = -1    // actually, any negative except 2
    // any other value is a MapID meaning successful join
};

class BattleGround;
/**
 * @brief
 *
 */
class BattleGroundQueue
{
    public:
        /**
         * @brief
         *
         */
        BattleGroundQueue();
        /**
         * @brief
         *
         */
        ~BattleGroundQueue();

        /**
         * @brief
         *
         * @param bgTypeId
         * @param bracket_id
         */
        void Update(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        /**
         * @brief
         *
         * @param bg
         * @param bracket_id
         */
        void FillPlayersToBG(BattleGround* bg, BattleGroundBracketId bracket_id);
        /**
         * @brief
         *
         * @param bracket_id
         * @param MinPlayersPerTeam
         * @param MaxPlayersPerTeam
         * @return bool
         */
        bool CheckPremadeMatch(BattleGroundBracketId bracket_id, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam);
        /**
         * @brief
         *
         * @param bracket_id
         * @param minPlayers
         * @param maxPlayers
         * @return bool
         */
        bool CheckNormalMatch(BattleGroundBracketId bracket_id, uint32 minPlayers, uint32 maxPlayers);
        /**
         * @brief
         *
         * @param leader
         * @param group
         * @param bgTypeId
         * @param bracketId
         * @param isPremade
         * @return GroupQueueInfo
         */
        GroupQueueInfo* AddGroup(Player* leader, Group* group, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracketId, bool isPremade);
        /**
         * @brief
         *
         * @param guid
         * @param decreaseInvitedCount
         */
        void RemovePlayer(ObjectGuid guid, bool decreaseInvitedCount);
        /**
         * @brief
         *
         * @param pl_guid
         * @param bgInstanceGuid
         * @param removeTime
         * @return bool
         */
        bool IsPlayerInvited(ObjectGuid pl_guid, const uint32 bgInstanceGuid, const uint32 removeTime);
        /**
         * @brief
         *
         * @param guid
         * @param ginfo
         * @return bool
         */
        bool GetPlayerGroupInfoData(ObjectGuid guid, GroupQueueInfo* ginfo);
        /**
         * @brief
         *
         * @param ginfo
         * @param bracket_id
         */
        void PlayerInvitedToBGUpdateAverageWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id);
        /**
         * @brief
         *
         * @param ginfo
         * @param bracket_id
         * @return uint32
         */
        uint32 GetAverageQueueWaitTime(GroupQueueInfo* ginfo, BattleGroundBracketId bracket_id);

    private:
        ACE_Recursive_Thread_Mutex  m_Lock; /**< mutex that should not allow changing private data, nor allowing to update Queue during private data change. */

        /**
         * @brief
         *
         */
        typedef std::map<ObjectGuid, PlayerQueueInfo> QueuedPlayersMap;
        QueuedPlayersMap m_QueuedPlayers; /**< TODO */

        /**
         * @brief we need constant add to begin and constant remove / add from the end, therefore deque suits our problem well
         *
         */
        typedef std::list<GroupQueueInfo*> GroupsQueueType;

        /*
        This two dimensional array is used to store All queued groups
        First dimension specifies the bgTypeId
        Second dimension specifies the player's group types -
             BG_QUEUE_PREMADE_ALLIANCE  is used for premade alliance groups and alliance rated arena teams
             BG_QUEUE_PREMADE_HORDE     is used for premade horde groups and horde rated arena teams
             BG_QUEUE_NORMAL_ALLIANCE   is used for normal (or small) alliance groups or non-rated arena matches
             BG_QUEUE_NORMAL_HORDE      is used for normal (or small) horde groups or non-rated arena matches
        */
        GroupsQueueType m_QueuedGroups[MAX_BATTLEGROUND_BRACKETS][BG_QUEUE_GROUP_TYPES_COUNT]; /**< TODO */

        /**
         * @brief class to select and invite groups to bg
         *
         */
        class SelectionPool
        {
            public:
                /**
                 * @brief
                 *
                 * Constructor
                 */
                SelectionPool() : PlayerCount(0) {}
                /**
                 * @brief
                 *
                 */
                void Init();
                /**
                 * @brief
                 *
                 * @param ginfo
                 * @param desiredCount
                 * @return bool
                 */
                bool AddGroup(GroupQueueInfo* ginfo, uint32 desiredCount);
                /**
                 * @brief
                 *
                 * @param size
                 * @return bool
                 */
                bool KickGroup(uint32 size);
                /**
                 * @brief
                 *
                 * @return uint32
                 */
                uint32 GetPlayerCount() const {return PlayerCount;}
            public:
                GroupsQueueType SelectedGroups; /**< TODO */
            private:
                uint32 PlayerCount; /**< TODO */
        };

        SelectionPool m_SelectionPools[PVP_TEAM_COUNT]; /**< one selection pool for horde, other one for alliance */

        /**
         * @brief
         *
         * @param ginfo
         * @param bg
         * @param side
         * @return bool
         */
        bool InviteGroupToBG(GroupQueueInfo* ginfo, BattleGround* bg, Team side);
        uint32 m_WaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS][COUNT_OF_PLAYERS_TO_AVERAGE_WAIT_TIME]; /**< TODO */
        uint32 m_WaitTimeLastPlayer[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS]; /**< TODO */
        uint32 m_SumOfWaitTimes[PVP_TEAM_COUNT][MAX_BATTLEGROUND_BRACKETS]; /**< TODO */
};

/**
 * @brief This class is used to invite player to BG again, when minute lasts from his first invitation it is capable to solve all possibilities
 *
 */
class BGQueueInviteEvent : public BasicEvent
{
    public:
        /**
         * @brief
         *
         * @param pl_guid
         * @param BgInstanceGUID
         * @param BgTypeId
         * @param removeTime
         */
        BGQueueInviteEvent(ObjectGuid pl_guid, uint32 BgInstanceGUID, BattleGroundTypeId BgTypeId, uint32 removeTime) :
            m_PlayerGuid(pl_guid), m_BgInstanceGUID(BgInstanceGUID), m_BgTypeId(BgTypeId), m_RemoveTime(removeTime)
        {
        };
        /**
         * @brief
         *
         */
        virtual ~BGQueueInviteEvent() {};

        /**
         * @brief
         *
         * @param e_time
         * @param p_time
         * @return bool
         */
        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        /**
         * @brief
         *
         * @param e_time
         */
        virtual void Abort(uint64 e_time) override;
    private:
        ObjectGuid m_PlayerGuid; /**< TODO */
        uint32 m_BgInstanceGUID; /**< TODO */
        BattleGroundTypeId m_BgTypeId; /**< TODO */
        uint32 m_RemoveTime; /**< TODO */
};

/**
 * @brief This class is used to remove player from BG queue after 1 minute 20 seconds from first invitation
 *
 * We must store removeInvite time in case player left queue and joined and is invited again
 * We must store bgQueueTypeId, because battleground can be deleted already, when player entered it
 *
 */
class BGQueueRemoveEvent : public BasicEvent
{
    public:
        /**
         * @brief
         *
         * @param plGuid
         * @param bgInstanceGUID
         * @param BgTypeId
         * @param bgQueueTypeId
         * @param removeTime
         */
        BGQueueRemoveEvent(ObjectGuid plGuid, uint32 bgInstanceGUID, BattleGroundTypeId BgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 removeTime)
            : m_PlayerGuid(plGuid), m_BgInstanceGUID(bgInstanceGUID), m_RemoveTime(removeTime), m_BgTypeId(BgTypeId), m_BgQueueTypeId(bgQueueTypeId)
        {}

        /**
         * @brief
         *
         */
        virtual ~BGQueueRemoveEvent() {}

        /**
         * @brief
         *
         * @param e_time
         * @param p_time
         * @return bool
         */
        virtual bool Execute(uint64 e_time, uint32 p_time) override;
        /**
         * @brief
         *
         * @param e_time
         */
        virtual void Abort(uint64 e_time) override;
    private:
        ObjectGuid m_PlayerGuid; /**< TODO */
        uint32 m_BgInstanceGUID; /**< TODO */
        uint32 m_RemoveTime; /**< TODO */
        BattleGroundTypeId m_BgTypeId; /**< TODO */
        BattleGroundQueueTypeId m_BgQueueTypeId; /**< TODO */
};

/**
 * @brief
 *
 */
class BattleGroundMgr
{
    public:
        /**
         * @brief Construction
         *
         */
        BattleGroundMgr();
        /**
         * @brief
         *
         */
        ~BattleGroundMgr();
        /**
         * @brief
         *
         * @param diff
         */
        void Update(uint32 diff);

        /* Packet Building */
        /**
         * @brief
         *
         * @param data
         * @param plr
         */
        void BuildPlayerJoinedBattleGroundPacket(WorldPacket* data, Player* plr);
        /**
         * @brief
         *
         * @param data
         * @param guid
         */
        void BuildPlayerLeftBattleGroundPacket(WorldPacket* data, ObjectGuid guid);
        /**
         * @brief
         *
         * @param data
         * @param guid
         * @param plr
         * @param bgTypeId
         */
        void BuildBattleGroundListPacket(WorldPacket* data, ObjectGuid guid, Player* plr, BattleGroundTypeId bgTypeId);
        /**
         * @brief
         *
         * @param data
         * @param bgTypeId
         */
        void BuildGroupJoinedBattlegroundPacket(WorldPacket* data, int32 status);
        /**
         * @brief
         *
         * @param data
         * @param field
         * @param value
         */
        void BuildUpdateWorldStatePacket(WorldPacket* data, uint32 field, uint32 value);
        /**
         * @brief
         *
         * @param data
         * @param bg
         */
        void BuildPvpLogDataPacket(WorldPacket* data, BattleGround* bg);
        /**
         * @brief
         *
         * @param data
         * @param bg
         * @param QueueSlot
         * @param StatusID
         * @param Time1
         * @param Time2
         */
        void BuildBattleGroundStatusPacket(WorldPacket* data, BattleGround* bg, uint8 QueueSlot, uint8 StatusID, uint32 Time1, uint32 Time2);
        /**
         * @brief
         *
         * @param data
         * @param soundid
         */
        void BuildPlaySoundPacket(WorldPacket* data, uint32 soundid);

        /* Battlegrounds */
        /**
         * @brief
         *
         * @param instanceId
         * @param bgTypeId
         * @return BattleGround
         */
        BattleGround* GetBattleGroundThroughClientInstance(uint32 instanceId, BattleGroundTypeId bgTypeId);
        /**
         * @brief
         *
         * @param InstanceID
         * @param bgTypeId
         * @return BattleGround
         */
        BattleGround* GetBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId); // there must be uint32 because MAX_BATTLEGROUND_TYPE_ID means unknown

        /**
         * @brief
         *
         * @param bgTypeId
         * @return BattleGround
         */
        BattleGround* GetBattleGroundTemplate(BattleGroundTypeId bgTypeId);
        /**
         * @brief
         *
         * @param bgTypeId
         * @param bracket_id
         * @return BattleGround
         */
        BattleGround* CreateNewBattleGround(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);

        /**
         * @brief
         *
         * @param bgTypeId
         * @param MinPlayersPerTeam
         * @param MaxPlayersPerTeam
         * @param LevelMin
         * @param LevelMax
         * @param BattleGroundName
         * @param MapID
         * @param Team1StartLocX
         * @param Team1StartLocY
         * @param Team1StartLocZ
         * @param Team1StartLocO
         * @param Team2StartLocX
         * @param Team2StartLocY
         * @param Team2StartLocZ
         * @param Team2StartLocO
         * @return uint32
         */
        uint32 CreateBattleGround(BattleGroundTypeId bgTypeId, uint32 MinPlayersPerTeam, uint32 MaxPlayersPerTeam, uint32 LevelMin, uint32 LevelMax, char const* BattleGroundName, uint32 MapID, float Team1StartLocX, float Team1StartLocY, float Team1StartLocZ, float Team1StartLocO, float Team2StartLocX, float Team2StartLocY, float Team2StartLocZ, float Team2StartLocO, float StartMaxDist);

        /**
         * @brief
         *
         * @param InstanceID
         * @param bgTypeId
         * @param BG
         */
        void AddBattleGround(uint32 InstanceID, BattleGroundTypeId bgTypeId, BattleGround* BG) { m_BattleGrounds[bgTypeId][InstanceID] = BG; };
        /**
         * @brief
         *
         * @param instanceID
         * @param bgTypeId
         */
        void RemoveBattleGround(uint32 instanceID, BattleGroundTypeId bgTypeId) { m_BattleGrounds[bgTypeId].erase(instanceID); }
        /**
         * @brief
         *
         * @param bgTypeId
         * @param bracket_id
         * @return uint32
         */
        uint32 CreateClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);
        /**
         * @brief
         *
         * @param bgTypeId
         * @param bracket_id
         * @param clientInstanceID
         */
        void DeleteClientVisibleInstanceId(BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id, uint32 clientInstanceID)
        {
            m_ClientBattleGroundIds[bgTypeId][bracket_id].erase(clientInstanceID);
        }

        /**
         * @brief
         *
         */
        void CreateInitialBattleGrounds();
        /**
         * @brief
         *
         */
        void DeleteAllBattleGrounds();

        /**
         * @brief
         *
         * @param pl
         * @param InstanceID
         * @param bgTypeId
         */
        void SendToBattleGround(Player* pl, uint32 InstanceID, BattleGroundTypeId bgTypeId);

        /* Battleground queues */
        // these queues are instantiated when creating BattlegroundMrg
        BattleGroundQueue m_BattleGroundQueues[MAX_BATTLEGROUND_QUEUE_TYPES]; /**< public, because we need to access them in BG handler code */

        BGFreeSlotQueueType BGFreeSlotQueue[MAX_BATTLEGROUND_TYPE_ID]; /**< TODO */

        /**
         * @brief
         *
         * @param bgQueueTypeId
         * @param bgTypeId
         * @param bracket_id
         */
        void ScheduleQueueUpdate(BattleGroundQueueTypeId bgQueueTypeId, BattleGroundTypeId bgTypeId, BattleGroundBracketId bracket_id);
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetPrematureFinishTime() const;

        /**
         * @brief
         *
         */
        void ToggleTesting();

        /**
         * @brief
         *
         */
        void LoadBattleMastersEntry();
        /**
         * @brief
         *
         * @param entry
         * @return BattleGroundTypeId
         */
        BattleGroundTypeId GetBattleMasterBG(uint32 entry) const
        {
            BattleMastersMap::const_iterator itr = mBattleMastersMap.find(entry);
            if (itr != mBattleMastersMap.end())
                { return itr->second; }
            return BATTLEGROUND_TYPE_NONE;
        }

        /**
         * @brief
         *
         */
        void LoadBattleEventIndexes();
        /**
         * @brief
         *
         * @param dbTableGuidLow
         * @return const BattleGroundEventIdx
         */
        const BattleGroundEventIdx GetCreatureEventIndex(uint32 dbTableGuidLow) const
        {
            CreatureBattleEventIndexesMap::const_iterator itr = m_CreatureBattleEventIndexMap.find(dbTableGuidLow);
            if (itr != m_CreatureBattleEventIndexMap.end())
                { return itr->second; }
            return m_CreatureBattleEventIndexMap.find(-1)->second;
        }
        /**
         * @brief
         *
         * @param dbTableGuidLow
         * @return const BattleGroundEventIdx
         */
        const BattleGroundEventIdx GetGameObjectEventIndex(uint32 dbTableGuidLow) const
        {
            GameObjectBattleEventIndexesMap::const_iterator itr = m_GameObjectBattleEventIndexMap.find(dbTableGuidLow);
            if (itr != m_GameObjectBattleEventIndexMap.end())
                { return itr->second; }
            return m_GameObjectBattleEventIndexMap.find(-1)->second;
        }

        /**
         * @brief
         *
         * @return bool
         */
        bool isTesting() const { return m_Testing; }

        /**
         * @brief
         *
         * @param bgTypeId
         * @return BattleGroundQueueTypeId
         */
        static BattleGroundQueueTypeId BGQueueTypeId(BattleGroundTypeId bgTypeId);
        /**
         * @brief
         *
         * @param bgQueueTypeId
         * @return BattleGroundTypeId
         */
        static BattleGroundTypeId BGTemplateId(BattleGroundQueueTypeId bgQueueTypeId);

        /**
         * @brief
         *
         * @param bgTypeId
         * @return HolidayIds
         */
        static HolidayIds BGTypeToWeekendHolidayId(BattleGroundTypeId bgTypeId);
        /**
         * @brief
         *
         * @param holiday
         * @return BattleGroundTypeId
         */
        static BattleGroundTypeId WeekendHolidayIdToBGType(HolidayIds holiday);
        /**
         * @brief
         *
         * @param bgTypeId
         * @return bool
         */
        static bool IsBGWeekend(BattleGroundTypeId bgTypeId);
    private:
        ACE_Thread_Mutex    SchedulerLock; /**< TODO */
        BattleMastersMap    mBattleMastersMap; /**< TODO */
        CreatureBattleEventIndexesMap m_CreatureBattleEventIndexMap; /**< TODO */
        GameObjectBattleEventIndexesMap m_GameObjectBattleEventIndexMap; /**< TODO */

        /* Battlegrounds */
        BattleGroundSet m_BattleGrounds[MAX_BATTLEGROUND_TYPE_ID]; /**< TODO */
        std::vector<uint32> m_QueueUpdateScheduler; /**< TODO */
        /**
         * @brief
         *
         */
        typedef std::set<uint32> ClientBattleGroundIdSet;
        ClientBattleGroundIdSet m_ClientBattleGroundIds[MAX_BATTLEGROUND_TYPE_ID][MAX_BATTLEGROUND_BRACKETS]; /**< the instanceids just visible for the client */
        bool   m_Testing; /**< TODO */
};

#define sBattleGroundMgr MaNGOS::Singleton<BattleGroundMgr>::Instance()
#endif
