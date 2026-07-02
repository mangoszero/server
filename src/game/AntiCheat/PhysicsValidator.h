/*
 * Anti-Cheat / Movement-Validation framework — physics plausibility stage.
 *
 * Pure, side-effect-free validation: given a player, a normalised move state and
 * the incoming MovementInfo, decide whether the position is physically plausible.
 * Never mutates state. Cheap checks first; callers only invoke this on suspicion
 * so the moderate-cost terrain queries stay off the hot path for honest players.
 */

#ifndef MANGOS_ANTICHEAT_PHYSICSVALIDATOR_H
#define MANGOS_ANTICHEAT_PHYSICSVALIDATOR_H

#include "Common.h"
#include "AntiCheatDefines.h"

class Player;
class MovementInfo;

namespace PhysicsValidator
{
    // Validate the destination position in the incoming packet against terrain
    // for the given normalised move state. `reason` (optional out) receives a
    // short static description when the result is not OK.
    AntiCheatPhysicsResult Validate(Player* player, AntiCheatMoveState state,
                                    MovementInfo const& mi, const char** reason = NULL);
}

#endif // MANGOS_ANTICHEAT_PHYSICSVALIDATOR_H
