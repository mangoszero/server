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
#  include <openssl/provider.h>
#  include "Auth/OpenSSLProvider.h"
#endif

#include "Platform/Define.h"
#include <cstdio>
#include <cstring>
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


#ifdef _WIN32
#include "ServiceWin32.h"
#include "WheatyExceptionReport.h"

char serviceName[]        = "MaNGOS";               // service short name
char serviceLongName[]    = "MaNGOS World Service"; // service long name
char serviceDescription[] = "MaNGOS World Service - no description available";

int m_ServiceStatus = -1;

#else
#include "PosixDaemon.h"
#endif

DatabaseType WorldDatabase;                                 ///< Accessor to the world database
DatabaseType CharacterDatabase;                             ///< Accessor to the character database
DatabaseType LoginDatabase;                                 ///< Accessor to the realm/login database

uint32 realmID = 0;                                         ///< Id of the realm

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
    signal(SIGINT,  on_signal);
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
static void usage(const char* prog)
{
    sLog.outString("Usage: \n %s [<options>]\n"
        "    -v, --version              print version and exist\n\r"
        "    -c <config_file>           use config_file as configuration file\n\r"
        "    -a, --ahbot <config_file>  use config_file as ahbot configuration file\n\r"
#ifdef WIN32
        "    Running as service functions:\n\r"
        "    -s run                     run as service\n\r"
        "    -s install                 install service\n\r"
        "    -s uninstall               uninstall service\n\r"
#else
        "    Running as daemon functions:\n\r"
        "    -s run                     run as daemon\n\r"
        "    -s stop                    stop daemon\n\r"
#endif
    , prog);
}

/// Progress-bar console sink: forward a fully-built bar redraw to the off-thread
/// console writer (verbatim, no prefix/color/newline) so the bar shares one
/// serialized stdout with the log lines and cannot tear against them. Installed
/// once the writer thread is running; before that BarGoLink uses its default
/// synchronous sink.
static void MangosBarConsoleSink(char const* bytes, size_t len)
{
    sLog.ConsoleEmitRaw(std::string(bytes, len));
}

/// Progress-bar sink for the full-screen console: it draws its own bar, so it
/// wants the percentage, not a redraw. -1 means the bar finished.
static void MangosBarProgressSink(int percent)
{
    MaNGOS::Console::ConsoleUI::Instance().SetProgress(percent);
}

/// Launch the mangos server
int main(int argc, char** argv)
{
#ifdef _WIN32
      // Install the exception handler for unhandled exceptions in the main thread
        static WheatyExceptionReport exceptionReport;
        SetUnhandledExceptionFilter(WheatyExceptionReport::WheatyUnhandledExceptionFilter);
#endif

    ///- Command line parsing
    char const* cfg_file = MANGOSD_CONFIG_LOCATION;

    char serviceDaemonMode = '\0';

    // Walked by hand rather than with ACE_Get_Opt (gone with the rest of ACE) or
    // getopt (absent on MSVC). Four options do not justify a dependency.
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
            if (mode == "run")            { serviceDaemonMode = 'r'; }
#ifdef _WIN32
            else if (mode == "install")   { serviceDaemonMode = 'i'; }
            else if (mode == "uninstall") { serviceDaemonMode = 'u'; }
#else
            else if (mode == "stop")      { serviceDaemonMode = 's'; }
#endif
            else
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

#ifdef _WIN32                                                // windows service command need execute before config read
    switch (serviceDaemonMode)
    {
        case 'i':
            if (WinServiceInstall())
            {
                sLog.outString("Installing service");
            }
            return 1;
        case 'u':
            if (WinServiceUninstall())
            {
                sLog.outString("Uninstalling service");
            }
            return 1;
        case 'r':
            WinServiceRun();
            break;
    }
#endif
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

#ifndef _WIN32
    switch (serviceDaemonMode)
    {
        case 'r':
            startDaemon();
            break;
        case 's':
            stopDaemon();
            break;
    }
#endif

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
    if (sConfig.GetBoolDefault("Console.FullScreen", true) &&
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
/// @}
