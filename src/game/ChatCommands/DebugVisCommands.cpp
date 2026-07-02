/*
 * GM chat commands for the server-side debug visualization toolkit:
 *   .debug vis cells|los|path|collision|height
 * Each draws a one-shot, auto-despawning set of markers (engine-style debug draw).
 */

#include "Chat.h"
#include "DebugVis.h"
#include "PerformanceMonitor.h"
#include "Player.h"
#include "Map.h"
#include "World.h"
#include "GridDefines.h"
#include "PathFinder.h"

#include <cmath>
#include <cstdlib>
#include <cstdio>

bool ChatHandler::HandleDebugVisCellsCommand(char* args)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;

    int radius = 2;
    if (args && *args)
        radius = atoi(args);
    if (radius < 1) radius = 1;
    if (radius > 5) radius = 5; // (2r+1)^2 markers cap

    Map* map = player->GetMap();
    const float cell = SIZE_OF_GRID_CELL;
    // Snap to the cell lattice so markers line up with cell spacing.
    float baseX = floor(player->GetPositionX() / cell) * cell;
    float baseY = floor(player->GetPositionY() / cell) * cell;

    uint32 placed = 0;
    for (int i = -radius; i <= radius; ++i)
    {
        for (int j = -radius; j <= radius; ++j)
        {
            float wx = baseX + i * cell;
            float wy = baseY + j * cell;
            // Search ground from well above the player's elevation so distant cells
            // snap to real terrain; skip cells with no ground beneath (was placing
            // markers at the player's Z, which left them floating in mid-air).
            float wz = map->GetHeight(wx, wy, player->GetPositionZ() + 50.0f);
            if (wz < -50000.0f)
                continue;
            char lbl[160];
            snprintf(lbl, sizeof(lbl), "DebugVis CELL [%+d,%+d]\n(%.1f, %.1f, %.1f)  ground Z=%.1f",
                     i, j, wx, wy, wz, wz);
            if (DebugVis::Marker(player, DebugVis::DV_CELL, wx, wy, wz, lbl))
                ++placed;
        }
    }
    PSendSysMessage("DebugVis: drew %u cell-grid markers (cell=%.1f yd, radius=%d). Despawn in %us.",
                    placed, cell, radius, DebugVis::DespawnSeconds());
    return true;
}

bool ChatHandler::HandleDebugVisLosCommand(char* /*args*/)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;

    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(".debug vis los FAILED: no target selected. It draws the line of sight "
                       "from you to a unit (green = clear, red = blocked, with the hit point marked). "
                       "Select/target a creature or player, then run .debug vis los again.");
        SetSentErrorMessage(true);
        return false;
    }

    Map* map = player->GetMap();
    float x1 = player->GetPositionX(), y1 = player->GetPositionY(), z1 = player->GetPositionZ() + 2.0f;
    float x2 = target->GetPositionX(), y2 = target->GetPositionY(), z2 = target->GetPositionZ() + 2.0f;

    float total = sqrtf((x2 - x1) * (x2 - x1) + (y2 - y1) * (y2 - y1) + (z2 - z1) * (z2 - z1));
    if (map->IsInLineOfSight(x1, y1, z1, x2, y2, z2))
    {
        char lbl[160];
        snprintf(lbl, sizeof(lbl), "DebugVis LoS: CLEAR\nto %s, %.1f yd", target->GetName(), total);
        DebugVis::Line(player, DebugVis::DV_LOS_OK, x1, y1, z1, x2, y2, z2, 2.0f, lbl);
        SendSysMessage("DebugVis: line of sight is CLEAR (green).");
    }
    else
    {
        float hx = x2, hy = y2, hz = z2;
        map->GetHitPosition(x1, y1, z1, hx, hy, hz, -0.5f);
        float hitDist = sqrtf((hx - x1) * (hx - x1) + (hy - y1) * (hy - y1) + (hz - z1) * (hz - z1));
        DebugVis::Line(player, DebugVis::DV_LOS_BLOCK, x1, y1, z1, hx, hy, hz, 2.0f, "DebugVis LoS: BLOCKED");
        char lbl[192];
        snprintf(lbl, sizeof(lbl),
                 "DebugVis LoS BLOCKED\nhit (%.1f, %.1f, %.1f)\n%.1f yd from you (target %.1f yd)",
                 hx, hy, hz, hitDist, total);
        DebugVis::Marker(player, DebugVis::DV_HITPOINT, hx, hy, hz, lbl);
        PSendSysMessage("DebugVis: line of sight BLOCKED (red); hit at (%.1f, %.1f, %.1f).", hx, hy, hz);
    }
    return true;
}

