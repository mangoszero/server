/*
 * GM chat commands for the Anti-Cheat framework:
 * .anticheat status/report/reload/warn/jail/unjail/delete + GM test tooling
 * (.anticheat test/top/set/score/rubberband, .spoof, .timesync desync).
 */

#include "Chat.h"
#include "AntiCheatMgr.h"
#include "MovementAnticheat.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "Map.h"
#include "Log.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"

#include <cstring>
#include <cctype>
#include <cstdlib>
#include <vector>
#include <utility>

bool ChatHandler::HandleAntiCheatStatusCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage("AntiCheat: select a player (or run in-game).");
        SetSentErrorMessage(true);
        return false;
    }

    std::string out;
    sAntiCheatMgr->BuildStatus(target, out);
    SendSysMessage(out.c_str());
    return true;
}

bool ChatHandler::HandleAntiCheatReportCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage("AntiCheat: select a player (or run in-game).");
        SetSentErrorMessage(true);
        return false;
    }

    QueryResult* result = CharacterDatabase.PQuery(
        "SELECT `type`,`score`,`map`,`detail`,`time` FROM `character_anticheat_violation` "
        "WHERE `guid`=%u ORDER BY `id` DESC LIMIT 10", target->GetGUIDLow());

    if (!result)
    {
        PSendSysMessage("AntiCheat: no recorded violations for %s.", target->GetName());
        return true;
    }

    PSendSysMessage("AntiCheat: last violations for %s:", target->GetName());
    do
    {
        Field* f = result->Fetch();
        PSendSysMessage("  type=%u score=%u map=%u  %s  [%s]",
                        f[0].GetUInt32(), f[1].GetUInt32(), f[2].GetUInt32(),
                        f[3].GetCppString().c_str(), f[4].GetCppString().c_str());
    }
    while (result->NextRow());
    delete result;
    return true;
}

bool ChatHandler::HandleAntiCheatTopCommand(char* args)
{
    uint32 limit = 10;
    if (char* tok = strtok(args, " "))
    {
        int n = atoi(tok);
        if (n > 0) limit = uint32(n);
    }
    if (limit > 50) limit = 50;

    std::vector<std::pair<uint32, float> > top;
    sAntiCheatMgr->GetTopScores(limit, top);
    if (top.empty())
    {
        SendSysMessage("AntiCheat: no players currently carry a live violation score.");
        return true;
    }

    PSendSysMessage("AntiCheat: top %u by live score:", uint32(top.size()));
    for (size_t i = 0; i < top.size(); ++i)
    {
        Player* p = sObjectAccessor.FindPlayer(ObjectGuid(HIGHGUID_PLAYER, top[i].first));
        PSendSysMessage("  %2u. %s (guid %u): %.0f",
                        uint32(i + 1), p ? p->GetName() : "<offline>", top[i].first, top[i].second);
    }
    return true;
}

bool ChatHandler::HandleAntiCheatReloadCommand(char* /*args*/)
{
    sAntiCheatMgr->LoadConfig();
    SendSysMessage("AntiCheat: configuration reloaded.");
    return true;
}

