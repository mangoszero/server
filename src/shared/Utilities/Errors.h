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

#ifndef MANGOSSERVER_ERRORS_H
#define MANGOSSERVER_ERRORS_H

#include <cassert>
#include "Platform/Define.h"
#include <cstdio>
#include <cstdlib>

// A failed check is fatal, in every configuration.
//
// This used to call assert(), which <cassert> removes when NDEBUG is defined -- and
// NDEBUG is in every Release build. The macro therefore printed a line and carried on,
// while its own comment claimed otherwise, and callers like
// MANGOS_ASSERT(index < m_valuesCount || ...) went on to index out of bounds. abort()
// cannot be compiled out.
//
// do/while(0), because a bare `if (...) { }` lets `if (a) MANGOS_ASSERT(b); else c();`
// bind the else to the macro's own if. stderr, because stdout belongs to the off-thread
// console writer and this message must not tear against it.
#define WPError(CONDITION)                                                   \
do                                                                           \
{                                                                            \
    if (!(CONDITION))                                                        \
    {                                                                        \
        std::fprintf(stderr,                                                 \
            "%s:%i: Error: Assertion in %s failed: %s\n",                    \
            __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION));         \
        std::fflush(stderr);                                                 \
        std::abort();                                                        \
    }                                                                        \
} while (0)

// Reports and continues.
#define WPWarning(CONDITION)                                                 \
do                                                                           \
{                                                                            \
    if (!(CONDITION))                                                        \
    {                                                                        \
        std::fprintf(stderr,                                                 \
            "%s:%i: Warning: Assertion in %s failed: %s\n",                  \
            __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION));         \
    }                                                                        \
} while (0)

#define MANGOS_ASSERT WPError // Fatal in release too -- see above.

#endif
