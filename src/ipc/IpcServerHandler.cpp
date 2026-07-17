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
#include "IpcReliable.h"
#include "IpcVersion.h"
#include "Log/Log.h"

IpcServerHandler::IpcServerHandler(IpcSocket&& sock,
                                   BoundedQueue<IpcMessage>* inbound,
                                   const std::string& secret,
                                   IpcServerLink* link,
                                   uint32 runId,
                                   uint8 writeAuthority)
    : m_sock(std::move(sock)),
      m_state(IPC_SRV_WAIT_HELLO),
      m_secret(secret),
      m_runId(runId),
      m_writeAuthority(writeAuthority),
      m_inbound(inbound),
      m_link(link),
      m_closing(false)
{
    if (m_link)
    {
        m_link->AddRef();
    }
}

IpcServerHandler::~IpcServerHandler()
{
    if (m_link)
    {
        m_link->Release();
        m_link = nullptr;
    }
}

// ---------------------------------------------------------------------------
// ReceiveLoop - reassemble and dispatch frames until close / stop
// ---------------------------------------------------------------------------

void IpcServerHandler::ReceiveLoop(std::atomic<bool>& stop)
{
    char buf[4096];

    while (!stop.load(std::memory_order_acquire) &&
           !m_closing.load(std::memory_order_acquire))
    {
        const std::ptrdiff_t n = m_sock.RecvSome(buf, sizeof(buf), 200);
        if (n == -2)
        {
            continue; // timeout: re-check stop
        }
        if (n <= 0)
        {
            break; // 0 = peer closed, -1 = error
        }

        m_recvBuf.append(reinterpret_cast<const uint8*>(buf),
                         static_cast<size_t>(n));

        bool fatal = false;
        while (m_recvBuf.rpos() < m_recvBuf.size())
        {
            // PF2-C: fail-fast on oversize-for-op BEFORE buffering the body.
            if (m_recvBuf.size() - m_recvBuf.rpos() >= 8)
            {
                if (RejectOversizeForOp())
                {
                    fatal = true;
                    break;
                }
            }

            IpcMessage msg;
            std::string err;

            if (!IpcMessage::Decode(m_recvBuf, msg, err))
            {
                // Only "short header" / "incomplete" are transient; every other
                // error is a corrupt/hostile stream and is fatal.
                if (err == "incomplete" || err == "short header")
                {
                    break;
                }
                sLog.outError("IpcServerHandler: framing error: %s - closing",
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

        // Drop consumed front bytes so a peer that always leaves a trailing
        // partial frame cannot make m_recvBuf grow without bound.
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

void IpcServerHandler::CompactRecvBuf()
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
// RejectOversizeForOp - PF2-C fail-fast on oversize-for-op at header parse
// ---------------------------------------------------------------------------

bool IpcServerHandler::RejectOversizeForOp()
{
    // Caller guarantees >= 8 header bytes at rpos(). Peek opcode (+2) and body
    // length (+4) at absolute offsets WITHOUT advancing rpos, so the subsequent
    // Decode() still owns frame consumption.
    const size_t base = m_recvBuf.rpos();
    const uint16 op  = m_recvBuf.read<uint16>(base + 2);
    const uint32 len = m_recvBuf.read<uint32>(base + 4);

    const IpcBodySizeRule rule = IpcExpectedBodySize(op);
    if (!rule.known)
    {
        return false; // Decode caps at IPC_MAX_FRAME; ProcessFrame drops it.
    }

    if (len > rule.maxLen)
    {
        sLog.outError("IpcServerHandler: opcode 0x%04X declares oversize body"
                      " (%u > %s%u) at header - closing before reassembly",
                      op, len, rule.exact ? "==" : "<=", rule.maxLen);
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// SendFrame - encode and write directly to the socket (thread-safe)
// ---------------------------------------------------------------------------

int IpcServerHandler::SendFrame(const IpcMessage& msg)
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
// ProcessFrame - handshake state machine (server side)
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
                return -1;
            }

            // IPC_HELLO body: uint16 proto, uint32 pid, string secret
            if (msg.body.size() < 6)
            {
                sLog.outError("IpcServerHandler: IPC_HELLO body too short"
                              " - closing");
                return -1;
            }

            ByteBuffer b;
            b.append(msg.body.contents(), msg.body.size());

            uint16 proto;
            uint32 pid;
            b >> proto >> pid;

            const size_t secretLen = b.size() - b.rpos();
            const char* secretPtr =
                reinterpret_cast<const char*>(b.contents() + b.rpos());
            std::string secret(secretPtr, secretLen);

            if (proto != IPC_PROTOCOL_VERSION)
            {
                sLog.outError("IpcServerHandler: proto mismatch"
                              " (got %u, expected %u) - closing",
                              proto, IPC_PROTOCOL_VERSION);
                return -1;
            }

            if (secret != m_secret)
            {
                sLog.outError("IpcServerHandler: secret mismatch from pid %u"
                              " - closing", pid);
                return -1;
            }

            sLog.outString("IpcServerHandler: IPC_HELLO OK from pid %u", pid);

            IpcMessage ack;
            ack.op = IPC_HELLO_ACK;
            ack.body << m_runId << m_writeAuthority; // run-id + SP-2 authority
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
                return -1;
            }

            m_state = IPC_SRV_LIVE;

            // Publish the live send target and liveness for the facade.
            if (m_link)
            {
                m_link->SetSendTarget(shared_from_this());
                m_link->live.store(true, std::memory_order_release);
            }

            sLog.outString("IpcServerHandler: AH service READY");
            break;
        }

        case IPC_SRV_LIVE:
        {
            // The child is UNTRUSTED: validate the body length against the
            // per-opcode rule and reject unknown opcodes, so every accepted
            // frame is tiny.
            const IpcBodySizeRule rule = IpcExpectedBodySize(msg.op);
            const uint32 bodyLen = static_cast<uint32>(msg.body.size());

            if (!rule.known)
            {
                sLog.outError("IpcServerHandler: unknown opcode 0x%04X"
                              " from child - dropping frame", msg.op);
                break;
            }

            const bool sizeOk = rule.exact ? (bodyLen == rule.maxLen)
                                           : (bodyLen <= rule.maxLen);
            if (!sizeOk)
            {
                sLog.outError("IpcServerHandler: opcode 0x%04X bad body size"
                              " (%u, expected %s%u) - dropping frame",
                              msg.op, bodyLen, rule.exact ? "==" : "<=",
                              rule.maxLen);
                break;
            }

            // PF2-B: stamp with our per-connection run-id so the supervisor can
            // drop a frame produced by a PRIOR child that slipped into the queue.
            IpcMessage stamped(msg);
            stamped.generation = m_runId;

            // [SP-2 decision 10] Mutation-class frames ride the UNBOUNDED
            // reliable lane (never dropped); everything else stays on the
            // bounded drop-newest queue charged against its byte budget.
            if (m_link && IpcIsReliableOpcode(stamped.op))
            {
                m_link->PushReliable(stamped);
            }
            else if (!m_inbound->push(stamped, stamped.body.size()))
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
// OnClose - clear the link and close the socket (idempotent)
// ---------------------------------------------------------------------------

void IpcServerHandler::OnClose()
{
    m_closing.store(true, std::memory_order_release);

    if (m_link)
    {
        m_link->live.store(false, std::memory_order_release);
        m_link->ClearSendTarget(this);
        m_link->handlerActive.store(false, std::memory_order_release);
    }

    m_sock.Close();
}
