/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

/**
 * @file WorldSession.cpp
 * @brief World session implementation
 *
 * This file implements WorldSession which manages a player's connection
 * to the world server. It handles:
 *
 * - Packet processing and opcode dispatch
 * - Player authentication and login
 * - Character management
 * - Movement and action handling
 * - Chat and social interactions
 *
 * The session filters packets based on thread safety and context:
 * - Map::Update() context: Only process thread-safe packets
 * - World::UpdateSessions() context: Process all packets
 *
 * @see WorldSession for the session class
 * @see proto::IClientLink for the client protocol link
 * @see Opcodes.cpp for opcode registration
 */

#include <zlib.h>
#include "IClientLink.h"
#include <utility>
#include "Common/ServerDefines.h"
#include "Platform/Define.h"
#include "Common/Locales.h"
#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <set>
#include <memory>
#include <type_traits>
#include <variant>
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "OpcodeTable.h"
#include "SessionMailbox.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Player.h"
#include "ObjectMgr.h"
#include "Group.h"
#include "CinematicFlyover.h"
#include "GameTime.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "World.h"
#include "WardenEnforcementPolicy.h"
#include "WardenIncidentStore.h"
#include "WardenManager.h"
#include "BattleGround/BattleGroundMgr.h"
#include "SocialMgr.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_PLAYERBOTS
#include "playerbot.h"
#endif

#include <cstdarg>

/**
 * @brief Helper for Map session filtering
 * @param session World session
 * @param opHandle Opcode handler
 * @return True if packet can be processed in Map::Update
 *
 * Determines if an opcode can be safely processed in the Map::Update
 * thread context based on thread safety requirements.
 */
static bool MapSessionFilterHelper(WorldSession* session, OpcodeHandler const& opHandle)
{
    // we do not process thread-unsafe packets
    if (opHandle.packetProcessing == PROCESS_THREADUNSAFE)
    {
        return false;
    }

    // we do not process not loggined player packets
    Player* plr = session->GetPlayer();
    if (!plr)
    {
        return false;
    }

    // in Map::Update() we do not process packets where player is not in world!
    return plr->IsInWorld();
}

static warden::WardenConfiguration SnapshotWardenConfiguration()
{
    warden::WardenRawConfiguration raw;
    raw.enforcementMode =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_ENFORCEMENT_MODE);
    raw.requireExactProfile =
        sWorld.getConfig(CONFIG_BOOL_WARDEN_REQUIRE_EXACT_PROFILE);
    raw.normalMinSeconds =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_CHECK_INTERVAL_MIN);
    raw.normalMaxSeconds =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_CHECK_INTERVAL_MAX);
    raw.aggressiveMinSeconds =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_AGGRESSIVE_INTERVAL_MIN);
    raw.aggressiveMaxSeconds =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_AGGRESSIVE_INTERVAL_MAX);
    raw.aggressiveThreshold =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_AGGRESSIVE_THRESHOLD);
    raw.banThreshold =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_BAN_THRESHOLD);
    raw.incidentWindowSeconds =
        sWorld.getConfig(CONFIG_UINT32_WARDEN_INCIDENT_WINDOW);
    return warden::NormalizeWardenConfiguration(raw).value;
}

/**
 * @brief Process packet in Map context
 * @param packet Packet to process
 * @return True if packet should be processed
 *
 * Filters packets for processing in Map::Update context.
 * Only processes thread-safe packets when player is in world.
 */
bool MapSessionFilter::Process(WorldPacket* packet)
{
    OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
    if (opHandle.packetProcessing == PROCESS_INPLACE)
    {
        return true;
    }

    // let's check if our opcode can be really processed in Map::Update()
    return MapSessionFilterHelper(m_pSession, opHandle);
}

/**
 * @brief Process packet in World context
 * @param packet Packet to process
 * @return True if packet should be processed
 *
 * Filters packets for processing in World::UpdateSessions context.
 * Processes all packets when player is not in world or when
 * packet handler is not thread-safe.
 */
bool WorldSessionFilter::Process(WorldPacket* packet)
{
    OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
    // check if packet handler is supposed to be safe
    if (opHandle.packetProcessing == PROCESS_INPLACE)
    {
        return true;
    }

    // let's check if our opcode can't be processed in Map::Update()
    return !MapSessionFilterHelper(m_pSession, opHandle);
}

/// WorldSession constructor
WorldSession::WorldSession(uint32 id, std::shared_ptr<proto::IClientLink> link,
                           std::shared_ptr<SessionMailbox> mailbox, AccountTypes sec,
                           time_t mute_time, LocaleConstant locale)
    : WorldSession(id, std::move(link), std::move(mailbox), sec, mute_time,
          locale, warden::AdmissionData())
{
}

WorldSession::WorldSession(uint32 id, std::shared_ptr<proto::IClientLink> link,
                           std::shared_ptr<SessionMailbox> mailbox, AccountTypes sec,
                           time_t mute_time, LocaleConstant locale,
                           warden::AdmissionData&& admission)
    : m_muteTime(mute_time),
    _player(NULL), m_link(std::move(link)),
    m_mailbox(mailbox ? std::move(mailbox) : std::make_shared<SessionMailbox>()),
    m_pendingWardenAdmission(admission.available
        ? std::make_unique<warden::AdmissionData>(std::move(admission))
        : nullptr),
    m_wardenAdmissionHandled(false),
    _security(sec), _accountId(id), _logoutTime(0),
    m_inQueue(false), m_playerLoading(false), m_playerLogout(false), m_playerRecentlyLogout(false), m_playerSave(false),
    m_clientLocale(locale), m_sessionDbcLocale(sWorld.GetAvailableDbcLocale(locale)),
    m_sessionDbLocaleIndex(sObjectMgr.GetIndexForLocale(locale)),
    m_latency(0), m_clientTimeDelay(0), m_tutorialState(TUTORIALDATA_UNCHANGED), m_npcWatchLastGuid(),
    m_pingTracker()
{
    if (m_link)
    {
        m_Address = m_link->GetRemoteAddress();
    }
}

/// WorldSession destructor
WorldSession::~WorldSession()
{
    if (m_pendingWardenAdmission)
    {
        m_pendingWardenAdmission->Clear();
        m_pendingWardenAdmission.reset();
    }
    m_warden.reset();
    m_mailbox->Close();

    ///- unload player if not unloaded
    if (_player)
    {
        LogoutPlayer(true);
    }

    /// - If the client link remains live, close it
    if (m_link)
    {
        m_link->Close();
        m_link.reset();
    }

}

/**
 * @brief Logs an invalid client packet size for the current opcode.
 *
 * @param packet The offending packet.
 * @param size The expected packet size.
 */
