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

#ifndef MANGOS_H_BATTLEGROUND
#define MANGOS_H_BATTLEGROUND

#include "Common.h"
#include "SharedDefines.h"
#include "Map.h"
#include "ByteBuffer.h"
#include "ObjectGuid.h"

// magic event-numbers
#define BG_EVENT_NONE 255
// those generic events should get a high event id
#define BG_EVENT_DOOR 254


class Creature;
class GameObject;
class Group;
class Player;
class WorldPacket;
class BattleGroundMap;

struct WorldSafeLocsEntry;

/**
 * @brief
 *
 */
struct BattleGroundEventIdx
{
    uint8 event1; /**< TODO */
    uint8 event2; /**< TODO */
};

/**
 * @brief
 *
 */
enum BattleGroundSounds
{
    SOUND_HORDE_WINS                = 8454,
    SOUND_ALLIANCE_WINS             = 8455,
    SOUND_BG_START                  = 3439
};

/**
 * @brief
 *
 */
enum BattleGroundQuests
{
    SPELL_WS_QUEST_REWARD           = 43483,
    SPELL_AB_QUEST_REWARD           = 43484,
    SPELL_AV_QUEST_REWARD           = 43475,
    SPELL_AV_QUEST_KILLED_BOSS      = 23658,
    SPELL_AB_QUEST_REWARD_4_BASES   = 24061,
    SPELL_AB_QUEST_REWARD_5_BASES   = 24064
};

/**
 * @brief
 *
 */
enum BattleGroundMarks
{
    SPELL_WS_MARK_LOSER             = 24950,
    SPELL_WS_MARK_WINNER            = 24951,
    SPELL_AB_MARK_LOSER             = 24952,
    SPELL_AB_MARK_WINNER            = 24953,
    SPELL_AV_MARK_LOSER             = 24954,
    SPELL_AV_MARK_WINNER            = 24955
};

/**
 * @brief
 *
 */
enum BattleGroundMarksCount
{
    ITEM_WINNER_COUNT               = 3,
    ITEM_LOSER_COUNT                = 1
};

/**
 * @brief
 *
 */
enum BattleGroundTimeIntervals
{
    CHECK_PLAYER_POSITION_INVERVAL  = 1000,                 // ms
    RESURRECTION_INTERVAL           = 30000,                // ms
    INVITATION_REMIND_TIME          = 60000,                // ms
    INVITE_ACCEPT_WAIT_TIME         = 80000,                // ms
    TIME_TO_AUTOREMOVE              = 120000,               // ms
    MAX_OFFLINE_TIME                = 300,                  // secs
    RESPAWN_ONE_DAY                 = 86400,                // secs
    RESPAWN_IMMEDIATELY             = 0,                    // secs
    BUFF_RESPAWN_TIME               = 180                   // secs
};

/**
 * @brief
 *
 */
enum BattleGroundStartTimeIntervals
{
    BG_START_DELAY_2M               = 120000,               // ms (2 minutes)
    BG_START_DELAY_1M               = 60000,                // ms (1 minute)
    BG_START_DELAY_30S              = 30000,                // ms (30 seconds)
    BG_START_DELAY_NONE             = 0                     // ms
};

/**
 * @brief
 *
 */
enum BattleGroundBuffObjects
{
    BG_OBJECTID_SPEEDBUFF_ENTRY     = 179871,
    BG_OBJECTID_REGENBUFF_ENTRY     = 179904,
    BG_OBJECTID_BERSERKERBUFF_ENTRY = 179905
};

const uint32 Buff_Entries[3] = { BG_OBJECTID_SPEEDBUFF_ENTRY, BG_OBJECTID_REGENBUFF_ENTRY, BG_OBJECTID_BERSERKERBUFF_ENTRY }; /**< TODO */

/**
 * @brief
 *
 */
enum BattleGroundStatus
{
    STATUS_NONE         = 0,                                // first status, should mean bg is not instance
    STATUS_WAIT_QUEUE   = 1,                                // means bg is empty and waiting for queue
    STATUS_WAIT_JOIN    = 2,                                // this means, that BG has already started and it is waiting for more players
    STATUS_IN_PROGRESS  = 3,                                // means bg is running
    STATUS_WAIT_LEAVE   = 4                                 // means some faction has won BG and it is ending
};

/**
 * @brief
 *
 */
struct BattleGroundPlayer
{
    time_t  OfflineRemoveTime;                              /**< for tracking and removing offline players from queue after 5 minutes */
    Team    PlayerTeam;                                     /**< Player's team */
};

