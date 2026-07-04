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

/// \addtogroup mangosd
/// @{
/// \file

#include <ace/Synch_Traits.h>
#include <ace/Svc_Handler.h>
#include <ace/Thread_Mutex.h>
#include <ace/TP_Reactor.h>
#include <ace/Event_Handler.h>

#include "GdbServerThread.h"

#include "Log.h"
#include "World.h"
#include "Debug/GdbServer/GdbServer.h"

#include <cstring>

// -----------------------------------------------------------------------------
// Shared socket plumbing — buffered, mutex-guarded output that can be written
// to from the world thread (the GdbServer flush path) into a socket living in
// this reactor's thread, exactly like RASocket::sendf.
// -----------------------------------------------------------------------------

class GdbSocketBase : public ACE_Svc_Handler < ACE_SOCK_STREAM, ACE_NULL_SYNCH >
{
    protected:
        typedef ACE_Svc_Handler < ACE_SOCK_STREAM, ACE_NULL_SYNCH > Base;

        enum { GDB_BUFF_SIZE = 65536 };

        GdbSocketBase() : Base(), outActive(false), outputBuffer{}, outputBufferLen(0), outBufferLock()
        {
            reference_counting_policy().value(ACE_Event_Handler::Reference_Counting_Policy::ENABLED);
        }

        virtual ~GdbSocketBase()
        {
            peer().close();
        }

        // Thread-safe append + reactor wakeup. Binary-safe (length carried).
        int sendBytes(const uint8* data, uint32 len)
        {
            ACE_GUARD_RETURN(ACE_Thread_Mutex, Guard, outBufferLock, -1);

            if (closing_)
            {
                return -1;
            }
            if (outputBufferLen + len > GDB_BUFF_SIZE)
            {
                return -1;
            }

            ACE_OS::memcpy(outputBuffer + outputBufferLen, data, len);
            outputBufferLen += len;

            if (!outActive)
            {
                if (reactor()->schedule_wakeup(this, ACE_Event_Handler::WRITE_MASK) == -1)
                {
                    sLog.outError("GdbSocket: schedule_wakeup failed");
                    return -1;
                }
                outActive = true;
            }
            return 0;
        }

        int sendCStr(const char* s)
        {
            return sendBytes(reinterpret_cast<const uint8*>(s), static_cast<uint32>(strlen(s)));
        }

        int handle_output(ACE_HANDLE /*h*/ = ACE_INVALID_HANDLE) override
        {
            ACE_GUARD_RETURN(ACE_Thread_Mutex, Guard, outBufferLock, -1);

            if (closing_)
            {
                return -1;
            }

            if (!outputBufferLen)
            {
                if (reactor()->cancel_wakeup(this, ACE_Event_Handler::WRITE_MASK) == -1)
                {
                    return -1;
                }
                outActive = false;
                return 0;
            }
#ifdef MSG_NOSIGNAL
            ssize_t n = peer().send(outputBuffer, outputBufferLen, MSG_NOSIGNAL);
#else
            ssize_t n = peer().send(outputBuffer, outputBufferLen);
#endif
            if (n <= 0)
            {
                return -1;
            }

            ACE_OS::memmove(outputBuffer, outputBuffer + n, outputBufferLen - n);
            outputBufferLen -= n;
            return 0;
        }

        // Derived classes detach from GdbServer here before the reference is
        // dropped, so the world thread stops using this socket as a writer.
        virtual void onClose() {}

        int close(u_long /*unused*/) override
        {
            if (closing_)
            {
                return -1;
            }
            shutdown();
            closing_ = true;
            onClose();
            remove_reference();
            return 0;
        }

        int handle_close(ACE_HANDLE /*h*/ = ACE_INVALID_HANDLE,
            ACE_Reactor_Mask /*mask*/ = ACE_Event_Handler::ALL_EVENTS_MASK) override
        {
            {
                ACE_GUARD_RETURN(ACE_Thread_Mutex, Guard, outBufferLock, -1);
                if (closing_)
                {
                    return -1;
                }
                closing_ = true;
            }
            onClose();
            remove_reference();
            return 0;
        }

        bool outActive;
        char outputBuffer[GDB_BUFF_SIZE];
        uint32 outputBufferLen;
        ACE_Thread_Mutex outBufferLock;
};

// -----------------------------------------------------------------------------
// GDB RSP socket — feeds raw bytes to the protocol engine and is registered as
// the active RSP output writer.
// -----------------------------------------------------------------------------

class GdbRspSocket : public GdbSocketBase
{
    public:
        friend class ACE_Acceptor<GdbRspSocket, ACE_SOCK_ACCEPTOR>;

    protected:
        int open(void* /*unused*/) override
        {
            if (reactor()->register_handler(this,
                    ACE_Event_Handler::READ_MASK | ACE_Event_Handler::WRITE_MASK) == -1)
            {
                sLog.outError("GdbRspSocket::open: register_handler failed: %s", ACE_OS::strerror(errno));
                return -1;
            }

            ACE_INET_Addr remote_addr;
            if (peer().get_remote_addr(remote_addr) != -1)
            {
                sLog.outString("GdbServer: RSP connection from %s", remote_addr.get_host_addr());
            }

            sGdbServer.AttachRsp(this, &GdbRspSocket::WriteThunk);
            return 0;
        }