void WorldSession::SizeError(WorldPacket const& packet, uint32 size) const
{
    sLog.outError("Client (account %u) send packet %s (%u) with size %zu but expected %u (attempt crash server?), skipped",
        GetAccountId(), LookupOpcodeName(packet.GetOpcode()), packet.GetOpcode(), packet.size(), size);
}

/// Get the player name
char const* WorldSession::GetPlayerName() const
{
    return GetPlayer() ? GetPlayer()->GetName() : "<none>";
}

/// Send a packet to the client
void WorldSession::SendPacket(WorldPacket const* packet)
{
#ifdef ENABLE_PLAYERBOTS
    if (GetPlayer())
    {
        if (GetPlayer()->GetPlayerbotAI())
        {
            GetPlayer()->GetPlayerbotAI()->HandleBotOutgoingPacket(*packet);
        }
        else if (GetPlayer()->GetPlayerbotMgr())
        {
            GetPlayer()->GetPlayerbotMgr()->HandleMasterOutgoingPacket(*packet);
        }
    }
#endif

    if (!m_link)
    {
        return;
    }

    if (opcodeTable[packet->GetOpcode()].status == STATUS_UNHANDLED)
    {
        sLog.outError("SESSION: tried to send an unhandled opcode 0x%.4X", packet->GetOpcode());
        return;
    }

#ifdef MANGOS_DEBUG

    // Code for network use statistic
    static uint64 sendPacketCount = 0;
    static uint64 sendPacketBytes = 0;

    static time_t firstTime = time(NULL);
    static time_t lastTime = firstTime;                     // next 60 secs start time

    static uint64 sendLastPacketCount = 0;
    static uint64 sendLastPacketBytes = 0;

    time_t cur_time = time(NULL);

    if ((cur_time - lastTime) < 60)
    {
        sendPacketCount += 1;
        sendPacketBytes += packet->size();

        sendLastPacketCount += 1;
        sendLastPacketBytes += packet->size();
    }
    else
    {
        uint64 minTime = uint64(cur_time - lastTime);
        uint64 fullTime = uint64(lastTime - firstTime);
        DETAIL_LOG("Send all time packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f time: %u", sendPacketCount, sendPacketBytes, float(sendPacketCount) / fullTime, float(sendPacketBytes) / fullTime, uint32(fullTime));
        DETAIL_LOG("Send last min packets count: " UI64FMTD " bytes: " UI64FMTD " avr.count/sec: %f avr.bytes/sec: %f", sendLastPacketCount, sendLastPacketBytes, float(sendLastPacketCount) / minTime, float(sendLastPacketBytes) / minTime);

        lastTime = cur_time;
        sendLastPacketCount = 1;
        sendLastPacketBytes = packet->wpos();               // wpos is real written size
    }

#endif                                                  // !MANGOS_DEBUG

    m_link->SendPacket(*packet);
}

void WorldSession::SetPendingAddonInfo(std::unique_ptr<WorldPacket> packet)
{
    m_pendingAddonInfo = std::move(packet);
}

void WorldSession::SendPendingAddonInfo()
{
    if (!m_pendingAddonInfo)
    {
        return;
    }

    SendPacket(m_pendingAddonInfo.get());
    m_pendingAddonInfo.reset();
}

void WorldSession::OnAuthenticatedAdmission()
{
    // Admission is one-shot and runs only after AUTH_OK. Queued sessions retain
    // no active Warden object and cannot emit module traffic early.
    if (m_wardenAdmissionHandled)
        return;
    m_wardenAdmissionHandled = true;

    if (!m_pendingWardenAdmission || !m_pendingWardenAdmission->available)
    {
        m_pendingWardenAdmission.reset();
        return;
    }

    warden::AdmissionData admission(std::move(*m_pendingWardenAdmission));
    m_pendingWardenAdmission.reset();
    uint32 const accountId = GetAccountId();

    m_wardenConfiguration = SnapshotWardenConfiguration();
    m_wardenBuild = admission.build;
    m_wardenClientLocale = std::move(admission.clientLocale);
    m_wardenAggressiveUntil = 0;
    m_wardenAggressive = false;
    m_wardenEnforcementClosed = false;
    m_wardenLoggedAnomalies.clear();

    bool const configuredToEnforce =
        m_wardenConfiguration.enforcementMode !=
            warden::WardenEnforcementMode::Observe;
    bool const exactEnforcementProfile =
        warden::IsWardenEnforcementProfile(m_wardenBuild,
            admission.platform, m_wardenClientLocale);
    warden::WardenProfileDisposition const profileDisposition =
        warden::ClassifyWardenProfile(m_wardenConfiguration.enforcementMode,
            m_wardenConfiguration.requireExactProfile,
            exactEnforcementProfile);
    if (profileDisposition == warden::WardenProfileDisposition::Reject)
    {
        sLog.outError("Warden rejected an unprofiled client claim for account "
            "%u (build %u; platform %s; locale %s; enforcement mode %u); "
            "strict exact-profile admission is enabled.", accountId,
            m_wardenBuild, admission.platform.c_str(),
            m_wardenClientLocale.c_str(),
            static_cast<uint32>(m_wardenConfiguration.enforcementMode));
        admission.Clear();
        m_wardenEnforcementClosed = true;
        KickPlayer();
        return;
    }
    if (configuredToEnforce &&
        profileDisposition == warden::WardenProfileDisposition::Observe)
    {
        m_wardenConfiguration.enforcementMode =
            warden::WardenEnforcementMode::Observe;
        sLog.outError("Warden exact-profile admission is disabled for "
            "unprofiled client account %u (build %u; platform %s; locale "
            "%s); forcing observation-only checks.", accountId, m_wardenBuild,
            admission.platform.c_str(), m_wardenClientLocale.c_str());
    }

    if (exactEnforcementProfile)
    {
        std::optional<warden::WardenIncidentWindowState> const history =
            warden::WardenIncidentStore::Instance().Load(accountId,
                m_wardenConfiguration.incidentWindowSeconds,
                m_wardenConfiguration.aggressiveThreshold);
        uint64 const now = static_cast<uint64>(GameTime::GetGameTime());
        if (history)
        {
            m_wardenAggressiveUntil = warden::RebaseIncidentDeadline(
                history->aggressiveUntil, history->databaseNow, now);
            m_wardenAggressive = history->recentCount >=
                m_wardenConfiguration.aggressiveThreshold &&
                m_wardenAggressiveUntil > now;
        }
        else
        {
            sLog.outError("Warden incident history unavailable for account %u "
                "(build %u; locale %s); using normal cadence.", accountId,
                m_wardenBuild, m_wardenClientLocale.c_str());
        }
    }

    warden::WardenCreationOptions options;
    options.configuration = m_wardenConfiguration;
    options.initialAggressive = m_wardenAggressive;
    options.requireMemCatalog =
        profileDisposition == warden::WardenProfileDisposition::Enforce;

    // The send adapter owns only the outer world packet. WardenServer supplies
    // an already encrypted, complete inner body and advances its own stream.
    std::unique_ptr<warden::WardenServer> server =
        warden::WardenManager::Instance().Create(m_wardenBuild,
            admission.platform, m_wardenClientLocale, admission.sessionKey,
            [this](warden::Bytes const& payload)
            {
                if (!m_link || m_link->IsClosed())
                    return false;

                WorldPacket packet(SMSG_WARDEN_DATA, payload.size());
                if (!payload.empty())
                    packet.append(payload.data(), payload.size());
                SendPacket(&packet);
                return m_link && !m_link->IsClosed();
            }, options, [this](auto const& event)
            {
                HandleWardenLifecycle(event);
            }, [this](auto const& batch)
            {
                HandleWardenEvidenceBatch(batch);
            });
    admission.Clear();

    if (!server)
    {
        sLog.outError("Warden unavailable for account %u (build %u): "
            "unsupported or invalid profile for locale %s.", accountId,
            m_wardenBuild, m_wardenClientLocale.c_str());
        if (options.requireMemCatalog)
        {
            sLog.outError("Warden enforcement requires an exact memory "
                "catalogue for account %u; closing the client link.",
                accountId);
            m_wardenEnforcementClosed = true;
            KickPlayer();
        }
        return;
    }

    m_wardenPolicy = std::make_unique<warden::WardenEnforcementPolicy>(
        m_wardenConfiguration.enforcementMode);
    m_warden = std::move(server);
}