/**
 * @brief
 *
 */
struct BattleGroundObjectInfo
{
/**
 * @brief
 *
 */
    BattleGroundObjectInfo() : object(NULL), timer(0), spellid(0) {}

    GameObject*  object; /**< TODO */
    int32       timer; /**< TODO */
    uint32      spellid; /**< TODO */
};

/**
 * @brief handle the queue types and bg types separately to enable joining queue for different sized arenas at the same time
 *
 */
enum BattleGroundQueueTypeId
{
    BATTLEGROUND_QUEUE_NONE     = 0,
    BATTLEGROUND_QUEUE_AV       = 1,
    BATTLEGROUND_QUEUE_WS       = 2,
    BATTLEGROUND_QUEUE_AB       = 3
};
#define MAX_BATTLEGROUND_QUEUE_TYPES 4

/**
 * @brief
 *
 */
enum BattleGroundBracketId                                  // bracketId for level ranges
{
    BG_BRACKET_ID_TEMPLATE       = -1,                      // used to mark bg as template
    BG_BRACKET_ID_FIRST          = 0,                       // brackets start from specific BG min level and each include 10 levels range
    BG_BRACKET_ID_LAST           = 5                        // so for start level 10 will be 10-19, 20-29, ...  all greater max bg level included in last breaket
};

#define MAX_BATTLEGROUND_BRACKETS  6

/**
 * @brief
 *
 */
enum ScoreType
{
    SCORE_KILLING_BLOWS         = 1,
    SCORE_DEATHS                = 2,
    SCORE_HONORABLE_KILLS       = 3,
    SCORE_BONUS_HONOR           = 4,
    // WS
    SCORE_FLAG_CAPTURES         = 7,
    SCORE_FLAG_RETURNS          = 8,
    // AB
    SCORE_BASES_ASSAULTED       = 9,
    SCORE_BASES_DEFENDED        = 10,
    // AV
    SCORE_GRAVEYARDS_ASSAULTED  = 11,
    SCORE_GRAVEYARDS_DEFENDED   = 12,
    SCORE_TOWERS_ASSAULTED      = 13,
    SCORE_TOWERS_DEFENDED       = 14,
    SCORE_SECONDARY_OBJECTIVES  = 15
};

/**
 * @brief
 *
 */
enum BattleGroundStartingEvents
{
    BG_STARTING_EVENT_NONE  = 0x00,
    BG_STARTING_EVENT_1     = 0x01,
    BG_STARTING_EVENT_2     = 0x02,
    BG_STARTING_EVENT_3     = 0x04,
    BG_STARTING_EVENT_4     = 0x08
};

/**
 * @brief
 *
 */
enum BattleGroundStartingEventsIds
{
    BG_STARTING_EVENT_FIRST     = 0,
    BG_STARTING_EVENT_SECOND    = 1,
    BG_STARTING_EVENT_THIRD     = 2,
    BG_STARTING_EVENT_FOURTH    = 3
};
#define BG_STARTING_EVENT_COUNT 4

/**
 * @brief
 *
 */
enum BattleGroundJoinError
{
    BG_JOIN_ERR_OK = 0,
    BG_JOIN_ERR_OFFLINE_MEMBER = 1,
    BG_JOIN_ERR_GROUP_TOO_MANY = 2,
    BG_JOIN_ERR_MIXED_FACTION = 3,
    BG_JOIN_ERR_MIXED_LEVELS = 4,
    // BG_JOIN_ERR_MIXED_ARENATEAM = 5,
    BG_JOIN_ERR_GROUP_MEMBER_ALREADY_IN_QUEUE = 6,
    BG_JOIN_ERR_GROUP_DESERTER = 7,
    BG_JOIN_ERR_ALL_QUEUES_USED = 8,
    BG_JOIN_ERR_GROUP_NOT_ENOUGH = 9
};

/**
 * @brief
 *
 */
class BattleGroundScore
{
    public:
        /**
         * @brief
         *
         */
        BattleGroundScore() : KillingBlows(0), Deaths(0), HonorableKills(0),
            DishonorableKills(0), BonusHonor(0)
        {}
        /**
         * @brief virtual destructor is used when deleting score from scores map
         *
         */
        virtual ~BattleGroundScore() {}

