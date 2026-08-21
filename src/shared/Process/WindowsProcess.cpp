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

#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <string>
#include <vector>

namespace Process
{

namespace
{

/// What the service manager last asked for. Written by the control handler on
/// the SCM's own thread and read by the world loop on its own, so they are
/// atomic: a plain int across those two threads is a data race whichever way
/// it is read.
std::atomic<bool> g_stopRequested{false};
std::atomic<bool> g_paused{false};
std::atomic<bool> g_inBackground{false};

SERVICE_STATUS g_status{};
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;

/// The server's loop, handed over by RunInBackground.
std::function<int()> g_serve;
std::string g_serviceName;
int g_exitCode = EXIT_FAILURE;

void PublishStatus(DWORD state)
{
    g_status.dwCurrentState = state;

    if (g_statusHandle)
    {
        SetServiceStatus(g_statusHandle, &g_status);
    }
}

void WINAPI ControlHandler(DWORD control)
{
    switch (control)
    {
        case SERVICE_CONTROL_INTERROGATE:
            break;

        case SERVICE_CONTROL_SHUTDOWN:
        case SERVICE_CONTROL_STOP:
            // Only recorded. Stopping is the server's to do, and doing it from
            // this thread would tear the world down underneath the loop still
            // running in it.
            g_stopRequested.store(true, std::memory_order_release);

            // A stop while paused would otherwise never be seen: the loop is
            // stalled on the pause flag and never reaches the stop check.
            g_paused.store(false, std::memory_order_release);
            PublishStatus(SERVICE_STOP_PENDING);
            return;

        case SERVICE_CONTROL_PAUSE:
            g_paused.store(true, std::memory_order_release);
            PublishStatus(SERVICE_PAUSED);
            return;

        case SERVICE_CONTROL_CONTINUE:
            g_paused.store(false, std::memory_order_release);
            PublishStatus(SERVICE_RUNNING);
            return;

        default:
            break;
    }

    PublishStatus(g_status.dwCurrentState);
}

/// The full path of this executable, or empty when it cannot be determined.
std::string ExecutablePath()
{
    std::vector<char> path(MAX_PATH);

    for (;;)
    {
        const DWORD written =
            GetModuleFileNameA(nullptr, path.data(), DWORD(path.size()));

        if (written == 0)
        {
            return std::string();
        }

        // Truncation is reported by filling the buffer, not by an error, so the
        // only way to tell is that the whole buffer came back. Long-path support
        // makes this reachable with an ordinary install directory.
        if (written < path.size())
        {
            return std::string(path.data(), written);
        }

        path.resize(path.size() * 2);
    }
}

/// The directory the executable is in, or empty when it cannot be determined.
std::string ExecutableDirectory()
{
    const std::string full = ExecutablePath();

    if (full.empty())
    {
        return std::string();
    }

    const std::size_t slash = full.find_last_of("\\/");

    // No separator at all is not a directory: returning an empty string rather
    // than an empty prefix keeps the caller from chdir-ing to the drive root.
    return slash == std::string::npos ? std::string() : full.substr(0, slash);
}

void WINAPI ServiceMain(DWORD, char**)
{
    g_status = SERVICE_STATUS{};
    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = SERVICE_START_PENDING;
    g_status.dwControlsAccepted =
        SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN | SERVICE_ACCEPT_PAUSE_CONTINUE;
    g_status.dwWin32ExitCode = NO_ERROR;

    g_statusHandle = RegisterServiceCtrlHandlerA(g_serviceName.c_str(), ControlHandler);

    if (!g_statusHandle)
    {
        return;
    }

    PublishStatus(SERVICE_START_PENDING);

    // The service manager starts the process from system32, not from where the
    // binary lives, so every relative path in the configuration would resolve
    // somewhere else. Both the lookup and the chdir are checked: an empty
    // directory handed to SetCurrentDirectory succeeds at nothing.
    const std::string directory = ExecutableDirectory();

    if (directory.empty() || !SetCurrentDirectoryA(directory.c_str()))
    {
        g_status.dwWin32ExitCode = ERROR_PATH_NOT_FOUND;
        PublishStatus(SERVICE_STOPPED);
        return;
    }

    g_inBackground.store(true, std::memory_order_release);

    PublishStatus(SERVICE_RUNNING);

    // The server's own loop, handed over by the caller.
    g_exitCode = g_serve ? g_serve() : EXIT_FAILURE;

    PublishStatus(SERVICE_STOP_PENDING);

    g_status.dwControlsAccepted = 0;
    g_status.dwWin32ExitCode =
        g_exitCode == EXIT_SUCCESS ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
    g_status.dwServiceSpecificExitCode = DWORD(g_exitCode);
    PublishStatus(SERVICE_STOPPED);
}

} // namespace

bool HasServiceManager()
{
    return true;
}

bool UseExecutableDirectory()
{
    const std::string directory = ExecutableDirectory();

    if (directory.empty())
    {
        sLog.outError("SERVICE: cannot determine this executable's directory.");
        return false;
    }

    if (!SetCurrentDirectoryA(directory.c_str()))
    {
        sLog.outError("SERVICE: cannot change to '%s': error %u",
                      directory.c_str(), unsigned(GetLastError()));
        return false;
    }

    return true;
}

bool Install(const Options& options)
{
    SC_HANDLE manager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);

    if (!manager)
    {
        sLog.outError("SERVICE: no access to the service control manager."
                      " Run as administrator.");
        return false;
    }

    const std::string binary = ExecutablePath();

