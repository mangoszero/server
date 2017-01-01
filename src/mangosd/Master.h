/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_H_MASTER
#define MANGOS_H_MASTER

#include "Common.h"
#include "Policies/Singleton.h"

/**
 * @brief Start the server
 *
 */
class Master
{
    public:
        /**
         * @brief
         *
         */
        Master();
        /**
         * @brief
         *
         */
        ~Master();
        /**
         * @brief
         *
         * @return int
         */
        int Run();
        static volatile uint32 m_masterLoopCounter; /**< TODO */

    private:
        /**
         * @brief
         *
         * @return bool
         */
        bool _StartDB();

        /**
         * @brief
         *
         */
        void _HookSignals();
        /**
         * @brief
         *
         */
        void _UnhookSignals();
        /**
         * @brief
         *
         * @param s
         */
        static void _OnSignal(int s);

        /**
         * @brief
         *
         */
        void clearOnlineAccounts();
};

#define sMaster MaNGOS::Singleton<Master>::Instance()
#endif
/// @}
