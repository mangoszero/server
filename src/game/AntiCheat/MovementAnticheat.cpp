/*
 * Anti-Cheat / Movement-Validation framework — per-player movement validator.
 */

#include "MovementAnticheat.h"
#include "AntiCheatMgr.h"
#include "PhysicsValidator.h"
#include "Player.h"
#include "World.h"
#include "Unit.h"
#include "SpellAuraDefines.h"
#include "Map.h"
#include "Opcodes.h"
#include "Timer.h"
#include "Log.h"

#include <cmath>

namespace
{
    const float  VERT_CLIMB_SUSPECT = 5.0f;   // yd upward in one packet on ground
    const float  SPEED_SLACK_YD     = 2.0f;   // constant distance fudge per packet
    const uint32 GAP_RESET_MS       = 3000;   // packet gap implying load/teleport
    const float  FALL_SUPPRESS_YD   = 20.0f;  // drop beyond this should incur fall damage
    const uint32 BURST_PER_SEC      = 50;     // movement packets/sec beyond this = burst
    const uint32 CLIENT_TIME_BACK_MS = 500;   // client timestamp regression tolerance
    const uint32 CAST_WINDOW_MS      = 1000;   // cast-spam counting window
    const uint32 CAST_GCD_SLACK_MS   = 150;    // tolerance below GCD on top of latency
    const float  NOCLIP_MIN_STEP     = 4.0f;   // min ground step (yd) to run the LoS no-clip test

    // Active locomotion-START opcodes — illegal to issue while rooted/stunned (a
    // legit client suppresses them in that state). Stop/heartbeat/turn opcodes are
    // allowed (turning in place is fine while rooted).
    bool IsActiveMoveStart(uint16 op)
    {
        switch (op)
        {
            case MSG_MOVE_START_FORWARD:
            case MSG_MOVE_START_BACKWARD:
            case MSG_MOVE_START_STRAFE_LEFT:
            case MSG_MOVE_START_STRAFE_RIGHT:
            case MSG_MOVE_START_SWIM:
            case MSG_MOVE_JUMP:
                return true;
            default:
                return false;
        }
    }
}

MovementAnticheat::MovementAnticheat(Player* owner)
    : m_player(owner), m_hasLast(false), m_trustNext(false),
      m_lastX(0.f), m_lastY(0.f), m_lastZ(0.f), m_lastO(0.f),
      m_lastMS(0), m_lastFlags(0),
      m_hasValid(false), m_validX(0.f), m_validY(0.f), m_validZ(0.f), m_validO(0.f),
      m_airborne(false), m_fallApexZ(0.f),
      m_burstWinStartMS(0), m_burstCount(0), m_lastClientTime(0), m_hasClientTime(false),
      m_hasLastCast(false), m_lastCastMS(0), m_lastCastGcd(0),
      m_castWinStartMS(0), m_castCount(0),
      m_hasKin(false), m_lastSpeed(0.f),
      m_grantedFlags(0),
      m_botWinStartMS(0), m_botSamples(0), m_botCleanCycles(0), m_botRunDist(0.f),
      m_botHasHeading(false), m_botLastHeading(0.f),
      m_botHasPkt(false), m_botLastPktMS(0), m_botIntervalN(0), m_botIntMean(0.f), m_botIntM2(0.f)
{
}

