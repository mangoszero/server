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

#ifndef MANGOS_H_RASOCKET
#define MANGOS_H_RASOCKET

#include "Common.h"
#include <ace/Synch_Traits.h>
#include <ace/Svc_Handler.h>
#include <ace/SOCK_Acceptor.h>
#include <ace/Acceptor.h>
#include <ace/Thread_Mutex.h>
#include <ace/Semaphore.h>

#define RA_BUFF_SIZE 8192

/**
 * @brief Remote Administration socket
 *
 */
typedef ACE_Svc_Handler < ACE_SOCK_STREAM, ACE_NULL_SYNCH> RAHandler;
/**
 * @brief
 *
 */
class RASocket: protected RAHandler
{
    public:
        ACE_Semaphore pendingCommands; /**< TODO */
        /**
         * @brief
         *
         */
        typedef ACE_Acceptor<RASocket, ACE_SOCK_ACCEPTOR > Acceptor;
        friend class ACE_Acceptor<RASocket, ACE_SOCK_ACCEPTOR >;

        /**
         * @brief
         *
         * @param
         * @return int
         */
        int sendf(const char*);

    protected:
        /**
         * @brief things called by ACE framework.
         *
         */
        RASocket(void);
        /**
         * @brief
         *
         */
        virtual ~RASocket(void);

        /**
         * @brief Called on open ,the void* is the acceptor.
         *
         * @param
         * @return int
         */
        virtual int open(void*) override;

        /**
         * @brief Called on failures inside of the acceptor, don't call from your code.
         *
         * @param int
         * @return int
         */
        virtual int close(int);

        /**
         * @brief Called when we can read from the socket.
         *
         * @param ACE_HANDLE
         * @return int
         */
        virtual int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE) override;

        /**
         * @brief Called when the socket can write.
         *
         * @param ACE_HANDLE
         * @return int
         */
        virtual int handle_output(ACE_HANDLE = ACE_INVALID_HANDLE) override;

        /**
         * @brief Called when connection is closed or error happens.
         *
         * @param ACE_HANDLE
         * @param ACE_Reactor_Mask
         * @return int
         */
        virtual int handle_close(ACE_HANDLE = ACE_INVALID_HANDLE,
                                 ACE_Reactor_Mask = ACE_Event_Handler::ALL_EVENTS_MASK);

    private:
        bool outActive; /**< TODO */

        char inputBuffer[RA_BUFF_SIZE]; /**< TODO */
        uint32 inputBufferLen; /**< TODO */

        ACE_Thread_Mutex outBufferLock; /**< TODO */
        char outputBuffer[RA_BUFF_SIZE]; /**< TODO */
        uint32 outputBufferLen; /**< TODO */

        uint32 accId; /**< TODO */
        AccountTypes accAccessLevel; /**< TODO */
        bool bSecure;                                       /**< kick on wrong pass, non exist. user OR user with no priv. will protect from DOS, bruteforce attacks */
        bool bStricted;                                     /**< not allow execute console only commands (SEC_CONSOLE) remotly */
        AccountTypes iMinLevel; /**< TODO */
        /**
         * @brief
         *
         */
        enum
        {
            NONE,                                           // initial value
            LG,                                             // only login was entered
            OK                                              // both login and pass were given, they were correct and user has enough priv.
        } stage; /**< TODO */

        /**
         * @brief
         *
         * @param callbackArg
         * @param szText
         */
        static void zprint(void* callbackArg, const char* szText);
        /**
         * @brief
         *
         * @param callbackArg
         * @param success
         */
        static void commandFinished(void* callbackArg, bool success);
};
#endif
/// @}
