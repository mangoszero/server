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

#include "IpcServerHandler.h"
#include "IpcVersion.h"
#include "Log/Log.h"

#include <ace/Reactor.h>
#include <ace/OS_NS_unistd.h>
#include <cstdio>

// ---------------------------------------------------------------------------
// Static context injected by IpcThread::run before the acceptor runs.
// All of these are written on the reactor thread before any accept fires.
// ---------------------------------------------------------------------------

BoundedQueue<IpcMessage>* IpcServerHandler::s_pendingInbound = nullptr;
std::string               IpcServerHandler::s_pendingSecret;
IpcServerLink*            IpcServerHandler::s_pendingLink    = nullptr;

void IpcServerHandler::SetPendingContext(BoundedQueue<IpcMessage>* inbound,
                                         const std::string& secret,
                                         IpcServerLink* link)
{
    s_pendingInbound = inbound;
    s_pendingSecret  = secret;
    s_pendingLink    = link;
}

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------

IpcServerHandler::IpcServerHandler()
    : m_outBuffer(nullptr),
      m_state(IPC_SRV_WAIT_HELLO),
      m_inbound(nullptr),
      m_link(nullptr),
      m_closing(false)
{
    reference_counting_policy().value(
        ACE_Event_Handler::Reference_Counting_Policy::ENABLED);
}

IpcServerHandler::~IpcServerHandler()
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
// open() - called by ACE_Acceptor immediately after accept()
// ---------------------------------------------------------------------------

int IpcServerHandler::open(void* /*acceptor*/)
{
    if (m_outBuffer)
    {
        // Already opened - shouldn't happen.
        return -1;
    }

    // Grab context injected before the reactor started.
    m_inbound = s_pendingInbound;
    m_secret  = s_pendingSecret;
    if (s_pendingLink)
    {
        m_link = s_pendingLink;
        m_link->AddRef();
    }

    if (!m_inbound)
    {
        sLog.outError("IpcServerHandler::open: no inbound queue set"
                      " - call SetPendingContext first");
        return -1;
    }

    ACE_NEW_RETURN(m_outBuffer, ACE_Message_Block(k_outBufSize), -1);

    // Register for READ events; reactor owns ref after this.
    if (reactor()->register_handler(this, ACE_Event_Handler::READ_MASK) == -1)
    {
        sLog.outError("IpcServerHandler::open: register_handler failed");
        return -1;
    }

    sLog.outString("IpcServerHandler: child connected, awaiting handshake");

    // Publish ourselves into the link as the reactor-thread handler slot.
    // This runs ON the reactor thread, so the facade never sees a raw pointer.
    if (m_link)
    {
        m_link->handler = this;
    }

    // Reactor now holds a reference; release ours.
    remove_reference();
    return 0;
}

// ---------------------------------------------------------------------------
// close() - ACE close hook
// ---------------------------------------------------------------------------

int IpcServerHandler::close(u_long /*flags*/)
{
    shutdown();
    m_closing = true;
    remove_reference();
    return 0;
}

// ---------------------------------------------------------------------------
// handle_input() - data available on socket
// ---------------------------------------------------------------------------

int IpcServerHandler::handle_input(ACE_HANDLE)
{
    if (m_closing)
    {
        return -1;
    }

    // Read into a stack buffer, append to reassembly ByteBuffer.
    char buf[4096];
    const ssize_t n = peer().recv(buf, sizeof(buf));

    if (n <= 0)
    {
        if (n == 0 || (errno != EWOULDBLOCK && errno != EAGAIN))
        {
            return -1; // connection closed or hard error
        }
        return 0;
    }

    m_recvBuf.append(reinterpret_cast<const uint8*>(buf),
                     static_cast<size_t>(n));

    // Decode as many complete frames as possible.
    while (m_recvBuf.rpos() < m_recvBuf.size())
    {
        IpcMessage msg;
        std::string err;

        if (!IpcMessage::Decode(m_recvBuf, msg, err))
        {
            // The only non-fatal Decode outcomes are "need more bytes" cases
            // ("short header" / "incomplete"); every other error ("version
            // mismatch", "oversize frame", ...) is a corrupt/hostile stream and
            // is fatal. We match the two known transient strings explicitly and
            // treat anything else as fatal - without depending on the exact
            // wording of the fatal strings.
            if (err == "incomplete" || err == "short header")
            {
                break; // need more data
            }

            // Fatal framing error.
            sLog.outError("IpcServerHandler: framing error: %s - closing",
                          err.c_str());
            return handle_close(ACE_INVALID_HANDLE,
                                ACE_Event_Handler::ALL_EVENTS_MASK);
        }

        if (ProcessFrame(msg) == -1)
        {
            return -1;
        }
    }

    // Compact the reassembly buffer. If fully drained, clear it; otherwise drop
    // the consumed front bytes so a peer that always leaves a trailing partial
    // frame cannot make m_recvBuf grow without bound.
    CompactRecvBuf();

    return 0;
}

// ---------------------------------------------------------------------------
// CompactRecvBuf() - drop already-consumed front bytes from the reassembly buf
// ---------------------------------------------------------------------------

void IpcServerHandler::CompactRecvBuf()
{
    const size_t consumed = m_recvBuf.rpos();
    if (consumed == 0)
    {
        return; // nothing consumed; leave the (possibly partial) frame in place
    }

    if (consumed == m_recvBuf.size())
    {
        m_recvBuf.clear();
        return;
    }

    // Move the unconsumed tail to the front and reset positions.
    const size_t remaining = m_recvBuf.size() - consumed;
    ByteBuffer compacted;
    compacted.append(m_recvBuf.contents() + consumed, remaining);
    m_recvBuf = compacted;
}

