/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#include "Common/Common.h"
#include "Log.h"
#include "Policies/Singleton.h"
#include "Config/Config.h"
#include "Utilities/Util.h"
#include "Utilities/ByteBuffer.h"
#include "Utilities/ProgressBar.h"

#include <stdarg.h>
#include <fstream>
#include <iostream>

#include <ace/OS_NS_unistd.h>

INSTANTIATE_SINGLETON_1(Log);

LogFilterData logFilterData[LOG_FILTER_COUNT] =
{
    { "transport_moves",     "LogFilter_TransportMoves",     true  },
    { "creature_moves",      "LogFilter_CreatureMoves",      true  },
    { "visibility_changes",  "LogFilter_VisibilityChanges",  true  },
    { "",                    "",                             true  },
    { "weather",             "LogFilter_Weather",            true  },
    { "player_stats",        "LogFilter_PlayerStats",        false },
    { "sql_text",            "LogFilter_SQLText",            true  },
    { "player_moves",        "LogFilter_PlayerMoves",        true  },
    { "periodic_effects",    "LogFilter_PeriodicAffects",    false },
    { "ai_and_movegens",     "LogFilter_AIAndMovegens",      false },
    { "damage",              "LogFilter_Damage",             false },
    { "combat",              "LogFilter_Combat",             false },
    { "spell_cast",          "LogFilter_SpellCast",          false },
    { "db_stricted_check",   "LogFilter_DbStrictedCheck",    true  },
    { "ahbot_seller",        "LogFilter_AhbotSeller",        true  },
    { "ahbot_buyer",         "LogFilter_AhbotBuyer",         true  },
    { "pathfinding",         "LogFilter_Pathfinding",        true  },
    { "map_loading",         "LogFilter_MapLoading",         true  },
    { "event_ai_dev",        "LogFilter_EventAiDev",         true  },
};

enum LogType
{
    LogNormal = 0,
    LogDetails,
    LogDebug,
    LogError
};

const int LogType_count = int(LogError) + 1;

Log::Log() :
    raLogfile(NULL), logfile(NULL), gmLogfile(NULL), charLogfile(NULL), dberLogfile(NULL), 
#ifdef ENABLE_ELUNA
    elunaErrLogfile(NULL), 
#endif /* ENABLE_ELUNA */
    eventAiErLogfile(NULL), scriptErrLogFile(NULL), worldLogfile(NULL), wardenLogfile(NULL), m_colored(false),
    m_includeTime(false), m_gmlog_per_account(false), m_scriptLibName(NULL)
{
    Initialize();
}

void Log::InitColors(const std::string& str)
{
    if (str.empty())
    {
        m_colored = false;
        return;
    }

    int color[4];

    std::istringstream ss(str);

    for (int i = 0; i < LogType_count; ++i)
    {
        ss >> color[i];

        if (!ss)
            { return; }

        if (color[i] < 0 || color[i] >= Color_count)
            { return; }
    }

    for (int i = 0; i < LogType_count; ++i)
        { m_colors[i] = Color(color[i]); }

    m_colored = true;
}

