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

#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#include <openssl/provider.h>
#include "Auth/OpenSSLProvider.h"
#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "GitRevision.h"
#include "ProgressBar.h"
#include "StartupUI.h"
#include "Log.h"
#include "SystemConfig.h"
#include "AuctionHouseBot.h"
#include "revision_data.h"
#include "World.h"
#include "Util.h"
#include "DBCStores.h"
#include "MassMailMgr.h"
#include "ScriptMgr.h"

#include "Master.h"

#ifdef _WIN32
#include <process.h>
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
        "    --console-demo [<style>]   draw a sample startup on this console and exit;\n\r"
        "                               style is auto (default), fancy or plain\n\r"
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
    std::string testMode;
    bool consoleDemo = false;               ///< --console-demo: draw a sample startup and exit
    std::string demoStyle = "auto";

    // Minimal command-line parser. Recognised: -c <file>, -a/--ahbot <file>,
    // -s <mode>, -t <mode>, -v/--version.
    for (int i = 1; i < argc; ++i)
    {
        char const* arg = argv[i];

        // Fetch the value for an option that requires an argument, or fail.
        auto value = [&](char const* name) -> char const*
        {
            if (i + 1 >= argc)
            {
                sLog.outError("Runtime-Error: %s option requires an input argument", name);
                usage(argv[0]);
                Log::WaitBeforeContinueIfNeed();
                return nullptr;
            }
            return argv[++i];
        };

        if (!strcmp(arg, "-v") || !strcmp(arg, "--version"))
        {
            printf("%s\n", GitRevision::GetProjectRevision());
            return 0;
        }
        else if (!strcmp(arg, "-c"))
        {
            char const* v = value("-c");
            if (!v) { return 1; }
            cfg_file = v;
        }
        else if (!strcmp(arg, "-a") || !strcmp(arg, "--ahbot"))
        {
            char const* v = value("--ahbot");
            if (!v) { return 1; }
            sAuctionBotConfig.SetConfigFileName(v);
        }
        else if (!strcmp(arg, "-t"))
        {
            char const* v = value("-t");
            if (!v) { return 1; }
            testMode = v;
        }
        else if (!strcmp(arg, "--console-demo"))
        {
            consoleDemo = true;
            // The style is optional: "--console-demo" alone means "auto".
            if (i + 1 < argc && argv[i + 1][0] != '-')
            {
                demoStyle = argv[++i];
            }
        }
        else if (!strcmp(arg, "-s"))
        {
            char const* mode = value("-s");
            if (!mode) { return 1; }

            if (!strcmp(mode, "run"))
            {
                serviceDaemonMode = 'r';
            }
#ifdef WIN32
            else if (!strcmp(mode, "install"))
            {
                serviceDaemonMode = 'i';
            }
            else if (!strcmp(mode, "uninstall"))
            {
                serviceDaemonMode = 'u';
            }
#else
            else if (!strcmp(mode, "stop"))
            {
                serviceDaemonMode = 's';
            }
#endif
            else
            {
                sLog.outError("Runtime-Error: -s unsupported argument %s", mode);
                usage(argv[0]);
                Log::WaitBeforeContinueIfNeed();
                return 1;
            }
        }
        else
        {
            sLog.outError("Runtime-Error: bad format of commandline arguments");
            usage(argv[0]);
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }
    }

    ///- Draw a sample startup and leave. Deliberately ahead of the config and the
    ///  databases: the point is to look at this console, and neither has to exist
    ///  for that. Runs on the same off-thread writer the real startup uses, so what
    ///  it shows is what a real startup shows.
    if (consoleDemo)
    {
        sLog.StartConsoleThread();
        StartupUI::Demo(demoStyle);
        sLog.StopConsoleThread();
        return 0;
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

    ///- Probe the console and install the startup UI hooks. Must precede the first
    ///  line drawn, and follows the config, which decides the style.
    StartupUI::Initialize(sConfig.GetStringDefault("Console.Style", "auto"));

    sLog.outString("%s [world-daemon]", GitRevision::GetProjectRevision());
    sLog.outString("%s", GitRevision::GetFullRevision());
    sLog.outString("%s", GitRevision::GetDepElunaFullRevisionStr());
    sLog.outString("%s", GitRevision::GetDepSD3FullRevisionStr());
    print_banner();
    sLog.outString("Using configuration file %s.", cfg_file);

    DETAIL_LOG("Using SSL version: %s (Library: %s)", OPENSSL_VERSION_TEXT, OpenSSL_version(OPENSSL_VERSION));

    // RAII provider management - automatically handles cleanup
    OpenSSLProviderManager providerManager;
    if (!providerManager.IsInitialized())
    {
        Log::WaitBeforeContinueIfNeed();
        return 0;
    }

    ///- Set progress bars show mode
    BarGoLink::SetOutputState(sConfig.GetBoolDefault("ShowProgressBars", true));

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

    ///- Catch termination signals
    hook_signals();

    ///- Bring the world up, run it, and take it back down. Returns once the world has
    ///  stopped and every auxiliary thread has been joined.
    Master master;
    const int code = master.Run(testMode);

    ///- Remove signal handling before leaving
    unhook_signals();

#ifdef WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    // Stop and join the off-thread console writer before the final shutdown lines:
    // later lines ("Bye!") then take the synchronous fallback. Master::Run() has already
    // joined EVERY console-producing thread -- the world/map-update workers, the CLI
    // reader and the SOAP listener -- so no concurrent producer can race the writer
    // delete. The remaining main-thread shutdown lines are drained by the still-running
    // writer before it joins. Precedes the final Flush.
    sLog.StopConsoleThread();

    sLog.outString("Bye!");

    // Final flush of the buffered file logs before exit. ~Log/CloseLogFiles also
    // flush via fclose, but this guarantees "Bye!" and any late shutdown lines
    // reach disk first.
    sLog.Flush();

    return code;
}
/// @}
