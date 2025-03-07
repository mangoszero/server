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

#ifndef MANGOS_PATH_FINDER_H
#define MANGOS_PATH_FINDER_H

#include "DetourNavMesh.h"
#include "DetourNavMeshQuery.h"

#include "MoveMapSharedDefines.h"
#include "movement/MoveSplineInitArgs.h"

using Movement::Vector3;
using Movement::PointsArray;

class Unit;

// 74*4.0f=296y  number_of_points*interval = max_path_len
// this is way more than actual evade range
// I think we can safely cut those down even more
#define MAX_PATH_LENGTH         74
#define MAX_POINT_PATH_LENGTH   74

#define SMOOTH_PATH_STEP_SIZE   4.0f
#define SMOOTH_PATH_SLOP        0.3f
#define SMOOTH_PATH_HEIGHT      1.0f

#define VERTEX_SIZE       3
#define INVALID_POLYREF   0

/**
 * @brief Enum representing the type of path.
 */
enum PathType
{
    PATHFIND_BLANK          = 0x0000,   // path not built yet
    PATHFIND_NORMAL         = 0x0001,   // normal path
    PATHFIND_SHORTCUT       = 0x0002,   // travel through obstacles, terrain, air, etc (old behavior)
    PATHFIND_INCOMPLETE     = 0x0004,   // we have partial path to follow - getting closer to target
    PATHFIND_NOPATH         = 0x0008,   // no valid path at all or error in generating one
    PATHFIND_NOT_USING_PATH = 0x0010    // used when we are either flying/swimming or on map w/o mmaps
};

/**
 * @brief Class responsible for finding paths for units.
 */
class PathFinder
{
    public:
        /**
         * @brief Constructor for PathFinder.
         * @param owner Pointer to the unit that owns this PathFinder.
         */
        PathFinder(Unit const* owner);

        /**
         * @brief Destructor for PathFinder.
         */
        ~PathFinder();

        /**
         * @brief Calculate the path from owner to given destination.
         * @param destX X-coordinate of the destination.
         * @param destY Y-coordinate of the destination.
         * @param destZ Z-coordinate of the destination.
         * @param forceDest Whether to force the destination.
         * @return True if a new path was calculated, false otherwise (no change needed).
         */
        bool calculate(float destX, float destY, float destZ, bool forceDest = false);

        // Option setters - use optional
        /**
         * @brief Set whether to use a straight path.
         * @param useStraightPath Whether to use a straight path.
         */
        void setUseStrightPath(bool useStraightPath) { m_useStraightPath = useStraightPath; };

        /**
         * @brief Set the path length limit.
         * @param distance The path length limit.
         */
        void setPathLengthLimit(float distance) { m_pointPathLimit = std::min<uint32>(uint32(distance / SMOOTH_PATH_STEP_SIZE), MAX_POINT_PATH_LENGTH); };

        // Result getters
        /**
         * @brief Get the start position of the path.
         * @return The start position of the path.
         */
        Vector3 getStartPosition() const { return m_startPosition; }

        /**
         * @brief Get the end position of the path.
         * @return The end position of the path.
         */
        Vector3 getEndPosition() const { return m_endPosition; }

        /**
         * @brief Get the actual end position of the path.
         * @return The actual end position of the path.
         */
        Vector3 getActualEndPosition() const { return m_actualEndPosition; }

        /**
         * @brief Normalize the path.
         * @param size The size of the path.
         */
        void NormalizePath(uint32& size);

        /**
         * @brief Get the path points.
         * @return The path points.
         */
        PointsArray& getPath() { return m_pathPoints; }

        /**
         * @brief Get the type of the path.
         * @return The type of the path.
         */
        PathType getPathType() const { return m_type; }

    private:

        dtPolyRef      m_pathPolyRefs[MAX_PATH_LENGTH];   // Array of detour polygon references
        uint32         m_polyLength;                      // Number of polygons in the path

        PointsArray    m_pathPoints;       // Our actual (x,y,z) path to the target
        PathType       m_type;             // Tells what kind of path this is

        bool           m_useStraightPath;  // Type of path that will be generated
        bool           m_forceDestination; // When set, we will always arrive at the given point
        uint32         m_pointPathLimit;   // Limit point path size; min(this, MAX_POINT_PATH_LENGTH)

        Vector3        m_startPosition;    // {x, y, z} of current location
        Vector3        m_endPosition;      // {x, y, z} of the destination
        Vector3        m_actualEndPosition;// {x, y, z} of the closest possible point to the given destination

        const Unit* const       m_sourceUnit;       // The unit that is moving
        const dtNavMesh*        m_navMesh;          // The navigation mesh
        const dtNavMeshQuery*   m_navMeshQuery;     // The navigation mesh query used to find the path

        dtQueryFilter m_filter;                     // Use a single filter for all movements, update it when needed

        /**
         * @brief Set the start position of the path.
         * @param point The start position.
         */
        void setStartPosition(const Vector3 &point) { m_startPosition = point; }

