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

#include "IpcProcess.h"

#include <vector>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdlib>
#endif

#ifdef _WIN32

IpcProcess::IpcProcess() : m_pid(INVALID_PID), m_handle(NULL) {}

IpcProcess::~IpcProcess()
{
    if (m_handle != NULL)
    {
        CloseHandle(m_handle);
        m_handle = NULL;
    }
}

bool IpcProcess::Spawn(const std::string& exePath,
                       const std::vector<std::string>& args,
                       const std::string& envName,
                       const std::string& envValue)
{
    // Build a single command line: "exe" arg1 "arg with spaces" ...
    std::string cmd = "\"" + exePath + "\"";
    for (const std::string& a : args)
    {
        cmd += ' ';
        if (a.find_first_of(" \t") != std::string::npos)
        {
            cmd += '"';
            cmd += a;
            cmd += '"';
        }
        else
        {
            cmd += a;
        }
    }

    // Pass the secret out-of-band: set it in this process's environment just
    // for the CreateProcess call (inherited by the child), then remove it.
    SetEnvironmentVariableA(envName.c_str(), envValue.c_str());

    std::vector<char> mutableCmd(cmd.begin(), cmd.end());
    mutableCmd.push_back('\0');

    STARTUPINFOA si;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);

    PROCESS_INFORMATION pi;
    ZeroMemory(&pi, sizeof(pi));

    const BOOL ok = CreateProcessA(
        NULL, mutableCmd.data(), NULL, NULL, FALSE,
        CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi);

    SetEnvironmentVariableA(envName.c_str(), NULL);

    if (!ok)
    {
        return false;
    }

    CloseHandle(pi.hThread);
    m_handle = pi.hProcess;
    m_pid    = static_cast<uint32>(pi.dwProcessId);
    return true;
}

bool IpcProcess::Running() const
{
    if (m_handle == NULL)
    {
        return false;
    }
    return WaitForSingleObject(m_handle, 0) == WAIT_TIMEOUT;
}

void IpcProcess::Terminate()
{
    if (m_handle != NULL)
    {
        TerminateProcess(m_handle, 1);
    }
}

void IpcProcess::Reap()
{
    if (m_handle != NULL)
    {
        CloseHandle(m_handle);
        m_handle = NULL;
    }
    m_pid = INVALID_PID;
}

#else // POSIX

IpcProcess::IpcProcess() : m_pid(INVALID_PID) {}

IpcProcess::~IpcProcess() {}

bool IpcProcess::Spawn(const std::string& exePath,
                       const std::vector<std::string>& args,
                       const std::string& envName,
                       const std::string& envValue)
{
    const pid_t pid = ::fork();
    if (pid < 0)
    {
        return false;
    }

    if (pid == 0)
    {
        // Child: add the secret to our environment (only the child's), then exec.
        ::setenv(envName.c_str(), envValue.c_str(), 1);

        std::vector<char*> argv;
        argv.reserve(args.size() + 2);
        argv.push_back(const_cast<char*>(exePath.c_str()));
        for (const std::string& a : args)
        {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);

        ::execv(exePath.c_str(), argv.data());
        // exec only returns on failure.
        ::_exit(127);
    }

    m_pid = static_cast<uint32>(pid);
    return true;
}

bool IpcProcess::Running() const
{
    if (m_pid == INVALID_PID)
    {
        return false;
    }
    int status = 0;
    const pid_t r = ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
    // 0 => still running; >0 => exited (reaped here); <0 => not our child / gone.
    return r == 0;
}

void IpcProcess::Terminate()
{
    if (m_pid != INVALID_PID)
    {
        ::kill(static_cast<pid_t>(m_pid), SIGKILL);
    }
}

void IpcProcess::Reap()
{
    if (m_pid != INVALID_PID)
    {
        int status = 0;
        ::waitpid(static_cast<pid_t>(m_pid), &status, WNOHANG);
        m_pid = INVALID_PID;
    }
}

#endif
