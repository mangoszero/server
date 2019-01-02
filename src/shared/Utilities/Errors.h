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

#ifndef MANGOSSERVER_ERRORS_H
#define MANGOSSERVER_ERRORS_H

#include "Common/Common.h"


#ifdef HAVE_ACE_STACK_TRACE_H
#  include <ace/Stack_Trace.h>
#endif

#ifdef HAVE_ACE_STACK_TRACE_H
// Normal assert.
#define WPError(CONDITION) \
    if (!(CONDITION)) \
    { \
        ACE_Stack_Trace st; \
        printf("%s:%i: Error: Assertion in %s failed: %s\nStack Trace:\n%s", \
               __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION), st.c_str()); \
        assert(STRINGIZE(CONDITION) && 0); \
    }

// Just warn.
#define WPWarning(CONDITION) \
    if (!(CONDITION)) \
    { \
        ACE_Stack_Trace st; \
        printf("%s:%i: Warning: Assertion in %s failed: %s\nStack Trace:\n%s",\
               __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION), st.c_str()); \
    }
#else
// Normal assert.
#define WPError(CONDITION) \
    if (!(CONDITION)) \
    { \
        printf("%s:%i: Error: Assertion in %s failed: %s", \
               __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION)); \
        assert(STRINGIZE(CONDITION) && 0); \
    }

// Just warn.
#define WPWarning(CONDITION) \
    if (!(CONDITION)) \
    { \
        printf("%s:%i: Warning: Assertion in %s failed: %s",\
               __FILE__, __LINE__, __FUNCTION__, STRINGIZE(CONDITION)); \
    }
#endif

#define MANGOS_ASSERT WPError                             // Error even if in release mode.

#endif
