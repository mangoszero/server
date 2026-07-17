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

#include <cstdio>

#ifdef _WIN32
#include <process.h>
#define IPC_GETPID() static_cast<uint32>(::_getpid())
#else
#include <unistd.h>
#define IPC_GETPID() static_cast<uint32>(::getpid())
#endif

IpcClientHandler::IpcClientHandler(IpcSocket&& sock,
                                   BoundedQueue<IpcMessage>* inbound,
                                   const std::string& secret,
                                   IpcClientLink* link)
    : m_sock(std::move(sock)),
      m_state(IPC_CLI_WAIT_CONNECT),
      m_secret(secret),
      m_inbound(inbound),
      m_link(link),
      m_closing(false)
{
    if (m_link)
    {
        m_link->AddRef();
    }
}

IpcClientHandler::~IpcClientHandler()
{
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

// ---------------------------------------------------------------------------
// SendHello - initiate the handshake
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
// ReceiveLoop
// ---------------------------------------------------------------------------

void IpcClientHandler::ReceiveLoop(std::atomic<bool>& stop)
{
    char buf[4096];

    while (!stop.load(std::memory_order_acquire) &&
           !m_closing.load(std::memory_order_acquire))
    {
        const std::ptrdiff_t n = m_sock.RecvSome(buf, sizeof(buf), 200);
        if (n == -2)
        {
            continue;
        }
        if (n <= 0)
        {
            break;
        }

        m_recvBuf.append(reinterpret_cast<const uint8*>(buf),
                         static_cast<size_t>(n));

        bool fatal = false;
        while (m_recvBuf.rpos() < m_recvBuf.size())
        {
            IpcMessage msg;
            std::string err;

            if (!IpcMessage::Decode(m_recvBuf, msg, err))
            {
                if (err == "incomplete" || err == "short header")
                {
                    break;
                }
                fprintf(stderr, "IpcClientHandler: framing error: %s\n",
                        err.c_str());
                fatal = true;
                break;
            }

            if (ProcessFrame(msg) == -1)
            {
                fatal = true;
                break;
            }
        }

        CompactRecvBuf();

        if (fatal)
        {
            break;
        }
    }

    OnClose();
}

// ---------------------------------------------------------------------------
// CompactRecvBuf
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
// SendFrame
// ---------------------------------------------------------------------------

int IpcClientHandler::SendFrame(const IpcMessage& msg)
{
    if (m_closing.load(std::memory_order_acquire))
    {
        return -1;
    }

    ByteBuffer wire;
    msg.Encode(wire);

    std::lock_guard<std::mutex> g(m_sendMtx);
    if (m_closing.load(std::memory_order_acquire))
    {
        return -1;
    }
    if (!m_sock.SendAll(wire.contents(), wire.size()))
    {
        return -1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// InboundFrameAcceptable - per-opcode size predicate (shared with tests)
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
// ProcessFrame - handshake state machine (client side)
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
                return -1;
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

            IpcMessage ready;
            ready.op = IPC_READY;

            m_state = IPC_CLI_LIVE;

            if (SendFrame(ready) == -1)
            {
                return -1;
            }

            // Publish the live send target and liveness for the facade.
            if (m_link)
            {
                m_link->SetSendTarget(shared_from_this());
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
            // reliable lane; everything else stays on the bounded queue.
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
// OnClose - clear the link and close the socket (idempotent)
// ---------------------------------------------------------------------------

void IpcClientHandler::OnClose()
{
    m_closing.store(true, std::memory_order_release);

    if (m_link)
    {
        m_link->live.store(false, std::memory_order_release);
        m_link->ClearSendTarget(this);
    }

    m_sock.Close();
}
