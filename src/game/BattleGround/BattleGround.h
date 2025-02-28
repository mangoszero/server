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
 * @brief Structure to hold battleground event indices.
 */
struct BattleGroundEventIdx
{
    uint8 event1; /**< First event index */
    uint8 event2; /**< Second event index */
};

/**
 * @brief Enum for battleground sounds.
 */
enum BattleGroundSounds
{
    SOUND_HORDE_WINS = 8454,
    SOUND_ALLIANCE_WINS = 8455,
    SOUND_BG_START = 3439
};

/**
 * @brief Enum for battleground quests.
 */
enum BattleGroundQuests
{
    SPELL_WS_QUEST_REWARD = 43483,
    SPELL_AB_QUEST_REWARD = 43484,
    SPELL_AV_QUEST_REWARD = 43475,
    SPELL_AV_QUEST_KILLED_BOSS = 23658,
    SPELL_AB_QUEST_REWARD_4_BASES = 24061,
    SPELL_AB_QUEST_REWARD_5_BASES = 24064
};

/**
 * @brief Enum for battleground marks.
 */
enum BattleGroundMarks
{
    SPELL_WS_MARK_LOSER = 24950,
    SPELL_WS_MARK_WINNER = 24951,
    SPELL_AB_MARK_LOSER = 24952,
    SPELL_AB_MARK_WINNER = 24953,
    SPELL_AV_MARK_LOSER = 24954,
    SPELL_AV_MARK_WINNER = 24955,
};

/**
 * @brief Enum for battleground marks count.
 */
enum BattleGroundMarksCount
{
    ITEM_WINNER_COUNT = 3,
    ITEM_LOSER_COUNT = 1
};

/**
 * @brief Enum for battleground time intervals.
 */
enum BattleGroundTimeIntervals
{
    CHECK_PLAYER_POSITION_INVERVAL = 1000,  // ms
    RESURRECTION_INTERVAL = 30000,          // ms
    INVITATION_REMIND_TIME = 60000,         // ms
    INVITE_ACCEPT_WAIT_TIME = 80000,        // ms
    TIME_TO_AUTOREMOVE = 120000,            // ms
    MAX_OFFLINE_TIME = 300,                 // secs
    RESPAWN_ONE_DAY = 86400,                // secs
    RESPAWN_IMMEDIATELY = 0,                // secs
    BUFF_RESPAWN_TIME = 180                 // secs
};

/**
 * @brief Enum for battleground start time intervals.
 */
enum BattleGroundStartTimeIntervals
{
    BG_START_DELAY_2M = 120000,             // ms (2 minutes)
    BG_START_DELAY_1M = 60000,              // ms (1 minute)
    BG_START_DELAY_30S = 30000,             // ms (30 seconds)
    BG_START_DELAY_NONE = 0,                // ms
};

/**
 * @brief Enum for battleground buff objects.
 */
enum BattleGroundBuffObjects
{
    BG_OBJECTID_SPEEDBUFF_ENTRY = 179871,
    BG_OBJECTID_REGENBUFF_ENTRY = 179904,
    BG_OBJECTID_BERSERKERBUFF_ENTRY = 179905
};

const uint32 Buff_Entries[3] = { BG_OBJECTID_SPEEDBUFF_ENTRY, BG_OBJECTID_REGENBUFF_ENTRY, BG_OBJECTID_BERSERKERBUFF_ENTRY }; /**< Array of buff entries */

/**
 * @brief Enum for battleground status.
 */
enum BattleGroundStatus
{
    STATUS_NONE = 0,                                // first status, should mean bg is not instance
    STATUS_WAIT_QUEUE = 1,                                // means bg is empty and waiting for queue
    STATUS_WAIT_JOIN = 2,                                // this means, that BG has already started and it is waiting for more players
    STATUS_IN_PROGRESS = 3,                                // means bg is running
    STATUS_WAIT_LEAVE = 4                                 // means some faction has won BG and it is ending
};

/**
 * @brief Structure to hold battleground player information.
 */
struct BattleGroundPlayer
{
    time_t  OfflineRemoveTime;                              /**< for tracking and removing offline players from queue after 5 minutes */
    Team    PlayerTeam;                                     /**< Player's team */
};

/**
 * @brief Structure to hold battleground object information.
 */
struct BattleGroundObjectInfo
{
    BattleGroundObjectInfo() : object(NULL), timer(0), spellid(0) {}

    GameObject* object; /**< Pointer to the game object */
    int32       timer; /**< Timer for the object */
    uint32      spellid; /**< Spell ID associated with the object */
};

/**
 * @brief Enum for battleground queue types.
 * Handle the queue types and bg types separately to enable joining queue for different sized arenas at the same time.
 */
enum BattleGroundQueueTypeId
{
    BATTLEGROUND_QUEUE_NONE = 0,
    BATTLEGROUND_QUEUE_AV = 1,
    BATTLEGROUND_QUEUE_WS = 2,
    BATTLEGROUND_QUEUE_AB = 3,
};
#define MAX_BATTLEGROUND_QUEUE_TYPES 4

/**
 * @brief Enum for battleground bracket IDs.
 * Bracket ID for level ranges.
 */
enum BattleGroundBracketId
{
    BG_BRACKET_ID_TEMPLATE = -1,                      // used to mark bg as template
    BG_BRACKET_ID_FIRST = 0,                       // brackets start from specific BG min level and each include 10 levels range
    BG_BRACKET_ID_LAST = 5                        // so for start level 10 will be 10-19, 20-29, ...  all greater max bg level included in last bracket
};

#define MAX_BATTLEGROUND_BRACKETS  6

/**
 * @brief Enum for score types.
 */