void Log::SetColor(bool stdout_stream, Color color)
{
#if PLATFORM == PLATFORM_WINDOWS

    static WORD WinColorFG[Color_count] =
    {
        0,                                                  // BLACK
        FOREGROUND_RED,                                     // RED
        FOREGROUND_GREEN,                                   // GREEN
        FOREGROUND_RED | FOREGROUND_GREEN,                  // BROWN
        FOREGROUND_BLUE,                                    // BLUE
        FOREGROUND_RED |                    FOREGROUND_BLUE,// MAGENTA
        FOREGROUND_GREEN | FOREGROUND_BLUE,                 // CYAN
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,// WHITE
        // YELLOW
        FOREGROUND_RED | FOREGROUND_GREEN |                   FOREGROUND_INTENSITY,
        // RED_BOLD
        FOREGROUND_RED |                                      FOREGROUND_INTENSITY,
        // GREEN_BOLD
        FOREGROUND_GREEN |                   FOREGROUND_INTENSITY,
        FOREGROUND_BLUE | FOREGROUND_INTENSITY,             // BLUE_BOLD
        // MAGENTA_BOLD
        FOREGROUND_RED |                    FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        // CYAN_BOLD
        FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
        // WHITE_BOLD
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
    };

    HANDLE hConsole = GetStdHandle(stdout_stream ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    SetConsoleTextAttribute(hConsole, WinColorFG[color]);
#else

    enum ANSITextAttr
    {
        TA_NORMAL = 0,
        TA_BOLD = 1,
        TA_BLINK = 5,
        TA_REVERSE = 7
    };

    enum ANSIFgTextAttr
    {
        FG_BLACK = 30, FG_RED,  FG_GREEN, FG_BROWN, FG_BLUE,
        FG_MAGENTA,  FG_CYAN, FG_WHITE, FG_YELLOW
    };

    enum ANSIBgTextAttr
    {
        BG_BLACK = 40, BG_RED,  BG_GREEN, BG_BROWN, BG_BLUE,
        BG_MAGENTA,  BG_CYAN, BG_WHITE
    };

    static uint8 UnixColorFG[Color_count] =
    {
        FG_BLACK,                                           // BLACK
        FG_RED,                                             // RED
        FG_GREEN,                                           // GREEN
        FG_BROWN,                                           // BROWN
        FG_BLUE,                                            // BLUE
        FG_MAGENTA,                                         // MAGENTA
        FG_CYAN,                                            // CYAN
        FG_WHITE,                                           // WHITE
        FG_YELLOW,                                          // YELLOW
        FG_RED,                                             // LRED
        FG_GREEN,                                           // LGREEN
        FG_BLUE,                                            // LBLUE
        FG_MAGENTA,                                         // LMAGENTA
        FG_CYAN,                                            // LCYAN
        FG_WHITE                                            // LWHITE
    };

    fprintf((stdout_stream ? stdout : stderr), "\x1b[%d%sm", UnixColorFG[color], (color >= YELLOW && color < Color_count ? ";1" : ""));
#endif
}

void Log::ResetColor(bool stdout_stream)
{
#if PLATFORM == PLATFORM_WINDOWS
    HANDLE hConsole = GetStdHandle(stdout_stream ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE);
    SetConsoleTextAttribute(hConsole, FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_RED);
#else
    fprintf((stdout_stream ? stdout : stderr), "\x1b[0m");
#endif
}

void Log::SetLogLevel(char* level)
{
    int32 newLevel = atoi((char*)level);

    if (newLevel < LOG_LVL_MINIMAL)
        { newLevel = LOG_LVL_MINIMAL; }
    else if (newLevel > LOG_LVL_DEBUG)
        { newLevel = LOG_LVL_DEBUG; }

    m_logLevel = LogLevel(newLevel);

    printf("LogLevel is %u\n", m_logLevel);
}

void Log::SetLogFileLevel(char* level)
{
    int32 newLevel = atoi((char*)level);

    if (newLevel < LOG_LVL_MINIMAL)
        { newLevel = LOG_LVL_MINIMAL; }
    else if (newLevel > LOG_LVL_DEBUG)
        { newLevel = LOG_LVL_DEBUG; }

    m_logFileLevel = LogLevel(newLevel);

    printf("LogFileLevel is %u\n", m_logFileLevel);
}

void Log::Initialize()
{
    /// Common log files data
    m_logsDir = sConfig.GetStringDefault("LogsDir", "");
    if (!m_logsDir.empty())
    {
        if ((m_logsDir.at(m_logsDir.length() - 1) != '/') && (m_logsDir.at(m_logsDir.length() - 1) != '\\'))
            { m_logsDir.append("/"); }
    }

    m_logsTimestamp = "_" + GetTimestampStr();

    /// Open specific log files
    logfile = openLogFile("LogFile", "LogTimestamp", "w");

    m_gmlog_per_account = sConfig.GetBoolDefault("GmLogPerAccount", false);
    if (!m_gmlog_per_account)
        { gmLogfile = openLogFile("GMLogFile", "GmLogTimestamp", "a"); }
    else
    {
        // GM log settings for per account case
        m_gmlog_filename_format = sConfig.GetStringDefault("GMLogFile", "");
        if (!m_gmlog_filename_format.empty())
        {
            bool m_gmlog_timestamp = sConfig.GetBoolDefault("GmLogTimestamp", false);

            size_t dot_pos = m_gmlog_filename_format.find_last_of(".");
            if (dot_pos != m_gmlog_filename_format.npos)
            {
                if (m_gmlog_timestamp)
                    { m_gmlog_filename_format.insert(dot_pos, m_logsTimestamp); }

                m_gmlog_filename_format.insert(dot_pos, "_#%u");
            }
            else
            {
                m_gmlog_filename_format += "_#%u";

                if (m_gmlog_timestamp)
                    { m_gmlog_filename_format += m_logsTimestamp; }
            }

            m_gmlog_filename_format = m_logsDir + m_gmlog_filename_format;
        }
    }

    charLogfile = openLogFile("CharLogFile", "CharLogTimestamp", "a");
    dberLogfile = openLogFile("DBErrorLogFile", NULL, "a");
#ifdef ENABLE_ELUNA
    elunaErrLogfile = openLogFile("ElunaErrorLogFile", NULL, "a");
#endif /* ENABLE_ELUNA */
    eventAiErLogfile = openLogFile("EventAIErrorLogFile", NULL, "a");
    raLogfile = openLogFile("RaLogFile", NULL, "a");
    worldLogfile = openLogFile("WorldLogFile", "WorldLogTimestamp", "a");
    wardenLogfile = openLogFile("WardenLogFile", "WardenLogTimestamp", "a");

    // Main log file settings
    m_includeTime  = sConfig.GetBoolDefault("LogTime", false);
    m_logLevel     = LogLevel(sConfig.GetIntDefault("LogLevel", 0));
    m_logFileLevel = LogLevel(sConfig.GetIntDefault("LogFileLevel", 0));
    InitColors(sConfig.GetStringDefault("LogColors", ""));

    m_logFilter = 0;
    for (int i = 0; i < LOG_FILTER_COUNT; ++i)
        if (*logFilterData[i].name)
            if (sConfig.GetBoolDefault(logFilterData[i].configName, logFilterData[i].defaultState))
                { m_logFilter |= (1 << i); }

    // Char log settings
    m_charLog_Dump = sConfig.GetBoolDefault("CharLogDump", false);
}

FILE* Log::openLogFile(char const* configFileName, char const* configTimeStampFlag, char const* mode)
{
    std::string logfn = sConfig.GetStringDefault(configFileName, "");
    if (logfn.empty())
        { return NULL; }

    if (configTimeStampFlag && sConfig.GetBoolDefault(configTimeStampFlag, false))
    {
        size_t dot_pos = logfn.find_last_of(".");
        if (dot_pos != logfn.npos)
            { logfn.insert(dot_pos, m_logsTimestamp); }
        else
            { logfn += m_logsTimestamp; }
    }

    return fopen((m_logsDir + logfn).c_str(), mode);
}

FILE* Log::openGmlogPerAccount(uint32 account)
{
    if (m_gmlog_filename_format.empty())
        { return NULL; }

    char namebuf[MANGOS_PATH_MAX];
    snprintf(namebuf, MANGOS_PATH_MAX, m_gmlog_filename_format.c_str(), account);
    return fopen(namebuf, "a");
}

void Log::outTimestamp(FILE* file)
{
    time_t t = time(NULL);
    tm* aTm = localtime(&t);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    fprintf(file, "%-4d-%02d-%02d %02d:%02d:%02d ", aTm->tm_year + 1900, aTm->tm_mon + 1, aTm->tm_mday, aTm->tm_hour, aTm->tm_min, aTm->tm_sec);
}

void Log::outTime()
{
    time_t t = time(NULL);
    tm* aTm = localtime(&t);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    printf("%02d:%02d:%02d ", aTm->tm_hour, aTm->tm_min, aTm->tm_sec);
}

std::string Log::GetTimestampStr()
{
    time_t t = time(NULL);
    tm* aTm = localtime(&t);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d_%02d-%02d-%02d", aTm->tm_year + 1900, aTm->tm_mon + 1, aTm->tm_mday, aTm->tm_hour, aTm->tm_min, aTm->tm_sec);
    return std::string(buf);
}

void Log::outString()
{
    if (m_includeTime)
        { outTime(); }
    printf("\n");
    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "\n");
        fflush(logfile);
    }

    fflush(stdout);
}

