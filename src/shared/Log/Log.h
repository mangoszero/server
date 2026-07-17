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

#ifndef MANGOSSERVER_LOG_H
#define MANGOSSERVER_LOG_H

#include "Common/Common.h"
#include "Policies/Singleton.h"

#include <cstdio>
#include <mutex>
#include <string>
#include <cstdarg>

class Config;
class ByteBuffer;
class ConsoleLogWriter;
namespace MaNGOS { class Thread; }

/**
 * @brief Logging severity levels for message filtering
 *
 * Defines the verbosity levels for logging output. Messages are only logged
 * if their severity level is less than or equal to the configured threshold.
 */
enum LogLevel
{
    LOG_LVL_MINIMAL = 0,  /**< Only critical errors */
    LOG_LVL_BASIC   = 1,  /**< Basic information and errors */
    LOG_LVL_DETAIL  = 2,  /**< Detailed diagnostic information */
    LOG_LVL_DEBUG   = 3   /**< Full debug output */
};

/**
 * @brief Bitmask flags for selective logging of different subsystems
 *
 * Allows fine-grained control over which types of events are logged.
 * Each flag represents a specific subsystem or type of event that can be
 * independently enabled or disabled. Multiple flags can be combined using bitwise OR.
 *
 * @note When adding new filters, update logFilterData array and LOG_FILTER_COUNT
 */
enum LogFilters
{
    LOG_FILTER_TRANSPORT_MOVES    = 0x000001,               //  0 any related to transport moves
    LOG_FILTER_CREATURE_MOVES     = 0x000002,               //  1 creature move by cells
    LOG_FILTER_VISIBILITY_CHANGES = 0x000004,               //  2 update visibility for diff objects and players
    // reserved for future version
    LOG_FILTER_WEATHER            = 0x000010,               //  4 weather changes
    LOG_FILTER_PLAYER_STATS       = 0x000020,               //  5 player save data
    LOG_FILTER_SQL_TEXT           = 0x000040,               //  6 raw SQL text send to DB engine
    LOG_FILTER_PLAYER_MOVES       = 0x000080,               //  7 player moves by grid/cell
    LOG_FILTER_PERIODIC_AFFECTS   = 0x000100,               //  8 DoT/HoT apply trace
    LOG_FILTER_AI_AND_MOVEGENSS   = 0x000200,               //  9 AI/movement generators debug output
    LOG_FILTER_DAMAGE             = 0x000400,               // 10 Direct/Area damage trace
    LOG_FILTER_COMBAT             = 0x000800,               // 11 attack states/roll attack results/etc
    LOG_FILTER_SPELL_CAST         = 0x001000,               // 12 spell cast/aura apply/spell proc events
    LOG_FILTER_DB_STRICTED_CHECK  = 0x002000,               // 13 stricted DB data checks output (with possible false reports) for DB devs
    LOG_FILTER_AHBOT_SELLER       = 0x004000,               // 14 Auction House Bot seller part
    LOG_FILTER_AHBOT_BUYER        = 0x008000,               // 15 Auction House Bot buyer part
    LOG_FILTER_PATHFINDING        = 0x010000,               // 16 Pathfinding
    LOG_FILTER_MAP_LOADING        = 0x020000,               // 17 Map loading/unloading (MAP, VMAPS, MMAP)
    LOG_FILTER_EVENT_AI_DEV       = 0x040000,               // 18 Event AI actions
    LOG_FILTER_CELL_ENVELOPE      = 0x080000,               // 19 LivingWorld B-Cell envelope load/accrete trace
    LOG_FILTER_GRID_ADD           = 0x100000,               // 20 object added to a grid cell ("X enters grid[x,y]") - high-volume, mostly creatures
    LOG_FILTER_DB_SCRIPTS         = 0x200000,               // 21 db_scripts command processing trace (execution, not errors)
};

#define LOG_FILTER_COUNT            22

/**
 * @brief Configuration data for individual log filters
 *
 * Defines the display name, configuration file parameter name, and default
 * state for each log filter. This structure is used to manage filter settings
 * from configuration files.
 */
struct LogFilterData
{
    char const* name; /**< Display name for the log filter */
    char const* configName; /**< Configuration file parameter name */
    bool defaultState; /**< Default enabled/disabled state */
};

extern LogFilterData logFilterData[LOG_FILTER_COUNT]; /**< Array of log filter configuration data **/