AntiCheatMoveState MovementAnticheat::NormalizeState(MovementInfo const& mi) const
{
    if (mi.HasMovementFlag(MOVEFLAG_ONTRANSPORT))
        return AC_MOVE_TRANSPORT;
    if (mi.HasMovementFlag(MOVEFLAG_SWIMMING))
        return AC_MOVE_SWIM;
    if (mi.HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
        return AC_MOVE_FALL;
    if (mi.HasMovementFlag(MovementFlags(MOVEFLAG_FLYING | MOVEFLAG_CAN_FLY | MOVEFLAG_LEVITATING)))
        return AC_MOVE_FLY;
    return AC_MOVE_GROUND;
}

void MovementAnticheat::HandlePositionUpdate(uint16 opcode, MovementInfo const& mi)
{
    if (!m_player || !sAntiCheatMgr->MovementEnabled())
        return;

    uint32 nowMS = getMSTime();
    Position const* pos = mi.GetPos();
    AntiCheatMoveState state = NormalizeState(mi);

    // (Re)establish baseline on first packet, after a server relocation, after a
    // long gap (loading screen / teleport), or while on a taxi spline.
    if (!m_hasLast || m_trustNext ||
        getMSTimeDiff(m_lastMS, nowMS) > GAP_RESET_MS || m_player->IsTaxiFlying())
    {
        m_trustNext = false;
        m_hasLast = true;
        m_lastX = pos->x; m_lastY = pos->y; m_lastZ = pos->z; m_lastO = pos->o;
        m_lastMS = nowMS; m_lastFlags = mi.GetMovementFlags();
        m_hasValid = true;
        m_validX = pos->x; m_validY = pos->y; m_validZ = pos->z; m_validO = pos->o;
        m_airborne = (state == AC_MOVE_FALL);
        m_fallApexZ = pos->z;
        m_hasClientTime = false;
        m_burstWinStartMS = nowMS;
        m_burstCount = 0;
        return;
    }

    // --- Detector: movement-packet burst (flood / timing manipulation) ---
    if (getMSTimeDiff(m_burstWinStartMS, nowMS) >= 1000)
    {
        m_burstWinStartMS = nowMS;
        m_burstCount = 0;
    }
    ++m_burstCount;
    if (m_burstCount == BURST_PER_SEC + 1) // fire once when first exceeding the cap
    {
        AntiCheatContext bctx;
        bctx.mapId = m_player->GetMapId();
        bctx.x = pos->x; bctx.y = pos->y; bctx.z = pos->z;
        bctx.latency = m_player->GetSession() ? m_player->GetSession()->GetLatencyEWMA() : 0;
        bctx.detail = "movement packet burst";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_BURST, 15.0f, bctx);
    }

    // --- Detector: client movement-timestamp regression ---
    {
        uint32 ct = mi.GetTime();
        if (m_hasClientTime)
        {
            if (m_lastClientTime > ct && (m_lastClientTime - ct) > CLIENT_TIME_BACK_MS)
            {
                AntiCheatContext tctx;
                tctx.mapId = m_player->GetMapId();
                tctx.x = pos->x; tctx.y = pos->y; tctx.z = pos->z;
                tctx.latency = m_player->GetSession() ? m_player->GetSession()->GetLatencyEWMA() : 0;
                tctx.detail = "client timestamp regression";
                sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PACKETTIMING, 10.0f, tctx);
            }
        }
        m_lastClientTime = ct;
        m_hasClientTime = true;
    }

    uint32 dtMS = getMSTimeDiff(m_lastMS, nowMS);
    if (dtMS == 0)
        dtMS = 1;
    float dtSec = float(dtMS) / 1000.0f;

    float dx = pos->x - m_lastX;
    float dy = pos->y - m_lastY;
    float dz = pos->z - m_lastZ;
    float horiz = sqrtf(dx * dx + dy * dy);

    uint32 latency = m_player->GetSession() ? m_player->GetSession()->GetLatencyEWMA() : 0;

    // Choose the relevant speed for the current state.
    UnitMoveType mtype = MOVE_RUN;
    if (state == AC_MOVE_SWIM)
        mtype = MOVE_SWIM;
    else if (mi.HasMovementFlag(MOVEFLAG_WALK_MODE))
        mtype = MOVE_WALK;
    float allowed = m_player->GetSpeed(mtype);

    AntiCheatContext ctx;
    ctx.mapId = m_player->GetMapId();
    ctx.x = pos->x; ctx.y = pos->y; ctx.z = pos->z;
    ctx.speed = horiz / dtSec;
    ctx.latency = latency;

    bool cheapTrip = false;

    // --- Detector: flag contradiction (vanilla players never legitimately fly,
    // unless the server granted it e.g. via GM tooling). ---
    if (mi.HasMovementFlag(MovementFlags(MOVEFLAG_FLYING | MOVEFLAG_CAN_FLY)) &&
        !(m_grantedFlags & (MOVEFLAG_FLYING | MOVEFLAG_CAN_FLY)))
    {
        ctx.detail = "fly movement flag set";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FLAG_CONTRADICT, 40.0f, ctx);
        cheapTrip = true;
    }

    // --- Detectors: movement-flag spoofing (the client asserts a capability flag
    // it has no aura/grant to back). Legit if a backing aura OR a server grant is
    // present; only judged for the living player. ---
    if (m_player->IsAlive())
    {
        if (mi.HasMovementFlag(MOVEFLAG_WATERWALKING) && !(m_grantedFlags & MOVEFLAG_WATERWALKING) &&
            !m_player->HasAuraType(SPELL_AURA_WATER_WALK))
        {
            ctx.detail = "water-walk flag without aura";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FLAG_CONTRADICT, 25.0f, ctx);
            cheapTrip = true;
        }
        if (mi.HasMovementFlag(MOVEFLAG_HOVER) && !(m_grantedFlags & MOVEFLAG_HOVER) &&
            !m_player->HasAuraType(SPELL_AURA_HOVER))
        {
            ctx.detail = "hover flag without aura";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FLAG_CONTRADICT, 25.0f, ctx);
            cheapTrip = true;
        }
        // Rogue "Safe Fall" is a class passive (no feather-fall aura) — excluded.
        if (mi.HasMovementFlag(MOVEFLAG_SAFE_FALL) && !(m_grantedFlags & MOVEFLAG_SAFE_FALL) &&
            !m_player->HasAuraType(SPELL_AURA_FEATHER_FALL) && m_player->getClass() != CLASS_ROGUE)
        {
            ctx.detail = "slow-fall flag without aura";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FLAG_CONTRADICT, 15.0f, ctx);
        }
    }

    // --- Detector: transport-flag spoof (claims ONTRANSPORT with no transport) —
    // closes the bypass where the speed/teleport detectors skip transport state. ---
    if (state == AC_MOVE_TRANSPORT && !m_player->GetTransport())
    {
        ctx.detail = "transport flag without transport (bypass)";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FLAG_CONTRADICT, 20.0f, ctx);
    }

    // --- Detector: swim-flag spoof (swimming while not in liquid) ---
    if (state == AC_MOVE_SWIM && !m_player->IsInWater())
    {
        ctx.detail = "swim flag while not in water";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FLAG_CONTRADICT, 20.0f, ctx);
    }

    // --- Detector: teleport / blink (single-packet displacement) ---
    float teleMax = float(sAntiCheatMgr->GetTeleportDistance())
                  + allowed * (float(latency) / 1000.0f);
    if (state != AC_MOVE_TRANSPORT && !m_player->IsTaxiFlying() && horiz > teleMax)
    {
        ctx.detail = "teleport/blink jump";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_TELEPORT, 25.0f, ctx);
        cheapTrip = true;
    }
    else if (state != AC_MOVE_TRANSPORT)
    {
        // --- Detector: speed (distance vs time, latency-tolerant) ---
        float tol = float(sAntiCheatMgr->GetSpeedTolerancePct()) / 100.0f;
        float expectMax = allowed * tol * dtSec
                        + allowed * (float(latency) / 1000.0f) + SPEED_SLACK_YD;
        if (horiz > expectMax * 1.05f && allowed > 0.0f)
        {
            float ratio = horiz / (expectMax > 0.01f ? expectMax : 0.01f);
            float weight = (ratio - 1.0f) * 50.0f;
            if (weight < 5.0f) weight = 5.0f;
            if (weight > 25.0f) weight = 25.0f;
            ctx.detail = "speed over allowed";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_SPEED, weight, ctx);
            cheapTrip = true;
        }
    }

    // --- Detector: acceleration / velocity-delta gate (config-gated, OFF by
    // default — FP-prone). Catches a sudden implausible speed increase to a real
    // speed within one packet (instant 0->fast stutter / oscillating speedhacks
    // that average legitimately) that the steady-state speed check misses. Only on
    // ground, not after teleport, not when another detector already tripped. ---
    if (sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_ACCEL_CHECK) && m_hasKin && !cheapTrip &&
        !m_trustNext && state == AC_MOVE_GROUND && allowed > 0.0f)
    {
        float dv = ctx.speed - m_lastSpeed;            // accelerating only
        if (dv > 0.0f)
        {
            float accel = dv / dtSec;
            float cap = allowed * float(sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_ACCEL_MULT));
            if (accel > cap && ctx.speed > allowed * 0.5f)
            {
                float ratio = accel / (cap > 0.01f ? cap : 0.01f);
                float weight = (ratio - 1.0f) * 20.0f;
                if (weight < 5.0f)  weight = 5.0f;
                if (weight > 25.0f) weight = 25.0f;
                ctx.detail = "implausible acceleration (velocity delta)";
                sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_SPEED, weight, ctx);
            }
        }
    }

    // --- Detector: opcode legality by state — an active locomotion-START command
    // issued while the player cannot move (rooted/stunned) is illegal; a legit
    // client never sends one in that state. ---
    if (IsActiveMoveStart(opcode) && m_player->hasUnitState(UNIT_STAT_ROOT | UNIT_STAT_STUNNED))
    {
        ctx.detail = "move-start opcode while rooted/stunned";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PHYSICS, 15.0f, ctx);
    }

    // --- Detector: movement while rooted (root-break) ---
    // A rooted unit may turn/jump in place but never translate horizontally. A
    // clear horizontal step while UNIT_STAT_ROOT is set is a root-break hack.
    // Restricted to grounded, non-flagged packets to exclude knockback/transport.
    if (state == AC_MOVE_GROUND && horiz > 3.0f && !cheapTrip &&
        m_player->hasUnitState(UNIT_STAT_ROOT | UNIT_STAT_STUNNED))
    {
        // Fear/confuse are server-driven movement, so only root/stun are judged.
        ctx.detail = "horizontal movement while rooted/stunned";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PHYSICS, 20.0f, ctx);
        cheapTrip = true;
    }

    // --- Detector: unexplained vertical climb on the ground ---
    if (state == AC_MOVE_GROUND && dz > VERT_CLIMB_SUSPECT && horiz < dz &&
        !mi.HasMovementFlag(MovementFlags(MOVEFLAG_FALLING | MOVEFLAG_FALLINGFAR)))
    {
        ctx.detail = "vertical climb without cause";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_VERTICAL, 15.0f, ctx);
        cheapTrip = true;
    }

    // --- Physics plausibility (moderate cost): only on suspicion or vertical move ---
    if (sAntiCheatMgr->PhysicsEnabled() && state == AC_MOVE_GROUND &&
        (cheapTrip || fabs(dz) > 2.0f))
    {
        const char* reason = NULL;
        AntiCheatPhysicsResult r = PhysicsValidator::Validate(m_player, state, mi, &reason);
        if (r == AC_PHYS_IMPOSSIBLE)
        {
            ctx.detail = reason ? reason : "physics impossible";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PHYSICS, 30.0f, ctx);
        }
        else if (r == AC_PHYS_SUSPECT)
        {
            ctx.detail = reason ? reason : "physics suspect";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PHYSICS, 10.0f, ctx);
        }
    }

    // --- Detector: wall-clip / no-clip (moved through solid geometry) ---
    // A legitimate step keeps line-of-sight between the previous and the new
    // position; a sizable ground step with NO LoS between them means the client
    // walked through world geometry. VMap query, so gated behind the physics
    // module + a step floor (bounds cost and corner false-positives). Skipped
    // right after a server relocation and when another detector already tripped.
    if (sAntiCheatMgr->PhysicsEnabled() && m_hasLast && !m_trustNext && !cheapTrip &&
        state == AC_MOVE_GROUND && horiz > NOCLIP_MIN_STEP)
    {
        Map* map = m_player->GetMap();
        if (map && !map->IsInLineOfSight(m_lastX, m_lastY, m_lastZ + 1.5f,
                                         pos->x, pos->y, pos->z + 1.5f))
        {
            ctx.detail = "moved through geometry (no-clip)";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PHYSICS, 15.0f, ctx);
            cheapTrip = true;
        }
    }

    // --- Detectors: illegal jump (infinite/double jump) + fall-damage suppression ---
    if (opcode == MSG_MOVE_JUMP)
    {
        // A jump issued while already airborne (no FALL_LAND since the last jump
        // or fall) is an illegal mid-air / infinite jump.
        if (m_airborne)
        {
            ctx.detail = "mid-air / infinite jump";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_JUMP, 30.0f, ctx);
            cheapTrip = true;
        }
        m_airborne = true;
        m_fallApexZ = pos->z;
    }
    else if (state == AC_MOVE_FALL)
    {
        // In a fall/jump arc: track the episode and its apex.
        m_airborne = true;
        if (pos->z > m_fallApexZ)
            m_fallApexZ = pos->z;
    }
    else if (m_airborne)
    {
        // Episode ended this packet. A legit landing sends MSG_MOVE_FALL_LAND and
        // the core applies fall damage. Becoming grounded WITHOUT a FALL_LAND after
        // a damaging drop (and not into water) means the client suppressed fall damage.
        float drop = m_fallApexZ - pos->z;
        if (opcode != MSG_MOVE_FALL_LAND && state != AC_MOVE_SWIM && drop >= FALL_SUPPRESS_YD)
        {
            ctx.detail = "fall-damage suppressed (no FALL_LAND)";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_FALL, 25.0f, ctx);
            cheapTrip = true;
        }
        m_airborne = false;
        m_fallApexZ = pos->z;
    }
    else
    {
        // Grounded: keep the apex tracking current so the next fall measures from here.
        m_fallApexZ = pos->z;
    }

    // --- Detector: bot-like movement (snap-to-waypoint + metronomic timing) ---
    // Classic third-party bots path in straight lines between waypoints, turning
    // SHARPLY at each node, on a metronomic clock. Humans wander and have jittery
    // timing. Accumulated over a 30s window so a one-off human sharp turn (e.g. a
    // both-mouse-button look-behind flip) can't trip it — only a sustained, repeated,
    // regular pattern does. Config-gated (heuristic), OFF by default.
    if (sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_BOT_DETECT) && state != AC_MOVE_TRANSPORT)
    {
        if (m_botWinStartMS == 0)
            m_botWinStartMS = nowMS;

        // Inter-packet timing regularity (Welford running variance).
        if (m_botHasPkt)
        {
            float iv = float(getMSTimeDiff(m_botLastPktMS, nowMS));
            ++m_botIntervalN;
            float d = iv - m_botIntMean;
            m_botIntMean += d / float(m_botIntervalN);
            m_botIntM2 += d * (iv - m_botIntMean);
        }
        m_botLastPktMS = nowMS; m_botHasPkt = true;

        if (horiz > 1.0f)
        {
            ++m_botSamples;
            const float PI_F = 3.14159265f;
            float heading = atan2f(dy, dx);
            if (m_botHasHeading)
            {
                float turn = fabs(heading - m_botLastHeading);
                if (turn > PI_F) turn = 2.0f * PI_F - turn;   // normalise to 0..pi
                if (turn > 1.4f)            // ~80deg: a sharp "snap"
                {
                    // a snap that ended a sustained straight run = waypoint cycle
                    if (m_botRunDist >= 15.0f) ++m_botCleanCycles;
                    m_botRunDist = 0.0f;
                }
                else if (turn < 0.35f)      // ~20deg: still a straight line
                    m_botRunDist += horiz;
                else
                    m_botRunDist = 0.0f;     // gentle curve, not a clean straight run
            }
            m_botLastHeading = heading; m_botHasHeading = true;
        }

        if (getMSTimeDiff(m_botWinStartMS, nowMS) >= 30000)
        {
            if (m_botSamples >= 50 && m_botIntervalN >= 30)
            {
                float var = m_botIntM2 / float(m_botIntervalN);
                float sd = sqrtf(var > 0.0f ? var : 0.0f);
                float cv = m_botIntMean > 1.0f ? sd / m_botIntMean : 1.0f;
                // bot = repeated clean snap->straight cycles AND metronomic timing
                if (m_botCleanCycles >= 4 && cv < 0.15f)
                {
                    ctx.detail = "bot-like movement (snap-to-waypoint + regular timing)";
                    sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_BOT, 12.0f, ctx);
                }
            }
            m_botWinStartMS = nowMS; m_botSamples = 0; m_botCleanCycles = 0; m_botRunDist = 0.0f;
            m_botIntervalN = 0; m_botIntMean = 0.0f; m_botIntM2 = 0.0f;
        }
    }

    // Update the rolling baseline. Track last clean position for rubberband use
    // by the enforcement path (a non-teleport, non-impossible packet).
    m_lastX = pos->x; m_lastY = pos->y; m_lastZ = pos->z; m_lastO = pos->o;
    m_lastMS = nowMS; m_lastFlags = mi.GetMovementFlags();
    m_lastSpeed = ctx.speed; m_hasKin = true;   // for the acceleration gate
    if (!cheapTrip)
    {
        m_hasValid = true;
        m_validX = pos->x; m_validY = pos->y; m_validZ = pos->z; m_validO = pos->o;
    }
}