void Log::outString(const char* str, ...)
{
    if (!str)
        { return; }

    if (m_colored)
        { SetColor(true, m_colors[LogNormal]); }

    if (m_includeTime)
        { outTime(); }

    va_list ap;

    va_start(ap, str);
    vutf8printf(stdout, str, &ap);
    va_end(ap);

    if (m_colored)
        { ResetColor(true); }

    printf("\n");

    if (logfile)
    {
        outTimestamp(logfile);

        va_start(ap, str);
        vfprintf(logfile, str, ap);
        fprintf(logfile, "\n");
        va_end(ap);

        fflush(logfile);
    }

    fflush(stdout);
}

void Log::outError(const char* err, ...)
{
    if (!err)
    {
        return;
    }

    if (m_colored)
    {
        SetColor(false, m_colors[LogError]);
    }

    if (m_includeTime)
    {
        outTime();
    }

    va_list ap;

    va_start(ap, err);
    vutf8printf(stderr, err, &ap);
    va_end(ap);

    if (m_colored)
    {
        ResetColor(false);
    }

    fprintf(stderr, "\n");
    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR:");

        va_start(ap, err);
        vfprintf(logfile, err, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    fflush(stderr);
}

void Log::outErrorDb()
{
    if (m_includeTime)
        { outTime(); }

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR:\n");
        fflush(logfile);
    }

    if (dberLogfile)
    {
        outTimestamp(dberLogfile);
        fprintf(dberLogfile, "\n");
        fflush(dberLogfile);
    }

    fflush(stderr);
}