/**
 * @brief Console text color enumeration
 *
 * Defines the available colors for console output on supported platforms.
 * Used to colorize log messages for improved readability in terminal windows.
 */
enum Color
{
    BLACK,    /**< Black text */
    RED,      /**< Red text */
    GREEN,    /**< Green text */
    BROWN,    /**< Brown/Yellow text */
    BLUE,     /**< Blue text */
    MAGENTA,  /**< Magenta text */
    CYAN,     /**< Cyan text */
    GREY,     /**< Grey text */
    YELLOW,   /**< Bright yellow text */
    LRED,     /**< Light red text */
    LGREEN,   /**< Light green text */
    LBLUE,    /**< Light blue text */
    LMAGENTA, /**< Light magenta text */
    LCYAN,    /**< Light cyan text */
    WHITE     /**< White text */
};

const int Color_count = int(WHITE) + 1; /**< Total number of available colors **/

/**
 * @brief One formatted console line handed to the off-thread writer
 *
 * Producers format text (time prefix + body, WITHOUT the trailing newline) and
 * pick the target stream/color; the writer thread renders this record and
 * appends the newline after ResetColor (terminator outside the color span).
 */
struct ConsoleLogRecord
{
    std::string text; /**< Formatted line WITHOUT the trailing newline; the writer appends '\n' after ResetColor */
    Color color; /**< Color to apply when applyColor is set */
    bool applyColor; /**< Whether to wrap the write in SetColor/ResetColor */
    bool toStdout; /**< true => stdout, false => stderr */
    bool isRaw; /**< Raw passthrough: write text verbatim with NO color and NO appended newline (used for progress-bar redraws, which carry their own '\r'/'\n' and must not be reformatted) */

    ConsoleLogRecord() : color(WHITE), applyColor(false), toStdout(true), isRaw(false) {}
};

/**
 * @brief Singleton log manager for server-wide logging
 *
 * Log provides thread-safe, singleton-based logging functionality for the MaNGOS server.
 * It manages multiple log files (standard, GM, character, debug) and provides various
 * logging methods with filtering capabilities. Supports different log levels and selective
 * output based on event type bitmasks.
 *
 * Features:
 * - Multiple output files (main, GM commands, character actions, debug)
 * - Configurable log levels and filters
 * - Thread-safe singleton implementation
 * - Console color support for improved readability
 * - Formatted output with timestamps
 */
class Log : public MaNGOS::Singleton<Log>
{
    friend class MaNGOS::Singleton<Log>;

    /**
     * @brief Constructs the Log singleton instance
     *
     * Initializes all log file handles and filter settings.
     */
    Log();

    /**
     * @brief Destructs the Log instance and closes all open files
     *
     * Ensures all log files are properly closed and file handles are cleaned up.
     */
    ~Log()
    {
        CloseLogFiles();
    }
    public:
        /**
         * @brief
         *
         */
        void Initialize();

        /**
         * @brief
         *
         * @param init_str
         */
        void InitColors(const std::string& init_str);

        /**
         * @brief
         *
         * @param account
         * @param str...
         */
        void outCommand(uint32 account, const char* str, ...) ATTR_PRINTF(3, 4);

        /**
         * @brief any log level
         *
         */
        void outString();

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outString(const char* str, ...)      ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         * @param err...
         */
        void outError(const char* err, ...)       ATTR_PRINTF(2, 3);

        /**
         * @brief log level >= 1
         *
         * @param str...
         */
        void outBasic(const char* str, ...)       ATTR_PRINTF(2, 3);

        /**
         * @brief log level >= 2
         *
         * @param str...
         */
        void outDetail(const char* str, ...)      ATTR_PRINTF(2, 3);

        /**
         * @brief log level >= 3
         *
         * @param str...
         */
        void outDebug(const char* str, ...)       ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         */
        void outErrorDb();

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outErrorDb(const char* str, ...)     ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outChar(const char* str, ...)        ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outErrorEluna();

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outErrorEluna(const char* str, ...)        ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         */
        void outErrorEventAI();

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outErrorEventAI(const char* str, ...)      ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         */
        void outErrorScriptLib();

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outErrorScriptLib(const char* str, ...)     ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         * @param socket
         * @param opcode
         * @param opcodeName
         * @param packet
         * @param incoming
         */
        void outWorldPacketDump(uint32 socket, uint32 opcode, char const* opcodeName, ByteBuffer const* packet, bool incoming);

