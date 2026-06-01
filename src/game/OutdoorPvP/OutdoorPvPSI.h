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

#ifndef WORLD_PVP_SI
#define WORLD_PVP_SI

#include "Common.h"
#include "OutdoorPvP.h"

/**
 * @brief Silithus outdoor PvP constants
 *
 * Contains NPC, game object, spell, quest, area trigger,
 * and world state IDs for the Silithus outdoor PvP zone.
 */
enum
{
    // npcs
    NPC_SILITHUS_DUST_QUEST_ALLIANCE = 17090,  ///< Alliance NPC for quest credit
    NPC_SILITHUS_DUST_QUEST_HORDE    = 18199,  ///< Horde NPC for quest credit

    // game objects
    GO_SILITHYST_MOUND               = 181597, ///< Mound created when player drops flag
    GO_SILITHYST_GEYSER              = 181598, ///< Geyser spawned on map by default

    // spells
//  SPELL_SILITHYST_OBJECT           = 29518,  ///< unknown, related to the GO
    SPELL_SILITHYST                  = 29519,  ///< Buff when carrying silithyst
    SPELL_TRACES_OF_SILITHYST        = 29534,  ///< Individual buff after delivering silithyst
    SPELL_CENARION_FAVOR             = 30754,  ///< Zone buff when team gathers 200 silithyst
    SPELL_SILITHYST_FLAG_DROP        = 29533,  ///< Drop the flag spell

    // quests
    QUEST_SCOURING_DESERT_ALLIANCE   = 9419,   ///< Alliance scouring desert quest
    QUEST_SCOURING_DESERT_HORDE      = 9422,   ///< Horde scouring desert quest

    // area triggers
    AREATRIGGER_SILITHUS_ALLIANCE    = 4162,   ///< Alliance area trigger
    AREATRIGGER_SILITHUS_HORDE       = 4168,   ///< Horde area trigger

    // misc
    FACTION_CENARION_CIRCLE          = 609,    ///< Cenarion Circle faction
    HONOR_REWARD_SILITHYST           = 199,    ///< Honor reward per silithyst
    REPUTATION_REWARD_SILITHYST      = 20,     ///< Reputation reward per silithyst
    MAX_SILITHYST                    = 200,    ///< Maximum silithyst for zone buff

    // world states
    WORLD_STATE_SI_GATHERED_A        = 2313,   ///< Alliance silithyst gathered count
    WORLD_STATE_SI_GATHERED_H        = 2314,   ///< Horde silithyst gathered count
    WORLD_STATE_SI_SILITHYST_MAX     = 2317    ///< Maximum silithyst world state
};

/**
 * @brief Silithus outdoor PvP implementation
 *
 * Handles the Silithus outdoor PvP zone where players collect
 * silithyst from geysers and deliver them to their faction's
 * base to earn honor and reputation rewards.
 */
class OutdoorPvPSI : public OutdoorPvP
{
    public:
        /**
         * @brief Constructor
         */
        OutdoorPvPSI();

        /**
         * @brief Handle player entering Silithus zone
         * @param player Player entering the zone
         * @param isMainZone True if player entered main zone, false if subzone
         */
        void HandlePlayerEnterZone(Player* player, bool isMainZone) override;

        /**
         * @brief Handle player leaving Silithus zone
         * @param player Player leaving the zone
         * @param isMainZone True if player left main zone, false if subzone
         */
        void HandlePlayerLeaveZone(Player* player, bool isMainZone) override;

        /**
         * @brief Fill initial world states for Silithus
         * @param data World packet to fill with state data
         * @param count Count of world states added
         */
        void FillInitialWorldStates(WorldPacket& data, uint32& count) override;

        /**
         * @brief Handle area trigger in Silithus
         * @param player Player triggering the area
         * @param triggerId Area trigger ID
         * @return True if event was handled, false otherwise
         */
        bool HandleAreaTrigger(Player* player, uint32 triggerId) override;

        /**
         * @brief Handle game object use in Silithus
         * @param player Player using the game object
         * @param go Game object being used
         * @return True if event was handled, false otherwise
         */
        bool HandleGameObjectUse(Player* player, GameObject* go) override;

        /**
         * @brief Handle flag drop in Silithus
         * @param player Player dropping the flag
         * @param spellId Spell ID associated with the flag
         * @return True if event was handled, false otherwise
         */
        bool HandleDropFlag(Player* player, uint32 spellId) override;

    private:
        uint8 m_resourcesAlliance; ///< Alliance silithyst resources gathered
        uint8 m_resourcesHorde; ///< Horde silithyst resources gathered
        Team m_zoneOwner; ///< Current zone owner team
};

#endif
