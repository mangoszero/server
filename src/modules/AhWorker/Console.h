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

#ifndef AH_SERVICE_CONSOLE_H
#define AH_SERVICE_CONSOLE_H

/**
 * @file Console.h
 * @brief Platform-independent console show/hide interface for ah-service.
 *
 * Windows: Console_Show() calls ShowWindow(GetConsoleWindow(), ...).
 * POSIX:   Console_Show() is a no-op (logs a message instead).
 *
 * Console_InstallParentDeathGuard():
 *   Windows: no-op (the Job Object in WorkerSupervisor handles orphan kill).
 *   POSIX:   calls prctl(PR_SET_PDEATHSIG, SIGTERM) so the child receives
 *            SIGTERM if the mangosd parent process dies.
 */

/**
 * @brief Show or hide the child service console window.
 *
 * @param show  true = show the console window; false = hide it.
 */
void Console_Show(bool show);

/**
 * @brief Install a parent-death guard so the child exits if mangosd dies.
 *
 * On Windows this is a no-op; the Job Object created by WorkerSupervisor
 * already terminates the child when mangosd exits.
 * On POSIX this calls prctl(PR_SET_PDEATHSIG, SIGTERM).
 */
void Console_InstallParentDeathGuard();

#endif // AH_SERVICE_CONSOLE_H
