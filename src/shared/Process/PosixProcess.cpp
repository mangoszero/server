/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "Process/Process.h"

#include "Log/Log.h"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace Process
{

namespace
{

/**
 * ===== ONLY sig_atomic_t MAY CROSS INTO A HANDLER =====
 *
 * These are written by the process and read from a signal handler, and
 * volatile sig_atomic_t is the only type the standard allows to be touched
 * there. A plain pid_t in their place is undefined behaviour that happens to
 * work until it does not.
 * ======================================================
 */
volatile sig_atomic_t g_parentPid = 0;
volatile sig_atomic_t g_childPid = 0;

/// Read from the world loop's thread, written once in the forked child before
/// the loop exists. Atomic rather than plain bool because those are not the
/// same thread.
std::atomic<bool> g_inBackground{false};

/**
 * @brief Runs in the PARENT, while it waits for the child to come up.
 *
 * ===== _exit, NEVER exit =====
 *
 * This is a signal handler. exit() runs the atexit handlers and flushes stdio;
 * arriving there from inside malloc or a FILE lock deadlocks the process at the
 * exact moment it is trying to report a failure. getpid, kill and _exit are all
 * async-signal-safe, and nothing else here is called.
 * =============================
 */
void HandleStartupSignal(int signal)
{
    if (getpid() != g_parentPid)
    {
        return;
    }

    // The child says it is up.
    if (signal == SIGUSR1)
    {
        _exit(EXIT_SUCCESS);
    }

    // ===== THE CHILD DIED BEFORE IT WAS READY =====
    //
    // This is what makes a timeout unnecessary. A start-up that fails -- no
    // database, a bad configuration, a missing DBC -- ends with the child
    // exiting, and that exit is reported here, immediately and however long the
    // load was going to take. Nothing to forward: it is already gone.
    // ==============================================
    if (signal == SIGCHLD)
    {
        _exit(EXIT_FAILURE);
    }

    // Interrupted, terminated, or the wait timed out. Pass it on to the child so
    // a half-started server does not survive the command that started it.
    if (g_childPid)
    {
        kill(static_cast<pid_t>(g_childPid), signal);
    }

    _exit(EXIT_FAILURE);
}

bool RedirectStandardStreams()
{
    // The daemon has no terminal. Left attached, the first write to stdout after
    // the terminal closes takes the process down with SIGHUP or a write error at
    // an arbitrary moment.
    return std::freopen("/dev/null", "rt", stdin)
           && std::freopen("/dev/null", "wt", stdout)
           && std::freopen("/dev/null", "wt", stderr);
}

bool WritePidFile(const std::string& path)
{
    if (path.empty())
    {
        return true;
    }

    std::ofstream file(path.c_str(), std::ios::out | std::ios::trunc);
    if (!file)
    {
        return false;
    }

    file << getpid() << '\n';
    return bool(file);
}

/**
 * @brief Is @p pid running the same executable as this process?
 *
 * @return true when it is, AND when the platform cannot say. A false negative
 *         would refuse a legitimate stop, which is worse than the stale-pid case
 *         this guards -- so only a definite mismatch refuses.
 */
bool IsSameExecutable(pid_t pid)
{
#ifdef __linux__
    // ===== IDENTITY, NOT SPELLING =====
    //
    // stat() through /proc/<pid>/exe lands on the binary itself, so the two are
    // compared by device and inode rather than by the path they happen to be
    // reachable at. That is the question actually being asked, and the string
    // form got it wrong in both directions: an in-place upgrade leaves the path
    // identical while the file underneath is a different one, and a hard link or
    // a bind mount gives one file two names.
    //
    // No buffer either, so there is no PATH_MAX and no truncation to reason about.
    // ==================================
    char link[64] = {};
    std::snprintf(link, sizeof(link), "/proc/%ld/exe", static_cast<long>(pid));

    struct stat mine = {};
    struct stat theirs = {};

    if (stat("/proc/self/exe", &mine) != 0)
    {
        return true;
    }

    if (stat(link, &theirs) != 0)
    {
        // No such process, or not ours to look at. kill() answers that better.
        return true;
    }

    return mine.st_dev == theirs.st_dev && mine.st_ino == theirs.st_ino;
#else
    // FreeBSD and macOS need sysctl/libproc for this. Not worth the platform code
    // until a stale pid file actually bites somewhere other than Linux.
    (void)pid;
    return true;
#endif
}

} // namespace

bool HasServiceManager()
{
    return false;
}

bool UseExecutableDirectory()
{
    // Nothing to move to. A POSIX start keeps the directory it was invoked from
    // until RunInBackground forks, and that is what makes a relative -c path and
    // a relative pid file mean what the operator typed.
    return true;
}

bool Install(const Options&)
{
    sLog.outError("This platform has no service manager to install into."
                  " Use the init system: a systemd unit or an rc.d script.");
    return false;
}

bool Uninstall(const Options&)
{
    sLog.outError("This platform has no service manager to uninstall from.");
    return false;
}

int RunInBackground(const Options& options, const std::function<int()>& serve)
{
    if (!serve)
    {
        return EXIT_FAILURE;
    }

    g_parentPid = getpid();

    std::signal(SIGUSR1, HandleStartupSignal);
    std::signal(SIGCHLD, HandleStartupSignal);
    std::signal(SIGINT, HandleStartupSignal);
    std::signal(SIGTERM, HandleStartupSignal);
    std::signal(SIGALRM, HandleStartupSignal);

    // Blocked across the fork. g_childPid is assigned only AFTER fork returns,
    // so a SIGTERM landing in that window would find it still zero: the handler
    // would forward nothing and the parent would leave a child running with no
    // one holding its pid.
    sigset_t blocked;
    sigset_t previous;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGTERM);
    sigprocmask(SIG_BLOCK, &blocked, &previous);

    const pid_t child = fork();
    g_childPid = child;

    if (child < 0)
    {
        sigprocmask(SIG_SETMASK, &previous, nullptr);
        sLog.outError("Cannot fork into the background: %s", std::strerror(errno));
        return EXIT_FAILURE;
    }

    if (child > 0)
    {
        // The parent. It waits here until the child reports ready, dies, or --
        // where one was asked for -- the alarm fires; every one of those leaves
        // through the handler.
        sigprocmask(SIG_SETMASK, &previous, nullptr);

        if (options.readyTimeoutSeconds != 0)
        {
            alarm(options.readyTimeoutSeconds);
        }

        // Looped: pause() also returns after a signal whose handler simply
        // returned, and a single call would then fall through and report a
        // failure that did not happen.
        for (;;)
        {
            pause();
        }
    }

    // The child. Its inherited handlers belong to the parent's wait, and in the
    // child HandleStartupSignal returns without doing anything -- so leaving
    // SIGINT and SIGTERM pointed at it would swallow them for the whole of
    // start-up, until Serve() installs the server's own. A start command
    // cancelled in that window, or a `-s stop` racing it, would be ignored and
    // the daemon would survive.
    //
    // Reset while the two are still blocked, and unblocked only afterwards, so
    // there is no instant where the disposition is the parent's and the signal
    // can arrive. SIGCHLD goes back too: the server forks nothing today, but an
    // inherited disposition is how that surprises whoever adds a helper process.
    std::signal(SIGUSR1, SIG_DFL);
    std::signal(SIGALRM, SIG_DFL);
    std::signal(SIGCHLD, SIG_DFL);
    std::signal(SIGINT, SIG_DFL);
    std::signal(SIGTERM, SIG_DFL);

    sigprocmask(SIG_SETMASK, &previous, nullptr);

    umask(0);

    if (setsid() < 0)
    {
        _exit(EXIT_FAILURE);
    }

    // Off whatever directory it was started from, so the daemon does not hold a
    // mount busy and cannot be unmounted out from under.
    if (chdir("/") < 0)
    {
        _exit(EXIT_FAILURE);
    }

    if (!WritePidFile(options.pidFile))
    {
        _exit(EXIT_FAILURE);
    }

    if (!RedirectStandardStreams())
    {
        _exit(EXIT_FAILURE);
    }

    g_inBackground.store(true, std::memory_order_release);

    return serve();
}

