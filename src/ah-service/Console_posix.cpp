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
 */

/**
 * @file Console_posix.cpp
 * @brief POSIX console show/hide stub + parent-death guard for ah-service.
 *
 * Compiled on non-Windows platforms only (see ah-service/CMakeLists.txt).
 *
 * Console_Show() is a no-op on POSIX: there is no separate console window
 * to show or hide; output goes to the log file instead.
 *
 * Console_InstallParentDeathGuard() uses prctl(PR_SET_PDEATHSIG, SIGTERM)
 * so the child process receives SIGTERM if the mangosd parent dies
 * unexpectedly, preventing orphaned ah-service processes.
 */

#include "Console.h"

#include <cstdio>
#include <sys/prctl.h>
#include <signal.h>

void Console_Show(bool show)
{
    // POSIX has no console window to show or hide.
    // The service writes all output to its log file.
    // This message confirms receipt of the IPC_CONSOLE frame.
    printf("ah-service: console show/hide requested (show=%d); see log file\n",
           show ? 1 : 0);
    fflush(stdout);
}

void Console_InstallParentDeathGuard()
{
    // Ask the kernel to deliver SIGTERM to this process when its parent
    // (mangosd) dies.  This closes the Linux orphan-guard TODO from Task 4.
    if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0)
    {
        perror("ah-service: prctl(PR_SET_PDEATHSIG) failed");
    }
}
