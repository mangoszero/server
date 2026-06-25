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
#include "Chat.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "World.h"
#include "WorkerSupervisor.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"

#include <cctype>
#include <ctime>
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
        default:
            return "unknown";
    }
}

static bool HasLiveAuction(uint32 auctionId)
{
    for (uint32 i = 0; i < MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        AuctionHouseObject* auctions = sAuctionMgr.GetAuctionsMap(AuctionHouseType(i));
        if (auctions->GetAuction(auctionId))
        {
            return true;
        }
    }

    return false;
}

static bool TerminalizeCustodyRow(CustodyRow const& row, uint8 terminalState)
{
    CharacterDatabase.BeginTransaction();
    CustodyLedger::SetState(row.idemKey, terminalState,
                            static_cast<uint64>(time(NULL)));
    return CharacterDatabase.CommitTransactionChecked();
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

static void PrintRepairRow(ChatHandler& handler, char const* prefix,
                           CustodyRow const& row)
{
    handler.PSendSysMessage(
        "%s key=%s auction=%u kind=%s role=%s state=%u owner=%u amount=%u item=%u",
        prefix, row.idemKey.c_str(), row.auctionId,
        CustodyKindName(row.kind), CustodyRoleName(row.role),
        uint32(row.state), row.ownerGuid, row.amount, row.itemGuid);
}

static bool RepairGoldRow(ChatHandler& handler, CustodyRow const& row)
{
    sLog.outError("ah repair: terminalizing orphan gold custody row without disbursement key=%s auction=%u owner=%u amount=%u",
                  row.idemKey.c_str(), row.auctionId, row.ownerGuid, row.amount);
    if (!TerminalizeCustodyRow(row, CST_TERMINAL_OK))
    {
        sLog.outError("ah repair: checked commit failed for gold row key=%s",
                      row.idemKey.c_str());
        handler.PSendSysMessage("ah repair: checked commit failed for %s.",
                                row.idemKey.c_str());
        return false;
    }

    handler.PSendSysMessage("ah repair: terminalized gold row %s without disbursing %u copper.",
                            row.idemKey.c_str(), row.amount);
    return true;
}

static bool RepairItemRow(ChatHandler& handler, CustodyRow const& row)
{
    sLog.outError("ah repair: terminalizing orphan item custody row without re-mailing key=%s auction=%u item=%u owner=%u; manually verify mAitems/item_instance for residual item_guid=%u",
                  row.idemKey.c_str(), row.auctionId, row.itemGuid,
                  row.ownerGuid, row.itemGuid);

    if (!TerminalizeCustodyRow(row, CST_TERMINAL_OK))
    {
        handler.PSendSysMessage("ah repair: failed to terminalize item row %s.",
                                row.idemKey.c_str());
        return false;
    }

    handler.PSendSysMessage("ah repair: terminalized item row %s without re-mailing; verify item %u manually.",
                            row.idemKey.c_str(), row.itemGuid);
    return true;
}

static bool RepairCustodyRow(ChatHandler& handler, CustodyRow const& row)
{
    if (row.id == 0)
    {
        handler.PSendSysMessage("ah repair: cannot auto-repair synthetic drift %s.",
                                row.idemKey.c_str());
        return false;
    }

    if (row.state != CST_RESERVED)
    {
        handler.PSendSysMessage("ah repair: skipping non-reserved row %s state=%u.",
                                row.idemKey.c_str(), uint32(row.state));
        return false;
    }

    if (HasLiveAuction(row.auctionId))
    {
        handler.PSendSysMessage("ah repair: skipping live-auction drift %s; use force-forfeit after manual verification.",
                                row.idemKey.c_str());
        return false;
    }

    if (row.kind == CUSTODY_ITEM)
    {
        return RepairItemRow(handler, row);
    }

    if (row.kind == CUSTODY_GOLD)
    {
        return RepairGoldRow(handler, row);
    }

    handler.PSendSysMessage("ah repair: skipping unknown custody kind for %s.",
                            row.idemKey.c_str());
    return false;
}

static bool ForceForfeitRow(ChatHandler& handler, CustodyRow const& row)
{
    if (row.id == 0)
    {
        handler.PSendSysMessage("ah repair: cannot force-forfeit synthetic drift %s.",
                                row.idemKey.c_str());
        return false;
    }

    if (row.state != CST_RESERVED)
    {
        handler.PSendSysMessage("ah repair: cannot force-forfeit non-reserved row %s state=%u.",
                                row.idemKey.c_str(), uint32(row.state));
        return false;
    }

    sLog.outError("ah repair: force-forfeit terminalizing custody row without disbursement key=%s auction=%u kind=%u role=%u owner=%u amount=%u item=%u",
                  row.idemKey.c_str(), row.auctionId, uint32(row.kind),
                  uint32(row.role), row.ownerGuid, row.amount, row.itemGuid);
    if (!TerminalizeCustodyRow(row, CST_TERMINAL_OK))
    {
        handler.PSendSysMessage("ah repair: force-forfeit commit failed for %s.",
                                row.idemKey.c_str());
        return false;
    }

    handler.PSendSysMessage("ah repair: force-forfeit terminalized %s without disbursement or item mail.",
                            row.idemKey.c_str());
    return true;
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

    std::vector<CustodyRow> drift;
    CustodyService::ReconcileScan(true, drift);

    PSendSysMessage("ah repair: %u custody-ledger drift row(s) found.",
                    uint32(drift.size()));
    SendSysMessage("ah repair: scope is custody-ledger drift only; legacy tears are not repaired.");

    if (drift.empty())
    {
        if (forceForfeit)
        {
            PSendSysMessage("ah repair: force-forfeit key %s is not current drift.",
                            forceForfeitKey.c_str());
            SetSentErrorMessage(true);
            return false;
        }

        return true;
    }

    if (forceForfeit)
    {
        for (size_t i = 0; i < drift.size(); ++i)
        {
            if (drift[i].idemKey == forceForfeitKey)
            {
                return ForceForfeitRow(*this, drift[i]);
            }
        }

        PSendSysMessage("ah repair: force-forfeit key %s is not current drift.",
                        forceForfeitKey.c_str());
        SetSentErrorMessage(true);
        return false;
    }

    for (size_t i = 0; i < drift.size(); ++i)
    {
        PrintRepairRow(*this, apply ? "apply:" : "dry-run:", drift[i]);
    }

    if (!apply)
    {
        SendSysMessage("ah repair: dry-run only. Use 'ah repair apply' or 'ah repair force-forfeit <key>' to mutate supported rows.");
        return true;
    }

    uint32 repaired = 0;
    uint32 skipped = 0;
    for (size_t i = 0; i < drift.size(); ++i)
    {
        if (RepairCustodyRow(*this, drift[i]))
        {
            ++repaired;
        }
        else
        {
            ++skipped;
        }
    }

    PSendSysMessage("ah repair: apply complete, repaired=%u skipped=%u.",
                    repaired, skipped);
    return true;
}
