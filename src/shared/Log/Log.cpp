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
 * @file Log.cpp
 * @brief Logging system implementation
 *
 * This file implements the logging system for MaNGOS, providing:
 * - Multiple log levels (DEBUG, INFO, ERROR, etc.)
 * - Colored console output (platform-specific)
 * - File-based logging with automatic rotation
 * - Specialized log files for different subsystems
 * - Log filtering by category
 * - Thread-safe logging operations
 *
 * The Log class is a singleton accessed via sLog throughout the codebase.
 * It supports both formatted output and raw string logging.
 */

#include "Common/Common.h"
#include "Log.h"
#include "ConsoleLogWriter.h"
#include "Policies/Singleton.h"
#include "Config/Config.h"
#include "Utilities/ConsoleStyle.h"
#include "Utilities/Util.h"
#include "Utilities/ByteBuffer.h"
#include "Utilities/ProgressBar.h"

#include <stdarg.h>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <chrono>
#include <mutex>
#include <thread>


INSTANTIATE_SINGLETON_1(Log);

LogFilterData logFilterData[LOG_FILTER_COUNT] =
{
    { "transport_moves",     "LogFilter_TransportMoves",     true  },
    { "creature_moves",      "LogFilter_CreatureMoves",      true  },
    { "visibility_changes",  "LogFilter_VisibilityChanges",  true  },
    { "",                    "",                             true  },
    { "weather",             "LogFilter_Weather",            true  },
    { "player_stats",        "LogFilter_PlayerStats",        true  },
    { "sql_text",            "LogFilter_SQLText",            true  },
    { "player_moves",        "LogFilter_PlayerMoves",        true  },
    { "periodic_effects",    "LogFilter_PeriodicAffects",    true  },
    { "ai_and_movegens",     "LogFilter_AIAndMovegens",      true  },
    { "damage",              "LogFilter_Damage",             true  },
    { "combat",              "LogFilter_Combat",             true  },
    { "spell_cast",          "LogFilter_SpellCast",          true  },
    { "db_stricted_check",   "LogFilter_DbStrictedCheck",    true  },
    { "ahbot_seller",        "LogFilter_AhbotSeller",        true  },
    { "ahbot_buyer",         "LogFilter_AhbotBuyer",         true  },
    { "pathfinding",         "LogFilter_Pathfinding",        true  },
    { "map_loading",         "LogFilter_MapsLoading",        true  },
    { "event_ai_dev",        "LogFilter_EventAiDev",         true  },
    { "cell_envelope",       "LogFilter_CellEnvelope",       true  },
    { "grid_add",            "LogFilter_GridAdd",            true  },
    { "db_scripts",          "LogFilter_DbScripts",          true  },
};

enum LogType
{
    LogNormal = 0,
    LogDetails,
    LogDebug,
    LogError
};

const int LogType_count = int(LogError) + 1;

/**
 * @brief Construct the Log singleton
 *
 * Initializes all log file handles to NULL and sets default values:
 * - No colored output initially
 * - Time not included by default
 * - GM log not split per account
 * - Calls Initialize() to set up log filters
 *
 * @note This is called automatically when sLog is first accessed
 */
Log::Log()
    : raLogfile(NULL), logfile(NULL), gmLogfile(NULL), charLogfile(NULL), dberLogfile(NULL),
#ifdef ENABLE_ELUNA
    elunaErrLogfile(NULL),
#endif /* ENABLE_ELUNA */

eventAiErLogfile(NULL), scriptErrLogFile(NULL), worldLogfile(NULL), wardenLogfile(NULL),
    m_consoleBody(NULL), m_consoleThread(NULL), m_consoleAsync(false), m_consoleFilter(NULL),
    m_colored(false), m_includeTime(false), m_gmlog_per_account(false), m_scriptLibName(NULL)
{
    Initialize();
}

