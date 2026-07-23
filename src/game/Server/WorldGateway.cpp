#include "WorldGateway.h"

#include "AddonHandler.h"
#include "Auth/BigNumber.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "IClientLink.h"
#include "Log.h"
#include "OpcodeTable.h"
#include "SessionMailbox.h"
#include "SharedDefines.h"
#include "World.h"
#include "WorldSession.h"

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif

#include <cstring>
#include <memory>
#include <string>
#include <utility>

namespace
{
struct AccountRow final : proto::AuthContext
{
    uint32 id = 0;
    AccountTypes security = SEC_PLAYER;
    time_t muteTime = 0;
    LocaleConstant locale = LOCALE_enUS;
    std::string os;
    BigNumber sessionKey;
};

void EnsureDbThreadRegistered()
{
    static thread_local DbThreadGuard guard(&LoginDatabase);
    (void)guard;
}

proto::AuthLookup Rejected(proto::AuthStatus status)
{
    proto::AuthLookup lookup;
    lookup.status = status;
    return lookup;
}
}

bool WorldGateway::FilterAuthPacket(WorldPacket& packet)
{
#ifdef ENABLE_ELUNA
    if (Eluna* eluna = sWorld.GetEluna())
    {
        return eluna->OnPacketReceive(nullptr, packet);
    }
#endif
    return true;
}

void WorldGateway::TracePacket(WorldPacket const& packet, bool incoming)
{
    if (sLog.IsPacketLoggingEnabled())
    {
        sLog.outWorldPacketDump(0, packet.GetOpcode(),
            LookupOpcodeName(packet.GetOpcode()), &packet, incoming);
    }
}

proto::AuthLookup WorldGateway::LookupAccount(proto::AuthRequest const& request)
{
    EnsureDbThreadRegistered();

    if (!IsAcceptableClientBuild(request.build))
    {
        return Rejected(proto::AuthStatus::VersionMismatch);
    }

    std::string safeAccount = request.account;
    LoginDatabase.escape_string(safeAccount);
    std::string safeAddress = request.peerAddress;
    LoginDatabase.escape_string(safeAddress);

    std::unique_ptr<QueryResult> result(LoginDatabase.PQuery(
        "SELECT "
        "`a`.`id`, "
        "`a`.`gmlevel`, "
        "`a`.`sessionkey`, "
        "`a`.`last_ip`, "
        "`a`.`locked`, "
        "`a`.`v`, "
        "`a`.`s`, "
        "`a`.`mutetime`, "
        "`a`.`locale`, "
        "`a`.`os`, "
        "(SELECT 1 FROM `account_banned` WHERE `id` = `a`.`id` AND `active` = 1 "
        "AND (`unbandate` > UNIX_TIMESTAMP() OR `unbandate` = `bandate`) LIMIT 1), "
        "(SELECT 1 FROM `ip_banned` WHERE (`unbandate` = `bandate` "
        "OR `unbandate` > UNIX_TIMESTAMP()) AND `ip` = '%s' LIMIT 1) "
        "FROM `account` AS `a` WHERE `a`.`username` = '%s'",
        safeAddress.c_str(), safeAccount.c_str()));

    if (!result)
    {
        return Rejected(proto::AuthStatus::UnknownAccount);
    }

    Field const* fields = result->Fetch();
    if (fields[10].GetUInt32() || fields[11].GetUInt32())
    {
        return Rejected(proto::AuthStatus::Banned);
    }

    if (fields[4].GetBool()
        && std::strcmp(fields[3].GetString(), request.peerAddress.c_str()) != 0)
    {
        return Rejected(proto::AuthStatus::Failed);
    }

    uint32 security = fields[1].GetUInt16();
    if (security > SEC_ADMINISTRATOR)
    {
        security = SEC_ADMINISTRATOR;
    }

    AccountTypes const allowedAccountType = sWorld.GetPlayerSecurityLimit();
    if (allowedAccountType > SEC_PLAYER
        && AccountTypes(security) < allowedAccountType)
    {
        return Rejected(proto::AuthStatus::Unavailable);
    }

    std::string const os = fields[9].GetString();
    bool const wardenActive = sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED)
        || sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED);
    if (wardenActive && os != "Win" && os != "OSX")
    {
        return Rejected(proto::AuthStatus::Reject);
    }

    char const* verifier = fields[5].GetString();
    char const* salt = fields[6].GetString();
    DEBUG_LOG("WorldGateway::LookupAccount: (s,v) present: s=%s v=%s",
        salt && *salt ? "yes" : "no",
        verifier && *verifier ? "yes" : "no");

    auto row = std::make_shared<AccountRow>();
    row->id = fields[0].GetUInt32();
    row->security = AccountTypes(security);
    row->muteTime = time_t(fields[7].GetUInt64());
    uint8 const locale = fields[8].GetUInt8();
    row->locale = locale >= MAX_LOCALE ? LOCALE_enUS : LocaleConstant(locale);
    row->os = os;
    row->sessionKey.SetHexStr(fields[2].GetString());

    proto::AuthLookup lookup;
    lookup.status = proto::AuthStatus::Ok;
    lookup.sessionKey = row->sessionKey;
    lookup.context = row;
    return lookup;
}

