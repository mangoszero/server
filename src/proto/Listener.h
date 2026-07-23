#ifndef MANGOS_PROTO_LISTENER_H
#define MANGOS_PROTO_LISTENER_H

#include "IWorldGateway.h"
#include "net/Server.hpp"

#include <string>

namespace proto
{
class Listener
{
public:
    explicit Listener(IWorldGateway& gateway);
    bool Start(uint16 port, std::string const& bindIp);
    void Stop();

private:
    IWorldGateway& m_gateway;
    net::Server m_server;
};
}

#endif
