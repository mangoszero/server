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

#ifndef MANGOS_H_BATTLEGROUNDAV
#define MANGOS_H_BATTLEGROUNDAV

#include "Common.h"
#include "BattleGround.h"

#define BG_AV_MAX_NODE_DISTANCE             25              // distance in which players are still counted in range of a banner (for alliance towers this is calculated from the center of the tower)

#define BG_AV_BOSS_KILL_QUEST_SPELL         23658

#define BG_AV_CAPTIME                       240000          // 4 minutes
#define BG_AV_SNOWFALL_FIRSTCAP             300000          // 5 minutes but i also have seen 4:05

#define BG_AV_SCORE_INITIAL_POINTS          600
#define BG_AV_SCORE_NEAR_LOSE               120

// description: KILL = bonushonor kill one kill is 21honor worth at 0
// REP reputation, RES = resources a team will lose
#define BG_AV_KILL_BOSS                     4
#define BG_AV_REP_BOSS                      350
#define BG_AV_REP_BOSS_HOLIDAY              525

#define BG_AV_KILL_CAPTAIN                  3
#define BG_AV_REP_CAPTAIN                   125
#define BG_AV_REP_CAPTAIN_HOLIDAY           185
#define BG_AV_RES_CAPTAIN                   100

#define BG_AV_KILL_TOWER                    3
#define BG_AV_REP_TOWER                     12
#define BG_AV_REP_TOWER_HOLIDAY             18
#define BG_AV_RES_TOWER                     75

#define BG_AV_KILL_GET_COMMANDER            1               // for a safely returned wingcommander TODO implement it

// bonushonor at the end
#define BG_AV_KILL_SURVIVING_TOWER          2
#define BG_AV_REP_SURVIVING_TOWER           12
#define BG_AV_REP_SURVIVING_TOWER_HOLIDAY   18

#define BG_AV_KILL_SURVIVING_CAPTAIN        2
#define BG_AV_REP_SURVIVING_CAPTAIN         125
#define BG_AV_REP_SURVIVING_CAPTAIN_HOLIDAY 175

#define BG_AV_KILL_MAP_COMPLETE             0
#define BG_AV_KILL_MAP_COMPLETE_HOLIDAY     4

#define BG_AV_REP_OWNED_GRAVE               12
#define BG_AV_REP_OWNED_GRAVE_HOLIDAY       18

#define BG_AV_REP_OWNED_MINE                24
#define BG_AV_REP_OWNED_MINE_HOLIDAY        36

/**
 * @brief Enum for various sounds used in the battleground.
 */
enum BG_AV_Sounds
{
    BG_AV_SOUND_NEAR_LOSE               = 8456,             // not confirmed yet

    BG_AV_SOUND_ALLIANCE_ASSAULTS       = 8212,             // tower, grave + enemy boss if someone tries to attack him
    BG_AV_SOUND_HORDE_ASSAULTS          = 8174,
    BG_AV_SOUND_ALLIANCE_GOOD           = 8173,             // if something good happens for the team: wins, captures mine or grave, destroys tower and defends grave
    BG_AV_SOUND_HORDE_GOOD              = 8213,
    BG_AV_SOUND_BOTH_TOWER_DEFEND       = 8192,

    BG_AV_SOUND_ALLIANCE_CAPTAIN        = 8232,             // gets called when someone attacks them and at the beginning after 3min + rand(x) * 10sec (maybe buff)
    BG_AV_SOUND_HORDE_CAPTAIN           = 8333
};

/**
 * @brief Enum for other values used in the battleground.
 */
enum BG_AV_OTHER_VALUES
{
    BG_AV_NORTH_MINE            = 0,
    BG_AV_SOUTH_MINE            = 1,
    BG_AV_MINE_TICK_TIMER       = 45000,
    BG_AV_MINE_RECLAIM_TIMER    = 1200000,                  // TODO: get the right value.. this is currently 20 minutes
    BG_AV_FACTION_A             = 730,
    BG_AV_FACTION_H             = 729
};
#define BG_AV_MAX_MINES 2

