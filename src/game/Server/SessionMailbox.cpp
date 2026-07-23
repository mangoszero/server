#include "SessionMailbox.h"

SessionMailbox::~SessionMailbox()
{
    Close();
}

bool SessionMailbox::Enqueue(std::unique_ptr<WorldPacket> packet)
{
    if (!packet)
    {
        return false;
    }

    std::lock_guard<std::mutex> guard(m_stateLock);
    if (m_closed)
    {
        return false;
    }

    WorldPacket* const accepted = packet.get();
    m_packets.add(accepted);
    return packet.release() == accepted;
}

bool SessionMailbox::Next(WorldPacket*& packet)
{
    std::lock_guard<std::mutex> guard(m_stateLock);
    if (m_closed)
    {
        return false;
    }
    return m_packets.next(packet);
}

void SessionMailbox::Close()
{
    {
        std::lock_guard<std::mutex> guard(m_stateLock);
        if (m_closed)
        {
            return;
        }
        m_closed = true;
    }

    WorldPacket* packet = nullptr;
    while (m_packets.next(packet))
    {
        delete packet;
    }
}

bool SessionMailbox::IsClosed() const
{
    std::lock_guard<std::mutex> guard(m_stateLock);
    return m_closed;
}
