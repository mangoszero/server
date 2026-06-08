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

#ifndef WORLD_PVP_MGR_H
#define WORLD_PVP_MGR_H

#include "Common.h"
#include "Timer.h"

/**
 * @brief Timer constants for outdoor PvP manager
 */
enum
{
    TIMER_OPVP_MGR_UPDATE = MINUTE * IN_MILLISECONDS ///< Update interval for outdoor PvP manager (1 minute)
};

/**
 * @brief Outdoor PvP type identifiers
 */
enum OutdoorPvPTypes
{
    OPVP_ID_SI = 0, ///< Silithus outdoor PvP
    OPVP_ID_EP,     ///< Eastern Plaguelands outdoor PvP

    MAX_OPVP_ID    ///< Maximum outdoor PvP ID
};

/**
 * @brief Zone IDs for outdoor PvP areas
 */
enum OutdoorPvPZones
{
    ZONE_ID_SILITHUS            = 1377, ///< Silithus zone
    ZONE_ID_TEMPLE_OF_AQ        = 3428, ///< Temple of Ahn'Qiraj zone
    ZONE_ID_RUINS_OF_AQ         = 3429, ///< Ruins of Ahn'Qiraj zone
    ZONE_ID_GATES_OF_AQ         = 3478, ///< Gates of Ahn'Qiraj zone

    ZONE_ID_EASTERN_PLAGUELANDS = 139,  ///< Eastern Plaguelands zone
    ZONE_ID_STRATHOLME          = 2017, ///< Stratholme zone
    ZONE_ID_SCHOLOMANCE         = 2057  ///< Scholomance zone
};

/**
 * @brief Capture point slider state
 */
struct CapturePointSlider
{

    /**
     * @brief Default constructor
     */
    CapturePointSlider() : Value(0.0f), IsLocked(false) {}

    /**
     * @brief Constructor with values
     * @param value Slider value
     * @param isLocked Lock state
     */
    CapturePointSlider(float value, bool isLocked) : Value(value), IsLocked(isLocked) {}

    float Value;  ///< Current slider value
    bool IsLocked; ///< Whether the capture point is locked
};

// forward declaration
class Player;
class GameObject;
class Creature;
class OutdoorPvP;

/**
 * @brief Map of capture point entries to slider states
 */
typedef std::map<uint32 /*capture point entry*/, CapturePointSlider /*slider value and lock state*/> CapturePointSliderMap;

/**
 * @brief Manager class for outdoor PvP zones
 *
 * Handles initialization, updates, and event routing for all
 * outdoor PvP zones in the game world.
 */
class OutdoorPvPMgr
{
    public:
        /**
         * @brief Constructor
         */
        OutdoorPvPMgr();

        /**
         * @brief Destructor
         */
        ~OutdoorPvPMgr();

        /**
         * @brief Initialize all outdoor PvP scripts
         */
        void InitOutdoorPvP();

        /**
         * @brief Handle player entering an outdoor PvP area
         * @param player Player entering the zone
         * @param zoneId Zone ID being entered
         */
        void HandlePlayerEnterZone(Player* player, uint32 zoneId);

        /**
         * @brief Handle player leaving an outdoor PvP area
         * @param player Player leaving the zone
         * @param zoneId Zone ID being left
         */
        void HandlePlayerLeaveZone(Player* player, uint32 zoneId);

        /**
         * @brief Get outdoor PvP script for a zone
         * @param zoneId Zone ID
         * @return OutdoorPvP script for the zone, or nullptr if none
         */
        OutdoorPvP* GetScript(uint32 zoneId);

        /**
         * @brief Update outdoor PvP manager
         * @param diff Time difference since last update in milliseconds
         */
        void Update(uint32 diff);

        /**
         * @brief Get capture point slider map
         * @return Pointer to capture point slider map
         */
        CapturePointSliderMap const* GetCapturePointSliderMap() const { return &m_capturePointSlider; }

        /**
         * @brief Set capture point slider value
         * @param entry Capture point entry
         * @param value Slider value and lock state
         */
        void SetCapturePointSlider(uint32 entry, CapturePointSlider value) { m_capturePointSlider[entry] = value; }

    private:
        /**
         * @brief Get outdoor PvP script for affected zone
         * @param zoneId Zone ID
         * @return OutdoorPvP script for the zone
         */
        OutdoorPvP* GetScriptOfAffectedZone(uint32 zoneId);

        OutdoorPvP* m_scripts[MAX_OPVP_ID]; ///< Array of all outdoor PvP scripts

        CapturePointSliderMap m_capturePointSlider; ///< Map of capture point slider states

        IntervalTimer m_updateTimer; ///< Update interval timer
};

/**
 * @brief Global outdoor PvP manager instance
 */
#define sOutdoorPvPMgr MaNGOS::Singleton<OutdoorPvPMgr>::Instance()

#endif