void Log::outErrorDb(const char* err, ...)
{
    if (!err)
        { return; }

    if (m_colored)
        { SetColor(false, m_colors[LogError]); }

    if (m_includeTime)
        { outTime(); }

    va_list ap;

    va_start(ap, err);
    vutf8printf(stderr, err, &ap);
    va_end(ap);

    if (m_colored)
        { ResetColor(false); }

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR:");

        va_start(ap, err);
        vfprintf(logfile, err, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    if (dberLogfile)
    {
        outTimestamp(dberLogfile);

        va_list ap;
        va_start(ap, err);
        vfprintf(dberLogfile, err, ap);
        va_end(ap);

        fprintf(dberLogfile, "\n");
        fflush(dberLogfile);
    }

    fflush(stderr);
}

#ifdef ENABLE_ELUNA
void Log::outErrorEluna()
{
    if (m_includeTime)
        outTime();

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR Eluna\n");
        fflush(logfile);
    }

    if (elunaErrLogfile)
    {
        outTimestamp(elunaErrLogfile);
        fprintf(elunaErrLogfile, "\n");
        fflush(elunaErrLogfile);
    }

    fflush(stderr);
}
#else
/* This is made to not fiddle with the eluna code in LuaEngine/ at all */
void Log::outErrorEluna() {}
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_ELUNA
void Log::outErrorEluna(const char* err, ...)
{
    if (!err)
        return;

    if (m_colored)
        SetColor(false, m_colors[LogError]);

    if (m_includeTime)
        outTime();

    va_list ap;

    va_start(ap, err);
    vutf8printf(stderr, err, &ap);
    va_end(ap);

    if (m_colored)
        ResetColor(false);

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR Eluna: ");

        va_start(ap, err);
        vfprintf(logfile, err, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    if (elunaErrLogfile)
    {
        outTimestamp(elunaErrLogfile);

        va_list ap;
        va_start(ap, err);
        vfprintf(elunaErrLogfile, err, ap);
        va_end(ap);

        fprintf(elunaErrLogfile, "\n");
        fflush(elunaErrLogfile);
    }

    fflush(stderr);
}
#else
/* This is made to not fiddle with the eluna code in LuaEngine/ at all */
void Log::outErrorEluna(const char* err, ...) {}
#endif /* ENABLE_ELUNA */

void Log::outErrorEventAI()
{
    if (m_includeTime)
        { outTime(); }

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR CreatureEventAI\n");
        fflush(logfile);
    }

    if (eventAiErLogfile)
    {
        outTimestamp(eventAiErLogfile);
        fprintf(eventAiErLogfile, "\n");
        fflush(eventAiErLogfile);
    }

    fflush(stderr);
}

