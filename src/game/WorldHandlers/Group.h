/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOSSERVER_GROUP_H
#define MANGOSSERVER_GROUP_H

#include "Common.h"
#include "ObjectGuid.h"
#include "GroupReference.h"
#include "GroupRefManager.h"
#include "BattleGround.h"
#include "LootMgr.h"
#include "DBCEnums.h"
#include "SharedDefines.h"
#include "LFGHandler.h"
#include "LFGMgr.h"

class WorldSession;
class Map;
class BattleGround;
class DungeonPersistentState;
class Field;
class Unit;

#define MAX_GROUP_SIZE 5
#define MAX_RAID_SIZE 40
#define MAX_RAID_SUBGROUPS (MAX_RAID_SIZE / MAX_GROUP_SIZE)
#define TARGET_ICON_COUNT 8

enum LootMethod
{
    FREE_FOR_ALL      = 0,
    ROUND_ROBIN       = 1,
    MASTER_LOOT       = 2,
    GROUP_LOOT        = 3,
    NEED_BEFORE_GREED = 4
};

enum RemoveMethod
{
    GROUP_LEAVE            = 0,
    GROUP_KICK             = 1
};

enum InviteMethod
{
    GROUP_JOIN             = 0,
    GROUP_LFG              = 1
};

enum RollVote
{
    ROLL_PASS              = 0,
    ROLL_NEED              = 1,
    ROLL_GREED             = 2,

    // other not send by client
    MAX_ROLL_FROM_CLIENT   = 3,

    ROLL_NOT_EMITED_YET    = 3,                             // send to client
    ROLL_NOT_VALID         = 4                              // not send to client
};

enum GroupMemberOnlineStatus
{
    MEMBER_STATUS_OFFLINE   = 0x0000,
    MEMBER_STATUS_ONLINE    = 0x0001,                       // Lua_UnitIsConnected
    MEMBER_STATUS_PVP       = 0x0002,                       // Lua_UnitIsPVP
    MEMBER_STATUS_DEAD      = 0x0004,                       // Lua_UnitIsDead
    MEMBER_STATUS_GHOST     = 0x0008,                       // Lua_UnitIsGhost
    MEMBER_STATUS_PVP_FFA   = 0x0010,                       // Lua_UnitIsPVPFreeForAll
    MEMBER_STATUS_UNK3      = 0x0020,                       // used in calls from Lua_GetPlayerMapPosition/Lua_GetBattlefieldFlagPosition
    MEMBER_STATUS_AFK       = 0x0040,                       // Lua_UnitIsAFK
    MEMBER_STATUS_DND       = 0x0080,                       // Lua_UnitIsDND
};

enum GroupType
{
    GROUPTYPE_NORMAL = 0,
    GROUPTYPE_RAID   = 1
};

enum GroupUpdateFlags
{
    GROUP_UPDATE_FLAG_NONE              = 0x00000000,       // nothing
    GROUP_UPDATE_FLAG_STATUS            = 0x00000001,       // uint8, enum GroupMemberOnlineStatus
    GROUP_UPDATE_FLAG_CUR_HP            = 0x00000002,       // uint16
    GROUP_UPDATE_FLAG_MAX_HP            = 0x00000004,       // uint16
    GROUP_UPDATE_FLAG_POWER_TYPE        = 0x00000008,       // uint8, enum Powers
    GROUP_UPDATE_FLAG_CUR_POWER         = 0x00000010,       // uint16
    GROUP_UPDATE_FLAG_MAX_POWER         = 0x00000020,       // uint16
    GROUP_UPDATE_FLAG_LEVEL             = 0x00000040,       // uint16
    GROUP_UPDATE_FLAG_ZONE              = 0x00000080,       // uint16
    GROUP_UPDATE_FLAG_POSITION          = 0x00000100,       // uint16, uint16
    GROUP_UPDATE_FLAG_AURAS             = 0x00000200,       // uint32 mask, for each bit set uint16 spellid
    GROUP_UPDATE_FLAG_AURAS_2           = 0x00000400,       // uint16 above mask continuation, giving max total of 48 auras possible
    GROUP_UPDATE_FLAG_PET_GUID          = 0x00000800,       // uint64 pet guid
    GROUP_UPDATE_FLAG_PET_NAME          = 0x00001000,       // pet name, NULL terminated string
    GROUP_UPDATE_FLAG_PET_MODEL_ID      = 0x00002000,       // uint16, model id
    GROUP_UPDATE_FLAG_PET_CUR_HP        = 0x00004000,       // uint16 pet cur health
    GROUP_UPDATE_FLAG_PET_MAX_HP        = 0x00008000,       // uint16 pet max health
    GROUP_UPDATE_FLAG_PET_POWER_TYPE    = 0x00010000,       // uint8 pet power type
    GROUP_UPDATE_FLAG_PET_CUR_POWER     = 0x00020000,       // uint16 pet cur power
    GROUP_UPDATE_FLAG_PET_MAX_POWER     = 0x00040000,       // uint16 pet max power
    GROUP_UPDATE_FLAG_PET_AURAS         = 0x00080000,       // uint32 mask, for each bit set uint16 spellid, pet auras...
    GROUP_UPDATE_FLAG_PET_AURAS_2       = 0x00100000,       // uint16 above mask continuation, giving max total of 48 auras possible
    GROUP_UPDATE_MODE_OFFLINE           = 0x10000000,       // 

