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

#include <ace/Synch_Traits.h>
#include <ace/Svc_Handler.h>
#include <ace/Thread_Mutex.h>
#include <ace/TP_Reactor.h>
#include <ace/Event_Handler.h>

#include "RAThread.h"

#include "AccountMgr.h"
#include "Log.h"
#include "World.h"
#include "Util.h"
#include "Language.h"
#include "Config.h"
#include "ObjectMgr.h"


class RASocket: protected ACE_Svc_Handler < ACE_SOCK_STREAM, ACE_NULL_SYNCH>
{
    typedef ACE_Svc_Handler < ACE_SOCK_STREAM, ACE_NULL_SYNCH> Base;

    enum { RA_BUFF_SIZE = 8192 };

    public:
        friend class ACE_Acceptor<RASocket, ACE_SOCK_ACCEPTOR >;

        int sendf(const char* msg)
        {
            ACE_GUARD_RETURN(ACE_Thread_Mutex, Guard, outBufferLock, -1);

            if (closing_)
              { return -1; }

            int msgLen = strlen(msg);

            if (msgLen + outputBufferLen > RA_BUFF_SIZE)
              { return -1; }

            ACE_OS::memcpy(outputBuffer + outputBufferLen, msg, msgLen);
            outputBufferLen += msgLen;

            if (!outActive)
            {
                if (reactor()->schedule_wakeup(this, ACE_Event_Handler::WRITE_MASK) == -1)
                {
                    sLog.outError("RASocket::sendf error while schedule_wakeup");
                    return -1;
                }
                outActive = true;
            }
            return 0;
        }

    protected:
        RASocket(void) : Base(),outBufferLock(), outActive(false), inputBufferLen(0),
                         outputBufferLen(0), stage(NONE)
        {
            bSecure = sConfig.GetBoolDefault("RA.Secure", true);
            bStricted = sConfig.GetBoolDefault("RA.Stricted", false);
            iMinLevel = AccountTypes(sConfig.GetIntDefault("RA.MinLevel", SEC_ADMINISTRATOR));
            reference_counting_policy().value(ACE_Event_Handler::Reference_Counting_Policy::ENABLED);
        }

        virtual ~RASocket(void)
        {
            peer().close();
            sLog.outRALog("Connection was closed.");
        }

        virtual int open(void* unused) override
        {
            if (reactor()->register_handler(this, ACE_Event_Handler::READ_MASK | ACE_Event_Handler::WRITE_MASK) == -1)
            {
                sLog.outError("RASocket::open: unable to accept connection from client, error = %s", ACE_OS::strerror(errno));
                return -1;
            }

            ACE_INET_Addr remote_addr;

            if (peer().get_remote_addr(remote_addr) == -1)
            {
                sLog.outError("RASocket::open: peer ().get_remote_addr errno = %s", ACE_OS::strerror(errno));
                return -1;
            }

            sLog.outRALog("Incoming connection from %s.", remote_addr.get_host_addr());

            ///- print Motd
            sendf(sWorld.GetMotd());
            sendf("\r\n");
            sendf(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_USER));

