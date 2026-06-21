/*
 * GM chat commands for the Anti-Cheat framework:
 * .anticheat status/report/reload/warn/jail/unjail/delete.
 */

#include "Chat.h"
#include "AntiCheatMgr.h"
#include "MovementAnticheat.h"
#include "Player.h"
#include "World.h"
#include "ObjectMgr.h"
#include "Log.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"

#include <cstring>

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

bool ChatHandler::HandleAntiCheatReloadCommand(char* /*args*/)
{
    sAntiCheatMgr->LoadConfig();
    SendSysMessage("AntiCheat: configuration reloaded.");
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

// --- Time-sync / desync subsystem commands (get/set/inspect + actions) ---

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
