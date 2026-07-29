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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_CLISERVICE
#define MANGOS_H_CLISERVICE

#include "Service.h"

#include <atomic>
#include <thread>

/**
 * @brief Reads console commands and queues them for the world thread.
 *
 * The commands themselves are never executed here: they go onto the world's
 * command queue and run on the world thread, which is the only thread allowed
 * to touch game state. This service is purely a reader.
 */
class CliService : public IService
{
    public:

        /// @param beep Emit a terminal bell once the console is ready.
        explicit CliService(bool beep);
        ~CliService() override;

        const char* Name() const override { return "console"; }

        void Start() override;
        void RequestStop() override;
        void Join() override;

    private:

        void Run();

        /// Reader loop for the full-screen console, which owns the keyboard.
        void RunManaged();

        /// Reader loop for a plain line-oriented terminal (or redirected stdin).
        void RunLineBased();

        const bool        m_beep;
        std::thread       m_thread;
        std::atomic<bool> m_stop;
};

#endif
