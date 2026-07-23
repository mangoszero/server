#ifndef MANGOS_H_WORLDGATEWAY
#define MANGOS_H_WORLDGATEWAY

#include "IWorldGateway.h"

#include <memory>
#include <mutex>
#include <unordered_map>

class SessionMailbox;

class WorldGateway final : public proto::IWorldGateway
{
public:
    bool FilterAuthPacket(WorldPacket& packet) override;
    void TracePacket(WorldPacket const& packet, bool incoming) override;
    proto::AuthLookup LookupAccount(proto::AuthRequest const& request) override;
    proto::SessionId Attach(proto::AuthRequest const& request,
        std::shared_ptr<proto::IClientLink> const& link,
        std::shared_ptr<proto::AuthContext> const& context) override;
    void Deliver(proto::SessionId session, WorldPacket&& packet) override;
    void Detach(proto::SessionId session) override;

private:
    std::mutex m_lock;
    proto::SessionId m_nextSessionId = proto::INVALID_SESSION_ID;
    std::unordered_map<proto::SessionId, std::shared_ptr<SessionMailbox>> m_routes;
};

#endif
