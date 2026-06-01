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

#ifndef WORLD_PVP_EP
#define WORLD_PVP_EP

#include "Common.h"
#include "OutdoorPvP.h"
#include "Language.h"

/**
 * @brief Eastern Plaguelands outdoor PvP constants
 *
 * Contains tower IDs, spells, NPCs, game objects, events,
 * and world states for the Eastern Plaguelands outdoor PvP zone.
 */
enum
{
    TOWER_ID_NORTHPASS = 0,  ///< Northpass tower ID
    TOWER_ID_CROWNGUARD = 1, ///< Crownguard tower ID
    TOWER_ID_EASTWALL = 2,   ///< Eastwall tower ID
    TOWER_ID_PLAGUEWOOD = 3, ///< Plaguewood tower ID
    MAX_EP_TOWERS = 4,       ///< Maximum number of towers

    // spells
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_1 = 11413, ///< Alliance buff for 1 tower
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_2 = 11414, ///< Alliance buff for 2 towers
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_3 = 11415, ///< Alliance buff for 3 towers
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_4 = 1386,  ///< Alliance buff for 4 towers

    SPELL_ECHOES_OF_LORDAERON_HORDE_1 = 30880, ///< Horde buff for 1 tower
    SPELL_ECHOES_OF_LORDAERON_HORDE_2 = 30683, ///< Horde buff for 2 towers
    SPELL_ECHOES_OF_LORDAERON_HORDE_3 = 30682, ///< Horde buff for 3 towers
    SPELL_ECHOES_OF_LORDAERON_HORDE_4 = 29520, ///< Horde buff for 4 towers

    // graveyards
    GRAVEYARD_ZONE_EASTERN_PLAGUE = 139, ///< Eastern Plaguelands zone ID
    GRAVEYARD_ID_EASTERN_PLAGUE = 927,   ///< Eastern Plaguelands graveyard ID

    // misc
    HONOR_REWARD_PLAGUELANDS = 189, ///< Honor reward per tower

    // npcs
    NPC_SPECTRAL_FLIGHT_MASTER = 17209, ///< Spectral flight master NPC

    // flight master factions
    FACTION_FLIGHT_MASTER_ALLIANCE = 774, ///< Alliance flight master faction
    FACTION_FLIGHT_MASTER_HORDE = 775,    ///< Horde flight master faction

    SPELL_SPIRIT_PARTICLES_BLUE = 17327,  ///< Blue spirit particles visual
    SPELL_SPIRIT_PARTICLES_RED = 31309,   ///< Red spirit particles visual

    // quest NPCs
    NPC_CROWNGUARD_TOWER_QUEST_DOODAD = 17689, ///< Crownguard tower quest NPC
    NPC_EASTWALL_TOWER_QUEST_DOODAD = 17690,   ///< Eastwall tower quest NPC
    NPC_NORTHPASS_TOWER_QUEST_DOODAD = 17696,  ///< Northpass tower quest NPC
    NPC_PLAGUEWOOD_TOWER_QUEST_DOODAD = 17698, ///< Plaguewood tower quest NPC

    // alliance NPCs
    NPC_LORDAERON_COMMANDER = 17635, ///< Lordaeron commander
    NPC_LORDAERON_SOLDIER = 17647,   ///< Lordaeron soldier

    // horde NPCs
    NPC_LORDAERON_VETERAN = 17995, ///< Lordaeron veteran
    NPC_LORDAERON_FIGHTER = 17996, ///< Lordaeron fighter

    // gameobjects
    GO_LORDAERON_SHRINE_ALLIANCE = 181682, ///< Alliance Lordaeron shrine
    GO_LORDAERON_SHRINE_HORDE = 181955,    ///< Horde Lordaeron shrine
    GO_TOWER_FLAG = 182106,                ///< Tower flag

    // possible shrine auras - not used
    // GO_ALLIANCE_BANNER_AURA                       = 180100,
    // GO_HORDE_BANNER_AURA                          = 180101,