/**
 * @brief Enum for object IDs used in the battleground.
 */
enum BG_AV_ObjectIds
{
    // mine supplies
    BG_AV_OBJECTID_MINE_N               = 178785,
    BG_AV_OBJECTID_MINE_S               = 178784
};

/**
 * @brief Enum for nodes in the battleground.
 */
enum BG_AV_Nodes
{
    BG_AV_NODES_FIRSTAID_STATION        = 0,
    BG_AV_NODES_STORMPIKE_GRAVE         = 1,
    BG_AV_NODES_STONEHEART_GRAVE        = 2,
    BG_AV_NODES_SNOWFALL_GRAVE          = 3,
    BG_AV_NODES_ICEBLOOD_GRAVE          = 4,
    BG_AV_NODES_FROSTWOLF_GRAVE         = 5,
    BG_AV_NODES_FROSTWOLF_HUT           = 6,
    BG_AV_NODES_DUNBALDAR_SOUTH         = 7,
    BG_AV_NODES_DUNBALDAR_NORTH         = 8,
    BG_AV_NODES_ICEWING_BUNKER          = 9,
    BG_AV_NODES_STONEHEART_BUNKER       = 10,
    BG_AV_NODES_ICEBLOOD_TOWER          = 11,
    BG_AV_NODES_TOWER_POINT             = 12,
    BG_AV_NODES_FROSTWOLF_ETOWER        = 13,
    BG_AV_NODES_FROSTWOLF_WTOWER        = 14,
    BG_AV_NODES_ERROR                   = 255
};
#define BG_AV_NODES_MAX                 15

// for node events we will use event1=node
// event2 is related to BG_AV_States
// 0 = alliance assaulted
// 1 = alliance control
// 2 = horde assaulted
// 3 = horde control
// 4 = neutral assaulted
// 5 = neutral control

// graves have special creatures - their defenders can be in 4 different states
// through some quests with armor scraps
// so i use event1=BG_AV_NODES_MAX+node (15-21)
// and event2=type

#define BG_AV_MINE_BOSSES       46                          // + mineid will be exact event
#define BG_AV_MINE_BOSSES_NORTH 46
#define BG_AV_MINE_BOSSES_SOUTH 47
#define BG_AV_CAPTAIN_A         48
#define BG_AV_CAPTAIN_H         49
#define BG_AV_MINE_EVENT        50                          // + mineid will be exact event
#define BG_AV_MINE_EVENT_NORTH  50
#define BG_AV_MINE_EVENT_SOUTH  51

#define BG_AV_MARSHAL_A_SOUTH   52
#define BG_AV_MARSHAL_A_NORTH   53
#define BG_AV_MARSHAL_A_ICE     54
#define BG_AV_MARSHAL_A_STONE   55
#define BG_AV_MARSHAL_H_ICE     56
#define BG_AV_MARSHAL_H_TOWER   57
#define BG_AV_MARSHAL_H_ETOWER  58
#define BG_AV_MARSHAL_H_WTOWER  59

#define BG_AV_HERALD            60
#define BG_AV_BOSS_A            61
#define BG_AV_BOSS_H            62
#define BG_AV_NodeEventCaptainDead_A 63
#define BG_AV_NodeEventCaptainDead_H 64

/**
 * @brief Enum for graveyards in the battleground.
 */
enum BG_AV_Graveyards
{
    BG_AV_GRAVE_STORM_AID          = 751,
    BG_AV_GRAVE_STORM_GRAVE        = 689,
    BG_AV_GRAVE_STONE_GRAVE        = 729,
    BG_AV_GRAVE_SNOWFALL           = 169,
    BG_AV_GRAVE_ICE_GRAVE          = 749,
    BG_AV_GRAVE_FROSTWOLF          = 690,
    BG_AV_GRAVE_FROST_HUT          = 750,
    BG_AV_GRAVE_MAIN_ALLIANCE      = 611,
    BG_AV_GRAVE_MAIN_HORDE         = 610
};

