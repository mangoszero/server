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

#ifndef MANGOS_WAYPOINTMOVEMENTGENERATOR_H
#define MANGOS_WAYPOINTMOVEMENTGENERATOR_H

/** @page PathMovementGenerator is used to generate movements
 * of waypoints and flight paths.  Each serves the purpose
 * of generate activities so that it generates updated
 * packets for the players.
 */

#include "MovementGenerator.h"
#include "WaypointManager.h"
#include "DBCStructure.h"

#include <set>

#define FLIGHT_TRAVEL_UPDATE  100
#define STOP_TIME_FOR_PLAYER  (3 * MINUTE * IN_MILLISECONDS)// 3 Minutes

/**
 * @brief Base class for path movement generators
 *
 * Provides common functionality for path-based movement.
 *
 * @tparam T Type of the unit (Player or Creature)
 * @tparam P Type of the path
 */
template<class T, class P>
class PathMovementBase
{
    public:
        /**
         * @brief Constructor
         */
        PathMovementBase() : i_path(nullptr), i_currentNode(0) {}

        /**
         * @brief Virtual destructor
         */
        virtual ~PathMovementBase() {};

        /**
         * @brief Load path for the unit
         * @param unit Reference to the unit
         */
        void LoadPath(T&);

        /**
         * @brief Get current node in the path
         * @return Current node index
         */
        uint32 GetCurrentNode() const { return i_currentNode; }

    protected:
        P i_path; ///< Path for the movement
        uint32 i_currentNode; ///< Current node in the path
};

/**
 * @brief Waypoint movement generator for creatures
 *
 * Loads a series of waypoints from the database and applies
 * them to the creature's movement generator. The creature will
 * move according to its predefined waypoints.
 */
template<class T>
class WaypointMovementGenerator;

template<>
class WaypointMovementGenerator<Creature>
    : public MovementGeneratorMedium< Creature, WaypointMovementGenerator<Creature> >,
      public PathMovementBase<Creature, WaypointPath const*>
{
    public:
        /**
         * @brief Constructor
         * @param creature Reference to the creature
         */
        WaypointMovementGenerator(Creature&) : i_nextMoveTime(0), m_isArrivalDone(false), m_lastReachedWaypoint(0), m_pathId(0) {}

        /**
         * @brief Destructor
         */
        ~WaypointMovementGenerator() { i_path = NULL; }

        /**
         * @brief Initialize the movement generator
         * @param u Reference to the creature
         */
        void Initialize(Creature& u);

        /**
         * @brief Interrupt the movement generator
         * @param u Reference to the creature
         */
        void Interrupt(Creature&);

        /**
         * @brief Finalize the movement generator
         * @param u Reference to the creature
         */
        void Finalize(Creature&);

        /**
         * @brief Reset the movement generator
         * @param u Reference to the creature
         */
        void Reset(Creature& u);

        /**
         * @brief Update the movement generator
         * @param u Reference to the creature
         * @param diff Time difference in milliseconds
         * @return True if update successful, false otherwise
         */
        bool Update(Creature& u, const uint32& diff);

        /**
         * @brief Initialize waypoint path
         * @param u Reference to the creature
         * @param id Path ID
         * @param wpSource Waypoint path origin
         * @param initialDelay Initial delay before starting
         * @param overwriteEntry Entry to overwrite
         */
        void InitializeWaypointPath(Creature& u, int32 id, WaypointPathOrigin wpSource, uint32 initialDelay, uint32 overwriteEntry);

        /**
         * @brief Get movement generator type
         * @return WAYPOINT_MOTION_TYPE
         */
        MovementGeneratorType GetMovementGeneratorType() const { return WAYPOINT_MOTION_TYPE; }

        /**
         * @brief Get reset position for evade
         * @param creature Reference to the creature
         * @param x X-coordinate output
         * @param y Y-coordinate output
         * @param z Z-coordinate output
         * @param o Orientation output
         * @return True if reset position obtained
         */
        bool GetResetPosition(Creature&, float& /*x*/, float& /*y*/, float& /*z*/, float& /*o*/) const;

        /**
         * @brief Get last reached waypoint
         * @return Last reached waypoint ID
         */
        uint32 getLastReachedWaypoint() const { return m_lastReachedWaypoint; }

        /**
         * @brief Get path information
         * @param pathId Path ID output
         * @param wpOrigin Waypoint path origin output
         */
        void GetPathInformation(int32& pathId, WaypointPathOrigin& wpOrigin) const { pathId = m_pathId; wpOrigin = m_PathOrigin; }

        /**
         * @brief Get path information as string
         * @param oss Output string stream
         */
        void GetPathInformation(std::ostringstream& oss) const;

        /**
         * @brief Add time to waypoint pause
         * @param waitTimeDiff Time difference to add in milliseconds
         */
        void AddToWaypointPauseTime(int32 waitTimeDiff);

        /**
         * @brief Set next waypoint
         * @param pointId Waypoint ID to set as next
         * @return True if successful
         */
        bool SetNextWaypoint(uint32 pointId);

    private:
        /**
         * @brief Load path from database
         * @param c Reference to the creature
         * @param id Path ID
         * @param wpOrigin Waypoint path origin
         * @param overwriteEntry Entry to overwrite
         */
        void LoadPath(Creature& c, int32 id, WaypointPathOrigin wpOrigin, uint32 overwriteEntry);

        /**
         * @brief Stop movement for specified time
         * @param time Time to stop in milliseconds
         */
        void Stop(int32 time) { i_nextMoveTime.Reset(time); }

        /**
         * @brief Check if movement is stopped
         * @param u Reference to the creature
         * @return True if stopped
         */
        bool Stopped(Creature& u);

        /**
         * @brief Check if creature can move
         * @param diff Time difference
         * @param u Reference to the creature
         * @return True if can move
         */
        bool CanMove(int32 diff, Creature& u);

        /**
         * @brief Called when waypoint is reached
         * @param creature Reference to the creature
         */
        void OnArrived(Creature&);

        /**
         * @brief Start movement to next waypoint
         * @param creature Reference to the creature
         */
        void StartMove(Creature&);

        TimeTracker i_nextMoveTime; ///< Time tracker for the next move
        bool m_isArrivalDone; ///< Indicates if the arrival is done
        uint32 m_lastReachedWaypoint; ///< Last reached waypoint

        int32 m_pathId; ///< Path ID
        WaypointPathOrigin m_PathOrigin; ///< Path origin
};

