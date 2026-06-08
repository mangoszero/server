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

#ifndef OUTDOOR_PVP_H
#define OUTDOOR_PVP_H

#include "Common.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "OutdoorPvPMgr.h"

// forward declaration
class WorldPacket;
class WorldObject;
class Player;
class GameObject;
class Unit;
class Creature;

/**
 * @brief Capture point art kit IDs for different factions
 */
enum CapturePointArtKits
{
    CAPTURE_ARTKIT_ALLIANCE = 2, ///< Alliance capture point art kit
    CAPTURE_ARTKIT_HORDE    = 1, ///< Horde capture point art kit
    CAPTURE_ARTKIT_NEUTRAL  = 21 ///< Neutral capture point art kit
};

/**
 * @brief Capture point animation IDs for different factions
 */
enum CapturePointAnimations
{
    CAPTURE_ANIM_ALLIANCE   = 1, ///< Alliance capture animation
    CAPTURE_ANIM_HORDE      = 0, ///< Horde capture animation
    CAPTURE_ANIM_NEUTRAL    = 2  ///< Neutral capture animation
};

/**
 * @brief Map of player GUIDs to zone status
 */
typedef std::map < ObjectGuid /*playerGuid*/, bool /*isMainZone*/ > GuidZoneMap;

/**
 * @brief Base class for outdoor PvP zones
 *
 * Provides framework for managing outdoor PvP areas including
 * capture points, player tracking, and zone-specific events.
 */
class OutdoorPvP
{
    friend class OutdoorPvPMgr;

    public:
        /**
         * @brief Constructor
         */
        OutdoorPvP() {}

        /**
         * @brief Destructor
         */
        virtual ~OutdoorPvP() {}

        /**
         * @brief Fill initial world states when zone is initialized
         * @param data World packet to fill with state data
         * @param count Count of world states added
         */
        virtual void FillInitialWorldStates(WorldPacket& /*data*/, uint32& /*count*/) {}

        /**
         * @brief Process capture event
         * @param eventId Event ID to handle
         * @param go Game object involved in the event
         * @return True if event was handled, false otherwise
         */
        virtual bool HandleEvent(uint32 /*eventId*/, GameObject* /*go*/) { return false; }

        /**
         * @brief Handle capture objective complete event
         * @param eventId Event ID for objective completion
         * @param players List of players involved in the objective
         * @param team Team that completed the objective
         */
        virtual void HandleObjectiveComplete(uint32 /*eventId*/, const std::list<Player*> &/*players*/, Team /*team*/) {}

        /**
         * @brief Called when a creature is created in the zone
         * @param creature Creature that was created
         */
        virtual void HandleCreatureCreate(Creature* /*creature*/) {}

        /**
         * @brief Called when a game object is created in the zone
         * @param go Game object that was created
         */
        virtual void HandleGameObjectCreate(GameObject* /*go*/);

        /**
         * @brief Called when a game object is removed from the zone
         * @param go Game object that was removed
         */
        virtual void HandleGameObjectRemove(GameObject* /*go*/);

        /**
         * @brief Called when a creature dies in the zone
         * @param creature Creature that died
         */
        virtual void HandleCreatureDeath(Creature* /*creature*/) {}

        /**
         * @brief Called when a player uses a game object related to outdoor PvP
         * @param player Player using the game object
         * @param go Game object being used
         * @return True if event was handled, false otherwise
         */
        virtual bool HandleGameObjectUse(Player* /*player*/, GameObject* /*go*/) { return false; }

        /**
         * @brief Called when a player triggers an area trigger
         * @param player Player triggering the area trigger
         * @param triggerId Area trigger ID
         * @return True if event was handled, false otherwise
         */
        virtual bool HandleAreaTrigger(Player* /*player*/, uint32 /*triggerId*/) { return false; }

        /**
         * @brief Called when a player drops a flag
         * @param player Player dropping the flag
         * @param spellId Spell ID associated with the flag
         * @return True if event was handled, false otherwise
         */
        virtual bool HandleDropFlag(Player* /*player*/, uint32 /*spellId*/) { return false; }

        /**
         * @brief Update outdoor PvP zone state
         * @param diff Time difference since last update in milliseconds
         */
        virtual void Update(uint32 /*diff*/) {}

        /**
         * @brief Handle player kill event
         * @param killer Player who killed the victim
         * @param victim Player who was killed
         */
        void HandlePlayerKill(Player* killer, Player* victim);

    protected:

        // Player related stuff

        /**
         * @brief Called when a player enters the zone
         * @param player Player entering the zone
         * @param isMainZone True if player entered main zone, false if subzone
         */
        virtual void HandlePlayerEnterZone(Player* /*player*/, bool /*isMainZone*/);

        /**
         * @brief Called when a player leaves the zone
         * @param player Player leaving the zone
         * @param isMainZone True if player left main zone, false if subzone
         */
        virtual void HandlePlayerLeaveZone(Player* /*player*/, bool /*isMainZone*/);

        /**
         * @brief Remove world states for a player
         * @param player Player to remove world states for
         */
        virtual void SendRemoveWorldStates(Player* /*player*/) {}

        /**
         * @brief Handle player kill inside area
         * @param killer Player who made the kill
         */
        virtual void HandlePlayerKillInsideArea(Player* /*killer*/) {}

        /**
         * @brief Send world state update to all players in the zone
         * @param field World state field ID
         * @param value World state value
         */
        void SendUpdateWorldState(uint32 field, uint32 value);

        /**
         * @brief Apply or remove buff to a team in the zone
         * @param team Team to apply buff to
         * @param spellId Spell ID of the buff
         * @param remove True to remove buff, false to apply
         */
        void BuffTeam(Team team, uint32 spellId, bool remove = false);

        /**
         * @brief Get banner art kit based on controlling team
         * @param team Controlling team
         * @param artKitAlliance Art kit for Alliance control
         * @param artKitHorde Art kit for Horde control
         * @param artKitNeutral Art kit for neutral control
         * @return Appropriate art kit ID
         */
        uint32 GetBannerArtKit(Team team, uint32 artKitAlliance = CAPTURE_ARTKIT_ALLIANCE, uint32 artKitHorde = CAPTURE_ARTKIT_HORDE, uint32 artKitNeutral = CAPTURE_ARTKIT_NEUTRAL);

        /**
         * @brief Set banner visual by GUID
         * @param objRef Reference world object for context
         * @param goGuid GUID of the game object to update
         * @param artKit Art kit to apply
         * @param animId Animation ID to play
         */
        void SetBannerVisual(const WorldObject* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId);

        /**
         * @brief Set banner visual directly on game object
         * @param go Game object to update
         * @param artKit Art kit to apply
         * @param animId Animation ID to play
         */
        void SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId);

        /**
         * @brief Handle game object spawn or despawn
         * @param objRef Reference world object for context
         * @param goGuid GUID of the game object to respawn
         * @param respawn True to spawn, false to despawn
         */
        void RespawnGO(const WorldObject* objRef, ObjectGuid goGuid, bool respawn);

        GuidZoneMap m_zonePlayers; ///< Map of players inside the area
};

#endif
