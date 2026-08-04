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
 * @file AhServiceCommands.cpp
 * @brief mangosd CLI commands for the AH subprocess service.
 *
 * Provides the "ah console show|hide" command which sends an IPC_CONSOLE
 * frame to the running ah-service child process, toggling its console
 * window visibility (Windows) or logging the request (POSIX).
 *
 * Usage (mangosd console):
 *   ah console show    -- make the ah-service console window visible
 *   ah console hide    -- hide the ah-service console window
 *   ah repair          -- dry-run custody-ledger drift repair
 *   ah repair apply    -- terminalize supported custody-ledger drift
 *   ah repair force-forfeit <key>
 */

#include "AuctionHouseMgr.h"
#include "AuctionHouseBot/CustodyLedger.h"
#include "AuctionHouseBot/CustodyService.h"
#include "Bag.h"
#include "Chat.h"
#include "Database/DatabaseEnv.h"
#include "Item.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "PlayerMutations.h"
#include "World.h"
#include "WorkerSupervisor.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"

#include <cctype>
#include <ctime>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Helper: check that the supervisor is live and connected.
// Must be called from a ChatHandler member (has access to protected members).
// Returns true if the channel is ready.
// ---------------------------------------------------------------------------

static bool IsAhServiceConnected(WorkerSupervisor* sv)
{
    return sv != NULL && sv->Channel().Connected();
}

static std::string TrimRepairArgs(char const* args)
{
    std::string text = args ? args : "";
    std::string::size_type first = 0;
    while (first < text.size() &&
           std::isspace(static_cast<unsigned char>(text[first])))
    {
        ++first;
    }

    std::string::size_type last = text.size();
    while (last > first &&
           std::isspace(static_cast<unsigned char>(text[last - 1])))
    {
        --last;
    }

    return text.substr(first, last - first);
}

static char const* CustodyKindName(uint8 kind)
{
    switch (kind)
    {
        case CUSTODY_GOLD:
            return "gold";
        case CUSTODY_ITEM:
            return "item";
        default:
            return "unknown";
    }
}

static char const* CustodyRoleName(uint8 role)
{
    switch (role)
    {
        case ROLE_DEPOSIT:
            return "deposit";
        case ROLE_BID:
            return "bid";
        case ROLE_PROCEEDS:
            return "proceeds";
        case ROLE_ITEM:
            return "item";
        case ROLE_RESOLUTION:
            return "resolution";
        default:
            return "unknown";
    }
}

static bool IsBotMarkerKey(std::string const& key)
{
    return key.compare(0, 8, "botlist:") == 0;
}

static bool TerminalizeCustodyRow(CustodyRow const& row, uint8 terminalState)
{
    CharacterDatabase.BeginTransaction();
    CustodyLedger::SetState(row.idemKey, terminalState,
                            static_cast<uint64>(time(NULL)));
    return CharacterDatabase.CommitTransactionChecked();
}

static int RepairHexNibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static bool RepairHexDecode(std::string const& hex, std::string& out)
{
    if (hex.empty() || (hex.size() % 2u) != 0u)
    {
        return false;
    }

    out.clear();
    out.reserve(hex.size() / 2u);
    for (size_t i = 0; i + 1u < hex.size(); i += 2u)
    {
        int const hi = RepairHexNibble(hex[i]);
        int const lo = RepairHexNibble(hex[i + 1u]);
        if (hi < 0 || lo < 0)
        {
            out.clear();
            return false;
        }
        out.push_back(static_cast<char>((hi << 4) | lo));
    }
    return true;
}

static std::string RepairAuctionMailSubject(uint32 itemTemplate,
                                            int32 randomPropertyId,
                                            MailAuctionAnswers mailType)
{
    std::ostringstream subject;
    subject << itemTemplate << ":" << randomPropertyId << ":" << uint32(mailType);
    return subject.str();
}

static bool ReadCommittedCancelJournal(uint32 auctionId, PlayerMutationResult& out)
{
    std::unique_ptr<QueryResult> q(CharacterDatabase.PQuery(
        "SELECT `facts` FROM `ah_worker_journal` "
        "WHERE `auction_id`=%u AND `kind`=%u AND `state` IN (1,3) "
        "ORDER BY `resolved_time` DESC, `created_time` DESC LIMIT 1",
        auctionId, uint32(IPC_PLAYER_CANCEL & 0xFFu)));
    if (!q)
    {
        return false;
    }

    std::string bin;
    if (!RepairHexDecode(q->Fetch()[0].GetCppString(), bin))
    {
        return false;
    }

    ByteBuffer bb;
    bb.append(reinterpret_cast<uint8 const*>(bin.data()), bin.size());
    PlayerMutationResult res;
    if (!res.Decode(bb) || res.facts.auctionId != auctionId)
    {
        return false;
    }

    out = res;
    return true;
}

