#include "WaypointSmoothing.h"

/**
 * @brief Whether smoothing may pass through the given node without stopping.
 * @param node Per-node smoothing properties.
 * @return True if the node imposes no stop (no delay, script or behavior).
 */
bool IsWaypointSmoothingSafe(WaypointSmoothingNode const& node)
{
    // Movement splines face along their path; waypoint orientation is only
    // applied when a node stops.
    return !node.hasDelay &&
           !node.hasScript &&
           !node.hasBehavior;
}

/**
 * @brief Whether the spline has reached a tracked waypoint endpoint.
 * @param currentPathIdx Current spline point index (movespline->currentPathIdx()).
 * @param endpointPathIndex Path-point index recorded for the waypoint.
 * @return True once the spline index has reached or passed the endpoint.
 */
bool HasReachedWaypointEndpoint(int32 currentPathIdx, size_t endpointPathIndex)
{
    if (currentPathIdx <= 0)
    {
        return false;
    }

    return static_cast<size_t>(currentPathIdx) >= endpointPathIndex;
}

/**
 * @brief Classifies an in-progress segment, giving a force-stop precedence over a finished spline.
 * @param splineFinalized Whether the spline has completed.
 * @param creatureStopped Whether the unit is stopped (sampled before arrival handling).
 * @return The resulting WaypointSegmentUpdateState.
 */
WaypointSegmentUpdateState GetWaypointSegmentUpdateState(bool splineFinalized, bool creatureStopped)
{
    // A force-stop (talking to the NPC, root, etc.) clears UNIT_STAT_MOVING and also
    // finalizes the spline, so the stopped state must win or the unit would relaunch
    // instead of pausing. The caller samples it before arrival handling clears
    // UNIT_STAT_ROAMING_MOVE, else a normally finishing spline would look stopped too.
    if (creatureStopped)
    {
        return WaypointSegmentUpdateState::Stopped;
    }

    if (splineFinalized)
    {
        return WaypointSegmentUpdateState::Finalized;
    }

    return WaypointSegmentUpdateState::Moving;
}

/**
 * @brief Expands the bounding box to include the given point.
 * @param bounds Bounding box to grow.
 * @param x Point X-coordinate.
 * @param y Point Y-coordinate.
 * @param z Point Z-coordinate.
 */
void AddWaypointSmoothingPoint(WaypointSmoothingBounds& bounds, float x, float y, float z)
{
    if (!bounds.initialized)
    {
        bounds.minX = bounds.maxX = x;
        bounds.minY = bounds.maxY = y;
        bounds.minZ = bounds.maxZ = z;
        bounds.initialized = true;
        return;
    }

    bounds.minX = std::min(bounds.minX, x);
    bounds.maxX = std::max(bounds.maxX, x);
    bounds.minY = std::min(bounds.minY, y);
    bounds.maxY = std::max(bounds.maxY, y);
    bounds.minZ = std::min(bounds.minZ, z);
    bounds.maxZ = std::max(bounds.maxZ, z);
}

/**
 * @brief Whether the bounding box stays within the packable offset budget.
 * @param bounds Bounding box to test.
 * @return True while within budget (an empty box is within budget).
 */
bool IsWaypointSmoothingWithinBudget(WaypointSmoothingBounds const& bounds)
{
    if (!bounds.initialized)
    {
        return true;
    }

    return (bounds.maxX - bounds.minX) <= WAYPOINT_SMOOTHING_MAX_XY_SPAN &&
           (bounds.maxY - bounds.minY) <= WAYPOINT_SMOOTHING_MAX_XY_SPAN &&
           (bounds.maxZ - bounds.minZ) <= WAYPOINT_SMOOTHING_MAX_Z_SPAN;
}