bool ChatHandler::HandleDebugVisPathCommand(char* /*args*/)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;

    Unit* target = getSelectedUnit();
    if (!target)
    {
        SendSysMessage(".debug vis path FAILED: no target selected. It draws the navmesh path "
                       "from you to a unit (green = valid path, red = incomplete/none). "
                       "Select/target a creature or player, then run .debug vis path again.");
        SetSentErrorMessage(true);
        return false;
    }

    PathFinder pf(player);
    pf.calculate(target->GetPositionX(), target->GetPositionY(), target->GetPositionZ());
    PathType type = pf.getPathType();
    PointsArray& pts = pf.getPath();

    DebugVis::Category cat = (type & (PATHFIND_NOPATH | PATHFIND_INCOMPLETE | PATHFIND_NOT_USING_PATH))
                             ? DebugVis::DV_PATH_BAD : DebugVis::DV_PATH;

    uint32 total = uint32(pts.size());
    uint32 placed = 0;
    uint32 idx = 0;
    for (PointsArray::const_iterator it = pts.begin(); it != pts.end(); ++it, ++idx)
    {
        // Skip the path point on/next to the player — markers are solid and would
        // trap the caster inside the object.
        float pdx = it->x - player->GetPositionX();
        float pdy = it->y - player->GetPositionY();
        if (pdx * pdx + pdy * pdy < 9.0f)   // within ~3 yd
            continue;
        char lbl[176];
        snprintf(lbl, sizeof(lbl),
                 "DebugVis PATH pt %u/%u\n(%.1f, %.1f, %.1f)\ntype=0x%X (%s)",
                 idx + 1, total, it->x, it->y, it->z, uint32(type),
                 cat == DebugVis::DV_PATH ? "ok" : "incomplete/none");
        if (DebugVis::Marker(player, cat, it->x, it->y, it->z, lbl))
            ++placed;
    }

    PSendSysMessage("DebugVis: path type=0x%X, %u points (%s).",
                    uint32(type), placed, cat == DebugVis::DV_PATH ? "green=ok" : "red=incomplete/none");
    return true;
}

bool ChatHandler::HandleDebugVisCollisionCommand(char* args)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;

    float dist = 40.0f;
    if (args && *args)
    {
        float v = (float)atof(args);
        if (v > 1.0f && v < 300.0f)
            dist = v;
    }

    Map* map = player->GetMap();
    float o = player->GetOrientation();
    float x1 = player->GetPositionX(), y1 = player->GetPositionY(), z1 = player->GetPositionZ() + 2.0f;
    float x2 = x1 + cos(o) * dist, y2 = y1 + sin(o) * dist, z2 = z1;

    float hx = x2, hy = y2, hz = z2;
    if (map->GetHitPosition(x1, y1, z1, hx, hy, hz, -0.5f))
    {
        float hitDist = sqrtf((hx - x1) * (hx - x1) + (hy - y1) * (hy - y1));
        DebugVis::Line(player, DebugVis::DV_COLLISION, x1, y1, z1, hx, hy, hz, 2.0f, "DebugVis: collision ray");
        char lbl[176];
        snprintf(lbl, sizeof(lbl), "DebugVis COLLISION\nhit (%.1f, %.1f, %.1f)\n%.1f yd ahead",
                 hx, hy, hz, hitDist);
        DebugVis::Marker(player, DebugVis::DV_HITPOINT, hx, hy, hz, lbl);
        PSendSysMessage("DebugVis: collision at (%.1f, %.1f, %.1f), %.1f yd ahead.", hx, hy, hz, hitDist);
    }
    else
    {
        char lbl[96];
        snprintf(lbl, sizeof(lbl), "DebugVis: no collision\nwithin %.0f yd ahead", dist);
        DebugVis::Line(player, DebugVis::DV_COLLISION, x1, y1, z1, x2, y2, z2, 2.0f, lbl);
        PSendSysMessage("DebugVis: no collision within %.0f yd ahead.", dist);
    }
    return true;
}

