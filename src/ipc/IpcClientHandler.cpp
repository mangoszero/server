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

#include "IpcClientHandler.h"
#include "IpcReliable.h"
#include "IpcVersion.h"
#include "Log/Log.h"

#include <ace/Reactor.h>
#include <ace/OS_NS_unistd.h>
#include <cstdio>

#ifdef _WIN32
#include <process.h>
#define IPC_GETPID() static_cast<uint32>(::_getpid())
#else
#include <unistd.h>
#define IPC_GETPID() static_cast<uint32>(::getpid())
#endif

// ---------------------------------------------------------------------------
// Static context
// ---------------------------------------------------------------------------

BoundedQueue<IpcMessage>* IpcClientHandler::s_pendingInbound = nullptr;
std::string               IpcClientHandler::s_pendingSecret;
IpcClientLink*            IpcClientHandler::s_pendingLink    = nullptr;

void IpcClientHandler::SetPendingContext(BoundedQueue<IpcMessage>* inbound,
                                          const std::string& secret,
                                          IpcClientLink* link)
{
    s_pendingInbound = inbound;
    s_pendingSecret  = secret;
    s_pendingLink    = link;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

IpcClientHandler::IpcClientHandler()
    : m_outBuffer(nullptr),
      m_state(IPC_CLI_WAIT_CONNECT),
      m_inbound(nullptr),
      m_link(nullptr),
      m_closing(false)
{
    reference_counting_policy().value(
        ACE_Event_Handler::Reference_Counting_Policy::ENABLED);
}

IpcClientHandler::~IpcClientHandler()
{
    if (m_outBuffer)
    {
        m_outBuffer->release();
        m_outBuffer = nullptr;
    }
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
    peer().close();
}

// ---------------------------------------------------------------------------
// open() - called by ACE_Connector when connection succeeds
// ---------------------------------------------------------------------------

int IpcClientHandler::open(void* /*connector*/)
{
    if (m_outBuffer)
    {
        return -1;
    }

    m_inbound = s_pendingInbound;
    m_secret  = s_pendingSecret;
    if (s_pendingLink)
    {
        m_link = s_pendingLink;
        m_link->AddRef();
    }

    ACE_NEW_RETURN(m_outBuffer, ACE_Message_Block(k_outBufSize), -1);

    if (reactor()->register_handler(this, ACE_Event_Handler::READ_MASK) == -1)
    {
        fprintf(stderr, "IpcClientHandler::open: register_handler failed\n");
        return -1;
    }

    // Publish ourselves into the link as the reactor-thread handler slot
    // (runs on the reactor thread). Liveness stays false until the handshake
    // completes in ProcessFrame().
    if (m_link)
    {
        m_link->handler = this;
    }

    // Begin the handshake BEFORE releasing our local reference. SendHello()
    // touches `this`, so dropping the reference first would risk a use-after-
    // free if the reactor concurrently closed the handler. We send first, then
    // hand ownership to the reactor.
    const int rc = SendHello();

    // Reactor now holds a reference; release ours.
    remove_reference();
    return rc;
}

// ---------------------------------------------------------------------------
// close()
// ---------------------------------------------------------------------------

int IpcClientHandler::close(u_long /*flags*/)
{
    shutdown();
    m_closing = true;
    remove_reference();
    return 0;
}

// ---------------------------------------------------------------------------
// handle_input()
// ---------------------------------------------------------------------------

int IpcClientHandler::handle_input(ACE_HANDLE)
{
    if (m_closing)
    {
        return -1;
    }

    char buf[4096];
    const ssize_t n = peer().recv(buf, sizeof(buf));

    if (n <= 0)
    {
        if (n == 0 || (errno != EWOULDBLOCK && errno != EAGAIN))
        {
            return -1;
        }
        return 0;
    }

    m_recvBuf.append(reinterpret_cast<const uint8*>(buf),
                     static_cast<size_t>(n));

    while (m_recvBuf.rpos() < m_recvBuf.size())
    {
        IpcMessage msg;
        std::string err;

        if (!IpcMessage::Decode(m_recvBuf, msg, err))
        {
            // Only "short header" / "incomplete" are transient (need more
            // bytes); any other Decode error is a corrupt stream and fatal.
            if (err == "incomplete" || err == "short header")
            {
                break;
            }

            fprintf(stderr, "IpcClientHandler: framing error: %s\n",
                    err.c_str());
            return handle_close(ACE_INVALID_HANDLE,
                                ACE_Event_Handler::ALL_EVENTS_MASK);
        }

        if (ProcessFrame(msg) == -1)
        {
            return -1;
        }
    }

    // Compact: drop consumed front bytes so a peer that always leaves a
    // trailing partial frame cannot grow m_recvBuf without bound.
    CompactRecvBuf();

    return 0;
}

// ---------------------------------------------------------------------------
// CompactRecvBuf() - drop already-consumed front bytes from the reassembly buf
// ---------------------------------------------------------------------------

void IpcClientHandler::CompactRecvBuf()
{
    const size_t consumed = m_recvBuf.rpos();
    if (consumed == 0)
    {
        return;
    }

    if (consumed == m_recvBuf.size())
    {
        m_recvBuf.clear();
        return;
    }

    const size_t remaining = m_recvBuf.size() - consumed;
    ByteBuffer compacted;
    compacted.append(m_recvBuf.contents() + consumed, remaining);
    m_recvBuf = compacted;
}

// ---------------------------------------------------------------------------
// handle_output()
// ---------------------------------------------------------------------------

int IpcClientHandler::handle_output(ACE_HANDLE)
{
    return FlushOutBuffer();
}

// ---------------------------------------------------------------------------
// handle_close()
// ---------------------------------------------------------------------------

int IpcClientHandler::handle_close(ACE_HANDLE h, ACE_Reactor_Mask)
{
    {
        ACE_GUARD_RETURN(LockType, g, m_outLock, -1);
        m_closing = true;
        if (h == ACE_INVALID_HANDLE)
        {
            peer().close_writer();
        }
    }

    // Clear the link so the facade stops reporting us live and the reactor-
    // thread notifier stops routing through us. Runs on the reactor thread.
    if (m_link)
    {
        m_link->live.store(false, std::memory_order_release);
        if (m_link->handler == this)
        {
            m_link->handler = nullptr;
        }
    }

    reactor()->remove_handler(this,
        ACE_Event_Handler::DONT_CALL | ACE_Event_Handler::ALL_EVENTS_MASK);
    return 0;
}

// ---------------------------------------------------------------------------
// SendFrame()
// ---------------------------------------------------------------------------

int IpcClientHandler::SendFrame(const IpcMessage& msg)
{
    ACE_GUARD_RETURN(LockType, g, m_outLock, -1);

    if (m_closing)
    {
        return -1;
    }

    ByteBuffer wire;
    msg.Encode(wire);

    const size_t len = wire.size();
    if (m_outBuffer->space() < len)
    {
        fprintf(stderr, "IpcClientHandler::SendFrame: output buffer full\n");
        return -1;
    }

    m_outBuffer->copy(reinterpret_cast<const char*>(wire.contents()), len);

    if (reactor()->schedule_wakeup(this, ACE_Event_Handler::WRITE_MASK) == -1)
    {
        fprintf(stderr, "IpcClientHandler::SendFrame:"
                        " schedule_wakeup WRITE_MASK failed\n");
        return -1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// IsLive / IsClosing
// ---------------------------------------------------------------------------

bool IpcClientHandler::IsLive() const
{
    return m_state == IPC_CLI_LIVE;
}

bool IpcClientHandler::IsClosing() const
{
    return m_closing;
}

// ---------------------------------------------------------------------------
// InboundFrameAcceptable() - per-opcode size predicate (shared with tests)
// ---------------------------------------------------------------------------

bool IpcClientHandler::InboundFrameAcceptable(uint16 op, uint32 bodyLen)
{
    const IpcBodySizeRule rule = IpcExpectedBodySize(op);
    if (!rule.known)
    {
        return false;
    }
    return rule.exact ? (bodyLen == rule.maxLen) : (bodyLen <= rule.maxLen);
}

// ---------------------------------------------------------------------------
// SendHello() - initiate the handshake
// ---------------------------------------------------------------------------

int IpcClientHandler::SendHello()
{
    IpcMessage hello;
    hello.op = IPC_HELLO;

    // body: uint16 proto, uint32 pid, then secret bytes
    hello.body << uint16(IPC_PROTOCOL_VERSION)
               << uint32(IPC_GETPID());
    hello.body.append(reinterpret_cast<const uint8*>(m_secret.data()),
                      m_secret.size());

    m_state = IPC_CLI_WAIT_HELLO_ACK;
    return SendFrame(hello);
}

// ---------------------------------------------------------------------------
// ProcessFrame() - handshake state machine (client side)
// ---------------------------------------------------------------------------

int IpcClientHandler::ProcessFrame(const IpcMessage& msg)
{
    switch (m_state)
    {
        case IPC_CLI_WAIT_HELLO_ACK:
        {
            if (msg.op != IPC_HELLO_ACK)
            {
                fprintf(stderr, "IpcClientHandler: expected IPC_HELLO_ACK,"
                                " got 0x%04X\n", static_cast<unsigned>(msg.op));
                return handle_close(ACE_INVALID_HANDLE,
                                    ACE_Event_Handler::ALL_EVENTS_MASK);
            }

            // Body: uint32 run-id + (SP-2) uint8 write-authority. A legacy
            // 4-byte body decodes authority=0 (never granted).
            uint32 runId = 0;
            uint8  writeAuthority = 0;
            {
                ByteBuffer b;
                b.append(msg.body.contents(), msg.body.size());
                if (b.size() >= sizeof(uint32))
                {
                    b >> runId;
                }
                if (b.rpos() + sizeof(uint8) <= b.size())
                {
                    b >> writeAuthority;
                }
            }
            if (m_link)
            {
                m_link->runId.store(runId, std::memory_order_release);
                m_link->writeAuthority.store(writeAuthority,
                                             std::memory_order_release);
            }
            fprintf(stdout, "IpcClientHandler: received run-id %u,"
                            " write-authority %u\n", runId,
                            static_cast<unsigned>(writeAuthority));
            fflush(stdout);
            // Send IPC_READY
            IpcMessage ready;
            ready.op = IPC_READY;

            m_state = IPC_CLI_LIVE;

            if (SendFrame(ready) == -1)
            {
                return -1;
            }

            // Publish liveness for the facade. Runs on the reactor thread.
            if (m_link)
            {
                m_link->live.store(true, std::memory_order_release);
            }

            fprintf(stdout,
                    "IpcClientHandler: handshake complete - channel live\n");
            fflush(stdout);
            break;
        }

        case IPC_CLI_LIVE:
        {
            if (!InboundFrameAcceptable(msg.op,
                                        static_cast<uint32>(msg.body.size())))
            {
                fprintf(stderr, "IpcClientHandler: rejecting inbound frame"
                                " 0x%04X (body=%u) - bad size/unknown opcode\n",
                                static_cast<unsigned>(msg.op),
                                static_cast<unsigned>(msg.body.size()));
                break;
            }
            // [SP-2 decision 10] Mutation-class frames ride the UNBOUNDED
            // reliable lane (never dropped); the facade drains it to exhaustion
            // before the bounded queue each pass. Everything else stays on the
            // bounded drop-newest queue. Fall back to bounded if no link.
            if (m_link && IpcIsReliableOpcode(msg.op))
            {
                m_link->PushReliable(msg);
            }
            else if (m_inbound)
            {
                if (!m_inbound->push(msg))
                {
                    fprintf(stderr, "IpcClientHandler: inbound queue full"
                                    " - frame 0x%04X dropped\n",
                                    static_cast<unsigned>(msg.op));
                }
            }
            break;
        }

        case IPC_CLI_WAIT_CONNECT:
        case IPC_CLI_WAIT_SEND_READY:
        case IPC_CLI_CLOSING:
        default:
            break;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// FlushOutBuffer()
// ---------------------------------------------------------------------------

int IpcClientHandler::FlushOutBuffer()
{
    ACE_GUARD_RETURN(LockType, g, m_outLock, -1);

    if (m_closing)
    {
        return -1;
    }

    const size_t sendLen = m_outBuffer->length();
    if (sendLen == 0)
    {
        reactor()->cancel_wakeup(this, ACE_Event_Handler::WRITE_MASK);
        return 0;
    }

#ifdef MSG_NOSIGNAL
    ssize_t n = peer().send(m_outBuffer->rd_ptr(), sendLen, MSG_NOSIGNAL);
#else
    ssize_t n = peer().send(m_outBuffer->rd_ptr(), sendLen);
#endif

    if (n == 0)
    {
        return -1;
    }
    else if (n == -1)
    {
        if (errno == EWOULDBLOCK || errno == EAGAIN)
        {
            return 0;
        }
        return -1;
    }
    else if (n < static_cast<ssize_t>(sendLen))
    {
        m_outBuffer->rd_ptr(static_cast<size_t>(n));
        m_outBuffer->crunch();
        return 0;
    }
    else
    {
        m_outBuffer->reset();
        reactor()->cancel_wakeup(this, ACE_Event_Handler::WRITE_MASK);
        return 0;
    }
}
