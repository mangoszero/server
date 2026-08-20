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

#ifndef MANGOS_H_PROCESS
#define MANGOS_H_PROCESS

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

/**
 * @file Process.h
 * @brief Running in the background, on whichever platform this is.
 *
 * ===== THE SERVER'S LOOP IS A PARAMETER =====
 *
 * Windows and POSIX arrive at the background from opposite directions. On
 * Windows the service manager starts the process and calls into it; on POSIX
 * the process forks itself and tells its own parent when it is up. Written out
 * at the call site, that is two `#ifdef`ed halves of main() that drift.
 *
 * So the server hands its loop over instead, and the platform decides how it
 * gets called. One code path in main(), and the difference stays here.
 * ============================================
 *
 * What is NOT pretended to be common: registering with a service manager.
 * Windows has one and the server can install itself into it; on POSIX that is
 * systemd's or rc.d's business and writing unit files is not the server's job.
 * The functions say so rather than quietly succeeding.
 */

namespace Process
{

/// What `-s <word>` on the command line asks for.
enum class ServiceAction
{
    None,       ///< run in the foreground, the ordinary case
    Install,    ///< register with the service manager and exit
    Uninstall,  ///< deregister and exit
    Run,        ///< go to the background
    Stop        ///< signal a running background instance to stop
};

/// The action @p word names, or None for anything unrecognised.
ServiceAction ParseServiceAction(std::string_view word);

/// Everything the platform needs. The names are only read where the platform
/// has a service manager to give them to.
struct Options
{
    /// Where the background instance writes its pid, and where Stop reads it.
    std::string pidFile;

    /**
     * How long the forking parent waits for the child to report ready before it
     * gives up, kills the child and reports failure. POSIX only; on Windows the
     * service manager keeps its own timeout.
     *
     * ===== ZERO MEANS WAIT =====
     *
     * Zero -- the default -- sets no alarm: the parent waits until the child
     * either reports ready or dies, and the death is what reports the failure.
     * That is the right setting for a world server, and a wrong non-zero value
     * is not a slow start-up, it is a killed one: loading a full world takes
     * minutes on a cold database, so a ten-second alarm would fire in the middle
     * of it and terminate the server that was still coming up.
     * ===========================
     */
    std::uint32_t readyTimeoutSeconds = 0;

    std::string serviceName = "MaNGOS";
    std::string serviceDisplayName = "MaNGOS World Service";
    std::string serviceDescription = "MaNGOS World Service - serves a World of"
                                     " Warcraft realm.";
};

/// Whether this platform has a service manager to register with. False on
/// POSIX, and that is not a gap: init is not ours to write into.
bool HasServiceManager();

/// Register with the service manager. False, with a reason logged, where there
/// is none.
bool Install(const Options& options);

/// Deregister. False where there is no service manager.
bool Uninstall(const Options& options);

/**
 * @brief Go to the background and run @p serve there.
 *
 * On Windows this hands control to the service manager, which starts a thread
 * that calls @p serve. On POSIX it forks: the child calls @p serve, and the
 * parent waits for ReportReady() before exiting successfully -- so a start-up
 * that fails before the server is up is reported to whoever ran the command,
 * rather than leaving them with a shell prompt and a dead process.
 *
 * @return what @p serve returned, or a non-zero code when the background start
 *         itself failed.
 */
int RunInBackground(const Options& options, const std::function<int()>& serve);

/**
 * @brief From inside @p serve, once the server is actually serving.
 *
 * On POSIX this is what releases the waiting parent. Calling it early makes the
 * command look successful while the server is still deciding whether it can
 * start; never calling it makes every start look like a timeout.
 *
 * Harmless and ignored on Windows, and harmless in the foreground.
 */
void ReportReady();

/// Signal the instance named by `options.pidFile` to stop. False when there is
/// no pid file, it cannot be read, or the process it names is not there.
bool Stop(const Options& options);

/**
 * ===== WHAT THE LOOP HAS TO ASK =====
 *
 * The three below are what the world loop needs from the platform, and they are
 * the reason this interface is not just "start me in the background". A service
 * manager talks to a running server -- stop it, pause it, resume it -- on its
 * own thread, and the answers have to reach the loop somehow. They cross a
 * thread boundary, so they cross it through these, not through a shared int.
 * ====================================
 */

/**
 * @brief Is the served loop running detached from a terminal?
 *
 * True inside a Windows service and inside the forked POSIX child; false in the
 * foreground. What it gates is the console: a background instance has no stdin,
 * and a CLI reader that treats end-of-input as "shut down" turns a successful
 * daemon start into an immediate exit.
 */
bool IsRunningInBackground();

/**
 * @brief Has the service manager asked the server to stop?
 *
 * Always false where there is no service manager: on POSIX a stop arrives as
 * SIGINT or SIGTERM and the server's own signal handlers already have it.
 */
bool StopRequested();

/**
 * @brief Is the service manager holding the server paused?
 *
 * The loop is expected to stall while this is true. Always false on POSIX,
 * which has no equivalent control.
 */
bool IsPaused();

} // namespace Process

#endif
