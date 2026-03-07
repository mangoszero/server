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

/// \addtogroup playerbotd MaNGOS Playerbot Daemon
/// @{
/// \file

#include <openssl/opensslv.h>
#include <openssl/crypto.h>
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
#  include <openssl/provider.h>
#endif
#include <ace/Version.h>
#include <ace/Get_Opt.h>

#include "Common.h"
#include "Database/DatabaseEnv.h"
#include "Config/Config.h"
#include "GitRevision.h"
#include "ProgressBar.h"
#include "Log.h"
#include "SystemConfig.h"
#include "AuctionHouseBot.h"
#include "revision_data.h"
#include "World.h"
#include "Util.h"
#include "DBCStores.h"
#include "MassMailMgr.h"
#include "ScriptMgr.h"

#include "WorldThread.h"
#include "CliThread.h"
#include "AFThread.h"
#include "RAThread.h"

#ifdef _WIN32
 #include "ServiceWin32.h"

  char serviceName[]        = "MaNGOSBots";                       // service short name
  char serviceLongName[]    = "MaNGOS Playerbot Service";         // service long name
  char serviceDescription[] = "MaNGOS Playerbot Service - no description available";

  int m_ServiceStatus = -1;

#else
 #include "PosixDaemon.h"
#endif

//*******************************************************************************************************//
DatabaseType WorldDatabase;                                 ///< Accessor to the world database
DatabaseType CharacterDatabase;                             ///< Accessor to the character database
DatabaseType LoginDatabase;                                 ///< Accessor to the realm/login database

uint32 realmID = 0;                                         ///< Id of the realm
//*******************************************************************************************************//


/// Clear 'online' status for all accounts with characters in this realm
static void clear_online_accounts()
{
    LoginDatabase.PExecute("UPDATE `account` SET `active_realm_id` = 0, `os` = ''  WHERE `active_realm_id` = '%u'", realmID);
    CharacterDatabase.Execute("UPDATE `characters` SET `online` = 0 WHERE `online`<>0");
    CharacterDatabase.Execute("UPDATE `character_battleground_data` SET `instance_id` = 0");
}