const uint32 BG_AV_GraveyardIds[9] = /**< TODO */
{
    BG_AV_GRAVE_STORM_AID,
    BG_AV_GRAVE_STORM_GRAVE,
    BG_AV_GRAVE_STONE_GRAVE,
    BG_AV_GRAVE_SNOWFALL,
    BG_AV_GRAVE_ICE_GRAVE,
    BG_AV_GRAVE_FROSTWOLF,
    BG_AV_GRAVE_FROST_HUT,
    BG_AV_GRAVE_MAIN_ALLIANCE,
    BG_AV_GRAVE_MAIN_HORDE
};

/**
 * @brief Enum for states of nodes in the battleground.
 */
enum BG_AV_States
{
    POINT_ASSAULTED             = 0,
    POINT_CONTROLLED            = 1
};
#define BG_AV_MAX_STATES 2

/**
 * @brief Enum for world states in the battleground.
 */
enum BG_AV_WorldStates
{
    BG_AV_Alliance_Score        = 3127,
    BG_AV_Horde_Score           = 3128,
    BG_AV_SHOW_H_SCORE          = 3133,
    BG_AV_SHOW_A_SCORE          = 3134,
    AV_SNOWFALL_N               = 1966
};

/**
 * @brief Special version with more wide values range than BattleGroundTeamIndex.
 *
 * BattleGroundAVTeamIndex <- BattleGroundTeamIndex cast safe
 * BattleGroundAVTeamIndex -> BattleGroundTeamIndex cast safe and array with BG_TEAMS_COUNT elements must checked != BG_AV_TEAM_NEUTRAL before used
 *
 */
enum BattleGroundAVTeamIndex
{
    BG_AV_TEAM_ALLIANCE        = TEAM_INDEX_ALLIANCE,
    BG_AV_TEAM_HORDE           = TEAM_INDEX_HORDE,
    BG_AV_TEAM_NEUTRAL         = TEAM_INDEX_NEUTRAL,                         // this is the neutral owner of snowfall
};

#define BG_AV_TEAMS_COUNT 3

const uint32 BG_AV_MineWorldStates[2][BG_AV_TEAMS_COUNT] = /**< alliance_control horde_control neutral_control */
{
    {1358, 1359, 1360},
    {1355, 1356, 1357}
};

const uint32 BG_AV_NodeWorldStates[BG_AV_NODES_MAX][4] = /**< alliance_control alliance_assault h_control h_assault */
{
    // Stormpike first aid station
    {1326, 1325, 1328, 1327},
    // Stormpike Graveyard
    {1335, 1333, 1336, 1334},
    // Stoneheart Grave
    {1304, 1302, 1303, 1301},
    // Snowfall Grave
    {1343, 1341, 1344, 1342},
    // Iceblood grave
    {1348, 1346, 1349, 1347},
    // Frostwolf Grave
    {1339, 1337, 1340, 1338},
    // Frostwolf Hut
    {1331, 1329, 1332, 1330},
    // Dunbaldar South Bunker
    {1375, 1361, 1378, 1370},
    // Dunbaldar North Bunker
    {1374, 1362, 1379, 1371},
    // Icewing Bunker
    {1376, 1363, 1380, 1372},
    // Stoneheart Bunker
    {1377, 1364, 1381, 1373},
    // Iceblood Tower
    {1390, 1368, 1395, 1385},
    // Tower Point
    {1389, 1367, 1394, 1384},
    // Frostwolf East
    {1388, 1366, 1393, 1383},
    // Frostwolf West
    {1387, 1365, 1392, 1382},
};

#define BG_AV_MAX_GRAVETYPES 4
/**
 * @brief Through the armorscap-quest 4 different grave defender exist.
 *
 */
