#include "Listener.h"

#include "ClientConnection.h"

#include <memory>

namespace proto
{
Listener::Listener(IWorldGateway& gateway)
    : m_gateway(gateway)
{
}

bool Listener::Start(uint16 port, std::string const& bindIp)
{
    return m_server.start(port,
        [this]() -> std::shared_ptr<net::ISession>
        {
            return std::make_shared<ClientConnection>(m_gateway);
        }, bindIp);
}

void Listener::Stop()
{
    m_server.stop();
}
}
