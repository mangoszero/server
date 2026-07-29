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

#ifndef MANGOS_DEFINE_H
#define MANGOS_DEFINE_H

#include <sys/types.h>

#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "Platform/CompilerDefs.h"

#define MANGOS_LITTLEENDIAN 0
#define MANGOS_BIGENDIAN    1

// Normally supplied by CMake (TEST_BIG_ENDIAN -> MANGOS_ENDIAN). The fallback keeps
// this header self-contained for tooling that compiles it outside the build.
#if !defined(MANGOS_ENDIAN)
#  if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && \
      (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)
#    define MANGOS_ENDIAN MANGOS_BIGENDIAN
#  else
#    define MANGOS_ENDIAN MANGOS_LITTLEENDIAN
#  endif
#endif // MANGOS_ENDIAN

#if PLATFORM == PLATFORM_WINDOWS
#  define MANGOS_PATH_MAX 260                               // ::MAX_PATH, without dragging in windows.h
#elif defined(PATH_MAX)
#  define MANGOS_PATH_MAX PATH_MAX
#else
#  define MANGOS_PATH_MAX 1024
#endif

#if PLATFORM == PLATFORM_WINDOWS
#  ifndef DECLSPEC_NORETURN
#    define DECLSPEC_NORETURN __declspec(noreturn)
#  endif // DECLSPEC_NORETURN
#else // PLATFORM != PLATFORM_WINDOWS
#  define DECLSPEC_NORETURN
#endif // PLATFORM

#if COMPILER == COMPILER_GNU || COMPILER == COMPILER_CLANG
#  define ATTR_NORETURN __attribute__((noreturn))
#  define ATTR_PRINTF(F, V) __attribute__ ((format (printf, F, V)))
#  define ATTR_DEPRECATED __attribute__((deprecated))
#else //COMPILER != COMPILER_GNU
#  define ATTR_NORETURN
#  define ATTR_PRINTF(F, V)
#  define ATTR_DEPRECATED
#endif //COMPILER == COMPILER_GNU

/// A signed integer of 64 bits
typedef std::int64_t  int64;
/// A signed integer of 32 bits
typedef std::int32_t  int32;
/// A signed integer of 16 bits
typedef std::int16_t  int16;
/// A signed integer of 8 bits
typedef std::int8_t   int8;
/// An unsigned integer of 64 bits
typedef std::uint64_t uint64;
/// An unsigned integer of 32 bits
typedef std::uint32_t uint32;
/// An unsigned integer of 16 bits
typedef std::uint16_t uint16;
/// An unsigned integer of 8 bits
typedef std::uint8_t  uint8;

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

// ---------------------------------------------------------------------------
// 64-bit printf formatting and literals.
//
// These used to expand to ACE_UINT64_FORMAT_SPECIFIER and friends, which is why
// printing a uint64 anywhere in the server pulled in ACE. <cinttypes> is the
// standard answer to the same problem and is correct on every platform by
// definition, so the macros survive but their contents no longer come from a
// third-party portability layer.
// ---------------------------------------------------------------------------
#define UI64FMTD "%" PRIu64
#define SI64FMTD "%" PRId64
#define I64FMT   "%016" PRIX64
#define I32FMT   "%08" PRIX32

#define UI64LIT(N) UINT64_C(N)
#define SI64LIT(N) INT64_C(N)

/// Number of elements in a C array. Rejects pointers at compile time, which the
/// old sizeof/sizeof macro silently accepted.
template<typename T, std::size_t N>
constexpr std::size_t countof(T const (&)[N]) noexcept
{
    return N;
}

#define STRINGIZE(a) #a

// MSVC spells the case-insensitive compares with a leading underscore.
#if COMPILER == COMPILER_MICROSOFT
#  define strnicmp _strnicmp
#else
#  define strnicmp strncasecmp
#endif

#endif // MANGOS_DEFINE_H