bool ChatHandler::HandleAntiCheatSetCommand(char* args)
{
    char* f = strtok(args, " ");
    char* v = strtok(NULL, " ");
    if (!f || !v)
    {
        SendSysMessage(".anticheat set FAILED. Usage: .anticheat set <field> <value>. Fields:");
        SendSysMessage("  bool: enable, movement, physics, accelcheck, exemptbots, persist, autoban");
        SendSysMessage("  uint: action(1-4), warn, rubberband, kick, decay, speedtol, teledist,");
        SendSysMessage("        castburst, accelmult, exemptgm");
        SetSentErrorMessage(true);
        return false;
    }
    std::string field = f;
    for (size_t i = 0; i < field.size(); ++i) field[i] = (char)tolower(field[i]);
    uint32 val = uint32(atoi(v));

    if      (field == "enable")     sWorld.setConfig(CONFIG_BOOL_ANTICHEAT_ENABLE, val != 0);
    else if (field == "movement")   sWorld.setConfig(CONFIG_BOOL_ANTICHEAT_MOVEMENT, val != 0);
    else if (field == "physics")    sWorld.setConfig(CONFIG_BOOL_ANTICHEAT_PHYSICS, val != 0);
    else if (field == "accelcheck") sWorld.setConfig(CONFIG_BOOL_ANTICHEAT_ACCEL_CHECK, val != 0);
    else if (field == "exemptbots") sWorld.setConfig(CONFIG_BOOL_ANTICHEAT_EXEMPT_BOTS, val != 0);
    else if (field == "persist")    sWorld.setConfig(CONFIG_BOOL_ANTICHEAT_PERSIST, val != 0);
    else if (field == "autoban")    sWorld.setConfig(CONFIG_BOOL_AC_AUTOBAN_ENABLE, val != 0);
    else if (field == "action")     sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_ACTION, val);
    else if (field == "warn")       sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_SCORE_WARN, val);
    else if (field == "rubberband") sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_SCORE_RUBBER, val);
    else if (field == "kick")       sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_SCORE_KICK, val);
    else if (field == "decay")      sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_DECAY, val);
    else if (field == "speedtol")   sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_SPEED_TOL, val);
    else if (field == "teledist")   sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_TELE_DIST, val);
    else if (field == "castburst")  sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_CAST_BURST, val);
    else if (field == "accelmult")  sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_ACCEL_MULT, val);
    else if (field == "exemptgm")   sWorld.setConfig(CONFIG_UINT32_ANTICHEAT_EXEMPT_GM, val);
    else
    {
        PSendSysMessage(".anticheat set FAILED: unknown field '%s'.", field.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    // Refresh the manager's cached config snapshot so the change takes effect now.
    sAntiCheatMgr->LoadConfig();
    PSendSysMessage("AntiCheat: set %s = %u (runtime; reverts on restart/reload from file).",
                    field.c_str(), val);
    return true;
}

bool ChatHandler::HandleAntiCheatWarnCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(".anticheat warn FAILED: no player selected. Select/target an online "
                       "player, then run .anticheat warn (sends them a warning).");
        SetSentErrorMessage(true);
        return false;
    }
    if (target->GetSession())
        target->GetSession()->SendNotification("[AntiCheat] You have been warned by a GM for suspicious activity.");
    PSendSysMessage("AntiCheat: warned %s.", target->GetName());
    sLog.outBasic("AntiCheat: GM %s warned %s (guid %u)",
                  m_session ? m_session->GetPlayerName() : "CONSOLE", target->GetName(), target->GetGUIDLow());
    return true;
}

bool ChatHandler::HandleAntiCheatJailCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(".anticheat jail FAILED: no player selected. Select/target an online "
                       "player, then run .anticheat jail (teleports them to the configured jail).");
        SetSentErrorMessage(true);
        return false;
    }
    // Configurable jail location (defaults to the GM transport map cell).
    uint32 jmap = uint32(sConfig.GetIntDefault("AntiCheat.Jail.Map", 13));
    float jx = sConfig.GetFloatDefault("AntiCheat.Jail.X", -109.0f);
    float jy = sConfig.GetFloatDefault("AntiCheat.Jail.Y", -1.0f);
    float jz = sConfig.GetFloatDefault("AntiCheat.Jail.Z", -2.4f);
    float jo = sConfig.GetFloatDefault("AntiCheat.Jail.O", 3.14f);
    target->TeleportTo(jmap, jx, jy, jz, jo);
    if (target->GetSession())
        target->GetSession()->SendNotification("[AntiCheat] You have been jailed by a GM.");
    PSendSysMessage("AntiCheat: jailed %s (map %u: %.1f, %.1f, %.1f).", target->GetName(), jmap, jx, jy, jz);
    sLog.outBasic("AntiCheat: GM %s jailed %s (guid %u)",
                  m_session ? m_session->GetPlayerName() : "CONSOLE", target->GetName(), target->GetGUIDLow());
    return true;
}

