#ifndef MANGOS_PROTO_IWORLDGATEWAY_H
#define MANGOS_PROTO_IWORLDGATEWAY_H

#include "Auth/BigNumber.h"
#include "Platform/Define.h"
#include "Utilities/WorldPacket.h"

#include <memory>
#include <string>
#include <vector>

namespace proto
{
using SessionId = uint32;
constexpr SessionId INVALID_SESSION_ID = 0;

enum class AuthStatus : uint8
{
    Ok = 0x0C,
    Failed = 0x0D,
    Reject = 0x0E,
    BadServerProof = 0x0F,
    Unavailable = 0x10,
    SystemError = 0x11,
    BillingError = 0x12,
    BillingExpired = 0x13,
    VersionMismatch = 0x14,
    UnknownAccount = 0x15,
    IncorrectPassword = 0x16,
    SessionExpired = 0x17,
    ServerShuttingDown = 0x18,
    AlreadyLoggingIn = 0x19,
    LoginServerNotFound = 0x1A,
    WaitQueue = 0x1B,
    Banned = 0x1C,
    AlreadyOnline = 0x1D,
    NoTime = 0x1E,
    DatabaseBusy = 0x1F,
    Suspended = 0x20,
    ParentalControl = 0x21
};

struct AuthRequest
{
    uint32 build = 0;
    uint32 unknown = 0;
    std::string account;
    uint32 clientSeed = 0;
    uint8 digest[20]{};
    std::vector<uint8> addonData;
    std::string peerAddress;
};

struct AuthContext
{
    virtual ~AuthContext() = default;
};

struct AuthLookup
{
    AuthStatus status = AuthStatus::UnknownAccount;
    BigNumber sessionKey;
    std::shared_ptr<AuthContext> context;
};

class IClientLink;

class IWorldGateway
{
public:
    virtual ~IWorldGateway() = default;
    virtual bool FilterAuthPacket(WorldPacket& packet) = 0;
    virtual void TracePacket(WorldPacket const& packet, bool incoming) = 0;
    virtual AuthLookup LookupAccount(AuthRequest const& request) = 0;
    virtual SessionId Attach(AuthRequest const& request,
        std::shared_ptr<IClientLink> const& link,
        std::shared_ptr<AuthContext> const& context) = 0;
    virtual void Deliver(SessionId session, WorldPacket&& packet) = 0;
    virtual void Detach(SessionId session) = 0;
};
}

#endif
