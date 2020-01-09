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

#ifndef MANGOS_H_BATTLEGROUNDAB
#define MANGOS_H_BATTLEGROUNDAB

#include "Common.h"
#include "BattleGround.h"

/**
 * @brief
 *
 */
enum BG_AB_WorldStates
{
    BG_AB_OP_OCCUPIED_BASES_HORDE       = 1778,
    BG_AB_OP_OCCUPIED_BASES_ALLY        = 1779,
    BG_AB_OP_RESOURCES_ALLY             = 1776,
    BG_AB_OP_RESOURCES_HORDE            = 1777,
    BG_AB_OP_RESOURCES_MAX              = 1780,
    BG_AB_OP_RESOURCES_WARNING          = 1955
    /*
        BG_AB_OP_STABLE_ICON                = 1842,         // Stable map icon (NONE)
        BG_AB_OP_STABLE_STATE_ALIENCE       = 1767,         // Stable map state (ALIENCE)
        BG_AB_OP_STABLE_STATE_HORDE         = 1768,         // Stable map state (HORDE)
        BG_AB_OP_STABLE_STATE_CON_ALI       = 1769,         // Stable map state (CON ALIENCE)
        BG_AB_OP_STABLE_STATE_CON_HOR       = 1770,         // Stable map state (CON HORDE)
        BG_AB_OP_FARM_ICON                  = 1845,         // Farm map icon (NONE)
        BG_AB_OP_FARM_STATE_ALIENCE         = 1772,         // Farm state (ALIENCE)
        BG_AB_OP_FARM_STATE_HORDE           = 1773,         // Farm state (HORDE)
        BG_AB_OP_FARM_STATE_CON_ALI         = 1774,         // Farm state (CON ALIENCE)
        BG_AB_OP_FARM_STATE_CON_HOR         = 1775,         // Farm state (CON HORDE)

        BG_AB_OP_BLACKSMITH_ICON            = 1846,         // Blacksmith map icon (NONE)
        BG_AB_OP_BLACKSMITH_STATE_ALIENCE   = 1782,         // Blacksmith map state (ALIENCE)
        BG_AB_OP_BLACKSMITH_STATE_HORDE     = 1783,         // Blacksmith map state (HORDE)
        BG_AB_OP_BLACKSMITH_STATE_CON_ALI   = 1784,         // Blacksmith map state (CON ALIENCE)
        BG_AB_OP_BLACKSMITH_STATE_CON_HOR   = 1785,         // Blacksmith map state (CON HORDE)
        BG_AB_OP_LUMBERMILL_ICON            = 1844,         // Lumber Mill map icon (NONE)
        BG_AB_OP_LUMBERMILL_STATE_ALIENCE   = 1792,         // Lumber Mill map state (ALIENCE)
        BG_AB_OP_LUMBERMILL_STATE_HORDE     = 1793,         // Lumber Mill map state (HORDE)
        BG_AB_OP_LUMBERMILL_STATE_CON_ALI   = 1794,         // Lumber Mill map state (CON ALIENCE)
        BG_AB_OP_LUMBERMILL_STATE_CON_HOR   = 1795,         // Lumber Mill map state (CON HORDE)
        BG_AB_OP_GOLDMINE_ICON              = 1843,         // Gold Mine map icon (NONE)
        BG_AB_OP_GOLDMINE_STATE_ALIENCE     = 1787,         // Gold Mine map state (ALIENCE)
        BG_AB_OP_GOLDMINE_STATE_HORDE       = 1788,         // Gold Mine map state (HORDE)
        BG_AB_OP_GOLDMINE_STATE_CON_ALI     = 1789,         // Gold Mine map state (CON ALIENCE
        BG_AB_OP_GOLDMINE_STATE_CON_HOR     = 1790,         // Gold Mine map state (CON HORDE)
    */
};

const uint32 BG_AB_OP_NODESTATES[5] =    {1767, 1782, 1772, 1792, 1787}; /**< TODO */

const uint32 BG_AB_OP_NODEICONS[5]  =    {1842, 1846, 1845, 1844, 1843}; /**< TODO */

