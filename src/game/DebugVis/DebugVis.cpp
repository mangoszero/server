/*
 * Server-side debug visualization toolkit — implementation.
 */

#include "DebugVis.h"
#include "Player.h"
#include "GameObject.h"
#include "Map.h"
#include "ObjectGuid.h"
#include "World.h"
#include "Config/Config.h"

#include <cmath>
#include <map>
#include <vector>

namespace
{
    // Per-category marker MODEL/COLOUR (a GameObjectDisplayInfo.dbc display id),
    // applied per-instance via GAMEOBJECT_DISPLAYID.
    //
    // IMPORTANT: the client only lets you mouse-over/hover a gameobject whose
    // *model* is a solid clickable mesh. Particle/effect doodads (light columns,
    // auras, ground reticles) render but are NOT hoverable, so their per-instance
    // tooltip never shows. Both presets below therefore use solid crystal models.
    //   DebugVis.Style 0 = colour-coded Power Crystals [default], 1 = assorted
    //   floating crystals. Any single category is overridable via DebugVis.Disp.<Cat>.
    uint32 ColorDisplayId(DebugVis::Category cat)
    {
        const bool alt = sConfig.GetIntDefault("DebugVis.Style", 0) != 0;

        // {primary (power crystals, clean palette), alternate (assorted crystals)}.
        uint32 prim, alt2; const char* key;
        switch (cat)
        {
            case DebugVis::DV_CELL:      key = "DebugVis.Disp.Cell";      prim = 5912; alt2 = 5811; break; // WSG (silverwing) flag - small, clean grid marker
            case DebugVis::DV_LOS_OK:    key = "DebugVis.Disp.LosOk";     prim = 2972; alt2 = 6431; break; // green  power crystal / glyphed crystal
            case DebugVis::DV_LOS_BLOCK: key = "DebugVis.Disp.LosBlock";  prim = 2973; alt2 = 6573; break; // red    power crystal / red crystal
            case DebugVis::DV_PATH:      key = "DebugVis.Disp.Path";      prim = 2972; alt2 = 6431; break; // green  power crystal / glyphed crystal
            case DebugVis::DV_PATH_BAD:  key = "DebugVis.Disp.PathBad";   prim = 2973; alt2 = 6573; break; // red    power crystal / red crystal
            case DebugVis::DV_COLLISION: key = "DebugVis.Disp.Collision"; prim = 1667; alt2 = 1667; break; // purple floating crystal
            case DebugVis::DV_HEIGHT:    key = "DebugVis.Disp.Height";    prim = 2974; alt2 = 6570; break; // yellow power crystal / silithus crystal
            case DebugVis::DV_HITPOINT:  key = "DebugVis.Disp.HitPoint";  prim = 5746; alt2 = 6571; break; // crimson shard / broken red crystal
            case DebugVis::DV_GENERIC:
            default:                     key = "DebugVis.Disp.Generic";   prim = 5811; alt2 = 5746; break; // dark crystal / crimson shard
        }
        return uint32(sConfig.GetIntDefault(key, int32(alt ? alt2 : prim)));
    }

