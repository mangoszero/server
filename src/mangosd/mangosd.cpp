/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file mangosd.cpp
 * @brief World server daemon entry point
 *
 * This file implements the main entry point for the MaNGOS world server
 * daemon (mangosd). It handles:
 * - Command line argument parsing
 * - Service/daemon mode initialization
 * - Database connections (World, Character, Login)
 * - Server subsystem initialization
 * - Multiple thread management (World, CLI, Auto-freeze, SOAP)
 * - Main event loop and shutdown
 *
 * The world server is responsible for running the game simulation,
 * handling player connections, and managing game state.
 *
 * @addtogroup mangosd Mangos Daemon
 * @{
 */

#include <csignal>
#include "Common/ServerDefines.h"
#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#include <openssl/provider.h>
#include "Auth/OpenSSLProvider.h"
#endif

#include "Platform/Define.h"
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "GitRevision.h"
#include "ProgressBar.h"
#include "Console/ConsoleUI.h"
#include "Log.h"
#include "SystemConfig.h"
#include "AuctionHouseBot.h"
#include "Master.h"
#include "World.h"
#include "Util.h"
#include "DBCStores.h"
#include "MassMailMgr.h"
#include "ScriptMgr.h"
#include "Process/Process.h"

#ifdef _WIN32
#include "WheatyExceptionReport.h"
#endif

DatabaseType WorldDatabase;     ///< Accessor to the world database
DatabaseType CharacterDatabase; ///< Accessor to the character database
DatabaseType LoginDatabase;     ///< Accessor to the realm/login database

uint32 realmID = 0; ///< Id of the realm

/**
 * @brief Clear online status for realm accounts on startup
 *
 * Resets the 'online' status for all accounts that were marked as
 * connected to this realm. This handles cases where the server
 * crashed without properly logging out all players.
 *
 * Also resets character online status and battleground instance data.
 */

/**
 * @brief Initialize database connections
 * @return true if all databases connected successfully, false otherwise
 *
 * Connects to three databases:
 * - World Database: Contains game data (creatures, items, quests, etc.)
 * - Character Database: Contains player character data
 * - Login Database: References realm authentication data
 *
 * Validates database versions and connection counts from configuration.
 * On failure, properly cleans up any connections that were established.
 */

/// Handle termination signals
static void on_signal(int s)
{
    switch (s)
    {
    case SIGINT:
        World::StopNow(RESTART_EXIT_CODE);
        break;
    case SIGTERM:
#ifdef _WIN32
    case SIGBREAK:
#endif
        World::StopNow(SHUTDOWN_EXIT_CODE);
        break;
    }

    signal(s, on_signal);
}

/// Define hook for all termination signals
static void hook_signals()
{
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
#ifdef _WIN32
    signal(SIGBREAK, on_signal);
#endif
}

/// Unhook the signals before leaving
static void unhook_signals()
{
    signal(SIGINT, 0);
    signal(SIGTERM, 0);
#ifdef _WIN32
    signal(SIGBREAK, 0);
#endif
}

/// Print out the usage string for this program on the console.
static void usage(const char *prog)
{
    sLog.outString("Usage: \n %s [<options>]\n"
                   "    -v, --version              print version and exit\n\r"
                   "    -c <config_file>           use config_file as configuration file\n\r"
                   "    -a, --ahbot <config_file>  use config_file as ahbot configuration file\n\r"
                   "    Running in the background:\n\r"
                   "    -s run                     run in the background\n\r"
                   "    -s stop                    stop the background instance\n\r",
                   prog);

    // Only where there is a service manager to register with. On POSIX the
    // functions exist and refuse, because writing systemd units is not the
    // server's job -- so they are not offered here either.
    if (Process::HasServiceManager())
    {
        sLog.outString(
            "    -s install                 register as a system service\n\r"
            "    -s uninstall               deregister the system service\n\r");
    }
}

/// Progress-bar console sink: forward a fully-built bar redraw to the off-thread
/// console writer (verbatim, no prefix/color/newline) so the bar shares one
/// serialized stdout with the log lines and cannot tear against them. Installed
/// once the writer thread is running; before that BarGoLink uses its default
/// synchronous sink.
static void MangosBarConsoleSink(char const *bytes, size_t len)
{
    sLog.ConsoleEmitRaw(std::string(bytes, len));
}

/// Progress-bar sink for the full-screen console: it draws its own bar, so it
/// wants the percentage, not a redraw. -1 means the bar finished.
static void MangosBarProgressSink(int percent)
{
    MaNGOS::Console::ConsoleUI::Instance().SetProgress(percent);
}

/**
 * @brief The server itself: everything from the banner to the last flush.
 *
 * ===== THIS IS THE PARAMETER =====
 *
 * Handed to Process::RunInBackground() for `-s run`, and called directly in the
 * foreground. One loop, and the platform decides how it is entered.
 * =================================
 *
 * The configuration is already loaded when this runs -- the pid file it names is
 * what the POSIX fork and `-s stop` need before the server exists.
 *
 * @param cfg_file the configuration file that was loaded, for the log line.
 * @return the process exit code.
 */
static int Serve(char const *cfg_file)
{
    sLog.outString("%s [world-daemon]", GitRevision::GetProjectRevision());
    sLog.outString("%s", GitRevision::GetFullRevision());
    sLog.outString("%s", GitRevision::GetDepElunaFullRevisionStr());
    sLog.outString("%s", GitRevision::GetDepSD3FullRevisionStr());
    print_banner();
    sLog.outString("Using configuration file %s.", cfg_file);

    DETAIL_LOG("Using SSL version: %s (Library: %s)", OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
    if (!OpenSSLProviderManager::Instance().IsInitialized())
    {
        Log::WaitBeforeContinueIfNeed();
        return 1;
    }
#else
    if (SSLeay() < 0x10100000L || SSLeay() > 0x10200000L)
    {
        DETAIL_LOG("WARNING: OpenSSL version may be out of date or unsupported. Logins to server may not work!");
        DETAIL_LOG("WARNING: Minimal required version [OpenSSL 1.1.x] and Maximum supported version [OpenSSL 1.2]");
    }
#endif

    ///- Set progress bars show mode
    BarGoLink::SetOutputState(sConfig.GetBoolDefault("ShowProgressBars", true));

    // Serialise the bar through the console writer instead of letting it write
    // straight to stdout from the loading thread: two owners of stdout is what
    // shreds the start-up log, with bar redraws landing inside log lines.
    BarGoLink::SetConsoleSink(&MangosBarConsoleSink);

    /// worldd PID file creation
    std::string pidfile = sConfig.GetStringDefault("PidFile", "");
    if (!pidfile.empty())
    {
        uint32 pid = CreatePIDFile(pidfile);
        if (!pid)
        {
            sLog.outError("Can not create PID file %s.\n", pidfile.c_str());
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }

        sLog.outString("Daemon PID: %u\n", pid);
    }

    // Move the console emit off the world/map-update threads. Started before the
    // world loads (the LivingWorld spawn burst) so the hot console path is
    // covered, and after the fallible init above so an early return never leaves
    // a writer thread running into stdio teardown.
    sLog.StartConsoleThread();

    // Only now: until the writer thread exists, console emits take the
    // synchronous path and would write straight over the full-screen frame.
    // "plain" never draws the loading UI. "auto" and "fancy" both ask for it,
    // and Start() declines on its own when stdout is not a real terminal.
    //
    // Not in the background: a service has no terminal at all, and the daemon's
    // stdout is /dev/null. Start() would decline anyway, but asking it to draw a
    // full-screen frame into a null device is not a question worth putting.
    const std::string consoleStyle =
        sConfig.GetStringDefault("Console.Style", "auto");
    if (consoleStyle != "plain" && !Process::IsRunningInBackground() &&
        MaNGOS::Console::ConsoleUI::Instance().Start("MaNGOS Zero", "Vanilla 1.12.x"))
    {
        MaNGOS::Console::ConsoleUI::Instance().SetHeaderRight(
            std::string("realm ") + std::to_string(realmID));
        MaNGOS::Console::ConsoleUI::Instance().SetHint(
            "PgUp/PgDn scroll \xC2\xB7 Ctrl+L redraw");
        MaNGOS::Console::ConsoleUI::Instance().SetKeyEcho(
            sConfig.GetBoolDefault("Console.DebugKeys", false));
        MaNGOS::Console::ConsoleUI::Instance().SetScrollback(
            uint32(sConfig.GetIntDefault("Console.Scrollback", 20000)));
        BarGoLink::SetProgressSink(&MangosBarProgressSink);
    }

    ///- Catch termination signals
    hook_signals();

    // Databases, world, listener and background services all live in Master. It
    // runs the world loop on this thread and returns once the world has stopped
    // and every service has been joined.
    Master master;
    const int runCode = master.Run();

    ///- Remove signal handling before leaving
    unhook_signals();

    ///- Set server offline in realmlist
    LoginDatabase.DirectPExecute(
        "UPDATE `realmlist` SET `realmflags` = `realmflags` | %u WHERE `id` = '%u'",
        REALM_FLAG_OFFLINE, realmID);

    // Master has already kicked the players, stopped every service, cleared the
    // online flags and halted the database delay threads. What is left here is
    // process-level teardown only.

    // Unload the script library explicitly: ~ScriptMgr() runs too late, at static
    // destruction, to unload the shared object safely.
    sLog.outString("[shutdown] unloading script library...");
    sScriptMgr.UnloadScriptLibrary();
    sLog.outString("[shutdown] script library unloaded");

#ifdef _WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    // Stop and join the off-thread console writer last. Every thread that can
    // emit to the console -- the map-update workers, the network workers and all
    // background services -- has been joined by Master::Run() before this point,
    // so nothing can race the writer's deletion. The remaining main-thread lines
    // drain through it before it joins; "Bye!" then takes the synchronous path.
    sLog.StopConsoleThread();

    // After the writer is joined, so no repaint can race the terminal restore.
    MaNGOS::Console::ConsoleUI::Instance().Stop();

    sLog.outString("Bye!");

    // Final flush of the buffered file logs before exit. ~Log/CloseLogFiles also
    // flush via fclose, but this guarantees "Bye!" and any late shutdown lines
    // reach disk first.
    sLog.Flush();

    return runCode;
}

/// Launch the mangos server
int main(int argc, char **argv)
{
#ifdef _WIN32
    // Install the exception handler for unhandled exceptions in the main thread
    static WheatyExceptionReport exceptionReport;
    SetUnhandledExceptionFilter(WheatyExceptionReport::WheatyUnhandledExceptionFilter);
#endif

    ///- Command line parsing
    char const *cfg_file = MANGOSD_CONFIG_LOCATION;

    Process::ServiceAction action = Process::ServiceAction::None;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        const bool hasValue = (i + 1) < argc;

        if (arg == "-v" || arg == "--version")
        {
            printf("%s\n", GitRevision::GetProjectRevision());
            return 0;
        }
        else if ((arg == "-c") && hasValue)
        {
            cfg_file = argv[++i];
        }
        else if ((arg == "-a" || arg == "--ahbot") && hasValue)
        {
            sAuctionBotConfig.SetConfigFileName(argv[++i]);
        }
        else if (arg == "-s" && hasValue)
        {
            const std::string mode = argv[++i];
            action = Process::ParseServiceAction(mode);

            // Install and uninstall are accepted only where there is a service
            // manager behind them. Elsewhere they would reach a function whose
            // whole job is to refuse, which is a worse error message than this
            // one.
            const bool needsManager =
                action == Process::ServiceAction::Install ||
                action == Process::ServiceAction::Uninstall;

            if (action == Process::ServiceAction::None ||
                (needsManager && !Process::HasServiceManager()))
            {
                sLog.outError("Runtime-Error: -s unsupported argument %s", mode.c_str());
                usage(argv[0]);
                Log::WaitBeforeContinueIfNeed();
                return 1;
            }
        }
        else
        {
            sLog.outError("Runtime-Error: unsupported option %s", arg.c_str());
            usage(argv[0]);
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }
    }

    Process::Options processOptions;
    processOptions.serviceName = "MaNGOS";
    processOptions.serviceDisplayName = "MaNGOS World Service";
    processOptions.serviceDescription =
        "MaNGOS World Service - serves a World of Warcraft 1.12.x realm.";

    // Registering with the service manager touches neither the configuration nor
    // the databases, so it runs before the config file is looked for -- which is
    // also what lets the service be installed on a host that is not configured
    // yet. Zero on success, so `mangosd -s install && net start MaNGOS` works.
    switch (action)
    {
    case Process::ServiceAction::Install:
        return Process::Install(processOptions) ? 0 : 1;

    case Process::ServiceAction::Uninstall:
        return Process::Uninstall(processOptions) ? 0 : 1;

    case Process::ServiceAction::Stop:
        // Where there is a service manager, stopping is its business and needs
        // nothing but the service name -- so it must not be made to depend on
        // finding a configuration file. `mangosd -s stop` run from any other
        // directory would otherwise fail on the config lookup below and never
        // reach the stop it was asked for.
        //
        // Without one, the pid file is the only handle on the running instance
        // and that comes from the configuration, so the POSIX stop falls through
        // and is dispatched after it is loaded.
        if (Process::HasServiceManager())
        {
            return Process::Stop(processOptions) ? 0 : 1;
        }
        break;

    default:
        break;
    }

    // Before the configuration is looked for, not after. A Windows service is
    // started from system32, so the fallback below -- the config file beside the
    // binary -- would be searched for in the wrong directory and the service
    // would fail to start before RunInBackground ever got the chance to correct
    // it. A no-op everywhere else.
    if (action == Process::ServiceAction::Run && !Process::UseExecutableDirectory())
    {
        Log::WaitBeforeContinueIfNeed();
        return 1;
    }

    if (!sConfig.SetSource(cfg_file))
    {
        // Try current folder as fallback if SYSCONFDIR path fails
        if (!sConfig.SetSource(MANGOSD_CONFIG_NAME))
        {
            sLog.outError("Could not find configuration file %s.", cfg_file);
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }
        cfg_file = MANGOSD_CONFIG_NAME;
    }

    // Read here rather than inside Serve(): on POSIX the forked child writes this
    // file before its parent is allowed to exit, so that `-s run` followed
    // immediately by `-s stop` finds a pid to signal. Serve() writes it again
    // with the same value, which also covers the foreground case.
    processOptions.pidFile = sConfig.GetStringDefault("PidFile", "");

    if (action == Process::ServiceAction::Stop)
    {
        return Process::Stop(processOptions) ? 0 : 1;
    }

    // The one code path. Foreground calls it here; the background hands it to the
    // platform, which calls it from the service thread or the forked child.
    const std::function<int()> serve = [cfg_file]()
    { return Serve(cfg_file); };

    if (action == Process::ServiceAction::Run)
    {
        return Process::RunInBackground(processOptions, serve);
    }

    return serve();
}
/// @}
