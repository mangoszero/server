/*
 * Anti-Cheat / Movement-Validation framework — central manager.
 *
 * The manager is the single ingress for all detected violations and the ONLY
 * place countermeasures (rubberband / kick) are applied. Detectors never punish
 * directly: they call RecordViolation() and the manager decides.
 *
 * Per-player score is held here in a guid-keyed map rather than on the Player
 * object, which keeps the manager self-contained and independently testable.
 */

#ifndef MANGOS_ANTICHEATMGR_H
#define MANGOS_ANTICHEATMGR_H

#include "Common.h"
#include "AntiCheatDefines.h"

#include <map>
#include <string>
#include <vector>
#include <mutex>

class Player;

// Snapshot of where/what when a violation fired. Cheap to build; passed by const ref.
struct AntiCheatContext
{
    uint32 mapId;
    float  x, y, z;
    float  speed;       // observed horizontal speed (yd/s), 0 if N/A
    uint32 latency;     // session EWMA latency in ms at time of event
    const char* detail; // short static description, never user input

    AntiCheatContext()
        : mapId(0), x(0.f), y(0.f), z(0.f), speed(0.f), latency(0), detail("") {}
};

class AntiCheatMgr
{
    public:
        static AntiCheatMgr* instance()
        {
            static AntiCheatMgr inst;
            return &inst;
        }

        // Read config snapshot + open log channel. Called from World startup.
        void Init();
        // Re-read config (on .reload config). Safe to call repeatedly.
        void LoadConfig();

        bool IsEnabled() const { return m_enabled; }

        // True if this player should not be validated (GM, exempt bot, etc.).
        bool IsExempt(Player* player) const;

        // The single ingress for detectors. Adds weighted score, persists,
        // and evaluates escalation. weight is clamped to a sane range.
        void RecordViolation(Player* player, AntiCheatViolationType type,
                             float weight, AntiCheatContext const& ctx);

        // GM/dev TEST ingress: runs the full scoring + persist + escalation
        // pipeline while BYPASSING the enabled/exempt gate, so `.anticheat test`
        // can exercise every capability on a GM. Not used by detectors.
        void TestInject(Player* player, AntiCheatViolationType type,
                        float weight, AntiCheatContext const& ctx);

        // Append a human-readable snapshot of the live AC config (for diagnostics).
        void BuildDiag(std::string& out);

        // GM tool: set a player's live AC score directly and evaluate escalation
        // (lets a GM drive the score to a threshold to exercise warn/rubberband/kick).
        void SetScore(Player* player, float score);

        // When set, RecordViolation bypasses the enabled/exempt gate and the
        // *Enabled() getters report true — so `.spoof` simulations drive the real
        // detectors and apply results even on an exempt GM / with AC disabled.
        // Set only briefly around a synchronous simulation on the world thread.
        void SetTestBypass(bool on) { m_testBypass = on; }

        // World-tick maintenance: prune idle score entries + apply queued autobans.
        void Update(uint32 diff);

        // Drop per-player state on logout so memory and scores don't leak.
        void RemovePlayer(uint32 lowGuid);

        // GM review: append a human-readable status line for target (or all).
        void BuildStatus(Player* target, std::string& out);

        // GM triage: the highest live (decayed) scores, sorted desc, as
        // (characterLowGuid, score) pairs, capped at `limit`.
        void GetTopScores(uint32 limit, std::vector<std::pair<uint32, float> >& out);

        // Config getters (cached snapshot).
        uint32 GetSpeedTolerancePct() const { return m_speedTolerancePct; }
        uint32 GetTeleportDistance()  const { return m_teleportDistance; }
        bool   MovementEnabled() const { return m_testBypass || (m_enabled && m_movementEnabled); }
        bool   PhysicsEnabled()  const { return m_testBypass || (m_enabled && m_physicsEnabled); }

    private:
        AntiCheatMgr();
        ~AntiCheatMgr() {}
        AntiCheatMgr(AntiCheatMgr const&);
        AntiCheatMgr& operator=(AntiCheatMgr const&);

        struct ScoreState
        {
            float  score;
            uint32 lastUpdateMS;
            uint32 violations;
            ScoreState() : score(0.f), lastUpdateMS(0), violations(0) {}
        };

        // Per-account anti-gaming state for autoban. Decays slowly (hours) using
        // wall-clock time so it survives restarts and so spacing offences out
        // does not evade the ban.
        struct AccountState
        {
            float  kickScore;
            uint32 banCount;
            uint32 lastUpdate;  // unix seconds (sWorld.GetGameTime)
            AccountState() : kickScore(0.f), banCount(0), lastUpdate(0) {}
        };

        // A ban decided on a worker/map thread, applied later on the world thread.
        struct PendingBan
        {
            std::string charName;   // BAN_CHARACTER bans the owning account
            uint32 durationSecs;    // 0 = permanent
            std::string reason;
        };

        // Shared post-gate body for RecordViolation/TestInject: score, persist,
        // escalate.
        void DoRecord(Player* player, AntiCheatViolationType type,
                      float weight, AntiCheatContext const& ctx);

        // Lazily decay the score to "now" using the configured decay rate.
        float DecayedScore(ScoreState& s, uint32 nowMS) const;

        // Apply the highest escalation the (decayed) score warrants, capped by
        // the configured action ceiling. The ONLY place punishment happens.
        void Apply(Player* player, float score, AntiCheatViolationType type,
                   AntiCheatContext const& ctx);

        void Persist(Player* player, AntiCheatViolationType type, float score,
                     AntiCheatContext const& ctx);
        void AlertGMs(Player* player, AntiCheatViolationType type, float score,
                      AntiCheatContext const& ctx);

        // Anti-gaming autoban: accumulate a kick against the player's account and,
        // if the decayed account score crosses the threshold, queue an escalating
        // ban (applied on the world thread in Update()). Called from Apply() on KICK.
        void AccumulateKick(Player* player);
        float DecayedKickScore(AccountState& s, uint32 nowSec) const;
        void LoadAccounts();
        void PersistAccount(uint32 accountId, AccountState const& s);

        bool   m_enabled;
        bool   m_testBypass;          // transient: `.spoof` simulation in progress
        bool   m_movementEnabled;
        bool   m_physicsEnabled;
        bool   m_exemptBots;
        bool   m_persist;
        uint32 m_exemptGmLevel;
        uint32 m_actionCeiling;       // AntiCheatAction
        uint32 m_speedTolerancePct;
        uint32 m_teleportDistance;
        uint32 m_scoreWarn;
        uint32 m_scoreRubberband;
        uint32 m_scoreKick;
        uint32 m_decayPerSec;

        // Autoban config
        bool   m_autobanEnable;
        uint32 m_autobanKickPoints;
        uint32 m_autobanThreshold;
        uint32 m_autobanDecayPerHour;
        uint32 m_autobanDur[3];

        std::map<uint32, ScoreState>   m_scores;   // keyed by character low-guid
        std::map<uint32, AccountState> m_accounts; // keyed by account id (autoban)
        std::vector<PendingBan>        m_pendingBans;
        std::mutex m_lock;
};

#define sAntiCheatMgr AntiCheatMgr::instance()

#endif // MANGOS_ANTICHEATMGR_H