    // Optional GLOW companion model spawned alongside each clickable crystal: a
    // colour-matched particle effect (light column / glow circle) for the
    // "engine debug-draw" look. Effect models aren't clickable, which is fine —
    // the co-located crystal is the hover target. 0 disables a category's glow.
    uint32 GlowDisplayId(DebugVis::Category cat)
    {
        uint32 def; const char* key;
        switch (cat)
        {
            case DebugVis::DV_CELL:      key = "DebugVis.GlowDisp.Cell";      def = 0;    break; // none: the WSG flag is the cell marker (no tall column)
            case DebugVis::DV_LOS_OK:    key = "DebugVis.GlowDisp.LosOk";     def = 3993; break; // green column
            case DebugVis::DV_LOS_BLOCK: key = "DebugVis.GlowDisp.LosBlock";  def = 327;  break; // red
            case DebugVis::DV_PATH:      key = "DebugVis.GlowDisp.Path";      def = 3993; break; // green column
            case DebugVis::DV_PATH_BAD:  key = "DebugVis.GlowDisp.PathBad";   def = 327;  break; // red
            case DebugVis::DV_COLLISION: key = "DebugVis.GlowDisp.Collision"; def = 363;  break; // purple column
            case DebugVis::DV_HEIGHT:    key = "DebugVis.GlowDisp.Height";    def = 266;  break; // yellow column
            case DebugVis::DV_HITPOINT:  key = "DebugVis.GlowDisp.HitPoint";  def = 6430; break; // cannon target reticle
            case DebugVis::DV_GENERIC:
            default:                     key = "DebugVis.GlowDisp.Generic";   def = 6679; break; // glow circle
        }
        return uint32(sConfig.GetIntDefault(key, int32(def)));
    }

    // Per-category marker scale. Some models (the collision/hit crystals) render
    // much larger than the rest; shrink those so the line/impact markers aren't
    // oversized. Overridable via config.
    float MarkerScale(DebugVis::Category cat)
    {
        switch (cat)
        {
            case DebugVis::DV_COLLISION: return sConfig.GetFloatDefault("DebugVis.Scale.Collision", 0.5f);
            case DebugVis::DV_HITPOINT:  return sConfig.GetFloatDefault("DebugVis.Scale.HitPoint", 0.6f);
            default:                     return sConfig.GetFloatDefault("DebugVis.Scale", 1.0f);
        }
    }

    // entry -> per-instance tooltip text for spawned pool markers. Touched only
    // from the world/session thread (command handlers + GO-query handler), so no
    // locking needed. Bounded to the pool size (ring reuse overwrites).
    std::map<uint32, std::string>& LabelMap()
    {
        static std::map<uint32, std::string> s_labels;
        return s_labels;
    }

    // Next ring slot. Each labeled marker consumes one pool entry.
    uint32 NextLabeledEntry(const std::string& label)
    {
        static uint32 s_cursor = 0;
        uint32 entry = uint32(DebugVis::DEBUGVIS_ENTRY_BASE) + (s_cursor % DebugVis::DEBUGVIS_ENTRY_COUNT);
        s_cursor = (s_cursor + 1) % DebugVis::DEBUGVIS_ENTRY_COUNT;
        LabelMap()[entry] = label;
        return entry;
    }

    // Shared, never-relabeled pool entry for unlabeled fill markers (ray dots).
    // Uses the very top of the pool so the ring above doesn't clobber it often.
    uint32 SharedFillEntry()
    {
        return uint32(DebugVis::DEBUGVIS_ENTRY_BASE) + uint32(DebugVis::DEBUGVIS_ENTRY_COUNT) - 1;
    }

    // Per-placer record of spawned marker GO guids, so `.debug vis clear` can
    // despawn them on demand (before their auto-despawn fires). Same single
    // world thread as Marker()/Clear(), so no locking.
    std::map<ObjectGuid, std::vector<ObjectGuid> >& MarkerOwners()
    {
        static std::map<ObjectGuid, std::vector<ObjectGuid> > s_owners;
        return s_owners;
    }
}

namespace DebugVis
{
    uint32 DespawnSeconds()
    {
        return sWorld.getConfig(CONFIG_UINT32_DEBUGVIS_DESPAWN);
    }

    bool IsDebugEntry(uint32 entry)
    {
        return entry >= uint32(DEBUGVIS_ENTRY_BASE) &&
               entry <  uint32(DEBUGVIS_ENTRY_BASE) + uint32(DEBUGVIS_ENTRY_COUNT);
    }

    bool GetEntryLabel(uint32 entry, std::string& out)
    {
        std::map<uint32, std::string>& m = LabelMap();
        std::map<uint32, std::string>::const_iterator it = m.find(entry);
        if (it == m.end() || it->second.empty())
            return false;
        out = it->second;
        return true;
    }

