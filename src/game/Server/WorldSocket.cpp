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
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#include "WorldSocket.h"
#include "Common.h"

#include "Util.h"
#include "World.h"
#include "WorldPacket.h"
#include "SharedDefines.h"
#include "ByteBuffer.h"
#include "AddonHandler.h"
#include "Opcodes.h"
#include "Database/DatabaseEnv.h"
#include "Auth/BigNumber.h"
#include "Auth/Sha1.h"
#include "WorldSession.h"
#include "WorldSocketMgr.h"
#include "Log.h"
#include "DBCStores.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

#include <cstring>
#include <memory>

#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

struct ServerPktHeader
{
    uint16 size;
    uint16 cmd;
};

struct ClientPktHeader
{
    uint16 size;
    uint32 cmd;
};

#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

#ifdef _WIN32
std::atomic<uint32> WorldSocket::s_openConnections{0};
#endif

WorldSocket::WorldSocket()
    : m_closed(false),
      m_recvBuf(),
      m_headerPending(false),
      m_recvOpcode(0),
      m_recvSize(0),
      m_Seed(static_cast<uint32>(rand32())),
      m_LastPingTime(),
      m_hasPinged(false),
      m_OverSpeedPings(0)
{
#ifdef _WIN32
    s_openConnections.fetch_add(1, std::memory_order_relaxed);
#endif
}

WorldSocket::~WorldSocket()
{
#ifdef _WIN32
    s_openConnections.fetch_sub(1, std::memory_order_relaxed);
#endif
}

WorldSocket::SessionLease WorldSocket::GetSession()
{
    return m_session.acquire();
}

void WorldSocket::SetSession(WorldSession* session)
{
    m_session.publish(session);
}

void WorldSocket::DetachSessionAndWait()
{
    m_session.detachAndWait();
}

void WorldSocket::CloseSocket()
{
    if (m_closed.exchange(true))
    {
        return;
    }

    m_session.detach();

    if (m_closer)
    {
        m_closer();
    }
}

std::vector<uint8_t> WorldSocket::EncodePacket(const WorldPacket& pct)
{
    ServerPktHeader header;
    header.cmd  = pct.GetOpcode();
    header.size = static_cast<uint16>(pct.size() + 2);

    EndianConvertReverse(header.size);
    EndianConvert(header.cmd);

    std::vector<uint8_t> frame;
    frame.reserve(sizeof(header) + pct.size());

    {
        // EncryptSend advances the cipher, and SendPacket is reachable from both the
        // world and network threads, so encrypt-and-append must be atomic.
        std::lock_guard<std::mutex> guard(m_CryptSendLock);

        m_Crypt.EncryptSend(reinterpret_cast<uint8*>(&header), sizeof(header));

        const uint8_t* raw = reinterpret_cast<const uint8_t*>(&header);
        frame.insert(frame.end(), raw, raw + sizeof(header));

        if (!pct.empty())
        {
            frame.insert(frame.end(), pct.contents(), pct.contents() + pct.size());
        }
    }

    return frame;
}

int WorldSocket::SendPacket(const WorldPacket& pct)
{
    if (m_closed.load() || !m_sender)
    {
        return -1;
    }

    if (sLog.IsPacketLoggingEnabled())
    {
        sLog.outWorldPacketDump(0, pct.GetOpcode(), pct.GetOpcodeName(), &pct, false);
    }

    const std::vector<uint8_t> frame = EncodePacket(pct);
    m_sender(frame.data(), frame.size());

    return 0;
}

std::vector<uint8_t> WorldSocket::onConnect()
{
    WorldPacket packet(SMSG_AUTH_CHALLENGE, 4);
    packet << m_Seed;

    return EncodePacket(packet);
}

void WorldSocket::onClose()
{
    m_closed.store(true);
    m_session.detach();
}

