/*
 * Anti-Cheat / Movement-Validation framework — central manager implementation.
 */

#include "AntiCheatMgr.h"
#include "MovementAnticheat.h"
#include "Player.h"
#include "World.h"
#include "Log.h"
#include "Timer.h"
#include "Database/DatabaseEnv.h"

#include <cstdio>
#include <algorithm>

AntiCheatMgr::AntiCheatMgr()
    : m_enabled(false), m_movementEnabled(false), m_physicsEnabled(false),
      m_exemptBots(true), m_persist(true), m_exemptGmLevel(1),
      m_actionCeiling(AC_ACTION_LOG), m_speedTolerancePct(110),
      m_teleportDistance(50), m_scoreWarn(30), m_scoreRubberband(60),
      m_scoreKick(120), m_decayPerSec(2),
      m_autobanEnable(false), m_autobanKickPoints(10), m_autobanThreshold(30),
      m_autobanDecayPerHour(1)
{
    m_autobanDur[0] = 86400; m_autobanDur[1] = 604800; m_autobanDur[2] = 0;
}

void AntiCheatMgr::Init()
{
    LoadConfig();
    if (m_autobanEnable)
        LoadAccounts();
    if (m_enabled)
        sLog.outString("AntiCheat: enabled (action ceiling=%u, movement=%u, physics=%u)",
                       m_actionCeiling, MovementEnabled() ? 1 : 0, PhysicsEnabled() ? 1 : 0);
    else
        sLog.outString("AntiCheat: disabled (AntiCheat.Enable = 0)");
}

void AntiCheatMgr::LoadConfig()
{
    m_enabled         = sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_ENABLE);
    m_movementEnabled = sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_MOVEMENT);
    m_physicsEnabled  = sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_PHYSICS);
    m_exemptBots      = sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_EXEMPT_BOTS);
    m_persist         = sWorld.getConfig(CONFIG_BOOL_ANTICHEAT_PERSIST);
    m_exemptGmLevel   = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_EXEMPT_GM);
    m_actionCeiling   = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_ACTION);
    m_speedTolerancePct = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_SPEED_TOL);
    m_teleportDistance  = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_TELE_DIST);
    m_scoreWarn       = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_SCORE_WARN);
    m_scoreRubberband = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_SCORE_RUBBER);
    m_scoreKick       = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_SCORE_KICK);
    m_decayPerSec     = sWorld.getConfig(CONFIG_UINT32_ANTICHEAT_DECAY);

    m_autobanEnable      = sWorld.getConfig(CONFIG_BOOL_AC_AUTOBAN_ENABLE);
    m_autobanKickPoints  = sWorld.getConfig(CONFIG_UINT32_AC_AUTOBAN_KICKPOINTS);
    m_autobanThreshold   = sWorld.getConfig(CONFIG_UINT32_AC_AUTOBAN_THRESHOLD);
    m_autobanDecayPerHour = sWorld.getConfig(CONFIG_UINT32_AC_AUTOBAN_DECAY_PER_HOUR);
    m_autobanDur[0]      = sWorld.getConfig(CONFIG_UINT32_AC_AUTOBAN_DUR1);
    m_autobanDur[1]      = sWorld.getConfig(CONFIG_UINT32_AC_AUTOBAN_DUR2);
    m_autobanDur[2]      = sWorld.getConfig(CONFIG_UINT32_AC_AUTOBAN_DUR3);
}

bool AntiCheatMgr::IsExempt(Player* player) const
{
    if (!player)
        return true;

    // GMs at or above the configured security level are not validated.
    // ExemptGMLevel == 0 means "exempt nobody by level" (every account is >= 0,
    // so without this guard a value of 0 would exempt everyone).
    if (m_exemptGmLevel > 0 && player->GetSession() &&
        player->GetSession()->GetSecurity() >= (AccountTypes)m_exemptGmLevel)
        return true;

    // A GM with .gm on is also exempt regardless of level.
    if (player->isGameMaster())
        return true;

#ifdef ENABLE_PLAYERBOTS
    if (m_exemptBots && player->GetPlayerbotAI())
        return true;
#endif

    return false;
}

float AntiCheatMgr::DecayedScore(ScoreState& s, uint32 nowMS) const
{
    if (s.lastUpdateMS && m_decayPerSec)
    {
        uint32 elapsedMS = getMSTimeDiff(s.lastUpdateMS, nowMS);
        float decay = (float(elapsedMS) / 1000.0f) * float(m_decayPerSec);
        s.score = s.score > decay ? s.score - decay : 0.0f;
    }
    s.lastUpdateMS = nowMS;
    return s.score;
}