/* node events */
// node-events are just event1=BG_AB_Nodes, event2=BG_AB_NodeStatus
// so we don't need to define the constants here :)

/**
 * @brief
 *
 */
enum BG_AB_Timers
{
    BG_AB_FLAG_CAPTURING_TIME           = 60000
};

/**
 * @brief
 *
 */
enum BG_AB_Score
{
    BG_AB_WARNING_NEAR_VICTORY_SCORE    = 1800,
    BG_AB_MAX_TEAM_SCORE                = 2000
};

/* do NOT change the order, else wrong behaviour */
/**
 * @brief
 *
 */
enum BG_AB_Nodes
{
    BG_AB_NODE_STABLES          = 0,
    BG_AB_NODE_BLACKSMITH       = 1,
    BG_AB_NODE_FARM             = 2,
    BG_AB_NODE_LUMBER_MILL      = 3,
    BG_AB_NODE_GOLD_MINE        = 4,
    BG_AB_NODES_ERROR           = 255
};

#define BG_AB_NODES_MAX   5

/**
 * @brief
 *
 */
enum BG_AB_NodeStatus
{
    BG_AB_NODE_TYPE_NEUTRAL             = 0,
    BG_AB_NODE_TYPE_CONTESTED           = 1,
    BG_AB_NODE_STATUS_ALLY_CONTESTED    = 1,
    BG_AB_NODE_STATUS_HORDE_CONTESTED   = 2,
    BG_AB_NODE_TYPE_OCCUPIED            = 3,
    BG_AB_NODE_STATUS_ALLY_OCCUPIED     = 3,
    BG_AB_NODE_STATUS_HORDE_OCCUPIED    = 4
};

/**
 * @brief
 *
 */
enum BG_AB_Sounds
{
    BG_AB_SOUND_NODE_CLAIMED            = 8192,
    BG_AB_SOUND_NODE_CAPTURED_ALLIANCE  = 8173,
    BG_AB_SOUND_NODE_CAPTURED_HORDE     = 8213,
    BG_AB_SOUND_NODE_ASSAULTED_ALLIANCE = 8212,
    BG_AB_SOUND_NODE_ASSAULTED_HORDE    = 8174,
    BG_AB_SOUND_NEAR_VICTORY            = 8456
};

#define AB_NORMAL_HONOR_INTERVAL        330
#define AB_WEEKEND_HONOR_INTERVAL       200
#define AB_NORMAL_REPUTATION_INTERVAL   330
#define AB_WEEKEND_REPUTATION_INTERVAL  200

// Tick intervals and given points: case 0,1,2,3,4,5 captured nodes
const uint32 BG_AB_TickIntervals[6] = {0, 12000, 9000, 6000, 3000, 1000}; /**< TODO */
const uint32 BG_AB_TickPoints[6] = {0, 10, 10, 10, 10, 30}; /**< TODO */

// Honor granted depending on player's level
const uint32 BG_AB_PerTickHonor[MAX_BATTLEGROUND_BRACKETS] = {24, 41, 68, 113, 189, 198}; /**< TODO */
const uint32 BG_AB_WinMatchHonor[MAX_BATTLEGROUND_BRACKETS] = {24, 41, 68, 113, 189, 198}; /**< TODO */

// WorldSafeLocs ids for 5 nodes, and for ally, and horde starting location
const uint32 BG_AB_GraveyardIds[7] = {895, 894, 893, 897, 896, 898, 899}; /**< TODO */

// x, y, z, o
const float BG_AB_BuffPositions[BG_AB_NODES_MAX][4] = /**< TODO */
{
    {1185.71f, 1185.24f, -56.36f, 2.56f},                   // stables
    {990.75f, 1008.18f, -42.60f, 2.43f},                    // blacksmith
    {817.66f, 843.34f, -56.54f, 3.01f},                     // farm
    {807.46f, 1189.16f, 11.92f, 5.44f},                     // lumber mill
    {1146.62f, 816.94f, -98.49f, 6.14f}                     // gold mine
};

/**
 * @brief
 *
 */
struct BG_AB_BannerTimer
{
    uint32      timer; /**< TODO */
    uint8       type; /**< TODO */
    uint8       teamIndex; /**< TODO */
};