        uint32 GetKillingBlows() const      { return KillingBlows; }
        uint32 GetDeaths() const            { return Deaths; }
        uint32 GetHonorableKills() const    { return HonorableKills; }
        uint32 GetBonusHonor() const        { return BonusHonor; }
        uint32 GetDamageDone() const        { return 0; }
        uint32 GetHealingDone() const       { return 0; }

        virtual uint32 GetAttr1() const     { return 0; }
        virtual uint32 GetAttr2() const     { return 0; }
        virtual uint32 GetAttr3() const     { return 0; }
        virtual uint32 GetAttr4() const     { return 0; }
        virtual uint32 GetAttr5() const     { return 0; }
        uint32 KillingBlows; /**< TODO */
        uint32 Deaths; /**< TODO */
        uint32 HonorableKills; /**< TODO */
        uint32 DishonorableKills; /**< TODO */
        uint32 BonusHonor; /**< TODO */
};

/**
 * @brief This class is used to:
 * 1. Add player to battleground
 * 2. Remove player from battleground
 * 3. some certain cases, same for all battlegrounds
 * 4. It has properties same for all battlegrounds
 *
 */
class BattleGround
{
        friend class BattleGroundMgr;

    public:
        /**
         * @brief Construction
         *
         */
        BattleGround();
        /**
         * @brief
         *
         */
        virtual ~BattleGround();
        /**
         * @brief
         *
         * @param diff
         */
        virtual void Update(uint32 diff);                   // must be implemented in BG subclass of BG specific update code, but must in begginning call parent version
        /**
         * @brief
         *
         */
        virtual void Reset();                               // resets all common properties for battlegrounds, must be implemented and called in BG subclass
        /**
         * @brief
         *
         */
        virtual void StartingEventCloseDoors() {}
        /**
         * @brief
         *
         */
        virtual void StartingEventOpenDoors() {}

