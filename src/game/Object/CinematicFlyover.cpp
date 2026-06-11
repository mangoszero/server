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
 */

#include "CinematicFlyover.h"
#include "Player.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Map.h"
#include "Camera.h"
#include "Log.h"
#include "Config/Config.h"
#include "SharedDefines.h"

// Config keys
static const uint32 CONFIG_UPDATE_INTERVAL_MS = 200;
static const uint32 CONFIG_TIMEOUT_SEC = 120;
static const uint32 CONFIG_VISIBILITY_DISTANCE = 250;

CinematicFlyover::CinematicFlyover(Player* player, uint8 raceId)
    : m_player(player), m_route(nullptr), m_viewerMap(nullptr),
      m_viewerRadius(0.0f), m_bodyEntry(0), m_elapsedMs(0),
      m_updateTimer(0), m_timeoutMs(0), m_armed(false), m_begun(false),
      m_active(false)
{
    // Validate config is enabled
    if (!sConfig.GetBoolDefault("Cinematic.Flyover.Enable", false))
    {
        sLog.outDebug("CinematicFlyover: Feature disabled by config");
        return;
    }

    // Get route for race
    m_route = GetCinematicFlyoverRouteForRace(raceId);
    if (!m_route)
    {
        sLog.outDebug("CinematicFlyover: No route found for race %u", raceId);
        return;
    }

    // Validate route map matches player map
    if (m_route->mapId != m_player->GetMapId())
    {
        sLog.outDebug("CinematicFlyover: Route map %u does not match player map %u",
                      m_route->mapId, m_player->GetMapId());
        return;
    }

    // Route must have keyframes to spawn at
    if (m_route->keyframeCount == 0)
    {
        sLog.outError("CinematicFlyover: Route has no keyframes");
        return;
    }

    // Validate body entry from config
    uint32 bodyEntry = sConfig.GetIntDefault("Cinematic.Flyover.BodyEntry",
                                            12999);
    if (bodyEntry == 0)
    {
        sLog.outError("CinematicFlyover: BodyEntry is 0, cannot create body");
        return;
    }

    if (!ObjectMgr::GetCreatureTemplate(bodyEntry))
    {
        sLog.outError("CinematicFlyover: Creature entry %u not found in "
                      "creature_template", bodyEntry);
        return;
    }

    m_bodyEntry = bodyEntry;

    // Arm only. The body summon + camera bind are deferred to Begin(), triggered
    // by the first CMSG_NEXT_CINEMATIC_CAMERA. Binding farsight at login (before
    // the client enters the cinematic) hijacks the camera during the brief
    // control window and breaks the intro control-handover (drumbeat) sequence.
    m_armed = true;

    sLog.outDebug("CinematicFlyover: Armed for player %s (race %u), "
                  "awaiting cinematic start", m_player->GetName(), raceId);
}

