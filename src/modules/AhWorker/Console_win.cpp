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
 */

/**
 * @file Console_win.cpp
 * @brief Windows console show/hide for ah-service.
 *
 * Compiled on Windows only (see ah-service/CMakeLists.txt).
 */

#include "Console.h"

#include <windows.h>
#include <cstdio>

void Console_Show(bool show)
{
    HWND hwnd = GetConsoleWindow();
    if (hwnd)
    {
        ShowWindow(hwnd, show ? SW_SHOW : SW_HIDE);
    }
}

void Console_InstallParentDeathGuard()
{
    // Windows: the Job Object created by WorkerSupervisor already ensures
    // the child terminates when mangosd exits.  Nothing extra needed here.
}