enum ScoreType
{
    SCORE_KILLING_BLOWS = 1,
    SCORE_DEATHS = 2,
    SCORE_HONORABLE_KILLS = 3,
    SCORE_BONUS_HONOR = 4,
    // WS
    SCORE_FLAG_CAPTURES = 7,
    SCORE_FLAG_RETURNS = 8,
    // AB
    SCORE_BASES_ASSAULTED = 9,
    SCORE_BASES_DEFENDED = 10,
    // AV
    SCORE_GRAVEYARDS_ASSAULTED = 11,
    SCORE_GRAVEYARDS_DEFENDED = 12,
    SCORE_TOWERS_ASSAULTED = 13,
    SCORE_TOWERS_DEFENDED = 14,
    SCORE_SECONDARY_OBJECTIVES = 15
};

/**
 * @brief Enum for battleground starting events.
 */
enum BattleGroundStartingEvents
{
    BG_STARTING_EVENT_NONE = 0x00,
    BG_STARTING_EVENT_1 = 0x01,
    BG_STARTING_EVENT_2 = 0x02,
    BG_STARTING_EVENT_3 = 0x04,
    BG_STARTING_EVENT_4 = 0x08
};

/**
 * @brief Enum for battleground starting event IDs.
 */
enum BattleGroundStartingEventsIds
{
    BG_STARTING_EVENT_FIRST = 0,
    BG_STARTING_EVENT_SECOND = 1,
    BG_STARTING_EVENT_THIRD = 2,
    BG_STARTING_EVENT_FOURTH = 3
};
#define BG_STARTING_EVENT_COUNT 4

/**
 * @brief Enum for battleground join errors.
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
 * @brief Class to hold battleground score information.
 */
class BattleGroundScore
{
public:
    /**
     * @brief Constructor for BattleGroundScore.
     */
    BattleGroundScore() : KillingBlows(0), Deaths(0), HonorableKills(0),
        DishonorableKills(0), BonusHonor(0)
        {}
    /**
     * @brief Virtual destructor for BattleGroundScore.
     * Used when deleting score from scores map.
     */
    virtual ~BattleGroundScore() {}

    uint32 GetKillingBlows() const { return KillingBlows; }
    uint32 GetDeaths() const { return Deaths; }
    uint32 GetHonorableKills() const { return HonorableKills; }
    uint32 GetBonusHonor() const { return BonusHonor; }
    uint32 GetDamageDone() const { return 0; }
    uint32 GetHealingDone() const { return 0; }

    virtual uint32 GetAttr1() const { return 0; }
    virtual uint32 GetAttr2() const { return 0; }
    virtual uint32 GetAttr3() const { return 0; }
    virtual uint32 GetAttr4() const { return 0; }
    virtual uint32 GetAttr5() const { return 0; }
    uint32 KillingBlows; /**< Number of killing blows */
    uint32 Deaths; /**< Number of deaths */
    uint32 HonorableKills; /**< Number of honorable kills */
    uint32 DishonorableKills; /**< Number of dishonorable kills */
    uint32 BonusHonor; /**< Amount of bonus honor */
};

/**
 * @brief This class is used to:
 * 1. Add player to battleground
 * 2. Remove player from battleground
 * 3. Handle certain cases, same for all battlegrounds
 * 4. It has properties same for all battlegrounds
 */
class BattleGround
{
    friend class BattleGroundMgr;

public:
    /**
     * @brief Constructor for BattleGround.
     */
    BattleGround();
    /**
     * @brief Destructor for BattleGround.
     */
    virtual ~BattleGround();
    /**
     * @brief Updates the battleground.
     * Must be implemented in BG subclass of BG specific update code, but must in beginning call parent version.
     *
     * @param diff Time difference since last update.
     */
    virtual void Update(uint32 diff);
    /**
     * @brief Resets all common properties for battlegrounds.
     * Must be implemented and called in BG subclass.
     */
    virtual void Reset();
    /**
     * @brief Closes the doors at the start of the battleground.
     */
    virtual void StartingEventCloseDoors() {}
    /**
     * @brief Opens the doors at the start of the battleground.
     */
    virtual void StartingEventOpenDoors() {}

    /* Battleground */
    // Get methods:
    /**
     * @brief Gets the name of the battleground.
     *
     * @return const char* Name of the battleground.
     */
    char const* GetName() const { return m_Name; }
    /**
     * @brief Gets the type ID of the battleground.
     *
     * @return BattleGroundTypeId Type ID of the battleground.
     */
    BattleGroundTypeId GetTypeID() const
    {
        return m_TypeID;
    }
    /**
     * @brief Gets the bracket ID of the battleground.
     *
     * @return BattleGroundBracketId Bracket ID of the battleground.
     */
    BattleGroundBracketId GetBracketId() const { return m_BracketId; }
    // the instanceId check is also used to determine a bg-template
    // that's why the m_map hack is here..
    /**
     * @brief Gets the instance ID of the battleground.
     *
     * @return uint32 Instance ID of the battleground.
     */
    uint32 GetInstanceID() { return m_Map ? GetBgMap()->GetInstanceId() : 0; }
    /**
     * @brief Gets the status of the battleground.
     *
     * @return BattleGroundStatus Status of the battleground.
     */
    BattleGroundStatus GetStatus() const { return m_Status; }
    /**
     * @brief Gets the client instance ID of the battleground.
     *
     * @return uint32 Client instance ID of the battleground.
     */
    uint32 GetClientInstanceID() const { return m_ClientInstanceID; }
    /**
     * @brief Gets the start time of the battleground.
     *
     * @return uint32 Start time of the battleground.
     */
    uint32 GetStartTime() const { return m_StartTime; }
    /**
     * @brief Gets the end time of the battleground.
     *
     * @return uint32 End time of the battleground.
     */
    uint32 GetEndTime() const { return m_EndTime; }
    /**
     * @brief Gets the maximum number of players in the battleground.
     *
     * @return uint32 Maximum number of players.
     */
    uint32 GetMaxPlayers() const { return m_MaxPlayers; }
    /**
     * @brief Gets the minimum number of players in the battleground.
     *
     * @return uint32 Minimum number of players.
     */
    uint32 GetMinPlayers() const { return m_MinPlayers; }