enum BG_AV_QuestIds
{
    BG_AV_QUEST_A_SCRAPS1       = 7223,                     // first quest
    BG_AV_QUEST_A_SCRAPS2       = 6781,                     // repeatable
    BG_AV_QUEST_H_SCRAPS1       = 7224,
    BG_AV_QUEST_H_SCRAPS2       = 6741,
    BG_AV_QUEST_A_COMMANDER1    = 6942,                     // soldier
    BG_AV_QUEST_H_COMMANDER1    = 6825,
    BG_AV_QUEST_A_COMMANDER2    = 6941,                     // lieutenant
    BG_AV_QUEST_H_COMMANDER2    = 6826,
    BG_AV_QUEST_A_COMMANDER3    = 6943,                     // commander
    BG_AV_QUEST_H_COMMANDER3    = 6827,
    BG_AV_QUEST_A_BOSS1         = 7386,                     // 5 crystal/blood
    BG_AV_QUEST_H_BOSS1         = 7385,
    BG_AV_QUEST_A_BOSS2         = 6881,                     // 1
    BG_AV_QUEST_H_BOSS2         = 6801,
    BG_AV_QUEST_A_NEAR_MINE     = 5892,                     // the mine near start location of team
    BG_AV_QUEST_H_NEAR_MINE     = 5893,
    BG_AV_QUEST_A_OTHER_MINE    = 6982,                     // the other mine ;)
    BG_AV_QUEST_H_OTHER_MINE    = 6985,
    BG_AV_QUEST_A_RIDER_HIDE    = 7026,
    BG_AV_QUEST_H_RIDER_HIDE    = 7002,
    BG_AV_QUEST_A_RIDER_TAME    = 7027,
    BG_AV_QUEST_H_RIDER_TAME    = 7001
};

/**
 * @brief Structure to hold information about a node in the battleground.
 */
struct BG_AV_NodeInfo
{
    BattleGroundAVTeamIndex TotalOwner; /**< The total owner of the node. */
    BattleGroundAVTeamIndex Owner; /**< The current owner of the node. */
    BattleGroundAVTeamIndex PrevOwner; /**< The previous owner of the node. */
    BG_AV_States State; /**< The current state of the node. */
    BG_AV_States PrevState; /**< The previous state of the node. */
    uint32       Timer; /**< The timer for the node. */
    bool         Tower; /**< Whether the node is a tower. */
};

/**
 * @brief Increment operator for BG_AV_Nodes enum.
 *
 * @param i The node to increment.
 * @return BG_AV_Nodes& The incremented node.
 */
inline BG_AV_Nodes& operator++(BG_AV_Nodes& i)
{
    return i = BG_AV_Nodes(i + 1);
}

/**
 * @brief Class to hold the score for a player in the battleground.
 */
class BattleGroundAVScore : public BattleGroundScore
{
    public:
        /**
         * @brief Constructor for BattleGroundAVScore.
         */
        BattleGroundAVScore() : GraveyardsAssaulted(0), GraveyardsDefended(0), TowersAssaulted(0), TowersDefended(0), SecondaryObjectives(0), LieutnantCount(0), SecondaryNPC(0) {};
        /**
         * @brief Destructor for BattleGroundAVScore.
         */
        virtual ~BattleGroundAVScore() {};

        uint32 GetAttr1() const { return GraveyardsAssaulted; }
        uint32 GetAttr2() const { return GraveyardsDefended; }
        uint32 GetAttr3() const { return TowersAssaulted; }
        uint32 GetAttr4() const { return TowersDefended; }
        uint32 GetAttr5() const { return SecondaryObjectives; }

        uint32 GraveyardsAssaulted; /**< Number of graveyards assaulted. */
        uint32 GraveyardsDefended; /**< Number of graveyards defended. */
        uint32 TowersAssaulted; /**< Number of towers assaulted. */
        uint32 TowersDefended; /**< Number of towers defended. */
        uint32 SecondaryObjectives; /**< Number of secondary objectives completed. */
        uint32 LieutnantCount; /**< Number of lieutenants killed. */
        uint32 SecondaryNPC; /**< Number of secondary NPCs killed. */
};

/**
 * @brief Class for the Alterac Valley battleground.
 */