void WorldSession::HandleWardenLifecycle(
    warden::WardenLifecycleEvent const& event)
{
    if (m_wardenEnforcementClosed)
        return;

    uint32 const accountId = GetAccountId();
    if (event.state == warden::WardenState::ModuleReady)
    {
        sLog.outString("Warden initialized for account %u "
            "(build %u; module %s).", accountId, m_wardenBuild,
            event.transferCount ? "transferred" : "cache hit");
        return;
    }

    if (event.state != warden::WardenState::Failed)
        return;

    sLog.outError("Warden protocol failed for account %u (build %u): %s.",
        accountId, m_wardenBuild, warden::ToString(event.failure));
    if (!m_wardenPolicy)
        return;

    warden::WardenPolicyDecision const decision =
        m_wardenPolicy->EvaluateLifecycle(event);
    if (decision.action == warden::WardenPolicyAction::Kick)
    {
        sLog.outError("Warden lifecycle enforcement is closing the client "
            "link for account %u.", accountId);
        m_wardenEnforcementClosed = true;
        KickPlayer();
    }
}

void WorldSession::HandleWardenEvidenceBatch(
    warden::WardenEvidenceBatch const& batch)
{
    if (m_wardenEnforcementClosed)
        return;

    uint32 const accountId = GetAccountId();
    bool const operatorPass =
        batch.purpose == warden::CheckPlanPurpose::Initial ||
        batch.purpose == warden::CheckPlanPurpose::AggressiveImmediate;
    bool const confirmation =
        batch.purpose == warden::CheckPlanPurpose::Confirmation;
    auto const firstAnomaly = [this](uint32 category, uint32 checkId,
        uint32 outcome)
    {
        uint64 const key = (uint64(category) << 56u) |
            (uint64(outcome) << 48u) | uint64(checkId);
        return m_wardenLoggedAnomalies.insert(key).second;
    };

    // Only typed classifications and catalogue IDs cross this boundary.
    for (warden::WardenEvidence const& evidence : batch.evidence)
    {
        std::visit([this, accountId, operatorPass, confirmation,
            &firstAnomaly](
            auto const& typedEvidence)
        {
            using Evidence = std::decay_t<decltype(typedEvidence)>;
            Player* const player = GetPlayer();
            bool const playerInWorld = player && player->IsInWorld();

            if constexpr (std::is_same_v<Evidence,
                    warden::TimingEvidence>)
            {
                if (typedEvidence.outcome == warden::TimingOutcome::Stable)
                {
                    if (operatorPass && playerInWorld)
                    {
                        sLog.outString("Warden healthy for player %s "
                            "(account %u).", player->GetName(), accountId);
                    }
                    else if (operatorPass)
                    {
                        sLog.outString("Warden healthy for account %u.",
                            accountId);
                    }
                    else
                    {
                        DEBUG_LOG("Warden timing check passed for account %u.",
                            accountId);
                    }
                    return;
                }

                if (firstAnomaly(1u, 0u,
                    static_cast<uint32>(typedEvidence.outcome)))
                {
                    sLog.outError("Warden timing check %s for account %u "
                        "(observation only).",
                        warden::ToString(typedEvidence.outcome), accountId);
                }
                else
                {
                    DEBUG_LOG("Warden timing check %s for account %u "
                        "(repeat observation).",
                        warden::ToString(typedEvidence.outcome), accountId);
                }
            }
            else if constexpr (std::is_same_v<Evidence,
                    warden::MpqEvidence>)
            {
                if (typedEvidence.outcome == warden::MpqOutcome::Match)
                {
                    if (operatorPass)
                    {
                        sLog.outString("Warden archive check passed for account "
                            "%u (check %u).", accountId,
                            typedEvidence.checkId);
                    }
                    else
                    {
                        DEBUG_LOG("Warden archive check passed for account %u "
                            "(check %u).", accountId, typedEvidence.checkId);
                    }
                    return;
                }

                if (firstAnomaly(2u, typedEvidence.checkId,
                    static_cast<uint32>(typedEvidence.outcome)))
                {
                    sLog.outError("Warden archive check %s for account %u "
                        "(check %u; observation only).",
                        warden::ToString(typedEvidence.outcome), accountId,
                        typedEvidence.checkId);
                }
                else
                {
                    DEBUG_LOG("Warden archive check %s for account %u "
                        "(check %u; repeat observation).",
                        warden::ToString(typedEvidence.outcome), accountId,
                        typedEvidence.checkId);
                }
            }
            else if constexpr (std::is_same_v<Evidence,
                    warden::LuaEvidence>)
            {
                if (typedEvidence.outcome == warden::LuaOutcome::Match)
                {
                    if (operatorPass)
                    {
                        sLog.outString("Warden script check passed for account "
                            "%u (check %u).", accountId,
                            typedEvidence.checkId);
                    }
                    else
                    {
                        DEBUG_LOG("Warden script check passed for account %u "
                            "(check %u).", accountId, typedEvidence.checkId);
                    }
                    return;
                }

                if (firstAnomaly(3u, typedEvidence.checkId,
                    static_cast<uint32>(typedEvidence.outcome)))
                {
                    sLog.outError("Warden script check %s for account %u "
                        "(check %u; observation only).",
                        warden::ToString(typedEvidence.outcome), accountId,
                        typedEvidence.checkId);
                }
                else
                {
                    DEBUG_LOG("Warden script check %s for account %u "
                        "(check %u; repeat observation).",
                        warden::ToString(typedEvidence.outcome), accountId,
                        typedEvidence.checkId);
                }
            }
            else
            {
                static_assert(std::is_same_v<Evidence,
                    warden::MemEvidence>);
                if (typedEvidence.outcome == warden::MemOutcome::Match)
                {
                    if (confirmation)
                    {
                        DEBUG_LOG("Warden memory confirmation matched for "
                            "account %u (check %u).", accountId,
                            typedEvidence.checkId);
                    }
                    else if (operatorPass)
                    {
                        sLog.outString("Warden memory check passed for account "
                            "%u (check %u).", accountId,
                            typedEvidence.checkId);
                    }
                    else
                    {
                        DEBUG_LOG("Warden memory check passed for account %u "
                            "(check %u).", accountId, typedEvidence.checkId);
                    }
                    return;
                }

                uint32 const category = confirmation ? 5u : 4u;
                if (firstAnomaly(category, typedEvidence.checkId,
                    static_cast<uint32>(typedEvidence.outcome)))
                {
                    sLog.outError("Warden memory %s %s for account %u "
                        "(check %u).",
                        confirmation ? "confirmation" : "check",
                        warden::ToString(typedEvidence.outcome), accountId,
                        typedEvidence.checkId);
                }
                else
                {
                    DEBUG_LOG("Warden memory %s %s for account %u "
                        "(check %u; repeat observation).",
                        confirmation ? "confirmation" : "check",
                        warden::ToString(typedEvidence.outcome), accountId,
                        typedEvidence.checkId);
                }
            }
        }, evidence);
    }

    if (!m_wardenPolicy)
    {
        sLog.outError("Warden evidence arrived without a policy for account "
            "%u.", accountId);
        if (m_wardenConfiguration.enforcementMode !=
            warden::WardenEnforcementMode::Observe)
        {
            m_wardenEnforcementClosed = true;
            KickPlayer();
        }
        return;
    }

    for (warden::WardenPolicyDecision const& decision :
        m_wardenPolicy->EvaluateBatch(batch))
    {
        switch (decision.action)
        {
            case warden::WardenPolicyAction::QueueConfirmation:
                if (m_warden &&
                    m_warden->QueueConfirmation(decision.checkId))
                {
                    if (firstAnomaly(6u, decision.checkId,
                        static_cast<uint32>(decision.outcome)))
                    {
                        sLog.outError("Warden isolated confirmation queued for "
                            "account %u (check %u; initial outcome %s).",
                            accountId, decision.checkId,
                            warden::ToString(decision.outcome));
                    }
                    else
                    {
                        DEBUG_LOG("Warden isolated confirmation queued again "
                            "for account %u (check %u; outcome %s).",
                            accountId, decision.checkId,
                            warden::ToString(decision.outcome));
                    }
                    break;
                }

                sLog.outError("Warden could not queue an isolated confirmation "
                    "for account %u (check %u).", accountId,
                    decision.checkId);
                if (m_wardenConfiguration.enforcementMode !=
                    warden::WardenEnforcementMode::Observe)
                {
                    m_wardenEnforcementClosed = true;
                    KickPlayer();
                }
                return;
            case warden::WardenPolicyAction::ConfirmationCleared:
                if (firstAnomaly(7u, decision.checkId,
                    static_cast<uint32>(decision.outcome)))
                {
                    sLog.outString("Warden memory confirmation cleared for "
                        "account %u (check %u); no incident recorded.",
                        accountId, decision.checkId);
                }
                else
                {
                    DEBUG_LOG("Warden memory confirmation cleared again for "
                        "account %u (check %u).", accountId,
                        decision.checkId);
                }
                break;
            case warden::WardenPolicyAction::ConfirmedObservation:
                if (firstAnomaly(8u, decision.checkId,
                    static_cast<uint32>(decision.outcome)))
                {
                    sLog.outError("Warden confirmed memory %s for account %u "
                        "(check %u; observe mode, no incident or kick).",
                        warden::ToString(decision.outcome), accountId,
                        decision.checkId);
                }
                else
                {
                    DEBUG_LOG("Warden confirmed memory %s again for account %u "
                        "(check %u; observe mode).",
                        warden::ToString(decision.outcome), accountId,
                        decision.checkId);
                }
                break;
            case warden::WardenPolicyAction::PersistAndKick:
                PersistWardenIncidentAndKick(decision);
                return;
            case warden::WardenPolicyAction::Kick:
                sLog.outError("Warden confirmation contract failed for account "
                    "%u; closing the client link without an incident.",
                    accountId);
                m_wardenEnforcementClosed = true;
                KickPlayer();
                return;
            case warden::WardenPolicyAction::None:
                break;
        }
    }
}

