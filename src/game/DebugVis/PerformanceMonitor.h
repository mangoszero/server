/*
 * Lightweight server performance monitor.
 *
 * Tracks World::Update tick timing (the diff passed each world tick) so server
 * health is observable via `.debug perf`. Single-threaded: World::Update runs on
 * the world thread only, so no locking is needed.
 */

#ifndef MANGOS_PERFORMANCEMONITOR_H
#define MANGOS_PERFORMANCEMONITOR_H

#include "Common.h"

namespace PerformanceMonitor
{
    // Record one world tick (its elapsed diff in ms).
    void TrackUpdate(uint32 diffMs);

    // Read cumulative stats since the last Reset().
    void GetStats(uint32& ticks, uint32& avgMs, uint32& maxMs, uint32& lastMs);

    // Clear accumulated stats.
    void Reset();
}

#endif // MANGOS_PERFORMANCEMONITOR_H
