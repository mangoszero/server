#ifndef MANGOS_PROTO_ICLIENTLINK_H
#define MANGOS_PROTO_ICLIENTLINK_H

#include <string>

class WorldPacket;

namespace proto
{
class IClientLink
{
public:
    virtual ~IClientLink() = default;
    virtual void SendPacket(WorldPacket const& packet) = 0;
    virtual void Close() = 0;
    virtual std::string const& GetRemoteAddress() const = 0;
    virtual bool IsClosed() const = 0;
};
}

#endif
