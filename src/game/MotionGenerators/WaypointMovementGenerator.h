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

#include "DBCStructure.h"
#include "IntentMovementGenerator.h"
#include "WaypointManager.h"
#include "movement/MoveSplineInitArgs.h"

#include <sstream>
#include <vector>

/// How long a patroller waits after being force-stopped by a player talking to it.
#define STOP_TIME_FOR_PLAYER  (3 * MINUTE * IN_MILLISECONDS)

/**
 * @brief Patrol: walk a list of waypoints, pausing, emoting and running scripts at the
 *        ones that say to.
 *
 * This is the one movement kind that must dictate the EXACT geometry of its leg rather
 * than name a destination and let the driver route to it: a smoothed segment welds
 * several waypoint legs into a single spline so the creature does not visibly stop and
 * relaunch at every node. It therefore hands the driver its own points on the intent.
 */
class WaypointMovementGenerator final : public IntentMovementGenerator
{
    public:
        explicit WaypointMovementGenerator(Creature&) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return WAYPOINT_MOTION_TYPE; }

        bool GetResetPosition(Unit& owner, float& x, float& y, float& z, float& o) const override;

        /// Load a path and start walking it after `initialDelay` ms.
        void InitializeWaypointPath(Unit& owner, int32 pathId, WaypointPathOrigin wpSource,
                                    uint32 initialDelay, uint32 overwriteEntry);

        uint32 getLastReachedWaypoint() const { return m_lastReachedWaypoint; }

        void GetPathInformation(int32& pathId, WaypointPathOrigin& wpOrigin) const
        {
            pathId = m_pathId;
            wpOrigin = m_pathOrigin;
        }

        void GetPathInformation(std::ostringstream& oss) const;

        /// Extend (or cut short) the pause at the current node.
        void AddToWaypointPauseTime(int32 waitTimeDiff);

        /// Jump the patrol to a given node; it moves on the next tick.
        bool SetNextWaypoint(uint32 pointId);

    protected:
        Motion::MoveIntent Intent(Unit& owner, Motion::MoveStatus const& status,
                                  uint32 diff) override;

    private:
        /// A waypoint reached inside an active smoothed segment.
        struct SegmentWaypoint
        {
            uint32 pointId;        ///< Waypoint id in the path.
            size_t pathPointIndex; ///< Index of its endpoint within the spline points.
        };

        void LoadPath(Creature& creature, int32 pathId, WaypointPathOrigin wpOrigin,
                      uint32 overwriteEntry);

        /// Advance to the next waypoint and prepare the leg that reaches it. This is the
        /// old StartMove with the launching taken out: it still advances the node, fires
        /// the AI informs and applies the node's model change, and it still builds the
        /// smoothed geometry — but it hands that to the driver as an intent rather than
        /// pushing a spline itself.
        Motion::MoveIntent PrepareMove(Creature& creature);

        /// The intent that walks the leg PrepareMove built.
        Motion::MoveIntent WalkPreparedLeg() const;

        /// Everything that happens on getting to a node: scripts, emotes, AI informs,
        /// and the pause the node asks for.
        void OnArrived(Creature& creature);

        /// Fire arrival handling for any smoothed waypoints the spline has now passed.
        void ProcessSegmentProgress(Creature& creature, int32 pathIndex);

        /// Weld as many upcoming legs as will fit into one spline. Leaves m_legPoints
        /// empty when the segment cannot be smoothed, and the driver then routes a plain
        /// leg to the next node instead.
        void BuildSmoothPath(Creature& creature, WaypointPath::const_iterator startPoint);

        bool Stopped(Unit const& owner) const;
        bool CanMove(Unit const& owner, uint32 diff);
        void Stop(int32 time) { m_nextMoveTime.Reset(time); }

        void ClearSegment()
        {
            m_segment.clear();
            m_segmentArrivals = 0;
        }

        WaypointPath const* m_path = nullptr;
        uint32 m_currentNode = 0;
        uint32 m_lastReachedWaypoint = 0;
        int32 m_pathId = 0;
        WaypointPathOrigin m_pathOrigin = PATH_NO_PATH;

        TimeTracker m_nextMoveTime{0};
        bool m_isArrivalDone = false;

        std::vector<SegmentWaypoint> m_segment; ///< Waypoints inside the smoothed leg.
        size_t m_segmentArrivals = 0;           ///< How many of them we have passed.

        /// The leg PrepareMove built. Empty points mean "not smoothed — route to
        /// m_legEnd instead". The driver holds a pointer to these while the leg is in
        /// flight, so they must not be rebuilt until the leg ends.
        Movement::PointsArray m_legPoints;
        Motion::Vector3 m_legEnd;
        Motion::Facing m_legFacing;
        bool m_legWalk = true; ///< Pace of the leg; false only for a DB-flagged runner.
        bool m_haveLeg = false;
};

/**
 * @brief The player taxi flight.
 *
 * Deliberately NOT on the intent model: it lays one scripted spline through the taxi
 * nodes and watches the path index. There is nothing to route, nothing to re-path and
 * nothing to face, so an intent would buy it nothing.
 */
class FlightPathMovementGenerator final : public MovementGenerator
{
    public:
        explicit FlightPathMovementGenerator(TaxiPathNodeList const& pathnodes,
                                             uint32 startNode = 0)
            : m_path(&pathnodes), m_currentNode(startNode) {}

        void Initialize(Unit& owner) override;
        void Finalize(Unit& owner) override;
        void Interrupt(Unit& owner) override;
        void Reset(Unit& owner) override;
        bool Update(Unit& owner, uint32 diff) override;

        MovementGeneratorType GetMovementGeneratorType() const override { return FLIGHT_MOTION_TYPE; }

        bool GetResetPosition(Unit& owner, float& x, float& y, float& z, float& o) const override;

        TaxiPathNodeList const& GetPath() const { return *m_path; }
        uint32 GetCurrentNode() const { return m_currentNode; }

        /// Index of the first node on a different map — where this leg of the flight ends.
        uint32 GetPathAtMapEnd() const;

        bool HasArrived() const { return m_currentNode >= m_path->size(); }

        void SetCurrentNodeAfterTeleport();
        void SkipCurrentNode() { ++m_currentNode; }

    private:
        TaxiPathNodeList const* m_path;
        uint32 m_currentNode;
};

#endif // MANGOS_WAYPOINTMOVEMENTGENERATOR_H