    bool Marker(Player* viewer, Category cat, float x, float y, float z, const std::string& label)
    {
        if (!viewer || !viewer->IsInWorld())
            return false;

        uint32 despawnMs = DespawnSeconds() * IN_MILLISECONDS;
        float ori = viewer->GetOrientation();
        float scale = MarkerScale(cat);
        ObjectGuid owner = viewer->GetObjectGuid();
        bool placed = false;

        // Optional colour-matched glow effect co-located with the crystal, for the
        // "debug-draw" look. Effect model => not clickable (that's the crystal's
        // job), so it shares the fill entry and carries no tooltip.
        if (sConfig.GetIntDefault("DebugVis.Glow", 1))
        {
            uint32 glow = GlowDisplayId(cat);
            if (glow)
                if (GameObject* g = viewer->SummonGameObject(SharedFillEntry(), x, y, z, ori, despawnMs, glow, scale))
                {
                    g->SetSpawnedByDefault(false);   // honour the despawn timer
                    MarkerOwners()[owner].push_back(g->GetObjectGuid());
                    placed = true;
                }
        }

        // Clickable crystal — the hover target. Labeled markers each take a distinct
        // pool entry (per-instance tooltip); unlabeled ones share a single entry.
        uint32 entry = label.empty() ? SharedFillEntry() : NextLabeledEntry(label);
        if (GameObject* go = viewer->SummonGameObject(entry, x, y, z, ori, despawnMs, ColorDisplayId(cat), scale))
        {
            go->SetSpawnedByDefault(false);   // honour the despawn timer
            MarkerOwners()[owner].push_back(go->GetObjectGuid());
            placed = true;
        }

        return placed;
    }

    uint32 Clear(Player* viewer)
    {
        if (!viewer)
            return 0;

        std::map<ObjectGuid, std::vector<ObjectGuid> >& owners = MarkerOwners();
        std::map<ObjectGuid, std::vector<ObjectGuid> >::iterator it = owners.find(viewer->GetObjectGuid());
        if (it == owners.end())
            return 0;

        Map* map = viewer->GetMap();
        uint32 removed = 0;
        for (std::vector<ObjectGuid>::const_iterator g = it->second.begin(); g != it->second.end(); ++g)
        {
            // Skip any that already auto-despawned (GetGameObject returns NULL).
            if (map)
                if (GameObject* go = map->GetGameObject(*g))
                {
                    go->Delete();
                    ++removed;
                }
        }
        owners.erase(it);
        return removed;
    }

    uint32 Line(Player* viewer, Category cat,
                float x1, float y1, float z1, float x2, float y2, float z2,
                float spacing, const std::string& label)
    {
        if (!viewer || !viewer->IsInWorld())
            return 0;
        if (spacing < 0.5f)
            spacing = 0.5f;

        float dx = x2 - x1, dy = y2 - y1, dz = z2 - z1;
        float len = sqrtf(dx * dx + dy * dy + dz * dz);
        uint32 steps = uint32(len / spacing);
        if (steps > 200) // safety cap so a long ray can't flood the world
            steps = 200;

        // Don't drop a marker on (or right next to) the caster — markers are solid
        // and would trap the player inside the object. Skip the first few yards.
        const float MIN_FROM_START = 3.0f;

        uint32 placed = 0;
        bool labelUsed = false;
        for (uint32 i = 0; i <= steps; ++i)
        {
            float t = steps ? float(i) / float(steps) : 0.0f;
            if (len * t < MIN_FROM_START)
                continue;                       // skip dots on/next to the caster
            // The first PLACED dot (out along the line) carries the verbose tooltip;
            // the rest are unlabeled fill so we don't burn the whole ring on one ray.
            std::string dotLabel = labelUsed ? std::string() : label;
            if (Marker(viewer, cat, x1 + dx * t, y1 + dy * t, z1 + dz * t, dotLabel))
            {
                ++placed;
                labelUsed = true;
            }
        }
        return placed;
    }
}
