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

#ifndef MANGOS_DEFINE_H
#define MANGOS_DEFINE_H

#include <sys/types.h>

#include <ace/Basic_Types.h>
#include <ace/Default_Constants.h>
#include <ace/OS_NS_dlfcn.h>
#include <ace/ACE_export.h>

#include "Platform/CompilerDefs.h"

#define MANGOS_LITTLEENDIAN 0
#define MANGOS_BIGENDIAN    1

#if !defined(MANGOS_ENDIAN)
#  if defined (ACE_BIG_ENDIAN)
#    define MANGOS_ENDIAN MANGOS_BIGENDIAN
#  else // ACE_BYTE_ORDER != ACE_BIG_ENDIAN
#    define MANGOS_ENDIAN MANGOS_LITTLEENDIAN
#  endif // ACE_BYTE_ORDER
#endif // MANGOS_ENDIAN

/**
 * @brief
 *
 */
typedef ACE_SHLIB_HANDLE MANGOS_LIBRARY_HANDLE;

#define MANGOS_SCRIPT_NAME "mangosscript"
#define MANGOS_SCRIPT_SUFFIX ACE_DLL_SUFFIX
#define MANGOS_SCRIPT_PREFIX ACE_DLL_PREFIX
#define MANGOS_LOAD_LIBRARY(libname)    ACE_OS::dlopen(libname)
#define MANGOS_CLOSE_LIBRARY(hlib)      ACE_OS::dlclose(hlib)
#define MANGOS_GET_PROC_ADDR(hlib,name) ACE_OS::dlsym(hlib,name)

#define MANGOS_PATH_MAX PATH_MAX                            // ace/os_include/os_limits.h -> ace/Basic_Types.h

#if PLATFORM == PLATFORM_WINDOWS
#  define MANGOS_EXPORT __declspec(dllexport)
#  define MANGOS_IMPORT __cdecl
#else // PLATFORM != PLATFORM_WINDOWS
#  define MANGOS_EXPORT export
#  if defined(__APPLE_CC__) && defined(BIG_ENDIAN)
#    if (defined(__ppc__) || defined(__powerpc__))
#      define MANGOS_IMPORT __attribute__ ((longcall))
#    else
#      define MANGOS_IMPORT
#    endif
#  elif defined(__x86_64__)
#    define MANGOS_IMPORT
#  else
#    define MANGOS_IMPORT __attribute__ ((cdecl))
#  endif //__APPLE_CC__ && BIG_ENDIAN
#endif // PLATFORM

#if PLATFORM == PLATFORM_WINDOWS
#  define MANGOS_DLL_SPEC __declspec(dllexport)
#  ifndef DECLSPEC_NORETURN
#    define DECLSPEC_NORETURN __declspec(noreturn)
#  endif // DECLSPEC_NORETURN
#else // PLATFORM != PLATFORM_WINDOWS
#  define MANGOS_DLL_SPEC
#  define DECLSPEC_NORETURN
#endif // PLATFORM

#if !defined(DEBUG)
#  define MANGOS_INLINE inline
#else // DEBUG
#  if !defined(MANGOS_DEBUG)
#    define MANGOS_DEBUG
#  endif // MANGOS_DEBUG
#  define MANGOS_INLINE
#endif //!DEBUG

#if COMPILER == COMPILER_GNU || COMPILER == COMPILER_CLANG
#  define ATTR_NORETURN __attribute__((noreturn))
#  define ATTR_PRINTF(F, V) __attribute__ ((format (printf, F, V)))
#  define ATTR_DEPRECATED __attribute__((deprecated))
#else //COMPILER != COMPILER_GNU
#  define ATTR_NORETURN
#  define ATTR_PRINTF(F, V)
#  define ATTR_DEPRECATED
#endif //COMPILER == COMPILER_GNU

#if COMPILER_HAS_CPP11_SUPPORT
#  define OVERRIDE override
#  define FINAL final
#else
#  define OVERRIDE
#  define FINAL
#endif //COMPILER_HAS_CPP11_SUPPORT

/**
 * @brief A signed integer of 64 bits
 *
 */
typedef ACE_INT64 int64;
/**
 * @brief A signed integer of 32 bits
 *
 */
typedef ACE_INT32 int32;
/**
 * @brief A signed integer of 16 bits
 *
 */
typedef ACE_INT16 int16;
/**
 * @brief A signed integer of 8 bits
 *
 */
typedef ACE_INT8 int8;
/**
 * @brief An unsigned integer of 64 bits
 *
 */
typedef ACE_UINT64 uint64;
/**
 * @brief An unsigned integer of 32 bits
 *
 */
typedef ACE_UINT32 uint32;
/**
 * @brief An unsigned integer of 16 bits
 *
 */
typedef ACE_UINT16 uint16;
/**
 * @brief An unsigned integer of 8 bits
 *
 */
typedef ACE_UINT8 uint8;

#if COMPILER != COMPILER_MICROSOFT
/**
 * @brief An unsigned integer of 16 bits, only for Win
 *
 */
typedef uint16      WORD;
/**
 * @brief An unsigned integer of 32 bits, only for Win
 *
 */
typedef uint32      DWORD;
#endif // COMPILER

#define CONCAT(x, y) CONCAT1(x, y)
#define CONCAT1(x, y) x##y
#define STATIC_ASSERT_WORKAROUND(expr, msg) typedef char CONCAT(static_assert_failed_at_line_, __LINE__) [(expr) ? 1 : -1]

#ifndef COMPILER_HAS_CPP11_SUPPORT
#if COMPILER == COMPILER_GNU
#  if !defined(__GXX_EXPERIMENTAL_CXX0X__) || (__GNUC__ < 4) || (__GNUC__ == 4) && (__GNUC_MINOR__ < 7)
#    define override
#    define static_assert(a, b) STATIC_ASSERT_WORKAROUND(a, b)
#  endif
#elif COMPILER == COMPILER_CLANG
#  ifndef __cxx_static_assert
#    define override
#    define static_assert(a, b) STATIC_ASSERT_WORKAROUND(a, b)
#  endif
#elif COMPILER == COMPILER_MICROSOFT
#  if _MSC_VER < 1600
#    define static_assert(a, b) STATIC_ASSERT_WORKAROUND(a, b)
#  endif
#endif
#endif

/**
 * @brief
 *
 */
typedef uint64 OBJECT_HANDLE;

#endif // MANGOS_DEFINE_H