void WorldSession::PersistWardenIncidentAndKick(
    warden::WardenPolicyDecision const& decision)
{
    if (m_wardenEnforcementClosed)
        return;
    m_wardenEnforcementClosed = true;

    uint32 const accountId = GetAccountId();
    std::optional<warden::WardenIncidentOutcome> const outcome =
        warden::ToIncidentOutcome(decision.outcome);
    warden::WardenIncidentWriteResult writeResult;
    if (outcome)
    {
        warden::WardenIncidentContext context;
        context.accountId = accountId;
        context.realmId = realmID;
        context.clientBuild = m_wardenBuild;
        context.clientLocale = m_wardenClientLocale;
        context.checkId = decision.checkId;
        context.outcome = *outcome;
        writeResult = warden::WardenIncidentStore::Instance().Record(context,
            m_wardenConfiguration);
    }
    else
    {
        sLog.outError("Warden rejected a non-negative incident classification "
            "for account %u (check %u).", accountId, decision.checkId);
    }

    warden::WardenIncidentApplication const application =
        warden::ClassifyIncidentWriteResult(writeResult);
    if (!application.durable)
    {
        sLog.outError("Warden confirmed memory %s for account %u (build %u; "
            "locale %s; check %u), but the incident transaction failed; "
            "kicking without a durable incident.",
            warden::ToString(decision.outcome), accountId, m_wardenBuild,
            m_wardenClientLocale.c_str(), decision.checkId);
    }
    else if (!application.summaryKnown)
    {
        sLog.outError("Warden confirmed memory %s for account %u (build %u; "
            "locale %s; check %u); the incident committed but its summary "
            "could not be reloaded; kicking without inventing a count.",
            warden::ToString(decision.outcome), accountId, m_wardenBuild,
            m_wardenClientLocale.c_str(), decision.checkId);
    }
    else
    {
        sLog.outError("Warden confirmed memory %s for account %u (build %u; "
            "locale %s; check %u; recent count %u); kicking.",
            warden::ToString(decision.outcome), accountId, m_wardenBuild,
            m_wardenClientLocale.c_str(), decision.checkId,
            application.recentCount);

        if (application.recentCount ==
            m_wardenConfiguration.aggressiveThreshold)
        {
            if (m_wardenConfiguration.incidentWindowSeconds % 60u == 0u)
            {
                sLog.outError("Warden: account %u reached %u confirmed memory "
                    "kicks within %u minutes; aggressive interrogation "
                    "enabled.", accountId,
                    m_wardenConfiguration.aggressiveThreshold,
                    m_wardenConfiguration.incidentWindowSeconds / 60u);
            }
            else
            {
                sLog.outError("Warden: account %u reached %u confirmed memory "
                    "kicks within %u seconds; aggressive interrogation "
                    "enabled.", accountId,
                    m_wardenConfiguration.aggressiveThreshold,
                    m_wardenConfiguration.incidentWindowSeconds);
            }
        }

        if (application.permanentBanActive)
        {
            sLog.outError("Warden permanent account ban is active for account "
                "%u after %u confirmed memory incidents within the active "
                "window.",
                accountId, application.recentCount);
        }
    }

    KickPlayer();
}

