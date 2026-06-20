/*
 * Lightweight server performance monitor — implementation.
 */

#include "PerformanceMonitor.h"

namespace
{
    uint32 s_ticks = 0;
    uint64 s_sumMs = 0;
    uint32 s_maxMs = 0;
    uint32 s_lastMs = 0;
}

namespace PerformanceMonitor
{
    void TrackUpdate(uint32 diffMs)
    {
        ++s_ticks;
        s_sumMs += diffMs;
        s_lastMs = diffMs;
        if (diffMs > s_maxMs)
            s_maxMs = diffMs;
    }

    void GetStats(uint32& ticks, uint32& avgMs, uint32& maxMs, uint32& lastMs)
    {
        ticks  = s_ticks;
        avgMs  = s_ticks ? uint32(s_sumMs / s_ticks) : 0;
        maxMs  = s_maxMs;
        lastMs = s_lastMs;
    }

    void Reset()
    {
        s_ticks = 0; s_sumMs = 0; s_maxMs = 0; s_lastMs = 0;
    }
}