void CinematicFlyover::Begin()
{
    // Begin once, only if armed at login. Guards repeated CMSG_NEXT_CINEMATIC_CAMERA.
    if (!m_armed || m_begun)
    {
        return;
    }

    // Defensive re-validation: the player must still be in-world on the route map
    if (!m_player || !m_player->IsInWorld() || !m_route ||
        m_route->mapId != m_player->GetMapId())
    {
        sLog.outDebug("CinematicFlyover: Begin aborted, player not on route map");
        m_armed = false;
        return;
    }

    const CinematicFlyoverKeyframe& startKeyframe = m_route->keyframes[0];

    // Spawn temporary invisible creature body.
    // Despawn must outlast the flyover timeout so our Stop() despawns it first.
    uint32 despawnMs = m_route->durationMs + (CONFIG_TIMEOUT_SEC * 1000) +
                       60000; // +60s buffer
    Creature* body = m_player->SummonCreature(m_bodyEntry, startKeyframe.x,
                                              startKeyframe.y, startKeyframe.z,
                                              startKeyframe.orientation,
                                              TEMPSPAWN_TIMED_DESPAWN,
                                              despawnMs, true, false);

    if (!body)
    {
        sLog.outError("CinematicFlyover: Failed to summon body creature entry %u", m_bodyEntry);
        m_armed = false;
        return;
    }

    // Store body GUID for resolution (not a raw pointer)
    m_bodyGuid = body->GetObjectGuid();

    // Widen the populate radius for this viewpoint only, so NPCs along the route
    // stream in well before the camera reaches them and persist as it pulls away.
    // The override lives on the body, so it reverts automatically when the camera
    // resets to the player on stop - no global map state is touched.
    float visDist = float(sConfig.GetIntDefault(
                          "Cinematic.Flyover.VisibilityDistance",
                          CONFIG_VISIBILITY_DISTANCE));
    body->SetVisibilityDistanceOverride(visDist);

    // Extend the map's packet broadcast radius to match: creatures revealed by
    // the wide radius must keep delivering their movement/update packets to
    // this viewer's remote camera, or the client freezes them until the camera
    // closes to the default visibility distance and then fast-walks them to
    // catch up. The registered map/radius are stored so Stop() removes exactly
    // this registration; flyovers only run on continents, which persist.
    m_viewerMap = body->GetMap();
    m_viewerRadius = visDist;
    m_viewerMap->AddCinematicViewer(m_viewerRadius);

    // Bind player camera to body (farsight), now that the client is in the cinematic
    m_player->GetCamera().SetView(body, true);

    // Start the route clock from the cinematic's actual start
    m_elapsedMs = 0;
    m_updateTimer = 0;
    m_timeoutMs = m_route->durationMs + (CONFIG_TIMEOUT_SEC * 1000);
    m_begun = true;
    m_active = true;

    sLog.outDebug("CinematicFlyover: Began for player %s, route duration %u ms",
                  m_player->GetName(), m_route->durationMs);
}

CinematicFlyover::~CinematicFlyover()
{
    Stop();
}

void CinematicFlyover::Update(uint32 updateDiff)
{
    if (!m_active)
    {
        return;
    }

    // Resolve body from GUID (handles async removal safely)
    Creature* body = ResolveBody();
    if (!body)
    {
        sLog.outDebug("CinematicFlyover: Body no longer valid, stopping");
        Stop();
        return;
    }

    // Advance the route clock by real elapsed time every tick so the body
    // tracks the client's cinematic camera. Crediting only the nominal
    // interval per relocation made the body run at ~80% speed (the world
    // tick overshoot was discarded), leaving it ~17s behind by route end.
    m_elapsedMs += updateDiff;

    // Relocate only when the update interval has accumulated
    m_updateTimer += updateDiff;
    uint32 updateInterval = sConfig.GetIntDefault(
                              "Cinematic.Flyover.UpdateIntervalMs",
                              CONFIG_UPDATE_INTERVAL_MS);
    if (m_updateTimer < updateInterval)
    {
        return;
    }

    // Keep the remainder so relocation cadence does not drift either
    m_updateTimer -= updateInterval;
    if (m_updateTimer > updateInterval)
    {
        m_updateTimer = updateInterval; // cap catch-up after a long stall
    }

    // Check timeout
    if (m_elapsedMs > m_timeoutMs)
    {
        sLog.outDebug("CinematicFlyover: Timeout after %u ms", m_elapsedMs);
        Stop();
        return;
    }

    // Interpolate the body to the camera's current route position. The wide
    // visibility override (set in Begin) does the work of streaming NPCs ahead
    // of and behind the camera, so no look-ahead offset is needed here.
    float x, y, z, o;
    if (!InterpolatePosition(m_elapsedMs, x, y, z, o))
    {
        sLog.outDebug("CinematicFlyover: Interpolation failed, stopping");
        Stop();
        return;
    }

    // Relocate body using Map::CreatureRelocation to trigger OnRelocated
    // and visibility updates
    if (body->GetMap())
    {
        body->GetMap()->CreatureRelocation(body, x, y, z, o);

        if (sConfig.GetBoolDefault("Cinematic.Flyover.Debug", false))
        {
            sLog.outDebug("CinematicFlyover: Relocated body to (%.2f, %.2f, %.2f, %.2f)",
                          x, y, z, o);
        }
    }
}