bool ChatHandler::HandleDebugVisualCommand(char* args)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;
    if (!args || !*args)
    {
        // NOTE: SMSG_PLAY_SPELL_VISUAL takes a SpellVisualKit.dbc id (not SpellVisual).
        // 179 is confirmed working (trainer learn-spell sparkle). Valid kit ids in the
        // 1.12.1 client run 1..6757; good ones to try below.
        SendSysMessage("Usage: .debug visual <SpellVisualKit id> (1-6757). Confirmed working: 179. "
                       "Try also 224, 300, 451, 686, 1027, 5670.");
        SetSentErrorMessage(true);
        return false;
    }
    uint32 id = uint32(atoi(args));
    if (!id)
    {
        SendSysMessage("DebugVis: invalid visual id.");
        SetSentErrorMessage(true);
        return false;
    }
    player->PlaySpellVisual(id);
    PSendSysMessage("DebugVis: played spell visual kit %u on you (visible to nearby players).", id);
    return true;
}

bool ChatHandler::HandleDebugPerfCommand(char* args)
{
    if (args && *args && (*args == 'r' || *args == 'R'))
    {
        PerformanceMonitor::Reset();
        SendSysMessage("PerformanceMonitor: stats reset.");
        return true;
    }

    uint32 ticks, avgMs, maxMs, lastMs;
    PerformanceMonitor::GetStats(ticks, avgMs, maxMs, lastMs);
    PSendSysMessage("Server perf: world ticks=%u  avg=%ums  max=%ums  last=%ums  (.debug perf r to reset)",
                    ticks, avgMs, maxMs, lastMs);
    return true;
}

bool ChatHandler::HandleDebugVisHeightCommand(char* /*args*/)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;

    Map* map = player->GetMap();
    float px = player->GetPositionX(), py = player->GetPositionY(), pz = player->GetPositionZ();
    float groundZ = map->GetHeight(px, py, pz);

    if (groundZ < -50000.0f)
    {
        SendSysMessage("DebugVis: no terrain height data at this position.");
        return true;
    }

    char lbl[160];
    snprintf(lbl, sizeof(lbl), "DebugVis HEIGHT\nground Z=%.2f\nyou Z=%.2f (delta %.2f)",
             groundZ, pz, pz - groundZ);
    DebugVis::Marker(player, DebugVis::DV_HEIGHT, px, py, groundZ, lbl);
    PSendSysMessage("DebugVis: ground Z=%.2f, you Z=%.2f (delta %.2f).", groundZ, pz, pz - groundZ);
    return true;
}

bool ChatHandler::HandleDebugVisClearCommand(char* /*args*/)
{
    Player* player = m_session ? m_session->GetPlayer() : NULL;
    if (!player)
        return false;

    uint32 removed = DebugVis::Clear(player);
    PSendSysMessage("DebugVis: cleared %u of your markers (the rest had already auto-despawned). "
                    "Markers also auto-clear after %u seconds.", removed, DebugVis::DespawnSeconds());
    return true;
}