proto::SessionId WorldGateway::Attach(proto::AuthRequest const& request,
    std::shared_ptr<proto::IClientLink> const& link,
    std::shared_ptr<proto::AuthContext> const& context)
{
    EnsureDbThreadRegistered();
    std::shared_ptr<AccountRow> const account =
        std::dynamic_pointer_cast<AccountRow>(context);
    if (!link || !account)
    {
        return proto::INVALID_SESSION_ID;
    }

    static SqlStatementID updateAccount;
    SqlStatement statement = LoginDatabase.CreateStatement(updateAccount,
        "UPDATE `account` SET `last_ip` = ? WHERE `username` = ?");
    statement.PExecute(request.peerAddress.c_str(), request.account.c_str());

    auto mailbox = std::make_shared<SessionMailbox>();
    auto session = std::make_unique<WorldSession>(account->id, link, mailbox,
        account->security, account->muteTime, account->locale);
    session->LoadTutorialsData();

    WorldPacket addonSource(CMSG_AUTH_SESSION, request.addonData.size());
    if (!request.addonData.empty())
    {
        addonSource.append(request.addonData.data(), request.addonData.size());
    }

    bool const wardenActive = sWorld.getConfig(CONFIG_BOOL_WARDEN_WIN_ENABLED)
        || sWorld.getConfig(CONFIG_BOOL_WARDEN_OSX_ENABLED);
    if (wardenActive)
    {
        session->InitWarden(uint16(request.build), &account->sessionKey, account->os);
    }

    WorldPacket addonResponse;
    if (sAddOnHandler.BuildAddonPacket(&addonSource, &addonResponse))
    {
        session->SetPendingAddonInfo(
            std::make_unique<WorldPacket>(std::move(addonResponse)));
    }
    if (link->IsClosed())
    {
        return proto::INVALID_SESSION_ID;
    }

    proto::SessionId sessionId;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        do
        {
            sessionId = ++m_nextSessionId;
            if (sessionId == proto::INVALID_SESSION_ID)
            {
                sessionId = ++m_nextSessionId;
            }
        }
        while (m_routes.find(sessionId) != m_routes.end());
        m_routes.emplace(sessionId, mailbox);
    }

    WorldSession* const publishedSession = session.release();
    try
    {
        sWorld.AddSession(publishedSession);
    }
    catch (...)
    {
        session.reset(publishedSession);
        Detach(sessionId);
        throw;
    }
    return sessionId;
}

void WorldGateway::Deliver(proto::SessionId session, WorldPacket&& packet)
{
    std::shared_ptr<SessionMailbox> mailbox;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        auto const route = m_routes.find(session);
        if (route == m_routes.end())
        {
            return;
        }
        mailbox = route->second;
    }

    mailbox->Enqueue(std::make_unique<WorldPacket>(std::move(packet)));
}

void WorldGateway::Detach(proto::SessionId session)
{
    std::shared_ptr<SessionMailbox> mailbox;
    {
        std::lock_guard<std::mutex> guard(m_lock);
        auto const route = m_routes.find(session);
        if (route == m_routes.end())
        {
            return;
        }
        mailbox = route->second;
        m_routes.erase(route);
    }

    mailbox->Close();
}