    /**
     * @brief Gets the minimum level required to join the battleground.
     *
     * @return uint32 Minimum level.
     */
    uint32 GetMinLevel() const { return m_LevelMin; }
    /**
     * @brief Gets the maximum level allowed in the battleground.
     *
     * @return uint32 Maximum level.
     */
    uint32 GetMaxLevel() const { return m_LevelMax; }

    /**
     * @brief Gets the maximum number of players per team in the battleground.
     *
     * @return uint32 Maximum number of players per team.
     */
    uint32 GetMaxPlayersPerTeam() const { return m_MaxPlayersPerTeam; }
    /**
     * @brief Gets the minimum number of players per team in the battleground.
     *
     * @return uint32 Minimum number of players per team.
     */
    uint32 GetMinPlayersPerTeam() const { return m_MinPlayersPerTeam; }

    /**
     * @brief Gets the start delay time of the battleground.
     *
     * @return int32 Start delay time.
     */
    int32 GetStartDelayTime() const { return m_StartDelayTime; }
    /**
     * @brief Gets the winner of the battleground.
     *
     * @return Team Winner of the battleground.
     */
    Team GetWinner() const { return m_Winner; }
    /**
     * @brief Gets the premature winner of the battleground.
     *
     * @return Team Premature winner of the battleground.
     */
    virtual Team GetPrematureWinner();
    /**
     * @brief Gets the battlemaster entry of the battleground.
     *
     * @return uint32 Battlemaster entry.
     */
    uint32 GetBattlemasterEntry() const;
    /**
     * @brief Gets the bonus honor from a kill.
     *
     * @param kills Number of kills.
     * @return uint32 Bonus honor.
     */
    uint32 GetBonusHonorFromKill(uint32 kills) const;

    // Set methods:
    /**
     * @brief Sets the name of the battleground.
     *
     * @param Name Name of the battleground.
     */
    void SetName(char const* Name) { m_Name = Name; }
    /**
     * @brief Sets the type ID of the battleground.
     *
     * @param TypeID Type ID of the battleground.
     */
    void SetTypeID(BattleGroundTypeId TypeID) { m_TypeID = TypeID; }
    /**
     * @brief Sets the bracket ID of the battleground.
     *
     * @param ID Bracket ID of the battleground.
     */
    void SetBracketId(BattleGroundBracketId ID) { m_BracketId = ID; }
    /**
     * @brief Sets the status of the battleground.
     *
     * @param Status Status of the battleground.
     */
    void SetStatus(BattleGroundStatus Status) { m_Status = Status; }
    /**
     * @brief Sets the client instance ID of the battleground.
     *
     * @param InstanceID Client instance ID.
     */
    void SetClientInstanceID(uint32 InstanceID) { m_ClientInstanceID = InstanceID; }
    /**
     * @brief Sets the start time of the battleground.
     *
     * @param Time Start time.
     */
    void SetStartTime(uint32 Time) { m_StartTime = Time; }
    /**
     * @brief Sets the end time of the battleground.
     *
     * @param Time End time.
     */
    void SetEndTime(uint32 Time) { m_EndTime = Time; }
    /**
     * @brief Sets the maximum number of players in the battleground.
     *
     * @param MaxPlayers Maximum number of players.
     */
    void SetMaxPlayers(uint32 MaxPlayers) { m_MaxPlayers = MaxPlayers; }
    /**
     * @brief Sets the minimum number of players in the battleground.
     *
     * @param MinPlayers Minimum number of players.
     */
    void SetMinPlayers(uint32 MinPlayers) { m_MinPlayers = MinPlayers; }

    /**
     * @brief Sets the level range for the battleground.
     *
     * @param min Minimum level.
     * @param max Maximum level.
     */
    void SetLevelRange(uint32 min, uint32 max) { m_LevelMin = min; m_LevelMax = max; }

    /**
     * @brief Sets the winner of the battleground.
     *
     * @param winner The winning team.
     */
    void SetWinner(Team winner) { m_Winner = winner; }

    /**
     * @brief Modifies the start delay time of the battleground.
     *
     * @param diff The amount to modify the start delay time by.
     */
    void ModifyStartDelayTime(int diff) { m_StartDelayTime -= diff; }

    /**
     * @brief Sets the start delay time of the battleground.
     *
     * @param Time The start delay time.
     */
    void SetStartDelayTime(int Time) { m_StartDelayTime = Time; }

    /**
     * @brief Sets the maximum number of players per team in the battleground.
     *
     * @param MaxPlayers Maximum number of players per team.
     */
    void SetMaxPlayersPerTeam(uint32 MaxPlayers) { m_MaxPlayersPerTeam = MaxPlayers; }

    /**
     * @brief Sets the minimum number of players per team in the battleground.
     *
     * @param MinPlayers Minimum number of players per team.
     */
    void SetMinPlayersPerTeam(uint32 MinPlayers) { m_MinPlayersPerTeam = MinPlayers; }

    /**
     * @brief Adds the battleground to the free slot queue.
     * This queue will be useful when more battleground instances are available.
     */
    void AddToBGFreeSlotQueue();

    /**
     * @brief Removes the battleground from the free slot queue.
     * This method could delete the whole BG instance if another free instance is available.
     */
    void RemoveFromBGFreeSlotQueue();

