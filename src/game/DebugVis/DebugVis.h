/*
 * Server-side debug visualization toolkit.
 *
 * Engine-style "debug draw", but rendered server-side by spawning temporary
 * gameobjects the client can see. General purpose: anti-cheat movement traces,
 * map cells/grid, line-of-sight, pathfinding, collision raycasts, terrain
 * height/liquid — anything server-authoritative we can place a marker for.
 *
 * Diagnostic only (markers are non-interactive generic gameobjects; no gameplay
 * effect) and GM-driven via the `.debug` commands. Markers auto-despawn.
 */

#ifndef MANGOS_DEBUGVIS_H
#define MANGOS_DEBUGVIS_H

#include "Common.h"

#include <string>

class Player;

namespace DebugVis
{
    // Visual categories -> colour/model (per-category display id, config-driven).
    enum Category
    {
        DV_GENERIC = 0,
        DV_CELL,        // map grid / cell boundaries
        DV_LOS_OK,      // line of sight: clear segment
        DV_LOS_BLOCK,   // line of sight: blocked segment
        DV_PATH,        // pathfinding: a navmesh path point
        DV_PATH_BAD,    // pathfinding: incomplete / no path
        DV_COLLISION,   // collision raycast ray
        DV_HEIGHT,      // terrain / liquid height sample
        DV_HITPOINT     // impact point (LoS block / collision) — reticle
    };

    // Reserved gameobject_template entry pool (see debugvis_marker_pool.sql).
    // Each *labeled* marker takes a distinct entry from this ring so the client
    // (which caches GO name per entry) can show that marker's own tooltip.
    enum
    {
        DEBUGVIS_ENTRY_BASE  = 305000,
        DEBUGVIS_ENTRY_COUNT = 512
    };

    // Marker lifetime (seconds) from config.
    uint32 DespawnSeconds();

    // Spawn a single marker at (x,y,z), summoned into the viewer's map so the
    // viewer (and nearby players) can see it. If `label` is non-empty it becomes
    // the marker's mouse-over tooltip (verbose per-instance debug text).
    // Returns false if it couldn't spawn.
    bool Marker(Player* viewer, Category cat, float x, float y, float z,
                const std::string& label = std::string());

    // Spawn markers along the 3D segment p1->p2 every `spacing` yards (raw 3D, not
    // ground-snapped — used for LoS/collision rays). Returns markers placed.
    // The ray fill dots share a generic tooltip (`label`, optional).
    uint32 Line(Player* viewer, Category cat,
                float x1, float y1, float z1, float x2, float y2, float z2,
                float spacing, const std::string& label = std::string());

    // Immediately despawn all markers the given viewer has placed (that haven't
    // already auto-despawned). Returns how many were removed.
    uint32 Clear(Player* viewer);

    // True if `entry` belongs to the reserved debug-marker pool.
    bool IsDebugEntry(uint32 entry);

    // Look up the per-instance tooltip stored for a pool entry. Returns false if
    // none recorded. Consumed by the gameobject-query handler.
    bool GetEntryLabel(uint32 entry, std::string& out);
}

#endif // MANGOS_DEBUGVIS_H