/**
 * @brief
 *
 */
class BattleGroundABScore : public BattleGroundScore
{
    public:
/**
 * @brief
 *
 */
        BattleGroundABScore(): BasesAssaulted(0), BasesDefended(0) {};
        /**
         * @brief
         *
         */
        virtual ~BattleGroundABScore() {};

        uint32 GetAttr1() const { return BasesAssaulted; }
        uint32 GetAttr2() const { return BasesDefended; }

        uint32 BasesAssaulted; /**< TODO */
        uint32 BasesDefended; /**< TODO */
};

/**
 * @brief
 *
 */
class BattleGroundAB : public BattleGround
{
        friend class BattleGroundMgr;

    public:
        /**
         * @brief
         *
         */
        BattleGroundAB();
        /**
         * @brief
         *
         */
        ~BattleGroundAB();

        /**
         * @brief
         *
         * @param diff
         */
        void Update(uint32 diff) override;
        /**
         * @brief
         *
         * @param plr
         */
        void AddPlayer(Player* plr) override;
        /**
         * @brief
         *
         */
        virtual void StartingEventOpenDoors() override;
        /**
         * @brief
         *
         * @param plr
         * @param guid
         */
        void RemovePlayer(Player* plr, ObjectGuid guid) override;
        /**
         * @brief
         *
         * @param source
         * @param trigger
         */
        bool HandleAreaTrigger(Player* source, uint32 trigger) override;
        /**
         * @brief
         *
         */
        virtual void Reset() override;
        /**
         * @brief
         *
         * @param winner
         */
        void EndBattleGround(Team winner) override;
        /**
         * @brief
         *
         * @param player
         * @return const WorldSafeLocsEntry
         */
        virtual WorldSafeLocsEntry const* GetClosestGraveYard(Player* player) override;

        /* Scorekeeping */
        /**
         * @brief
         *
         * @param source
         * @param type
         * @param value
         */
        virtual void UpdatePlayerScore(Player* source, uint32 type, uint32 value) override;

        /**
         * @brief
         *
         * @param data
         * @param count
         */
        virtual void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

        /* Nodes occupying */
        /**
         * @brief
         *
         * @param source
         * @param target_obj
         */
        virtual void EventPlayerClickedOnFlag(Player* source, GameObject* target_obj) override;

        /* Premature finish */
        /**
         * @brief
         *
         */
        virtual Team GetPrematureWinner() override;

    private:
        /* Gameobject spawning/despawning */
        /**
         * @brief
         *
         * @param node
         * @param type
         * @param teamIndex
         * @param delay
         */
        void _CreateBanner(uint8 node, uint8 type, uint8 teamIndex, bool delay);
        /**
         * @brief
         *
         * @param node
         */
        void _SendNodeUpdate(uint8 node);

        /* Creature spawning/despawning */
        // TODO: working, scripted peons spawning
        /**
         * @brief
         *
         * @param node
         * @param team
         */
        void _NodeOccupied(uint8 node, Team team);

        /**
         * @brief
         *
         * @param node
         * @return int32
         */
        int32 _GetNodeNameId(uint8 node);

        /* Nodes info:
            0: neutral
            1: ally contested
            2: horde contested
            3: ally occupied
            4: horde occupied     */
        uint8               m_Nodes[BG_AB_NODES_MAX]; /**< TODO */
        uint8               m_prevNodes[BG_AB_NODES_MAX];   /**< used for performant wordlstate-updating */
        BG_AB_BannerTimer   m_BannerTimers[BG_AB_NODES_MAX]; /**< TODO */
        uint32              m_NodeTimers[BG_AB_NODES_MAX]; /**< TODO */
        uint32              m_lastTick[PVP_TEAM_COUNT]; /**< TODO */
        uint32              m_honorScoreTicks[PVP_TEAM_COUNT]; /**< TODO */
        uint32              m_ReputationScoreTics[PVP_TEAM_COUNT]; /**< TODO */
        bool                m_IsInformedNearVictory; /**< TODO */
        uint32              m_honorTicks; /**< TODO */
        uint32              m_ReputationTics; /**< TODO */
};
#endif