    GROUP_UPDATE_PLAYER                 = 0x000007FF,
    GROUP_UPDATE_PET                    = 0x001FF800,       // all pet flags
    GROUP_UPDATE_FULL                   = 0x001FFFFF,       // all known flags with data
};

#define GROUP_UPDATE_FLAGS_COUNT          21
                                                    // bit number: 0, 1, 2, 3, 4, 5, 6, 7, 8,    9,   10,11, 12,13,14,15,16,17,18,   19,   20
static const uint8 GroupUpdateLength[GROUP_UPDATE_FLAGS_COUNT] = { 1, 2, 2, 1, 2, 2, 2, 2, 4, 4+32, 2+16, 8, 10, 2, 2, 2, 1, 2, 2, 4+32, 2+16};

class Roll : public LootValidatorRef
{
    public:
        Roll(ObjectGuid _lootedTragetGuid, LootItem const& li)
            : lootedTargetGUID(_lootedTragetGuid), itemid(li.itemid), itemRandomPropId(li.randomPropertyId),
              totalPlayersRolling(0), totalNeed(0), totalGreed(0), totalPass(0), itemSlot(0) {}
        ~Roll() { }
        void setLoot(Loot* pLoot)
        {
            link(pLoot, this);
        }
        Loot* getLoot()
        {
            return getTarget();
        }
        void targetObjectBuildLink() override;

        ObjectGuid lootedTargetGUID;
        uint32 itemid;
        int32  itemRandomPropId;
        typedef UNORDERED_MAP<ObjectGuid, RollVote> PlayerVote;
        PlayerVote playerVote;                              // vote position correspond with player position (in group)
        uint8 totalPlayersRolling;
        uint8 totalNeed;
        uint8 totalGreed;
        uint8 totalPass;
        uint8 itemSlot;
};

struct InstanceGroupBind
{
    DungeonPersistentState* state;
    bool perm;
    /* permanent InstanceGroupBinds exist iff the leader has a permanent
       PlayerInstanceBind for the same instance. */
    InstanceGroupBind() : state(NULL), perm(false) {}
};

/** request member stats checken **/
/** todo: uninvite people that not accepted invite **/
class Group
{
    public:
        /**
        * Struct MemberSlot
        * Represent a member of a group with some of its caracteristics
        */
        struct MemberSlot
        {
            /* GUID of the player. */
            ObjectGuid  guid;
            /* Name of the player. */
            std::string name;
            /* Group of the player. */
            uint8       group;
            /* Indicates whether the player is assistant. */
            bool        assistant;
            /* The time when the player has joined the group. */
            time_t        joinTime;
        };
        typedef std::list<MemberSlot> MemberSlotList;
        typedef MemberSlotList::const_iterator member_citerator;

        typedef UNORDERED_MAP < uint32 /*mapId*/, InstanceGroupBind > BoundInstancesMap;
    protected:
        typedef MemberSlotList::iterator member_witerator;
        typedef std::set<Player*> InvitesList;
        typedef std::vector<Roll*> Rolls;