void MovementAnticheat::PeriodicCheck()
{
    if (!m_player || !m_player->IsInWorld())
        return;
    if (sAntiCheatMgr->IsExempt(m_player))
        return;

    // Idle terrain re-validation needs the physics module enabled.
    if (!sAntiCheatMgr->PhysicsEnabled())
        return;

    // Re-validate the player's current (idle) position against terrain — catches
    // static exploits with no movement packets. Only the grounded case is judged.
    AntiCheatMoveState state = NormalizeState(m_player->m_movementInfo);
    if (state != AC_MOVE_GROUND)
        return;

    const char* reason = NULL;
    if (PhysicsValidator::Validate(m_player, state, m_player->m_movementInfo, &reason) == AC_PHYS_IMPOSSIBLE)
    {
        AntiCheatContext ctx;
        ctx.mapId = m_player->GetMapId();
        ctx.x = m_player->GetPositionX(); ctx.y = m_player->GetPositionY(); ctx.z = m_player->GetPositionZ();
        ctx.latency = m_player->GetSession() ? m_player->GetSession()->GetLatencyEWMA() : 0;
        ctx.detail = reason ? reason : "physics impossible (idle)";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_PHYSICS, 20.0f, ctx);
    }
}

void MovementAnticheat::NotifySpellCast(uint32 /*spellId*/, uint32 /*castTimeMs*/, uint32 gcdMs)
{
    if (!m_player || !sAntiCheatMgr->MovementEnabled() || sAntiCheatMgr->IsExempt(m_player))
        return;

    uint32 now = getMSTime();
    uint32 latency = m_player->GetSession() ? m_player->GetSession()->GetLatencyEWMA() : 0;

    AntiCheatContext ctx;
    ctx.mapId = m_player->GetMapId();
    ctx.x = m_player->GetPositionX(); ctx.y = m_player->GetPositionY(); ctx.z = m_player->GetPositionZ();
    ctx.latency = latency;

    // --- Detector: cast spam (too many cast requests per second) ---
    if (m_castWinStartMS == 0 || now - m_castWinStartMS > CAST_WINDOW_MS)
    {
        m_castWinStartMS = now;
        m_castCount = 0;
    }
    ++m_castCount;
    if (m_castCount > sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_CAST_BURST))
    {
        ctx.detail = "spell cast spam";
        sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_SPELL, 12.0f, ctx);
    }

    // --- Detector: GCD bypass (two GCD-triggering casts closer than the global
    // cooldown, minus latency + slack, allows) — a cast-speed / no-GCD hack. ---
    if (m_hasLastCast && gcdMs > 0 && m_lastCastGcd > 0)
    {
        uint32 interval = now - m_lastCastMS;
        uint32 floorMs = (m_lastCastGcd > latency + CAST_GCD_SLACK_MS)
                       ? m_lastCastGcd - latency - CAST_GCD_SLACK_MS : 0;
        if (floorMs > 0 && interval < floorMs)
        {
            float ratio = float(floorMs - interval) / float(floorMs);   // 0..1
            float weight = 8.0f + ratio * 17.0f;                        // 8..25
            ctx.detail = "cast faster than GCD (cast-speed hack)";
            sAntiCheatMgr->RecordViolation(m_player, AC_VIOLATION_SPELL, weight, ctx);
        }
    }

    m_hasLastCast = true;
    m_lastCastMS = now;
    m_lastCastGcd = gcdMs;
}