    /**
     * @brief Decreases the invited count for the specified team.
     *
     * @param team The team to decrease the invited count for.
     */
    void DecreaseInvitedCount(Team team) { (team == ALLIANCE) ? --m_InvitedAlliance : --m_InvitedHorde; }

    /**
     * @brief Increases the invited count for the specified team.
     *
     * @param team The team to increase the invited count for.
     */
    void IncreaseInvitedCount(Team team) { (team == ALLIANCE) ? ++m_InvitedAlliance : ++m_InvitedHorde; }

    /**
     * @brief Gets the invited count for the specified team.
     *
     * @param team The team to get the invited count for.
     * @return uint32 The invited count for the team.
     */
    uint32 GetInvitedCount(Team team) const
    {
        if (team == ALLIANCE)
        {
            return m_InvitedAlliance;
        }
        else
        {
            return m_InvitedHorde;
        }
    }

    /**
     * @brief Checks if the battleground has free slots.
     *
     * @return bool True if the battleground has free slots, false otherwise.
     */
    bool HasFreeSlots() const;

    /**
     * @brief Gets the number of free slots for the specified team.
     *
     * @param team The team to get the free slots for.
     * @return uint32 The number of free slots for the team.
     */
    uint32 GetFreeSlotsForTeam(Team team) const;

    /**
     * @brief Map to hold battleground player information.
     */
    typedef std::map<ObjectGuid, BattleGroundPlayer> BattleGroundPlayerMap;

    /**
     * @brief Gets the players in the battleground.
     *
     * @return const BattleGroundPlayerMap& The players in the battleground.
     */
    BattleGroundPlayerMap const& GetPlayers() const { return m_Players; }

    /**
     * @brief Gets the number of players in the battleground.
     *
     * @return uint32 The number of players in the battleground.
     */
    uint32 GetPlayersSize() const { return m_Players.size(); }

    /**
     * @brief Map to hold battleground score information.
     */
    typedef std::map<ObjectGuid, BattleGroundScore*> BattleGroundScoreMap;

    /**
     * @brief Gets the beginning iterator for the player scores.
     *
     * @return BattleGroundScoreMap::const_iterator The beginning iterator for the player scores.
     */
    BattleGroundScoreMap::const_iterator GetPlayerScoresBegin() const { return m_PlayerScores.begin(); }

    /**
     * @brief Gets the ending iterator for the player scores.
     *
     * @return BattleGroundScoreMap::const_iterator The ending iterator for the player scores.
     */
    BattleGroundScoreMap::const_iterator GetPlayerScoresEnd() const { return m_PlayerScores.end(); }

    /**
     * @brief Gets the number of player scores in the battleground.
     *
     * @return uint32 The number of player scores in the battleground.
     */
    uint32 GetPlayerScoresSize() const { return m_PlayerScores.size(); }

    /**
     * @brief Starts the battleground.
     */
    void StartBattleGround();

    /* Location */
    /**
     * @brief Sets the map ID for the battleground.
     *
     * @param MapID The map ID.
     */
    void SetMapId(uint32 MapID) { m_MapId = MapID; }

    /**
     * @brief Gets the map ID for the battleground.
     *
     * @return uint32 The map ID.
     */
    uint32 GetMapId() const { return m_MapId; }

    /* Map pointers */
    /**
     * @brief Sets the battleground map.
     *
     * @param map The battleground map.
     */
    void SetBgMap(BattleGroundMap* map) { m_Map = map; }

    /**
     * @brief Gets the battleground map.
     *
     * @return BattleGroundMap* The battleground map.
     */
    BattleGroundMap* GetBgMap()
    {
        MANGOS_ASSERT(m_Map);
        return m_Map;
    }

    /**
     * @brief Sets the start location for the specified team.
     *
     * @param team The team.
     * @param X The X coordinate.
     * @param Y The Y coordinate.
     * @param Z The Z coordinate.
     * @param O The orientation.
     */
    void SetTeamStartLoc(Team team, float X, float Y, float Z, float O);

    /**
     * @brief Gets the start location for the specified team.
     *
     * @param team The team.
     * @param X The X coordinate.
     * @param Y The Y coordinate.
     * @param Z The Z coordinate.
     * @param O The orientation.
     */
    void GetTeamStartLoc(Team team, float& X, float& Y, float& Z, float& O) const
    {
        PvpTeamIndex idx = GetTeamIndexByTeamId(team);
        X = m_TeamStartLocX[idx];
        Y = m_TeamStartLocY[idx];
        Z = m_TeamStartLocZ[idx];
        O = m_TeamStartLocO[idx];
    }

    /**
     * @brief Sets the maximum start distance.
     *
     * @param startMaxDist The maximum start distance.
     */
    void SetStartMaxDist(float startMaxDist) { m_startMaxDist = startMaxDist; }

    /**
     * @brief Gets the maximum start distance.
     *
     * @return float The maximum start distance.
     */
    float GetStartMaxDist() const { return m_startMaxDist; }

    /* Packet Transfer */
    // method that should fill worldpacket with actual world states (not yet implemented for all battlegrounds!)
    /**
     * @brief Fills the initial world states.
     *
     * @param data The world packet.
     * @param count The count of world states.
     */
    virtual void FillInitialWorldStates(WorldPacket& /*data*/, uint32& /*count*/) {}

    /**
     * @brief Sends a packet to the specified team.
     *
     * @param team The team.
     * @param packet The packet.
     * @param sender The sender.
     * @param self Whether to send to self.
     */
    void SendPacketToTeam(Team team, WorldPacket* packet, Player* sender = NULL, bool self = true);

    /**
     * @brief Sends a packet to all players in the battleground.
     *
     * @param packet The packet.
     */
    void SendPacketToAll(WorldPacket* packet);

    template<class Do>
    /**
     * @brief Broadcasts a worker function to all players in the battleground.
     *
     * @param _do The worker function.
     */
    void BroadcastWorker(Do& _do);

