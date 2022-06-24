/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#include "GitRevision.h"

char const* GitRevision::GetHash()
{
    return REVISION_HASH;
}

char const* GitRevision::GetDate()
{
    return REVISION_DATE;
}

char const* GitRevision::GetBranch()
{
    return REVISION_BRANCH;
}

char const* GitRevision::GetCMakeVersion()
{
    return CMAKE_VERSION;
}

char const* GitRevision::GetHostOSVersion()
{
    return "Compiled on: " CMAKE_HOST_SYSTEM;
}

// Platform Define
#if PLATFORM == PLATFORM_WINDOWS
    #ifdef _WIN64
        #define MANGOS_PLATFORM_STR "Win64"
    #else
        #define MANGOS_PLATFORM_STR "Win32"
    #endif
#elif PLATFORM == PLATFORM_APPLE
    #define MANGOS_PLATFORM_STR "MacOSX"
#elif PLATFORM == PLATFORM_INTEL
    #define MANGOS_PLATFORM_STR "Intel"
#elif PLATFORM == PLATFORM_UNIX
    #define MANGOS_PLATFORM_STR "Linux"
#else
    #define MANGOS_PLATFORM_STR "Unknown System"
#endif

char const* GitRevision::GetFullRevision()
{
    return "Mangos revision: " VER_PRODUCTVERSION_STR;
}

char const* GitRevision::GetRunningSystem()
{
    return "Running on: " CMAKE_HOST_SYSTEM;
}

char const* GitRevision::GetCompanyNameStr()
{
    return VER_COMPANY_NAME_STR;
}

char const* GitRevision::GetLegalCopyrightStr()
{
    return VER_LEGALCOPYRIGHT_STR;
}

char const* GitRevision::GetFileVersionStr()
{
    return VER_FILEVERSION_STR;
}

char const* GitRevision::GetProductVersionStr()
{
    return VER_PRODUCTVERSION_STR;
}
