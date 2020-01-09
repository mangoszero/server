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

#ifndef WORLD_PVP_EP
#define WORLD_PVP_EP

#include "Common.h"
#include "OutdoorPvP.h"
#include "Language.h"

/**
 * @brief
 *
 */
enum
{
    TOWER_ID_NORTHPASS                              = 0,
    TOWER_ID_CROWNGUARD                             = 1,
    TOWER_ID_EASTWALL                               = 2,
    TOWER_ID_PLAGUEWOOD                             = 3,
    MAX_EP_TOWERS                                   = 4,

    // spells
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_1            = 11413,
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_2            = 11414,
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_3            = 11415,
    SPELL_ECHOES_OF_LORDAERON_ALLIANCE_4            = 1386,

    SPELL_ECHOES_OF_LORDAERON_HORDE_1               = 30880,
    SPELL_ECHOES_OF_LORDAERON_HORDE_2               = 30683,
    SPELL_ECHOES_OF_LORDAERON_HORDE_3               = 30682,
    SPELL_ECHOES_OF_LORDAERON_HORDE_4               = 29520,

    // graveyards
    GRAVEYARD_ZONE_EASTERN_PLAGUE                   = 139,
    GRAVEYARD_ID_EASTERN_PLAGUE                     = 927,

    // misc
    HONOR_REWARD_PLAGUELANDS                        = 189, // guessed, same factor as silithus

    // npcs
    NPC_SPECTRAL_FLIGHT_MASTER                      = 17209,

    // flight master factions
    FACTION_FLIGHT_MASTER_ALLIANCE                  = 774,
    FACTION_FLIGHT_MASTER_HORDE                     = 775,

    SPELL_SPIRIT_PARTICLES_BLUE                     = 17327,
    SPELL_SPIRIT_PARTICLES_RED                      = 31309,

    // quest
    NPC_CROWNGUARD_TOWER_QUEST_DOODAD               = 17689,
    NPC_EASTWALL_TOWER_QUEST_DOODAD                 = 17690,
    NPC_NORTHPASS_TOWER_QUEST_DOODAD                = 17696,
    NPC_PLAGUEWOOD_TOWER_QUEST_DOODAD               = 17698,

    // alliance
    NPC_LORDAERON_COMMANDER                         = 17635,
    NPC_LORDAERON_SOLDIER                           = 17647,

    // horde
    NPC_LORDAERON_VETERAN                           = 17995,
    NPC_LORDAERON_FIGHTER                           = 17996,

    // gameobjects
    GO_LORDAERON_SHRINE_ALLIANCE                    = 181682,
    GO_LORDAERON_SHRINE_HORDE                       = 181955,
    GO_TOWER_FLAG                                   = 182106,

    // possible shrine auras - not used
    // GO_ALLIANCE_BANNER_AURA                       = 180100,
    // GO_HORDE_BANNER_AURA                          = 180101,

    // capture points
    GO_TOWER_BANNER_NORTHPASS                       = 181899,
    GO_TOWER_BANNER_CROWNGUARD                      = 182096,
    GO_TOWER_BANNER_EASTWALL                        = 182097,
    GO_TOWER_BANNER_PLAGUEWOOD                      = 182098,

    GO_TOWER_BANNER                                 = 182106,   // tower banners around

    // events
    // EVENT_NORTHPASS_WIN_ALLIANCE                  = 10568,
    // EVENT_NORTHPASS_WIN_HORDE                     = 10556,
    // EVENT_NORTHPASS_CONTEST_ALLIANCE              = 10697,
    // EVENT_NORTHPASS_CONTEST_HORDE                 = 10696,
    EVENT_NORTHPASS_PROGRESS_ALLIANCE               = 10699,
    EVENT_NORTHPASS_PROGRESS_HORDE                  = 10698,
    EVENT_NORTHPASS_NEUTRAL_ALLIANCE                = 11151,
    EVENT_NORTHPASS_NEUTRAL_HORDE                   = 11150,

    // EVENT_CROWNGUARD_WIN_ALLIANCE                 = 10570,
    // EVENT_CROWNGUARD_WIN_HORDE                    = 10566,
    // EVENT_CROWNGUARD_CONTEST_ALLIANCE             = 10703,
    // EVENT_CROWNGUARD_CONTEST_HORDE                = 10702,
    EVENT_CROWNGUARD_PROGRESS_ALLIANCE              = 10705,
    EVENT_CROWNGUARD_PROGRESS_HORDE                 = 10704,
    EVENT_CROWNGUARD_NEUTRAL_ALLIANCE               = 11155,
    EVENT_CROWNGUARD_NEUTRAL_HORDE                  = 11154,