    /**
     * @brief Plays a sound to the specified team.
     *
     * @param SoundID The sound ID.
     * @param team The team.
     */
    void PlaySoundToTeam(uint32 SoundID, Team team);

    /**
     * @brief Plays a sound to all players in the battleground.
     *
     * @param SoundID The sound ID.
     */
    void PlaySoundToAll(uint32 SoundID);

    /**
     * @brief Casts a spell on the specified team.
     *
     * @param SpellID The spell ID.
     * @param team The team.
     */
    void CastSpellOnTeam(uint32 SpellID, Team team);

    /**
     * @brief Rewards honor to the specified team.
     *
     * @param Honor The amount of honor.
     * @param team The team.
     */
    void RewardHonorToTeam(uint32 Honor, Team team);

    /**
     * @brief Rewards reputation to the specified team.
     *
     * @param faction_id The faction ID.
     * @param Reputation The amount of reputation.
     * @param team The team.
     */
    void RewardReputationToTeam(uint32 faction_id, uint32 Reputation, Team team);

    /**
     * @brief Rewards a mark to the specified player.
     *
     * @param plr The player.
     * @param count The number of marks.
     */
    void RewardMark(Player* plr, uint32 count);
    /**
     * @brief Sends a reward mark by mail to the specified player.
     *
     * @param plr The player to send the reward mark to.
     * @param mark The mark to send.
     * @param count The number of marks to send.
     */
    void SendRewardMarkByMail(Player* plr, uint32 mark, uint32 count) const;

    /**
     * @brief Rewards an item to the specified player.
     *
     * @param plr The player to reward the item to.
     * @param item_id The ID of the item to reward.
     * @param count The number of items to reward.
     */
    void RewardItem(Player* plr, uint32 item_id, uint32 count);

    /**
     * @brief Completes a quest for the specified player.
     *
     * @param plr The player to complete the quest for.
     */
    void RewardQuestComplete(Player* plr);

    /**
     * @brief Casts a reward spell on the specified player.
     *
     * @param plr The player to cast the spell on.
     * @param spell_id The ID of the spell to cast.
     */
    void RewardSpellCast(Player* plr, uint32 spell_id);

    /**
     * @brief Updates a world state.
     *
     * @param Field The field to update.
     * @param Value The value to set.
     */
    void UpdateWorldState(uint32 Field, uint32 Value);

    /**
     * @brief Updates a world state for a specific player.
     *
     * @param Field The field to update.
     * @param Value The value to set.
     * @param Source The player to update the world state for.
     */
    void UpdateWorldStateForPlayer(uint32 Field, uint32 Value, Player* Source);

    /**
     * @brief Ends the battleground.
     *
     * @param winner The winning team.
     */
    virtual void EndBattleGround(Team winner);

    /**
     * @brief Blocks the movement of the specified player.
     *
     * @param plr The player to block movement for.
     */
    void BlockMovement(Player* plr);

    /**
     * @brief Sends a message to all players in the battleground.
     *
     * @param entry The entry ID of the message.
     * @param type The type of chat message.
     * @param source The source player of the message.
     */
    void SendMessageToAll(int32 entry, ChatMsg type, Player const* source = NULL);

    /**
     * @brief Sends a yell to all players in the battleground.
     *
     * @param entry The entry ID of the yell.
     * @param language The language of the yell.
     * @param guid The GUID of the source of the yell.
     */
    void SendYellToAll(int32 entry, uint32 language, ObjectGuid guid);

    /**
     * @brief Sends a formatted message to all players in the battleground.
     *
     * @param entry The entry ID of the message.
     * @param type The type of chat message.
     * @param source The source player of the message.
     * @param ... Additional arguments for the formatted message.
     */
    void PSendMessageToAll(int32 entry, ChatMsg type, Player const* source, ...);

    // specialized version with 2 string id args
    /**
     * @brief Sends a message with two string IDs to all players in the battleground.
     *
     * @param entry The entry ID of the message.
     * @param type The type of chat message.
     * @param source The source player of the message.
     * @param strId1 The first string ID.
     * @param strId2 The second string ID.
     */
    void SendMessage2ToAll(int32 entry, ChatMsg type, Player const* source, int32 strId1 = 0, int32 strId2 = 0);

    /**
     * @brief Sends a yell with two arguments to all players in the battleground.
     *
     * @param entry The entry ID of the yell.
     * @param language The language of the yell.
     * @param guid The GUID of the source of the yell.
     * @param arg1 The first argument.
     * @param arg2 The second argument.
     */
    void SendYell2ToAll(int32 entry, uint32 language, ObjectGuid guid, int32 arg1, int32 arg2);

    /* Raid Group */
    /**
     * @brief Gets the raid group for the specified team.
     *
     * @param team The team to get the raid group for.
     * @return Group* The raid group for the team.
     */
    Group* GetBgRaid(Team team) const { return m_BgRaids[GetTeamIndexByTeamId(team)]; }

    /**
     * @brief Sets the raid group for the specified team.
     *
     * @param team The team to set the raid group for.
     * @param bg_raid The raid group to set.
     */
    void SetBgRaid(Team team, Group* bg_raid);

    /**
     * @brief Updates the player score.
     *
     * @param Source The player to update the score for.
     * @param type The type of score to update.
     * @param value The value to update the score by.
     */
    virtual void UpdatePlayerScore(Player* Source, uint32 type, uint32 value);

    /**
     * @brief Gets the team index by team ID.
     *
     * @param team The team ID.
     * @return PvpTeamIndex The team index.
     */
    static PvpTeamIndex GetTeamIndexByTeamId(Team team) { return team == ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE; }