    // capture point banners
    GO_TOWER_BANNER_NORTHPASS = 181899,  ///< Northpass tower banner
    GO_TOWER_BANNER_CROWNGUARD = 182096, ///< Crownguard tower banner
    GO_TOWER_BANNER_EASTWALL = 182097,   ///< Eastwall tower banner
    GO_TOWER_BANNER_PLAGUEWOOD = 182098, ///< Plaguewood tower banner
    GO_TOWER_BANNER = 182106,            ///< Generic tower banner

    // events
//  EVENT_NORTHPASS_WIN_ALLIANCE = 10568,
//  EVENT_NORTHPASS_WIN_HORDE = 10556,
//  EVENT_NORTHPASS_CONTEST_ALLIANCE = 10697,
//  EVENT_NORTHPASS_CONTEST_HORDE = 10696,
    EVENT_NORTHPASS_PROGRESS_ALLIANCE = 10699, ///< Northpass Alliance progress
    EVENT_NORTHPASS_PROGRESS_HORDE = 10698,    ///< Northpass Horde progress
    EVENT_NORTHPASS_NEUTRAL_ALLIANCE = 11151,  ///< Northpass neutral from Alliance
    EVENT_NORTHPASS_NEUTRAL_HORDE = 11150,     ///< Northpass neutral from Horde

//  EVENT_CROWNGUARD_WIN_ALLIANCE = 10570,
//  EVENT_CROWNGUARD_WIN_HORDE = 10566,
//  EVENT_CROWNGUARD_CONTEST_ALLIANCE = 10703,
//  EVENT_CROWNGUARD_CONTEST_HORDE = 10702,
    EVENT_CROWNGUARD_PROGRESS_ALLIANCE = 10705, ///< Crownguard Alliance progress
    EVENT_CROWNGUARD_PROGRESS_HORDE = 10704,    ///< Crownguard Horde progress
    EVENT_CROWNGUARD_NEUTRAL_ALLIANCE = 11155,  ///< Crownguard neutral from Alliance
    EVENT_CROWNGUARD_NEUTRAL_HORDE = 11154,     ///< Crownguard neutral from Horde

//  EVENT_EASTWALL_WIN_ALLIANCE = 10569,
//  EVENT_EASTWALL_WIN_HORDE = 10565,
//  EVENT_EASTWALL_CONTEST_ALLIANCE = 10689,
//  EVENT_EASTWALL_CONTEST_HORDE = 10690,
    EVENT_EASTWALL_PROGRESS_ALLIANCE = 10691, ///< Eastwall Alliance progress
    EVENT_EASTWALL_PROGRESS_HORDE = 10692,    ///< Eastwall Horde progress
    EVENT_EASTWALL_NEUTRAL_ALLIANCE = 11149,  ///< Eastwall neutral from Alliance
    EVENT_EASTWALL_NEUTRAL_HORDE = 11148,     ///< Eastwall neutral from Horde

//  EVENT_PLAGUEWOOD_WIN_ALLIANCE = 10567,
//  EVENT_PLAGUEWOOD_WIN_HORDE = 10564,
//  EVENT_PLAGUEWOOD_CONTEST_ALLIANCE = 10687,
//  EVENT_PLAGUEWOOD_CONTEST_HORDE = 10688,
    EVENT_PLAGUEWOOD_PROGRESS_ALLIANCE = 10701, ///< Plaguewood Alliance progress
    EVENT_PLAGUEWOOD_PROGRESS_HORDE = 10700,    ///< Plaguewood Horde progress
    EVENT_PLAGUEWOOD_NEUTRAL_ALLIANCE = 11153,  ///< Plaguewood neutral from Alliance
    EVENT_PLAGUEWOOD_NEUTRAL_HORDE = 11152,     ///< Plaguewood neutral from Horde