void ReportReady()
{
    if (g_parentPid && g_parentPid != getpid())
    {
        kill(static_cast<pid_t>(g_parentPid), SIGUSR1);

        // Once, and only once: a second one would arrive at a parent that has
        // already exited and whose pid may since have been reused.
        g_parentPid = 0;
    }
}

bool Stop(const Options& options)
{
    if (options.pidFile.empty())
    {
        sLog.outError("Cannot stop a background instance: no pid file was configured.");
        return false;
    }

    std::ifstream file(options.pidFile.c_str());
    if (!file)
    {
        sLog.outError("Cannot read the pid file %s", options.pidFile.c_str());
        return false;
    }

    long pid = 0;
    if (!(file >> pid) || pid <= 0 ||
        pid > static_cast<long>(std::numeric_limits<pid_t>::max()))
    {
        // Zero or below would go to the process GROUP or to every process this
        // user owns. The upper bound matters just as much and is easier to miss:
        // pid_t is 32-bit where long is 64-bit, so 4294967295 passes a "> 0"
        // test and then narrows to -1 -- and kill(-1) is the same broadcast by
        // another route. Both ends are checked before either cast.
        sLog.outError("The pid file %s does not name a process", options.pidFile.c_str());
        return false;
    }

    // A pid file outlives the process it names, and pids are reused. Signalling
    // whatever now holds that number is how a stale file comes to interrupt an
    // unrelated program. Where the check is available, the target must be running
    // the same executable as this one.
    if (!IsSameExecutable(static_cast<pid_t>(pid)))
    {
        sLog.outError("The pid file %s names process %ld, which is not this server;"
                      " refusing to signal it. Remove the stale file.",
                      options.pidFile.c_str(), pid);
        return false;
    }

    if (kill(static_cast<pid_t>(pid), SIGINT) < 0)
    {
        sLog.outError("Cannot stop process %ld: %s", pid, std::strerror(errno));
        return false;
    }

    return true;
}

bool IsRunningInBackground()
{
    return g_inBackground.load(std::memory_order_acquire);
}

bool StopRequested()
{
    // There is no service manager here. A stop arrives as SIGINT or SIGTERM,
    // which the server's own handlers already turn into World::StopNow().
    return false;
}

bool IsPaused()
{
    return false;
}

} // namespace Process