/// Initialize connection to the databases
static bool start_db()
{
    ///- Get world database info from configuration file
    std::string dbstring = sConfig.GetStringDefault("WorldDatabaseInfo", "");
    int nConnections = sConfig.GetIntDefault("WorldDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("Database not specified in configuration file");
        return false;
    }
    sLog.outString("World Database total connections: %i", nConnections + 1);

    ///- Initialise the world database
    if (!WorldDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Can not connect to world database %s", dbstring.c_str());
        return false;
    }

    ///- Check the World database version
    if (!WorldDatabase.CheckDatabaseVersion(DATABASE_WORLD))
    {
        WorldDatabase.HaltDelayThread();
        return false;
    }

    dbstring = sConfig.GetStringDefault("CharacterDatabaseInfo", "");
    nConnections = sConfig.GetIntDefault("CharacterDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("Character Database not specified in configuration file");
        WorldDatabase.HaltDelayThread();
        return false;
    }
    sLog.outString("Character Database total connections: %i", nConnections + 1);

    ///- Initialise the Character database
    if (!CharacterDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Can not connect to Character database %s", dbstring.c_str());
        WorldDatabase.HaltDelayThread();
        return false;
    }

    ///- Check the Character database version
    if (!CharacterDatabase.CheckDatabaseVersion(DATABASE_CHARACTER))
    {
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Get login database info from configuration file
    dbstring = sConfig.GetStringDefault("LoginDatabaseInfo", "");
    nConnections = sConfig.GetIntDefault("LoginDatabaseConnections", 1);
    if (dbstring.empty())
    {
        sLog.outError("Login database not specified in configuration file");
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Initialise the login database
    sLog.outString("Login Database total connections: %i", nConnections + 1);
    if (!LoginDatabase.Initialize(dbstring.c_str(), nConnections))
    {
        sLog.outError("Can not connect to login database %s", dbstring.c_str());
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        return false;
    }

    ///- Check the Realm database version
    if (!LoginDatabase.CheckDatabaseVersion(DATABASE_REALMD))
    {
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    sLog.outString();

    ///- Get the realm Id from the configuration file
    realmID = sConfig.GetIntDefault("RealmID", 0);
    if (!realmID)
    {
        sLog.outError("Realm ID not defined in configuration file");
        WorldDatabase.HaltDelayThread();
        CharacterDatabase.HaltDelayThread();
        LoginDatabase.HaltDelayThread();
        return false;
    }

    sLog.outString("Realm running as realm ID %d", realmID);
    sLog.outString();

    ///- Clean the database before starting
    clear_online_accounts();

    sWorld.LoadDBVersion();

    sLog.outString("Using World DB: %s", sWorld.GetDBVersion());
    sLog.outString();
    return true;
}

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
                   "    -v, --version              print version and exit\n\r"
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

/// Launch the playerbot daemon
int main(int argc, char** argv)
{
    ///- Command line parsing
    char const* cfg_file = PLAYERBOTD_CONFIG_LOCATION;

    char const* options = ":a:c:s:";

    ACE_Get_Opt cmd_opts(argc, argv, options);
    cmd_opts.long_option("version", 'v', ACE_Get_Opt::NO_ARG);
    cmd_opts.long_option("ahbot", 'a', ACE_Get_Opt::ARG_REQUIRED);

    char serviceDaemonMode = '\0';

    int option;
    while ((option = cmd_opts()) != EOF)
    {
        switch (option)
        {
            case 'a':
                sAuctionBotConfig.SetConfigFileName(cmd_opts.opt_arg());
                break;
            case 'c':
                cfg_file = cmd_opts.opt_arg();
                break;
            case 'v':
                printf("%s\n", GitRevision::GetProjectRevision());
                return 0;
            case 's':
            {
                const char* mode = cmd_opts.opt_arg();

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
                    sLog.outError("Runtime-Error: -%c unsupported argument %s", cmd_opts.opt_opt(), mode);
                    usage(argv[0]);
                    Log::WaitBeforeContinueIfNeed();
                    return 1;
                }
                break;
            }
            case ':':
                sLog.outError("Runtime-Error: -%c option requires an input argument", cmd_opts.opt_opt());
                usage(argv[0]);
                Log::WaitBeforeContinueIfNeed();
                return 1;
            default:
                sLog.outError("Runtime-Error: bad format of commandline arguments");
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
        if (!sConfig.SetSource(PLAYERBOTD_CONFIG_NAME))
        {
            sLog.outError("Could not find configuration file %s.", cfg_file);
            Log::WaitBeforeContinueIfNeed();
            return 1;
        }
        cfg_file = PLAYERBOTD_CONFIG_NAME;
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

    sLog.outString("%s [playerbot-daemon]", GitRevision::GetProjectRevision());
    sLog.outString("%s", GitRevision::GetFullRevision());
    sLog.outString("%s", GitRevision::GetDepElunaFullRevisionStr());
    sLog.outString("%s", GitRevision::GetDepSD3FullRevisionStr());
    print_banner();
    sLog.outString("Using configuration file %s.", cfg_file);

    DETAIL_LOG("Using SSL version: %s (Library: %s)", OPENSSL_VERSION_TEXT, SSLeay_version(SSLEAY_VERSION));

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
    OSSL_PROVIDER* legacy;
    OSSL_PROVIDER* deflt;

    /* Load Multiple providers into the default (NULL) library context */
    legacy = OSSL_PROVIDER_load(NULL, "legacy");
    if (legacy == NULL) {
        sLog.outError("Failed to load OpenSSL 3.x Legacy provider\n");
#ifdef WIN32
        sLog.outError("\nPlease check you have set the following Environment Variable:\n");
        sLog.outError("OPENSSL_MODULES=C:\\OpenSSL-Win64\\bin\n");
        sLog.outError("(where C:\\OpenSSL-Win64\\bin is the location you installed OpenSSL\n");
#endif
        Log::WaitBeforeContinueIfNeed();
        return 0;
    }
    deflt = OSSL_PROVIDER_load(NULL, "default");
    if (deflt == NULL) {
        sLog.outError("Failed to load OpenSSL 3.x Default provider\n");
        OSSL_PROVIDER_unload(legacy);
        Log::WaitBeforeContinueIfNeed();
        return 0;
    }
#else
    if (SSLeay() < 0x10100000L || SSLeay() > 0x10200000L)
    {
        DETAIL_LOG("WARNING: OpenSSL version may be out of date or unsupported. Logins to server may not work!");
        DETAIL_LOG("WARNING: Minimal required version [OpenSSL 1.1.x] and Maximum supported version [OpenSSL 1.2]");
    }
#endif


    DETAIL_LOG("Using ACE: %s", ACE_VERSION);

    ///- Set progress bars show mode
    BarGoLink::SetOutputState(sConfig.GetBoolDefault("ShowProgressBars", true));

    /// playerbotd PID file creation
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

    ///- Start the databases
    if (!start_db())
    {
        Log::WaitBeforeContinueIfNeed();
        return 1;
    }

    ///- Set Realm to Offline, if crash happens. Only used once.
    LoginDatabase.DirectPExecute("UPDATE `realmlist` SET `realmflags` = `realmflags` | %u WHERE `id` = '%u'", REALM_FLAG_OFFLINE, realmID);

    ///- Initialize the World
    sWorld.SetInitialWorldSettings();

#ifndef _WIN32
    detachDaemon();
#endif

    // set realm flag by configuration boolean
    uint8 recommendedornew = sWorld.getConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW) ? REALM_FLAG_NEW_PLAYERS : REALM_FLAG_RECOMMENDED;
    uint8 realmstatus = sWorld.getConfig(CONFIG_BOOL_REALM_RECOMMENDED_OR_NEW_ENABLED) ? recommendedornew : uint8(REALM_FLAG_NONE);

    // set realmbuilds depend on playerbotd expected builds, and set server online
    std::string builds = AcceptableClientBuildsListStr();
    LoginDatabase.escape_string(builds);
    LoginDatabase.DirectPExecute("UPDATE `realmlist` SET `realmflags` = %u, `population` = 0, `realmbuilds` = '%s'  WHERE `id` = '%u'", realmstatus, builds.c_str(), realmID);

    // server loaded successfully => enable async DB requests
    WorldDatabase.ThreadStart();

    CharacterDatabase.AllowAsyncTransactions();
    WorldDatabase.AllowAsyncTransactions();
    LoginDatabase.AllowAsyncTransactions();

    ///- Catch termination signals
    hook_signals();


    //************************************************************************************************************************
    // 1. Start the World thread
    //************************************************************************************************************************

    std::string host = sConfig.GetStringDefault("BindIP", "0.0.0.0");
    uint16 port = sWorld.getConfig(CONFIG_UINT32_PORT_WORLD);

    WorldThread* worldThread = new WorldThread(port, host.c_str());
    worldThread->open(0);


    //************************************************************************************************************************
    // 2. Start the remote access listener thread
    //************************************************************************************************************************
    RAThread* raThread = NULL;
    if (sConfig.GetBoolDefault("Ra.Enable", false))
    {
        port = sConfig.GetIntDefault("Ra.Port", 3443);
        host = sConfig.GetStringDefault("Ra.IP", "0.0.0.0");

        raThread = new RAThread(port, host.c_str());
        raThread->open(0);
    }


    //************************************************************************************************************************
    // 3. Start the freeze catcher thread
    //************************************************************************************************************************
    AntiFreezeThread* freezeThread = new AntiFreezeThread(1000 * sConfig.GetIntDefault("MaxCoreStuckTime", 0));
    freezeThread->open(NULL);


    //************************************************************************************************************************
    // 4. Start the console thread
    //************************************************************************************************************************
    CliThread* cliThread = NULL;
#ifdef _WIN32
    if (sConfig.GetBoolDefault("Console.Enable", true) && (m_ServiceStatus == -1)/* need disable console in service mode*/)
#else
    if (sConfig.GetBoolDefault("Console.Enable", true))
#endif
    {
        ///- Launch CliRunnable thread
        cliThread = new CliThread(sConfig.GetBoolDefault("BeepAtStart", true));
        cliThread->activate();
    }

    worldThread->wait();

    if (cliThread)
    {
        cliThread->cli_shutdown();
        delete cliThread;
    }

    ACE_Thread_Manager::instance()->wait();
    sLog.outString("Halting process...");

    ///- Stop freeze protection before shutdown tasks
    if (freezeThread)
    {
        delete freezeThread;
    }

    if (raThread)
    {
        delete raThread;
    }

    delete worldThread;

    ///- Remove signal handling before leaving
    unhook_signals();

    ///- Set server offline in realmlist
    LoginDatabase.DirectPExecute("UPDATE `realmlist` SET `realmflags` = `realmflags` | %u WHERE `id` = '%u'", REALM_FLAG_OFFLINE, realmID);

    ///- Clean account database before leaving
    clear_online_accounts();

    // send all still queued mass mails (before DB connections shutdown)
    sMassMailMgr.Update(true);

    ///- Wait for DB delay threads to end
    CharacterDatabase.HaltDelayThread();
    WorldDatabase.HaltDelayThread();
    LoginDatabase.HaltDelayThread();

    // This is done to make sure that we cleanup our so file before it's
    // unloaded automatically, since the ~ScriptMgr() is called to late
    // as it's allocated with static storage.
    sScriptMgr.UnloadScriptLibrary();

    ///- Exit the process with specified return value
    int code = World::GetExitCode();

#ifdef WIN32
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif

    sLog.outString("Bye!");
    return code;
}
/// @}