        /**
         * @brief any log level
         *
         * @param str
         * @param account_id
         * @param guid
         * @param name
         */
        void outCharDump(const char* str, uint32 account_id, uint32 guid, const char* name);

        /**
         * @brief
         *
         * @param str...
         */
        void outRALog(const char* str, ...)       ATTR_PRINTF(2, 3);

        /**
         * @brief any log level
         *
         */
        void outWarden();

        /**
         * @brief any log level
         *
         * @param str...
         */
        void outWarden(const char* str, ...)      ATTR_PRINTF(2, 3);

        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetLogLevel() const { return m_logLevel; }

        /**
         * @brief
         *
         * @param Level
         */
        void SetLogLevel(char* Level);

        /**
         * @brief
         *
         * @param Level
         */
        void SetLogFileLevel(char* Level);

        /**
         * @brief
         *
         * @param stdout_stream
         * @param color
         */
        static void SetColor(bool stdout_stream, Color color);

        /**
         * @brief
         *
         * @param stdout_stream
         */
        static void ResetColor(bool stdout_stream);

        /**
         * @brief
         *
         */
        void outTime();

        /**
         * @brief
         *
         * @param file
         */
        static void outTimestamp(FILE* file);

        /**
         * @brief
         *
         * @return std::string
         */
        static std::string GetTimestampStr();

        /**
         * @brief
         *
         * @param filter
         * @return bool
         */
        bool HasLogFilter(uint32 filter) const { return m_logFilter & filter; }

        /**
         * @brief
         *
         * @param filter
         * @param on
         */
        void SetLogFilter(LogFilters filter, bool on) { if (on) { m_logFilter |= filter; } else { m_logFilter &= ~filter; } }

        /**
         * @brief
         *
         * @param loglvl
         * @return bool
         */
        bool HasLogLevelOrHigher(LogLevel loglvl) const { return m_logLevel >= loglvl || (m_logFileLevel >= loglvl && logfile); }

        /**
         * @brief Flush buffered file log output to the OS. Called periodically
         *        from the world tick and once at shutdown. The file sinks are
         *        fully buffered (setvbuf), so this bounds how long a buffered
         *        line waits to reach disk. Best-effort: error paths still flush
         *        immediately at emit time.
         */
        void Flush();

        /**
         * @brief Start the off-thread console writer
         *
         * Spawns the dedicated thread that performs the SetColor + write +
         * ResetColor for console output, so the world/map-update threads no
         * longer stall on the per-call console flush. Idempotent: a second
         * call (realmd double-init) is a no-op. Must be called after
         * Initialize() has applied config (InitColors), and before the world
         * threads start producing console lines.
         */
        void StartConsoleThread();

        /**
         * @brief Stop and join the off-thread console writer
         *
         * Switches console emit back to the synchronous fallback, signals the
         * writer to stop, and joins it (the writer performs a final drain
         * before returning). Safe to call when the thread was never started.
         */
        void StopConsoleThread();

        /**
         * @brief Emit raw bytes to the console verbatim (no time prefix, no
         *        color, no appended newline), routed through the off-thread
         *        writer when it is running or written synchronously otherwise.
         *
         * This is the single funnel for every piece of console output that is
         * NOT a normal log line: progress-bar redraws (which carry their own
         * '\r'/'\n'), the interactive CLI prompt, CLI command output, and the
         * loglevel-change notices. Routing them all through the one writer queue
         * keeps stdout single-owner, so none of them can tear against -- or
         * overtake -- each other or the writer's log lines; everything drains in
         * FIFO (program) order. Without this, a synchronous direct-stdout write
         * (e.g. the prompt reprinted after a .reload) would jump ahead of bar
         * frames still sitting in the queue.
         *
         * @param bytes the exact bytes to write to stdout
         */
        void ConsoleEmitRaw(const std::string& bytes);

        /**
         * @brief Console-side line filter, applied to a formatted log line before
         *        it reaches the console. Returning true consumes the line: the
         *        console does not show it, the file log still records it.
         *
         * The startup UI installs one for the duration of world initialization so
         * it can fold the ">> Loaded N rows" result lines into the step they
         * belong to, and drop the blank spacer lines that would break an in-place
         * repaint. It is uninstalled once the world is up, so no runtime log line
         * pays for it. Never applied to errors, which must always be visible.
         *
         * @param text the formatted line (no trailing newline), never NULL
         * @return true to suppress the line on the console
         */
        typedef bool (*ConsoleLineFilter)(const char* text);

