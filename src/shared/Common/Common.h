/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2023 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOSSERVER_COMMON_H
#define MANGOSSERVER_COMMON_H

#include "Platform/Define.h"
#include "Utilities/UnorderedMapSet.h"
#include "ServerDefines.h"
#include "Utilities/Errors.h"
#include "LockedQueue/LockedQueue.h"
#include "Threading/Threading.h"

#include <ace/Basic_Types.h>
#include <ace/Guard_T.h>
#include <ace/RW_Thread_Mutex.h>
#include <ace/Thread_Mutex.h>
#include <ace/OS_NS_arpa_inet.h>

#include <cinttypes>
#include <set>
#include <list>
#include <string>
#include <map>
#include <queue>
#include <sstream>
#include <algorithm>

//TODO: Maybe unify all constants, enums and macros used global-wide here.
//      SharedDefines.h, ServerDefines.h, Define.h, CompilerDefs and other atrocities...really necessary?

//For some obscure reason, Eluna will not compile without
#define PLATFORM_WINDOWS 1
#define PLATFORM_OTHER   0

#ifdef _WIN32
#  define PLATFORM PLATFORM_WINDOWS
#else
# define PLATFORM PLATFORM_OTHER
#endif
// For Eluna only, until fixed



#define SI64LIT(N) ACE_INT64_LITERAL(N)
#define UI64LIT(N) ACE_UINT64_LITERAL(N)

#define UI64FMTD "%" PRIu64
#define I64FMTD "%" PRIi64
#define UI32FMTD "%" PRIu32
#define I32FMTD "%" PRIi32

#define SI64FMTD "%" SCNi64

#define SIZEFMTD "%zu"
#define STRINGIZE(a) #a


// used for creating values for respawn for example
#define MAKE_PAIR64(l, h)  uint64( uint32(l) | ( uint64(h) << 32 ) )
#define PAIR64_HIPART(x)   (uint32)((uint64(x) >> 32) & UI64LIT(0x00000000FFFFFFFF))
#define PAIR64_LOPART(x)   (uint32)(uint64(x)         & UI64LIT(0x00000000FFFFFFFF))

#define MAKE_PAIR32(l, h)  uint32( uint16(l) | ( uint32(h) << 16 ) )
#define PAIR32_HIPART(x)   (uint16)((uint32(x) >> 16) & 0x0000FFFF)
#define PAIR32_LOPART(x)   (uint16)(uint32(x)         & 0x0000FFFF)

/**
 * @brief
 *
 */
enum TimeConstants
{
    MINUTE = 60,
    HOUR   = MINUTE * 60,
    DAY    = HOUR * 24,
    WEEK   = DAY * 7,
    MONTH  = DAY * 30,
    YEAR   = MONTH * 12,
    IN_MILLISECONDS = 1000
};

/**
 * @brief
 *
 */
enum LocaleConstant
{
    LOCALE_enUS = 0,                                        // also enGB
    LOCALE_koKR = 1,
    LOCALE_frFR = 2,
    LOCALE_deDE = 3,
    LOCALE_zhCN = 4,
    LOCALE_zhTW = 5,
    LOCALE_esES = 6,
    LOCALE_esMX = 7,

};

#define MAX_LOCALE 8
#define DEFAULT_LOCALE LOCALE_enUS

/**
 * @brief
 *
 * @param name
 * @return LocaleConstant
 */
LocaleConstant GetLocaleByName(const std::string& name);

typedef std::vector<std::string> StringVector;

extern char const* localeNames[MAX_LOCALE]; /**< TODO */

/**
 * @brief
 *
 */
struct LocaleNameStr
{
    char const* name; /**< TODO */
    LocaleConstant locale; /**< TODO */
};

extern LocaleNameStr const fullLocaleNameList[]; /**< used for iterate all names including alternative */

/**
 * @brief operator new[] based version of strdup() function! Release memory by using operator delete[] !
 *
 * @param source
 * @return char
 */
inline char* mangos_strdup(const char* source)
{
    char* dest = new char[strlen(source) + 1];
    strcpy(dest, source);
    return dest;
}

#ifndef M_PI
#  define M_PI          3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F        float(M_PI)
#endif

#ifndef countof
#define countof(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifdef max
#  undef max
#endif

#ifdef min
#  undef min
#endif


#endif
