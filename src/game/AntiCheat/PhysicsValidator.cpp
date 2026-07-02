/*
 * Anti-Cheat / Movement-Validation framework — physics plausibility stage.
 */

#include "PhysicsValidator.h"
#include "Player.h"
#include "Map.h"
#include "Unit.h"

namespace
{
    // Tolerances are deliberately generous to keep false positives near zero;
    // tuning happens from real logs before enforcement is enabled.
    const float GROUND_FLOAT_SUSPECT    = 5.0f;   // yd above ground -> suspect
    const float GROUND_FLOAT_IMPOSSIBLE = 18.0f;  // yd above ground -> impossible
    const float UNDERGROUND_IMPOSSIBLE  = 6.0f;   // yd below ground -> impossible
    const float NO_TERRAIN_DATA         = -50000.0f;
}

namespace PhysicsValidator
{
    AntiCheatPhysicsResult Validate(Player* player, AntiCheatMoveState state,
                                    MovementInfo const& mi, const char** reason)
    {
        if (!player || !player->IsInWorld())
            return AC_PHYS_OK;

        // Only the grounded case has a cheap, reliable terrain expectation.
        // Falling/flying/swimming/transport legitimately deviate from the
        // heightmap, so we do not judge them here.
        if (state != AC_MOVE_GROUND)
            return AC_PHYS_OK;

        Position const* pos = mi.GetPos();
        Map* map = player->GetMap();
        if (!map)
            return AC_PHYS_OK;

        // Terrain + VMAP height (accounts for buildings/bridges via the existing
        // nearest-surface logic). If no data, we cannot judge -> OK.
        float groundZ = map->GetHeight(pos->x, pos->y, pos->z);
        if (groundZ < NO_TERRAIN_DATA)
            return AC_PHYS_OK;

        float dz = pos->z - groundZ;

        if (dz > GROUND_FLOAT_IMPOSSIBLE)
        {
            if (reason) { *reason = "floating far above terrain"; }
            return AC_PHYS_IMPOSSIBLE;
        }
        if (dz < -UNDERGROUND_IMPOSSIBLE)
        {
            if (reason) { *reason = "below terrain surface"; }
            return AC_PHYS_IMPOSSIBLE;
        }
        if (dz > GROUND_FLOAT_SUSPECT)
        {
            if (reason) { *reason = "hovering above terrain"; }
            return AC_PHYS_SUSPECT;
        }

        return AC_PHYS_OK;
    }
}