void CinematicFlyover::Stop()
{
    // Disarm so a late CMSG_NEXT_CINEMATIC_CAMERA cannot Begin() after a stop
    m_armed = false;

    if (!m_active)
    {
        return;
    }

    sLog.outDebug("CinematicFlyover: Stopping for player %s",
                  m_player->GetName());

    // Step 1: Reset camera (must happen before body removal)
    m_player->GetCamera().ResetView(true);

    // Step 2: Despawn body (resolve by GUID to avoid dangling pointer)
    Creature* body = ResolveBody();
    if (body)
    {
        body->AddObjectToRemoveList();
    }

    // Step 3: Revert the map's extended broadcast radius - unconditionally
    // paired with Begin's registration, on exactly the map/radius registered
    if (m_viewerMap)
    {
        m_viewerMap->RemoveCinematicViewer(m_viewerRadius);
        m_viewerMap = nullptr;
        m_viewerRadius = 0.0f;
    }

    // Step 4: Clear references
    m_bodyGuid.Clear();
    m_active = false;
}

bool CinematicFlyover::InterpolatePosition(uint32 atMs, float& x, float& y, float& z, float& o)
{
    if (!m_route || m_route->keyframeCount == 0)
    {
        return false;
    }

    // Some client camera tracks begin after 0 ms. Hold the body at the first
    // baked keyframe until the route enters its first interpolated segment.
    if (atMs <= m_route->keyframes[0].timestampMs)
    {
        const CinematicFlyoverKeyframe& first = m_route->keyframes[0];
        x = first.x;
        y = first.y;
        z = first.z;
        o = first.orientation;
        return true;
    }

    // Find surrounding keyframes
    const CinematicFlyoverKeyframe* prev = nullptr;
    const CinematicFlyoverKeyframe* next = nullptr;

    for (uint32 i = 0; i < m_route->keyframeCount - 1; ++i)
    {
        if (m_route->keyframes[i].timestampMs <= atMs &&
            m_route->keyframes[i + 1].timestampMs > atMs)
        {
            prev = &m_route->keyframes[i];
            next = &m_route->keyframes[i + 1];
            break;
        }
    }

    // If we're past the last keyframe, clamp to end
    if (!prev && !next)
    {
        if (atMs >= m_route->keyframes[m_route->keyframeCount - 1].timestampMs)
        {
            const CinematicFlyoverKeyframe& last =
                m_route->keyframes[m_route->keyframeCount - 1];
            x = last.x;
            y = last.y;
            z = last.z;
            o = last.orientation;
            return true;
        }
        return false;
    }

    // Linear interpolation
    if (prev && next)
    {
        uint32 timeDiff = next->timestampMs - prev->timestampMs;
        if (timeDiff == 0)
        {
            return false;
        }

        float t = float(atMs - prev->timestampMs) / float(timeDiff);
        x = prev->x + t * (next->x - prev->x);
        y = prev->y + t * (next->y - prev->y);
        z = prev->z + t * (next->z - prev->z);

        // Linear orientation interpolation
        o = prev->orientation + t * (next->orientation - prev->orientation);

        return true;
    }

    return false;
}

Creature* CinematicFlyover::ResolveBody() const
{
    if (m_bodyGuid.IsEmpty())
    {
        return nullptr;
    }

    // Resolve body from map by GUID (safe against async removal)
    if (!m_player || !m_player->GetMap())
    {
        return nullptr;
    }

    return m_player->GetMap()->GetCreature(m_bodyGuid);
}