bool ChatHandler::HandleAntiCheatUnjailCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
    {
        SendSysMessage(".anticheat unjail FAILED: no player selected. Select/target an online "
                       "player, then run .anticheat unjail (returns them to their homebind).");
        SetSentErrorMessage(true);
        return false;
    }
    target->TeleportToHomebind();
    if (target->GetSession())
        target->GetSession()->SendNotification("[AntiCheat] You have been released.");
    PSendSysMessage("AntiCheat: released %s to homebind.", target->GetName());
    return true;
}

bool ChatHandler::HandleAntiCheatDeleteCommand(char* args)
{
    uint32 guid = 0;
    std::string name;
    if (Player* target = getSelectedPlayer())
    {
        guid = target->GetGUIDLow();
        name = target->GetName();
    }
    else if (args && *args)
    {
        name = args;
        ObjectGuid og = sObjectMgr.GetPlayerGuidByName(name);
        if (og)
            guid = og.GetCounter();
    }

    if (!guid)
    {
        SendSysMessage(".anticheat delete FAILED: no player. Select/target a player OR pass a "
                       "character name (.anticheat delete <name>) to clear their violation records.");
        SetSentErrorMessage(true);
        return false;
    }

    CharacterDatabase.PExecute("DELETE FROM `character_anticheat_violation` WHERE `guid`=%u", guid);
    sAntiCheatMgr->RemovePlayer(guid);
    PSendSysMessage("AntiCheat: cleared violation records for %s (guid %u).", name.empty() ? "?" : name.c_str(), guid);
    return true;
}

// Names <-> violation types for the `.anticheat test` dev/debug command.
namespace
{
    struct AcTypeName { const char* name; AntiCheatViolationType type; };
    static const AcTypeName s_acTypeNames[] =
    {
        { "speed",        AC_VIOLATION_SPEED },
        { "teleport",     AC_VIOLATION_TELEPORT },
        { "vertical",     AC_VIOLATION_VERTICAL },
        { "flag",         AC_VIOLATION_FLAG_CONTRADICT },
        { "physics",      AC_VIOLATION_PHYSICS },
        { "desync",       AC_VIOLATION_DESYNC },
        { "jump",         AC_VIOLATION_JUMP },
        { "fall",         AC_VIOLATION_FALL },
        { "burst",        AC_VIOLATION_BURST },
        { "packettiming", AC_VIOLATION_PACKETTIMING },
        { "spell",        AC_VIOLATION_SPELL },
        { "item",         AC_VIOLATION_ITEM },
        { "interact",     AC_VIOLATION_INTERACT },
        { "bot",          AC_VIOLATION_BOT },
    };
}