    /**
     * @brief Gets the number of players in the specified team.
     *
     * @param team The team to get the player count for.
     * @return uint32 The number of players in the team.
     */
    uint32 GetPlayersCountByTeam(Team team) const { return m_PlayersCount[GetTeamIndexByTeamId(team)]; }

    /**
     * @brief Gets the number of alive players in the specified team.
     *
     * @param team The team to get the alive player count for.
     * @return uint32 The number of alive players in the team.
     */
    uint32 GetAlivePlayersCountByTeam(Team team) const; // used in arenas to correctly handle death in spirit of redemption / last stand etc. (killer = killed) cases

    /**
     * @brief Updates the player count for the specified team.
     *
     * @param team The team to update the player count for.
     * @param remove Whether to remove a player from the count.
     */
    void UpdatePlayersCountByTeam(Team team, bool remove)
    {
        if (remove)
        {
            --m_PlayersCount[GetTeamIndexByTeamId(team)];
        }
        else
        {
            ++m_PlayersCount[GetTeamIndexByTeamId(team)];
        }
    }

    /* Triggers handle */
    // must be implemented in BG subclass
    /**
     * @brief Handles an area trigger.
     *
     * @param Source The player that triggered the area.
     * @param Trigger The trigger ID.
     * @return bool True if the trigger was handled, false otherwise.
     */
    virtual bool HandleAreaTrigger(Player* /*Source*/, uint32 /*Trigger*/) { return false; }

    // must be implemented in BG subclass if need AND call base class generic code
    /**
     * @brief Handles a player kill.
     *
     * @param player The player that was killed.
     * @param killer The player that killed the other player.
     */
    virtual void HandleKillPlayer(Player* player, Player* killer);

    /**
     * @brief Handles a unit kill.
     *
     * @param unit The unit that was killed.
     * @param killer The player that killed the unit.
     */
    virtual void HandleKillUnit(Creature* /*unit*/, Player* /*killer*/) {}

    /**
     * @brief Handles an event.
     *
     * @param eventId The event ID.
     * @param go The game object associated with the event.
     * @return bool True if the event was handled, false otherwise.
     */
    virtual bool HandleEvent(uint32 /*eventId*/, GameObject* /*go*/) { return false; }

    // Called when a gameobject is created
    /**
     * @brief Handles the creation of a game object.
     *
     * @param go The game object that was created.
     */
    virtual void HandleGameObjectCreate(GameObject* /*go*/) {}

    /* Battleground events */
    /**
     * @brief Handles a player dropping a flag.
     *
     * @param player The player that dropped the flag.
     */
    virtual void EventPlayerDroppedFlag(Player* /*player*/) {}

    /**
     * @brief Handles a player clicking on a flag.
     *
     * @param player The player that clicked on the flag.
     * @param target_obj The game object representing the flag.
     */
    virtual void EventPlayerClickedOnFlag(Player* /*player*/, GameObject* /*target_obj*/) {}

    /**
     * @brief Handles a player capturing a flag.
     *
     * @param player The player that captured the flag.
     */
    virtual void EventPlayerCapturedFlag(Player* /*player*/) {}

    /**
     * @brief Handles a player logging in.
     *
     * @param player The player that logged in.
     */
    void EventPlayerLoggedIn(Player* player);

    /**
     * @brief Handles a player logging out.
     *
     * @param player The player that logged out.
     */
    void EventPlayerLoggedOut(Player* player);

    /* Death related */
    /**
     * @brief Gets the closest graveyard for the specified player.
     *
     * @param player The player to get the closest graveyard for.
     * @return const WorldSafeLocsEntry* The closest graveyard.
     */
    virtual WorldSafeLocsEntry const* GetClosestGraveYard(Player* player);

    /**
     * @brief Adds a player to the battleground.
     *
     * @param plr The player to add.
     */
    virtual void AddPlayer(Player* plr); // must be implemented in BG subclass
    /**
     * @brief Adds or sets a player to the correct battleground group.
     *
     * @param plr The player to add or set.
     * @param plr_guid The GUID of the player.
     * @param team The team of the player.
     */
    void AddOrSetPlayerToCorrectBgGroup(Player* plr, ObjectGuid plr_guid, Team team);

    /**
     * @brief Removes a player from the battleground when they leave.
     *
     * @param guid The GUID of the player.
     * @param Transport Whether to transport the player.
     * @param SendPacket Whether to send a packet.
     */
    virtual void RemovePlayerAtLeave(ObjectGuid guid, bool Transport, bool SendPacket);
    // can be extended in BG subclass

    /* Event related */
    // called when a creature gets added to map (NOTE: only triggered if
    // a player activates the cell of the creature)
    /**
     * @brief Handles the loading of a creature from the database.
     *
     * @param creature The creature that was loaded.
     */
    void OnObjectDBLoad(Creature* /*creature*/);

    /**
     * @brief Handles the loading of a game object from the database.
     *
     * @param obj The game object that was loaded.
     */
    void OnObjectDBLoad(GameObject* /*obj*/);

    // (de-)spawns creatures and gameobjects from an event
    /**
     * @brief Spawns or despawns event objects.
     *
     * @param event1 The first event ID.
     * @param event2 The second event ID.
     * @param spawn Whether to spawn or despawn the objects.
     */
    void SpawnEvent(uint8 event1, uint8 event2, bool spawn);

    /**
     * @brief Checks if an event is active.
     *
     * @param event1 The first event ID.
     * @param event2 The second event ID.
     * @return bool True if the event is active, false otherwise.
     */
    bool IsActiveEvent(uint8 event1, uint8 event2)
    {
        if (m_ActiveEvents.find(event1) == m_ActiveEvents.end())
        {
            return false;
        }
        return m_ActiveEvents[event1] == event2;
    }