std::vector<uint8_t> WorldSocket::onData(const uint8_t* data, size_t len)
{
    if (m_closed.load())
    {
        return {};
    }

    m_recvBuf.insert(m_recvBuf.end(), data, data + len);

    size_t pos = 0;
    for (;;)
    {
        if (!m_headerPending)
        {
            if (m_recvBuf.size() - pos < sizeof(ClientPktHeader))
            {
                break;
            }

            ClientPktHeader header;
            memcpy(&header, m_recvBuf.data() + pos, sizeof(header));
            pos += sizeof(header);

            m_Crypt.DecryptRecv(reinterpret_cast<uint8*>(&header), sizeof(header));

            EndianConvertReverse(header.size);
            EndianConvert(header.cmd);

            if ((header.size < 4) || (header.size > 10240) || (header.cmd > 10240))
            {
                sLog.outError("WorldSocket::onData: client sent malformed packet size = %d , cmd = %d",
                              header.size, header.cmd);
                CloseSocket();
                return {};
            }

            m_recvOpcode    = static_cast<uint16>(header.cmd);
            m_recvSize      = header.size - 4u;   // the opcode's own 4 bytes are counted in
            m_headerPending = true;
        }

        if (m_recvBuf.size() - pos < m_recvSize)
        {
            break;
        }

        WorldPacket* pct = new WorldPacket(m_recvOpcode, m_recvSize);
        if (m_recvSize > 0)
        {
            pct->resize(m_recvSize);
            memcpy(const_cast<uint8*>(pct->contents()), m_recvBuf.data() + pos, m_recvSize);
            pos += m_recvSize;
        }
        m_headerPending = false;

        if (ProcessIncoming(pct) == -1)
        {
            CloseSocket();
            return {};
        }

        if (m_closed.load())
        {
            return {};
        }
    }

    if (pos > 0)
    {
        m_recvBuf.erase(m_recvBuf.begin(), m_recvBuf.begin() + pos);
    }

    return {};
}

int WorldSocket::ProcessIncoming(WorldPacket* new_pct)
{
    MANGOS_ASSERT(new_pct);

    std::unique_ptr<WorldPacket> aptr(new_pct);

    const uint16 opcode = new_pct->GetOpcode();

    if (opcode >= NUM_MSG_TYPES)
    {
        sLog.outError("SESSION: received nonexistent opcode 0x%.4X", opcode);
        return -1;
    }

    if (m_closed.load())
    {
        return -1;
    }

    if (sLog.IsPacketLoggingEnabled())
    {
        sLog.outWorldPacketDump(0, new_pct->GetOpcode(), new_pct->GetOpcodeName(), new_pct, true);
    }

    // HandlePing acquires its own narrow leases. In particular, do not keep a
    // ProcessIncoming lease alive across its close-capable paths.
    SessionLease session = opcode == CMSG_PING ? SessionLease{} : GetSession();

    try
    {
        switch (opcode)
        {
            case CMSG_PING:
                return HandlePing(*new_pct);
            case CMSG_AUTH_SESSION:
                if (session)
                {
                    sLog.outError("WorldSocket::ProcessIncoming: Player send CMSG_AUTH_SESSION again");
                    return -1;
                }
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    if (!e->OnPacketReceive(session.get(), *new_pct))
                    {
                        return 0;
                    }
                }
#endif /* ENABLE_ELUNA */
                return HandleAuthSession(*new_pct);
            case CMSG_KEEP_ALIVE:
                DEBUG_LOG("CMSG_KEEP_ALIVE ,size: %zu ", new_pct->size());
#ifdef ENABLE_ELUNA
                if (Eluna* e = sWorld.GetEluna())
                {
                    e->OnPacketReceive(session.get(), *new_pct);
                }
#endif /* ENABLE_ELUNA */
                return 0;
            default:
                if (session)
                {
                    aptr.release();
                    session->QueuePacket(new_pct);
                    return 0;
                }
                sLog.outError("WorldSocket::ProcessIncoming: Client not authed opcode = %u", uint32(opcode));
                return -1;
        }
    }
    catch (ByteBufferException&)
    {
        sLog.outError("WorldSocket::ProcessIncoming ByteBufferException occured while parsing an instant handled packet (opcode: %u) from client %s, accountid=%i.",
            opcode, GetRemoteAddress().c_str(), session ? session->GetAccountId() : -1);

        if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
        {
            DEBUG_LOG("Dumping error-causing packet:");
            new_pct->hexlike();
        }

        if (sWorld.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
        {
            DETAIL_LOG("Disconnecting session [account id %i / address %s] for badly formatted packet.",
                session ? session->GetAccountId() : -1, GetRemoteAddress().c_str());
            return -1;
        }
        return 0;
    }
}