    public:
        Group();
        ~Group();

        // group manipulation methods
        bool   Create(ObjectGuid guid, const char* name);
        bool   LoadGroupFromDB(Field* fields);
        bool   LoadMemberFromDB(uint32 guidLow, uint8 subgroup, bool assistant);
        bool   AddInvite(Player* player);
        uint32 RemoveInvite(Player* player);
        void   RemoveAllInvites();
        bool   AddLeaderInvite(Player* player);
        bool   AddMember(ObjectGuid guid, const char* name, uint8 joinMethod = GROUP_JOIN);
        uint32 RemoveMember(ObjectGuid guid, uint8 removeMethod); // method: 0=just remove, 1=kick
        void   ChangeLeader(ObjectGuid guid);
        void   SetLootMethod(LootMethod method)
        {
            m_lootMethod = method;
        }
        void   SetLooterGuid(ObjectGuid guid)
        {
            m_looterGuid = guid;
        }
        void   UpdateLooterGuid(WorldObject* pSource, bool ifneed = false);
        void   SetLootThreshold(ItemQualities threshold)
        {
            m_lootThreshold = threshold;
        }
        void   Disband(bool hideDestroy = false);

        // properties accessories
        uint32 GetId() const
        {
            return m_Id;
        }
        bool IsFull() const
        {
            return (m_groupType == GROUPTYPE_NORMAL) ? (m_memberSlots.size() >= MAX_GROUP_SIZE) : (m_memberSlots.size() >= MAX_RAID_SIZE);
        }
        bool isRaidGroup() const
        {
            return m_groupType == GROUPTYPE_RAID;
        }
        bool isBGGroup()   const
        {
            return m_bgGroup != NULL;
        }
        bool IsCreated()   const
        {
            return GetMembersCount() > 0;
        }
        ObjectGuid GetLeaderGuid() const
        {
            return m_leaderGuid;
        }
        const char* GetLeaderName() const
        {
            return m_leaderName.c_str();
        }
        LootMethod    GetLootMethod() const
        {
            return m_lootMethod;
        }
        ObjectGuid GetLooterGuid() const
        {
            return m_looterGuid;
        }
        ItemQualities GetLootThreshold() const
        {
            return m_lootThreshold;
        }

        // member manipulation methods
        bool IsMember(ObjectGuid guid) const
        {
            return _getMemberCSlot(guid) != m_memberSlots.end();
        }
        bool IsLeader(ObjectGuid guid) const
        {
            return GetLeaderGuid() == guid;
        }
        ObjectGuid GetMemberGuid(const std::string& name)
        {
            for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                if (itr->name == name)
                    { return itr->guid; }

            return ObjectGuid();
        }
        bool IsAssistant(ObjectGuid guid) const
        {
            member_citerator mslot = _getMemberCSlot(guid);
            if (mslot == m_memberSlots.end())
                { return false; }

            return mslot->assistant;
        }
        Player* GetInvited(ObjectGuid guid) const;
        Player* GetInvited(const std::string& name) const;

        bool HasFreeSlotSubGroup(uint8 subgroup) const
        {
            return (m_subGroupsCounts && m_subGroupsCounts[subgroup] < MAX_GROUP_SIZE);
        }

        bool SameSubGroup(Player const* member1, Player const* member2) const;

        /**
        * Returns the joined time of a member if it exist.
        * \param guid GUID of the player to look for.
        * \return time_t representing the joined time for that player or NULL if it doesn't exist.
        */
        time_t GetMemberSlotJoinedTime(ObjectGuid guid)
        {
            member_citerator mslot = _getMemberCSlot(guid);
            if(mslot == m_memberSlots.end())
                { return 0; }

            return mslot->joinTime;
        }