void AntiCheatMgr::RecordViolation(Player* player, AntiCheatViolationType type,
                                   float weight, AntiCheatContext const& ctx)
{
    if (!player)
        return;
    if (!m_enabled || IsExempt(player))
        return;

    // Clamp weight defensively so a single buggy detector can't spike the score.
    if (weight < 0.0f) weight = 0.0f;
    if (weight > 100.0f) weight = 100.0f;

    uint32 nowMS = getMSTime();
    uint32 lowGuid = player->GetGUIDLow();
    float score;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        ScoreState& s = m_scores[lowGuid];
        DecayedScore(s, nowMS);
        s.score += weight;
        ++s.violations;
        score = s.score;
    }

    if (m_persist)
        Persist(player, type, score, ctx);

    Apply(player, score, type, ctx);
}

void AntiCheatMgr::Apply(Player* player, float score, AntiCheatViolationType type,
                         AntiCheatContext const& ctx)
{
    // Highest warranted action, capped by the configured ceiling. This is the
    // ONLY place a countermeasure is applied; detectors never punish directly.
    if (m_actionCeiling >= AC_ACTION_KICK && score >= float(m_scoreKick))
    {
        sLog.outError("AntiCheat: KICK guid=%u type=%u score=%.0f map=%u pos=(%.1f,%.1f,%.1f) %s",
                      player->GetGUIDLow(), type, score, ctx.mapId, ctx.x, ctx.y, ctx.z, ctx.detail);
        AlertGMs(player, type, score, ctx);
        // Anti-gaming autoban: count this kick against the account first.
        if (m_autobanEnable)
            AccumulateKick(player);
        if (player->GetSession())
            player->GetSession()->KickPlayer();
        return;
    }

    if (m_actionCeiling >= AC_ACTION_RUBBERBAND && score >= float(m_scoreRubberband))
    {
        // Rubberband to the last server-validated position tracked by the
        // per-player movement validator.
        MovementAnticheat* mac = player->GetMovementAnticheat();
        if (mac && mac->HasValid())
        {
            player->NearTeleportTo(mac->ValidX(), mac->ValidY(), mac->ValidZ(), mac->ValidO());
            sLog.outDetail("AntiCheat: rubberband guid=%u score=%.0f -> (%.1f,%.1f,%.1f)",
                           player->GetGUIDLow(), score, mac->ValidX(), mac->ValidY(), mac->ValidZ());
        }
        AlertGMs(player, type, score, ctx);
        return;
    }

    if (m_actionCeiling >= AC_ACTION_GM_ALERT && score >= float(m_scoreWarn))
    {
        AlertGMs(player, type, score, ctx);
        return;
    }

    if (m_actionCeiling >= AC_ACTION_LOG)
    {
        sLog.outDetail("AntiCheat: log guid=%u type=%u score=%.0f map=%u pos=(%.1f,%.1f,%.1f) speed=%.1f lat=%u %s",
                       player->GetGUIDLow(), type, score, ctx.mapId, ctx.x, ctx.y, ctx.z,
                       ctx.speed, ctx.latency, ctx.detail);
    }
}

void AntiCheatMgr::AlertGMs(Player* player, AntiCheatViolationType type, float score,
                            AntiCheatContext const& ctx)
{
    // Log to the server console/log. Broadcasting to online GMs in-game is a
    // planned enhancement (needs the GM-notify helper) — kept out for now so this
    // stays low-risk.
    sLog.outBasic("AntiCheat: ALERT guid=%u type=%u score=%.0f map=%u pos=(%.1f,%.1f,%.1f) speed=%.1f lat=%u %s",
                  player->GetGUIDLow(), type, score, ctx.mapId, ctx.x, ctx.y, ctx.z,
                  ctx.speed, ctx.latency, ctx.detail);
}

void AntiCheatMgr::Persist(Player* player, AntiCheatViolationType type, float score,
                           AntiCheatContext const& ctx)
{
    uint32 account = player->GetSession() ? player->GetSession()->GetAccountId() : 0;
    // detail is always a static string literal from the detectors — safe to embed.
    CharacterDatabase.PExecute(
        "INSERT INTO `character_anticheat_violation` "
        "(`guid`,`account`,`type`,`score`,`map`,`x`,`y`,`z`,`speed`,`latency`,`detail`) "
        "VALUES (%u,%u,%u,%u,%u,%f,%f,%f,%f,%u,'%s')",
        player->GetGUIDLow(), account, uint32(type), uint32(score), ctx.mapId,
        ctx.x, ctx.y, ctx.z, ctx.speed, ctx.latency, ctx.detail ? ctx.detail : "");
}