    // EVENT_EASTWALL_WIN_ALLIANCE                   = 10569,
    // EVENT_EASTWALL_WIN_HORDE                      = 10565,
    // EVENT_EASTWALL_CONTEST_ALLIANCE               = 10689,
    // EVENT_EASTWALL_CONTEST_HORDE                  = 10690,
    EVENT_EASTWALL_PROGRESS_ALLIANCE                = 10691,
    EVENT_EASTWALL_PROGRESS_HORDE                   = 10692,
    EVENT_EASTWALL_NEUTRAL_ALLIANCE                 = 11149,
    EVENT_EASTWALL_NEUTRAL_HORDE                    = 11148,

    // EVENT_PLAGUEWOOD_WIN_ALLIANCE                 = 10567,
    // EVENT_PLAGUEWOOD_WIN_HORDE                    = 10564,
    // EVENT_PLAGUEWOOD_CONTEST_ALLIANCE             = 10687,
    // EVENT_PLAGUEWOOD_CONTEST_HORDE                = 10688,
    EVENT_PLAGUEWOOD_PROGRESS_ALLIANCE              = 10701,
    EVENT_PLAGUEWOOD_PROGRESS_HORDE                 = 10700,
    EVENT_PLAGUEWOOD_NEUTRAL_ALLIANCE               = 11153,
    EVENT_PLAGUEWOOD_NEUTRAL_HORDE                  = 11152,

    // world states
    WORLD_STATE_EP_TOWER_COUNT_ALLIANCE             = 2327,
    WORLD_STATE_EP_TOWER_COUNT_HORDE                = 2328,

    // WORLD_STATE_EP_PLAGUEWOOD_CONTEST_ALLIANCE    = 2366,
    // WORLD_STATE_EP_PLAGUEWOOD_CONTEST_HORDE       = 2367,
    // WORLD_STATE_EP_PLAGUEWOOD_PROGRESS_ALLIANCE   = 2368,
    // WORLD_STATE_EP_PLAGUEWOOD_PROGRESS_HORDE      = 2369,
    WORLD_STATE_EP_PLAGUEWOOD_ALLIANCE              = 2370,
    WORLD_STATE_EP_PLAGUEWOOD_HORDE                 = 2371,
    WORLD_STATE_EP_PLAGUEWOOD_NEUTRAL               = 2353,

    // WORLD_STATE_EP_NORTHPASS_CONTEST_ALLIANCE     = 2362,
    // WORLD_STATE_EP_NORTHPASS_CONTEST_HORDE        = 2363,
    // WORLD_STATE_EP_NORTHPASS_PROGRESS_ALLIANCE    = 2364,
    // WORLD_STATE_EP_NORTHPASS_PROGRESS_HORDE       = 2365,
    WORLD_STATE_EP_NORTHPASS_ALLIANCE               = 2372,
    WORLD_STATE_EP_NORTHPASS_HORDE                  = 2373,
    WORLD_STATE_EP_NORTHPASS_NEUTRAL                = 2352,

    // WORLD_STATE_EP_EASTWALL_CONTEST_ALLIANCE      = 2359,
    // WORLD_STATE_EP_EASTWALL_CONTEST_HORDE         = 2360,
    // WORLD_STATE_EP_EASTWALL_PROGRESS_ALLIANCE     = 2357,
    // WORLD_STATE_EP_EASTWALL_PROGRESS_HORDE        = 2358,
    WORLD_STATE_EP_EASTWALL_ALLIANCE                = 2354,
    WORLD_STATE_EP_EASTWALL_HORDE                   = 2356,
    WORLD_STATE_EP_EASTWALL_NEUTRAL                 = 2361,

    // WORLD_STATE_EP_CROWNGUARD_CONTEST_ALLIANCE    = 2374,
    // WORLD_STATE_EP_CROWNGUARD_CONTEST_HORDE       = 2375,
    // WORLD_STATE_EP_CROWNGUARD_PROGRESS_ALLIANCE   = 2376,
    // WORLD_STATE_EP_CROWNGUARD_PROGRESS_HORDE      = 2377,
    WORLD_STATE_EP_CROWNGUARD_ALLIANCE              = 2378,
    WORLD_STATE_EP_CROWNGUARD_HORDE                 = 2379,
    WORLD_STATE_EP_CROWNGUARD_NEUTRAL               = 2355
};

