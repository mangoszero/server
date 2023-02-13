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

#ifndef MANGOS_DEFINE_H
#define MANGOS_DEFINE_H

//TODO: it's exhausting to have so many constants and macros scattered all over the place
//      exercise: where is MANGOS_ASSERT defined? How about static_assert? :)

#include "ace/Basic_Types.h"
#include "ace/Default_Constants.h"


#define MANGOS_LITTLEENDIAN 0
#define MANGOS_BIGENDIAN    1

#if !defined(MANGOS_ENDIAN)
#  if defined (ACE_BIG_ENDIAN)
#    define MANGOS_ENDIAN MANGOS_BIGENDIAN
#  else // ACE_BYTE_ORDER != ACE_BIG_ENDIAN
#    define MANGOS_ENDIAN MANGOS_LITTLEENDIAN
#  endif // ACE_BYTE_ORDER
#endif // MANGOS_ENDIAN


#define MANGOS_PATH_MAX PATH_MAX                            // ace/os_include/os_limits.h -> ace/Basic_Types.h

#ifndef DECLSPEC_NORETURN
#  ifdef _WIN32
#    define DECLSPEC_NORETURN __declspec(noreturn)
#  else
#    define DECLSPEC_NORETURN
#  endif
#endif

#if defined(_MSC_VER)
#  define ATTR_NORETURN
#  define ATTR_PRINTF(F, V)
#  define ATTR_DEPRECATED
#else
#ifndef __has_attribute
#define _has_attribute(X) 0
#endif

#if __has_attribute(noreturn)
#  define ATTR_NORETURN __attribute__((noreturn))
#else
#  define ATTR_NORETURN
#endif

#if __has_attribute(format)
#  define ATTR_PRINTF(F, V) __attribute__ ((format (printf, F, V)))
#else
#  define ATTR_PRINTF(F, V)
#endif

#if __has_attribute(deprecated)
#  define ATTR_DEPRECATED __attribute__((deprecated))
#else
#  define ATTR_DEPRECATED
#endif
#endif

typedef ACE_INT64  int64;
typedef ACE_INT32  int32;
typedef ACE_INT16  int16;
typedef ACE_INT8   int8;
typedef ACE_UINT64 uint64;
typedef ACE_UINT32 uint32;
typedef ACE_UINT16 uint16;
typedef ACE_UINT8  uint8;

#ifndef _WIN32
typedef uint16      WORD;
typedef uint32      DWORD;
#endif

typedef uint64 OBJECT_HANDLE;

#define CONCAT(x, y) CONCAT1(x, y)
#define CONCAT1(x, y) x##y
#define STATIC_ASSERT_WORKAROUND(expr, msg) typedef char CONCAT(static_assert_failed_at_line_, __LINE__) [(expr) ? 1 : -1]

#ifndef __cpp_static_assert
#  define static_assert(a, b) STATIC_ASSERT_WORKAROUND(a, b)
#endif


#endif // MANGOS_DEFINE_H