class BattleGroundAV : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        /**
         * @brief Constructor for BattleGroundAV.
         */
        BattleGroundAV();
        /**
         * @brief Updates the battleground.
         *
         * @param diff The time difference since the last update.
         */
        void Update(uint32 diff) override;

        /**
         * @brief Adds a player to the battleground.
         *
         * @param plr The player to add.
         */
        void AddPlayer(Player* plr) override;

        /**
         * @brief Opens the doors at the start of the battleground.
         */
        void StartingEventOpenDoors() override;
        /**
         * @brief Fills the initial world states for the battleground.
         *
         * @param data The world packet to fill.
         * @param count The count of world states.
         */
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

                /**
         * @brief Handles an area trigger.
         *
         * @param source The player who triggered the area.
         * @param trigger The ID of the trigger.
         * @return true If the trigger was handled.
         */
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;

        /**
         * @brief Resets the battleground to its initial state.
         */
        void Reset() override;

        /* General functions */
        /**
         * @brief Updates the score for a team.
         *
         * @param teamIdx The index of the team.
         * @param points The points to add to the team's score.
         */
        void UpdateScore(PvpTeamIndex teamIdx, int32 points);

        /**
         * @brief Updates the score for a player.
         *
         * @param source The player whose score is being updated.
         * @param type The type of score to update.
         * @param value The value to add to the score.
         */
        void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;

        /* Event handling functions - these are are called from external scripts */
        /**
         * @brief Handles a player clicking on a flag.
         *
         * @param source The player who clicked on the flag.
         * @param target_obj The game object representing the flag.
         */
        void EventPlayerClickedOnFlag(Player* source, GameObject* target_obj) override;

        /**
         * @brief Handles a player being killed.
         *
         * @param player The player who was killed.
         * @param killer The player who killed the player.
         */
        void HandleKillPlayer(Player* player, Player* killer) override;

        /**
         * @brief Handles a unit being killed.
         *
         * @param creature The unit that was killed.
         * @param killer The player who killed the unit.
         */
        void HandleKillUnit(Creature* creature, Player* killer) override;

        /**
         * @brief Handles a quest being completed.
         *
         * @param questid The ID of the quest.
         * @param player The player who completed the quest.
         */
        void HandleQuestComplete(uint32 questid, Player* player);

        /**
         * @brief Checks if a player can do a mine quest.
         *
         * @param GOId The ID of the game object.
         * @param team The team of the player.
         * @return true If the player can do the mine quest.
         */
        bool PlayerCanDoMineQuest(int32 GOId, Team team);

        /**
         * @brief Ends the battleground.
         *
         * @param winner The team that won the battleground.
         */
        void EndBattleGround(Team winner) override;

        /**
         * @brief Gets the closest graveyard to a player.
         *
         * @param plr The player.
         * @return const WorldSafeLocsEntry* The closest graveyard.
         */
        WorldSafeLocsEntry const* GetClosestGraveYard(Player* plr) override;

        /**
         * @brief Gets the premature winner of the battleground.
         *
         * @return Team The premature winner.
         */
        Team GetPrematureWinner() override;

        /**
         * @brief Gets the AV team index by team ID.
         *
         * @param team The team ID.
         * @return BattleGroundAVTeamIndex The AV team index.
         */
        static BattleGroundAVTeamIndex GetAVTeamIndexByTeamId(Team team) { return BattleGroundAVTeamIndex(GetTeamIndexByTeamId(team)); }

    private:
        /* Node handling functions */
        /**
         * @brief Handles a player assaulting a point.
         *
         * @param player The player assaulting the point.
         * @param node The node being assaulted.
         */
        void EventPlayerAssaultsPoint(Player* player, BG_AV_Nodes node);

        /**
         * @brief Handles a player defending a point.
         *
         * @param player The player defending the point.
         * @param node The node being defended.
         */
        void EventPlayerDefendsPoint(Player* player, BG_AV_Nodes node);

        /**
         * @brief Handles a player destroying a point.
         *
         * @param node The node being destroyed.
         */
        void EventPlayerDestroyedPoint(BG_AV_Nodes node);

        /**
         * @brief Assaults a node.
         *
         * @param node The node being assaulted.
         * @param teamIdx The index of the team assaulting the node.
         */
        void AssaultNode(BG_AV_Nodes node, PvpTeamIndex teamIdx);

        /**
         * @brief Destroys a node.
         *
         * @param node The node being destroyed.
         */
        void DestroyNode(BG_AV_Nodes node);

        /**
         * @brief Initializes a node.
         *
         * @param node The node being initialized.
         * @param teamIdx The index of the team owning the node.
         * @param tower Whether the node is a tower.
         */
        void InitNode(BG_AV_Nodes node, BattleGroundAVTeamIndex teamIdx, bool tower);

        /**
         * @brief Defends a node.
         *
         * @param node The node being defended.
         * @param teamIdx The index of the team defending the node.
         */
        void DefendNode(BG_AV_Nodes node, PvpTeamIndex teamIdx);

        /**
         * @brief Populates a node with NPCs.
         *
         * @param node The node being populated.
         */
        void PopulateNode(BG_AV_Nodes node);

        /**
         * @brief Gets the name of a node.
         *
         * @param node The node.
         * @return uint32 The name of the node.
         */
        uint32 GetNodeName(BG_AV_Nodes node) const;

        /**
         * @brief Checks if a node is a tower.
         *
         * @param node The node.
         * @return bool True if the node is a tower.
         */
        bool IsTower(BG_AV_Nodes node) const { return (node == BG_AV_NODES_ERROR) ? false : m_Nodes[node].Tower; }

        /**
         * @brief Checks if a node is a graveyard.
         *
         * @param node The node.
         * @return bool True if the node is a graveyard.
         */
        bool IsGrave(BG_AV_Nodes node) const { return (node == BG_AV_NODES_ERROR) ? false : !m_Nodes[node].Tower; }

        /* Mine handling functions */
        /**
         * @brief Changes the owner of a mine.
         *
         * @param mine The mine.
         * @param teamIdx The index of the team owning the mine.
         */
        void ChangeMineOwner(uint8 mine, BattleGroundAVTeamIndex teamIdx);

        /* World state handling functions */
        /**
         * @brief Gets the world state type for a node.
         *
         * @param state The state of the node.
         * @param teamIdx The index of the team owning the node.
         * @return uint8 The world state type.
         */
        uint8 GetWorldStateType(uint8 state, BattleGroundAVTeamIndex teamIdx) const { return teamIdx * BG_AV_MAX_STATES + state; }

        /**
         * @brief Sends the world states for a mine.
         *
         * @param mine The mine.
         */
        void SendMineWorldStates(uint32 mine);

        /**
         * @brief Updates the world state for a node.
         *
         * @param node The node.
         */
        void UpdateNodeWorldState(BG_AV_Nodes node);

        /* Variables */
        uint32 m_Team_QuestStatus[PVP_TEAM_COUNT][9];       /**< The quest status for each team. [x][y] x=team y=quest counter. */

        BG_AV_NodeInfo m_Nodes[BG_AV_NODES_MAX]; /**< Information about each node. */

        // Only for world states needed
        BattleGroundAVTeamIndex m_Mine_Owner[BG_AV_MAX_MINES]; /**< The owner of each mine. */
        BattleGroundAVTeamIndex m_Mine_PrevOwner[BG_AV_MAX_MINES]; /**< The previous owner of each mine. */
        int32 m_Mine_Timer[BG_AV_MAX_MINES]; /**< The timer for each mine. */
        uint32 m_Mine_Reclaim_Timer[BG_AV_MAX_MINES]; /**< The reclaim timer for each mine. */

        bool m_IsInformedNearLose[PVP_TEAM_COUNT]; /**< Whether each team has been informed of a near loss. */

        uint32 m_HonorMapComplete; /**< The honor for completing the map. */
        uint32 m_RepTowerDestruction; /**< The reputation for destroying a tower. */
        uint32 m_RepCaptain; /**< The reputation for killing a captain. */
        uint32 m_RepBoss; /**< The reputation for killing a boss. */
        uint32 m_RepOwnedGrave; /**< The reputation for owning a graveyard. */
        uint32 m_RepOwnedMine; /**< The reputation for owning a mine. */
        uint32 m_RepSurviveCaptain; /**< The reputation for surviving a captain. */
        uint32 m_RepSurviveTower; /**< The reputation for surviving a tower. */
};

#endif