    // world states
    WORLD_STATE_EP_TOWER_COUNT_ALLIANCE = 2327, ///< Alliance tower count
    WORLD_STATE_EP_TOWER_COUNT_HORDE = 2328,    ///< Horde tower count

//  WORLD_STATE_EP_PLAGUEWOOD_CONTEST_ALLIANCE = 2366,
//  WORLD_STATE_EP_PLAGUEWOOD_CONTEST_HORDE = 2367,
//  WORLD_STATE_EP_PLAGUEWOOD_PROGRESS_ALLIANCE = 2368,
//  WORLD_STATE_EP_PLAGUEWOOD_PROGRESS_HORDE = 2369,
    WORLD_STATE_EP_PLAGUEWOOD_ALLIANCE = 2370, ///< Plaguewood Alliance control
    WORLD_STATE_EP_PLAGUEWOOD_HORDE = 2371,    ///< Plaguewood Horde control
    WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL = 2353,  ///< Plaguewood neutral

//  WORLD_STATE_EP_NORTHPASS_CONTEST_ALLIANCE = 2362,
//  WORLD_STATE_EP_NORTHPASS_CONTEST_HORDE = 2363,
//  WORLD_STATE_EP_NORTHPASS_PROGRESS_ALLIANCE = 2364,
//  WORLD_STATE_EP_NORTHPASS_PROGRESS_HORDE  = 2365,
    WORLD_STATE_EP_NORTHPASS_ALLIANCE = 2372, ///< Northpass Alliance control
    WORLD_STATE_EP_NORTHPASS_HORDE = 2373,    ///< Northpass Horde control
    WORLD_STATE_EP_NORTHPASS_NEUTRAL = 2352,  ///< Northpass neutral

//  WORLD_STATE_EP_EASTWALL_CONTEST_ALLIANCE = 2359,
//  WORLD_STATE_EP_EASTWALL_CONTEST_HORDE = 2360,
//  WORLD_STATE_EP_EASTWALL_PROGRESS_ALLIANCE = 2357,
//  WORLD_STATE_EP_EASTWALL_PROGRESS_HORDE = 2358,
    WORLD_STATE_EP_EASTWALL_ALLIANCE = 2354, ///< Eastwall Alliance control
    WORLD_STATE_EP_EASTWALL_HORDE = 2356,    ///< Eastwall Horde control
    WORLD_STATE_EP_EASTWALL_NEUTRAL = 2361,  ///< Eastwall neutral

//  WORLD_STATE_EP_CROWNGUARD_CONTEST_ALLIANCE = 2374,
//  WORLD_STATE_EP_CROWNGUARD_CONTEST_HORDE = 2375,
//  WORLD_STATE_EP_CROWNGUARD_PROGRESS_ALLIANCE = 2376,
//  WORLD_STATE_EP_CROWNGUARD_PROGRESS_HORDE = 2377,
    WORLD_STATE_EP_CROWNGUARD_ALLIANCE = 2378, ///< Crownguard Alliance control
    WORLD_STATE_EP_CROWNGUARD_HORDE = 2379,    ///< Crownguard Horde control
    WORLD_STATE_EP_CROWNGUARD_NEUTRAL = 2355   ///< Crownguard neutral
};

/**
 * @brief Tower buff configuration for Plaguelands
 */
struct PlaguelandsTowerBuff
{
    uint32 spellIdAlliance; ///< Alliance buff spell ID
    uint32 spellIdHorde;    ///< Horde buff spell ID
};

/**
 * @brief Array of tower buffs for each tower
 */
static const PlaguelandsTowerBuff plaguelandsTowerBuffs[MAX_EP_TOWERS] =
{
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_1, SPELL_ECHOES_OF_LORDAERON_HORDE_1},
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_2, SPELL_ECHOES_OF_LORDAERON_HORDE_2},
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_3, SPELL_ECHOES_OF_LORDAERON_HORDE_3},
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_4, SPELL_ECHOES_OF_LORDAERON_HORDE_4}
};

/**
 * @brief Tower coordinates for banner sorting
 */
