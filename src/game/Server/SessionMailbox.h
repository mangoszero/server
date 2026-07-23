#ifndef MANGOS_H_SESSIONMAILBOX
#define MANGOS_H_SESSIONMAILBOX

#include "LockedQueue/LockedQueue.h"
#include "Utilities/WorldPacket.h"

#include <memory>
#include <mutex>

class SessionMailbox
{
public:
    SessionMailbox() = default;
    ~SessionMailbox();

    bool Enqueue(std::unique_ptr<WorldPacket> packet);
    bool Next(WorldPacket*& packet);

    template<class Checker>
    bool Next(WorldPacket*& packet, Checker& checker)
    {
        std::lock_guard<std::mutex> guard(m_stateLock);
        if (m_closed)
        {
            return false;
        }
        return m_packets.next(packet, checker);
    }

    void Close();
    bool IsClosed() const;

private:
    mutable std::mutex m_stateLock;
    bool m_closed = false;
    MaNGOS::LockedQueue<WorldPacket*> m_packets;
};

#endif