    /**
     * @brief Gets the GUID of a single creature for an event.
     *
     * @param event1 The first event ID.
     * @param event2 The second event ID.
     * @return ObjectGuid The GUID of the creature.
     */
    ObjectGuid GetSingleCreatureGuid(uint8 event1, uint8 event2);

    /**
     * @brief Opens a door for an event.
     *
     * @param event1 The first event ID.
     * @param event2 The second event ID.
     */
    void OpenDoorEvent(uint8 event1, uint8 event2 = 0);

    /**
     * @brief Checks if an event is a door event.
     *
     * @param event1 The first event ID.
     * @param event2 The second event ID.
     * @return bool True if the event is a door event, false otherwise.
     */
    bool IsDoor(uint8 event1, uint8 event2);

    /**
     * @brief Handles a trigger buff.
     *
     * @param go_guid The GUID of the game object that triggered the buff.
     */
    void HandleTriggerBuff(ObjectGuid go_guid);

    /**
     * @brief Spawns a battleground object.
     *
     * @param guid The GUID of the object.
     * @param respawntime The respawn time of the object.
     */
    void SpawnBGObject(ObjectGuid guid, uint32 respawntime);

    /**
     * @brief Spawns a battleground creature.
     *
     * @param guid The GUID of the creature.
     * @param respawntime The respawn time of the creature.
     */
    void SpawnBGCreature(ObjectGuid guid, uint32 respawntime);

    /**
     * @brief Opens a door.
     *
     * @param guid The GUID of the door.
     */
    void DoorOpen(ObjectGuid guid);

    /**
     * @brief Closes a door.
     *
     * @param guid The GUID of the door.
     */
    void DoorClose(ObjectGuid guid);

    /**
     * @brief Handles a player being under the map.
     *
     * @param plr The player that is under the map.
     * @return bool True if the player was handled, false otherwise.
     */
    virtual bool HandlePlayerUnderMap(Player* /*plr*/) { return false; }

    // since arenas can be AvA or Hvh, we have to get the "temporary" team of a player
    /**
     * @brief Gets the team of a player.
     *
     * @param guid The GUID of the player.
     * @return Team The team of the player.
     */
    Team GetPlayerTeam(ObjectGuid guid);

    /**
     * @brief Gets the opposing team.
     *
     * @param team The current team.
     * @return Team The opposing team.
     */
    static Team GetOtherTeam(Team team) { return team ? ((team == ALLIANCE) ? HORDE : ALLIANCE) : TEAM_NONE; }

    /**
     * @brief Gets the opposing team index.
     *
     * @param teamIdx The current team index.
     * @return PvpTeamIndex The opposing team index.
     */
    static PvpTeamIndex GetOtherTeamIndex(PvpTeamIndex teamIdx) { return teamIdx == TEAM_INDEX_ALLIANCE ? TEAM_INDEX_HORDE : TEAM_INDEX_ALLIANCE; }

    /**
     * @brief Checks if a player is in the battleground.
     *
     * @param guid The GUID of the player.
     * @return bool True if the player is in the battleground, false otherwise.
     */
    bool IsPlayerInBattleGround(ObjectGuid guid);

    /* virtual score-array - gets used in bg-subclasses */
    int32 m_TeamScores[PVP_TEAM_COUNT];

    /**
     * @brief Structure to hold event objects.
     */
    struct EventObjects
    {
        GuidVector gameobjects; /**< Vector of game object GUIDs */
        GuidVector creatures; /**< Vector of creature GUIDs */
    };

    // cause we create it dynamically i use a map - to avoid resizing when
    // using vector - also it contains 2*events concatenated with PAIR32
    // this is needed to avoid overhead of a 2dimensional std::map
    std::map<uint32, EventObjects> m_EventObjects; /**< Map of event objects */

    // this must be filled first in BattleGroundXY::Reset().. else
    // creatures will get added wrong
    // door-events are automatically added - but _ALL_ other must be in this vector
    std::map<uint8, uint8> m_ActiveEvents; /**< Map of active events */

protected:
    /**
     * @brief Ends a battleground.
     *
     * This method is called when the battleground cannot spawn its own spirit guide, or
     * something is wrong. It correctly ends the battleground.
     */
    void EndNow();

    /**
     * @brief Checks if the battleground is running when a player is added.
     *
     * @param plr The player that was added.
     */
    void PlayerAddedToBGCheckIfBGIsRunning(Player* plr);

    /* Scorekeeping */

    /**
     * @brief Map to hold player scores.
     */
    BattleGroundScoreMap m_PlayerScores; /**< Player scores */

    // must be implemented in BG subclass
    /**
     * @brief Removes a player from the battleground.
     *
     * @param player The player to remove.
     * @param guid The GUID of the player.
     */
    virtual void RemovePlayer(Player* /*player*/, ObjectGuid /*guid*/) {}

    /* Player lists, those need to be accessible by inherited classes */
    /**
     * @brief Map to hold battleground players.
     */
    BattleGroundPlayerMap m_Players; /**< Battleground players */

    /* Important variables used for starting messages */
    /**
     * @brief Number of events.
     */
    uint8 m_Events; /**< Number of events */

    /**
     * @brief Array of start delay times for each event.
     */
    BattleGroundStartTimeIntervals m_StartDelayTimes[BG_STARTING_EVENT_COUNT]; /**< Start delay times */

    /**
     * @brief Array of start message IDs for each event.
     */
    uint32 m_StartMessageIds[BG_STARTING_EVENT_COUNT]; /**< Start message IDs */

    /**
     * @brief Flag indicating if the buff has changed.
     */
    bool m_BuffChange; /**< Buff change flag */

private:
    /* Battleground */
    /**
     * @brief Type ID of the battleground.
     */
    BattleGroundTypeId m_TypeID; /**< Type ID */