static bool HasCommittedCancelJournal(uint32 auctionId)
{
    std::unique_ptr<QueryResult> q(CharacterDatabase.PQuery(
        "SELECT 1 FROM `ah_worker_journal` "
        "WHERE `auction_id`=%u AND `kind`=%u AND `state` IN (1,3) LIMIT 1",
        auctionId, uint32(IPC_PLAYER_CANCEL & 0xFFu)));
    return q.get() != NULL;
}

static Item* LoadRepairItemFromDb(uint32 itemGuid, uint32 itemTemplate,
                                  uint32 ownerGuid)
{
    ItemPrototype const* proto = ObjectMgr::GetItemPrototype(itemTemplate);
    if (!proto)
    {
        return NULL;
    }

    std::unique_ptr<QueryResult> q(CharacterDatabase.PQuery(
        "SELECT `data`,`text`,`owner_guid` FROM `item_instance` WHERE `guid`=%u",
        itemGuid));
    if (!q)
    {
        return NULL;
    }

    Item* item = NewItemOrBag(proto);
    if (!item->LoadFromDB(itemGuid, q->Fetch(), ObjectGuid(HIGHGUID_PLAYER, ownerGuid)))
    {
        delete item;
        return NULL;
    }

    return item;
}

enum AhRepairActionStatus
{
    AH_REPAIR_REPAIRED,
    AH_REPAIR_SKIPPED,
    AH_REPAIR_FAILED,
};

struct AhRepairActionResult
{
    AhRepairActionStatus status;
    uint32 repairedRows;
    std::string detail;
};

static AhRepairActionResult RepairAction(AhRepairActionStatus status,
                                         uint32 repairedRows,
                                         std::string const& detail)
{
    AhRepairActionResult result;
    result.status = status;
    result.repairedRows = repairedRows;
    result.detail = detail;
    return result;
}

