/*
 * Anti-Cheat / Movement-Validation framework — shared definitions.
 *
 * This header carries only enums and small constants so it can be included
 * widely without pulling heavy dependencies.
 */

#ifndef MANGOS_ANTICHEAT_DEFINES_H
#define MANGOS_ANTICHEAT_DEFINES_H

#include "Common.h"

// Types of detected violations. Persisted as the `type` column and used as the
// index into the (config-driven) scoring weights.
enum AntiCheatViolationType
{
    AC_VIOLATION_NONE            = 0,
    AC_VIOLATION_SPEED           = 1,   // horizontal speed beyond allowed + tolerance
    AC_VIOLATION_TELEPORT        = 2,   // single-packet jump beyond latency-adjusted max
    AC_VIOLATION_VERTICAL        = 3,   // unexplained upward Z movement (no jump/levitate/fly)
    AC_VIOLATION_FLAG_CONTRADICT = 4,   // movement flags that the server never granted
    AC_VIOLATION_PHYSICS         = 5,   // position implausible vs terrain/liquid/collision
    AC_VIOLATION_DESYNC          = 6,   // latency drift / time-sync anomaly (informational)
    AC_VIOLATION_JUMP            = 7,   // illegal mid-air / infinite jump (re-jump while airborne)
    AC_VIOLATION_FALL            = 8,   // fall-damage suppression (big drop, no FALL_LAND)
    AC_VIOLATION_BURST           = 9,   // abnormal burst of movement packets (flood/timing)
    AC_VIOLATION_PACKETTIMING    = 10,  // client movement timestamp inconsistency
    AC_VIOLATION_SPELL           = 11,  // cast of a spell the player does not have (injection)
    AC_VIOLATION_ITEM            = 12,  // illegitimate item use (e.g. using an item in the trade window)
    AC_VIOLATION_INTERACT        = 13,  // interaction beyond range (remote loot/use/interact attempt)
    AC_VIOLATION_BOT             = 14,  // bot-like movement (snap-to-waypoint + metronomic timing)

    AC_VIOLATION_MAX
};

// Escalation actions. Also the meaning of the AntiCheat.Action config ceiling:
// the manager never applies an action above the configured maximum.
enum AntiCheatAction
{
    AC_ACTION_NONE       = 0,   // disabled
    AC_ACTION_LOG        = 1,   // record only (log + DB)
    AC_ACTION_GM_ALERT   = 2,   // notify online GMs
    AC_ACTION_RUBBERBAND = 3,   // teleport player back to last validated position
    AC_ACTION_KICK       = 4    // disconnect the session
};

// Result of the physics-plausibility stage.
enum AntiCheatPhysicsResult
{
    AC_PHYS_OK         = 0,   // position is plausible for the current move state
    AC_PHYS_SUSPECT    = 1,   // borderline; raise confidence but light weight
    AC_PHYS_IMPOSSIBLE = 2    // cannot legitimately occupy this position
};

// Normalised movement state derived once from MovementInfo flags, so every
// detector reasons over the same interpretation rather than re-reading flags.
enum AntiCheatMoveState
{
    AC_MOVE_GROUND    = 0,
    AC_MOVE_SWIM      = 1,
    AC_MOVE_FALL      = 2,
    AC_MOVE_FLY       = 3,
    AC_MOVE_TRANSPORT = 4
};

#endif // MANGOS_ANTICHEAT_DEFINES_H