// ---------------------------------------------------------------------------
// handle_output() - socket ready to write
// ---------------------------------------------------------------------------

int IpcServerHandler::handle_output(ACE_HANDLE)
{
    return FlushOutBuffer();
}

// ---------------------------------------------------------------------------
// handle_close() - connection closed or error
// ---------------------------------------------------------------------------

int IpcServerHandler::handle_close(ACE_HANDLE h, ACE_Reactor_Mask)
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
    // thread notifier stops routing through us. Runs on the reactor thread, so
    // it is serialised against the notifier's drain and our own destruction.
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
// SendFrame() - encode + queue for send
// ---------------------------------------------------------------------------

int IpcServerHandler::SendFrame(const IpcMessage& msg)
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
        // Buffer full - this is a programming error (frames are small).
        sLog.outError("IpcServerHandler::SendFrame: output buffer full");
        return -1;
    }

    m_outBuffer->copy(reinterpret_cast<const char*>(wire.contents()), len);

    if (reactor()->schedule_wakeup(this, ACE_Event_Handler::WRITE_MASK) == -1)
    {
        sLog.outError("IpcServerHandler::SendFrame:"
                      " schedule_wakeup WRITE_MASK failed");
        return -1;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// IsLive / IsClosing
// ---------------------------------------------------------------------------

bool IpcServerHandler::IsLive() const
{
    return m_state == IPC_SRV_LIVE;
}

bool IpcServerHandler::IsClosing() const
{
    return m_closing;
}

// ---------------------------------------------------------------------------
// ProcessFrame() - dispatch a decoded frame through the handshake state machine
// ---------------------------------------------------------------------------

int IpcServerHandler::ProcessFrame(const IpcMessage& msg)
{
    switch (m_state)
    {
        case IPC_SRV_WAIT_HELLO:
        {
            if (msg.op != IPC_HELLO)
            {
                sLog.outError("IpcServerHandler: expected IPC_HELLO,"
                              " got 0x%04X - closing", msg.op);
                return handle_close(ACE_INVALID_HANDLE,
                                    ACE_Event_Handler::ALL_EVENTS_MASK);
            }

            // IPC_HELLO body: uint16 proto, uint32 pid, string secret
            if (msg.body.size() < 6)
            {
                sLog.outError("IpcServerHandler: IPC_HELLO body too short"
                              " - closing");
                return handle_close(ACE_INVALID_HANDLE,
                                    ACE_Event_Handler::ALL_EVENTS_MASK);
            }

            ByteBuffer b;
            b.append(msg.body.contents(), msg.body.size());

            uint16 proto;
            uint32 pid;
            b >> proto >> pid;

            // Remaining bytes are the secret (not null-terminated in the wire
            // buffer; extract as raw string).
            size_t secretLen = b.size() - b.rpos();
            const char* secretPtr =
                reinterpret_cast<const char*>(b.contents() + b.rpos());
            std::string secret(secretPtr, secretLen);

            if (proto != IPC_PROTOCOL_VERSION)
            {
                sLog.outError("IpcServerHandler: proto mismatch"
                              " (got %u, expected %u) - closing",
                              proto, IPC_PROTOCOL_VERSION);
                return handle_close(ACE_INVALID_HANDLE,
                                    ACE_Event_Handler::ALL_EVENTS_MASK);
            }

            if (secret != m_secret)
            {
                sLog.outError("IpcServerHandler: secret mismatch from pid %u"
                              " - closing", pid);
                return handle_close(ACE_INVALID_HANDLE,
                                    ACE_Event_Handler::ALL_EVENTS_MASK);
            }

            sLog.outString("IpcServerHandler: IPC_HELLO OK from pid %u", pid);

            // Reply IPC_HELLO_ACK { gametime = 0 (placeholder) }
            IpcMessage ack;
            ack.op = IPC_HELLO_ACK;
            ack.body << uint32(0); // gametime placeholder
            if (SendFrame(ack) == -1)
            {
                return -1;
            }

            m_state = IPC_SRV_WAIT_READY;
            break;
        }

        case IPC_SRV_WAIT_READY:
        {
            if (msg.op != IPC_READY)
            {
                sLog.outError("IpcServerHandler: expected IPC_READY,"
                              " got 0x%04X - closing", msg.op);
                return handle_close(ACE_INVALID_HANDLE,
                                    ACE_Event_Handler::ALL_EVENTS_MASK);
            }

            m_state = IPC_SRV_LIVE;

            // Publish liveness for the facade. Runs on the reactor thread.
            if (m_link)
            {
                m_link->live.store(true, std::memory_order_release);
            }

            sLog.outString("IpcServerHandler: AH service READY");
            break;
        }

        case IPC_SRV_LIVE:
        {
            // Push all live frames into the inbound queue for the facade.
            if (!m_inbound->push(msg))
            {
                sLog.outError("IpcServerHandler: inbound queue full"
                              " - frame 0x%04X dropped", msg.op);
            }
            break;
        }

        case IPC_SRV_CLOSING:
        default:
            break;
    }

    return 0;
}

// ---------------------------------------------------------------------------
// FlushOutBuffer() - write as much of m_outBuffer as the socket will take
// ---------------------------------------------------------------------------

int IpcServerHandler::FlushOutBuffer()
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
