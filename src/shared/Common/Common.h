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

#ifndef MANGOSSERVER_COMMON_H
#define MANGOSSERVER_COMMON_H

// config.h needs to be included 1st
#ifdef HAVE_CONFIG_H
#ifdef PACKAGE
#undef PACKAGE
#endif // PACKAGE

#ifdef PACKAGE_BUGREPORT
#undef PACKAGE_BUGREPORT
#endif // PACKAGE_BUGREPORT

#ifdef PACKAGE_NAME
#undef PACKAGE_NAME
#endif // PACKAGE_NAME

#ifdef PACKAGE_STRING
#undef PACKAGE_STRING
#endif // PACKAGE_STRING

#ifdef PACKAGE_TARNAME
#undef PACKAGE_TARNAME
#endif // PACKAGE_TARNAME

#ifdef PACKAGE_VERSION
#undef PACKAGE_VERSION
#endif // PACKAGE_VERSION

#ifdef VERSION
#undef VERSION
#endif // VERSION

#undef PACKAGE
#undef PACKAGE_BUGREPORT
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef VERSION
#endif // HAVE_CONFIG_H

#include "Platform/Define.h"

#if COMPILER == COMPILER_MICROSOFT
#  pragma warning(disable:4996)                             // 'function': was declared deprecated
#ifndef __SHOW_STUPID_WARNINGS__
#  pragma warning(disable:4244)                             // 'argument' : conversion from 'type1' to 'type2', possible loss of data
#  pragma warning(disable:4355)                             // 'this' : used in base member initializer list
#endif                                                      // __SHOW_STUPID_WARNINGS__

#endif                                                      // __GNUC__

#include "Utilities/UnorderedMapSet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <signal.h>
#include <assert.h>
#include "ServerDefines.h"

#if defined(__sun__)
#include <ieeefp.h> // finite() on Solaris
#endif

// Containers and utilities that many translation units rely on. Common.h names them
// explicitly, so dropping one is a visible change rather than a silent break.
#include <array>
#include <functional>
#include <memory>
#include <set>
#include <list>
#include <string>
#include <map>
#include <queue>
#include <sstream>
#include <algorithm>
#include <thread>
#include <atomic>
#include <utility>
#include <vector>
#include <cinttypes>
#include <shared_mutex>

#include "Utilities/Errors.h"
#include "LockedQueue/LockedQueue.h"
#include "Threading/Threading.h"

// The POSIX block below is deliberately explicit: much of the tree depends on these
// system headers arriving through Common.h, so anything it does not name here is not
// declared -- name it.
#if PLATFORM == PLATFORM_WINDOWS
#  if !defined (FD_SETSIZE)
#    define FD_SETSIZE 4096
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <sys/types.h>
#  include <sys/ioctl.h>
#  include <sys/socket.h>
#  include <sys/stat.h>
#  include <sys/time.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
#  include <netdb.h>
#  include <strings.h>   // strcasecmp/strncasecmp, for the stricmp/strnicmp aliases below
#endif

#if COMPILER == COMPILER_MICROSOFT
#  include <float.h>
#else
#  define stricmp strcasecmp
#  define strnicmp strncasecmp
#endif

// 64-bit printf specifiers and literals. <cinttypes> gives us these portably, so the
// per-compiler ladder these used to need is gone.
#define I32FMT   "%08" PRIX32
#define I64FMT   "%016" PRIX64
#define UI64FMTD "%" PRIu64
#define SI64FMTD "%" PRId64

#define UI64LIT(N) UINT64_C(N)
#define SI64LIT(N) INT64_C(N)

/**
 * @brief
 *
 * @param f
 * @return float
 */
inline float finiteAlways(float f) { return std::isfinite(f) ? f : 0.0f; }

#define atol(a) strtoul( a, NULL, 10)

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
#if !defined(CLASSIC)
    LOCALE_ruRU = 8,
#endif
};

#if defined(CLASSIC)
#define MAX_LOCALE 8
#else
#define MAX_LOCALE 9
#endif
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

// we always use stdlibc++ std::max/std::min, undefine some not C++ standard defines (Win API and some pother platforms)
#ifdef max
#  undef max
#endif

#ifdef min
#  undef min
#endif

#ifndef M_PI
#  define M_PI          3.14159265358979323846
#endif

#ifndef M_PI_F
#  define M_PI_F        float(M_PI)
#endif

#ifndef countof
#define countof(array) (sizeof(array) / sizeof((array)[0]))
#endif

#endif