        int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE) override
        {
            if (closing_)
            {
                return -1;
            }
            uint8 buf[8192];
            ssize_t readBytes = peer().recv(buf, sizeof(buf));
            if (readBytes <= 0)
            {
                return -1;
            }
            sGdbServer.FeedRsp(buf, static_cast<uint32>(readBytes));
            return 0;
        }

        void onClose() override
        {
            sGdbServer.DetachRsp(this);
        }

    private:
        // GdbServer output writer: invoked from the world thread.
        static void WriteThunk(void* ctx, const uint8* data, uint32 len)
        {
            static_cast<GdbRspSocket*>(ctx)->sendBytes(data, len);
        }
};

// -----------------------------------------------------------------------------
// Plain-text monitor socket — line oriented; submits "mangos ..." lines and
// writes the text reply back. For AI agents and non-RSP debuggers.
// -----------------------------------------------------------------------------

class GdbMonSocket : public GdbSocketBase
{
    public:
        friend class ACE_Acceptor<GdbMonSocket, ACE_SOCK_ACCEPTOR>;

    protected:
        GdbMonSocket() : GdbSocketBase(), inputBuffer{}, inputBufferLen(0) {}

        int open(void* /*unused*/) override
        {
            if (reactor()->register_handler(this,
                    ACE_Event_Handler::READ_MASK | ACE_Event_Handler::WRITE_MASK) == -1)
            {
                sLog.outError("GdbMonSocket::open: register_handler failed: %s", ACE_OS::strerror(errno));
                return -1;
            }
            sendCStr("MaNGOS GDB monitor bridge. Type 'mangos help'.\n");
            return 0;
        }

        int handle_input(ACE_HANDLE = ACE_INVALID_HANDLE) override
        {
            if (closing_)
            {
                return -1;
            }
            ssize_t readBytes = peer().recv(inputBuffer + inputBufferLen,
                MON_BUFF_SIZE - inputBufferLen - 1);
            if (readBytes <= 0)
            {
                return -1;
            }
            inputBufferLen += static_cast<uint32>(readBytes);

            // Process every complete line currently in the buffer.
            uint32 lineStart = 0;
            for (uint32 i = 0; i < inputBufferLen; ++i)
            {
                if (inputBuffer[i] == '\n' || inputBuffer[i] == '\r')
                {
                    inputBuffer[i] = '\0';
                    if (i > lineStart)
                    {
                        sGdbServer.SubmitMonitorLine(this, &GdbMonSocket::WriteTextThunk,
                            inputBuffer + lineStart);
                    }
                    lineStart = i + 1;
                }
            }

            // Shift any partial trailing line to the front.
            if (lineStart > 0)
            {
                uint32 remain = inputBufferLen - lineStart;
                if (remain > 0)
                {
                    ACE_OS::memmove(inputBuffer, inputBuffer + lineStart, remain);
                }
                inputBufferLen = remain;
            }
            else if (inputBufferLen >= MON_BUFF_SIZE - 1)
            {
                // Overlong line with no newline — drop it to avoid a stall.
                inputBufferLen = 0;
            }
            return 0;
        }

    private:
        enum { MON_BUFF_SIZE = 8192 };

        static void WriteTextThunk(void* ctx, const char* text)
        {
            static_cast<GdbMonSocket*>(ctx)->sendCStr(text);
        }

        char inputBuffer[MON_BUFF_SIZE];
        uint32 inputBufferLen;
};

// -----------------------------------------------------------------------------
// Listener thread
// -----------------------------------------------------------------------------

GdbServerThread::GdbServerThread(uint16 rspPort, uint16 monPort, const char* host)
    : m_RspAcceptor(nullptr), m_MonAcceptor(nullptr),
      m_rspAddr(rspPort, host), m_monAddr(monPort, host), m_monEnabled(monPort != 0)
{
    ACE_Reactor_Impl* imp = new ACE_TP_Reactor();
    imp->max_notify_iterations(128);
    m_Reactor = new ACE_Reactor(imp, 1);
    m_RspAcceptor = new GdbRspAcceptor;
    if (m_monEnabled)
    {
        m_MonAcceptor = new GdbMonAcceptor;
    }
}

GdbServerThread::~GdbServerThread()
{
    delete m_Reactor;
    delete m_RspAcceptor;
    delete m_MonAcceptor;
}

int GdbServerThread::open(void* /*unused*/)
{
    if (m_RspAcceptor->open(m_rspAddr, m_Reactor, ACE_NONBLOCK) == -1)
    {
        sLog.outError("GdbServer can not bind RSP port %d on %s",
            m_rspAddr.get_port_number(), m_rspAddr.get_host_addr());
        return -1;
    }
    if (m_monEnabled && m_MonAcceptor->open(m_monAddr, m_Reactor, ACE_NONBLOCK) == -1)
    {
        sLog.outError("GdbServer can not bind monitor port %d on %s",
            m_monAddr.get_port_number(), m_monAddr.get_host_addr());
        return -1;
    }
    activate();
    return 0;
}

int GdbServerThread::svc()
{
    sLog.outString("GdbServer Thread started (RSP on %s:%d%s)",
        m_rspAddr.get_host_addr(), m_rspAddr.get_port_number(),
        m_monEnabled ? ", monitor bridge enabled" : "");

    while (!m_Reactor->reactor_event_loop_done())
    {
        ACE_Time_Value interval(0, 10000);
        if (m_Reactor->run_reactor_event_loop(interval) == -1)
        {
            break;
        }
        if (World::IsStopped())
        {
            m_RspAcceptor->close();
            if (m_monEnabled)
            {
                m_MonAcceptor->close();
            }
            break;
        }
    }
    sLog.outString("GdbServer Thread stopped");
    return 0;
}
/// @}