        MemberSlotList const& GetMemberSlots() const { return m_memberSlots; }
        GroupReference* GetFirstMember() { return m_memberMgr.getFirst(); }
        GroupReference const* GetFirstMember() const { return m_memberMgr.getFirst(); }
        uint32 GetMembersCount() const { return m_memberSlots.size(); }
        void GetDataForXPAtKill(Unit const* victim, uint32& count, uint32& sum_level, Player*& member_with_max_level, Player*& not_gray_member_with_max_level, Player* additional = NULL);
        uint8 GetMemberGroup(ObjectGuid guid) const
        {
            member_citerator mslot = _getMemberCSlot(guid);
            if (mslot == m_memberSlots.end())
                { return MAX_RAID_SUBGROUPS + 1; }

            return mslot->group;
        }

        // some additional raid methods
        void ConvertToRaid();

        void SetBattlegroundGroup(BattleGround* bg)
        {
            m_bgGroup = bg;
        }
        uint32 CanJoinBattleGroundQueue(BattleGroundTypeId bgTypeId, BattleGroundQueueTypeId bgQueueTypeId, uint32 MinPlayerCount, uint32 MaxPlayerCount);

        void ChangeMembersGroup(ObjectGuid guid, uint8 group);
        void ChangeMembersGroup(Player* player, uint8 group);

        ObjectGuid GetMainTankGuid() const
        {
            return m_mainTankGuid;
        }
        ObjectGuid GetMainAssistantGuid() const
        {
            return m_mainAssistantGuid;
        }

        void SetAssistant(ObjectGuid guid, bool state)
        {
            if (!isRaidGroup())
                { return; }
            if (_setAssistantFlag(guid, state))
                { SendUpdate(); }
        }
        void SetMainTank(ObjectGuid guid)
        {
            if (!isRaidGroup())
                { return; }

            if (_setMainTank(guid))
                { SendUpdate(); }
        }
        void SetMainAssistant(ObjectGuid guid)
        {
            if (!isRaidGroup())
                { return; }

            if (_setMainAssistant(guid))
                { SendUpdate(); }
        }

        void SetTargetIcon(uint8 id, ObjectGuid targetGuid);
        bool InCombatToInstance(uint32 instanceId);
        void ResetInstances(InstanceResetMethod method, Player* SendMsgTo);

        void SendTargetIconList(WorldSession* session);
        void SendUpdate();
        void SendUpdateToPlayer(Player* pPlayer);
        void UpdatePlayerOutOfRange(Player* pPlayer);
        // ignore: GUID of player that will be ignored
        void BroadcastPacket(WorldPacket* packet, bool ignorePlayersInBGRaid, int group = -1, ObjectGuid ignore = ObjectGuid());
        void BroadcastReadyCheck(WorldPacket* packet);
        void OfflineReadyCheck();

        void RewardGroupAtKill(Unit* pVictim, Player* player_tap);

        /*********************************************************/
        /***                   LFG SYSTEM                      ***/
        /*********************************************************/

        void SetLFGAreaId(uint32 areaId) { m_LFGAreaId = areaId; }
        uint32 GetLFGAreaId()            { return m_LFGAreaId;   }
        bool isInLFG()                   { return (m_LFGAreaId > 0) ? true : false; }

        void CalculateLFGRoles(LFGGroupQueueInfo& data);
        void FillPremadeLFG(ObjectGuid plrGuid, ClassRoles requiredRole, uint32& InitRoles, uint32& DpsCount, std::vector<ObjectGuid>& playersProcessed);

        bool inLFGGroup(std::vector<ObjectGuid> processed, ObjectGuid plr)
        {
            for (uint32 i = 0; i < processed.size(); ++i)
            {
                if (processed[i] == plr)
                {
                    return true;
                }
            }

            return false;
        }

        /*********************************************************/
        /***                   LOOT SYSTEM                     ***/
        /*********************************************************/

        void SendLootStartRoll(uint32 CountDown, const Roll& r);
        void SendLootRoll(ObjectGuid const& targetGuid, uint8 rollNumber, uint8 rollType, const Roll& r);
        void SendLootRollWon(ObjectGuid const& targetGuid, uint8 rollNumber, RollVote rollType, const Roll& r);
        void SendLootAllPassed(const Roll& r);
        void GroupLoot(WorldObject* pSource, Loot* loot);
        void NeedBeforeGreed(WorldObject* pSource, Loot* loot);
        void MasterLoot(WorldObject* pSource, Loot* loot);
        bool CountRollVote(Player* player, ObjectGuid const& lootedTarget, uint32 itemSlot, RollVote vote);
        void StartLootRoll(WorldObject* lootTarget, LootMethod method, Loot* loot, uint8 itemSlot);
        void EndRoll();

