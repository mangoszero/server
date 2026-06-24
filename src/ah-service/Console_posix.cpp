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
 * Console_InstallParentDeathGuard() uses prctl(PR_SET_PDEATHSIG, SIGUSR1)
 * so the child process receives SIGUSR1 if the mangosd parent dies
 * unexpectedly, preventing orphaned ah-service processes.  The installed
 * SIGUSR1 handler verifies getppid() == 1 (reparented to init) before
 * self-terminating, so a spurious parent-death signal (e.g. the spawning
 * thread died but mangosd is still alive) does NOT kill an otherwise-healthy
 * child.
 *
 * OPEN-2: a DISTINCT parent-death signal (SIGUSR1, not SIGTERM) is used so
 * SIGTERM keeps its default-terminate disposition. The child therefore does
 * NOT swallow a stray/operator SIGTERM while parented. The supervisor's
 * hard-kill is unaffected either way: ACE_Process_Manager::terminate() ->
 * ACE::terminate_process() uses POSIX kill(pid, SIGKILL) (signal 9,
 * ACE.cpp:250), which is uncatchable, so the supervisor can ALWAYS kill the
 * child regardless of which signals it handles.
 */

#include "Console.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/prctl.h>
#include <signal.h>
#include <unistd.h>

/**
 * @brief SIGUSR1 handler for the parent-death guard.
 *
 * PR_SET_PDEATHSIG delivers SIGUSR1 when the PARENT THREAD that spawned this
 * process exits -- which is not necessarily mangosd dying (a worker thread
 * could exit while the process lives).  Guard against that false positive:
 * only self-terminate when this process has actually been reparented to init
 * (getppid() == 1), the unambiguous signal that the real parent is gone.
 * Otherwise ignore the signal and keep running.
 *
 * OPEN-2: SIGUSR1 (not SIGTERM) is the parent-death signal so a normal
 * SIGTERM is never swallowed -- it keeps its default-terminate behaviour.
 *
 * Uses only async-signal-safe calls (getppid, _exit). The ignore branch does
 * nothing at all -- no logging -- to stay strictly async-signal-safe.
 */
static void AhServicePDeathHandler(int /*sig*/)
{
    if (getppid() == 1)
    {
        // Real parent (mangosd) is gone: exit promptly. _exit avoids running
        // atexit handlers / flushing shared state from a signal context.
        _exit(0);
    }
    // Spurious SIGUSR1: parent still alive (not reparented to init). Ignore
    // and keep serving; do not tear down the healthy child.
}

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
    // OPEN-2: install the SIGUSR1 handler FIRST so the getppid()==1 guard is
    // in place before the kernel could deliver the parent-death signal.
    // SIGTERM is deliberately left UNHOOKED (default-terminate) so a normal
    // operator/stray SIGTERM is honoured rather than silently swallowed; the
    // parent-death notification rides a distinct, otherwise-unused signal.
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = AhServicePDeathHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGUSR1, &sa, nullptr) != 0)
    {
        perror("ah-service: sigaction(SIGUSR1) failed");
    }

    // Ask the kernel to deliver SIGUSR1 to this process when its parent
    // (mangosd) dies.  This closes the Linux orphan-guard TODO from Task 4.
    if (prctl(PR_SET_PDEATHSIG, SIGUSR1) != 0)
    {
        perror("ah-service: prctl(PR_SET_PDEATHSIG) failed");
    }

    // Race guard: if the parent already died between spawn and the prctl call
    // above, the death signal would never arrive. Check explicitly and exit
    // if we have already been reparented to init.
    if (getppid() == 1)
    {
        _exit(0);
    }
}
