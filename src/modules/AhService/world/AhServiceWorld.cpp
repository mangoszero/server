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
 * @file AhServiceWorld.cpp
 * @brief The service's share of the world tick: the supervisor, the frames
 *        the worker sends, and the custody upkeep.
 */

#include "AhServiceWorld.h"
#include "AhService.h"
#include "AhServiceHandlers.h"
#include "AuctionIntentExecutor.h"
#include "AuctionIntents.h"
#include "BrowseMessages.h"
#include "BrowsePending.h"
#include "CustodyLedger.h"
#include "CustodyService.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "MutationPending.h"
#include "PlayerMutations.h"
#include "WorkerSupervisor.h"

#include "Log.h"
#include "ObjectGuid.h"
#include "Opcodes.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include <ctime>
#include <vector>
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

// ---------------------------------------------------------------------------
// AhHandleInbound -- M1 stub; routes consumer frames in M2
// ---------------------------------------------------------------------------

/**
 * @brief Dispatch one inbound frame from the ah-service child process.
 *
 * M1: logs what arrived so the smoke test can confirm the drain pipeline
 * is live. M2 will add IPC_AH_* intent routing here; keep the switch
 * extensible.
 */
void AhHandleInbound(const IpcMessage& msg)
{
    switch (msg.op)
    {
        case IPC_ECHO_REPLY:
        {
            DETAIL_LOG("[AHSupervisor] IPC_ECHO_REPLY received"
                       " (body=%u bytes)", static_cast<unsigned>(msg.body.size()));
            break;
        }
        case IPC_HEARTBEAT_ACK:
        {
            // Should have been consumed by WorkerSupervisor::Tick(); log if
            // it somehow leaks through.
            DETAIL_LOG("[AHSupervisor] HandleAhInbound: unexpected"
                       " IPC_HEARTBEAT_ACK");
            break;
        }
        case IPC_INTENT_SELL:
        case IPC_INTENT_BID:
        case IPC_INTENT_BUYOUT:
        {
            // Authority side: re-validate against live state and apply
            // idempotently. The executor populates `result` with an
            // IPC_INTENT_RESULT frame to return to the child (or leaves it
            // empty -- op 0 -- for a malformed body, which we must not send).
            IpcMessage result;
            sAuctionIntentExecutor.Apply(msg, result);
            // PF3-A: acquire-load the published supervisor pointer (see
            // SetAhSupervisor()) before dereferencing it.
            WorkerSupervisor* const ahSupervisor = sAhService.GetSupervisor();
            if (ahSupervisor != NULL && result.op != IpcOpcode(0))
            {
                ahSupervisor->Channel().SendFrame(result);
            }
            break;
        }
        case IPC_INTENT_RESULT:
        {
            // mangosd -> child direction; should never be received here.
            sLog.outError("[AHSupervisor] HandleAhInbound: unexpected"
                          " IPC_INTENT_RESULT (ignored)");
            break;
        }
        case IPC_GMCMD_RESULT:
        {
            ByteBuffer body(msg.body);
            GmCmdResult res;
            if (res.Decode(body))
            {
                sLog.outString("[AHService] GM command %u result: %s",
                               static_cast<unsigned>(res.cmd),
                               res.ok ? "OK" : "FAIL");
            }
            else
            {
                sLog.outError("[AHSupervisor] HandleAhInbound:"
                              " IPC_GMCMD_RESULT decode failed");
            }
            break;
        }
        case IPC_GMCMD:
        {
            // mangosd -> child direction; should never be received inbound.
            sLog.outError("[AHSupervisor] HandleAhInbound: unexpected"
                          " IPC_GMCMD inbound (ignored)");
            break;
        }
        case IPC_BROWSE_RESULT:
        {
            ByteBuffer body(msg.body);
            BrowseResult res;
            if (!res.Decode(body))
            {
                sLog.outError("[AHSupervisor] IPC_BROWSE_RESULT decode failed");
                break;
            }
            PendingBrowse pb;
            if (!sAhService.GetBrowsePending().Take(res.queryId, pb))
            {
                DETAIL_LOG("[AHSupervisor] IPC_BROWSE_RESULT unknown queryId %llu",
                           (unsigned long long)res.queryId);
                break;
            }
            // I4: ignore a reply for a search the player has since superseded.
            if (!sAhService.GetBrowsePending().IsCurrent(pb.playerGuidLow, pb.kind, pb.seq))
            {
                break;
            }
            Player* player = sPlayerRegistry.Find(
                ObjectGuid(HIGHGUID_PLAYER, pb.playerGuidLow));
            if (!player || !player->IsInWorld())
            {
                break;   // logged out / different char (I4) -> drop cleanly
            }
            WorldSession* session = player->GetSession();
            if (!session)
            {
                break;
            }

            // tooMany: the worker declined (queue saturated / oversize failsafe /
            // over-cap deferred-Eluna). Coordinator model: no in-process fallback
            // -> the player gets "AH unavailable".
            if (res.tooMany)
            {
                AhSendBrowseUnavailable(session, pb.kind);
                break;
            }

            std::vector<BrowseEntry> finalEntries;
            uint32 totalcount = res.totalcount;

            if (res.elunaPending && pb.kind != uint8(BROWSE_BIDDER))
            {
                // Deferred-Eluna pass (D5/V2): run ONLY the OnCanUseItem veto per
                // entry via the thin Player::CanUseItemEluna accessor. The worker
                // already enforced every non-Eluna sub-filter on the same profile.
                // Run the hook over EVERY entry -- NO per-tick budget that keeps
                // unchecked entries (V2: that broke parity by counting/showing
                // Lua-vetoed items).
                std::vector<BrowseEntry> survivors;
                survivors.reserve(res.entries.size());
                for (size_t i = 0; i < res.entries.size(); ++i)
                {
                    if (AhScriptedItemUse(player, res.entries[i].itemEntry) != EQUIP_ERR_OK)
                    {
                        continue;   // Lua veto -> drop (exact parity)
                    }
                    survivors.push_back(res.entries[i]);
                }
                // The worker shipped the FULL surviving set un-paginated (the >cap
                // deferred-Eluna case is declined with tooMany, above). Run the Lua
                // veto over all survivors, then paginate here for exact in-process
                // parity.
                totalcount = uint32(survivors.size());
                uint32 from = pb.listfrom;
                for (uint32 i = from; i < survivors.size() && finalEntries.size() < 50u; ++i)
                {
                    finalEntries.push_back(survivors[i]);
                }
            }
            else
            {
                finalEntries = res.entries;   // worker already paginated
            }

            uint16 opcode = SMSG_AUCTION_LIST_RESULT;
            if (pb.kind == uint8(BROWSE_OWNER))
            {
                opcode = SMSG_AUCTION_OWNER_LIST_RESULT;
            }
            else if (pb.kind == uint8(BROWSE_BIDDER))
            {
                opcode = SMSG_AUCTION_BIDDER_LIST_RESULT;
            }

            WorldPacket data(opcode, 4 + 4 + finalEntries.size() * 60);
            ByteBuffer assembled;
            AhAssembleBrowseListBody(finalEntries, totalcount, assembled);
            data.append(assembled.contents(), assembled.size());
            session->SendPacket(&data);
            break;
        }
        case IPC_PLAYER_RESULT:
        {
            // SP-2 write-authority: worker mutation outcome + book facts.
            // AhHandlePlayerMutationResult applies value only, fail-closed
            // against the custody ledger. (IPC_RESOLVE_APPLY gets its own case
            // in Task 12.)
            ByteBuffer body(msg.body);
            PlayerMutationResult res;
            if (!res.Decode(body))
            {
                sLog.outError("[AHSupervisor] IPC_PLAYER_RESULT decode failed");
                break;
            }
            AhHandlePlayerMutationResult(res);
            break;
        }
        case IPC_RESOLVE_APPLY:
        {
            // SP-2 write-authority: worker-initiated resolution (WON / EXPIRED /
            // CANCELLED_UNLOCK / REPAIR_RETURN). AhHandleResolveApply applies the
            // per-kind value effects inside ONE checked txn with the
            // resolve:<uuid> applied-record (DUPLICATE == APPLIED). An
            // AH_RESOLVE_NO_ACK return is an unrecoverable protocol fault:
            // already alarmed, NO ack is sent (the worker never retries it).
            ByteBuffer body(msg.body);
            ResolveApply ra;
            if (!ra.Decode(body))
            {
                sLog.outError("[AHSupervisor] IPC_RESOLVE_APPLY decode failed");
                break;
            }
            uint8 const st = AhHandleResolveApply(ra);
            if (st == AH_RESOLVE_NO_ACK)
            {
                break;
            }
            WorkerSupervisor* const sv = sAhService.GetSupervisor();
            if (sv != NULL)
            {
                ResolveAck ack;
                ack.uuid   = ra.uuid;
                ack.status = st;
                IpcMessage reply;
                reply.op = IPC_RESOLVE_ACK;
                ack.Encode(reply.body);
                sv->Channel().SendFrame(reply);
            }
            break;
        }
        default:
        {
            DETAIL_LOG("[AHSupervisor] HandleAhInbound: opcode 0x%04X"
                       " (body=%u bytes) -- unhandled",
                       static_cast<unsigned>(msg.op),
                       static_cast<unsigned>(msg.body.size()));
            break;
        }
    }
}