/**
 * @brief Flight path movement generator for players
 *
 * Generates movement of the player along taxi flight paths.
 * Handles ground and activities for the player during flight.
 */
class FlightPathMovementGenerator
    : public MovementGeneratorMedium< Player, FlightPathMovementGenerator >,
      public PathMovementBase<Player, TaxiPathNodeList const*>
{
    public:
        /**
         * @brief Constructor
         * @param pathnodes Reference to path nodes
         * @param startNode Starting node index
         */
        explicit FlightPathMovementGenerator(TaxiPathNodeList const& pathnodes, uint32 startNode = 0)
        {
            i_path = &pathnodes;
            i_currentNode = startNode;
        }

        /**
         * @brief Initialize the movement generator
         * @param player Reference to the player
         */
        void Initialize(Player&);

        /**
         * @brief Finalize the movement generator
         * @param player Reference to the player
         */
        void Finalize(Player&);

        /**
         * @brief Interrupt the movement generator
         * @param player Reference to the player
         */
        void Interrupt(Player&);

        /**
         * @brief Reset the movement generator
         * @param player Reference to the player
         */
        void Reset(Player&);

        /**
         * @brief Update the movement generator
         * @param player Reference to the player
         * @param diff Time difference in milliseconds
         * @return True if update successful
         */
        bool Update(Player&, const uint32&);

        /**
         * @brief Get movement generator type
         * @return FLIGHT_MOTION_TYPE
         */
        MovementGeneratorType GetMovementGeneratorType() const override { return FLIGHT_MOTION_TYPE; }

        /**
         * @brief Get the flight path
         * @return Reference to path nodes
         */
        TaxiPathNodeList const& GetPath() { return *i_path; }

        /**
         * @brief Get node index at map end
         * @return Node index at map end
         */
        uint32 GetPathAtMapEnd() const;

        /**
         * @brief Check if player has arrived at destination
         * @return True if arrived
         */
        bool HasArrived() const { return (i_currentNode >= i_path->size()); }

        /**
         * @brief Set current node after teleport
         */
        void SetCurrentNodeAfterTeleport();

        /**
         * @brief Skip current node
         */
        void SkipCurrentNode() { ++i_currentNode; }

        /**
         * @brief Get reset position for evade
         * @param player Reference to the player
         * @param x X-coordinate output
         * @param y Y-coordinate output
         * @param z Z-coordinate output
         * @param o Orientation output
         * @return True if reset position obtained
         */
        bool GetResetPosition(Player&, float& /*x*/, float& /*y*/, float& /*z*/, float& /*o*/) const;
};

#endif // MANGOS_WAYPOINTMOVEMENTGENERATOR_H
