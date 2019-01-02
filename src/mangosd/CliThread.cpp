/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

/// \addtogroup mangosd
/// @{
/// \file

#include <ace/OS.h>

#include "CliThread.h"
#include "World.h"
#include "Util.h"

#ifdef _WIN32
  #include <windows.h>
#endif

static void prompt(void* callback = NULL, bool status = true)
{
    printf("mangos>");
    fflush(stdout);
}

// Non-blocking keypress detector, when return pressed, return 1, else always return 0
#if (PLATFORM != PLATFORM_WINDOWS)
static int kb_hit_return()
{
    struct timeval tv;
    fd_set fds;
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv);
    return FD_ISSET(STDIN_FILENO, &fds);
}
#endif


CliThread::CliThread(bool beep) : beep_(beep)
{
}

/// %Thread start
int CliThread::svc()
{
    ACE_OS::sleep(1);

    if (beep_)
        { printf("\a"); }    // \a = Alert

    prompt();

    ///- As long as the World is running (no World::m_stopEvent), get the command line and handle it
    while (!World::IsStopped())
    {
#if (PLATFORM != PLATFORM_WINDOWS)
        while (!kb_hit_return() && !World::IsStopped())
            // With this, we limit CLI to 10 commands/second
            { usleep(100); }
        if (World::IsStopped())
            { break; }
#endif
        char* command_str = fgets(buffer_, sizeof(buffer_), stdin);
        if (command_str != NULL)
        {
            for (int x = 0; command_str[x]; ++x)
                if (command_str[x] == '\r' || command_str[x] == '\n')
                {
                    command_str[x] = 0;
                    break;
                }

            if (!*command_str)
            {
                prompt();
                continue;
            }

            std::string command;
            if (!consoleToUtf8(command_str, command))       // convert from console encoding to utf8
            {
                prompt();
                continue;
            }

            sWorld.QueueCliCommand(new CliCommandHolder(0, SEC_CONSOLE, NULL, command.c_str(), &utf8print, &prompt));
        }

        else if (feof(stdin))
        {
            World::StopNow(SHUTDOWN_EXIT_CODE);
        }
    }

    return 0;
}

void CliThread::cli_shutdown()
{
#ifdef _WIN32

        // send keyboard input to safely unblock the CLI thread, which is blocked on fgets
        INPUT_RECORD b;
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);

        b.EventType = KEY_EVENT;
        b.Event.KeyEvent.bKeyDown = TRUE;
        b.Event.KeyEvent.dwControlKeyState = 0;
        b.Event.KeyEvent.uChar.AsciiChar = '\r';
        b.Event.KeyEvent.wVirtualKeyCode = VK_RETURN;
        b.Event.KeyEvent.wRepeatCount = 1;
        b.Event.KeyEvent.wVirtualScanCode = 0x1c;

        DWORD numb = 0;
        BOOL ret = WriteConsoleInput(hStdIn, &b, 1, &numb);

        wait();
#endif
}