int WorldSocket::HandleAuthSession(WorldPacket& recvPacket)
{
    uint8 digest[SHA_DIGEST_LENGTH];
    uint32 clientSeed;
    uint32 unk2;
    uint32 BuiltNumberClient;
    std::string account;
    BigNumber v, s, g, N, K;
    const bool wardenActive = (sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED) || sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED));

    recvPacket >> BuiltNumberClient;
    recvPacket >> unk2;
    recvPacket >> account;
    recvPacket >> clientSeed;
    recvPacket.read(digest, SHA_DIGEST_LENGTH);

    DEBUG_LOG("WorldSocket::HandleAuthSession: client %u, unk2 %u, account %s, clientseed %u",
        BuiltNumberClient, unk2, account.c_str(), clientSeed);

    if (!IsAcceptableClientBuild(BuiltNumberClient))
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_VERSION_MISMATCH);
        SendPacket(packet);

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (version mismatch).");
        return -1;
    }

    std::string safe_account = account; // Duplicate, else will screw the SHA hash verification below
    LoginDatabase.escape_string(safe_account);

    QueryResult* result =
        LoginDatabase.PQuery("SELECT "
        "`id`, "                      // 0
        "`gmlevel`, "                 // 1
        "`sessionkey`, "              // 2
        "`last_ip`, "                 // 3
        "`locked`, "                  // 4
        "`v`, "                       // 5
        "`s`, "                       // 6
        "`mutetime`, "                // 7
        "`locale`, "                  // 8
        "`os` "                       // 9
        "FROM `account` "
        "WHERE `username` = '%s'",
        safe_account.c_str());

    if (!result)
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_UNKNOWN_ACCOUNT);
        SendPacket(packet);

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (unknown account).");
        return -1;
    }

    const Field* fields = result->Fetch();

    N.SetHexStr("894B645E89E1535BBDAD5B8B290650530801B18EBFBF5E8FAB3C82872A3E9BB7");
    g.SetDword(7);

    v.SetHexStr(fields[5].GetString());
    s.SetHexStr(fields[6].GetString());

    const char* sStr = s.AsHexStr();                        // Must be freed by OPENSSL_free()
    const char* vStr = v.AsHexStr();                        // Must be freed by OPENSSL_free()

    DEBUG_LOG("WorldSocket::HandleAuthSession: (s,v) present: s=%s v=%s",
        sStr && *sStr ? "yes" : "no",
        vStr && *vStr ? "yes" : "no");

    OPENSSL_free((void*) sStr);
    OPENSSL_free((void*) vStr);

    ///- Re-check ip locking (same check as in realmd).
    if (fields[4].GetBool())
    {
        if (strcmp(fields[3].GetString(), GetRemoteAddress().c_str()))
        {
            WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
            packet << uint8(AUTH_FAILED);
            SendPacket(packet);

            delete result;
            BASIC_LOG("WorldSocket::HandleAuthSession: Sent Auth Response (Account IP differs).");
            return -1;
        }
    }

    uint32 id = fields[0].GetUInt32();
    uint32 security = fields[1].GetUInt16();
    if (security > SEC_ADMINISTRATOR)                       // prevent invalid security settings in DB
    {
        security = SEC_ADMINISTRATOR;
    }

    K.SetHexStr(fields[2].GetString());

    time_t mutetime = time_t (fields[7].GetUInt64());

    uint8 tmpLoc = fields[8].GetUInt8();
    LocaleConstant locale = tmpLoc >= MAX_LOCALE ? LOCALE_enUS : LocaleConstant(tmpLoc);

    std::string os = fields[9].GetString();

    delete result;

    // Re-check account ban (same check as in realmd)
    QueryResult* banresult =
        LoginDatabase.PQuery("SELECT 1 FROM `account_banned` WHERE `id` = %u AND `active` = 1 AND (`unbandate` > UNIX_TIMESTAMP() OR `unbandate` = `bandate`)"
        "UNION "
        "SELECT 1 FROM `ip_banned` WHERE (`unbandate` = `bandate` OR `unbandate` > UNIX_TIMESTAMP()) AND `ip` = '%s'",
        id, GetRemoteAddress().c_str());

    if (banresult) // if account banned
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_BANNED);
        SendPacket(packet);

        delete banresult;

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (Account banned).");
        return -1;
    }

    // Check locked state for server
    AccountTypes allowedAccountType = sWorld.GetPlayerSecurityLimit();

    if (allowedAccountType > SEC_PLAYER && AccountTypes(security) < allowedAccountType)
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_UNAVAILABLE);
        SendPacket(packet);

        BASIC_LOG("WorldSocket::HandleAuthSession: User tries to login but his security level is not enough");
        return -1;
    }

    // Must be done before WorldSession is created
    if (wardenActive && os != "Win" && os != "OSX")
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_REJECT);
        SendPacket(packet);

        BASIC_LOG("WorldSocket::HandleAuthSession: Client %s attempted to log in using invalid client OS (%s).", GetRemoteAddress().c_str(), os.c_str());
        return -1;
    }

    // Check that Key and account name are the same on client and server
    uint8 t[4]{ 0 };
    uint32 seed = m_Seed;

    Sha1Hash sha;
    sha.UpdateData(account);
    sha.UpdateData((uint8*) & t, 4);
    sha.UpdateData((uint8*) & clientSeed, 4);
    sha.UpdateData((uint8*) & seed, 4);
    sha.UpdateBigNumbers(&K, nullptr);
    sha.Finalize();

    if (memcmp(sha.GetDigest(), digest, SHA_DIGEST_LENGTH))
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_FAILED);
        SendPacket(packet);

        sLog.outError("WorldSocket::HandleAuthSession: Sent Auth Response (authentification failed).");
        return -1;
    }

    std::string address = GetRemoteAddress();

    DEBUG_LOG("WorldSocket::HandleAuthSession: Client '%s' authenticated successfully from %s.",
        account.c_str(), address.c_str());

    // Update the last_ip in the database. No SQL injection, username escaped.
    static SqlStatementID updAccount;

    SqlStatement stmt = LoginDatabase.CreateStatement(updAccount, "UPDATE `account` SET `last_ip` = ? WHERE `username` = ?");
    stmt.PExecute(address.c_str(), account.c_str());

    WorldSession* session = new WorldSession(id, std::static_pointer_cast<WorldSocket>(shared_from_this()),
                                             AccountTypes(security), mutetime, locale);

    m_Crypt.SetKey(K.AsByteArray(), 40);
    m_Crypt.Init();

    session->LoadTutorialsData();

    if (wardenActive)
    {
        session->InitWarden(uint16(BuiltNumberClient), &K, os);
    }

    SetSession(session);
    sWorld.AddSession(session);

    // Create and send the Addon packet
    WorldPacket SendAddonPacked;
    if (sAddOnHandler.BuildAddonPacket(&recvPacket, &SendAddonPacked))
    {
        SendPacket(SendAddonPacked);
    }

    return 0;
}