void WorldSession::StartWardenBootstrap()
{
    // Unsupported profiles intentionally have no Warden object. Start is
    // idempotent, so character enumeration and the login safety net may both
    // schedule bootstrap without duplicating MODULE_USE.
    if (m_warden && !m_wardenEnforcementClosed)
        m_warden->Start();
}

void WorldSession::UpdateWarden(uint32 diffMs)
{
    // World::UpdateSessions is the sole deadline owner. Map updates must never
    // advance this clock a second time. The state machine receives only the
    // derived eligibility fact, not a Player or map dependency.
    if (m_warden && !m_wardenEnforcementClosed)
    {
        uint64 const now = static_cast<uint64>(GameTime::GetGameTime());
        bool const aggressive = m_wardenAggressiveUntil > now;
        if (aggressive != m_wardenAggressive)
        {
            m_wardenAggressive = aggressive;
            m_warden->SetAggressive(aggressive);
            DEBUG_LOG("Warden aggressive cadence %s for account %u.",
                aggressive ? "enabled" : "expired", GetAccountId());
        }

        Player* const player = GetPlayer();
        bool const eligible = player && !m_playerLoading && player->IsInWorld();
        m_warden->Update(eligible, diffMs);
    }
}

/// Add an incoming packet to the queue
void WorldSession::QueuePacket(WorldPacket* new_packet)
{
    m_mailbox->Enqueue(std::unique_ptr<WorldPacket>(new_packet));
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnexpectedOpcode(WorldPacket* packet, const char* reason)
{
    sLog.outError("SESSION: received unexpected opcode %s (0x%.4X) %s",
        LookupOpcodeName(packet->GetOpcode()),
        packet->GetOpcode(),
        reason);
}

/// Logging helper for unexpected opcodes
void WorldSession::LogUnprocessedTail(WorldPacket* packet)
{
    sLog.outError("SESSION: opcode %s (0x%.4X) have unprocessed tail data (read stop at %zu from %zu)",
        LookupOpcodeName(packet->GetOpcode()),
        packet->GetOpcode(),
        packet->rpos(), packet->wpos());
}

/// Update the WorldSession (triggered by World update)
bool WorldSession::Update(PacketFilter& updater)
{
    ///- Retrieve packets from the receive queue and call the appropriate handlers
    /// not process packets if the client link already closed
    WorldPacket* packet = NULL;
    while (m_link && !m_link->IsClosed() && m_mailbox->Next(packet, updater))
    {
        /**#if 1
         * sLog.outError( "MOEP: %s (0x%.4X)",
         *                 LookupOpcodeName(packet->GetOpcode()),
         *                 packet->GetOpcode());
         * #endif*/

        OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
        try
        {
            switch (opHandle.status)
            {
                case STATUS_LOGGEDIN:
                    if (!_player)
                    {
                        // skip STATUS_LOGGEDIN opcode unexpected errors if player logout sometime ago - this can be network lag delayed packets
                        if (!m_playerRecentlyLogout)
                        {
                            LogUnexpectedOpcode(packet, "the player has not logged in yet");
                        }
                    }
                    else if (_player->IsInWorld())
                    {
                        ExecuteOpcode(opHandle, packet);
                    }

                    // lag can cause STATUS_LOGGEDIN opcodes to arrive after the player started a transfer

#ifdef ENABLE_PLAYERBOTS
                    if (_player && _player->GetPlayerbotMgr())
                    {
                        _player->GetPlayerbotMgr()->HandleMasterIncomingPacket(*packet);
                    }
#endif
                    break;
                case STATUS_LOGGEDIN_OR_RECENTLY_LOGGEDOUT:
                    if (!_player && !m_playerRecentlyLogout)
                    {
                        LogUnexpectedOpcode(packet, "the player has not logged in yet and not recently logout");
                    }
                    else
                        // not expected _player or must checked in packet hanlder
                    {
                        ExecuteOpcode(opHandle, packet);
                    }
                    break;
                case STATUS_TRANSFER:
                    if (!_player)
                    {
                        LogUnexpectedOpcode(packet, "the player has not logged in yet");
                    }
                    else if (_player->IsInWorld())
                    {
                        LogUnexpectedOpcode(packet, "the player is still in world");
                    }
                    else
                    {
                        ExecuteOpcode(opHandle, packet);
                    }
                    break;
                case STATUS_AUTHED:
                    // A queued client must still receive pong and keep-alive
                    // handling or it will time out while waiting.
                    if (m_inQueue && packet->GetOpcode() != CMSG_PING
                        && packet->GetOpcode() != CMSG_KEEP_ALIVE)
                    {
                        LogUnexpectedOpcode(packet, "the player not pass queue yet");
                        break;
                    }

                    // single from authed time opcodes send in to after logout time
                    // and before other STATUS_LOGGEDIN_OR_RECENTLY_LOGGOUT opcodes.
                    m_playerRecentlyLogout = false;

                    ExecuteOpcode(opHandle, packet);
                    break;
                case STATUS_NEVER:
                    sLog.outError("SESSION: received not allowed opcode %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
                    break;
                case STATUS_UNHANDLED:
                    DEBUG_LOG("SESSION: received not handled opcode %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
                    break;
                default:
                    sLog.outError("SESSION: received wrong-status-req opcode %s (0x%.4X)",
                        LookupOpcodeName(packet->GetOpcode()),
                        packet->GetOpcode());
                    break;
            }
        }
        catch (ByteBufferException&)
        {
            sLog.outError("WorldSession::Update ByteBufferException occured while parsing a packet (opcode: %u) from client %s, accountid=%i.",
                packet->GetOpcode(), GetRemoteAddress().c_str(), GetAccountId());
            if (sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
            {
                DEBUG_LOG("Dumping error causing packet:");
                packet->hexlike();
            }

            if (sWorld.getConfig(CONFIG_BOOL_KICK_PLAYER_ON_BAD_PACKET))
            {
                DETAIL_LOG("Disconnecting session [account id %u / address %s] for badly formatted packet.",
                    GetAccountId(), GetRemoteAddress().c_str());

                KickPlayer();
            }
        }

        delete packet;
    }

#ifdef ENABLE_PLAYERBOTS
    if (GetPlayer() && GetPlayer()->GetPlayerbotMgr())
    {
        GetPlayer()->GetPlayerbotMgr()->UpdateSessions(0);
    }
#endif

    ///- Cleanup client link if needed
    if (m_link && m_link->IsClosed())
    {
        m_link.reset();
    }

    // check if we are safe to proceed with logout
    // logout procedure should happen only in World::UpdateSessions() method!!!
    if (updater.ProcessLogout())
    {
        ///- If necessary, log the player out
        time_t currTime = time(NULL);
        if (!m_link || (ShouldLogOut(currTime) && !m_playerLoading))
        {
            LogoutPlayer(true);
        }

        if (!m_link)
        {
            return false;                                    // Will remove this session from the world session map
        }
    }

    return true;
}

#ifdef ENABLE_PLAYERBOTS

/**
 * @brief Processes queued packets for a playerbot-controlled session.
 */
void WorldSession::HandleBotPackets()
{
    WorldPacket* packet;
    while (m_mailbox->Next(packet))
    {
        OpcodeHandler const& opHandle = opcodeTable[packet->GetOpcode()];
        (this->*opHandle.handler)(*packet);
        delete packet;
    }
}
#endif

/// %Log the player out
void WorldSession::LogoutPlayer(bool Save)
{
    // finish pending transfers before starting the logout
    while (_player && _player->IsBeingTeleportedFar())
    {
        HandleMoveWorldportAckOpcode();
    }

    m_playerLogout = true;
    m_playerSave = Save;

    if (_player)
    {
        // Stop cinematic flyover if active
        if (CinematicFlyover* flyover = _player->GetCinematicFlyover())
        {
            if (flyover->IsActive())
            {
                flyover->Stop();
            }
        }

#ifdef ENABLE_PLAYERBOTS
        if (GetPlayer()->GetPlayerbotMgr())
        {
            GetPlayer()->GetPlayerbotMgr()->LogoutAllBots();
        }
#endif

        sLog.outChar("Account: %d (IP: %s) Logout Character:[%s] (guid: %u)", GetAccountId(), GetRemoteAddress().c_str(), _player->GetName() , _player->GetGUIDLow());

        if (ObjectGuid lootGuid = GetPlayer()->GetLootGuid())
        {
            DoLootRelease(lootGuid);
        }

#ifdef ENABLE_PLAYERBOTS
        if (_player->GetPlayerbotMgr())
        {
            _player->GetPlayerbotMgr()->LogoutAllBots();
        }
        sRandomPlayerbotMgr.OnPlayerLogout(_player);
#endif

        ///- If the player just died before logging out, make him appear as a ghost
        // FIXME: logout must be delayed in case lost connection with client in time of combat
        if (_player->GetDeathTimer())
        {
            _player->GetHostileRefManager().deleteReferences();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        else if (!_player->getAttackers().empty())
        {
            _player->CombatStop();
            _player->GetHostileRefManager().setOnlineOfflineState(false);
            _player->RemoveAllAurasOnDeath();

            // build set of player who attack _player or who have pet attacking of _player
            std::set<Player*> aset;
            for (Unit::AttackerSet::const_iterator itr = _player->getAttackers().begin(); itr != _player->getAttackers().end(); ++itr)
            {
                Unit* owner = (*itr)->GetOwner();           // including player controlled case
                if (owner)
                {
                    if (owner->GetTypeId() == TYPEID_PLAYER)
                    {
                        aset.insert((Player*)owner);
                    }
                }
                else if ((*itr)->GetTypeId() == TYPEID_PLAYER)
                {
                    aset.insert((Player*)(*itr));
                }
            }

            _player->SetPvPDeath(!aset.empty());
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();

            // give honor to all attackers from set like group case
            for (std::set<Player*>::const_iterator itr = aset.begin(); itr != aset.end(); ++itr)
            {
                (*itr)->RewardHonor(_player, aset.size());
            }

            // give bg rewards and update counters like kill by first from attackers
            // this can't be called for all attackers.
            if (!aset.empty())
            {
                if (BattleGround* bg = _player->GetBattleGround())
                {
                    bg->HandleKillPlayer(_player, *aset.begin());
                }
            }
        }
        else if (_player->HasAuraType(SPELL_AURA_SPIRIT_OF_REDEMPTION))
        {
            // this will kill character by SPELL_AURA_SPIRIT_OF_REDEMPTION
            _player->RemoveSpellsCausingAura(SPELL_AURA_MOD_SHAPESHIFT);
            //_player->SetDeathPvP(*); set at SPELL_AURA_SPIRIT_OF_REDEMPTION apply time
            _player->KillPlayer();
            _player->BuildPlayerRepop();
            _player->RepopAtGraveyard();
        }
        // drop a flag if player is carrying it
        if (BattleGround* bg = _player->GetBattleGround())
        {
            bg->EventPlayerLoggedOut(_player);
        }

        ///- Teleport to home if the player is in an invalid instance
        if (!_player->m_InstanceValid && !_player->isGameMaster())
        {
            _player->TeleportToHomebind();
            // this is a bad place to call for far teleport because we need player to be in world for successful logout
            // maybe we should implement delayed far teleport logout?
        }

        // FG: finish pending transfers after starting the logout
        // this should fix players beeing able to logout and login back with full hp at death position
        while (_player->IsBeingTeleportedFar())
        {
            HandleMoveWorldportAckOpcode();
        }

        for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
        {
            if (BattleGroundQueueTypeId bgQueueTypeId = _player->GetBattleGroundQueueTypeId(i))
            {
                _player->RemoveBattleGroundQueueId(bgQueueTypeId);
                sBattleGroundMgr.m_BattleGroundQueues[ bgQueueTypeId ].RemovePlayer(_player->GetObjectGuid(), true);
            }
        }

        ///- Reset the online field in the account table
        // no point resetting online in character table here as Player::SaveToDB() will set it to 1 since player has not been removed from world at this stage
        // No SQL injection as AccountID is uint32
#ifdef ENABLE_PLAYERBOTS
        if (!GetPlayer()->GetPlayerbotAI())
        {
            static SqlStatementID id;
            // playerbot mod
            if (!_player->GetPlayerbotAI())
            {
                SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
                stmt.PExecute(uint32(0), GetAccountId());
            }
        }
#else
        static SqlStatementID id;

        SqlStatement stmt = LoginDatabase.CreateStatement(id, "UPDATE `account` SET `active_realm_id` = ? WHERE `id` = ?");
        stmt.PExecute(uint32(0), GetAccountId());
#endif
        ///- If the player is in a guild, update the guild roster and broadcast a logout message to other guild members
        if (Guild* guild = sGuildMgr.GetGuildById(_player->GetGuildId()))
        {
            if (MemberSlot* slot = guild->GetMemberSlot(_player->GetObjectGuid()))
            {
                slot->SetMemberStats(_player);
                slot->UpdateLogoutTime();
            }

            guild->BroadcastEvent(GE_SIGNED_OFF, _player->GetObjectGuid(), _player->GetName());
        }

        ///- Remove pet
        _player->RemovePet(PET_SAVE_AS_CURRENT);

        ///- empty buyback items and save the player in the database
        // some save parts only correctly work in case player present in map/player_lists (pets, etc)
        if (Save)
        {
            _player->SaveToDB();
        }

        ///- Leave all channels before player delete...
        _player->CleanupChannels();
#ifndef ENABLE_PLAYERBOTS
        ///- If the player is in a group (or invited), remove him. If the group if then only 1 person, disband the group.
        _player->UninviteFromGroup();

        // remove player from the group if he is:
        // a) in group; b) not in raid group; c) logging out normally (not being kicked or disconnected)
        if (_player->GetGroup() && !_player->GetGroup()->isRaidGroup() && m_link)
        {
            _player->RemoveFromGroup();
        }
#endif
        ///- Send update to group
        if (_player->GetGroup())
        {
            _player->GetGroup()->SendUpdate();
        }

        ///- Broadcast a logout message to the player's friends
        sSocialMgr.SendFriendStatus(_player, FRIEND_OFFLINE, _player->GetObjectGuid(), true);
        sSocialMgr.RemovePlayerSocial(_player->GetGUIDLow());

#ifdef ENABLE_PLAYERBOTS
        uint32 guid = GetPlayer()->GetGUIDLow();
#endif

        ///- Used by Eluna
#ifdef ENABLE_ELUNA
        if (Eluna* e = sWorld.GetEluna())
        {
            e->OnLogout(_player);
        }
#endif /* ENABLE_ELUNA */

        ///- Remove the player from the world
        // the player may not be in the world when logging out
        // e.g if he got disconnected during a transfer to another map
        // calls to GetMap in this case may cause crashes
        if (_player->IsInWorld())
        {
            Map* _map = _player->GetMap();
            _map->Remove(_player, true);
        }
        else
        {
            _player->CleanupsBeforeDelete();
            Map::DeleteFromWorld(_player);
        }

        ClearNpcWatchLastGuid();
        SetPlayer(NULL);                                    // deleted in Remove/DeleteFromWorld call

        ///- Send the 'logout complete' packet to the client
        WorldPacket data(SMSG_LOGOUT_COMPLETE, 0);
        SendPacket(&data);

        ///- Since each account can only have one online character at any given time, ensure all characters for active account are marked as offline
        // No SQL injection as AccountId is uint32

        static SqlStatementID updChars;
#ifdef ENABLE_PLAYERBOTS
        SqlStatement stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 0 WHERE `account` = ?");
#else
        stmt = CharacterDatabase.CreateStatement(updChars, "UPDATE `characters` SET `online` = 0 WHERE `account` = ?");
#endif
        stmt.PExecute(GetAccountId());

        DEBUG_LOG("SESSION: Sent SMSG_LOGOUT_COMPLETE Message");
    }

    m_playerLogout = false;
    m_playerSave = false;
    m_playerRecentlyLogout = true;
    LogoutRequest(0);
}

/// Kick a player out of the World
void WorldSession::KickPlayer()
{
    if (m_link)
    {
        m_link->Close();
    }
}

void WorldSession::HandlePingOpcode(WorldPacket& recvPacket)
{
    uint32 ping = 0;
    uint32 latency = 0;
    recvPacket >> ping;
    recvPacket >> latency;

    const uint32 fastRun = m_pingTracker.Record(SessionPingTracker::Clock::now());
    if (m_pingTracker.ShouldKick(sWorld.getConfig(CONFIG_UINT32_MAX_OVERSPEED_PINGS),
                                 GetSecurity() == SEC_PLAYER))
    {
        sLog.outError(
            "WorldSession::HandlePingOpcode: account %u kicked for overspeeded "
            "pings (%u in a row), address = %s",
            GetAccountId(), fastRun, GetRemoteAddress().c_str());
        KickPlayer();
        return;
    }

    SetLatency(latency);
    SetClientTimeDelay(0);

    WorldPacket response(SMSG_PONG, 4);
    response << ping;
    SendPacket(&response);
}

void WorldSession::HandleKeepAliveOpcode(WorldPacket& recvPacket)
{
    DEBUG_LOG("CMSG_KEEP_ALIVE ,size: %zu ", recvPacket.size());
}

/// Cancel channeling handler

void WorldSession::SendAreaTriggerMessage(const char* Text, ...)
{
    va_list ap;
    char szStr [1024];
    szStr[0] = '\0';

    va_start(ap, Text);
    vsnprintf(szStr, 1024, Text, ap);
    va_end(ap);

    uint32 length = strlen(szStr) + 1;
    WorldPacket data(SMSG_AREA_TRIGGER_MESSAGE, 4 + length);
    data << length;
    data << szStr;
    SendPacket(&data);
}

/**
 * @brief Sends a formatted notification message to the client.
 *
 * @param format The printf-style message format.
 */
void WorldSession::SendNotification(const char* format, ...)
{
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, format);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data << szStr;
        SendPacket(&data);
    }
}

/**
 * @brief Sends a localized formatted notification message to the client.
 *
 * @param string_id The localization string identifier.
 */
void WorldSession::SendNotification(int32 string_id, ...)
{
    char const* format = GetMangosString(string_id);
    if (format)
    {
        va_list ap;
        char szStr [1024];
        szStr[0] = '\0';
        va_start(ap, string_id);
        vsnprintf(szStr, 1024, format, ap);
        va_end(ap);

        WorldPacket data(SMSG_NOTIFICATION, (strlen(szStr) + 1));
        data << szStr;
        SendPacket(&data);
    }
}

/**
 * @brief Resolves a localized MaNGOS string for this session locale.
 *
 * @param entry The localization entry id.
 * @return const char* The localized string text.
 */
const char* WorldSession::GetMangosString(int32 entry) const
{
    return sObjectMgr.GetMangosString(entry, GetSessionDbLocaleIndex());
}

/**
 * @brief Logs receipt of an unimplemented opcode handler.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_NULL(WorldPacket& recvPacket)
{
    DEBUG_LOG("SESSION: received unimplemented opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

/**
 * @brief Logs receipt of an opcode that should be handled earlier in socket processing.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_EarlyProccess(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received opcode %s (0x%.4X) that must be processed by the protocol layer",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

/**
 * @brief Logs receipt of an opcode reserved for server-side use.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_ServerSide(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received server-side opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

/**
 * @brief Logs receipt of a deprecated client opcode.
 *
 * @param recvPacket The received opcode packet.
 */
void WorldSession::Handle_Deprecated(WorldPacket& recvPacket)
{
    sLog.outError("SESSION: received deprecated opcode %s (0x%.4X)",
        LookupOpcodeName(recvPacket.GetOpcode()),
        recvPacket.GetOpcode());
}

/**
 * @brief Sends the authentication response or queue position to the client.
 *
 * @param position The queue position, or zero when login may proceed immediately.
 */
void WorldSession::SendAuthWaitQue(uint32 position)
{
    if (position == 0)
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1);
        packet << uint8(AUTH_OK);
        SendPacket(&packet);
    }
    else
    {
        WorldPacket packet(SMSG_AUTH_RESPONSE, 1 + 4);
        packet << uint8(AUTH_WAIT_QUEUE);
        packet << uint32(position);
        SendPacket(&packet);
    }
}

/**
 * @brief Loads tutorial flag state for the current account.
 */
void WorldSession::LoadTutorialsData()
{
    for (int aX = 0 ; aX < 8 ; ++aX)
    {
        m_Tutorials[ aX ] = 0;
    }

    QueryResult* result = CharacterDatabase.PQuery("SELECT `tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7` FROM `character_tutorial` WHERE `account` = '%u'", GetAccountId());

    if (!result)
    {
        m_tutorialState = TUTORIALDATA_NEW;
        return;
    }

    do
    {
        Field* fields = result->Fetch();

        for (int iI = 0; iI < 8; ++iI)
        {
            m_Tutorials[iI] = fields[iI].GetUInt32();
        }
    }
    while (result->NextRow());

    delete result;

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

/**
 * @brief Sends the current tutorial flags to the client.
 */
void WorldSession::SendTutorialsData()
{
    WorldPacket data(SMSG_TUTORIAL_FLAGS, 4 * 8);
    for (uint32 i = 0; i < 8; ++i)
    {
        data << m_Tutorials[i];
    }
    SendPacket(&data);
}

/**
 * @brief Persists tutorial flag state changes for the current account.
 */
void WorldSession::SaveTutorialsData()
{
    static SqlStatementID updTutorial ;
    static SqlStatementID insTutorial ;

    switch (m_tutorialState)
    {
        case TUTORIALDATA_CHANGED:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(updTutorial, "UPDATE `character_tutorial` SET `tut0`=?, `tut1`=?, `tut2`=?, `tut3`=?, `tut4`=?, `tut5`=?, `tut6`=?, `tut7`=? WHERE `account` = ?");
            for (int i = 0; i < 8; ++i)
            {
                stmt.addUInt32(m_Tutorials[i]);
            }

            stmt.addUInt32(GetAccountId());
            stmt.Execute();
        }
        break;

        case TUTORIALDATA_NEW:
        {
            SqlStatement stmt = CharacterDatabase.CreateStatement(insTutorial, "INSERT INTO `character_tutorial` (`account`,`tut0`,`tut1`,`tut2`,`tut3`,`tut4`,`tut5`,`tut6`,`tut7`) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");

            stmt.addUInt32(GetAccountId());
            for (int i = 0; i < 8; ++i)
            {
                stmt.addUInt32(m_Tutorials[i]);
            }

            stmt.Execute();
        }
        break;
        case TUTORIALDATA_UNCHANGED:
            break;
    }

    m_tutorialState = TUTORIALDATA_UNCHANGED;
}

// Send chat information about aborted transfer (mostly used by Player::SendTransferAbortedByLockstatus())
void WorldSession::SendTransferAborted(uint32 mapid, uint8 reason, uint8 arg)
{
    WorldPacket data(SMSG_TRANSFER_ABORTED, 1);
    data << uint8(reason);                                  // transfer abort reason
    SendPacket(&data);
}

/**
 * @brief Executes a validated opcode handler with delayed-teleport protection.
 *
 * @param opHandle The opcode handler metadata.
 * @param packet The packet to process.
 */
void WorldSession::ExecuteOpcode(OpcodeHandler const& opHandle, WorldPacket* packet)
{
#ifdef ENABLE_ELUNA
    if (Eluna* e = sWorld.GetEluna())
    {
        if (!e->OnPacketReceive(this, *packet))
        {
            return;
        }
    }
#endif /* ENABLE_ELUNA */

    // need prevent do internal far teleports in handlers because some handlers do lot steps
    // or call code that can do far teleports in some conditions unexpectedly for generic way work code
    if (_player)
    {
        _player->SetCanDelayTeleport(true);
    }

    (this->*opHandle.handler)(*packet);

    if (_player)
    {
        // can be not set in fact for login opcode, but this not create porblems.
        _player->SetCanDelayTeleport(false);

        // we should execute delayed teleports only for alive(!) players
        // because we don't want player's ghost teleported from graveyard
        if (_player->IsHasDelayedTeleport())
        {
            _player->TeleportTo(_player->m_teleport_dest, _player->m_teleport_options);
        }
    }

    if (packet->rpos() < packet->wpos() && sLog.HasLogLevelOrHigher(LOG_LVL_DEBUG))
    {
        LogUnprocessedTail(packet);
    }
}

/**
 * @brief Sends a spell visual kit to be played on a target object.
 *
 * @param guid The target object guid.
 * @param spellArtKit The spell visual kit id.
 */
void WorldSession::SendPlaySpellVisual(ObjectGuid guid, uint32 spellArtKit)
{
    WorldPacket data(SMSG_PLAY_SPELL_VISUAL, 8 + 4);        // visual effect on guid
    data << guid;
    data << spellArtKit;                                    // index from SpellVisualKit.dbc
    SendPacket(&data);
}