    /**
     * @brief Status of the battleground.
     */
    BattleGroundStatus m_Status; /**< Status */

    /**
     * @brief Client instance ID of the battleground.
     */
    uint32 m_ClientInstanceID; /**< Client instance ID */

    /**
     * @brief Start time of the battleground.
     */
    uint32 m_StartTime; /**< Start time */

    /**
     * @brief Timer for valid start position.
     */
    uint32 m_validStartPositionTimer; /**< Valid start position timer */

    /**
     * @brief End time of the battleground.
     */
    int32 m_EndTime; /**< End time */

    /**
     * @brief Bracket ID of the battleground.
     */
    BattleGroundBracketId m_BracketId; /**< Bracket ID */

    /**
     * @brief Flag indicating if the battleground is in the free slot queue.
     */
    bool m_InBGFreeSlotQueue; /**< Free slot queue flag */

    /**
     * @brief Winner of the battleground.
     */
    Team m_Winner; /**< Winner */

    /**
     * @brief Start delay time of the battleground.
     */
    int32 m_StartDelayTime; /**< Start delay time */

    /**
     * @brief Flag indicating if there is a premature countdown.
     */
    bool m_PrematureCountDown; /**< Premature countdown flag */

    /**
     * @brief Timer for the premature countdown.
     */
    uint32 m_PrematureCountDownTimer; /**< Premature countdown timer */

    /**
     * @brief Name of the battleground.
     */
    char const* m_Name; /**< Name */

    /* Player lists */
    /**
     * @brief Queue of offline players.
     */
    typedef std::deque<ObjectGuid> OfflineQueue;
    OfflineQueue m_OfflineQueue; /**< Offline player queue */

    /* Invited counters are useful for player invitation to BG - do not allow, if BG is started to one faction to have 2 more players than another faction */
    /* Invited counters will be changed only when removing already invited player from queue, removing player from battleground and inviting player to BG */
    /* Invited players counters */
    /**
     * @brief Number of invited players for the Alliance.
     */
    uint32 m_InvitedAlliance; /**< Invited Alliance players */

    /**
     * @brief Number of invited players for the Horde.
     */
    uint32 m_InvitedHorde; /**< Invited Horde players */

    /* Raid Group */
    /**
     * @brief Array of raid groups for each team.
     */
    Group* m_BgRaids[PVP_TEAM_COUNT]; /**< Raid groups */

    /* Players count by team */
    /**
     * @brief Array of player counts for each team.
     */
    uint32 m_PlayersCount[PVP_TEAM_COUNT]; /**< Player counts */

    /* Limits */
    /**
     * @brief Minimum level for the battleground.
     */
    uint32 m_LevelMin; /**< Minimum level */

    /**
     * @brief Maximum level for the battleground.
     */
    uint32 m_LevelMax; /**< Maximum level */

    /**
     * @brief Maximum number of players per team.
     */
    uint32 m_MaxPlayersPerTeam; /**< Maximum players per team */

    /**
     * @brief Maximum number of players in the battleground.
     */
    uint32 m_MaxPlayers; /**< Maximum players */

    /**
     * @brief Minimum number of players per team.
     */
    uint32 m_MinPlayersPerTeam; /**< Minimum players per team */

    /**
     * @brief Minimum number of players in the battleground.
     */
    uint32 m_MinPlayers; /**< Minimum players */

    /* Start location */
    /**
     * @brief Map ID of the battleground.
     */
    uint32 m_MapId; /**< Map ID */

    /**
     * @brief Pointer to the battleground map.
     */
    BattleGroundMap* m_Map; /**< Battleground map */

    /**
     * @brief Maximum start distance.
     */
    float m_startMaxDist; /**< Maximum start distance */

    /**
     * @brief Array of X coordinates for team start locations.
     */
    float m_TeamStartLocX[PVP_TEAM_COUNT]; /**< Team start X coordinates */

    /**
     * @brief Array of Y coordinates for team start locations.
     */
    float m_TeamStartLocY[PVP_TEAM_COUNT]; /**< Team start Y coordinates */

    /**
     * @brief Array of Z coordinates for team start locations.
     */
    float m_TeamStartLocZ[PVP_TEAM_COUNT]; /**< Team start Z coordinates */

    /**
     * @brief Array of orientations for team start locations.
     */
    float m_TeamStartLocO[PVP_TEAM_COUNT]; /**< Team start orientations */
};

    // helper functions for world state list fill
    /**
     * @brief Fills the initial world state.
     *
     * @param data The byte buffer to fill.
     * @param count The count of world states.
     * @param state The state to set.
     * @param value The value to set for the state.
     */
    inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, uint32 value)
    {
        data << uint32(state);
        data << uint32(value);
        ++count;
    }

    /**
     * @brief Fills the initial world state.
     *
     * @param data The byte buffer to fill.
     * @param count The count of world states.
     * @param state The state to set.
     * @param value The value to set for the state.
     */
    inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, int32 value)
    {
        data << uint32(state);
        data << int32(value);
        ++count;
    }

    /**
     * @brief Fills the initial world state.
     *
     * @param data The byte buffer to fill.
     * @param count The count of world states.
     * @param state The state to set.
     * @param value The value to set for the state.
     */
    inline void FillInitialWorldState(ByteBuffer& data, uint32& count, uint32 state, bool value)
    {
        data << uint32(state);
        data << uint32(value ? 1 : 0);
        ++count;
    }

    /**
     * @brief Structure to hold world state pairs.
     */
    struct WorldStatePair
    {
        uint32 state; /**< The state */
        uint32 value; /**< The value */
    };

    /**
     * @brief Fills the initial world state.
     *
     * @param data The byte buffer to fill.
     * @param count The count of world states.
     * @param array The array of world state pairs.
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