void Log::outErrorEventAI(const char* err, ...)
{
    if (!err)
        { return; }

    if (m_colored)
        { SetColor(false, m_colors[LogError]); }

    if (m_includeTime)
        { outTime(); }

    va_list ap;

    va_start(ap, err);
    vutf8printf(stderr, err, &ap);
    va_end(ap);

    if (m_colored)
        { ResetColor(false); }

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        fprintf(logfile, "ERROR CreatureEventAI: ");

        va_start(ap, err);
        vfprintf(logfile, err, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    if (eventAiErLogfile)
    {
        outTimestamp(eventAiErLogfile);

        va_list ap;
        va_start(ap, err);
        vfprintf(eventAiErLogfile, err, ap);
        va_end(ap);

        fprintf(eventAiErLogfile, "\n");
        fflush(eventAiErLogfile);
    }

    fflush(stderr);
}

void Log::outBasic(const char* str, ...)
{
    if (!str)
        { return; }

    if (m_logLevel >= LOG_LVL_BASIC)
    {
        if (m_colored)
            { SetColor(true, m_colors[LogDetails]); }

        if (m_includeTime)
            { outTime(); }

        va_list ap;
        va_start(ap, str);
        vutf8printf(stdout, str, &ap);
        va_end(ap);

        if (m_colored)
            { ResetColor(true); }

        printf("\n");
    }

    if (logfile && m_logFileLevel >= LOG_LVL_BASIC)
    {
        va_list ap;
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        fprintf(logfile, "\n");
        va_end(ap);
        fflush(logfile);
    }

    fflush(stdout);
}

void Log::outDetail(const char* str, ...)
{
    if (!str)
        { return; }

    if (m_logLevel >= LOG_LVL_DETAIL)
    {
        if (m_colored)
            { SetColor(true, m_colors[LogDetails]); }

        if (m_includeTime)
            { outTime(); }

        va_list ap;
        va_start(ap, str);
        vutf8printf(stdout, str, &ap);
        va_end(ap);

        if (m_colored)
            { ResetColor(true); }

        printf("\n");
    }

    if (logfile && m_logFileLevel >= LOG_LVL_DETAIL)
    {
        outTimestamp(logfile);

        va_list ap;
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    fflush(stdout);
}

void Log::outDebug(const char* str, ...)
{
    if (!str)
        { return; }

    if (m_logLevel >= LOG_LVL_DEBUG)
    {
        if (m_colored)
            { SetColor(true, m_colors[LogDebug]); }

        if (m_includeTime)
            { outTime(); }

        va_list ap;
        va_start(ap, str);
        vutf8printf(stdout, str, &ap);
        va_end(ap);

        if (m_colored)
            { ResetColor(true); }

        printf("\n");
    }

    if (logfile && m_logFileLevel >= LOG_LVL_DEBUG)
    {
        outTimestamp(logfile);

        va_list ap;
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    fflush(stdout);
}

void Log::outCommand(uint32 account, const char* str, ...)
{
    if (!str)
        { return; }

    if (m_logLevel >= LOG_LVL_DETAIL)
    {
        if (m_colored)
            { SetColor(true, m_colors[LogDetails]); }

        if (m_includeTime)
            { outTime(); }

        va_list ap;
        va_start(ap, str);
        vutf8printf(stdout, str, &ap);
        va_end(ap);

        if (m_colored)
            { ResetColor(true); }

        printf("\n");
    }

    if (logfile && m_logFileLevel >= LOG_LVL_DETAIL)
    {
        va_list ap;
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        fprintf(logfile, "\n");
        va_end(ap);
        fflush(logfile);
    }

    if (m_gmlog_per_account)
    {
        if (FILE* per_file = openGmlogPerAccount(account))
        {
            va_list ap;
            outTimestamp(per_file);
            va_start(ap, str);
            vfprintf(per_file, str, ap);
            fprintf(per_file, "\n");
            va_end(ap);
            fclose(per_file);
        }
    }
    else if (gmLogfile)
    {
        va_list ap;
        outTimestamp(gmLogfile);
        va_start(ap, str);
        vfprintf(gmLogfile, str, ap);
        fprintf(gmLogfile, "\n");
        va_end(ap);
        fflush(gmLogfile);
    }

    fflush(stdout);
}

void Log::outWarden()
{
    if (m_includeTime)
    {
        outTime();
    }
    printf("\n");
    if (wardenLogfile)
    {
        outTimestamp(wardenLogfile);
        fprintf(wardenLogfile, "\n");
        fflush(wardenLogfile);
    }

    fflush(stdout);
}

void Log::outWarden(const char* str, ...)
{
    if (!str)
    {
        return;
    }
    if (m_logLevel >= LOG_LVL_DETAIL)
    {
        if (m_colored)
        {
            SetColor(true, m_colors[LogNormal]);
        }

        if (m_includeTime)
        {
            outTime();
        }

        va_list ap;

        va_start(ap, str);
        vutf8printf(stdout, str, &ap);
        va_end(ap);

        if (m_colored)
        {
            ResetColor(true);
        }

        printf("\n");
    }

    if (wardenLogfile && m_logFileLevel >= LOG_LVL_DETAIL)
    {
        va_list ap;

        outTimestamp(wardenLogfile);
        fprintf(wardenLogfile, "[Warden]: ");

        va_start(ap, str);
        vfprintf(wardenLogfile, str, ap);
        fprintf(wardenLogfile, "\n");
        va_end(ap);

        fflush(wardenLogfile);
    }

    fflush(stdout);
}

void Log::outChar(const char* str, ...)
{
    if (!str)
        { return; }

    if (charLogfile)
    {
        va_list ap;
        outTimestamp(charLogfile);
        va_start(ap, str);
        vfprintf(charLogfile, str, ap);
        fprintf(charLogfile, "\n");
        va_end(ap);
        fflush(charLogfile);
    }
}

void Log::outErrorScriptLib()
{
    if (m_includeTime)
        { outTime(); }

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        if (m_scriptLibName)
            { fprintf(logfile, "<%s ERROR:> ", m_scriptLibName); }
        else
            { fprintf(logfile, "<Scripting Library ERROR>: "); }
        fflush(logfile);
    }

    if (scriptErrLogFile)
    {
        outTimestamp(scriptErrLogFile);
        fprintf(scriptErrLogFile, "\n");
        fflush(scriptErrLogFile);
    }

    fflush(stderr);
}

void Log::outErrorScriptLib(const char* err, ...)
{
    if (!err)
        { return; }

    if (m_colored)
        { SetColor(false, m_colors[LogError]); }

    if (m_includeTime)
        { outTime(); }

    va_list ap;

    va_start(ap, err);
    vutf8printf(stderr, err, &ap);
    va_end(ap);

    if (m_colored)
        { ResetColor(false); }

    fprintf(stderr, "\n");

    if (logfile)
    {
        outTimestamp(logfile);
        if (m_scriptLibName)
            { fprintf(logfile, "<%s ERROR>: ", m_scriptLibName); }
        else
            { fprintf(logfile, "<Scripting Library ERROR>: "); }

        va_start(ap, err);
        vfprintf(logfile, err, ap);
        va_end(ap);

        fprintf(logfile, "\n");
        fflush(logfile);
    }

    if (scriptErrLogFile)
    {
        outTimestamp(scriptErrLogFile);

        va_list ap;
        va_start(ap, err);
        vfprintf(scriptErrLogFile, err, ap);
        va_end(ap);

        fprintf(scriptErrLogFile, "\n");
        fflush(scriptErrLogFile);
    }

    fflush(stderr);
}

void Log::outWorldPacketDump(uint32 socket, uint32 opcode, char const* opcodeName, ByteBuffer const* packet, bool incoming)
{
    if (!worldLogfile)
        { return; }

    ACE_GUARD(ACE_Thread_Mutex, GuardObj, m_worldLogMtx);

    outTimestamp(worldLogfile);

    fprintf(worldLogfile, "\n%s:\nSOCKET: %u\nLENGTH: " SIZEFMTD "\nOPCODE: %s (0x%.4X)\nDATA:\n",
            incoming ? "CLIENT" : "SERVER",
            socket, packet->size(), opcodeName, opcode);

    size_t p = 0;
    while (p < packet->size())
    {
        for (size_t j = 0; j < 16 && p < packet->size(); ++j)
            { fprintf(worldLogfile, "%.2X ", (*packet)[p++]); }

        fprintf(worldLogfile, "\n");
    }

    fprintf(worldLogfile, "\n\n");
    fflush(worldLogfile);
}

void Log::outCharDump(const char* str, uint32 account_id, uint32 guid, const char* name)
{
    if (charLogfile)
    {
        fprintf(charLogfile, "== START DUMP == (account: %u guid: %u name: %s )\n%s\n== END DUMP ==\n", account_id, guid, name, str);
        fflush(charLogfile);
    }
}

void Log::outRALog(const char* str, ...)
{
    if (!str)
        { return; }

    if (raLogfile)
    {
        va_list ap;
        outTimestamp(raLogfile);
        va_start(ap, str);
        vfprintf(raLogfile, str, ap);
        fprintf(raLogfile, "\n");
        va_end(ap);
        fflush(raLogfile);
    }

    fflush(stdout);
}

void Log::WaitBeforeContinueIfNeed()
{
    int mode = sConfig.GetIntDefault("WaitAtStartupError", 0);

    if (mode < 0)
    {
        printf("\nPress <Enter> for continue\n");

        std::string line;
        std::getline(std::cin, line);
    }
    else if (mode > 0)
    {
        printf("\nWait %u secs for continue.\n", mode);
        BarGoLink bar(mode);
        for (int i = 0; i < mode; ++i)
        {
            bar.step();
            ACE_OS::sleep(1);
        }
    }
}

void Log::setScriptLibraryErrorFile(char const* fname, char const* libName)
{
    m_scriptLibName = libName;

    if (scriptErrLogFile)
        { fclose(scriptErrLogFile); }

    if (!fname)
    {
        scriptErrLogFile = NULL;
        return;
    }

    std::string fileName = m_logsDir;
    fileName.append(fname);
    scriptErrLogFile = fopen(fileName.c_str(), "a");
}

void outstring_log(const char* str, ...)
{
    if (!str)
        { return; }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    sLog.outString("%s", buf);
}

void detail_log(const char* str, ...)
{
    if (!str)
        { return; }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    sLog.outDetail("%s", buf);
}

void debug_log(const char* str, ...)
{
    if (!str)
        { return; }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    DEBUG_LOG("%s", buf);
}

void error_log(const char* str, ...)
{
    if (!str)
        { return; }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    sLog.outError("%s", buf);
}

void error_db_log(const char* str, ...)
{
    if (!str)
        { return; }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    sLog.outErrorDb("%s", buf);
}

void setScriptLibraryErrorFile(char const* fname, char const* libName)
{
    sLog.setScriptLibraryErrorFile(fname, libName);
}

void script_error_log(const char* str, ...)
{
    if (!str)
        { return; }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    sLog.outErrorScriptLib("%s", buf);
}