bool ChatHandler::HandleAntiCheatTestCommand(char* args)
{
    char* tok = strtok(args, " ");
    if (!tok)
    {
        SendSysMessage(".anticheat test FAILED: needs a subcommand. Usage:");
        SendSysMessage("  .anticheat test list                 - list violation type names");
        SendSysMessage("  .anticheat test config               - dump live AntiCheat config");
        SendSysMessage("  .anticheat test <type> [weight]      - inject one violation (default 25)");
        SendSysMessage("  .anticheat test all [weight]         - inject every type (default 10)");
        SendSysMessage("Injects on your target (or yourself). Bypasses enabled/exempt so the full");
        SendSysMessage("pipeline runs: scoring, decay, DB persist, marker, warn/rubberband/kick/autoban.");
        SetSentErrorMessage(true);
        return false;
    }

    std::string sub = tok;
    for (size_t i = 0; i < sub.size(); ++i) sub[i] = (char)tolower(sub[i]);

    const uint32 count = uint32(sizeof(s_acTypeNames) / sizeof(s_acTypeNames[0]));

    if (sub == "list")
    {
        SendSysMessage("AntiCheat violation types:");
        for (uint32 i = 0; i < count; ++i)
            PSendSysMessage("  %u = %s", uint32(s_acTypeNames[i].type), s_acTypeNames[i].name);
        return true;
    }

    if (sub == "config")
    {
        std::string out;
        sAntiCheatMgr->BuildDiag(out);
        SendSysMessage(out.c_str());
        return true;
    }

    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".anticheat test FAILED: no player. Select/target a player or run in-game.");
        SetSentErrorMessage(true);
        return false;
    }

    AntiCheatContext ctx;
    ctx.mapId = target->GetMapId();
    ctx.x = target->GetPositionX(); ctx.y = target->GetPositionY(); ctx.z = target->GetPositionZ();
    ctx.latency = target->GetSession() ? target->GetSession()->GetLatencyEWMA() : 0;
    ctx.detail = "manual test injection";

    if (!sAntiCheatMgr->IsEnabled())
        SendSysMessage("AntiCheat note: framework is DISABLED in config — live detection is off, "
                       "but this test still drives scoring/punishment (markers need it enabled).");

    if (sub == "all")
    {
        char* w = strtok(NULL, " ");
        float weight = w ? float(atof(w)) : 10.0f;
        for (uint32 i = 0; i < count; ++i)
            sAntiCheatMgr->TestInject(target, s_acTypeNames[i].type, weight, ctx);
        PSendSysMessage("AntiCheat test: injected all %u types at weight %.0f on %s.",
                        count, weight, target->GetName());
    }
    else
    {
        AntiCheatViolationType type = AC_VIOLATION_NONE;
        int numeric = atoi(sub.c_str());
        for (uint32 i = 0; i < count; ++i)
            if (sub == s_acTypeNames[i].name || (numeric && numeric == int(s_acTypeNames[i].type)))
            {
                type = s_acTypeNames[i].type;
                break;
            }
        if (type == AC_VIOLATION_NONE)
        {
            PSendSysMessage(".anticheat test FAILED: unknown type '%s'. Use .anticheat test list.", sub.c_str());
            SetSentErrorMessage(true);
            return false;
        }
        char* w = strtok(NULL, " ");
        float weight = w ? float(atof(w)) : 25.0f;
        sAntiCheatMgr->TestInject(target, type, weight, ctx);
        PSendSysMessage("AntiCheat test: injected type %u weight %.0f on %s.",
                        uint32(type), weight, target->GetName());
    }

    std::string st;
    sAntiCheatMgr->BuildStatus(target, st);
    SendSysMessage(st.c_str());
    return true;
}