/**
 * @brief Initialize console colors from configuration string
 * @param str Space-separated color indices (4 values for Normal, Details, Debug, Error)
 *
 * Parses color configuration and enables colored console output.
 * Expected format: "<normal> <details> <debug> <error>" where each
 * value is an index from the Color enum (0-15).
 *
 * @note If string is empty or invalid, colored output is disabled
 */
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
        {
            return;
        }

        if (color[i] < 0 || color[i] >= Color_count)
        {
            return;
        }
    }

    for (int i = 0; i < LogType_count; ++i)
    {
        m_colors[i] = Color(color[i]);
    }

    m_colored = true;
}

void Log::SetColor(bool stdout_stream, Color color)
{
#if PLATFORM == PLATFORM_WINDOWS

    static WORD WinColorFG[Color_count] =
    {
        0,                                                                         // BLACK
        FOREGROUND_RED,                                                            // RED
        FOREGROUND_GREEN,                                                          // GREEN
        FOREGROUND_RED | FOREGROUND_GREEN,                                         // BROWN
        FOREGROUND_BLUE,                                                           // BLUE
        FOREGROUND_RED |                    FOREGROUND_BLUE,                       // MAGENTA
        FOREGROUND_GREEN | FOREGROUND_BLUE,                                        // CYAN
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,                       // WHITE
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,                  // YELLOW
        FOREGROUND_RED | FOREGROUND_INTENSITY,                                     // RED_BOLD
        FOREGROUND_GREEN | FOREGROUND_INTENSITY,                                   // GREEN_BOLD
        FOREGROUND_BLUE | FOREGROUND_INTENSITY,                                    // BLUE_BOLD
        FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,                   // MAGENTA_BOLD
        FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,                 // CYAN_BOLD
        FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY // WHITE_BOLD
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
    {
        newLevel = LOG_LVL_MINIMAL;
    }
    else if (newLevel > LOG_LVL_DEBUG)
    {
        newLevel = LOG_LVL_DEBUG;
    }

    m_logLevel = LogLevel(newLevel);

    ConsoleEmitRaw("LogLevel is " + std::to_string((uint32)m_logLevel) + "\n");
}

void Log::SetLogFileLevel(char* level)
{
    int32 newLevel = atoi((char*)level);

    if (newLevel < LOG_LVL_MINIMAL)
    {
        newLevel = LOG_LVL_MINIMAL;
    }
    else if (newLevel > LOG_LVL_DEBUG)
    {
        newLevel = LOG_LVL_DEBUG;
    }

    m_logFileLevel = LogLevel(newLevel);

    ConsoleEmitRaw("LogFileLevel is " + std::to_string((uint32)m_logFileLevel) + "\n");
}

void Log::CloseLogFiles()
{
    if (logfile != NULL)
    {
        fclose(logfile);
        logfile = NULL;
    }
    if (gmLogfile != NULL)
    {
        fclose(gmLogfile);
        gmLogfile = NULL;
    }
    if (charLogfile != NULL)
    {
        fclose(charLogfile);
        charLogfile = NULL;
    }
    if (dberLogfile != NULL)
    {
        fclose(dberLogfile);
        dberLogfile = NULL;
    }
#ifdef ENABLE_ELUNA
    if (elunaErrLogfile != NULL)
    {
        fclose(elunaErrLogfile);
        elunaErrLogfile = NULL;
    }
#endif /* ENABLE_ELUNA */
    if (eventAiErLogfile != NULL)
    {
        fclose(eventAiErLogfile);
        eventAiErLogfile = NULL;
    }
    if (scriptErrLogFile != NULL)
    {
        fclose(scriptErrLogFile);
        scriptErrLogFile = NULL;
    }
    if (raLogfile != NULL)
    {
        fclose(raLogfile);
        raLogfile = NULL;
    }
    if (worldLogfile != NULL)
    {
        fclose(worldLogfile);
        worldLogfile = NULL;
    }
    if (wardenLogfile != NULL)
    {
        fclose(wardenLogfile);
        wardenLogfile = NULL;
    }
}

void Log::Flush()
{
    // Push the buffered sinks to the OS. Best-effort, not a crash drain: error
    // paths flush immediately at emit time, and shutdown flushes via fclose in
    // CloseLogFiles(). Also flush stdout so a redirected/piped console (fully
    // buffered when not a TTY) does not lag behind.
    fflush(stdout);

    {
        std::lock_guard<std::mutex> fileGuard(m_fileMtx);
        if (logfile != NULL)
        {
            fflush(logfile);
        }
    }

    {
        std::lock_guard<std::mutex> worldGuard(m_worldLogMtx);
        if (worldLogfile != NULL)
        {
            fflush(worldLogfile);
        }
    }
}

std::string Log::ConsoleTimePrefix() const
{
    if (!m_includeTime)
    {
        return std::string();
    }

    time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm aTm = safe_localtime(tt);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d ", aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
    return std::string(buf);
}

void Log::ConsoleEmit(bool toStdout, Color color, bool applyColor, const char* fmt, va_list* ap)
{
    // Format first, so the filter sees the message the way the user wrote it
    // (the time prefix, if any, is prepended only once the line survives).
    std::string message = vutf8format(fmt, ap);

    // Console-side filter: the startup UI folds the ">> Loaded N rows" result
    // lines into the step they belong to instead of printing them. The caller
    // still writes the line to the log file, so nothing is lost from the record.
    // stderr (outError/outErrorDb) is never offered: errors must always show.
    if (toStdout && m_consoleFilter && m_consoleFilter(message.c_str()))
    {
        return;
    }

    // Record text carries NO trailing newline: the newline is emitted after
    // ResetColor (here and in ConsoleLogWriter::Emit) so the line terminator
    // stays OUTSIDE the color span, byte-matching the legacy ordering
    // (SetColor -> body -> ResetColor -> "\n").
    std::string body;
    body.reserve(message.size() + 16);
    body += ConsoleTimePrefix();
    body += message;

    if (m_consoleAsync && m_consoleBody)
    {
        ConsoleLogRecord rec;
        rec.text = std::move(body);
        rec.color = color;
        rec.applyColor = applyColor;
        rec.toStdout = toStdout;
        m_consoleBody->Enqueue(rec);
    }
    else
    {
        // synchronous fallback (thread not started yet / already stopped)
        FILE* out = toStdout ? stdout : stderr;
        Log::ClearConsoleLine(out);
        if (applyColor)
        {
            Log::SetColor(toStdout, color);
        }
        fwrite(body.data(), 1, body.size(), out);
        if (applyColor)
        {
            Log::ResetColor(toStdout);
        }
        fputc('\n', out);
        if (!toStdout)
        {
            fflush(stderr);
        }
    }
}

void Log::ConsoleEmitBlank(bool toStdout)
{
    // A blank spacer would break a line the startup UI is repainting in place,
    // so offer it to the filter too (as the empty string) before drawing it.
    if (toStdout && m_consoleFilter && m_consoleFilter(""))
    {
        return;
    }

    // Uncolored variant: text is the (possibly empty) time prefix only; the
    // newline is appended after it, matching the legacy bare fprintf("\n").
    std::string b = ConsoleTimePrefix();
    if (m_consoleAsync && m_consoleBody)
    {
        ConsoleLogRecord rec;
        rec.text = b;
        rec.applyColor = false;
        rec.toStdout = toStdout;
        m_consoleBody->Enqueue(rec);
    }
    else
    {
        FILE* out = toStdout ? stdout : stderr;
        Log::ClearConsoleLine(out);
        fwrite(b.data(), 1, b.size(), out);
        fputc('\n', out);
        if (!toStdout)
        {
            fflush(stderr);
        }
    }
}

void Log::ConsoleEmitRaw(const std::string& bytes)
{
    // Verbatim console passthrough: NO time prefix, NO color, NO appended
    // newline. The bytes (a full progress-bar redraw, carrying their own
    // '\r'/'\n') are handed to the writer thread as ONE atomic record so they
    // cannot tear against concurrently-drained log lines. Before the writer is
    // started / after it is stopped (realmd, offline tools, startup + shutdown
    // tail) this falls back to a synchronous verbatim write, byte-identical to
    // the legacy BarGoLink printf()+fflush().
    if (bytes.empty())
    {
        return;
    }
    if (m_consoleAsync && m_consoleBody)
    {
        ConsoleLogRecord rec;
        rec.text = bytes;
        rec.isRaw = true;
        rec.applyColor = false;
        rec.toStdout = true;
        m_consoleBody->Enqueue(rec);
    }
    else
    {
        fwrite(bytes.data(), 1, bytes.size(), stdout);
        fflush(stdout);
        Log::MarkConsoleLineDirty(bytes[bytes.size() - 1] != '\n');
    }
}

// Cursor bookkeeping for in-place repaints. Written only by whoever currently
// owns stdout -- the writer thread while it runs, the producer on the
// synchronous fallback before it starts / after it stops -- and those two never
// overlap (see the StopConsoleThread invariant), so a plain bool is sufficient.
namespace
{
    bool s_consoleLineDirty = false;
}

void Log::MarkConsoleLineDirty(bool dirty)
{
    s_consoleLineDirty = dirty;
}

void Log::ClearConsoleLine(FILE* out)
{
    if (!s_consoleLineDirty)
    {
        return;
    }

    if (ConsoleStyle::Colored())
    {
        fputs("\r\x1b[K", out);                             // CSI K: erase to end of line
    }
    else
    {
        const std::string blank(size_t(ConsoleStyle::Width() - 1), ' ');
        fputc('\r', out);
        fwrite(blank.data(), 1, blank.size(), out);
        fputc('\r', out);
    }

    s_consoleLineDirty = false;
}

void Log::StartConsoleThread()
{
    if (m_consoleThread)
    {
        return;                                             // idempotent (realmd double-init)
    }
    m_consoleBody = new ConsoleLogWriter();
    m_consoleThread = new MaNGOS::Thread(m_consoleBody); // ctor auto-starts run()
    m_consoleAsync = true;
}

// INVARIANT: call only after every console-producing thread is quiesced (world +
// map-update workers joined). After m_consoleAsync=false a straggler
// producer takes the synchronous fallback, but a producer already past the
// m_consoleAsync check in ConsoleEmit could still touch m_consoleBody while we
// delete it; the shutdown ordering at the call site guarantees no such concurrent
// producer exists.
void Log::StopConsoleThread()
{
    if (!m_consoleBody || !m_consoleThread)
    {
        return;
    }
    m_consoleAsync = false;                                 // stragglers now take the synchronous fallback
    m_consoleBody->Stop();                                  // m_running = false (MUST be before wait())
    m_consoleThread->wait();                                // join: run() does its final DrainOnce then returns
    delete m_consoleThread;                                 // ALSO deletes m_consoleBody via refcount
    m_consoleThread = NULL;
    m_consoleBody = NULL;
}

void Log::Initialize()
{
    /// Common log files data
    m_logsDir = sConfig.GetStringDefault("LogsDir", "");
    if (!m_logsDir.empty())
    {
        if ((m_logsDir.at(m_logsDir.length() - 1) != '/') && (m_logsDir.at(m_logsDir.length() - 1) != '\\'))
        {
            m_logsDir.append("/");
        }
    }

    m_logsTimestamp = "_" + GetTimestampStr();

    // Idempotent re-init: realmd calls Initialize() a second time after its
    // config is loaded (the ctor's first call ran before config and opened
    // nothing). Close any handle already open so this reopens cleanly instead
    // of leaking the previous FILE*.
    CloseLogFiles();

    /// Open specific log files
    logfile = openLogFile("LogFile", "LogTimestamp", "w");

    m_gmlog_per_account = sConfig.GetBoolDefault("GmLogPerAccount", false);
    if (!m_gmlog_per_account)
    {
        gmLogfile = openLogFile("GMLogFile", "GmLogTimestamp", "a");
    }
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
                {
                    m_gmlog_filename_format.insert(dot_pos, m_logsTimestamp);
                }

                m_gmlog_filename_format.insert(dot_pos, "_#%u");
            }
            else
            {
                m_gmlog_filename_format += "_#%u";

                if (m_gmlog_timestamp)
                {
                    m_gmlog_filename_format += m_logsTimestamp;
                }
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
    // Packet logging is opt-in via PacketLoggingEnabled (default off): open the
    // packet log only when explicitly enabled, so it stays off even on legacy
    // configs that still set WorldLogFile with LogLevel=3 (and a disabled server
    // creates no stray empty world-packets.log). IsPacketLoggingEnabled() keys
    // off worldLogfile != NULL, so this open is the single gate.
    if (sConfig.GetBoolDefault("PacketLoggingEnabled", false))
    {
        worldLogfile = openLogFile("WorldLogFile", "WorldLogTimestamp", "a");
    }
    wardenLogfile = openLogFile("WardenLogFile", "WardenLogTimestamp", "a");

    // Main log file settings
    m_includeTime  = sConfig.GetBoolDefault("LogTime", false);
    m_logLevel     = LogLevel(sConfig.GetIntDefault("LogLevel", 0));
    m_logFileLevel = LogLevel(sConfig.GetIntDefault("LogFileLevel", 0));
    InitColors(sConfig.GetStringDefault("LogColors", ""));

    m_logFilter = 0;
    for (int i = 0; i < LOG_FILTER_COUNT; ++i)
    {
        if (*logFilterData[i].name)
        {
            if (sConfig.GetBoolDefault(logFilterData[i].configName, logFilterData[i].defaultState))
            {
                m_logFilter |= (1 << i);
            }
        }
    }

    // Char log settings
    m_charLog_Dump = sConfig.GetBoolDefault("CharLogDump", false);
}

FILE* Log::openLogFile(char const* configFileName, char const* configTimeStampFlag, char const* mode)
{
    std::string logfn = sConfig.GetStringDefault(configFileName, "");
    if (logfn.empty())
    {
        return NULL;
    }

    if (configTimeStampFlag && sConfig.GetBoolDefault(configTimeStampFlag, false))
    {
        size_t dot_pos = logfn.find_last_of(".");
        if (dot_pos != logfn.npos)
        {
            logfn.insert(dot_pos, m_logsTimestamp);
        }
        else
        {
            logfn += m_logsTimestamp;
        }
    }

    FILE* fp = fopen((m_logsDir + logfn).c_str(), mode);
    if (fp != NULL)
    {
        // Fully buffer log files (64 KiB). Per-line fflush is removed from the
        // hot loggers (outString/outBasic/outDetail/outDebug); Log::Flush()
        // (world tick + shutdown) and the error paths push the buffer to the
        // OS. This removes the per-line flush syscall that stalled the world
        // and map-update threads at LogFileLevel=3.
        setvbuf(fp, NULL, _IOFBF, 64 * 1024);
    }
    return fp;
}

FILE* Log::openGmlogPerAccount(uint32 account)
{
    if (m_gmlog_filename_format.empty())
    {
        return NULL;
    }

    char namebuf[MANGOS_PATH_MAX];
    snprintf(namebuf, MANGOS_PATH_MAX, m_gmlog_filename_format.c_str(), account);
    return fopen(namebuf, "a");
}

void Log::outTimestamp(FILE* file)
{
    time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm aTm = safe_localtime(tt);

    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    fprintf(file, "%-4d-%02d-%02d %02d:%02d:%02d ", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday, aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
}

void Log::outTime()
{
    time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm aTm = safe_localtime(tt);

    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    printf("%02d:%02d:%02d ", aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
}

std::string Log::GetTimestampStr()
{
    time_t tt = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());

    std::tm aTm = safe_localtime(tt);
    //       YYYY   year
    //       MM     month (2 digits 01-12)
    //       DD     day (2 digits 01-31)
    //       HH     hour (2 digits 00-23)
    //       MM     minutes (2 digits 00-59)
    //       SS     seconds (2 digits 00-59)
    char buf[20];
    snprintf(buf, 20, "%04d-%02d-%02d_%02d-%02d-%02d", aTm.tm_year + 1900, aTm.tm_mon + 1, aTm.tm_mday, aTm.tm_hour, aTm.tm_min, aTm.tm_sec);
    return std::string(buf);
}

void Log::outString()
{
    ConsoleEmitBlank(true);
    if (logfile)
    {
        std::lock_guard<std::mutex> fileGuard(m_fileMtx);
        outTimestamp(logfile);
        fprintf(logfile, "\n");
    }
}

void Log::outString(const char* str, ...)
{
    if (!str)
    {
        return;
    }

    va_list ap;

    va_start(ap, str);
    ConsoleEmit(true, m_colors[LogNormal], m_colored, str, &ap);
    va_end(ap);

    if (logfile)
    {
        std::lock_guard<std::mutex> fileGuard(m_fileMtx);
        outTimestamp(logfile);

        va_start(ap, str);
        vfprintf(logfile, str, ap);
        fprintf(logfile, "\n");
        va_end(ap);
    }
}

void Log::outError(const char* err, ...)
{
    if (!err)
    {
        return;
    }

    va_list ap;

    va_start(ap, err);
    ConsoleEmit(false, m_colors[LogError], m_colored, err, &ap);
    va_end(ap);

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
}

void Log::outErrorDb()
{
    ConsoleEmitBlank(false);

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
}

void Log::outErrorDb(const char* err, ...)
{
    if (!err)
    {
        return;
    }

    va_list ap;

    va_start(ap, err);
    ConsoleEmit(false, m_colors[LogError], m_colored, err, &ap);
    va_end(ap);

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
}

#ifdef ENABLE_ELUNA
void Log::outErrorEluna()
{
    ConsoleEmitBlank(false);

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
}
#else
/* This is made to not fiddle with the eluna code in LuaEngine/ at all */
void Log::outErrorEluna() {}
#endif /* ENABLE_ELUNA */

#ifdef ENABLE_ELUNA
void Log::outErrorEluna(const char* err, ...)
{
    if (!err)
    {
        return;
    }

    va_list ap;

    va_start(ap, err);
    ConsoleEmit(false, m_colors[LogError], m_colored, err, &ap);
    va_end(ap);

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
}
#else
/* This is made to not fiddle with the eluna code in LuaEngine/ at all */
void Log::outErrorEluna(const char* err, ...) {}
#endif /* ENABLE_ELUNA */

void Log::outErrorEventAI()
{
    ConsoleEmitBlank(false);

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
}

void Log::outErrorEventAI(const char* err, ...)
{
    if (!err)
    {
        return;
    }

    va_list ap;

    va_start(ap, err);
    ConsoleEmit(false, m_colors[LogError], m_colored, err, &ap);
    va_end(ap);

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
}

void Log::outBasic(const char* str, ...)
{
    if (!str)
    {
        return;
    }

    if (m_logLevel >= LOG_LVL_BASIC)
    {
        va_list ap;
        va_start(ap, str);
        ConsoleEmit(true, m_colors[LogDetails], m_colored, str, &ap);
        va_end(ap);
    }

    if (logfile && m_logFileLevel >= LOG_LVL_BASIC)
    {
        std::lock_guard<std::mutex> fileGuard(m_fileMtx);
        va_list ap;
        outTimestamp(logfile);
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        fprintf(logfile, "\n");
        va_end(ap);
    }
}

void Log::outDetail(const char* str, ...)
{
    if (!str)
    {
        return;
    }

    if (m_logLevel >= LOG_LVL_DETAIL)
    {
        va_list ap;
        va_start(ap, str);
        ConsoleEmit(true, m_colors[LogDetails], m_colored, str, &ap);
        va_end(ap);
    }

    if (logfile && m_logFileLevel >= LOG_LVL_DETAIL)
    {
        std::lock_guard<std::mutex> fileGuard(m_fileMtx);
        outTimestamp(logfile);

        va_list ap;
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);

        fprintf(logfile, "\n");
    }
}

void Log::outDebug(const char* str, ...)
{
    if (!str)
    {
        return;
    }

    if (m_logLevel >= LOG_LVL_DEBUG)
    {
        va_list ap;
        va_start(ap, str);
        ConsoleEmit(true, m_colors[LogDebug], m_colored, str, &ap);
        va_end(ap);
    }

    if (logfile && m_logFileLevel >= LOG_LVL_DEBUG)
    {
        std::lock_guard<std::mutex> fileGuard(m_fileMtx);
        outTimestamp(logfile);

        va_list ap;
        va_start(ap, str);
        vfprintf(logfile, str, ap);
        va_end(ap);

        fprintf(logfile, "\n");
    }
}

void Log::outCommand(uint32 account, const char* str, ...)
{
    if (!str)
    {
        return;
    }

    if (m_logLevel >= LOG_LVL_DETAIL)
    {
        va_list ap;
        va_start(ap, str);
        ConsoleEmit(true, m_colors[LogDetails], m_colored, str, &ap);
        va_end(ap);
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
    ConsoleEmitBlank(true);
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
        va_list ap;

        va_start(ap, str);
        ConsoleEmit(true, m_colors[LogNormal], m_colored, str, &ap);
        va_end(ap);
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
    {
        return;
    }

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
    ConsoleEmitBlank(false);

    if (logfile)
    {
        outTimestamp(logfile);
        if (m_scriptLibName)
        {
            fprintf(logfile, "<%s ERROR:> ", m_scriptLibName);
        }
        else
        {
            fprintf(logfile, "<Scripting Library ERROR>: ");
        }
        fflush(logfile);
    }

    if (scriptErrLogFile)
    {
        outTimestamp(scriptErrLogFile);
        fprintf(scriptErrLogFile, "\n");
        fflush(scriptErrLogFile);
    }
}

void Log::outErrorScriptLib(const char* err, ...)
{
    if (!err)
    {
        return;
    }

    va_list ap;

    va_start(ap, err);
    ConsoleEmit(false, m_colors[LogError], m_colored, err, &ap);
    va_end(ap);

    if (logfile)
    {
        outTimestamp(logfile);
        if (m_scriptLibName)
        {
            fprintf(logfile, "<%s ERROR>: ", m_scriptLibName);
        }
        else
        {
            fprintf(logfile, "<Scripting Library ERROR>: ");
        }

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
}

void Log::outWorldPacketDump(uint32 socket, uint32 opcode, char const* opcodeName, ByteBuffer const* packet, bool incoming)
{
    if (!worldLogfile)
    {
        return;
    }

    std::lock_guard<std::mutex> GuardObj(m_worldLogMtx);

    outTimestamp(worldLogfile);

    // Build the whole hex dump into one buffer and emit it with a single
    // fwrite, instead of one fprintf PER BYTE (16+ stdio calls per row). Output
    // is byte-identical to the previous format. Durability via Flush()/shutdown.
    std::string out;
    out.reserve(packet->size() * 3 + packet->size() / 16 + 128);

    // header[512] is ample for the fixed text plus a (short, compile-time)
    // opcode-name constant; snprintf is bounded, so output stays byte-identical
    // to the previous fprintf for every real opcode name.
    char header[512];
    snprintf(header, sizeof(header), "\n%s:\nSOCKET: %u\nLENGTH: %zu\nOPCODE: %s (0x%.4X)\nDATA:\n",
        incoming ? "CLIENT" : "SERVER",
        socket, packet->size(), opcodeName, opcode);
    out += header;

    char hexbuf[4];
    size_t p = 0;
    while (p < packet->size())
    {
        for (size_t j = 0; j < 16 && p < packet->size(); ++j)
        {
            snprintf(hexbuf, sizeof(hexbuf), "%.2X ", (*packet)[p++]);
            out += hexbuf;
        }

        out += "\n";
    }

    out += "\n\n";
    fwrite(out.c_str(), 1, out.size(), worldLogfile);
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
    {
        return;
    }

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
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void Log::setScriptLibraryErrorFile(char const* fname, char const* libName)
{
    m_scriptLibName = libName;

    if (scriptErrLogFile)
    {
        fclose(scriptErrLogFile);
    }

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
    {
        return;
    }

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
    {
        return;
    }

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
    {
        return;
    }

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
    {
        return;
    }

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
    {
        return;
    }

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
    {
        return;
    }

    char buf[256];
    va_list ap;
    va_start(ap, str);
    vsnprintf(buf, 256, str, ap);
    va_end(ap);

    sLog.outErrorScriptLib("%s", buf);
}
