/*
 * Anti-Cheat / Movement-Validation framework — per-player movement validator.
 *
 * One instance is owned by each Player. It tracks the last accepted movement
 * snapshot, normalises move state, runs the cheap event-driven detectors on each
 * movement packet, and reports trips to AntiCheatMgr (which owns all response).
 * It NEVER punishes or rejects packets itself — it only observes + scores.
 */

#ifndef MANGOS_ANTICHEAT_MOVEMENTANTICHEAT_H
#define MANGOS_ANTICHEAT_MOVEMENTANTICHEAT_H

#include "Common.h"
#include "AntiCheatDefines.h"

#include <string>

class Player;
class MovementInfo;

class MovementAnticheat
{
    public:
        explicit MovementAnticheat(Player* owner);

        // Main entry: called from the movement opcode handler after the packet
        // is parsed and before it is applied. opcode is the movement opcode.
        void HandlePositionUpdate(uint16 opcode, MovementInfo const& mi);

        // Periodic (timer-driven from Player::Update) re-validation of the player's
        // current position — a second cadence that catches static exploits where no
        // movement packets are sent (e.g. standing inside geometry).
        void PeriodicCheck();

        // Called when the SERVER relocates the player (teleport ack, map change)
        // so the next client packet is trusted and the baseline is rebuilt
        // instead of being scored as an impossible jump.
        void NotifyServerRelocation() { m_trustNext = true; }

        // Record a server-GRANTED movement capability flag (water-walk, hover,
        // slow-fall, levitate, fly) so the flag-spoof detectors treat the client
        // asserting it as legitimate (aura OR granted), not a spoof. Wired into the
        // Player Set* setters so all callers (GM commands, auras, scripts) agree.
        void SetGrantedFlag(uint32 flag, bool on)
        {
            if (on) m_grantedFlags |= flag; else m_grantedFlags &= ~flag;
        }

        // Time-based anti-cheat for the spell-cast path (CMSG_CAST_SPELL): detects
        // GCD bypass (casts closer together than the spell's global cooldown allows)
        // and cast spam. castTimeMs/gcdMs come from the spell entry.
        void NotifySpellCast(uint32 spellId, uint32 castTimeMs, uint32 gcdMs);

        // Last position that passed the teleport/physics gates (rubberband target
        // for enforcement). Valid only if HasValid() is true.
        bool HasValid() const { return m_hasValid; }
        float ValidX() const { return m_validX; }
        float ValidY() const { return m_validY; }
        float ValidZ() const { return m_validZ; }
        float ValidO() const { return m_validO; }

    private:
        AntiCheatMoveState NormalizeState(MovementInfo const& mi) const;

        Player* m_player;

        bool   m_hasLast;
        bool   m_trustNext;
        float  m_lastX, m_lastY, m_lastZ, m_lastO;
        uint32 m_lastMS;
        uint32 m_lastFlags;

        bool   m_hasValid;
        float  m_validX, m_validY, m_validZ, m_validO;

        // Jump / fall state machine (infinite-jump + fall-damage-suppression).
        bool   m_airborne;     // in a jump/fall episode (no FALL_LAND yet)
        float  m_fallApexZ;    // highest Z reached during the current airborne episode

        // Packet-burst + client-timestamp tracking.
        uint32 m_burstWinStartMS;  // start of the current 1s burst-count window
        uint32 m_burstCount;       // movement packets seen in the current window
        uint32 m_lastClientTime;   // last MovementInfo client timestamp
        bool   m_hasClientTime;

        // Spell-cast timing (GCD bypass + cast spam).
        bool   m_hasLastCast;
        uint32 m_lastCastMS;
        uint32 m_lastCastGcd;
        uint32 m_castWinStartMS;
        uint32 m_castCount;

        // Kinematics (acceleration / velocity-delta gate).
        bool   m_hasKin;
        float  m_lastSpeed;

        // Server-granted movement capability flags (water-walk/hover/etc.) — the
        // flag-spoof detectors accept these as legitimate alongside auras.
        uint32 m_grantedFlags;

        // Bot-movement heuristic (snap-to-waypoint + metronomic timing), windowed.
        uint32 m_botWinStartMS;
        uint32 m_botSamples;       // moving packets this window
        uint32 m_botCleanCycles;   // snap-after-straight-run cycles this window
        float  m_botRunDist;       // current straight-run distance since last snap
        bool   m_botHasHeading;
        float  m_botLastHeading;
        bool   m_botHasPkt;
        uint32 m_botLastPktMS;
        uint32 m_botIntervalN;     // inter-packet interval count (Welford)
        float  m_botIntMean;
        float  m_botIntM2;
};

#endif // MANGOS_ANTICHEAT_MOVEMENTANTICHEAT_H