// Live-fire SPOOF simulator: activates the actual cheat signature through the real
// detectors. Doubles as GM tooling and an end-to-end anti-cheat self-test. The
// legitimate counterpart is `.legit` (applies the real effect; should NOT detect).
bool ChatHandler::HandleSpoofCommand(char* args)
{
    static const char* kinds[] = {
        "speed", "teleport", "fly", "waterwalk", "hover", "slowfall", "swim",
        "transport", "vertical", "jump", "desync", "noclip"
    };
    const uint32 kindCount = uint32(sizeof(kinds) / sizeof(kinds[0]));

    char* tok = strtok(args, " ");
    std::string kind = tok ? tok : "";
    for (size_t i = 0; i < kind.size(); ++i) kind[i] = (char)tolower(kind[i]);
    if (!tok || kind == "list" || kind == "help")
    {
        SendSysMessage("Spoof simulator (live anti-cheat test + GM tooling). Usage:");
        SendSysMessage("  .spoof <kind>|all [magnitude] - run a cheat signature (or all) through detectors");
        std::string line = "  kinds: ";
        for (uint32 i = 0; i < kindCount; ++i) { line += kinds[i]; if (i + 1 < kindCount) line += ", "; }
        SendSysMessage(line.c_str());
        SendSysMessage("Runs on your target (or you). Bypasses GM-exempt/disabled so the result shows.");
        SendSysMessage("Legit versions use the real commands (.fly, .waterwalk, .modify speed) / spell");
        SendSysMessage("auras - those record a server grant so the AC does NOT flag them.");
        SendSysMessage("Fleet test: .spoof bots <kind>|all [n] - make nearby PLAYERBOTS run the cheats.");
        if (!tok) { SetSentErrorMessage(true); return false; }
        return true;
    }

    // Fleet red-team: drive PlayerBots (normally exempt + packetless) through real
    // cheat signatures so the anti-cheat catches them - a live, watchable self-test.
    if (kind == "bots")
    {
        char* subTok = strtok(NULL, " ");
        std::string sub = subTok ? subTok : "all";
        for (size_t i = 0; i < sub.size(); ++i) sub[i] = (char)tolower(sub[i]);
        char* cTok = strtok(NULL, " ");
        uint32 maxN = cTok ? uint32(atoi(cTok)) : 10;
        if (maxN < 1) maxN = 1;
        if (maxN > 200) maxN = 200;

        // Iterate ALL online players (works from in-game, the server console, and
        // SOAP — all of which run the command on the world thread). Drives the
        // PlayerBots through real cheat signatures so the AC scores them.
        uint32 done = 0;
        std::string desc;
        sAntiCheatMgr->SetTestBypass(true);
        sObjectAccessor.DoForAllPlayers([&](Player* b)
        {
            if (done >= maxN || !b || !b->IsInWorld())
                return;
#ifdef ENABLE_PLAYERBOTS
            if (!b->GetPlayerbotAI())
                return;
#else
            return;
#endif
            if (sub == "all")
                for (uint32 k = 0; k < kindCount; ++k)
                    b->GetMovementAnticheat()->SimulateCheat(kinds[k], 0.0f, desc);
            else
                b->GetMovementAnticheat()->SimulateCheat(sub, 0.0f, desc);
            ++done;
        });
        sAntiCheatMgr->SetTestBypass(false);

        if (!done)
        {
            SendSysMessage(".spoof bots: no PlayerBots online (or playerbots not built in).");
            return true;
        }
        PSendSysMessage("Spoof: ran '%s' on %u PlayerBot(s). Use .anticheat top to see them ranked.",
                        sub.c_str(), done);
        return true;
    }

    char* w = strtok(NULL, " ");
    float mag = w ? float(atof(w)) : 0.0f;

    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".spoof FAILED: no player. Select/target a player or run in-game.");
        SetSentErrorMessage(true);
        return false;
    }

    // Drive the real detectors and force the result to apply (target may be an
    // exempt GM, and AC may be off). Bypass is set only around this synchronous call.
    std::string desc;
    if (kind == "all")
    {
        sAntiCheatMgr->SetTestBypass(true);
        for (uint32 i = 0; i < kindCount; ++i)
            target->GetMovementAnticheat()->SimulateCheat(kinds[i], mag, desc);
        sAntiCheatMgr->SetTestBypass(false);
        PSendSysMessage("Spoof: ran all %u cheat signatures on %s. Detector result below:",
                        kindCount, target->GetName());
        std::string allst;
        sAntiCheatMgr->BuildStatus(target, allst);
        SendSysMessage(allst.c_str());
        return true;
    }

    sAntiCheatMgr->SetTestBypass(true);
    bool ok = target->GetMovementAnticheat()->SimulateCheat(kind, mag, desc);
    sAntiCheatMgr->SetTestBypass(false);

    if (!ok)
    {
        PSendSysMessage(".spoof FAILED: %s. Try .spoof list.", desc.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage("Spoof simulated on %s: %s. Detector result below (also logged/persisted):",
                    target->GetName(), desc.c_str());
    std::string status;
    sAntiCheatMgr->BuildStatus(target, status);
    SendSysMessage(status.c_str());
    return true;
}

// --- Dedicated AC-vector GM tools (manipulate the mechanics directly) ---

bool ChatHandler::HandleAntiCheatRubberbandCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".anticheat rubberband FAILED: no player. Select/target a player or run "
                       "in-game. It yanks them back to their last AC-validated position.");
        SetSentErrorMessage(true);
        return false;
    }

    MovementAnticheat* mac = target->GetMovementAnticheat();
    if (mac && mac->HasValid())
    {
        target->NearTeleportTo(mac->ValidX(), mac->ValidY(), mac->ValidZ(), mac->ValidO());
        PSendSysMessage("AntiCheat: rubberbanded %s to last valid (%.1f, %.1f, %.1f).",
                        target->GetName(), mac->ValidX(), mac->ValidY(), mac->ValidZ());
    }
    else
    {
        SendSysMessage("AntiCheat: no validated position yet for that player (they haven't moved "
                       "since login). Nothing to rubberband to.");
    }
    return true;
}