int WorldSocket::HandlePing(WorldPacket& recvPacket)
{
    uint32 ping;
    uint32 latency;

    recvPacket >> ping;
    recvPacket >> latency;

    const auto now = std::chrono::steady_clock::now();

    if (!m_hasPinged)
    {
        m_hasPinged    = true;
        m_LastPingTime = now;                                // for 1st ping
    }
    else
    {
        const auto diff = std::chrono::duration_cast<std::chrono::seconds>(now - m_LastPingTime).count();
        m_LastPingTime = now;

        if (diff < 27)
        {
            ++m_OverSpeedPings;

            uint32 max_count = sWorld.getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS);

            if (max_count && m_OverSpeedPings > max_count)
            {
                SessionLease session = GetSession();
                if (session && session->GetSecurity() == SEC_PLAYER)
                {
                    sLog.outError("WorldSocket::HandlePing: Player kicked for overspeeded pings address = %s",
                        GetRemoteAddress().c_str());
                    return -1;
                }
            }
        }
        else
        {
            m_OverSpeedPings = 0;
        }
    }

    SessionLease session = GetSession();
    if (session)
    {
        session->SetLatency(latency);
        session->SetClientTimeDelay(0); // recalculated on next movement packet
    }
    else
    {
        sLog.outError("WorldSocket::HandlePing: peer sent CMSG_PING, but is not authenticated or got recently kicked, address = %s",
            GetRemoteAddress().c_str());
        return -1;
    }

    WorldPacket packet(SMSG_PONG, 4);
    packet << ping;
    return SendPacket(packet);
}