void AntiCheatMgr::Update(uint32 /*diff*/)
{
    if (!m_enabled)
        return;

    // Apply any queued autobans here (world thread): AccumulateKick runs on the
    // map thread, but BanAccount touches the session list, so it is deferred.
    std::vector<PendingBan> bans;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        if (!m_pendingBans.empty())
            bans.swap(m_pendingBans);
    }
    for (std::vector<PendingBan>::const_iterator it = bans.begin(); it != bans.end(); ++it)
    {
        BanReturn r = sWorld.BanAccount(BAN_CHARACTER, it->charName, it->durationSecs, it->reason, "AntiCheat");
        sLog.outError("AntiCheat: AUTOBAN account of '%s' for %us (%s) -> result %u",
                      it->charName.c_str(), it->durationSecs, it->reason.c_str(), uint32(r));
    }

    // Prune fully-decayed idle entries so the score map doesn't grow unbounded
    // for players who never offend again.
    uint32 nowMS = getMSTime();
    std::lock_guard<std::mutex> guard(m_lock);
    for (std::map<uint32, ScoreState>::iterator it = m_scores.begin(); it != m_scores.end();)
    {
        if (DecayedScore(it->second, nowMS) <= 0.0f)
            m_scores.erase(it++);
        else
            ++it;
    }
}

float AntiCheatMgr::DecayedKickScore(AccountState& s, uint32 nowSec) const
{
    if (s.lastUpdate && m_autobanDecayPerHour && nowSec > s.lastUpdate)
    {
        float hours = float(nowSec - s.lastUpdate) / 3600.0f;
        float decay = hours * float(m_autobanDecayPerHour);
        s.kickScore = s.kickScore > decay ? s.kickScore - decay : 0.0f;
    }
    s.lastUpdate = nowSec;
    return s.kickScore;
}

void AntiCheatMgr::AccumulateKick(Player* player)
{
    if (!player || !player->GetSession())
        return;

    uint32 accountId = player->GetSession()->GetAccountId();
    uint32 nowSec = uint32(sWorld.GetGameTime());
    std::string charName = player->GetName();

    bool queueBan = false;
    uint32 duration = 0;
    AccountState snapshot;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        AccountState& s = m_accounts[accountId];
        DecayedKickScore(s, nowSec);
        s.kickScore += float(m_autobanKickPoints);

        if (s.kickScore >= float(m_autobanThreshold))
        {
            // Escalating duration by prior ban count (last tier is sticky).
            uint32 tier = s.banCount < 3 ? s.banCount : 2;
            duration = m_autobanDur[tier];
            ++s.banCount;
            s.kickScore = 0.0f; // reset accumulator after a ban
            queueBan = true;
        }
        snapshot = s;

        if (queueBan)
        {
            PendingBan pb;
            pb.charName = charName;
            pb.durationSecs = duration;
            pb.reason = "Automated: repeated anti-cheat kicks";
            m_pendingBans.push_back(pb);
        }
    }

    PersistAccount(accountId, snapshot);

    if (queueBan)
        sLog.outError("AntiCheat: account %u queued for autoban (%us) after repeated kicks (char '%s')",
                      accountId, duration, charName.c_str());
}

void AntiCheatMgr::PersistAccount(uint32 accountId, AccountState const& s)
{
    // Account-level aggregate lives in the realm DB (spans characters/realms).
    LoginDatabase.PExecute(
        "REPLACE INTO `account_anticheat` (`account`,`kick_score`,`ban_count`,`last_update`) "
        "VALUES (%u, %f, %u, %u)",
        accountId, s.kickScore, s.banCount, s.lastUpdate);
}

void AntiCheatMgr::LoadAccounts()
{
    std::lock_guard<std::mutex> guard(m_lock);
    m_accounts.clear();
    QueryResult* result = LoginDatabase.Query(
        "SELECT `account`,`kick_score`,`ban_count`,`last_update` FROM `account_anticheat`");
    if (!result)
        return;
    do
    {
        Field* f = result->Fetch();
        AccountState s;
        s.kickScore = f[1].GetFloat();
        s.banCount  = f[2].GetUInt32();
        s.lastUpdate = f[3].GetUInt32();
        m_accounts[f[0].GetUInt32()] = s;
    }
    while (result->NextRow());
    delete result;
    sLog.outString("AntiCheat: loaded %u account autoban records.", uint32(m_accounts.size()));
}

void AntiCheatMgr::RemovePlayer(uint32 lowGuid)
{
    std::lock_guard<std::mutex> guard(m_lock);
    m_scores.erase(lowGuid);
}

void AntiCheatMgr::BuildStatus(Player* target, std::string& out)
{
    if (!target)
    {
        out = "AntiCheat: no target.";
        return;
    }

    uint32 nowMS = getMSTime();
    float score = 0.0f;
    uint32 violations = 0;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        std::map<uint32, ScoreState>::iterator it = m_scores.find(target->GetGUIDLow());
        if (it != m_scores.end())
        {
            score = DecayedScore(it->second, nowMS);
            violations = it->second.violations;
        }
    }

    char buf[256];
    snprintf(buf, sizeof(buf),
             "AntiCheat status for %s: score=%.0f, lifetime violations=%u, %s",
             target->GetName(), score, violations,
             m_enabled ? "framework ENABLED" : "framework disabled");
    out = buf;
}