// --- Time-sync / desync system commands (get/set/inspect + actions) ---

bool ChatHandler::HandleTimeSyncStatusCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".timesync status FAILED: no player. Select/target a player or run in-game.");
        SetSentErrorMessage(true);
        return false;
    }

    uint32 latency = target->GetSession() ? target->GetSession()->GetLatencyEWMA() : 0;
    MovementAnticheat* mac = target->GetMovementAnticheat();
    PSendSysMessage("TimeSync %s: latency(EWMA)=%ums  clockOffset=%s%lldms  desyncStreak=%u",
                    target->GetName(), latency,
                    (mac && mac->HasClockOffset()) ? "" : "(none) ",
                    (mac && mac->HasClockOffset()) ? (long long)mac->GetClockOffsetMs() : 0LL,
                    mac ? mac->GetDesyncStreak() : 0);
    return true;
}

bool ChatHandler::HandleTimeSyncConfigCommand(char* /*args*/)
{
    PSendSysMessage("TimeSync config: enable=%u alpha=%u desyncThreshold=%ums maxSkip=%ums",
                    (uint32)sWorld.getConfig(CONFIG_BOOL_TIMESYNC_ENABLE),
                    sWorld.getConfig(CONFIG_UINT32_TIMESYNC_ALPHA),
                    sWorld.getConfig(CONFIG_UINT32_TIMESYNC_DESYNC),
                    sWorld.getConfig(CONFIG_UINT32_TIMESYNC_MAX_SKIP));
    PSendSysMessage("  moveCorrection=%u autoResync=%u resyncTrips=%u resyncCooldown=%ums",
                    (uint32)sWorld.getConfig(CONFIG_BOOL_TIMESYNC_MOVE_CORRECTION),
                    (uint32)sWorld.getConfig(CONFIG_BOOL_TIMESYNC_AUTORESYNC),
                    sWorld.getConfig(CONFIG_UINT32_TIMESYNC_RESYNC_TRIPS),
                    sWorld.getConfig(CONFIG_UINT32_TIMESYNC_RESYNC_COOLDOWN));
    SendSysMessage("Set at runtime with .timesync set <field> <value> (reverts on restart/reload).");
    return true;
}

bool ChatHandler::HandleTimeSyncSetCommand(char* args)
{
    char* f = strtok(args, " ");
    char* v = strtok(NULL, " ");
    if (!f || !v)
    {
        SendSysMessage(".timesync set FAILED. Usage: .timesync set <field> <value>. Fields:");
        SendSysMessage("  bool:  enable, movecorrection, autoresync");
        SendSysMessage("  uint:  alpha, desyncthreshold, maxskip, resynctrips, resynccooldown");
        SetSentErrorMessage(true);
        return false;
    }
    std::string field = f;
    for (size_t i = 0; i < field.size(); ++i) field[i] = (char)tolower(field[i]);
    uint32 val = uint32(atoi(v));

    if      (field == "enable")         sWorld.setConfig(CONFIG_BOOL_TIMESYNC_ENABLE, val != 0);
    else if (field == "movecorrection") sWorld.setConfig(CONFIG_BOOL_TIMESYNC_MOVE_CORRECTION, val != 0);
    else if (field == "autoresync")     sWorld.setConfig(CONFIG_BOOL_TIMESYNC_AUTORESYNC, val != 0);
    else if (field == "alpha")          sWorld.setConfig(CONFIG_UINT32_TIMESYNC_ALPHA, val);
    else if (field == "desyncthreshold")sWorld.setConfig(CONFIG_UINT32_TIMESYNC_DESYNC, val);
    else if (field == "maxskip")        sWorld.setConfig(CONFIG_UINT32_TIMESYNC_MAX_SKIP, val);
    else if (field == "resynctrips")    sWorld.setConfig(CONFIG_UINT32_TIMESYNC_RESYNC_TRIPS, val);
    else if (field == "resynccooldown") sWorld.setConfig(CONFIG_UINT32_TIMESYNC_RESYNC_COOLDOWN, val);
    else
    {
        PSendSysMessage(".timesync set FAILED: unknown field '%s'.", field.c_str());
        SetSentErrorMessage(true);
        return false;
    }
    PSendSysMessage("TimeSync: set %s = %u (runtime; reverts on restart/reload).", field.c_str(), val);
    return true;
}

