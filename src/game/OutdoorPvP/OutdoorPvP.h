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

#ifndef OUTDOOR_PVP_H
#define OUTDOOR_PVP_H

#include "Common.h"
#include "ObjectGuid.h"
#include "SharedDefines.h"
#include "OutdoorPvPMgr.h"

class WorldPacket;
class WorldObject;
class Player;
class GameObject;
class Unit;
class Creature;

/**
 * @brief
 *
 */
enum CapturePointArtKits
{
    CAPTURE_ARTKIT_ALLIANCE = 2,
    CAPTURE_ARTKIT_HORDE    = 1,
    CAPTURE_ARTKIT_NEUTRAL  = 21
};

/**
 * @brief
 *
 */
enum CapturePointAnimations
{
    CAPTURE_ANIM_ALLIANCE   = 1,
    CAPTURE_ANIM_HORDE      = 0,
    CAPTURE_ANIM_NEUTRAL    = 2
};

/**
 * @brief
 *
 */
typedef std::map < ObjectGuid /*playerGuid*/, bool /*isMainZone*/ > GuidZoneMap;

/**
 * @brief
 *
 */
class OutdoorPvP
{
        friend class OutdoorPvPMgr;

    public:
        /**
         * @brief
         *
         */
        OutdoorPvP() {}
        /**
         * @brief
         *
         */
        virtual ~OutdoorPvP() {}

        /**
         * @brief called when the zone is initialized
         *
         * @param
         * @param
         */
        virtual void FillInitialWorldStates(WorldPacket& /*data*/, uint32& /*count*/) {}

        /**
         * @brief Process Capture event
         *
         * @param uint32
         * @param
         * @return bool
         */
        virtual bool HandleEvent(uint32 /*eventId*/, GameObject* /*go*/) { return false; }

        /**
         * @brief handle capture objective complete
         *
         * @param uint32
         * @param >
         * @param Team
         */
        virtual void HandleObjectiveComplete(uint32 /*eventId*/, const std::list<Player*> &/*players*/, Team /*team*/) {}

        /**
         * @brief Called when a creature is created
         *
         * @param
         */
        virtual void HandleCreatureCreate(Creature* /*creature*/) {}

        /**
         * @brief Called when a gameobject is created or removed
         *
         * @param
         */
        virtual void HandleGameObjectCreate(GameObject* /*go*/);
        /**
         * @brief
         *
         * @param
         */
        virtual void HandleGameObjectRemove(GameObject* /*go*/);

        /**
         * @brief Called on creature death
         *
         * @param
         */
        virtual void HandleCreatureDeath(Creature* /*creature*/) {}

        /**
         * @brief called when a player uses a gameobject related to outdoor pvp events
         *
         * @param
         * @param
         * @return bool
         */
        virtual bool HandleGameObjectUse(Player* /*player*/, GameObject* /*go*/) { return false; }

        /**
         * @brief called when a player triggers an areatrigger
         *
         * @param
         * @param uint32
         * @return bool
         */
        virtual bool HandleAreaTrigger(Player* /*player*/, uint32 /*triggerId*/) { return false; }

        /**
         * @brief called when a player drops a flag
         *
         * @param
         * @param uint32
         * @return bool
         */
        virtual bool HandleDropFlag(Player* /*player*/, uint32 /*spellId*/) { return false; }

        /**
         * @brief update - called by the OutdoorPvPMgr
         *
         * @param uint32
         */
        virtual void Update(uint32 /*diff*/) {}

        /**
         * @brief Handle player kill
         *
         * @param killer
         * @param victim
         */
        void HandlePlayerKill(Player* killer, Player* victim);

    protected:

        // Player related stuff
        /**
         * @brief
         *
         * @param
         * @param bool
         */
        virtual void HandlePlayerEnterZone(Player* /*player*/, bool /*isMainZone*/);
        /**
         * @brief
         *
         * @param
         * @param bool
         */
        virtual void HandlePlayerLeaveZone(Player* /*player*/, bool /*isMainZone*/);

        //
        /**
         * @brief remove world states
         *
         * @param
         */
        virtual void SendRemoveWorldStates(Player* /*player*/) {}

        /**
         * @brief handle npc/player kill
         *
         * @param
         */
        virtual void HandlePlayerKillInsideArea(Player* /*killer*/) {}

        /**
         * @brief send world state update to all players present
         *
         * @param field
         * @param value
         */
        void SendUpdateWorldState(uint32 field, uint32 value);

        /**
         * @brief applies buff to a team inside the specific zone
         *
         * @param team
         * @param spellId
         * @param remove
         */
        void BuffTeam(Team team, uint32 spellId, bool remove = false);

        /**
         * @brief get banner artkit based on controlling team
         *
         * @param team
         * @param artKitAlliance
         * @param artKitHorde
         * @param artKitNeutral
         * @return uint32
         */
        uint32 GetBannerArtKit(Team team, uint32 artKitAlliance = CAPTURE_ARTKIT_ALLIANCE, uint32 artKitHorde = CAPTURE_ARTKIT_HORDE, uint32 artKitNeutral = CAPTURE_ARTKIT_NEUTRAL);

        /**
         * @brief set banner visual
         *
         * @param objRef
         * @param goGuid
         * @param artKit
         * @param animId
         */
        void SetBannerVisual(const WorldObject* objRef, ObjectGuid goGuid, uint32 artKit, uint32 animId);
        /**
         * @brief
         *
         * @param go
         * @param artKit
         * @param animId
         */
        void SetBannerVisual(GameObject* go, uint32 artKit, uint32 animId);

        /**
         * @brief Handle gameobject spawn / despawn
         *
         * @param objRef
         * @param goGuid
         * @param respawn
         */
        void RespawnGO(const WorldObject* objRef, ObjectGuid goGuid, bool respawn);

        GuidZoneMap m_zonePlayers; /**< store the players inside the area */
};

#endif