    if (binary.empty())
    {
        CloseServiceHandle(manager);
        sLog.outError("SERVICE: cannot determine this executable's path.");
        return false;
    }

    // Built as a string, not appended into a fixed array: the path alone can
    // fill MAX_PATH, and the arguments still have to go somewhere. Quoted, so
    // that a Program Files install is not read as a binary plus two arguments.
    const std::string command = "\"" + binary + "\" -s run";

    SC_HANDLE service = CreateServiceA(
        manager,
        options.serviceName.c_str(),
        options.serviceDisplayName.c_str(),
        SERVICE_ALL_ACCESS,
        // Not SERVICE_INTERACTIVE_PROCESS: it has been ignored since Vista and
        // asking for it only means asking to run in session 0 with a desktop.
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        // Not SERVICE_ERROR_IGNORE, which is what a failed start being invisible
        // in the event log looks like from the outside.
        SERVICE_ERROR_NORMAL,
        command.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr);

    if (!service)
    {
        const unsigned error = unsigned(GetLastError());
        CloseServiceHandle(manager);
        sLog.outError("SERVICE: cannot register '%s': error %u",
                      options.serviceName.c_str(), error);
        return false;
    }

    // Linked directly rather than looked up through GetProcAddress: every
    // Windows this server runs on exports it.
    SERVICE_DESCRIPTIONA description{};
    description.lpDescription = const_cast<char*>(options.serviceDescription.c_str());
    ChangeServiceConfig2A(service, SERVICE_CONFIG_DESCRIPTION, &description);

    SC_ACTION restart{};
    restart.Type = SC_ACTION_RESTART;
    restart.Delay = 10000;

    SERVICE_FAILURE_ACTIONSA failure{};
    failure.dwResetPeriod = INFINITE;
    failure.cActions = 1;
    failure.lpsaActions = &restart;
    ChangeServiceConfig2A(service, SERVICE_CONFIG_FAILURE_ACTIONS, &failure);

    CloseServiceHandle(service);
    CloseServiceHandle(manager);

    sLog.outString("SERVICE: '%s' installed.", options.serviceName.c_str());
    return true;
}

bool Uninstall(const Options& options)
{
    SC_HANDLE manager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);

    if (!manager)
    {
        sLog.outError("SERVICE: no access to the service control manager."
                      " Run as administrator.");
        return false;
    }

    SC_HANDLE service = OpenServiceA(manager, options.serviceName.c_str(),
                                     SERVICE_QUERY_STATUS | DELETE);

    if (!service)
    {
        CloseServiceHandle(manager);
        sLog.outError("SERVICE: '%s' is not installed.", options.serviceName.c_str());
        return false;
    }

    SERVICE_STATUS status{};
    bool removed = false;

    if (QueryServiceStatus(service, &status) && status.dwCurrentState == SERVICE_STOPPED)
    {
        removed = DeleteService(service) != FALSE;

        if (!removed)
        {
            sLog.outError("SERVICE: cannot remove '%s': error %u",
                          options.serviceName.c_str(), unsigned(GetLastError()));
        }
    }
    else
    {
        // Reported as the failure it is: deleting nothing and returning success
        // is how "uninstall" comes to leave the service installed.
        sLog.outError("SERVICE: '%s' is still running; stop it before removing it.",
                      options.serviceName.c_str());
    }

    CloseServiceHandle(service);
    CloseServiceHandle(manager);

    if (removed)
    {
        sLog.outString("SERVICE: '%s' removed.", options.serviceName.c_str());
    }

    return removed;
}

int RunInBackground(const Options& options, const std::function<int()>& serve)
{
    g_serve = serve;
    g_serviceName = options.serviceName;

    // A non-const copy because the table's name field is char*, and the API does
    // not promise not to touch it.
    std::vector<char> name(g_serviceName.begin(), g_serviceName.end());
    name.push_back('\0');

    SERVICE_TABLE_ENTRYA table[] =
    {
        { name.data(), ServiceMain },
        { nullptr, nullptr }
    };

    if (!StartServiceCtrlDispatcherA(table))
    {
        sLog.outError("SERVICE: cannot start the control dispatcher: error %u",
                      unsigned(GetLastError()));
        return EXIT_FAILURE;
    }

    return g_exitCode;
}

void ReportReady()
{
    // The service manager was told the service was running before the loop was
    // entered, which is as much as it wants to know.
}

bool Stop(const Options& options)
{
    SC_HANDLE manager = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);

    if (!manager)
    {
        sLog.outError("SERVICE: no access to the service control manager.");
        return false;
    }

    SC_HANDLE service = OpenServiceA(manager, options.serviceName.c_str(), SERVICE_STOP);
    bool stopped = false;

    if (service)
    {
        SERVICE_STATUS status{};
        stopped = ControlService(service, SERVICE_CONTROL_STOP, &status) != FALSE;

        if (!stopped)
        {
            sLog.outError("SERVICE: cannot stop '%s': error %u",
                          options.serviceName.c_str(), unsigned(GetLastError()));
        }

        CloseServiceHandle(service);
    }
    else
    {
        sLog.outError("SERVICE: '%s' is not installed.", options.serviceName.c_str());
    }

    CloseServiceHandle(manager);
    return stopped;
}

bool IsRunningInBackground()
{
    return g_inBackground.load(std::memory_order_acquire);
}

bool StopRequested()
{
    return g_stopRequested.load(std::memory_order_acquire);
}

bool IsPaused()
{
    return g_paused.load(std::memory_order_acquire);
}

} // namespace Process