static const float plaguelandsTowerLocations[MAX_EP_TOWERS][2] =
{
    {3181.08f, -4379.36f},       // Northpass
    {1860.85f, -3731.23f},       // Crownguard
    {2574.51f, -4794.89f},       // Eastwall
    {2962.71f, -3042.31f}        // Plaguewood
};

/**
 * @brief Tower event configuration
 */
struct PlaguelandsTowerEvent
{
    uint32 eventEntry; ///< Game event entry
    Team team; ///< Team associated with event
    uint32 defenseMessage; ///< Defense message language ID
    uint32 worldState; ///< World state to update
};

/**
 * @brief Array of tower events for each tower
 */
static const PlaguelandsTowerEvent plaguelandsTowerEvents[MAX_EP_TOWERS][4] =
{
    {
        {EVENT_NORTHPASS_PROGRESS_ALLIANCE,     ALLIANCE,   LANG_OPVP_EP_CAPTURE_NPT_A, WORLD_STATE_EP_NORTHPASS_ALLIANCE},
        {EVENT_NORTHPASS_PROGRESS_HORDE,        HORDE,      LANG_OPVP_EP_CAPTURE_NPT_H, WORLD_STATE_EP_NORTHPASS_HORDE},
        {EVENT_NORTHPASS_NEUTRAL_HORDE,         TEAM_NONE,  0,                          WORLD_STATE_EP_NORTHPASS_NEUTRAL},
        {EVENT_NORTHPASS_NEUTRAL_ALLIANCE,      TEAM_NONE,  0,                          WORLD_STATE_EP_NORTHPASS_NEUTRAL},
    },
    {
        {EVENT_CROWNGUARD_PROGRESS_ALLIANCE,    ALLIANCE,   LANG_OPVP_EP_CAPTURE_CGT_A, WORLD_STATE_EP_CROWNGUARD_ALLIANCE},
        {EVENT_CROWNGUARD_PROGRESS_HORDE,       HORDE,      LANG_OPVP_EP_CAPTURE_CGT_H, WORLD_STATE_EP_CROWNGUARD_HORDE},
        {EVENT_CROWNGUARD_NEUTRAL_HORDE,        TEAM_NONE,  0,                          WORLD_STATE_EP_CROWNGUARD_NEUTRAL},
        {EVENT_CROWNGUARD_NEUTRAL_ALLIANCE,     TEAM_NONE,  0,                          WORLD_STATE_EP_CROWNGUARD_NEUTRAL},
    },
    {
        {EVENT_EASTWALL_PROGRESS_ALLIANCE,      ALLIANCE,   LANG_OPVP_EP_CAPTURE_EWT_A, WORLD_STATE_EP_EASTWALL_ALLIANCE},
        {EVENT_EASTWALL_PROGRESS_HORDE,         HORDE,      LANG_OPVP_EP_CAPTURE_EWT_H, WORLD_STATE_EP_EASTWALL_HORDE},
        {EVENT_EASTWALL_NEUTRAL_HORDE,          TEAM_NONE,  0,                          WORLD_STATE_EP_EASTWALL_NEUTRAL},
        {EVENT_EASTWALL_NEUTRAL_ALLIANCE,       TEAM_NONE,  0,                          WORLD_STATE_EP_EASTWALL_NEUTRAL},
    },
    {
        {EVENT_PLAGUEWOOD_PROGRESS_ALLIANCE,    ALLIANCE,   LANG_OPVP_EP_CAPTURE_PWT_A, WORLD_STATE_EP_PLAGUEWOOD_ALLIANCE},
        {EVENT_PLAGUEWOOD_PROGRESS_HORDE,       HORDE,      LANG_OPVP_EP_CAPTURE_PWT_H, WORLD_STATE_EP_PLAGUEWOOD_HORDE},
        {EVENT_PLAGUEWOOD_NEUTRAL_HORDE,        TEAM_NONE,  0,                          WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL},
        {EVENT_PLAGUEWOOD_NEUTRAL_ALLIANCE,     TEAM_NONE,  0,                          WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL},
    },
};

/**
 * @brief Array of tower banner game object IDs
 */
