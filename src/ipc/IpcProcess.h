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

#ifndef AH_IPC_PROCESS_H
#define AH_IPC_PROCESS_H

#include "Common.h"

#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

/**
 * @brief Cross-platform spawn/liveness/terminate/reap of a single child process.
 *
 * Spawns and supervises the AH worker child. The shared secret is passed
 * out-of-band in the child's environment (never on argv), so it cannot be read
 * from the process command line by another local account.
 */
class IpcProcess
{
    public:
        static const uint32 INVALID_PID = 0;

        IpcProcess();
        ~IpcProcess();

        /**
         * @brief Spawn @p exePath with @p args and one extra environment
         *        variable @p envName=@p envValue (added to the inherited env).
         *
         * On Windows the child gets its own console (CREATE_NEW_CONSOLE).
         * @return true on success; Pid()/Handle() are then valid.
         */
        bool Spawn(const std::string& exePath,
                   const std::vector<std::string>& args,
                   const std::string& envName,
                   const std::string& envValue);

        /// True while the child is still running (non-blocking check).
        bool Running() const;

        /// Force-kill the child (SIGKILL / TerminateProcess). Best effort.
        void Terminate();

        /// Release the OS handle / reap the zombie once the child has exited.
        void Reap();

        uint32 Pid() const { return m_pid; }
        bool   Valid() const { return m_pid != INVALID_PID; }

#ifdef _WIN32
        /// Child process HANDLE (for Job Object assignment); NULL if none.
        HANDLE Handle() const { return m_handle; }
#endif

    private:
        uint32 m_pid;
#ifdef _WIN32
        HANDLE m_handle;
#endif

        IpcProcess(const IpcProcess&);
        IpcProcess& operator=(const IpcProcess&);
};

#endif // AH_IPC_PROCESS_H