        /* Battleground */
        // Get methods:
        /**
         * @brief
         *
         * @return const char
         */
        char const* GetName() const         { return m_Name; }
        /**
         * @brief
         *
         * @return BattleGroundTypeId
         */
        BattleGroundTypeId GetTypeID() const { return m_TypeID; }
        /**
         * @brief
         *
         * @return BattleGroundBracketId
         */
        BattleGroundBracketId GetBracketId() const { return m_BracketId; }
        // the instanceId check is also used to determine a bg-template
        // that's why the m_map hack is here..
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetInstanceID()              { return m_Map ? GetBgMap()->GetInstanceId() : 0; }
        /**
         * @brief
         *
         * @return BattleGroundStatus
         */
        BattleGroundStatus GetStatus() const { return m_Status; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetClientInstanceID() const  { return m_ClientInstanceID; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetStartTime() const         { return m_StartTime; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetEndTime() const           { return m_EndTime; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMaxPlayers() const        { return m_MaxPlayers; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMinPlayers() const        { return m_MinPlayers; }

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMinLevel() const          { return m_LevelMin; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMaxLevel() const          { return m_LevelMax; }

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMaxPlayersPerTeam() const { return m_MaxPlayersPerTeam; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMinPlayersPerTeam() const { return m_MinPlayersPerTeam; }

        /**
         * @brief
         *
         * @return int32
         */
        int32 GetStartDelayTime() const     { return m_StartDelayTime; }
        /**
         * @brief
         *
         * @return Team
         */
        Team GetWinner() const              { return m_Winner; }
        /**
         * @brief
         *
         * @return Team
         */
        virtual Team GetPrematureWinner();
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetBattlemasterEntry() const;
        /**
         * @brief
         *
         * @param kills
         * @return uint32
         */
        uint32 GetBonusHonorFromKill(uint32 kills) const;

        // Set methods:
        /**
         * @brief
         *
         * @param Name
         */
        void SetName(char const* Name)      { m_Name = Name; }
        /**
         * @brief
         *
         * @param TypeID
         */
        void SetTypeID(BattleGroundTypeId TypeID) { m_TypeID = TypeID; }
        /**
         * @brief
         *
         * @param ID
         */
        void SetBracketId(BattleGroundBracketId ID) { m_BracketId = ID; }
        /**
         * @brief
         *
         * @param Status
         */
        void SetStatus(BattleGroundStatus Status) { m_Status = Status; }
        /**
         * @brief
         *
         * @param InstanceID
         */
        void SetClientInstanceID(uint32 InstanceID) { m_ClientInstanceID = InstanceID; }
        /**
         * @brief
         *
         * @param Time
         */
        void SetStartTime(uint32 Time)      { m_StartTime = Time; }
        /**
         * @brief
         *
         * @param Time
         */
        void SetEndTime(uint32 Time)        { m_EndTime = Time; }
        /**
         * @brief
         *
         * @param MaxPlayers
         */
        void SetMaxPlayers(uint32 MaxPlayers) { m_MaxPlayers = MaxPlayers; }
        /**
         * @brief
         *
         * @param MinPlayers
         */
        void SetMinPlayers(uint32 MinPlayers) { m_MinPlayers = MinPlayers; }
        /**
         * @brief
         *
         * @param min
         * @param max
         */
        void SetLevelRange(uint32 min, uint32 max) { m_LevelMin = min; m_LevelMax = max; }
        /**
         * @brief
         *
         * @param winner
         */
        void SetWinner(Team winner)         { m_Winner = winner; }

        /**
         * @brief
         *
         * @param diff
         */
        void ModifyStartDelayTime(int diff) { m_StartDelayTime -= diff; }
        /**
         * @brief
         *
         * @param Time
         */
        void SetStartDelayTime(int Time)    { m_StartDelayTime = Time; }

        /**
         * @brief
         *
         * @param MaxPlayers
         */
        void SetMaxPlayersPerTeam(uint32 MaxPlayers) { m_MaxPlayersPerTeam = MaxPlayers; }
        /**
         * @brief
         *
         * @param MinPlayers
         */
        void SetMinPlayersPerTeam(uint32 MinPlayers) { m_MinPlayersPerTeam = MinPlayers; }

        /**
         * @brief
         *
         */
        void AddToBGFreeSlotQueue();                        // this queue will be useful when more battlegrounds instances will be available
        /**
         * @brief
         *
         */
        void RemoveFromBGFreeSlotQueue();                   // this method could delete whole BG instance, if another free is available

        /**
         * @brief
         *
         * @param team
         */
        void DecreaseInvitedCount(Team team)      { (team == ALLIANCE) ? --m_InvitedAlliance : --m_InvitedHorde; }
        /**
         * @brief
         *
         * @param team
         */
        void IncreaseInvitedCount(Team team)      { (team == ALLIANCE) ? ++m_InvitedAlliance : ++m_InvitedHorde; }
        /**
         * @brief
         *
         * @param team
         * @return uint32
         */
        uint32 GetInvitedCount(Team team) const
        {
            if (team == ALLIANCE)
                { return m_InvitedAlliance; }
            else
                { return m_InvitedHorde; }
        }
        /**
         * @brief
         *
         * @return bool
         */
        bool HasFreeSlots() const;
        /**
         * @brief
         *
         * @param team
         * @return uint32
         */
        uint32 GetFreeSlotsForTeam(Team team) const;

        /**
         * @brief
         *
         */
        typedef std::map<ObjectGuid, BattleGroundPlayer> BattleGroundPlayerMap;
        /**
         * @brief
         *
         * @return const BattleGroundPlayerMap
         */
        BattleGroundPlayerMap const& GetPlayers() const { return m_Players; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetPlayersSize() const { return m_Players.size(); }

        /**
         * @brief
         *
         */
        typedef std::map<ObjectGuid, BattleGroundScore*> BattleGroundScoreMap;
        /**
         * @brief
         *
         * @return BattleGroundScoreMap::const_iterator
         */
        BattleGroundScoreMap::const_iterator GetPlayerScoresBegin() const { return m_PlayerScores.begin(); }
        /**
         * @brief
         *
         * @return BattleGroundScoreMap::const_iterator
         */
        BattleGroundScoreMap::const_iterator GetPlayerScoresEnd() const { return m_PlayerScores.end(); }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetPlayerScoresSize() const { return m_PlayerScores.size(); }

        /**
         * @brief
         *
         */
        void StartBattleGround();

        /* Location */
        /**
         * @brief
         *
         * @param MapID
         */
        void SetMapId(uint32 MapID) { m_MapId = MapID; }
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetMapId() const { return m_MapId; }

        /* Map pointers */
        /**
         * @brief
         *
         * @param map
         */
        void SetBgMap(BattleGroundMap* map) { m_Map = map; }
        /**
         * @brief
         *
         * @return BattleGroundMap
         */
        BattleGroundMap* GetBgMap()
        {
            MANGOS_ASSERT(m_Map);
            return m_Map;
        }

        /**
         * @brief
         *
         * @param team
         * @param X
         * @param Y
         * @param Z
         * @param O
         */
        void SetTeamStartLoc(Team team, float X, float Y, float Z, float O);
        /**
         * @brief
         *
         * @param team
         * @param X
         * @param Y
         * @param Z
         * @param O
         */
        void GetTeamStartLoc(Team team, float& X, float& Y, float& Z, float& O) const
        {
            PvpTeamIndex idx = GetTeamIndexByTeamId(team);
            X = m_TeamStartLocX[idx];
            Y = m_TeamStartLocY[idx];
            Z = m_TeamStartLocZ[idx];
            O = m_TeamStartLocO[idx];
        }

        void SetStartMaxDist(float startMaxDist) { m_startMaxDist = startMaxDist; }
        float GetStartMaxDist() const { return m_startMaxDist; }

        /* Packet Transfer */
        // method that should fill worldpacket with actual world states (not yet implemented for all battlegrounds!)
        /**
         * @brief
         *
         * @param
         * @param
         */
        virtual void FillInitialWorldStates(WorldPacket& /*data*/, uint32& /*count*/) {}
        /**
         * @brief
         *
         * @param team
         * @param packet
         * @param sender
         * @param self
         */
        void SendPacketToTeam(Team team, WorldPacket* packet, Player* sender = NULL, bool self = true);
        /**
         * @brief
         *
         * @param packet
         */
        void SendPacketToAll(WorldPacket* packet);

        template<class Do>
        /**
         * @brief
         *
         * @param _do
         */
        void BroadcastWorker(Do& _do);

        /**
         * @brief
         *
         * @param SoundID
         * @param team
         */
        void PlaySoundToTeam(uint32 SoundID, Team team);
        /**
         * @brief
         *
         * @param SoundID
         */
        void PlaySoundToAll(uint32 SoundID);
        /**
         * @brief
         *
         * @param SpellID
         * @param team
         */
        void CastSpellOnTeam(uint32 SpellID, Team team);
        /**
         * @brief
         *
         * @param Honor
         * @param team
         */
        void RewardHonorToTeam(uint32 Honor, Team team);
        /**
         * @brief
         *
         * @param faction_id
         * @param Reputation
         * @param team
         */
        void RewardReputationToTeam(uint32 faction_id, uint32 Reputation, Team team);
        /**
         * @brief
         *
         * @param plr
         * @param count
         */
        void RewardMark(Player* plr, uint32 count);
        /**
         * @brief
         *
         * @param plr
         * @param mark
         * @param count
         */
        void SendRewardMarkByMail(Player* plr, uint32 mark, uint32 count);
        /**
         * @brief
         *
         * @param plr
         * @param item_id
         * @param count
         */
        void RewardItem(Player* plr, uint32 item_id, uint32 count);
        /**
         * @brief
         *
         * @param plr
         */
        void RewardQuestComplete(Player* plr);
        /**
         * @brief
         *
         * @param plr
         * @param spell_id
         */
        void RewardSpellCast(Player* plr, uint32 spell_id);
        /**
         * @brief
         *
         * @param Field
         * @param Value
         */
        void UpdateWorldState(uint32 Field, uint32 Value);
        /**
         * @brief
         *
         * @param Field
         * @param Value
         * @param Source
         */
        void UpdateWorldStateForPlayer(uint32 Field, uint32 Value, Player* Source);
        /**
         * @brief
         *
         * @param winner
         */
        virtual void EndBattleGround(Team winner);
        /**
         * @brief
         *
         * @param plr
         */
        void BlockMovement(Player* plr);

        /**
         * @brief
         *
         * @param entry
         * @param type
         * @param source
         */
        void SendMessageToAll(int32 entry, ChatMsg type, Player const* source = NULL);
        /**
         * @brief
         *
         * @param entry
         * @param language
         * @param guid
         */
        void SendYellToAll(int32 entry, uint32 language, ObjectGuid guid);
        /**
         * @brief
         *
         * @param entry
         * @param type
         * @param source...
         */
        void PSendMessageToAll(int32 entry, ChatMsg type, Player const* source, ...);

        // specialized version with 2 string id args
        /**
         * @brief
         *
         * @param entry
         * @param type
         * @param source
         * @param strId1
         * @param strId2
         */
        void SendMessage2ToAll(int32 entry, ChatMsg type, Player const* source, int32 strId1 = 0, int32 strId2 = 0);
        /**
         * @brief
         *
         * @param entry
         * @param language
         * @param guid
         * @param arg1
         * @param arg2
         */
        void SendYell2ToAll(int32 entry, uint32 language, ObjectGuid guid, int32 arg1, int32 arg2);

        /* Raid Group */
        /**
         * @brief
         *
         * @param team
         * @return Group
         */
        Group* GetBgRaid(Team team) const { return m_BgRaids[GetTeamIndexByTeamId(team)]; }
        /**
         * @brief
         *
         * @param team
         * @param bg_raid
         */
        void SetBgRaid(Team team, Group* bg_raid);

        /**
         * @brief
         *
         * @param Source
         * @param type
         * @param value
         */
        virtual void UpdatePlayerScore(Player* Source, uint32 type, uint32 value);

        /**
         * @brief
         *
         * @param team
         * @return BattleGroundTeamIndex
         */
        static PvpTeamIndex GetTeamIndexByTeamId(Team team) { return team == ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE; }
        /**
         * @brief
         *
         * @param team
         * @return uint32
         */
        uint32 GetPlayersCountByTeam(Team team) const { return m_PlayersCount[GetTeamIndexByTeamId(team)]; }
        /**
         * @brief
         *
         * @param team
         * @return uint32
         */
        uint32 GetAlivePlayersCountByTeam(Team team) const; // used in arenas to correctly handle death in spirit of redemption / last stand etc. (killer = killed) cases
        /**
         * @brief
         *
         * @param team
         * @param remove
         */
        void UpdatePlayersCountByTeam(Team team, bool remove)
        {
            if (remove)
                { --m_PlayersCount[GetTeamIndexByTeamId(team)]; }
            else
                { ++m_PlayersCount[GetTeamIndexByTeamId(team)]; }
        }

        /* Triggers handle */
        // must be implemented in BG subclass
        /**
         * @brief
         *
         * @param
         * @param uint32
         */
        virtual bool HandleAreaTrigger(Player* /*Source*/, uint32 /*Trigger*/) { return false;  }
        // must be implemented in BG subclass if need AND call base class generic code
        /**
         * @brief
         *
         * @param player
         * @param killer
         */
        virtual void HandleKillPlayer(Player* player, Player* killer);
        /**
         * @brief
         *
         * @param
         * @param
         */
        virtual void HandleKillUnit(Creature* /*unit*/, Player* /*killer*/) {}

        // Process Capture event
        /**
         * @brief
         *
         * @param uint32
         * @param
         * @return bool
         */
        virtual bool HandleEvent(uint32 /*eventId*/, GameObject* /*go*/) { return false; }

        // Called when a gameobject is created
        /**
         * @brief
         *
         * @param
         */
        virtual void HandleGameObjectCreate(GameObject* /*go*/) {}

        /* Battleground events */
        /**
         * @brief
         *
         * @param
         */
        virtual void EventPlayerDroppedFlag(Player* /*player*/) {}
        /**
         * @brief
         *
         * @param
         * @param
         */
        virtual void EventPlayerClickedOnFlag(Player* /*player*/, GameObject* /*target_obj*/) {}
        /**
         * @brief
         *
         * @param
         */
        virtual void EventPlayerCapturedFlag(Player* /*player*/) {}
        /**
         * @brief
         *
         * @param player
         */
        void EventPlayerLoggedIn(Player* player);
        /**
         * @brief
         *
         * @param player
         */
        void EventPlayerLoggedOut(Player* player);

        /* Death related */
        /**
         * @brief
         *
         * @param player
         * @return const WorldSafeLocsEntry
         */
        virtual WorldSafeLocsEntry const* GetClosestGraveYard(Player* player);

        /**
         * @brief
         *
         * @param plr
         */
        virtual void AddPlayer(Player* plr);                // must be implemented in BG subclass

        /**
         * @brief
         *
         * @param plr
         * @param plr_guid
         * @param team
         */
        void AddOrSetPlayerToCorrectBgGroup(Player* plr, ObjectGuid plr_guid, Team team);

        /**
         * @brief
         *
         * @param guid
         * @param Transport
         * @param SendPacket
         */
        virtual void RemovePlayerAtLeave(ObjectGuid guid, bool Transport, bool SendPacket);
        // can be extended in in BG subclass

        /* event related */
        // called when a creature gets added to map (NOTE: only triggered if
        // a player activates the cell of the creature)
        /**
         * @brief
         *
         * @param
         */
        void OnObjectDBLoad(Creature* /*creature*/);
        /**
         * @brief
         *
         * @param
         */
        void OnObjectDBLoad(GameObject* /*obj*/);
        // (de-)spawns creatures and gameobjects from an event
        /**
         * @brief
         *
         * @param event1
         * @param event2
         * @param spawn
         */
        void SpawnEvent(uint8 event1, uint8 event2, bool spawn);
        /**
         * @brief
         *
         * @param event1
         * @param event2
         * @return bool
         */
        bool IsActiveEvent(uint8 event1, uint8 event2)
        {
            if (m_ActiveEvents.find(event1) == m_ActiveEvents.end())
                { return false; }
            return m_ActiveEvents[event1] == event2;
        }
        /**
         * @brief
         *
         * @param event1
         * @param event2
         * @return ObjectGuid
         */
        ObjectGuid GetSingleCreatureGuid(uint8 event1, uint8 event2);

        /**
         * @brief
         *
         * @param event1
         * @param event2
         */
        void OpenDoorEvent(uint8 event1, uint8 event2 = 0);
        /**
         * @brief
         *
         * @param event1
         * @param event2
         * @return bool
         */
        bool IsDoor(uint8 event1, uint8 event2);

        /**
         * @brief
         *
         * @param go_guid
         */
        void HandleTriggerBuff(ObjectGuid go_guid);

        /**
         * @brief
         *
         * @param guid
         * @param respawntime
         */
        void SpawnBGObject(ObjectGuid guid, uint32 respawntime);
        /**
         * @brief
         *
         * @param guid
         * @param respawntime
         */
        void SpawnBGCreature(ObjectGuid guid, uint32 respawntime);

        /**
         * @brief
         *
         * @param guid
         */
        void DoorOpen(ObjectGuid guid);
        /**
         * @brief
         *
         * @param guid
         */
        void DoorClose(ObjectGuid guid);

        /**
         * @brief
         *
         * @param
         * @return bool
         */
        virtual bool HandlePlayerUnderMap(Player* /*plr*/) { return false; }

        // since arenas can be AvA or Hvh, we have to get the "temporary" team of a player
        /**
         * @brief
         *
         * @param guid
         * @return Team
         */
        Team GetPlayerTeam(ObjectGuid guid);
        /**
         * @brief
         *
         * @param team
         * @return Team
         */
        static Team GetOtherTeam(Team team) { return team ? ((team == ALLIANCE) ? HORDE : ALLIANCE) : TEAM_NONE; }
        /**
         * @brief
         *
         * @param teamIdx
         * @return BattleGroundTeamIndex
         */
        static PvpTeamIndex GetOtherTeamIndex(PvpTeamIndex teamIdx) { return teamIdx == TEAM_INDEX_ALLIANCE ? TEAM_INDEX_HORDE : TEAM_INDEX_ALLIANCE; }
        /**
         * @brief
         *
         * @param guid
         * @return bool
         */
        bool IsPlayerInBattleGround(ObjectGuid guid);

        /* virtual score-array - get's used in bg-subclasses */
        int32 m_TeamScores[PVP_TEAM_COUNT];

        /**
         * @brief
         *
         */
        struct EventObjects
        {
            GuidVector gameobjects; /**< TODO */
            GuidVector creatures; /**< TODO */
        };

        // cause we create it dynamicly i use a map - to avoid resizing when
        // using vector - also it contains 2*events concatenated with PAIR32
        // this is needed to avoid overhead of a 2dimensional std::map
        std::map<uint32, EventObjects> m_EventObjects; /**< TODO */
        // this must be filled first in BattleGroundXY::Reset().. else
        // creatures will get added wrong
        // door-events are automaticly added - but _ALL_ other must be in this vector
        std::map<uint8, uint8> m_ActiveEvents; /**< TODO */


    protected:
        /**
         * @brief Ends a battleground
         *
         * This method is called, when BG can not spawn its own spirit guide, or
         * something is wrong, It correctly ends BattleGround
         */
        void EndNow();
        /**
         * @brief
         *
         * @param plr
         */
        void PlayerAddedToBGCheckIfBGIsRunning(Player* plr);

        /* Scorekeeping */

        BattleGroundScoreMap m_PlayerScores;                /**< Player scores */
        // must be implemented in BG subclass
        /**
         * @brief
         *
         * @param
         * @param ObjectGuid
         */
        virtual void RemovePlayer(Player* /*player*/, ObjectGuid /*guid*/) {}

        /* Player lists, those need to be accessible by inherited classes */
        BattleGroundPlayerMap  m_Players; /**< TODO */

        /*
        these are important variables used for starting messages
        */
        uint8 m_Events; /**< TODO */
        BattleGroundStartTimeIntervals  m_StartDelayTimes[BG_STARTING_EVENT_COUNT]; /**< TODO */
        // this must be filled in constructors!
        uint32 m_StartMessageIds[BG_STARTING_EVENT_COUNT]; /**< TODO */

        bool   m_BuffChange; /**< TODO */

    private:
        /* Battleground */
        BattleGroundTypeId m_TypeID; /**< TODO */
        BattleGroundStatus m_Status; /**< TODO */
        uint32 m_ClientInstanceID;                          /**< the instance-id which is sent to the client and without any other internal use */
        uint32 m_StartTime; /**< TODO */
        uint32 m_validStartPositionTimer;
        int32 m_EndTime;                                    /**< it is set to 120000 when bg is ending and it decreases itself */
        BattleGroundBracketId m_BracketId; /**< TODO */
        bool   m_InBGFreeSlotQueue;                         /**< used to make sure that BG is only once inserted into the BattleGroundMgr.BGFreeSlotQueue[bgTypeId] deque */
        Team   m_Winner;                                    /**< 0=alliance, 1=horde, 2=none */
        int32  m_StartDelayTime; /**< TODO */
        bool   m_PrematureCountDown; /**< TODO */
        uint32 m_PrematureCountDownTimer; /**< TODO */
        char const* m_Name; /**< TODO */

        /* Player lists */
        /**
         * @brief
         *
         */
        typedef std::deque<ObjectGuid> OfflineQueue;
        OfflineQueue m_OfflineQueue;                        /**< Player GUID */

        /* Invited counters are useful for player invitation to BG - do not allow, if BG is started to one faction to have 2 more players than another faction */
        /* Invited counters will be changed only when removing already invited player from queue, removing player from battleground and inviting player to BG */
        /* Invited players counters*/
        uint32 m_InvitedAlliance; /**< TODO */
        uint32 m_InvitedHorde; /**< TODO */

        /* Raid Group */
        Group* m_BgRaids[PVP_TEAM_COUNT];                   /**< 0 - alliance, 1 - horde */

        /* Players count by team */
        uint32 m_PlayersCount[PVP_TEAM_COUNT]; /**< TODO */

        /* Limits */
        uint32 m_LevelMin; /**< TODO */
        uint32 m_LevelMax; /**< TODO */
        uint32 m_MaxPlayersPerTeam; /**< TODO */
        uint32 m_MaxPlayers; /**< TODO */
        uint32 m_MinPlayersPerTeam; /**< TODO */
        uint32 m_MinPlayers; /**< TODO */

        /* Start location */
        uint32 m_MapId; /**< TODO */
        BattleGroundMap* m_Map; /**< TODO */
        float m_startMaxDist;
        float m_TeamStartLocX[PVP_TEAM_COUNT]; /**< TODO */
        float m_TeamStartLocY[PVP_TEAM_COUNT]; /**< TODO */
        float m_TeamStartLocZ[PVP_TEAM_COUNT]; /**< TODO */
        float m_TeamStartLocO[PVP_TEAM_COUNT]; /**< TODO */
};

// helper functions for world state list fill
/**
 * @brief
 *
 * @param data
 * @param count
 * @param state
 * @param value
 */
inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, uint32 value)
{
    data << uint32(state);
    data << uint32(value);
    ++count;
}

/**
 * @brief
 *
 * @param data
 * @param count
 * @param state
 * @param value
 */
inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, int32 value)
{
    data << uint32(state);
    data << int32(value);
    ++count;
}

/**
 * @brief
 *
 * @param data
 * @param count
 * @param state
 * @param value
 */
inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, bool value)
{
    data << uint32(state);
    data << uint32(value ? 1 : 0);
    ++count;
}

/**
 * @brief
 *
 */
struct WorldStatePair
{
    uint32 state; /**< TODO */
    uint32 value; /**< TODO */
};

/**
 * @brief
 *
 * @param data
 * @param count
 * @param array
 */
inline void FillInitialWorldState(ByteBuffer& data, uint32& count, WorldStatePair const* array)
{
    for (WorldStatePair const* itr = array; itr->state; ++itr)
    {
        data << uint32(itr->state);
        data << uint32(itr->value);
        ++count;
    }
}

#endif