static AhRepairActionResult RepairCommittedCancelAuction(uint32 auctionId)
{
    std::ostringstream detail;
    if (CustodyLedger::AuctionExists(auctionId))
    {
        detail << "committed cancel auction " << auctionId
               << " became live; skipped";
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    PlayerMutationResult stored;
    if (!ReadCommittedCancelJournal(auctionId, stored))
    {
        detail << "committed cancel journal invalid for auction " << auctionId;
        return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
    }

    MutationFacts const& f = stored.facts;
    if (f.curBid != 0u || f.curBidderGuid != 0u)
    {
        detail << "committed cancel auction " << auctionId
               << " has a live bid; replay unsupported";
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    std::string const depKey = "dep:" + std::to_string(auctionId);
    std::string const itemKey = "item:" + std::to_string(auctionId);

    CustodyRow depRow;
    bool const hasDep = CustodyLedger::Get(depKey, depRow) &&
        depRow.state == CST_RESERVED && depRow.kind == CUSTODY_GOLD &&
        depRow.role == ROLE_DEPOSIT && depRow.auctionId == auctionId;

    CustodyRow itemRow;
    bool const hasItem = CustodyLedger::Get(itemKey, itemRow) &&
        itemRow.state == CST_RESERVED && itemRow.kind == CUSTODY_ITEM &&
        itemRow.role == ROLE_ITEM && itemRow.auctionId == auctionId;

    if (!hasDep && !hasItem)
    {
        detail << "committed cancel auction " << auctionId
               << " has no repairable reserved rows";
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    Item* item = NULL;
    if (hasItem)
    {
        if (itemRow.ownerGuid != f.sellerGuid || itemRow.itemGuid != f.itemGuid)
        {
            detail << "committed cancel custody mismatch for auction "
                   << auctionId;
            return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
        }
        item = LoadRepairItemFromDb(f.itemGuid, f.itemTemplate, f.sellerGuid);
        if (!item)
        {
            detail << "committed cancel item unavailable for auction "
                   << auctionId;
            return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
        }
    }

    uint32 repairedRows = 0;
    CustodyDeferred def;
    CharacterDatabase.BeginTransaction();

    if (hasDep)
    {
        CustodyService::CommitGoldLedgerOnly(depKey);
        ++repairedRows;
    }

    if (hasItem)
    {
        ObjectGuid ownerGuid(HIGHGUID_PLAYER, f.sellerGuid);
        Player* owner = sObjectMgr.GetPlayer(ownerGuid);
        MailDraft itemReturn(
            RepairAuctionMailSubject(f.itemTemplate, f.randomPropertyId,
                                     AUCTION_CANCELED),
            "");
        itemReturn.AddItem(item);
        CustodyService::DeliverItem(def, itemKey, itemReturn,
                                    MailReceiver(owner, ownerGuid),
                                    MailSender(MAIL_AUCTION, uint32(f.houseId),
                                               MAIL_STATIONERY_AUCTION),
                                    MAIL_CHECK_MASK_COPIED);
        ++repairedRows;
    }

    if (!CharacterDatabase.CommitTransactionChecked())
    {
        detail << "committed cancel repair commit failed for auction "
               << auctionId;
        return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
    }

    def.run();
    detail << "replayed committed cancel for auction " << auctionId
           << ", repaired=" << repairedRows;
    return RepairAction(AH_REPAIR_REPAIRED, repairedRows, detail.str());
}

static bool ExtractForceForfeitKey(std::string const& mode, std::string& key)
{
    char const forcePrefix[] = "force-forfeit ";
    char const dashForcePrefix[] = "--force-forfeit ";
    if (mode.compare(0, sizeof(forcePrefix) - 1, forcePrefix) == 0)
    {
        key = TrimRepairArgs(mode.c_str() + sizeof(forcePrefix) - 1);
        return !key.empty();
    }

    if (mode.compare(0, sizeof(dashForcePrefix) - 1, dashForcePrefix) == 0)
    {
        key = TrimRepairArgs(mode.c_str() + sizeof(dashForcePrefix) - 1);
        return !key.empty();
    }

    return false;
}

static void PrintRepairFinding(ChatHandler& handler,
                               CustodyDetailBudget& budget,
                               char const* mode,
                               CustodyFinding const& finding)
{
    if (!budget.Take())
    {
        return;
    }

    CustodyRow const& row = finding.row;
    handler.PSendSysMessage(
        "ah repair detail: mode=%s key=%s auction=%u kind=%s role=%s "
        "row-state=%u owner=%u amount=%u item=%u reason=%s ownership=%s "
        "finding-state=%s",
        mode, row.idemKey.c_str(), row.auctionId,
        CustodyKindName(row.kind), CustodyRoleName(row.role),
        uint32(row.state), row.ownerGuid, row.amount, row.itemGuid,
        CustodyFindingReasonName(finding.reason),
        CustodyRepairOwnershipName(finding.repairOwnership),
        CustodyFindingStateName(finding.state));
}

static void PrintRepairAction(ChatHandler& handler,
                              CustodyDetailBudget& budget,
                              AhRepairActionResult const& result)
{
    if (budget.Take())
    {
        handler.PSendSysMessage("ah repair action: %s", result.detail.c_str());
    }
}

static void PrintRepairSuppression(ChatHandler& handler,
                                   CustodyDetailBudget const& budget)
{
    if (budget.Suppressed())
    {
        handler.PSendSysMessage("ah repair: %u detail(s) suppressed.",
                                budget.Suppressed());
    }
}

static void PrintRepairSummary(ChatHandler& handler, char const* mode,
                               CustodyReconcileReport const& report,
                               uint32 repaired, uint32 skipped, uint32 failed)
{
    handler.PSendSysMessage(
        "ah repair: complete mode=%s confirmed=%u pending=%u sweep-owned=%u "
        "repaired=%u skipped=%u failed=%u.",
        mode, report.confirmedDriftCount, report.pendingBidCount,
        report.sweepOwnedCount, repaired, skipped, failed);
}

bool AhRepairFindingMutationAllowed(CustodyFinding const& finding)
{
    CustodyRow const& row = finding.row;
    return finding.state == CUSTODY_FINDING_CONFIRMED &&
           finding.repairOwnership == CUSTODY_REPAIR_GENERIC &&
           !IsBotMarkerKey(row.idemKey) && row.id != 0 &&
           row.state == CST_RESERVED &&
           !CustodyLedger::AuctionExists(row.auctionId);
}

static AhRepairActionResult RepairGoldRow(CustodyRow const& row)
{
    std::ostringstream detail;
    if (!TerminalizeCustodyRow(row, CST_TERMINAL_OK))
    {
        detail << "checked commit failed for gold row " << row.idemKey;
        return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
    }

    detail << "terminalized gold row " << row.idemKey
           << " without disbursing " << row.amount << " copper";
    return RepairAction(AH_REPAIR_REPAIRED, 1, detail.str());
}

static AhRepairActionResult RepairItemRow(CustodyRow const& row)
{
    std::ostringstream detail;
    if (!TerminalizeCustodyRow(row, CST_TERMINAL_OK))
    {
        detail << "failed to terminalize item row " << row.idemKey;
        return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
    }

    detail << "terminalized item row " << row.idemKey
           << " without re-mailing; verify item " << row.itemGuid;
    return RepairAction(AH_REPAIR_REPAIRED, 1, detail.str());
}

static AhRepairActionResult RepairCustodyRow(CustodyFinding const& finding)
{
    CustodyRow const& row = finding.row;
    std::ostringstream detail;
    if (!AhRepairFindingMutationAllowed(finding))
    {
        detail << "skipped ineligible or live custody row " << row.idemKey;
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    if (row.kind == CUSTODY_ITEM)
    {
        return RepairItemRow(row);
    }

    if (row.kind == CUSTODY_GOLD)
    {
        return RepairGoldRow(row);
    }

    detail << "skipped unknown custody kind for " << row.idemKey;
    return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
}

static AhRepairActionResult ForceForfeitRow(CustodyRow const& row)
{
    std::ostringstream detail;
    if (IsBotMarkerKey(row.idemKey))
    {
        detail << "reserved bot marker " << row.idemKey
               << " cannot be force-forfeited";
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    if (row.id == 0)
    {
        detail << "cannot force-forfeit synthetic drift " << row.idemKey;
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    if (row.state != CST_RESERVED)
    {
        detail << "cannot force-forfeit non-reserved row " << row.idemKey;
        return RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
    }

    if (!TerminalizeCustodyRow(row, CST_TERMINAL_OK))
    {
        detail << "force-forfeit commit failed for " << row.idemKey;
        return RepairAction(AH_REPAIR_FAILED, 0, detail.str());
    }

    detail << "force-forfeit terminalized " << row.idemKey
           << " without disbursement or item mail";
    return RepairAction(AH_REPAIR_REPAIRED, 1, detail.str());
}

static void CountRepairAction(AhRepairActionResult const& result,
                              uint32& repaired, uint32& skipped,
                              uint32& failed)
{
    switch (result.status)
    {
        case AH_REPAIR_REPAIRED:
            repaired += result.repairedRows;
            break;
        case AH_REPAIR_SKIPPED:
            ++skipped;
            break;
        case AH_REPAIR_FAILED:
            ++failed;
            break;
    }
}

// ---------------------------------------------------------------------------
// Command handlers
// ---------------------------------------------------------------------------

/**
 * @brief "ah console show" -- request the child to show its console window.
 */
bool ChatHandler::HandleAhServiceConsoleShowCommand(char* /*args*/)
{
    // Console-only: this toggles the CHILD's console window on the SERVER
    // HOST, which an in-game GM cannot see and never needs. m_session is
    // non-NULL only for an in-game chat invocation; reject those.
    if (m_session)
    {
        PSendSysMessage("This command is only available from the server"
                        " console.");
        SetSentErrorMessage(true);
        return false;
    }

    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (!IsAhServiceConnected(sv))
    {
        SendSysMessage("AH service is not running.");
        SetSentErrorMessage(true);
        return false;
    }

    IpcMessage msg;
    msg.op = IPC_CONSOLE;
    msg.body << static_cast<uint8>(1);
    if (!sv->Channel().SendFrame(msg))
    {
        SendSysMessage("AH service is not responding (send failed).");
        SetSentErrorMessage(true);
        return false;
    }
    SendSysMessage("AH service console show requested.");
    return true;
}

/**
 * @brief "ah console hide" -- request the child to hide its console window.
 */
bool ChatHandler::HandleAhServiceConsoleHideCommand(char* /*args*/)
{
    // Console-only: see HandleAhServiceConsoleShowCommand. An in-game GM
    // cannot see the host's child console window and never needs this.
    if (m_session)
    {
        PSendSysMessage("This command is only available from the server"
                        " console.");
        SetSentErrorMessage(true);
        return false;
    }

    WorkerSupervisor* sv = sWorld.GetAhSupervisor();
    if (!IsAhServiceConnected(sv))
    {
        SendSysMessage("AH service is not running.");
        SetSentErrorMessage(true);
        return false;
    }

    IpcMessage msg;
    msg.op = IPC_CONSOLE;
    msg.body << static_cast<uint8>(0);
    if (!sv->Channel().SendFrame(msg))
    {
        SendSysMessage("AH service is not responding (send failed).");
        SetSentErrorMessage(true);
        return false;
    }
    SendSysMessage("AH service console hide requested.");
    return true;
}

/**
 * @brief "ah repair" -- dry-run or repair custody-ledger drift.
 */
bool ChatHandler::HandleAhRepairCommand(char* args)
{
    if (m_session)
    {
        PSendSysMessage("This command is only available from the server"
                        " console.");
        SetSentErrorMessage(true);
        return false;
    }

    std::string const mode = TrimRepairArgs(args);
    bool const apply = mode == "apply" || mode == "--apply";
    std::string forceForfeitKey;
    bool const forceForfeit = ExtractForceForfeitKey(mode, forceForfeitKey);
    if (!mode.empty() && mode != "--dry-run" && !apply && !forceForfeit)
    {
        SendSysMessage("Syntax: ah repair [--dry-run|apply|force-forfeit <key>]");
        SetSentErrorMessage(true);
        return false;
    }

    CustodyReconcileReport report;
    CustodyService::ReconcileScan(static_cast<uint64>(time(NULL)),
                                  CUSTODY_SCAN_RUNTIME, report);

    CustodyDetailBudget budget(100);
    uint32 repaired = 0;
    uint32 skipped = 0;
    uint32 failed = 0;

    if (forceForfeit)
    {
        AhRepairActionResult result = RepairAction(
            AH_REPAIR_SKIPPED, 0,
            "force-forfeit key is not current drift: " + forceForfeitKey);
        bool found = false;
        if (IsBotMarkerKey(forceForfeitKey))
        {
            result = RepairAction(AH_REPAIR_SKIPPED, 0,
                "reserved bot marker " + forceForfeitKey +
                " cannot be force-forfeited");
        }
        else
        {
            for (size_t i = 0; i < report.findings.size(); ++i)
            {
                if (report.findings[i].row.idemKey == forceForfeitKey)
                {
                    found = true;
                    result = ForceForfeitRow(report.findings[i].row);
                    break;
                }
            }
        }

        CountRepairAction(result, repaired, skipped, failed);
        PrintRepairAction(*this, budget, result);
        PrintRepairSuppression(*this, budget);
        PrintRepairSummary(*this, "force-forfeit", report,
                           repaired, skipped, failed);
        if (!found || result.status != AH_REPAIR_REPAIRED)
        {
            SetSentErrorMessage(true);
            return false;
        }
        return true;
    }

    char const* modeName = apply ? "apply" : "dry-run";
    for (size_t i = 0; i < report.findings.size(); ++i)
    {
        PrintRepairFinding(*this, budget, modeName, report.findings[i]);
    }

    if (!apply)
    {
        PrintRepairSuppression(*this, budget);
        PrintRepairSummary(*this, modeName, report, 0, 0, 0);
        return true;
    }

    std::set<uint32> handledJournalAuctions;
    for (size_t i = 0; i < report.findings.size(); ++i)
    {
        CustodyFinding const& finding = report.findings[i];
        CustodyRow const& row = finding.row;
        AhRepairActionResult result;

        if (finding.state != CUSTODY_FINDING_CONFIRMED ||
            finding.repairOwnership != CUSTODY_REPAIR_GENERIC ||
            IsBotMarkerKey(row.idemKey))
        {
            std::ostringstream detail;
            detail << "skipped " << row.idemKey << " ownership="
                   << CustodyRepairOwnershipName(finding.repairOwnership)
                   << " state=" << CustodyFindingStateName(finding.state);
            result = RepairAction(AH_REPAIR_SKIPPED, 0, detail.str());
            CountRepairAction(result, repaired, skipped, failed);
            PrintRepairAction(*this, budget, result);
            continue;
        }

        if (HasCommittedCancelJournal(row.auctionId))
        {
            if (!handledJournalAuctions.insert(row.auctionId).second)
            {
                continue;
            }
            result = RepairCommittedCancelAuction(row.auctionId);
        }
        else
        {
            result = RepairCustodyRow(finding);
        }

        CountRepairAction(result, repaired, skipped, failed);
        PrintRepairAction(*this, budget, result);
    }

    PrintRepairSuppression(*this, budget);
    PrintRepairSummary(*this, modeName, report, repaired, skipped, failed);
    return failed == 0;
}
