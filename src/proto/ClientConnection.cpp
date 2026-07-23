#include "ClientConnection.h"

#include "Auth/Sha1.h"
#include "Opcodes.h"
#include "Utilities/Util.h"

#include <cstring>
#include <memory>

namespace proto
{
std::atomic<uint32> ClientConnection::s_openConnections{0};

ClientConnection::ClientConnection(IWorldGateway& gateway)
    : m_gateway(gateway), m_seed(rand32())
{
    s_openConnections.fetch_add(1, std::memory_order_relaxed);
}

ClientConnection::~ClientConnection()
{
    s_openConnections.fetch_sub(1, std::memory_order_relaxed);
}

std::vector<uint8_t> ClientConnection::onConnect()
{
    if (m_closed.load())
    {
        return {};
    }

    try
    {
        WorldPacket challenge(SMSG_AUTH_CHALLENGE, 4);
        challenge << m_seed;
        m_gateway.TracePacket(challenge, false);
        return EncodePacket(challenge);
    }
    catch (...)
    {
        Close();
        return {};
    }
}

std::vector<uint8_t> ClientConnection::onData(
    uint8_t const* data, std::size_t len)
{
    if (m_closed.load())
    {
        return {};
    }

    try
    {
        if (!data && len != 0)
        {
            Close();
            return {};
        }

        std::size_t offset = 0;
        std::vector<WorldPacket> packets;
        while (offset < len && !m_closed.load())
        {
            packets.clear();
            std::size_t consumed = 0;
            DecodeStatus const status =
                m_codec.FeedOne(data + offset, len - offset, consumed, packets);
            offset += consumed;

            if (status == DecodeStatus::Malformed)
            {
                Close();
                break;
            }
            if (status == DecodeStatus::NeedMore)
            {
                break;
            }

            WorldPacket& packet = packets.front();
            m_gateway.TracePacket(packet, true);
            if (!HandlePacket(packet))
            {
                Close();
            }
        }
    }
    catch (...)
    {
        Close();
    }

    return {};
}

void ClientConnection::onClose()
{
    try
    {
        m_closed.store(true);
        SessionId session = INVALID_SESSION_ID;
        {
            std::lock_guard<std::mutex> guard(m_sessionLock);
            session = m_session;
            m_session = INVALID_SESSION_ID;
        }
        if (session != INVALID_SESSION_ID)
        {
            try
            {
                m_gateway.Detach(session);
            }
            catch (...)
            {
            }
        }
    }
    catch (...)
    {
        m_closed.store(true);
    }
}

void ClientConnection::SendPacket(WorldPacket const& packet)
{
    try
    {
        std::lock_guard<std::mutex> guard(m_sendOrderLock);
        if (m_closed.load() || !m_sender)
        {
            return;
        }

        m_gateway.TracePacket(packet, false);
        std::vector<uint8> const frame = PacketCodec::Encode(packet,
            [this](uint8* header, std::size_t len)
            {
                m_crypt.EncryptSend(header, len);
            });
        m_sender(frame.data(), frame.size());
    }
    catch (...)
    {
        Close();
    }
}

void ClientConnection::Close()
{
    if (m_closed.exchange(true))
    {
        return;
    }

    try
    {
        if (m_closer)
        {
            m_closer();
        }
    }
    catch (...)
    {
    }
}

bool ClientConnection::HandlePacket(WorldPacket& packet)
{
    if (packet.GetOpcode() == CMSG_AUTH_SESSION)
    {
        return HandleAuthSession(packet);
    }
    if (packet.GetOpcode() >= NUM_MSG_TYPES)
    {
        return false;
    }

    SessionId const session = CurrentSession();
    if (session == INVALID_SESSION_ID)
    {
        return false;
    }

    m_gateway.Deliver(session, std::move(packet));
    return true;
}

SessionId ClientConnection::CurrentSession()
{
    std::lock_guard<std::mutex> guard(m_sessionLock);
    return m_session;
}

bool ClientConnection::HandleAuthSession(WorldPacket& packet)
{
    if (m_authStarted)
    {
        return false;
    }
    if (!m_gateway.FilterAuthPacket(packet))
    {
        return true;
    }

    m_authStarted = true;
    AuthRequest request;
    packet >> request.build;
    packet >> request.unknown;
    packet >> request.account;
    packet >> request.clientSeed;
    packet.read(request.digest, sizeof(request.digest));
    request.addonData.assign(packet.contents() + packet.rpos(),
        packet.contents() + packet.size());
    request.peerAddress = m_address;

    AuthLookup lookup = m_gateway.LookupAccount(request);
    if (lookup.status != AuthStatus::Ok)
    {
        SendAuthResponse(lookup.status);
        return false;
    }

    uint8 const zero[4] = {0, 0, 0, 0};
    Sha1Hash sha;
    sha.UpdateData(request.account);
    sha.UpdateData(zero, sizeof(zero));
    sha.UpdateData(reinterpret_cast<uint8 const*>(&request.clientSeed),
        sizeof(request.clientSeed));
    sha.UpdateData(reinterpret_cast<uint8 const*>(&m_seed), sizeof(m_seed));
    sha.UpdateBigNumbers(&lookup.sessionKey, nullptr);
    sha.Finalize();

    if (std::memcmp(sha.GetDigest(), request.digest, sizeof(request.digest)) != 0)
    {
        SendAuthResponse(AuthStatus::Failed);
        return false;
    }

    m_crypt.SetKey(lookup.sessionKey.AsByteArray(40), 40);
    m_crypt.Init();
    m_codec.SetHeaderDecryptor(
        [this](uint8* header, std::size_t len)
        {
            m_crypt.DecryptRecv(header, len);
        });

    std::shared_ptr<ClientConnection> const self =
        std::static_pointer_cast<ClientConnection>(shared_from_this());
    std::shared_ptr<IClientLink> const link = self;
    SessionId const session = m_gateway.Attach(request, link, lookup.context);
    if (session == INVALID_SESSION_ID)
    {
        SendAuthResponse(AuthStatus::SystemError);
        return false;
    }

    bool closedDuringAttach = false;
    {
        std::lock_guard<std::mutex> guard(m_sessionLock);
        closedDuringAttach = m_closed.load();
        if (!closedDuringAttach)
        {
            m_session = session;
        }
    }
    if (closedDuringAttach)
    {
        m_gateway.Detach(session);
        return false;
    }

    return true;
}

void ClientConnection::SendAuthResponse(AuthStatus status)
{
    WorldPacket response(SMSG_AUTH_RESPONSE, 1);
    response << uint8(status);
    SendPacket(response);
}

std::vector<uint8> ClientConnection::EncodePacket(WorldPacket const& packet)
{
    std::lock_guard<std::mutex> guard(m_sendOrderLock);
    return PacketCodec::Encode(packet,
        [this](uint8* header, std::size_t len)
        {
            m_crypt.EncryptSend(header, len);
        });
}
}