        /**
         * @brief Install (or, with NULL, remove) the console line filter.
         */
        void SetConsoleLineFilter(ConsoleLineFilter filter) { m_consoleFilter = filter; }

        /**
         * @brief Record whether the console cursor is parked mid-line.
         *
         * Set by whoever writes bytes that do not end in a newline -- a progress
         * bar redraw or a startup-UI line that will be repainted in place. The
         * console writer consults it before emitting an ordinary log line and, if
         * the line is dirty, wipes it first (see ClearConsoleLine) so the log line
         * cannot land on top of the half-drawn one.
         */
        static void MarkConsoleLineDirty(bool dirty);

        /**
         * @brief Return the cursor to a clean column 0, erasing whatever the
         *        in-place line had drawn there, and clear the dirty flag. No-op
         *        when the line is not dirty.
         */
        static void ClearConsoleLine(FILE* out);

        /**
         * @brief Whether world packet logging is active. Gated by the
         *        PacketLoggingEnabled config flag at startup: worldLogfile is
         *        only opened when the flag is set, so this is the single source
         *        of truth and is off by default even on legacy configs.
         * @return bool
         */
        bool IsPacketLoggingEnabled() const { return worldLogfile != NULL; }

        /**
         * @brief
         *
         * @return bool
         */
        bool IsOutCharDump() const { return m_charLog_Dump; }

        /**
         * @brief
         *
         * @return bool
         */
        bool IsIncludeTime() const { return m_includeTime; }

        /**
         * @brief
         *
         */
        static void WaitBeforeContinueIfNeed();

        /**
         * @brief Set filename for scriptlibrary error output
         *
         * @param fname
         * @param libName
         */
        void setScriptLibraryErrorFile(char const* fname, char const* libName);

    private:
        /**
         * @brief
         *
         * @param configFileName
         * @param configTimeStampFlag
         * @param mode
         * @return FILE
         */
        FILE* openLogFile(char const* configFileName, char const* configTimeStampFlag, char const* mode);

        /**
         * @brief
         *
         * @param account
         * @return FILE
         */
        FILE* openGmlogPerAccount(uint32 account);

        /**
         * @brief Closes all open log file handles. Used by the destructor and
         *        by Initialize() to make re-initialization idempotent (realmd
         *        calls Initialize() a second time after loading its config).
         */
        void CloseLogFiles();

        /**
         * @brief Format one console line and route it to the writer thread
         *        (async) or emit it inline (synchronous fallback). Builds the
         *        time prefix + body + newline; the caller supplies stream,
         *        color and whether color applies.
         *
         * @param toStdout true => stdout, false => stderr
         * @param color
         * @param applyColor
         * @param fmt
         * @param ap
         */
        void ConsoleEmit(bool toStdout, Color color, bool applyColor, const char* fmt, va_list* ap);

        /// Emit a blank console line (time prefix + newline) via the writer / fallback.
        void ConsoleEmitBlank(bool toStdout);

        /**
         * @brief Build the "HH:MM:SS " console time prefix, or an empty string
         *        when LogTime is disabled. Mirrors outTime().
         *
         * @return std::string
         */
        std::string ConsoleTimePrefix() const;

        FILE* raLogfile; /**< TODO */
        FILE* logfile; /**< TODO */
        FILE* gmLogfile; /**< TODO */
        FILE* charLogfile; /**< TODO */
        FILE* dberLogfile; /**< TODO */
#ifdef ENABLE_ELUNA
        FILE* elunaErrLogfile; /**< TODO */
#endif /* ENABLE_ELUNA */

        FILE* eventAiErLogfile; /**< TODO */
        FILE* scriptErrLogFile; /**< TODO */
        FILE* worldLogfile; /**< TODO */
        FILE* wardenLogfile; /**< TODO */
        std::mutex m_worldLogMtx; /**< Serializes packet-dump writes to worldLogfile */
        std::mutex m_fileMtx; /**< Serializes writes to the main logfile so concurrent map-update worker threads cannot tear lines */