        /**
        * function that returns whether the roll is done for this group for the given creature and the given item.
        * \param Creature pointer to the creature which has dropped some loots.
        * \param Item pointer to the item to check.
        * \return bool true if the roll is done, false otherwise.
        */
        bool IsRollDoneForItem(WorldObject * pObject, const LootItem * pItem);

        void LinkMember(GroupReference* pRef)
        {
            m_memberMgr.insertFirst(pRef);
        }
        void DelinkMember(GroupReference* /*pRef*/) { }

        InstanceGroupBind* BindToInstance(DungeonPersistentState* save, bool permanent, bool load = false);
        void UnbindInstance(uint32 mapid, bool unload = false);
        InstanceGroupBind* GetBoundInstance(uint32 mapid);
        BoundInstancesMap& GetBoundInstances()
        {
            return m_boundInstances;
        }

#ifdef ENABLE_PLAYERBOTS
        ObjectGuid GetTargetIcon(int index) { return m_targetIcons[index]; }
#endif

    protected:
        bool _addMember(ObjectGuid guid, const char* name, bool isAssistant = false);
        bool _addMember(ObjectGuid guid, const char* name, bool isAssistant, uint8 group);
        bool _removeMember(ObjectGuid guid);                // returns true if leader has changed
        void _setLeader(ObjectGuid guid);

        void _removeRolls(ObjectGuid guid);

        bool _setMembersGroup(ObjectGuid guid, uint8 group);
        bool _setAssistantFlag(ObjectGuid guid, const bool& state);
        bool _setMainTank(ObjectGuid guid);
        bool _setMainAssistant(ObjectGuid guid);

        void _homebindIfInstance(Player* player);

        void _initRaidSubGroupsCounter()
        {
            // Sub group counters initialization
            if (!m_subGroupsCounts)
                { m_subGroupsCounts = new uint8[MAX_RAID_SUBGROUPS]; }

            memset((void*)m_subGroupsCounts, 0, MAX_RAID_SUBGROUPS * sizeof(uint8));

            for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                { ++m_subGroupsCounts[itr->group]; }
        }

        member_citerator _getMemberCSlot(ObjectGuid guid) const
        {
            for (member_citerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                if (itr->guid == guid)
                    { return itr; }

            return m_memberSlots.end();
        }

        member_witerator _getMemberWSlot(ObjectGuid guid)
        {
            for (member_witerator itr = m_memberSlots.begin(); itr != m_memberSlots.end(); ++itr)
                if (itr->guid == guid)
                    { return itr; }

            return m_memberSlots.end();
        }

        void SubGroupCounterIncrease(uint8 subgroup)
        {
            if (m_subGroupsCounts)
                { ++m_subGroupsCounts[subgroup]; }
        }

        void SubGroupCounterDecrease(uint8 subgroup)
        {
            if (m_subGroupsCounts)
                { --m_subGroupsCounts[subgroup]; }
        }

        void CountTheRoll(Rolls::iterator& roll);           // iterator update to next, in CountRollVote if true
        bool CountRollVote(ObjectGuid const& playerGUID, Rolls::iterator& roll, RollVote vote);

        uint32              m_Id;                           // 0 for not created or BG groups
        MemberSlotList      m_memberSlots;
        GroupRefManager     m_memberMgr;
        InvitesList         m_invitees;
        ObjectGuid          m_leaderGuid;
        std::string         m_leaderName;
        ObjectGuid          m_mainTankGuid;
        ObjectGuid          m_mainAssistantGuid;
        GroupType           m_groupType;
        BattleGround*       m_bgGroup;
        ObjectGuid          m_targetIcons[TARGET_ICON_COUNT];
        LootMethod          m_lootMethod;
        ItemQualities       m_lootThreshold;
        ObjectGuid          m_looterGuid;
        Rolls               RollId;
        BoundInstancesMap   m_boundInstances;
        uint8*              m_subGroupsCounts;
        uint32              m_LFGAreaId;
};
#endif
