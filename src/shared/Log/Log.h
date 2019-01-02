/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

class Config;
class ByteBuffer;

/**
 * @brief various levels for logging
 *
 */
enum LogLevel
{
    LOG_LVL_MINIMAL = 0,
    LOG_LVL_BASIC   = 1,
    LOG_LVL_DETAIL  = 2,
    LOG_LVL_DEBUG   = 3
};

/**
 * @brief bitmask (not forgot update logFilterData content)
 *
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
    LOG_FILTER_EVENT_AI_DEV       = 0x040000                // 18 Event AI actions
};

#define LOG_FILTER_COUNT            19

/**
 * @brief
 *
 */
struct LogFilterData
{
    char const* name; /**< TODO */
    char const* configName; /**< TODO */
    bool defaultState; /**< TODO */
};

extern LogFilterData logFilterData[LOG_FILTER_COUNT]; /**< TODO */

/**
 * @brief
 *
 */
enum Color
{
    BLACK,
    RED,
    GREEN,
    BROWN,
    BLUE,
    MAGENTA,
    CYAN,
    GREY,
    YELLOW,
    LRED,
    LGREEN,
    LBLUE,
    LMAGENTA,
    LCYAN,
    WHITE
};

const int Color_count = int(WHITE) + 1; /**< TODO */

/**
 * @brief
 *
 */
class Log : public MaNGOS::Singleton<Log, MaNGOS::ClassLevelLockable<Log, ACE_Thread_Mutex> >
{
        friend class MaNGOS::OperatorNew<Log>;
        /**
         * @brief
         *
         */
        Log();

        /**
         * @brief
         *
         */
        ~Log()
        {
            if (logfile != NULL)
                { fclose(logfile); }
            logfile = NULL;

            if (gmLogfile != NULL)
                { fclose(gmLogfile); }
            gmLogfile = NULL;

            if (charLogfile != NULL)
                { fclose(charLogfile); }
            charLogfile = NULL;

            if (dberLogfile != NULL)
                { fclose(dberLogfile); }
            dberLogfile = NULL;

#ifdef ENABLE_ELUNA
            if (elunaErrLogfile != NULL)
                fclose(elunaErrLogfile);
            elunaErrLogfile = NULL;
#endif /* ENABLE_ELUNA */

            if (eventAiErLogfile != NULL)
                { fclose(eventAiErLogfile); }
            eventAiErLogfile = NULL;

            if (scriptErrLogFile != NULL)
                { fclose(scriptErrLogFile); }
            scriptErrLogFile = NULL;

            if (raLogfile != NULL)
                { fclose(raLogfile); }
            raLogfile = NULL;

            if (worldLogfile != NULL)
                { fclose(worldLogfile); }
            worldLogfile = NULL;

            if (wardenLogfile != NULL)
            {
                fclose(wardenLogfile);
            }
            wardenLogfile = NULL;
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
        void SetColor(bool stdout_stream, Color color);
        /**
         * @brief
         *
         * @param stdout_stream
         */
        void ResetColor(bool stdout_stream);
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
        ACE_Thread_Mutex m_worldLogMtx; /**< TODO */

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

#define BASIC_LOG(...)                                  \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_BASIC))    \
            sLog.outBasic(__VA_ARGS__);                 \
    } while(0)

#define BASIC_FILTER_LOG(F,...)                         \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_BASIC) && !sLog.HasLogFilter(F)) \
            sLog.outBasic(__VA_ARGS__);                 \
    } while(0)

#define DETAIL_LOG(...)                                 \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DETAIL))   \
            sLog.outDetail(__VA_ARGS__);                \
    } while(0)

#define DETAIL_FILTER_LOG(F,...)                        \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DETAIL) && !sLog.HasLogFilter(F)) \
            sLog.outDetail(__VA_ARGS__);                \
    } while(0)

#define DEBUG_LOG(...)                                  \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))    \
            sLog.outDebug(__VA_ARGS__);                 \
    } while(0)

#define DEBUG_FILTER_LOG(F,...)                         \
    do {                                                \
        if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG) && !sLog.HasLogFilter(F)) \
            sLog.outDebug(__VA_ARGS__);                 \
    } while(0)

#define ERROR_DB_FILTER_LOG(F,...)                      \
    do {                                                \
        if (!sLog.HasLogFilter(F))                      \
            sLog.outErrorDb(__VA_ARGS__);               \
    } while(0)

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