        /**
         * @brief Set the end position of the path.
         * @param point The end position.
         */
        void setEndPosition(const Vector3 &point) { m_actualEndPosition = point; m_endPosition = point; }

        /**
         * @brief Set the actual end position of the path.
         * @param point The actual end position.
         */
        void setActualEndPosition(const Vector3 &point) { m_actualEndPosition = point; }

        /**
         * @brief Clear the path data.
         */
        void clear()
        {
            m_polyLength = 0;
            m_pathPoints.clear();
        }

        /**
         * @brief Check if two points are in range.
         * @param p1 The first point.
         * @param p2 The second point.
         * @param r The range.
         * @param h The height.
         * @return True if the points are in range, false otherwise.
         */
        bool inRange(const Vector3& p1, const Vector3& p2, float r, float h) const;

        /**
         * @brief Calculate the squared 3D distance between two points.
         * @param p1 The first point.
         * @param p2 The second point.
         * @return The squared 3D distance between the points.
         */
        float dist3DSqr(const Vector3& p1, const Vector3& p2) const;

        /**
         * @brief Check if two points are in range in the YZX plane.
         * @param v1 The first point.
         * @param v2 The second point.
         * @param r The range.
         * @param h The height.
         * @return True if the points are in range, false otherwise.
         */
        bool inRangeYZX(const float* v1, const float* v2, float r, float h) const;

        /**
         * @brief Get the path polygon by position.
         * @param polyPath The polygon path.
         * @param polyPathSize The size of the polygon path.
         * @param point The position point.
         * @param distance The distance to the polygon.
         * @return The path polygon reference.
         */
        dtPolyRef getPathPolyByPosition(const dtPolyRef* polyPath, uint32 polyPathSize, const float* point, float* distance = NULL) const;

        /**
         * @brief Get the polygon by location.
         * @param point The location point.
         * @param distance The distance to the polygon.
         * @return The polygon reference.
         */
        dtPolyRef getPolyByLocation(const float* point, float* distance) const;

        /**
         * @brief Check if a tile exists at the given position.
         * @param p The position.
         * @return True if the tile exists, false otherwise.
         */
        bool HaveTile(const Vector3& p) const;

        /**
         * @brief Build the polygon path.
         * @param startPos The start position.
         * @param endPos The end position.
         */
        void BuildPolyPath(const Vector3& startPos, const Vector3& endPos);

        /**
         * @brief Build the point path.
         * @param startPoint The start point.
         * @param endPoint The end point.
         */
        void BuildPointPath(const float* startPoint, const float* endPoint);

        /**
         * @brief Build a shortcut path.
         */
        void BuildShortcut();

        /**
         * @brief Get the navigation terrain at the given position.
         * @param x The X-coordinate.
         * @param y The Y-coordinate.
         * @param z The Z-coordinate.
         * @return The navigation terrain.
         */
        NavTerrain getNavTerrain(float x, float y, float z);

        /**
         * @brief Create the query filter.
         */
        void createFilter();

        /**
         * @brief Update the query filter.
         */
        void updateFilter();

        // Smooth path auxiliary functions
        /**
         * @brief Fix up the corridor path.
         * @param path The path.
         * @param npath The number of path points.
         * @param maxPath The maximum path length.
         * @param visited The visited polygons.
         * @param nvisited The number of visited polygons.
         * @return The fixed up path length.
         */
        uint32 fixupCorridor(dtPolyRef* path, uint32 npath, uint32 maxPath,
                             const dtPolyRef* visited, uint32 nvisited);

        /**
         * @brief Get the steer target for the path.
         * @param startPos The start position.
         * @param endPos The end position.
         * @param minTargetDist The minimum target distance.
         * @param path The path.
         * @param pathSize The size of the path.
         * @param steerPos The steer position.
         * @param steerPosFlag The steer position flag.
         * @param steerPosRef The steer position reference.
         * @return True if the steer target was successfully obtained, false otherwise.
         */
        bool getSteerTarget(const float* startPos, const float* endPos, float minTargetDist,
                            const dtPolyRef* path, uint32 pathSize, float* steerPos,
                            unsigned char& steerPosFlag, dtPolyRef& steerPosRef);

        /**
         * @brief Find the smooth path.
         * @param startPos The start position.
         * @param endPos The end position.
         * @param polyPath The polygon path.
         * @param polyPathSize The size of the polygon path.
         * @param smoothPath The smooth path.
         * @param smoothPathSize The size of the smooth path.
         * @param smoothPathMaxSize The maximum size of the smooth path.
         * @return The status of the path finding.
         */
        dtStatus findSmoothPath(const float* startPos, const float* endPos,
                                const dtPolyRef* polyPath, uint32 polyPathSize,
                                float* smoothPath, int* smoothPathSize, uint32 smoothPathMaxSize);
};

#endif // MANGOS_PATH_FINDER_H