bool ChatHandler::HandleTimeSyncDesyncCommand(char* args)
{
    char* tok = strtok(args, " ");
    uint32 n = tok ? uint32(atoi(tok)) : 5;
    if (n < 1) n = 1;
    if (n > 50) n = 50;

    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".timesync desync FAILED: no player. Select/target a player or run in-game. "
                       "Usage: .timesync desync [count]. Fires the desync detector then resyncs.");
        SetSentErrorMessage(true);
        return false;
    }

    // Fire the real desync detector n times (visible: score climbs, markers, log)...
    std::string desc;
    sAntiCheatMgr->SetTestBypass(true);
    for (uint32 i = 0; i < n; ++i)
        target->GetMovementAnticheat()->SimulateCheat("desync", 0.0f, desc);
    sAntiCheatMgr->SetTestBypass(false);

    // ...then show the resync correction in action (the rubberband).
    target->NearTeleportTo(target->GetPositionX(), target->GetPositionY(),
                           target->GetPositionZ(), target->GetOrientation());
    if (MovementAnticheat* mac = target->GetMovementAnticheat())
        mac->NotifyServerRelocation();

    PSendSysMessage("TimeSync: simulated %u desync events on %s and performed a resync. "
                    "(For fully-automatic resync, enable TimeSync.AutoResync and trip a non-GM char.)",
                    n, target->GetName());
    std::string st;
    sAntiCheatMgr->BuildStatus(target, st);
    SendSysMessage(st.c_str());
    return true;
}

bool ChatHandler::HandleTimeSyncResyncCommand(char* /*args*/)
{
    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".timesync resync FAILED: no player. Select/target a player or run in-game. "
                       "It rubberbands them to their current authoritative position (clock resync).");
        SetSentErrorMessage(true);
        return false;
    }

    target->NearTeleportTo(target->GetPositionX(), target->GetPositionY(),
                           target->GetPositionZ(), target->GetOrientation());
    if (MovementAnticheat* mac = target->GetMovementAnticheat())
        mac->NotifyServerRelocation();
    PSendSysMessage("TimeSync: resynced %s to current position.", target->GetName());
    return true;
}

bool ChatHandler::HandleTimeSyncSkipCommand(char* args)
{
    char* tok = strtok(args, " ");
    if (!tok)
    {
        SendSysMessage(".timesync skip FAILED: needs milliseconds. Usage: .timesync skip <ms>. "
                       "Feeds a synthetic client time-skip to the target (drives the time-sync "
                       "service; large values score as a time hack).");
        SetSentErrorMessage(true);
        return false;
    }
    uint32 ms = uint32(atoi(tok));

    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".timesync skip FAILED: no player. Select/target a player or run in-game.");
        SetSentErrorMessage(true);
        return false;
    }

    if (MovementAnticheat* mac = target->GetMovementAnticheat())
        mac->NotifyClientTimeSkip(ms);
    PSendSysMessage("TimeSync: fed a %u ms time-skip to %s.", ms, target->GetName());
    return true;
}

bool ChatHandler::HandleAntiCheatScoreCommand(char* args)
{
    Player* target = getSelectedPlayer();
    if (!target)
        target = m_session ? m_session->GetPlayer() : NULL;
    if (!target)
    {
        SendSysMessage(".anticheat score FAILED: no player. Select/target a player or run in-game. "
                       "Usage: .anticheat score [value] (no value = show current).");
        SetSentErrorMessage(true);
        return false;
    }

    char* tok = strtok(args, " ");
    if (tok)
    {
        float val = float(atof(tok));
        sAntiCheatMgr->SetScore(target, val);
        PSendSysMessage("AntiCheat: set %s score to %.0f (escalation evaluated).", target->GetName(), val);
    }

    std::string st;
    sAntiCheatMgr->BuildStatus(target, st);
    SendSysMessage(st.c_str());
    return true;
}
