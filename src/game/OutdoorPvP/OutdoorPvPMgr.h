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

#ifndef WORLD_PVP_MGR_H
#define WORLD_PVP_MGR_H

#include "Common.h"
#include "Timer.h"

/**
 * @brief
 *
 */
enum
{
    TIMER_OPVP_MGR_UPDATE           = MINUTE * IN_MILLISECONDS // 1 minute is enough for us but this might change with wintergrasp support
};

/**
 * @brief
 *
 */
enum OutdoorPvPTypes
{
    OPVP_ID_SI = 0,
    OPVP_ID_EP,

    MAX_OPVP_ID
};

/**
 * @brief
 *
 */
enum OutdoorPvPZones
{
    ZONE_ID_SILITHUS                = 1377,
    ZONE_ID_TEMPLE_OF_AQ            = 3428,
    ZONE_ID_RUINS_OF_AQ             = 3429,
    ZONE_ID_GATES_OF_AQ             = 3478,

    ZONE_ID_EASTERN_PLAGUELANDS     = 139,
    ZONE_ID_STRATHOLME              = 2017,
    ZONE_ID_SCHOLOMANCE             = 2057
};

struct CapturePointSlider
{
    CapturePointSlider() : Value(0.0f), IsLocked(false) {}
    CapturePointSlider(float value, bool isLocked) : Value(value), IsLocked(isLocked) {}

    float Value;
    bool IsLocked;
};

class Player;
class GameObject;
class Creature;
class OutdoorPvP;

/**
 * @brief
 *
 */
typedef std::map<uint32 /*capture point entry*/, CapturePointSlider /*slider value and lock state*/> CapturePointSliderMap;

class OutdoorPvPMgr
{
    public:
        /**
         * @brief
         *
         */
        OutdoorPvPMgr();
        /**
         * @brief
         *
         */
        ~OutdoorPvPMgr();

        /**
         * @brief load all outdoor pvp scripts
         *
         */
        void InitOutdoorPvP();

        /**
         * @brief called when a player enters an outdoor pvp area
         *
         * @param player
         * @param zoneId
         */
        void HandlePlayerEnterZone(Player* player, uint32 zoneId);

        /**
         * @brief called when player leaves an outdoor pvp area
         *
         * @param player
         * @param zoneId
         */
        void HandlePlayerLeaveZone(Player* player, uint32 zoneId);

        /**
         * @brief return assigned outdoor pvp script
         *
         * @param zoneId
         * @return OutdoorPvP
         */
        OutdoorPvP* GetScript(uint32 zoneId);

        /**
         * @brief
         *
         * @param diff
         */
        void Update(uint32 diff);

        /**
         * Save and load capture point slider
         *
         * @param entry
         * @param value
         */
        CapturePointSliderMap const* GetCapturePointSliderMap() const { return &m_capturePointSlider; }
        void SetCapturePointSlider(uint32 entry, CapturePointSlider value) { m_capturePointSlider[entry] = value; }

    private:
        /**
         * @brief return assigned outdoor pvp script
         *
         * @param zoneId
         * @return OutdoorPvP
         */
        OutdoorPvP* GetScriptOfAffectedZone(uint32 zoneId);

        OutdoorPvP* m_scripts[MAX_OPVP_ID]; /**< contains all outdoor pvp scripts */

        /**
         * @brief
         *
         */
        CapturePointSliderMap m_capturePointSlider;

        ShortIntervalTimer m_updateTimer; /**< update interval */
};

#define sOutdoorPvPMgr MaNGOS::Singleton<OutdoorPvPMgr>::Instance()

#endif