            return 0;
        }

        virtual int close(u_long unused) override
        {
            if (closing_)
              { return -1; }

            shutdown();

            closing_ = true;

            remove_reference();
            return 0;
        }

        virtual int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE) override
        {
            if (closing_)
            {
                sLog.outError("Called RASocket::handle_input with closing_ = true");
                return -1;
            }

            size_t readBytes = peer().recv(inputBuffer + inputBufferLen, RA_BUFF_SIZE - inputBufferLen - 1);

            if (readBytes <= 0)
            {
                DEBUG_LOG("read " SIZEFMTD " bytes in RASocket::handle_input", readBytes);
                return -1;
            }

            ///- Discard data after line break or line feed
            bool gotenter = false;
            for (; readBytes > 0 ; --readBytes)
            {
                char c = inputBuffer[inputBufferLen];
                if (c == '\r' || c == '\n')
                {
                    gotenter = true;
                    break;
                }
                ++inputBufferLen;
            }

            if (gotenter)
            {
                inputBuffer[inputBufferLen] = 0;
                inputBufferLen = 0;
                switch (stage)
                {
                    case NONE:
                    {
                        std::string szLogin = inputBuffer;
                        accId = sAccountMgr.GetId(szLogin);

                        ///- If the user is not found, deny access
                        if (!accId)
                        {
                            sendf("-No such user.\r\n");
                            sLog.outRALog("User %s does not exist.", szLogin.c_str());
                            if (bSecure)
                            {
                                handle_output();
                                return -1;
                            }
                            sendf("\r\n");
                            sendf(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_USER));
                            break;
                        }

                        accAccessLevel = sAccountMgr.GetSecurity(accId);

                        ///- if gmlevel is too low, deny access
                        if (accAccessLevel < iMinLevel)
                        {
                            sendf("-Not enough privileges.\r\n");
                            sLog.outRALog("User %s has no privilege.", szLogin.c_str());
                            if (bSecure)
                            {
                                handle_output();
                                return -1;
                            }
                            sendf("\r\n");
                            sendf(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_USER));
                            break;
                        }

                        ///- allow by remotely connected admin use console level commands dependent from config setting
                        if (accAccessLevel >= SEC_ADMINISTRATOR && !bStricted)
                          { accAccessLevel = SEC_CONSOLE; }

                        stage = LG;
                        sendf(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_PASS));
                        break;
                    }
                    case LG:
                    {
                        // login+pass ok
                        std::string pw = inputBuffer;

                        if (sAccountMgr.CheckPassword(accId, pw))
                        {
                            stage = OK;

                            sendf("+Logged in.\r\n");
                            sLog.outRALog("User account %u has logged in.", accId);
                            sendf("mangos>");
                        }
                        else
                        {
                            ///- Else deny access
                            sendf("-Wrong pass.\r\n");
                            sLog.outRALog("User account %u has failed to log in.", accId);
                            if (bSecure)
                            {
                                handle_output();
                                return -1;
                            }
                            sendf("\r\n");
                            sendf(sObjectMgr.GetMangosStringForDBCLocale(LANG_RA_PASS));
                        }
                        break;
                    }
                    case OK:
                        if (strlen(inputBuffer))
                        {
                            sLog.outRALog("Got '%s' cmd.", inputBuffer);
                            if (strncmp(inputBuffer, "quit", 4) == 0)
                              { return -1; }
                            else
                            {
                                CliCommandHolder* cmd = new CliCommandHolder(accId, accAccessLevel, this, inputBuffer,
                                                                             &RASocket::zprint, &RASocket::commandFinished);
                                sWorld.QueueCliCommand(cmd);
                            }
                        }
                        else
                          { sendf("mangos>"); }
                        break;
                }
            }
            // no enter yet? wait for next input...
            return 0;
        }

        virtual int handle_output(ACE_HANDLE h = ACE_INVALID_HANDLE) override
        {
            ACE_GUARD_RETURN(ACE_Thread_Mutex, Guard, outBufferLock, -1);

            if (closing_)
              { return -1; }

            if (!outputBufferLen)
            {
                if (reactor()->cancel_wakeup(this, ACE_Event_Handler::WRITE_MASK) == -1)
                  { return -1; }
                outActive = false;
                return 0;
            }
#ifdef MSG_NOSIGNAL
            ssize_t n = peer().send(outputBuffer, outputBufferLen, MSG_NOSIGNAL);
#else
            ssize_t n = peer().send(outputBuffer, outputBufferLen);
#endif // MSG_NOSIGNAL

            if (n <= 0)
              { return -1; }

            ACE_OS::memmove(outputBuffer, outputBuffer + n, outputBufferLen - n);

            outputBufferLen -= n;

            return 0;
        }

        virtual int handle_close(ACE_HANDLE h = ACE_INVALID_HANDLE,
                                 ACE_Reactor_Mask mask = ACE_Event_Handler::ALL_EVENTS_MASK) override
        {
            if (closing_)
              { return -1; }

            ACE_GUARD_RETURN(ACE_Thread_Mutex, Guard, outBufferLock, -1);

            closing_ = true;

            if (h == ACE_INVALID_HANDLE)
                { peer().close_writer(); }
            remove_reference();
            return 0;
        }

    private:
        bool outActive;

        char inputBuffer[RA_BUFF_SIZE];
        uint32 inputBufferLen;

        char outputBuffer[RA_BUFF_SIZE];
        uint32 outputBufferLen;

        uint32 accId;
        AccountTypes accAccessLevel;
        bool bSecure;                                       /**< kick on wrong pass, non exist. user OR user with no priv. will protect from DOS, bruteforce attacks */
        bool bStricted;                                     /**< not allow execute console only commands (SEC_CONSOLE) remotly */
        AccountTypes iMinLevel;

        enum
        {
            NONE,                                           // initial value
            LG,                                             // only login was entered
            OK                                              // both login and pass were given, they were correct and user has enough priv.
        } stage;

        ACE_Thread_Mutex outBufferLock;

        static void zprint(void* callbackArg, const char* szText)
        {
            if (!szText)
              { return; }

            ((RASocket*)callbackArg)->sendf(szText);
        }

        static void commandFinished(void* callbackArg, bool success)
        {
            RASocket* raSocket = (RASocket*)callbackArg;
            raSocket->sendf("mangos>");
        }
};


RAThread::RAThread(uint16 port, const char* host) : listen_addr(port, host)
{
    ACE_Reactor_Impl* imp = 0;

    imp = new ACE_TP_Reactor();
    imp->max_notify_iterations(128);

    m_Reactor = new ACE_Reactor(imp, 1);
    m_Acceptor = new RAAcceptor;
}

RAThread::~RAThread()
{
    delete m_Reactor;
    delete m_Acceptor;
}

int RAThread::open(void* unused)
{
    if (m_Acceptor->open(listen_addr, m_Reactor, ACE_NONBLOCK) == -1)
    {
        sLog.outError("MaNGOS RA can not bind to port %d on %s\n", listen_addr.get_port_number(), listen_addr.get_host_addr());
        return -1;
    }
    activate();
    return 0;
}

int RAThread::svc()
{
    sLog.outString("Remote Access Thread started (listening on %s:%d)",
                    listen_addr.get_host_addr(),
                    listen_addr.get_port_number());

    while (!m_Reactor->reactor_event_loop_done())
    {
        ACE_Time_Value interval(0, 10000);

        if (m_Reactor->run_reactor_event_loop(interval) == -1)
          { break; }

        if (World::IsStopped())
        {
            m_Acceptor->close();
            break;
        }
    }
    sLog.outString("Remote Access Thread stopped");
    return 0;
}