InventoryResult AhScriptedItemUse(Player* player, uint32 itemEntry)
{
#ifdef ENABLE_ELUNA
    if (Eluna* e = player->GetEluna())
    {
        return e->OnCanUseItem(player, itemEntry);
    }
#else
    (void)player;
    (void)itemEntry;
#endif
    return EQUIP_ERR_OK;
}

void AhServiceMaintenance()
{
    // Announce each change of the worker's health. On the edge to active,
    // reconcile every in-flight player-mutation reservation against the shared
    // worker journal (committed => finalize-forward, absent => release,
    // cancel-prepared => abort + release).
    static bool s_prevServiceActive = false;
    bool const serviceActive = sAhService.IsWorkerActive();
    if (serviceActive != s_prevServiceActive)
    {
        if (serviceActive)
        {
            sLog.outString("[AHSupervisor] AH service active");
            AhReconcileOnReconnect();
        }
        else
        {
            sLog.outString("[AHSupervisor] AH service inactive");
        }
        s_prevServiceActive = serviceActive;
    }

    // Custody drift audit + terminal-row TTL prune. Orphan materialization
    // recovery has a separate adaptive timer so outage backlogs drain in
    // bounded batches without repeating the full reconciliation scan.
    static uint64 s_nextCustodyReconcileTime = 0;
    static uint64 s_nextOrphanSweepTime = 0;
    uint64 const now = static_cast<uint64>(sWorld.GetGameTime());
    uint64 const custodyTerminalRetention = 30 * DAY;
    uint32 const orphanSweepBatchSize = 100u;
    CustodyMaintenancePlan const plan =
        CustodyService::GetMaintenancePlan(sAhService.IsCustodyEnabled(),
                                           sAhService.IsWriteAuthority());
    if (!s_nextCustodyReconcileTime)
    {
        s_nextCustodyReconcileTime = now + HOUR;
    }
    else if (now >= s_nextCustodyReconcileTime)
    {
        if (plan.reconcile)
        {
            CustodyReconcileReport report;
            CustodyService::ReconcileScan(now, CUSTODY_SCAN_RUNTIME, report);
            CustodyService::LogReconcileReport("hourly", report);
        }

        if (plan.prune && now > custodyTerminalRetention)
        {
            CustodyLedger::DeleteTerminalOlderThan(now - custodyTerminalRetention);
        }

        s_nextCustodyReconcileTime = now + HOUR;
    }

    if (!s_nextOrphanSweepTime)
    {
        s_nextOrphanSweepTime = now + MINUTE;
    }
    else if (now >= s_nextOrphanSweepTime)
    {
        // Only WriteAuthority materializes bot listings through this path. A
        // full batch (or failed commit) retries in one minute; a drained queue
        // returns to a cheap hourly check.
        if (plan.sweepBotMaterializations)
        {
            OrphanMaterializationSweepReport const sweep =
                sAuctionIntentExecutor.SweepOrphanMaterializations(
                    uint32(now), orphanSweepBatchSize);
            bool const retrySoon = sweep.morePending || !sweep.committed;
            s_nextOrphanSweepTime = now + (retrySoon ? MINUTE : HOUR);
        }
        else
        {
            s_nextOrphanSweepTime = now + HOUR;
        }
    }
}