static const uint32 plaguelandsBanners[MAX_EP_TOWERS] = {GO_TOWER_BANNER_NORTHPASS, GO_TOWER_BANNER_CROWNGUARD, GO_TOWER_BANNER_EASTWALL, GO_TOWER_BANNER_PLAGUEWOOD};

/**
 * @brief Eastern Plaguelands outdoor PvP implementation
 *
 * Handles the Eastern Plaguelands outdoor PvP zone where players
 * capture and defend four towers to earn zone-wide buffs for their faction.
 */
class OutdoorPvPEP : public OutdoorPvP
{
    public:
        /**
         * @brief Constructor
         */
        OutdoorPvPEP();

        /**
         * @brief Handle player entering Eastern Plaguelands zone
         * @param player Player entering the zone
         * @param isMainZone True if player entered main zone, false if subzone
         */
        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;

        /**
         * @brief Handle player leaving Eastern Plaguelands zone
         * @param player Player leaving the zone
         * @param isMainZone True if player left main zone, false if subzone
         */
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;

        /**
         * @brief Fill initial world states for Eastern Plaguelands
         * @param data World packet to fill with state data
         * @param count Count of world states added
         */
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

        /**
         * @brief Send remove world states to player
         * @param player Player to remove world states for
         */
        void SendRemoveWorldStates(Player* player) override;

        /**
         * @brief Handle capture event
         * @param eventId Event ID
         * @param go Game object involved in event
         * @return True if event was handled, false otherwise
         */
        bool HandleEvent(uint32 eventId, GameObject* go) override;

        /**
         * @brief Handle objective complete event
         * @param eventId Event ID for objective completion
         * @param players List of players involved in the objective
         * @param team Team that completed the objective
         */
        void HandleObjectiveComplete(uint32 eventId, const std::list<Player*> &players, Team team) override;

        /**
         * @brief Handle creature creation in Eastern Plaguelands
         * @param creature Creature that was created
         */
        void HandleCreatureCreate(Creature* creature) override;

        /**
         * @brief Handle game object creation in Eastern Plaguelands
         * @param go Game object that was created
         */
        void HandleGameObjectCreate(GameObject* go) override;

        /**
         * @brief Handle game object use in Eastern Plaguelands
         * @param player Player using the game object
         * @param go Game object being used
         * @return True if event was handled, false otherwise
         */
        bool HandleGameObjectUse(Player* player, GameObject* go) override;

    private:
        /**
         * @brief Process tower capture event
         * @param go Game object being captured
         * @param towerId Tower ID being captured
         * @param team Team capturing the tower
         * @param newWorldState New world state to set
         * @return True if event was processed, false otherwise
         */
        bool ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState);

        /**
         * @brief Initialize tower banner
         * @param go Game object (banner)
         * @param towerId Tower ID
         */
        void InitBanner(GameObject* go, uint32 towerId);

        /**
         * @brief Unsummon flight master (Plaguewood bonus)
         * @param objRef Reference world object for context
         */
        void UnsummonFlightMaster(const WorldObject* objRef);

        /**
         * @brief Unsummon soldiers (Eastwall bonus)
         * @param objRef Reference world object for context
         */
        void UnsummonSoldiers(const WorldObject* objRef);

        Team m_towerOwner[MAX_EP_TOWERS]; ///< Current owner of each tower
        uint32 m_towerWorldState[MAX_EP_TOWERS]; ///< World state for each tower
        uint8 m_towersAlliance; ///< Number of towers controlled by Alliance
        uint8 m_towersHorde; ///< Number of towers controlled by Horde

        ObjectGuid m_flightMaster; ///< GUID of flight master NPC
        ObjectGuid m_lordaeronShrineAlliance; ///< GUID of Alliance shrine
        ObjectGuid m_lordaeronShrineHorde; ///< GUID of Horde shrine

        GuidList m_soldiers; ///< List of soldier NPCs

        GuidList m_towerBanners[MAX_EP_TOWERS]; ///< List of banner GUIDs for each tower
};

#endif