/**
 * @brief
 *
 */
struct PlaguelandsTowerBuff
{
    uint32 spellIdAlliance, spellIdHorde; /**< TODO */
};

static const PlaguelandsTowerBuff plaguelandsTowerBuffs[MAX_EP_TOWERS] = /**< TODO */
{
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_1, SPELL_ECHOES_OF_LORDAERON_HORDE_1},
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_2, SPELL_ECHOES_OF_LORDAERON_HORDE_2},
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_3, SPELL_ECHOES_OF_LORDAERON_HORDE_3},
    {SPELL_ECHOES_OF_LORDAERON_ALLIANCE_4, SPELL_ECHOES_OF_LORDAERON_HORDE_4}
};

static const float plaguelandsTowerLocations[MAX_EP_TOWERS][2] = /**< capture points coordinates to sort the banners */
{
    {3181.08f, -4379.36f},       // Northpass
    {1860.85f, -3731.23f},       // Crownguard
    {2574.51f, -4794.89f},       // Eastwall
    {2962.71f, -3042.31f}        // Plaguewood
};

/**
 * @brief
 *
 */
struct PlaguelandsTowerEvent
{
    uint32  eventEntry; /**< TODO */
    Team    team; /**< TODO */
    uint32  defenseMessage; /**< TODO */
    uint32  worldState; /**< TODO */
};

static const PlaguelandsTowerEvent plaguelandsTowerEvents[MAX_EP_TOWERS][4] = /**< TODO */
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

static const uint32 plaguelandsBanners[MAX_EP_TOWERS] = {GO_TOWER_BANNER_NORTHPASS, GO_TOWER_BANNER_CROWNGUARD, GO_TOWER_BANNER_EASTWALL, GO_TOWER_BANNER_PLAGUEWOOD}; /**< TODO */

/**
 * @brief
 *
 */
class OutdoorPvPEP : public OutdoorPvP
{
    public:
        /**
         * @brief
         *
         */
        OutdoorPvPEP();

        /**
         * @brief
         *
         * @param player
         * @param isMainZone
         */
        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;
        /**
         * @brief
         *
         * @param player
         * @param isMainZone
         */
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;
        /**
         * @brief
         *
         * @param data
         * @param count
         */
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;
        /**
         * @brief
         *
         * @param player
         */
        void SendRemoveWorldStates(Player* player) override;

        /**
         * @brief
         *
         * @param eventId
         * @param go
         * @return bool
         */
        bool HandleEvent(uint32 eventId, GameObject* go) override;
        /**
         * @brief
         *
         * @param eventId
         * @param players
         * @param team
         */
        void HandleObjectiveComplete(uint32 eventId, const std::list<Player*> &players, Team team) override;

        /**
         * @brief
         *
         * @param creature
         */
        void HandleCreatureCreate(Creature* creature) override;
        /**
         * @brief
         *
         * @param go
         */
        void HandleGameObjectCreate(GameObject* go) override;
        /**
         * @brief
         *
         * @param player
         * @param go
         * @return bool
         */
        bool HandleGameObjectUse(Player* player, GameObject* go) override;

    private:
        /**
         * @brief process capture events
         *
         * @param go
         * @param towerId
         * @param team
         * @param newWorldState
         * @return bool
         */
        bool ProcessCaptureEvent(GameObject* go, uint32 towerId, Team team, uint32 newWorldState);

        /**
         * @brief
         *
         * @param go
         * @param towerId
         */
        void InitBanner(GameObject* go, uint32 towerId);

        /**
         * @brief Plaguewood bonus - flight master
         *
         * @param objRef
         */
        void UnsummonFlightMaster(const WorldObject* objRef);
        /**
         * @brief Eastwall bonus - soldiers
         *
         * @param objRef
         */
        void UnsummonSoldiers(const WorldObject* objRef);

        Team m_towerOwner[MAX_EP_TOWERS]; /**< TODO */
        uint32 m_towerWorldState[MAX_EP_TOWERS]; /**< TODO */
        uint8 m_towersAlliance; /**< TODO */
        uint8 m_towersHorde; /**< TODO */

        ObjectGuid m_flightMaster; /**< TODO */
        ObjectGuid m_lordaeronShrineAlliance; /**< TODO */
        ObjectGuid m_lordaeronShrineHorde; /**< TODO */

        GuidList m_soldiers; /**< TODO */

        GuidList m_towerBanners[MAX_EP_TOWERS]; /**< TODO */
};

#endif