        ConsoleLogWriter* m_consoleBody; /**< Off-thread console writer Runnable (owned via thread refcount) */
        MaNGOS::Thread* m_consoleThread; /**< Thread driving m_consoleBody; deleting it drops the Runnable refcount */
        bool m_consoleAsync; /**< When true, console emits route to the writer thread; otherwise synchronous fallback */
        ConsoleLineFilter m_consoleFilter; /**< Optional console-side line filter (startup UI); NULL once the world is up */

        LogLevel m_logLevel; /**< log/console control */
        LogLevel m_logFileLevel; /**< TODO */
        bool m_colored; /**< TODO */
        bool m_includeTime; /**< TODO */
        Color m_colors[4]; /**< TODO */
        uint32 m_logFilter; /**< TODO */

        // cache values for after initilization use (like gm log per account case)
        std::string m_logsDir; /**< TODO */
        std::string m_logsTimestamp; /**< TODO */

        // char log control
        bool m_charLog_Dump; /**< TODO */

        // gm log control
        bool m_gmlog_per_account; /**< TODO */
        std::string m_gmlog_filename_format; /**< TODO */

        char const* m_scriptLibName; /**< TODO */
};

#define sLog MaNGOS::Singleton<Log>::Instance()

#define BASIC_LOG(...)                              \
do                                                  \
{                                                   \
    if (sLog.HasLogLevelOrHigher(LOG_LVL_BASIC))    \
    {                                               \
        sLog.outBasic(__VA_ARGS__);                 \
    }                                               \
} while (0)

#define BASIC_FILTER_LOG(F,...)                     \
do                                                  \
{                                                   \
    if (sLog.HasLogLevelOrHigher(LOG_LVL_BASIC) && !sLog.HasLogFilter(F)) \
    {                                               \
        sLog.outBasic(__VA_ARGS__);                 \
    }                                               \
} while (0)

#define DETAIL_LOG(...)                             \
do                                                  \
{                                                   \
    if (sLog.HasLogLevelOrHigher(LOG_LVL_DETAIL))   \
    {                                               \
        sLog.outDetail(__VA_ARGS__);                \
    }                                               \
}                                                   \
while (0)

#define DETAIL_FILTER_LOG(F,...)                    \
do                                                  \
{                                                   \
    if (sLog.HasLogLevelOrHigher(LOG_LVL_DETAIL) && !sLog.HasLogFilter(F)) \
    {                                               \
        sLog.outDetail(__VA_ARGS__);                \
    }                                               \
}                                                   \
while (0)

#define DEBUG_LOG(...)                              \
do                                                  \
{                                                   \
    if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))    \
    {                                               \
        sLog.outDebug(__VA_ARGS__);                 \
    }                                               \
}                                                   \
while (0)

#define DEBUG_FILTER_LOG(F,...)                     \
do                                                  \
{                                                   \
    if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) && !sLog.HasLogFilter(F)) \
    {                                               \
        sLog.outDebug(__VA_ARGS__);                 \
    }                                               \
}                                                   \
while (0)

#define ERROR_DB_FILTER_LOG(F,...)                  \
do                                                  \
{                                                   \
    if (!sLog.HasLogFilter(F))                      \
    {                                               \
        sLog.outErrorDb(__VA_ARGS__);               \
    }                                               \
}                                                   \
while (0)

#define ERROR_DB_STRICT_LOG(...) \
ERROR_DB_FILTER_LOG(LOG_FILTER_DB_STRICTED_CHECK, __VA_ARGS__)

/**
 * @brief primary for script library
 *
 * @param str...
 */
void  outstring_log(const char* str, ...) ATTR_PRINTF(1, 2);

/**
 * @brief
 *
 * @param str...
 */
void  detail_log(const char* str, ...) ATTR_PRINTF(1, 2);

/**
 * @brief
 *
 * @param str...
 */
void  debug_log(const char* str, ...) ATTR_PRINTF(1, 2);

/**
 * @brief
 *
 * @param str...
 */
void  error_log(const char* str, ...) ATTR_PRINTF(1, 2);

/**
 * @brief
 *
 * @param str...
 */
void  error_db_log(const char* str, ...) ATTR_PRINTF(1, 2);

/**
 * @brief
 *
 * @param fname
 * @param libName
 */
void  setScriptLibraryErrorFile(char const* fname, char const* libName);

/**
 * @brief
 *
 * @param str...
 */
void  script_error_log(const char* str, ...) ATTR_PRINTF(1, 2);

#endif