void AhServiceTick()
{
    WorkerSupervisor* const ahSupervisor = sAhService.GetSupervisor();
    if (ahSupervisor == NULL)
    {
        return;
    }

    // Tick() uses wall-clock deltas internally and drives heartbeat, restart
    // and protocol; it must run every tick regardless of service health (it is
    // what transitions the service from inactive back to active).
    ahSupervisor->Tick(sWorld.GetGameTime());

    // Overflow visibility: warn (rate-limited) when the inbound queue has
    // dropped frames since we last checked.
    static size_t s_lastDroppedSeen = 0;
    const size_t  dropped = ahSupervisor->InboundDropped();
    if (dropped > s_lastDroppedSeen)
    {
        static time_t s_lastOverflowWarn = 0;
        const time_t now = time(NULL);
        // Baseline only advances on emission so suppressed bursts are counted
        // correctly in the next warning.
        if (now - s_lastOverflowWarn >= 60)
        {
            sLog.outError("[AHSupervisor] inbound queue overflow:"
                          " %u frame(s) dropped (total %u)",
                          static_cast<unsigned>(dropped - s_lastDroppedSeen),
                          static_cast<unsigned>(dropped));
            s_lastOverflowWarn = now;
            s_lastDroppedSeen  = dropped;
        }
    }

    // The apply loop and the near-full back-pressure check run only while the
    // service is healthy: while it is not, the in-process bot trades, and
    // applying the dead child's last staged batch at the same time would
    // over-post against it.
    if (ahSupervisor->ServiceActive())
    {
        std::vector<IpcMessage> msgs;
        ahSupervisor->DrainInbound(msgs, 256);

        // Near-full back-pressure: at 80% capacity warn AND tell the child to
        // throttle via IPC_QUEUE_FULL so it pauses one bot cycle. Both are
        // rate-limited to once per 60s.
        const size_t qSize = ahSupervisor->Channel().InboundSize();
        if (qSize >= IPC_INBOUND_QUEUE_CAP * 4 / 5)
        {
            static time_t s_lastNearFullWarn = 0;
            const time_t now = time(NULL);
            if (now - s_lastNearFullWarn >= 60)
            {
                sLog.outError("[AHSupervisor] inbound queue near full:"
                              " %u / %u frames - sending IPC_QUEUE_FULL",
                              static_cast<unsigned>(qSize),
                              static_cast<unsigned>(IPC_INBOUND_QUEUE_CAP));

                IpcMessage qf;
                qf.op = IPC_QUEUE_FULL;
                ahSupervisor->Channel().SendFrame(qf);

                s_lastNearFullWarn = now;
            }
        }

        for (size_t i = 0; i < msgs.size(); ++i)
        {
            AhHandleInbound(msgs[i]);
        }

        // Retry failed value-finalizes and age un-answered player mutations
        // into in-doubt tombstones (forward-only; never rolls back).
        uint32 const mutationNowSec = uint32(time(NULL));
        AhProcessRedriveQueue(mutationNowSec);
        AhProcessReconnectRetryQueue(mutationNowSec);
    }

    // Expire processed-uuid dedup entries regardless of service health, once
    // per second of game time.
    static time_t s_lastIntentPurge = 0;
    const time_t nowSec = sWorld.GetGameTime();
    if (nowSec != s_lastIntentPurge)
    {
        sAuctionIntentExecutor.PurgeExpiredUuids(uint32(nowSec));
        s_lastIntentPurge = nowSec;

        // Sweep timed-out browse requests on the same clock used to register
        // them; tell each player the AH is unavailable iff it is still the
        // current search and the player is present.
        std::vector<PendingBrowse> timedOut;
        sAhService.GetBrowsePending().Sweep(uint32(nowSec), 10u, timedOut);
        for (size_t i = 0; i < timedOut.size(); ++i)
        {
            if (!sAhService.GetBrowsePending().IsCurrent(timedOut[i].playerGuidLow,
                                                         timedOut[i].kind, timedOut[i].seq))
            {
                continue;
            }
            Player* p = sPlayerRegistry.Find(
                ObjectGuid(HIGHGUID_PLAYER, timedOut[i].playerGuidLow));
            if (p && p->IsInWorld() && p->GetSession())
            {
                AhSendBrowseUnavailable(p->GetSession(), timedOut[i].kind);
            }
        }
    }
}
