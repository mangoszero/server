#ifndef MANGOS_PROTO_CLIENTCONNECTION_H
#define MANGOS_PROTO_CLIENTCONNECTION_H

#include "Auth/AuthCrypt.h"
#include "IClientLink.h"
#include "IWorldGateway.h"
#include "PacketCodec.h"
#include "net/ISession.hpp"

#include <atomic>
#include <mutex>
#include <string>
#include <utility>

namespace proto
{
class ClientConnection final : public net::ISession, public IClientLink
{
public:
    explicit ClientConnection(IWorldGateway& gateway);
    ~ClientConnection() override;

    void setPeerAddress(std::string const& address) override { m_address = address; }
    void setSender(net::Sender sender) override { m_sender = std::move(sender); }
    void setCloser(net::Closer closer) override { m_closer = std::move(closer); }
    std::vector<uint8_t> onConnect() override;
    std::vector<uint8_t> onData(uint8_t const* data, std::size_t len) override;
    void onClose() override;
    bool closed() const override { return m_closed.load(); }

    void SendPacket(WorldPacket const& packet) override;
    void Close() override;
    std::string const& GetRemoteAddress() const override { return m_address; }
    bool IsClosed() const override { return m_closed.load(); }

    static uint32 GetOpenConnectionCount()
    {
        return s_openConnections.load(std::memory_order_relaxed);
    }

private:
    bool HandlePacket(WorldPacket& packet);
    bool HandleAuthSession(WorldPacket& packet);
    SessionId CurrentSession();
    void SendAuthResponse(AuthStatus status);
    std::vector<uint8> EncodePacket(WorldPacket const& packet);

    IWorldGateway& m_gateway;
    std::string m_address;
    PacketCodec m_codec;
    AuthCrypt m_crypt;
    std::mutex m_sendOrderLock;
    std::mutex m_sessionLock;
    uint32 m_seed;
    SessionId m_session = INVALID_SESSION_ID;
    bool m_authStarted = false;
    std::atomic<bool> m_closed{false};
    net::Sender m_sender;
    net::Closer m_closer;

    static std::atomic<uint32> s_openConnections;
};
}

#endif
