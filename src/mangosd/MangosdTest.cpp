#include "Utilities/Errors.h"
#include <vector>
#include "MangosdTest.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "Chat.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "AuctionHouseBot/AuctionIntentExecutor.h"
#include "AuctionHouseBot/CustodyDeferred.h"
#include "AuctionHouseBot/CustodyLedger.h"
#include "AuctionHouseBot/CustodyReconciler.h"
#include "AuctionHouseBot/CustodyService.h"
#include "AuctionIntents.h"
#include "WorkerSupervisor.h"
#include "Object/AhUsabilityRef.h"
#include "AuctionHouseBot/BrowsePending.h"
#include "AuctionHouseBot/MutationPending.h"
#include "IpcOpcodes.h"
#include "Item.h"
#include "BrowseMessages.h"
#include "World.h"
#include "WorldSession.h"
#include "PlayerMutations.h"
#include <cstdio>
#include <ctime>
#include <memory>
#include <string>

bool AhBuildCancelPrepareForward(MutationPendingMap& pending, uint32 playerGuidLow,
                                 uint32 auctionId, uint64 uuid, uint32 sentSec,
                                 IpcMessage& out);
bool AhRepairFindingMutationAllowed(CustodyFinding const& finding);

struct TestCliCapture
{
    std::vector<std::string> chunks;
};

static void TestCliPrint(void* arg, char const* text)
{
    if (arg && text)
    {
        static_cast<TestCliCapture*>(arg)->chunks.push_back(text);
    }
}

static uint32 CountCliChunks(TestCliCapture const& capture,
                             std::string const& needle)
{
    uint32 count = 0;
    for (size_t i = 0; i < capture.chunks.size(); ++i)
    {
        if (capture.chunks[i].find(needle) != std::string::npos)
        {
            ++count;
        }
    }
    return count;
}

static std::string TestHexEncode(ByteBuffer const& bb)
{
    static char const hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(bb.size() * 2u);
    uint8 const* data = bb.contents();
    for (size_t i = 0; i < bb.size(); ++i)
    {
        out.push_back(hex[(data[i] >> 4) & 0x0Fu]);
        out.push_back(hex[data[i] & 0x0Fu]);
    }
    return out;
}

static CustodyRow TestCustodyRow(uint32 id, std::string const& key,
                                 uint8 kind, uint8 role, uint32 ownerGuid,
                                 uint32 amount, uint32 itemGuid,
                                 uint32 auctionId, uint64 createdTime = 0)
{
    CustodyRow row = {};
    row.id = id;
    row.idemKey = key;
    row.kind = kind;
    row.role = role;
    row.state = CST_RESERVED;
    row.ownerGuid = ownerGuid;
    row.amount = amount;
    row.itemGuid = itemGuid;
    row.auctionId = auctionId;
    row.createdTime = createdTime;
    return row;
}

static CustodySnapshotGroup TestCustodyGroup(
    uint32 auctionId, uint32 itemGuid, uint32 ownerGuid,
    uint32 bidderGuid, uint32 bid, uint32 deposit,
    std::vector<CustodyRow> const& rows)
{
    CustodySnapshotGroup group = {};
    group.auctionId = auctionId;
    group.auction.exists = true;
    group.auction.auctionId = auctionId;
    group.auction.itemGuid = itemGuid;
    group.auction.ownerGuid = ownerGuid;
    group.auction.bidderGuid = bidderGuid;
    group.auction.bid = bid;
    group.auction.deposit = deposit;
    group.rows = rows;
    return group;
}

static uint32 CountCustodyFindings(CustodyReconcileReport const& report,
                                   CustodyFindingReason reason,
                                   CustodyRepairOwnership ownership,
                                   CustodyFindingState state)
{
    uint32 count = 0;
    for (size_t i = 0; i < report.findings.size(); ++i)
    {
        CustodyFinding const& finding = report.findings[i];
        if (finding.reason == reason &&
            finding.repairOwnership == ownership &&
            finding.state == state)
        {
            ++count;
        }
    }
    return count;
}

static bool RunPureCustodyReconcilerTests()
{
    bool pass = true;
    uint32 const botOwner = AHBOT_SYSTEM_OWNER_GUID;

    auto marker = [botOwner](uint32 auctionId, uint32 itemGuid,
                             uint64 createdTime = 0) -> CustodyRow
    {
        return TestCustodyRow(auctionId, "botlist:test:" + std::to_string(auctionId),
            CUSTODY_ITEM, ROLE_RESOLUTION, botOwner, 0, itemGuid,
            auctionId, createdTime);
    };
    auto bidRow = [](uint32 id, uint32 auctionId, uint32 bidder,
                     uint32 amount, uint64 createdTime = 0) -> CustodyRow
    {
        return TestCustodyRow(id, "bid:" + std::to_string(auctionId) +
            ":" + std::to_string(id), CUSTODY_GOLD, ROLE_BID,
            bidder, amount, 0, auctionId, createdTime);
    };

    // Marker-owned listings do not invent player seller custody.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodySnapshotGroup> groups;
        groups.push_back(TestCustodyGroup(980001, 880001, botOwner, 0, 0, 0,
            std::vector<CustodyRow>(1, marker(980001, 880001))));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty() || report.rowVisits != 1)
        {
            printf("custody FAIL: valid DB-only bot marker produced drift\n");
            pass = false;
        }
    }

    // Marker plus matching player bid stays clean.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodyRow> rows;
        rows.push_back(marker(980002, 880002));
        rows.push_back(bidRow(2, 980002, 2202, 202));
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980002, 880002, botOwner, 2202, 202, 0, rows));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: matching bot-listing bid produced drift\n");
            pass = false;
        }
    }

    // A fresh row defers the whole group before and after auction facts change.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodyRow> rows;
        rows.push_back(marker(980003, 880003, 900));
        rows.push_back(bidRow(3, 980003, 2203, 203, 950));
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980003, 880003, botOwner, 0, 0, 0, rows));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty() || report.pendingBidCount != 0)
        {
            printf("custody FAIL: fresh bid was not deferred before auction update\n");
            pass = false;
        }
        groups[0].auction.bidderGuid = 2203;
        groups[0].auction.bid = 203;
        reconciler.Scan(groups, 1005, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty() || report.pendingBidCount != 0)
        {
            printf("custody FAIL: fresh bid was not deferred after auction update\n");
            pass = false;
        }
    }

    // Complete and partial player seller custody.
    {
        std::vector<CustodyRow> rows;
        rows.push_back(TestCustodyRow(1, "item:980004", CUSTODY_ITEM,
            ROLE_ITEM, 1204, 0, 880004, 980004));
        rows.push_back(TestCustodyRow(2, "dep:980004", CUSTODY_GOLD,
            ROLE_DEPOSIT, 1204, 44, 0, 980004));
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980004, 880004, 1204, 0, 0, 44, rows));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: complete player seller custody produced drift\n");
            pass = false;
        }

        groups[0].rows.pop_back();
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (CountCustodyFindings(report, CUSTODY_FINDING_MISSING,
                CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED) != 1)
        {
            printf("custody FAIL: partial seller custody did not report one missing row\n");
            pass = false;
        }
    }

    // Resolution-only legacy provenance and a valid bid-only overlay stay clean.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodySnapshotGroup> groups;
        groups.push_back(TestCustodyGroup(980005, 880005, 1205, 0, 0, 55,
            std::vector<CustodyRow>(1, TestCustodyRow(1, "resolve:test:980005",
                CUSTODY_GOLD, ROLE_RESOLUTION, 0, 0, 0, 980005))));
        groups.push_back(TestCustodyGroup(980006, 880006, 1206, 2206, 206, 66,
            std::vector<CustodyRow>(1, bidRow(1, 980006, 2206, 206))));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: valid legacy provenance or bid overlay produced drift\n");
            pass = false;
        }
    }

    // A future canonical bot item remains marker-owned.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodyRow> rows;
        rows.push_back(marker(980007, 880007));
        rows.push_back(TestCustodyRow(2, "item:980007", CUSTODY_ITEM,
            ROLE_ITEM, botOwner, 0, 880007, 980007));
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980007, 880007, botOwner, 0, 0, 0, rows));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: canonical bot item overrode marker provenance\n");
            pass = false;
        }
    }

    // Invalid and duplicate markers are confirmed manual-only findings.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        CustodyRow invalid = marker(980008, 880008);
        invalid.role = ROLE_ITEM;
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980008, 880008, botOwner, 0, 0, 0,
                std::vector<CustodyRow>(1, invalid)));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (CountCustodyFindings(report, CUSTODY_FINDING_INVALID_MARKER,
                CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_CONFIRMED) != 1)
        {
            printf("custody FAIL: invalid marker ownership/reason mismatch\n");
            pass = false;
        }

        std::vector<CustodyRow> duplicateRows;
        duplicateRows.push_back(marker(980009, 880009));
        CustodyRow second = marker(980009, 880009);
        second.id += 1;
        second.idemKey += ":duplicate";
        duplicateRows.push_back(second);
        groups[0] = TestCustodyGroup(980009, 880009, botOwner, 0, 0, 0,
            duplicateRows);
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (CountCustodyFindings(report, CUSTODY_FINDING_DUPLICATE_MARKER,
                CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_CONFIRMED) != 1)
        {
            printf("custody FAIL: duplicate markers did not produce one finding\n");
            pass = false;
        }
    }

    // Orphan marker cleanup belongs to the bot sweep; player rows remain generic.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        CustodySnapshotGroup orphan = {};
        orphan.auctionId = 980010;
        orphan.rows.push_back(marker(980010, 880010));
        orphan.rows.push_back(bidRow(2, 980010, 2210, 210));
        std::vector<CustodySnapshotGroup> groups(1, orphan);
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (report.sweepOwnedCount != 1 || report.confirmedDriftCount != 1 ||
            CountCustodyFindings(report, CUSTODY_FINDING_SWEEP_OWNED_MARKER,
                CUSTODY_REPAIR_BOT_SWEEP, CUSTODY_FINDING_CONFIRMED) != 1 ||
            CountCustodyFindings(report, CUSTODY_FINDING_ORPHAN_PLAYER,
                CUSTODY_REPAIR_GENERIC, CUSTODY_FINDING_CONFIRMED) != 1)
        {
            printf("custody FAIL: orphan marker/player ownership split is wrong\n");
            pass = false;
        }
    }

    // Missing, duplicate, mismatched, and unexpected bids begin pending.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodySnapshotGroup> groups;
        groups.push_back(TestCustodyGroup(980011, 880011, botOwner, 2211, 211, 0,
            std::vector<CustodyRow>(1, marker(980011, 880011))));
        std::vector<CustodyRow> duplicate;
        duplicate.push_back(bidRow(1, 980012, 2212, 212));
        duplicate.push_back(bidRow(2, 980012, 2212, 212));
        groups.push_back(TestCustodyGroup(980012, 880012, 1212, 2212, 212, 0,
            duplicate));
        std::vector<CustodyRow> mismatched;
        mismatched.push_back(marker(980013, 880013));
        mismatched.push_back(bidRow(2, 980013, 9999, 213));
        groups.push_back(TestCustodyGroup(980013, 880013, botOwner, 2213, 213, 0,
            mismatched));
        std::vector<CustodyRow> unexpected;
        unexpected.push_back(marker(980014, 880014));
        unexpected.push_back(bidRow(2, 980014, 2214, 214));
        groups.push_back(TestCustodyGroup(980014, 880014, botOwner, 0, 0, 0,
            unexpected));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 5 || report.confirmedDriftCount != 0 ||
            CountCustodyFindings(report, CUSTODY_FINDING_MISSING,
                CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_PENDING) != 1 ||
            CountCustodyFindings(report, CUSTODY_FINDING_DUPLICATE,
                CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_PENDING) != 2 ||
            CountCustodyFindings(report, CUSTODY_FINDING_MISMATCHED,
                CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_PENDING) != 1 ||
            CountCustodyFindings(report, CUSTODY_FINDING_UNEXPECTED,
                CUSTODY_REPAIR_MANUAL_ONLY, CUSTODY_FINDING_PENDING) != 1)
        {
            printf("custody FAIL: bid mismatch reason or pending counts are wrong\n");
            pass = false;
        }
    }

    // Stable bid mismatch confirmation uses exact 1000/1059/1060 boundaries.
    {
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980015, 880015, botOwner, 2215, 215, 0,
                std::vector<CustodyRow>(1, marker(980015, 880015))));
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: first bid mismatch was not pending\n");
            pass = false;
        }
        reconciler.Scan(groups, 1059, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: 59-second bid mismatch did not remain pending\n");
            pass = false;
        }
        reconciler.Scan(groups, 1060, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 0 || report.confirmedDriftCount != 1)
        {
            printf("custody FAIL: 60-second bid mismatch was not confirmed\n");
            pass = false;
        }

        groups[0].rows.push_back(bidRow(2, 980015, 2215, 215));
        reconciler.Scan(groups, 1061, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: matching bid did not clear pending cache\n");
            pass = false;
        }
    }

    // Changed, fresh, removed, and boot observations cannot confirm old state.
    {
        CustodyReconcileReport report;
        std::vector<CustodySnapshotGroup> groups(1,
            TestCustodyGroup(980016, 880016, botOwner, 2216, 216, 0,
                std::vector<CustodyRow>(1, marker(980016, 880016))));

        CustodyReconciler changed;
        changed.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        groups[0].auction.bid = 217;
        changed.Scan(groups, 1060, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: changed fingerprint confirmed stale mismatch\n");
            pass = false;
        }

        std::vector<CustodyRow> wrongShapeRows;
        wrongShapeRows.push_back(marker(980017, 880017));
        wrongShapeRows.push_back(bidRow(2, 980017, 2217, 217));
        wrongShapeRows[1].itemGuid = 1;
        std::vector<CustodySnapshotGroup> wrongShapeGroups(1,
            TestCustodyGroup(980017, 880017, botOwner, 2217, 217, 0,
                wrongShapeRows));
        CustodyReconciler rowChanged;
        rowChanged.Scan(wrongShapeGroups, 1000, CUSTODY_SCAN_RUNTIME, report);
        wrongShapeGroups[0].rows[1].itemGuid = 2;
        rowChanged.Scan(wrongShapeGroups, 1060, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: changed bid-row shape inherited mismatch age\n");
            pass = false;
        }

        CustodyReconciler fresh;
        fresh.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        groups[0].rows.push_back(bidRow(2, 980016, 9999, 217, 1100));
        fresh.Scan(groups, 1060, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: fresh group did not clear mismatch observation\n");
            pass = false;
        }
        groups[0].rows.pop_back();
        fresh.Scan(groups, 1200, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: fresh-group clear retained old mismatch age\n");
            pass = false;
        }

        CustodyReconciler removed;
        removed.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        removed.Scan(std::vector<CustodySnapshotGroup>(), 1060,
            CUSTODY_SCAN_RUNTIME, report);
        removed.Scan(groups, 1120, CUSTODY_SCAN_RUNTIME, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: removed group retained old mismatch age\n");
            pass = false;
        }

        CustodyReconciler boot;
        boot.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        boot.Scan(groups, 1060, CUSTODY_SCAN_BOOT, report);
        if (report.pendingBidCount != 1 || report.confirmedDriftCount != 0)
        {
            printf("custody FAIL: boot scan confirmed prior mismatch\n");
            pass = false;
        }

        CustodyReconciler future;
        groups[0].rows.push_back(bidRow(3, 980016, 9999, 217, 2000));
        future.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (!report.findings.empty())
        {
            printf("custody FAIL: future timestamp was treated as mature\n");
            pass = false;
        }
    }

    // A large clean population visits each input row exactly once.
    {
        std::vector<CustodySnapshotGroup> groups;
        groups.reserve(50000);
        for (uint32 i = 0; i < 50000; ++i)
        {
            uint32 const auctionId = 1000000 + i;
            groups.push_back(TestCustodyGroup(auctionId, 2000000 + i,
                3000000 + i, 0, 0, 0,
                std::vector<CustodyRow>(1, TestCustodyRow(i + 1,
                    "resolve:linear:" + std::to_string(i), CUSTODY_GOLD,
                    ROLE_RESOLUTION, 0, 0, 0, auctionId))));
        }
        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        if (report.rowVisits != 50000 || !report.findings.empty())
        {
            printf("custody FAIL: linear scan visits=" UI64FMTD " findings=%u\n",
                report.rowVisits, uint32(report.findings.size()));
            pass = false;
        }
    }

    // Automatic custody maintenance gates reconciliation/pruning and the bot
    // materialization sweep independently.
    {
        CustodyMaintenancePlan const disabled =
            CustodyService::GetMaintenancePlan(false, false);
        CustodyMaintenancePlan const custodyOnly =
            CustodyService::GetMaintenancePlan(true, false);
        CustodyMaintenancePlan const writeOnly =
            CustodyService::GetMaintenancePlan(false, true);
        CustodyMaintenancePlan const enabled =
            CustodyService::GetMaintenancePlan(true, true);

        if (disabled.reconcile || disabled.prune ||
            disabled.sweepBotMaterializations ||
            !custodyOnly.reconcile || !custodyOnly.prune ||
            custodyOnly.sweepBotMaterializations ||
            writeOnly.reconcile || writeOnly.prune ||
            !writeOnly.sweepBotMaterializations ||
            !enabled.reconcile || !enabled.prune ||
            !enabled.sweepBotMaterializations)
        {
            printf("custody FAIL: maintenance gate truth table mismatch\n");
            pass = false;
        }
    }

    // Each automatic/manual operation owns one independent detail budget.
    {
        CustodyDetailBudget budget(100);
        uint32 allowed = 0;
        for (uint32 i = 0; i < 101; ++i)
        {
            if (budget.Take())
            {
                ++allowed;
            }
        }

        CustodyDetailBudget second(100);
        if (allowed != 100 || budget.Allowed() != 100 ||
            budget.Suppressed() != 1 || !second.Take() ||
            second.Allowed() != 1 || second.Suppressed() != 0)
        {
            printf("custody FAIL: detail budget did not cap at 100\n");
            pass = false;
        }
    }

    // Shuffled input yields deterministic sorted detail output without
    // changing exact category totals when the first 100 details are selected.
    {
        std::vector<CustodySnapshotGroup> groups;
        for (uint32 i = 0; i < 101; ++i)
        {
            uint32 const auctionId = 990100 - i;
            CustodySnapshotGroup group = {};
            group.auctionId = auctionId;
            group.rows.push_back(TestCustodyRow(i + 1,
                "test:budget:" + std::to_string(auctionId), CUSTODY_GOLD,
                ROLE_BID, 4000000 + i, 100 + i, 0, auctionId));
            groups.push_back(group);
        }

        CustodyReconciler reconciler;
        CustodyReconcileReport report;
        reconciler.Scan(groups, 1000, CUSTODY_SCAN_RUNTIME, report);
        CustodyDetailBudget budget(100);
        uint32 firstAuctionId = 0;
        uint32 lastAuctionId = 0;
        for (size_t i = 0; i < report.findings.size(); ++i)
        {
            if (!budget.Take())
            {
                continue;
            }
            if (!firstAuctionId)
            {
                firstAuctionId = report.findings[i].row.auctionId;
            }
            lastAuctionId = report.findings[i].row.auctionId;
        }

        if (report.findings.size() != 101 ||
            report.confirmedDriftCount != 101 ||
            report.pendingBidCount != 0 || report.sweepOwnedCount != 0 ||
            firstAuctionId != 990000 || lastAuctionId != 990099 ||
            budget.Allowed() != 100 || budget.Suppressed() != 1)
        {
            printf("custody FAIL: bounded report order/totals mismatch\n");
            pass = false;
        }
    }

    return pass;
}

/// Self-test for Database::CommitTransactionChecked(): proves the runtime
/// (async-enabled) path is synchronous, durable and returns the REAL result.
/// Returns 0 on pass, non-zero on fail.
static int RunCommitTest()
{
    bool pass = true;

    // Force the runtime async path so the transaction is FIFO-queued through
    // the delay thread and CommitTransactionChecked() blocks until durable.
    CharacterDatabase.AllowAsyncTransactions();

    // Clean slate (ignore result; row may or may not exist from a prior run).
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:commit%'");

    // (a) Success path: a valid INSERT must commit and be visible BEFORE return.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:commit:ok',1,1,0,0,0,0,0,0,0,0)");
    bool ok = CharacterDatabase.CommitTransactionChecked();
    if (!ok)
    {
        printf("commit FAIL: success-path CommitTransactionChecked returned false\n");
        pass = false;
    }

    // Durability: a synchronous SELECT must see the row immediately after the
    // (blocking) checked commit returned - i.e. it is durable, not deferred.
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT 1 FROM `custody_ledger` WHERE `idem_key`='test:commit:ok'"));
        if (!res)
        {
            printf("commit FAIL: committed row not visible after return (not durable)\n");
            pass = false;
        }
    }

    // (b) Rollback path: a second row with the SAME idem_key violates uk_idem,
    // so the transaction must fail and CommitTransactionChecked() must return
    // false. The duplicate must NOT land.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:commit:ok',2,2,0,0,0,0,0,0,0,0)");
    bool ok2 = CharacterDatabase.CommitTransactionChecked();
    if (ok2)
    {
        printf("commit FAIL: rollback-path CommitTransactionChecked returned true\n");
        pass = false;
    }

    // The duplicate must not have landed: still exactly one row for the key.
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `custody_ledger` "
            "WHERE `idem_key`='test:commit:ok'"));
        if (!res || res->Fetch()[0].GetUInt64() != 1)
        {
            printf("commit FAIL: duplicate row landed after rollback\n");
            pass = false;
        }
    }

    // (c) Post-rollback TSS cleanliness: the false-returning rollback path above
    // must still have detached the transaction from the TSS slot. If it left a
    // residue, this third BeginTransaction() would trip MANGOS_ASSERT(!m_pTrans);
    // in TransHelper::init(); a clean detach lets a fresh checked commit succeed
    // and land its row.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:commit:after',1,1,0,0,0,0,0,0,0,0)");
    bool ok3 = CharacterDatabase.CommitTransactionChecked();
    if (!ok3)
    {
        printf("commit FAIL: post-rollback CommitTransactionChecked returned false (TSS not clean?)\n");
        pass = false;
    }
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT 1 FROM `custody_ledger` WHERE `idem_key`='test:commit:after'"));
        if (!res)
        {
            printf("commit FAIL: post-rollback committed row not visible (not durable)\n");
            pass = false;
        }
    }

    // Clean up (covers test:commit:ok and test:commit:after via the prefix).
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:commit%'");

    if (pass)
    {
        printf("commit OK\n");
        return 0;
    }

    return 2;
}

/// Self-test for MailDraft::SendMailToInTransaction + CustodyDeferred: proves
/// the co-commit variant appends the `mail` row to the CALLER's open
/// transaction (no own Begin/Commit) and that running the deferred queue after
/// a checked commit is safe. Uses existing offline character guid 1 (which has
/// an account, so the offline path writes a row). Returns 0 on pass.
static int RunMailTest()
{
    bool pass = true;

    CharacterDatabase.AllowAsyncTransactions();

    // Seed mail ids from MAX(mail.id)+1 so GenerateMailID() does not collide
    // with existing rows (world data is NOT loaded under -t).
    sObjectMgr.SetHighestGuids();

    // The offline mail path requires the recipient to resolve to an account.
    // Normal test clones may not have a character with guid 1, so seed one only
    // when absent and clean up only our own fixture row afterwards.
    bool seededReceiver = false;
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT 1 FROM `characters` WHERE `guid`=1"));
        if (!res)
        {
            CharacterDatabase.DirectExecute(
                "INSERT INTO `characters` (`guid`,`account`,`name`,`money`) "
                "VALUES (1, 1, 'AhMailTest1', 0)");
            seededReceiver = true;
        }
    }

    // Clean slate from any prior run.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `money`=123 "
        "AND `subject`='custodytest'");

    // Append the mail to OUR open transaction, then checked-commit, then run
    // the deferred queue (offline receiver => effects empty, items deferred).
    CharacterDatabase.BeginTransaction();
    MailDraft d("custodytest", "b");
    d.SetMoney(123);
    CustodyDeferred def;
    d.SendMailToInTransaction(MailReceiver(ObjectGuid(HIGHGUID_PLAYER, uint32(1))),
                              MailSender(MAIL_AUCTION, uint32(0)), def);
    bool ok = CharacterDatabase.CommitTransactionChecked();
    if (!ok)
    {
        printf("mail FAIL: CommitTransactionChecked returned false\n");
        pass = false;
    }
    else
    {
        def.run();
    }

    // The mail row must have co-committed in our transaction.
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`=1 AND `money`=123 "
            "AND `subject`='custodytest'"));
        if (!res || res->Fetch()[0].GetUInt64() != 1)
        {
            printf("mail FAIL: co-committed mail row not found (expected 1)\n");
            pass = false;
        }
    }

    // Clean up.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `money`=123 "
        "AND `subject`='custodytest'");
    if (seededReceiver)
    {
        CharacterDatabase.DirectExecute(
            "DELETE FROM `characters` WHERE `guid`=1 AND `name`='AhMailTest1'");
    }

    if (pass)
    {
        printf("mail OK\n");
        return 0;
    }

    return 2;
}

/// CRUD round-trip test for CustodyLedger: Insert, Get, GetRouteState, SetState,
/// LoadNonTerminal, DeleteTerminalOlderThan.  Returns 0 on pass.
static int RunCustodyTest()
{
    bool pass = true;

    if (CustodyService::ShouldCrashAtPhase("", "") ||
        CustodyService::ShouldCrashAtPhase("pre-commit", "") ||
        CustodyService::ShouldCrashAtPhase("", "pre-commit") ||
        CustodyService::ShouldCrashAtPhase("pre-commit", "pre-deferred") ||
        !CustodyService::ShouldCrashAtPhase("pre-commit", "pre-commit"))
    {
        printf("custody FAIL: crash phase predicate mismatch\n");
        pass = false;
    }

    CharacterDatabase.AllowAsyncTransactions();

    // Clean slate from any prior aborted run.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:crud%'");

    // ------------------------------------------------------------------ step 1
    // Insert a RESERVED gold row and commit.
    {
        CustodyRow r;
        r.id              = 0;
        r.idemKey         = "test:crud:1";
        r.kind            = CUSTODY_GOLD;
        r.role            = ROLE_BID;
        r.state           = CST_RESERVED;
        r.ownerGuid       = 1;
        r.beneficiaryGuid = 0;
        r.amount          = 50;
        r.itemGuid        = 0;
        r.auctionId       = 999;
        r.createdTime     = static_cast<uint64>(time(NULL));
        r.resolvedTime    = 0;

        CharacterDatabase.BeginTransaction();
        CustodyLedger::Insert(r);
        bool ok = CharacterDatabase.CommitTransactionChecked();
        if (!ok)
        {
            printf("custody FAIL: step 1 Insert CommitTransactionChecked returned false\n");
            pass = false;
        }
    }

    // ------------------------------------------------------------------ step 2
    // Get: row visible, state==CST_RESERVED, amount==50.
    {
        CustodyRow row;
        bool found = CustodyLedger::Get("test:crud:1", row);
        if (!found)
        {
            printf("custody FAIL: step 2 Get returned false (row not found)\n");
            pass = false;
        }
        else
        {
            if (row.state != CST_RESERVED)
            {
                printf("custody FAIL: step 2 state expected %u got %u\n",
                    uint32(CST_RESERVED), uint32(row.state));
                pass = false;
            }
            if (row.amount != 50)
            {
                printf("custody FAIL: step 2 amount expected 50 got %u\n", row.amount);
                pass = false;
            }
        }
    }

    // The seeded bid row selects only bid custody; an absent auction selects none.
    CustodyRouteState const seededRoute = CustodyLedger::GetRouteState(999);
    if (seededRoute.usesPlayerSellerCustody || !seededRoute.hasLiveBidCustody)
    {
        printf("custody FAIL: step 2 GetRouteState(999) mismatch\n");
        pass = false;
    }
    CustodyRouteState const absentRoute = CustodyLedger::GetRouteState(424242);
    if (absentRoute.usesPlayerSellerCustody || absentRoute.hasLiveBidCustody)
    {
        printf("custody FAIL: step 2 GetRouteState(424242) unexpectedly routed\n");
        pass = false;
    }

    // ------------------------------------------------------------------ step 3
    // SetState to CST_TERMINAL_OK and verify.
    {
        uint64 resolvedNow = static_cast<uint64>(time(NULL));
        CharacterDatabase.BeginTransaction();
        CustodyLedger::SetState("test:crud:1", CST_TERMINAL_OK, resolvedNow);
        bool ok = CharacterDatabase.CommitTransactionChecked();
        if (!ok)
        {
            printf("custody FAIL: step 3 SetState CommitTransactionChecked returned false\n");
            pass = false;
        }

        CustodyRow row;
        bool found = CustodyLedger::Get("test:crud:1", row);
        if (!found)
        {
            printf("custody FAIL: step 3 Get returned false after SetState\n");
            pass = false;
        }
        else if (row.state != CST_TERMINAL_OK)
        {
            printf("custody FAIL: step 3 state expected %u got %u\n",
                uint32(CST_TERMINAL_OK), uint32(row.state));
            pass = false;
        }
    }

    // ------------------------------------------------------------------ step 4
    // LoadNonTerminal must NOT contain "test:crud:1" (it is now terminal).
    {
        std::vector<CustodyRow> v;
        CustodyLedger::LoadNonTerminal(v);
        for (size_t i = 0; i < v.size(); ++i)
        {
            if (v[i].idemKey == "test:crud:1")
            {
                printf("custody FAIL: step 4 LoadNonTerminal contains terminal row\n");
                pass = false;
                break;
            }
        }
    }

    // ------------------------------------------------------------------ step 5
    // DeleteTerminalOlderThan(now+10): row must be pruned.
    {
        uint64 cutoff = static_cast<uint64>(time(NULL)) + 10;
        CharacterDatabase.BeginTransaction();
        CustodyLedger::DeleteTerminalOlderThan(cutoff);
        bool ok = CharacterDatabase.CommitTransactionChecked();
        if (!ok)
        {
            printf("custody FAIL: step 5 DeleteTerminalOlderThan CommitTransactionChecked returned false\n");
            pass = false;
        }

        CustodyRow row;
        bool found = CustodyLedger::Get("test:crud:1", row);
        if (found)
        {
            printf("custody FAIL: step 5 Get returned true after prune (row not deleted)\n");
            pass = false;
        }
    }

    // ------------------------------------------------------------------ step 6
    // Cleanup CRUD rows.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:crud%%'");
    CharacterDatabase.CommitTransactionChecked();

    // =================================================== authoritative reads
    // These rows exercise route provenance independently from local AH maps.
    uint32 const routeBase = 973100;
    uint32 const snapshotAuctionId = routeBase + 7;
    uint32 const orphanAuctionId = routeBase + 8;
    uint64 const routeNow = static_cast<uint64>(time(NULL));

    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` BETWEEN %u AND %u",
        routeBase + 1, orphanAuctionId);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `auction` WHERE `id`=%u", snapshotAuctionId);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) VALUES "
        "('item:973101',1,3,0,1101,0,0,883101,973101," UI64FMTD ",0),"
        "('dep:973101',0,0,0,1101,0,31,0,973101," UI64FMTD ",0),"
        "('bid:973102:1',0,1,0,2102,0,102,0,973102," UI64FMTD ",0),"
        "('resolve:test:973103',0,4,0,0,0,0,0,973103," UI64FMTD ",0),"
        "('botlist:test:973104',1,3,0,4294967294,0,0,883104,973104," UI64FMTD ",0),"
        "('botlist:test:973105',1,3,0,4294967294,0,0,883105,973105," UI64FMTD ",0),"
        "('bid:973105:1',0,1,0,2105,0,105,0,973105," UI64FMTD ",0),"
        "('botlist:test:973106',1,3,0,4294967294,0,0,883106,973106," UI64FMTD ",0),"
        "('item:973106',1,3,0,4294967294,0,0,883106,973106," UI64FMTD ",0),"
        "('test:snapshot:live',0,4,0,0,0,0,0,973107," UI64FMTD ",0),"
        "('test:snapshot:orphan',0,1,0,2208,0,208,0,973108," UI64FMTD ",0)",
        routeNow, routeNow, routeNow, routeNow, routeNow, routeNow,
        routeNow, routeNow, routeNow, routeNow, routeNow);
    CharacterDatabase.PExecute(
        "INSERT INTO `auction` "
        "(`id`,`houseid`,`itemguid`,`item_template`,`item_count`,"
        "`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,"
        "`lastbid`,`startbid`,`deposit`) "
        "VALUES (%u,7,883107,25,2,0,1234,500," UI64FMTD ",2345,3456,100,77)",
        snapshotAuctionId, routeNow + HOUR);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        printf("custody FAIL: authoritative-read seed commit returned false\n");
        pass = false;
    }

    struct RouteExpectation
    {
        uint32 auctionId;
        bool seller;
        bool bid;
        char const* label;
    };
    RouteExpectation const routeExpectations[] =
    {
        { routeBase + 1, true,  false, "player seller" },
        { routeBase + 2, false, true,  "bid only" },
        { routeBase + 3, false, false, "resolution only" },
        { routeBase + 4, false, false, "bot marker only" },
        { routeBase + 5, false, true,  "bot marker plus bid" },
        { routeBase + 6, false, false, "bot marker plus canonical item" },
    };
    for (size_t i = 0; i < sizeof(routeExpectations) / sizeof(routeExpectations[0]); ++i)
    {
        RouteExpectation const& expected = routeExpectations[i];
        CustodyRouteState const route = CustodyLedger::GetRouteState(expected.auctionId);
        if (route.usesPlayerSellerCustody != expected.seller ||
            route.hasLiveBidCustody != expected.bid)
        {
            printf("custody FAIL: route %s expected seller=%u bid=%u got seller=%u bid=%u\n",
                expected.label, uint32(expected.seller), uint32(expected.bid),
                uint32(route.usesPlayerSellerCustody), uint32(route.hasLiveBidCustody));
            pass = false;
        }
    }

    AuctionHouseObject* authoritativeMap =
        sAuctionMgr.GetAuctionsMap(AUCTION_HOUSE_NEUTRAL);
    authoritativeMap->RemoveAuction(snapshotAuctionId);
    if (authoritativeMap->GetAuction(snapshotAuctionId))
    {
        printf("custody FAIL: snapshot fixture unexpectedly exists in local AH map\n");
        pass = false;
    }

    std::vector<CustodySnapshotGroup> snapshot;
    CustodyLedger::LoadReconcileSnapshot(snapshot);
    bool sawSnapshotAuction = false;
    bool sawSnapshotOrphan = false;
    for (size_t i = 0; i < snapshot.size(); ++i)
    {
        CustodySnapshotGroup const& group = snapshot[i];
        if (group.auctionId == snapshotAuctionId)
        {
            sawSnapshotAuction = true;
            if (!group.auction.exists || group.auction.auctionId != snapshotAuctionId ||
                group.auction.itemGuid != 883107 || group.auction.ownerGuid != 1234 ||
                group.auction.bidderGuid != 2345 || group.auction.bid != 3456 ||
                group.auction.deposit != 77 || group.rows.size() != 1)
            {
                printf("custody FAIL: joined live auction facts do not match DB fixture\n");
                pass = false;
            }
        }
        else if (group.auctionId == orphanAuctionId)
        {
            sawSnapshotOrphan = true;
            if (group.auction.exists || group.rows.size() != 1 ||
                group.rows[0].idemKey != "test:snapshot:orphan")
            {
                printf("custody FAIL: joined orphan facts do not match DB fixture\n");
                pass = false;
            }
        }
    }
    if (!sawSnapshotAuction || !sawSnapshotOrphan)
    {
        printf("custody FAIL: authoritative snapshot omitted live=%u orphan=%u\n",
            uint32(sawSnapshotAuction), uint32(sawSnapshotOrphan));
        pass = false;
    }

    if (!CustodyLedger::AuctionExists(snapshotAuctionId))
    {
        printf("custody FAIL: AuctionExists did not see shared auction row\n");
        pass = false;
    }
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `auction` WHERE `id`=%u", snapshotAuctionId);
    if (CustodyLedger::AuctionExists(snapshotAuctionId))
    {
        printf("custody FAIL: AuctionExists retained deleted shared auction row\n");
        pass = false;
    }

    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` BETWEEN %u AND %u",
        routeBase + 1, orphanAuctionId);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `auction` WHERE `id`=%u", snapshotAuctionId);

    if (!RunPureCustodyReconcilerTests())
    {
        pass = false;
    }

    // ================================================================ reconcile
    // Task 13: ReconcileScan flags custody drift and DeleteTerminalOlderThan
    // prunes only old terminal rows.
    uint32 const liveAuctionId = 970002;
    uint32 const missingItemAuctionId = 970003;
    uint32 const duplicateBidAuctionId = 970006;
    uint64 const now = static_cast<uint64>(time(NULL));
    uint64 const oldTime = now > 7200 ? now - 7200 : 1;

    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:recon:%'");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` IN "
        "('item:970002','dep:970002','item:970003','dep:970003',"
        "'item:970006','dep:970006','bid:970006:1','bid:970006:2')");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `auction` WHERE `id` IN (970002,970003,970006)");

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `auction` "
        "(`id`,`houseid`,`itemguid`,`item_template`,`item_count`,"
        "`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,"
        "`lastbid`,`startbid`,`deposit`) VALUES "
        "(970002,7,880002,25,1,0,1001,0," UI64FMTD ",0,0,10,5),"
        "(970003,7,880003,25,1,0,1002,0," UI64FMTD ",0,0,10,5),"
        "(970006,7,880006,25,1,0,1006,0," UI64FMTD ",2006,77,10,5)",
        now + HOUR, now + HOUR, now + HOUR);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:recon:orphan','%u','%u','%u','%u','0','0','0','970001','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_DEPOSIT), uint32(CST_RESERVED), 1000, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('item:970002','%u','%u','%u','%u','0','0','880002','970002','" UI64FMTD "','0')",
        uint32(CUSTODY_ITEM), uint32(ROLE_ITEM), uint32(CST_RESERVED), 1001, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('dep:970002','%u','%u','%u','%u','0','5','0','970002','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_DEPOSIT), uint32(CST_RESERVED), 1001, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('dep:970003','%u','%u','%u','%u','0','5','0','970003','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_DEPOSIT), uint32(CST_RESERVED), 1002, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('item:970006','%u','%u','%u','%u','0','0','880006','970006','" UI64FMTD "','0')",
        uint32(CUSTODY_ITEM), uint32(ROLE_ITEM), uint32(CST_RESERVED), 1006, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('dep:970006','%u','%u','%u','%u','0','5','0','970006','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_DEPOSIT), uint32(CST_RESERVED), 1006, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('bid:970006:1','%u','%u','%u','%u','0','77','0','970006','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_BID), uint32(CST_RESERVED), 2006, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('bid:970006:2','%u','%u','%u','%u','0','77','0','970006','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_BID), uint32(CST_RESERVED), 2006, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:recon:old-terminal','%u','%u','%u','%u','0','0','0','970004','" UI64FMTD "','" UI64FMTD "')",
        uint32(CUSTODY_GOLD), uint32(ROLE_DEPOSIT), uint32(CST_TERMINAL_OK), 1004, oldTime, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:recon:recent-terminal','%u','%u','%u','%u','0','0','0','970005','" UI64FMTD "','" UI64FMTD "')",
        uint32(CUSTODY_GOLD), uint32(ROLE_DEPOSIT), uint32(CST_TERMINAL_BACK), 1005, now, now);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        printf("custody FAIL: reconcile seed commit returned false\n");
        pass = false;
    }

    {
        CustodyReconcileReport report;
        CustodyService::ReconcileScan(now, CUSTODY_SCAN_RUNTIME, report);
        bool sawOrphan = false;
        bool sawCleanLive = false;
        bool sawMissingItem = false;
        uint32 pendingDuplicateBids = 0;
        for (size_t i = 0; i < report.findings.size(); ++i)
        {
            CustodyFinding const& finding = report.findings[i];
            if (finding.row.idemKey == "test:recon:orphan" &&
                finding.reason == CUSTODY_FINDING_ORPHAN_PLAYER &&
                finding.repairOwnership == CUSTODY_REPAIR_GENERIC)
            {
                sawOrphan = true;
            }
            if (finding.row.auctionId == liveAuctionId)
            {
                sawCleanLive = true;
            }
            if (finding.row.idemKey == "item:970003" &&
                finding.reason == CUSTODY_FINDING_MISSING)
            {
                sawMissingItem = true;
            }
            if (finding.row.auctionId == duplicateBidAuctionId &&
                finding.reason == CUSTODY_FINDING_DUPLICATE &&
                finding.state == CUSTODY_FINDING_PENDING)
            {
                ++pendingDuplicateBids;
            }
        }
        if (!sawOrphan)
        {
            printf("custody FAIL: reconcile did not flag orphan non-terminal row\n");
            pass = false;
        }
        if (sawCleanLive)
        {
            printf("custody FAIL: reconcile flagged complete live custody auction\n");
            pass = false;
        }
        if (!sawMissingItem)
        {
            printf("custody FAIL: reconcile did not flag live auction missing item row\n");
            pass = false;
        }
        if (pendingDuplicateBids != 2)
        {
            printf("custody FAIL: reconcile duplicate bid pending count expected 2 got %u\n",
                pendingDuplicateBids);
            pass = false;
        }
    }

    {
        CharacterDatabase.BeginTransaction();
        CustodyLedger::DeleteTerminalOlderThan(now - 3600);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("custody FAIL: reconcile prune commit returned false\n");
            pass = false;
        }

        CustodyRow row;
        if (CustodyLedger::Get("test:recon:old-terminal", row))
        {
            printf("custody FAIL: old terminal row was not pruned\n");
            pass = false;
        }
        if (!CustodyLedger::Get("test:recon:recent-terminal", row))
        {
            printf("custody FAIL: recent terminal row was pruned\n");
            pass = false;
        }
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM `auction` WHERE `id` IN (970002,970003,970006)");
    CharacterDatabase.PExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:recon:%%'");
    CharacterDatabase.PExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` IN "
        "('item:970002','dep:970002','item:970003','dep:970003',"
        "'item:970006','dep:970006','bid:970006:1','bid:970006:2')");
    CharacterDatabase.CommitTransactionChecked();

    // ========================================================== repair command
    // Task 14: ah repair terminalizes custody drift conservatively and never
    // mints mail from orphan ledger rows.
    uint32 const repairOwner = 1;
    uint32 const repairAuctionId = 970010;
    uint32 const repairItemGuid = 880010;
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `subject`='AH custody repair'");
    uint64 beforeRepairMail = 0;
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`='%u'",
            repairOwner));
        if (res)
        {
            beforeRepairMail = res->Fetch()[0].GetUInt64();
        }
    }

    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:repair:%'");
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:repair:gold','%u','%u','%u','%u','0','12345','0','%u','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_BID), uint32(CST_RESERVED),
        repairOwner, repairAuctionId, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:repair:item','%u','%u','%u','%u','0','0','%u','%u','" UI64FMTD "','0')",
        uint32(CUSTODY_ITEM), uint32(ROLE_ITEM), uint32(CST_RESERVED),
        repairOwner, repairItemGuid, repairAuctionId + 1, oldTime);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        printf("custody FAIL: repair seed commit returned false\n");
        pass = false;
    }

    CliHandler cli(0, SEC_ADMINISTRATOR, NULL, &TestCliPrint);
    if (!cli.ParseCommands("ah repair apply"))
    {
        printf("custody FAIL: ah repair apply returned false\n");
        pass = false;
    }

    {
        CustodyRow row;
        if (!CustodyLedger::Get("test:repair:gold", row) ||
            row.state == CST_RESERVED)
        {
            printf("custody FAIL: ah repair did not terminalize gold row\n");
            pass = false;
        }
        if (!CustodyLedger::Get("test:repair:item", row) ||
            row.state == CST_RESERVED)
        {
            printf("custody FAIL: ah repair did not terminalize item row\n");
            pass = false;
        }
    }

    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`='%u'",
            repairOwner));
        uint64 const afterRepairMail = res ? res->Fetch()[0].GetUInt64() : 0;
        if (afterRepairMail != beforeRepairMail)
        {
            printf("custody FAIL: ah repair minted mail from orphan ledger row\n");
            pass = false;
        }
    }

    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `item_instance` WHERE `guid`='%u'",
            repairItemGuid));
        if (res && res->Fetch()[0].GetUInt64() != 0)
        {
            printf("custody FAIL: ah repair mutated sentinel item_instance row\n");
            pass = false;
        }
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('test:repair:force','%u','%u','%u','%u','0','54321','0','%u','" UI64FMTD "','0')",
        uint32(CUSTODY_GOLD), uint32(ROLE_BID), uint32(CST_RESERVED),
        repairOwner, repairAuctionId + 2, oldTime);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        printf("custody FAIL: repair force seed commit returned false\n");
        pass = false;
    }

    if (!cli.ParseCommands("ah repair force-forfeit test:repair:force"))
    {
        printf("custody FAIL: ah repair force-forfeit returned false\n");
        pass = false;
    }

    {
        CustodyRow row;
        if (!CustodyLedger::Get("test:repair:force", row) ||
            row.state == CST_RESERVED)
        {
            printf("custody FAIL: force-forfeit did not terminalize gold row\n");
            pass = false;
        }
    }

    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`='%u'",
            repairOwner));
        uint64 const afterForceMail = res ? res->Fetch()[0].GetUInt64() : 0;
        if (afterForceMail != beforeRepairMail)
        {
            printf("custody FAIL: force-forfeit minted mail from orphan ledger row\n");
            pass = false;
        }
    }

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:repair:%%'");
    CharacterDatabase.CommitTransactionChecked();
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `subject`='AH custody repair'");

    // The apply guard must consult the shared auction table at mutation time,
    // not the local AH map or the auction facts captured by the scan.
    {
        uint32 const livenessAuctionId = 970080;
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `auction` WHERE `id`=%u", livenessAuctionId);
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key`='test:repair:liveness'");
        CharacterDatabase.DirectPExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:repair:liveness',0,1,0,1,0,80,0,%u," UI64FMTD ",0)",
            livenessAuctionId, oldTime);

        CustodyReconcileReport report;
        CustodyService::ReconcileScan(now, CUSTODY_SCAN_RUNTIME, report);
        CustodyFinding scanned = {};
        bool found = false;
        for (size_t i = 0; i < report.findings.size(); ++i)
        {
            if (report.findings[i].row.idemKey == "test:repair:liveness")
            {
                scanned = report.findings[i];
                found = true;
                break;
            }
        }

        CharacterDatabase.DirectPExecute(
            "INSERT INTO `auction` "
            "(`id`,`houseid`,`itemguid`,`item_template`,`item_count`,"
            "`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,"
            "`lastbid`,`startbid`,`deposit`) "
            "VALUES (%u,7,880080,25,1,0,1,0," UI64FMTD ",0,0,10,5)",
            livenessAuctionId, now + HOUR);
        bool const blockedAfterInsert = found &&
            !AhRepairFindingMutationAllowed(scanned);
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `auction` WHERE `id`=%u", livenessAuctionId);
        bool const allowedAfterDelete = found &&
            AhRepairFindingMutationAllowed(scanned);
        if (!found || !blockedAfterInsert || !allowedAfterDelete)
        {
            printf("custody FAIL: repair liveness guard ignored post-scan DB state\n");
            pass = false;
        }

        CharacterDatabase.DirectExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key`='test:repair:liveness'");
    }

    // Generic apply must report but skip pending, manual-only, and bot-sweep
    // findings. Force-forfeit must reject the reserved bot marker explicitly.
    {
        CharacterDatabase.DirectExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'botlist:test:repair:%'");
        CharacterDatabase.DirectExecute(
            "DELETE FROM `auction` WHERE `id` IN (970081,970082)");
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `auction` "
            "(`id`,`houseid`,`itemguid`,`item_template`,`item_count`,"
            "`item_randompropertyid`,`itemowner`,`buyoutprice`,`time`,`buyguid`,"
            "`lastbid`,`startbid`,`deposit`) VALUES "
            "(970081,7,880081,25,1,0,%u,0," UI64FMTD ",0,0,10,0),"
            "(970082,7,880082,25,1,0,%u,0," UI64FMTD ",2082,82,10,0)",
            AHBOT_SYSTEM_OWNER_GUID, now + HOUR,
            AHBOT_SYSTEM_OWNER_GUID, now + HOUR);
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) VALUES "
            "('botlist:test:repair:manual',1,3,0,%u,0,0,880081,970081," UI64FMTD ",0),"
            "('botlist:test:repair:pending',1,4,0,%u,0,0,880082,970082," UI64FMTD ",0),"
            "('botlist:test:repair:sweep',1,4,0,%u,0,0,880083,970083," UI64FMTD ",0)",
            AHBOT_SYSTEM_OWNER_GUID, oldTime,
            AHBOT_SYSTEM_OWNER_GUID, oldTime,
            AHBOT_SYSTEM_OWNER_GUID, oldTime);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("custody FAIL: repair ownership fixtures failed to commit\n");
            pass = false;
        }

        TestCliCapture applyCapture;
        CliHandler applyCli(0, SEC_ADMINISTRATOR, &applyCapture, &TestCliPrint);
        if (!applyCli.ParseCommands("ah repair apply") ||
            CountCliChunks(applyCapture,
                "mode=apply confirmed=1 pending=1 sweep-owned=1 repaired=0 skipped=3 failed=0") != 1)
        {
            printf("custody FAIL: repair ownership summary mismatch\n");
            pass = false;
        }

        char const* markerKeys[] = {
            "botlist:test:repair:manual",
            "botlist:test:repair:pending",
            "botlist:test:repair:sweep",
        };
        for (size_t i = 0; i < sizeof(markerKeys) / sizeof(markerKeys[0]); ++i)
        {
            CustodyRow markerRow;
            if (!CustodyLedger::Get(markerKeys[i], markerRow) ||
                markerRow.state != CST_RESERVED)
            {
                printf("custody FAIL: generic apply mutated reserved bot marker %s\n",
                    markerKeys[i]);
                pass = false;
            }
        }

        TestCliCapture forceCapture;
        CliHandler forceCli(0, SEC_ADMINISTRATOR, &forceCapture, &TestCliPrint);
        forceCli.ParseCommands(
            "ah repair force-forfeit botlist:test:repair:sweep");
        if (CountCliChunks(forceCapture, "reserved bot marker") != 1)
        {
            printf("custody FAIL: force-forfeit did not reject bot marker\n");
            pass = false;
        }

        CharacterDatabase.DirectExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'botlist:test:repair:%'");
        CharacterDatabase.DirectExecute(
            "DELETE FROM `auction` WHERE `id` IN (970081,970082)");
    }

    // Failed journal replay is counted separately and leaves custody reserved.
    {
        uint32 const failedAuctionId = 970084;
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key`='test:repair:failed'");
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `ah_worker_journal` WHERE `auction_id`=%u",
            failedAuctionId);
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:repair:failed',0,1,0,1,0,84,0,%u," UI64FMTD ",0)",
            failedAuctionId, oldTime);
        CharacterDatabase.PExecute(
            "INSERT INTO `ah_worker_journal` "
            "(`uuid`,`auction_id`,`kind`,`state`,`facts`,`created_time`,`resolved_time`) "
            "VALUES (970084,%u,%u,1,'00'," UI64FMTD "," UI64FMTD ")",
            failedAuctionId, uint32(IPC_PLAYER_CANCEL & 0xFFu),
            oldTime, oldTime);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("custody FAIL: failed-replay fixtures did not commit\n");
            pass = false;
        }

        TestCliCapture failedCapture;
        CliHandler failedCli(0, SEC_ADMINISTRATOR,
                             &failedCapture, &TestCliPrint);
        failedCli.ParseCommands("ah repair apply");
        CustodyRow failedRow;
        if (CountCliChunks(failedCapture,
                "mode=apply confirmed=1 pending=0 sweep-owned=0 repaired=0 skipped=0 failed=1") != 1 ||
            !CustodyLedger::Get("test:repair:failed", failedRow) ||
            failedRow.state != CST_RESERVED)
        {
            printf("custody FAIL: failed repair summary/state mismatch\n");
            pass = false;
        }

        CharacterDatabase.DirectPExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key`='test:repair:failed'");
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `ah_worker_journal` WHERE `auction_id`=%u",
            failedAuctionId);
    }

    // One budget spans scan and action details. Exact totals remain uncapped.
    {
        CharacterDatabase.DirectExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:repair:budget:%'");
        CharacterDatabase.BeginTransaction();
        for (uint32 i = 0; i < 101; ++i)
        {
            CharacterDatabase.PExecute(
                "INSERT INTO `custody_ledger` "
                "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
                "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
                "VALUES ('test:repair:budget:%u',0,1,0,1,0,%u,0,%u," UI64FMTD ",0)",
                i, 100 + i, 974000 + i, oldTime);
        }
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("custody FAIL: repair budget fixtures failed to commit\n");
            pass = false;
        }

        TestCliCapture dryCapture;
        CliHandler dryCli(0, SEC_ADMINISTRATOR, &dryCapture, &TestCliPrint);
        if (!dryCli.ParseCommands("ah repair --dry-run") ||
            CountCliChunks(dryCapture, "ah repair detail:") != 100 ||
            CountCliChunks(dryCapture, "detail(s) suppressed") != 1 ||
            CountCliChunks(dryCapture,
                "mode=dry-run confirmed=101 pending=0 sweep-owned=0 repaired=0 skipped=0 failed=0") != 1)
        {
            printf("custody FAIL: dry-run detail cap or totals mismatch\n");
            pass = false;
        }

        TestCliCapture boundedApplyCapture;
        CliHandler boundedApplyCli(0, SEC_ADMINISTRATOR,
                                   &boundedApplyCapture, &TestCliPrint);
        if (!boundedApplyCli.ParseCommands("ah repair apply") ||
            CountCliChunks(boundedApplyCapture, "ah repair detail:") +
                CountCliChunks(boundedApplyCapture, "ah repair action:") != 100 ||
            CountCliChunks(boundedApplyCapture, "detail(s) suppressed") != 1 ||
            CountCliChunks(boundedApplyCapture,
                "mode=apply confirmed=101 pending=0 sweep-owned=0 repaired=101 skipped=0 failed=0") != 1)
        {
            printf("custody FAIL: apply shared detail cap or totals mismatch\n");
            pass = false;
        }

        std::unique_ptr<QueryResult> reserved(CharacterDatabase.Query(
            "SELECT COUNT(*) FROM `custody_ledger` "
            "WHERE `idem_key` LIKE 'test:repair:budget:%' AND `state`=0"));
        if (!reserved || reserved->Fetch()[0].GetUInt32() != 0)
        {
            printf("custody FAIL: bounded apply did not repair all 101 rows\n");
            pass = false;
        }

        CharacterDatabase.DirectExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:repair:budget:%'");
    }

    // ================================================================ primitive
    // Primitive round-trip: offline owner (no ModifyMoney),
    // ReserveGold -> RollbackGoldLedgerOnly -> assert no mail.

    // Clean slate for the primitive sub-test.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:rg%%'");

    // ------------------------------------------------------------------ prim 1
    // ReserveGold with offline owner: inserts RESERVED row, no money debit.
    {
        CharacterDatabase.BeginTransaction();
        CustodyDeferred def;
        CustodyService::ReserveGold(def, 1, NULL, 50, "test:rg:1", 999,
                                    uint8(ROLE_BID));
        bool ok = CharacterDatabase.CommitTransactionChecked();
        if (!ok)
        {
            printf("custody FAIL: prim 1 ReserveGold CommitTransactionChecked returned false\n");
            pass = false;
        }
        def.run();
    }

    // ------------------------------------------------------------------ prim 2
    // Row must be visible: state CST_RESERVED, amount 50.
    {
        CustodyRow row;
        bool found = CustodyLedger::Get("test:rg:1", row);
        if (!found)
        {
            printf("custody FAIL: prim 2 Get returned false (row not found)\n");
            pass = false;
        }
        else
        {
            if (row.state != CST_RESERVED)
            {
                printf("custody FAIL: prim 2 state expected %u got %u\n",
                    uint32(CST_RESERVED), uint32(row.state));
                pass = false;
            }
            if (row.amount != 50)
            {
                printf("custody FAIL: prim 2 amount expected 50 got %u\n",
                    row.amount);
                pass = false;
            }
        }
    }

    // ------------------------------------------------------------------ prim 3
    // RollbackGoldLedgerOnly: ledger-only, no mail; state must flip to BACK.
    {
        CharacterDatabase.BeginTransaction();
        CustodyService::RollbackGoldLedgerOnly("test:rg:1");
        bool ok = CharacterDatabase.CommitTransactionChecked();
        if (!ok)
        {
            printf("custody FAIL: prim 3 RollbackGoldLedgerOnly CommitTransactionChecked returned false\n");
            pass = false;
        }

        CustodyRow row;
        bool found = CustodyLedger::Get("test:rg:1", row);
        if (!found)
        {
            printf("custody FAIL: prim 3 Get returned false after RollbackGoldLedgerOnly\n");
            pass = false;
        }
        else if (row.state != CST_TERMINAL_BACK)
        {
            printf("custody FAIL: prim 3 state expected %u got %u\n",
                uint32(CST_TERMINAL_BACK), uint32(row.state));
            pass = false;
        }
    }

    // ------------------------------------------------------------------ prim 4
    // The ledger-only rollback must NOT have sent any mail for receiver 1.
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`=1"
            " AND `subject` LIKE 'test:rg%%'"));
        uint64 cnt = res ? res->Fetch()[0].GetUInt64() : 0u;
        if (cnt != 0)
        {
            printf("custody FAIL: prim 4 ledger-only rollback sent unexpected mail"
                   " (count=%u)\n", uint32(cnt));
            pass = false;
        }
    }

    // ------------------------------------------------------------------ final cleanup
    // Remove all test:% rows from both runs.
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:%%'");
    CharacterDatabase.CommitTransactionChecked();

    if (pass)
    {
        printf("custody OK\n");
        return 0;
    }

    return 2;
}

/// Self-test for the AH bot forged system owner: proves GetPlayerGuidByName
/// returns the sentinel GUID for the reserved name without a DB row, that the
/// match is case-insensitive, and that ordinary non-existent names still fall
/// through to an empty guid. Returns 0 on pass, non-zero on fail.
static int RunAhOwnerTest()
{
    // Task 1: GetPlayerGuidByName intercepts the forged system name -> sentinel,
    // case-insensitively, with NO dependency on a characters row.
    {
        ObjectGuid g = sObjectMgr.GetPlayerGuidByName(AHBOT_SYSTEM_OWNER_NAME);
        if (!g.IsPlayer() || g.GetCounter() != AHBOT_SYSTEM_OWNER_GUID)
        {
            printf("ahowner FAIL: GetPlayerGuidByName(\"AuctionHouse\") did not return the sentinel\n");
            return 1;
        }
        ObjectGuid gl = sObjectMgr.GetPlayerGuidByName("auctionhouse");
        if (gl.GetCounter() != AHBOT_SYSTEM_OWNER_GUID)
        {
            printf("ahowner FAIL: name match is not case-insensitive\n");
            return 1;
        }
        // a clearly-nonexistent ordinary name must still fall through to 0.
        ObjectGuid none = sObjectMgr.GetPlayerGuidByName("Zzqxnonexistentname");
        if (none)
        {
            printf("ahowner FAIL: non-system name unexpectedly resolved\n");
            return 1;
        }
    }

    // Task 2: the player-GUID allocator never hands out the reserved sentinel.
    if (SkipAhBotSystemOwnerGuid(AHBOT_SYSTEM_OWNER_GUID) != AHBOT_SYSTEM_OWNER_GUID + 1)
    {
        printf("ahowner FAIL: SkipAhBotSystemOwnerGuid did not step past the sentinel\n");
        return 1;
    }
    if (SkipAhBotSystemOwnerGuid(42u) != 42u)
    {
        printf("ahowner FAIL: SkipAhBotSystemOwnerGuid altered a normal GUID\n");
        return 1;
    }

    // Task 3: the forged system name is always reserved from players, even if
    // the reserved_name table is empty (self-healing across wipes).
    sObjectMgr.LoadReservedPlayersNames();
    if (!sObjectMgr.IsReservedName(AHBOT_SYSTEM_OWNER_NAME))
    {
        printf("ahowner FAIL: \"AuctionHouse\" is not reserved after load\n");
        return 1;
    }

    // Task 4: the system-owner guid predicate the mail-gate keys on.
    if (!IsAhBotSystemOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, AHBOT_SYSTEM_OWNER_GUID)))
    {
        printf("ahowner FAIL: IsAhBotSystemOwnerGuid false for the sentinel\n");
        return 1;
    }
    if (IsAhBotSystemOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, 7u)))
    {
        printf("ahowner FAIL: IsAhBotSystemOwnerGuid true for a normal guid\n");
        return 1;
    }

    // Task 5: the supervisor refuses to spawn (no restart loop) when the bot
    // GUID is unresolved (0).
    {
        WorkerSupervisor sup("ahowner-test", "nonexistent-exe", 0, "secret", 0 /*botGuid*/, "nonexistent.conf");
        if (sup.Start())
        {
            printf("ahowner FAIL: WorkerSupervisor::Start succeeded with botGuid 0\n");
            return 1;
        }
    }

    printf("ahowner OK\n");
    return 0;
}

namespace
{
    struct RefCtx
    {
        const uint16* skills;
        const uint32* skillIds;
        size_t nSkills;
        const uint32* spells;
        size_t nSpells;
        const uint32* repFactions;
        const uint8* repRanks;
        size_t nReps;
    };

    uint16 RefSkill(void* c, uint32 id)
    {
        RefCtx* x = (RefCtx*)c;
        for (size_t i = 0; i < x->nSkills; ++i)
        {
            if (x->skillIds[i] == id)
            {
                return x->skills[i];
            }
        }
        return 0;
    }
    bool RefSpell(void* c, uint32 id)
    {
        RefCtx* x = (RefCtx*)c;
        for (size_t i = 0; i < x->nSpells; ++i)
        {
            if (x->spells[i] == id)
            {
                return true;
            }
        }
        return false;
    }
    uint8 RefRep(void* c, uint32 f)
    {
        RefCtx* x = (RefCtx*)c;
        for (size_t i = 0; i < x->nReps; ++i)
        {
            if (x->repFactions[i] == f)
            {
                return x->repRanks[i];
            }
        }
        return 0;
    }
}

/// Self-test for AhUsabilityRef::Evaluate: drives the production reference
/// evaluator over a battery of synthetic profiles. Returns 0 on pass.
static int RunAhUsabilityRefTest()
{
    uint32 skillIds[1] = { 43u };
    uint16 skills[1]   = { 200u };
    uint32 spells[1]   = { 123u };
    uint32 repF[1]     = { 609u };
    uint8  repR[1]     = { 5u };
    RefCtx ctx;
    ctx.skills      = skills;
    ctx.skillIds    = skillIds;
    ctx.nSkills     = 1;
    ctx.spells      = spells;
    ctx.nSpells     = 1;
    ctx.repFactions = repF;
    ctx.repRanks    = repR;
    ctx.nReps       = 1;

    // warrior (class 1) / human (race 1); level 40; honor rank 2
    const uint32 cm    = 1u << (1u - 1u);
    const uint32 rm    = 1u << (1u - 1u);
    const uint32 level = 40u;
    const uint32 honor = 2u;
    const uint32 MM    = 40u;
    const uint32 EM    = 60u;

    AhRefItem it;
    it.itemClass            = 2u;
    it.allowableClass       = 0xFFFFFFFFu;
    it.allowableRace        = 0xFFFFFFFFu;
    it.requiredLevel        = 30u;
    it.itemId               = 12345u;
    it.requiredSkill        = 43u;
    it.requiredSkillRank    = 150u;
    it.requiredSpell        = 0u;
    it.requiredHonorRank    = 0u;
    it.requiredRepFaction   = 0u;
    it.requiredRepRank      = 0u;
    it.itemProficiencySkill = 43u;

    // baseline: everything satisfied
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, it,
                                 RefSkill, RefSpell, RefRep, &ctx) != AHUSE_OK)
    {
        printf("ahusabilityref FAIL: baseline\n");
        return 1;
    }

    // class gate: wrong class mask
    AhRefItem b = it;
    b.allowableClass = 0x80u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: class gate\n");
        return 1;
    }

    // honor gate fires when direct_action=true and rank is insufficient
    b = it;
    b.requiredHonorRank = 5u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: honor gate (direct_action)\n");
        return 1;
    }
    // D2: honor gate must NOT fire when direct_action=false
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, false, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) != AHUSE_OK)
    {
        printf("ahusabilityref FAIL: honor must be skipped when !direct_action\n");
        return 1;
    }

    // reputation gate: player rep rank 5, item requires 7
    b = it;
    b.requiredRepFaction = 609u;
    b.requiredRepRank    = 7u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: reputation gate\n");
        return 1;
    }

    // epic mount override: item 12302 bumps requiredLevel to EM(60); player is 40
    b = it;
    b.itemId         = 12302u;
    b.requiredLevel  = 30u;
    if (AhUsabilityRef::Evaluate(cm, rm, level, honor, true, MM, EM, b,
                                 RefSkill, RefSpell, RefRep, &ctx) == AHUSE_OK)
    {
        printf("ahusabilityref FAIL: epic-mount override\n");
        return 1;
    }

    printf("ahusabilityref OK\n");
    return 0;
}

/// Smoke test for the in-process browse helper (dispatch shape + order contract).
/// Does NOT require a live Player or world data: it exercises only the
/// client-outbid-prepend order invariant via the AhAppendClientOutbidsForTest
/// seam, proving that BuildListForKind will prepend outbid ids in CLIENT ORDER
/// before calling BuildListBidderItems. Returns 0 on pass, non-zero on fail.
static int RunAhBrowseHelperTest()
{
    // The bidder client-outbid-prepend is extracted as a free helper so it can
    // be tested without a Player. It appends each client outbid auction id in
    // CLIENT ORDER ahead of the sweep (matches HandleAuctionListBidderItems).
    // Here we assert the ORDER-PRESERVING contract over a stub appender.
    std::vector<uint32> clientIds;
    clientIds.push_back(7u);
    clientIds.push_back(3u);
    clientIds.push_back(9u);
    std::vector<uint32> appended;
    AhAppendClientOutbidsForTest(clientIds, appended);
    if (appended.size() != 3u || appended[0] != 7u || appended[1] != 3u || appended[2] != 9u)
    {
        printf("ahbrowsehelper FAIL: client outbid order not preserved\n");
        return 1;
    }
    printf("ahbrowsehelper OK\n");
    return 0;
}

/// Self-test for the SP-1 browse pending-map + SMSG assembly: exercises
/// Register/Take/Sweep, per-(char,kind) sequencing, and AhAssembleBrowseListBody.
/// No DB / world data needed. Returns 0 on pass, non-zero on fail.
static int RunAhBrowsePendingTest()
{
    BrowsePendingMap pend;
    PendingBrowse a; a.accountId=1001u; a.playerGuidLow=5001u; a.kind=uint8(BROWSE_LIST);
    a.seq = pend.NextSeqFor(5001u, uint8(BROWSE_LIST));
    uint64 id1 = pend.Register(a, 100u);
    PendingBrowse b; b.accountId=1002u; b.playerGuidLow=5002u; b.kind=uint8(BROWSE_OWNER);
    b.seq = pend.NextSeqFor(5002u, uint8(BROWSE_OWNER));
    uint64 id2 = pend.Register(b, 100u);
    if (id1 == id2 || pend.Size() != 2u)
    {
        printf("ahbrowsepending FAIL: register\n");
        return 1;
    }

    PendingBrowse got;
    if (!pend.Take(id1, got) || got.accountId!=1001u || got.playerGuidLow!=5001u ||
        got.kind != uint8(BROWSE_LIST))
    {
        printf("ahbrowsepending FAIL: Take(id1)\n");
        return 1;
    }
    if (pend.Take(id1, got))
    {
        printf("ahbrowsepending FAIL: Take twice\n");
        return 1;
    }

    // Per-(char,kind) sequencing: a newer search bumps the seq; the older seq
    // is no longer current (a stale timeout for it must be ignored).
    uint32 newSeq = pend.NextSeqFor(5002u, uint8(BROWSE_OWNER));   // newer than b.seq
    if (pend.IsCurrent(5002u, uint8(BROWSE_OWNER), b.seq))
    {
        printf("ahbrowsepending FAIL: stale seq still current\n");
        return 1;
    }
    if (!pend.IsCurrent(5002u, uint8(BROWSE_OWNER), newSeq))
    {
        printf("ahbrowsepending FAIL: newest seq not current\n");
        return 1;
    }

    // TTL sweep collects the timed-out entry (id2) for in-process fallback.
    std::vector<PendingBrowse> timedOut;
    pend.Sweep(100u + 10u + 1u, 10u, timedOut);
    if (pend.Size() != 0u || timedOut.size() != 1u || timedOut[0].accountId != 1002u)
    {
        printf("ahbrowsepending FAIL: sweep\n");
        return 1;
    }

    // Stale-seq guard on the sweep path: after a newer search is issued, the
    // swept entry's seq (b.seq) is NOT current -- the fallback must be ignored.
    // Bump the seq (simulates a newer search that arrived before the sweep ran).
    uint32 postSweepSeq = pend.NextSeqFor(5002u, uint8(BROWSE_OWNER));
    if (pend.IsCurrent(5002u, uint8(BROWSE_OWNER), b.seq))
    {
        printf("ahbrowsepending FAIL: swept stale seq still current\n");
        return 1;
    }
    if (!pend.IsCurrent(5002u, uint8(BROWSE_OWNER), postSweepSeq))
    {
        printf("ahbrowsepending FAIL: postSweepSeq not current\n");
        return 1;
    }

    // SMSG assembly preserves the worker's complete bidder order, including a
    // duplicate auction id, and writes the worker-provided totalcount.
    {
        std::vector<BrowseEntry> entries;
        const uint32 orderedIds[] = { 9u, 3u, 9u, 7u };
        for (size_t i = 0; i < 4u; ++i)
        {
            BrowseEntry e;
            e.id=orderedIds[i]; e.itemEntry=19019u; e.enchantId=0u;
            e.randomPropId=0u; e.suffixFactor=0u; e.count=1u;
            e.charges=0; e.ownerGuidLow=4u; e.startbid=100u;
            e.outbid=0u; e.buyout=5000u; e.timeLeftMs=720000u;
            e.bidderGuidLow=0u; e.curBid=100u;
            entries.push_back(e);
        }
        ByteBuffer body;
        AhAssembleBrowseListBody(entries, 4u, body);
        uint32 c=0; body >> c;
        if (c != 4u)
        {
            printf("ahbrowsepending FAIL: assembled count %u\n",
                   unsigned(c));
            return 1;
        }
        uint32 dummy32 = 0; uint64 dummy64 = 0;
        for (size_t i = 0; i < 4u; ++i)
        {
            uint32 id = 0; body >> id;
            if (id != orderedIds[i])
            {
                printf("ahbrowsepending FAIL: ordered id[%u]=%u\n",
                       unsigned(i), unsigned(id));
                return 1;
            }
            body >> dummy32 >> dummy32 >> dummy32;
            body >> dummy32 >> dummy32 >> dummy32;
            body >> dummy64;
            body >> dummy32 >> dummy32 >> dummy32 >> dummy32;
            body >> dummy64;
            body >> dummy32;
        }
        uint32 totalcount = 0; body >> totalcount;
        if (totalcount != 4u)
        {
            printf("ahbrowsepending FAIL: totalcount %u\n",
                   unsigned(totalcount));
            return 1;
        }
        (void)dummy32; (void)dummy64;
    }

    printf("ahbrowsepending OK\n");
    return 0;
}

/// Self-test for the SP-2 MutationPendingMap (spec 5.5): apply-all consume-once,
/// per-player + total caps checked BEFORE any reserve, cancel-phase rearm, and
/// the TTL sweep that tombstones in-doubt entries (emitting each newly-in-doubt
/// uuid exactly once for the AUCTION_ERR_DATABASE result). Pure in-memory; no
/// DB rows, no worker. Returns 0 on pass, non-zero on fail.
static int RunAhMutPendingTest()
{
    MutationPendingMap pend;

    // Mint sanity: two uuids differ and carry a nonzero (boot-second) high word,
    // keeping them disjoint from worker-minted (runId << 32) | seq uuids.
    uint64 u1 = AhMintMutationUuid();
    uint64 u2 = AhMintMutationUuid();
    if (u1 == u2 || (u1 >> 32) == 0u)
    {
        printf("ahmutpending FAIL: uuid mint\n");
        return 1;
    }

    // (a) cap-before-reserve: MAX_PER_PLAYER entries fill player 100's slot;
    // the next CanRegister is refused BEFORE any reserve would happen, while
    // another player is unaffected.
    std::vector<uint64> uuids;
    for (size_t i = 0; i < MutationPendingMap::MAX_PER_PLAYER; ++i)
    {
        if (!pend.CanRegister(100u))
        {
            printf("ahmutpending FAIL: CanRegister refused below cap\n");
            return 1;
        }

        PendingMutation pm;
        pm.uuid           = AhMintMutationUuid();
        pm.playerGuidLow  = 100u;
        pm.op             = uint16(IPC_PLAYER_BID);
        pm.auctionId      = uint32(9000u + i);
        pm.state          = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec        = 1000u;
        pm.reservedAmount = 55u;
        pm.reserveKey     = "bid:9000:1";
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);
        uuids.push_back(pm.uuid);
    }
    if (pend.CanRegister(100u))
    {
        printf("ahmutpending FAIL: per-player cap not enforced\n");
        return 1;
    }
    if (!pend.CanRegister(101u))
    {
        printf("ahmutpending FAIL: cap leaked to another player\n");
        return 1;
    }

    // (b) consume-once: Take returns the entry once, then never again; the
    // per-player slot is released.
    PendingMutation got;
    if (!pend.Take(uuids[0], got) || got.playerGuidLow != 100u ||
        got.op != uint16(IPC_PLAYER_BID) || got.reservedAmount != 55u ||
        got.reserveKey != "bid:9000:1")
    {
        printf("ahmutpending FAIL: Take(first)\n");
        return 1;
    }
    if (pend.Take(uuids[0], got))
    {
        printf("ahmutpending FAIL: Take twice\n");
        return 1;
    }
    if (!pend.CanRegister(100u))
    {
        printf("ahmutpending FAIL: slot not released after Take\n");
        return 1;
    }

    // (c) rearm (cancel phase 2, spec 4.2): AWAIT_RESULT (== AWAIT_PREPARE for
    // cancels) -> AWAIT_CONFIRM in the SAME cap slot, with a fresh TTL anchor.
    pend.RearmConfirm(uuids[1], 2000u);
    if (!pend.Peek(uuids[1], got) || got.state != uint8(PMUT_AWAIT_CONFIRM) ||
        got.sentSec != 2000u)
    {
        printf("ahmutpending FAIL: RearmConfirm\n");
        return 1;
    }

    // (d) explicit tombstone: the entry STAYS (in-doubt, reservation
    // non-terminal); Tombstone on a missing uuid reports false.
    if (!pend.Tombstone(uuids[2]))
    {
        printf("ahmutpending FAIL: Tombstone(existing)\n");
        return 1;
    }
    if (pend.Tombstone(0xDEADBEEFull))
    {
        printf("ahmutpending FAIL: Tombstone(missing) returned true\n");
        return 1;
    }

    // (e) sweep-to-tombstone: at now=2005 with ttl=10, entries stamped 1000 are
    // overdue EXCEPT the rearmed one (sentSec 2000). Already-tombstoned entries
    // are NOT re-emitted (their AUCTION_ERR_DATABASE was already sent).
    // Registered survivors: uuids[1..7]; uuids[1] fresh, uuids[2] pre-tombstoned
    // -> exactly uuids[3..7] (5) are newly in doubt.
    std::vector<uint64> inDoubt;
    pend.SweepToTombstones(2005u, 10u, inDoubt);
    if (inDoubt.size() != 5u)
    {
        printf("ahmutpending FAIL: sweep emitted %u (want 5)\n", unsigned(inDoubt.size()));
        return 1;
    }
    for (size_t i = 0; i < inDoubt.size(); ++i)
    {
        if (inDoubt[i] == uuids[1] || inDoubt[i] == uuids[2])
        {
            printf("ahmutpending FAIL: sweep emitted a fresh/tombstoned uuid\n");
            return 1;
        }
    }
    // A second sweep emits nothing new (everything overdue is tombstoned).
    inDoubt.clear();
    pend.SweepToTombstones(2005u, 10u, inDoubt);
    if (!inDoubt.empty())
    {
        printf("ahmutpending FAIL: second sweep re-emitted\n");
        return 1;
    }
    // Late reply against a tombstone still applies (apply-all): Take consumes it.
    if (!pend.Take(uuids[2], got) || got.state != uint8(PMUT_TOMBSTONE))
    {
        printf("ahmutpending FAIL: Take(tombstone)\n");
        return 1;
    }

    // (f) MAX_TOTAL: fill a fresh map to the global cap across many players
    // (4 per player, under the per-player cap); a fresh player is then refused.
    MutationPendingMap full;
    for (size_t i = 0; i < MutationPendingMap::MAX_TOTAL; ++i)
    {
        PendingMutation pm;
        pm.uuid           = 0x100000000ull + i;
        pm.playerGuidLow  = uint32(1000u + (i / 4u));
        pm.op             = uint16(IPC_PLAYER_SELL);
        pm.auctionId      = uint32(i + 1u);
        pm.state          = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec        = 1u;
        pm.reservedAmount = 0u;
        pm.reserveKey.clear();
        pm.itemKey.clear();
        pm.depKey.clear();
        full.Register(pm);
    }
    if (full.CanRegister(999999u))
    {
        printf("ahmutpending FAIL: MAX_TOTAL not enforced\n");
        return 1;
    }

    printf("ahmutpending OK\n");
    return 0;
}

/// DB-level self-test for the SP-2 forward-path reserve shapes (Task 10), no
/// live worker and no Player: owner guid 1 is offline so ReserveGold takes the
/// ledger-only path. Covers (a) the sell reserve pair item:<id> + dep:<id> in
/// ONE checked txn plus its pending registration, and (b) the bid classifier
/// AhClassifyBidForward against real ledger rows: no row -> IPC_PLAYER_BUYOUT
/// full reserve; own live row -> IPC_PLAYER_BUYOUT delta; price <= own live bid
/// -> 0 (inline ERR_HIGHER_BID); another owner's row -> BUYOUT full;
/// ambiguous (two live rows) -> BUYOUT full. Returns 0 on pass.
static int RunAhForwardReserveTest()
{
    bool pass = true;

    CharacterDatabase.AllowAsyncTransactions();

    uint32 const sellId = 990001u;   // synthetic ids far above live data
    uint32 const bidId  = 990002u;

    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` IN (990001, 990002)");

    // ---- (a) sell shape: escrow + deposit reserve, one checked txn ----
    MutationPendingMap pend;
    if (!pend.CanRegister(1u))
    {
        printf("ahforwardreserve FAIL: CanRegister(empty)\n");
        pass = false;
    }

    std::string const sellIdStr = std::to_string(sellId);
    CharacterDatabase.BeginTransaction();
    CustodyService::EscrowItem(1u, 424242u, "item:" + sellIdStr, sellId);
    CustodyService::ReserveGoldAlreadyDebited(1u, 77u, "dep:" + sellIdStr, sellId, ROLE_DEPOSIT);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        printf("ahforwardreserve FAIL: sell reserve txn\n");
        pass = false;
    }

    CustodyRow row;
    if (!CustodyLedger::Get("item:" + sellIdStr, row) ||
        row.kind != CUSTODY_ITEM || row.state != CST_RESERVED ||
        row.itemGuid != 424242u || row.auctionId != sellId)
    {
        printf("ahforwardreserve FAIL: item row shape\n");
        pass = false;
    }
    if (!CustodyLedger::Get("dep:" + sellIdStr, row) ||
        row.kind != CUSTODY_GOLD || row.role != ROLE_DEPOSIT ||
        row.state != CST_RESERVED || row.amount != 77u)
    {
        printf("ahforwardreserve FAIL: dep row shape\n");
        pass = false;
    }

    uint64 const sellUuid = AhMintMutationUuid();
    PendingMutation pm;
    pm.uuid           = sellUuid;
    pm.playerGuidLow  = 1u;
    pm.op             = uint16(IPC_PLAYER_SELL);
    pm.auctionId      = sellId;
    pm.state          = uint8(PMUT_AWAIT_RESULT);
    pm.sentSec        = uint32(time(NULL));
    pm.reservedAmount = 0u;
    pm.reserveKey.clear();
    pm.itemKey        = "item:" + sellIdStr;
    pm.depKey         = "dep:" + sellIdStr;
    pend.Register(pm);

    PendingMutation got;
    if (!pend.Take(sellUuid, got) || got.itemKey != "item:" + sellIdStr ||
        got.depKey != "dep:" + sellIdStr || got.op != uint16(IPC_PLAYER_SELL))
    {
        printf("ahforwardreserve FAIL: sell pending registration\n");
        pass = false;
    }

    // ---- (a2) cancel shape: no legacy AuctionsMap/AuctionEntry state needed ----
    MutationPendingMap cancelPend;
    uint64 const cancelUuid = UINT64_C(0xCA00000000000043);
    IpcMessage cancelFrame;
    if (!AhBuildCancelPrepareForward(cancelPend, 1u, sellId, cancelUuid, 1234u,
                                     cancelFrame))
    {
        printf("ahforwardreserve FAIL: cancel forward helper refused empty map\n");
        pass = false;
    }
    if (cancelFrame.op != IPC_PLAYER_CANCEL)
    {
        printf("ahforwardreserve FAIL: cancel forward opcode\n");
        pass = false;
    }
    PlayerCancelPrepare cancelIntent;
    if (!cancelIntent.Decode(cancelFrame.body) ||
        cancelIntent.uuid != cancelUuid || cancelIntent.auctionId != sellId ||
        cancelIntent.sellerGuid != 1u)
    {
        printf("ahforwardreserve FAIL: cancel forward body\n");
        pass = false;
    }
    PendingMutation cancelPm;
    if (!cancelPend.Take(cancelUuid, cancelPm) ||
        cancelPm.playerGuidLow != 1u ||
        cancelPm.op != uint16(IPC_PLAYER_CANCEL) ||
        cancelPm.auctionId != sellId ||
        cancelPm.state != uint8(PMUT_AWAIT_RESULT) ||
        cancelPm.sentSec != 1234u ||
        cancelPm.reservedAmount != 0u ||
        !cancelPm.reserveKey.empty() ||
        !cancelPm.itemKey.empty() ||
        !cancelPm.depKey.empty())
    {
        printf("ahforwardreserve FAIL: cancel pending shape\n");
        pass = false;
    }

    // ---- (b) bid classifier: no live row -> BUYOUT, full reserve ----
    uint32 reserveAmount = 0u;
    if (AhClassifyBidForward(bidId, 1u, 500u, reserveAmount) != uint16(IPC_PLAYER_BUYOUT) ||
        reserveAmount != 500u)
    {
        printf("ahforwardreserve FAIL: classify(no row)\n");
        pass = false;
    }

    // Seed guid 1's live bid of 500 with the handler's key scheme, one txn.
    std::string const bidKey = "bid:" + std::to_string(bidId) + ":" +
                               std::to_string(CustodyLedger::NextBidSeq(bidId));
    {
        CustodyDeferred def;
        CharacterDatabase.BeginTransaction();
        CustodyService::ReserveGold(def, 1u, NULL, 500u, bidKey, bidId, ROLE_BID);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahforwardreserve FAIL: bid reserve txn\n");
            pass = false;
        }
    }
    if (!CustodyLedger::GetSingleLiveBidRow(bidId, row) ||
        row.idemKey != bidKey || row.ownerGuid != 1u || row.amount != 500u)
    {
        printf("ahforwardreserve FAIL: single live bid row\n");
        pass = false;
    }

    // Holder raise/top-up 500 -> 600: route on IPC_PLAYER_BUYOUT with the
    // delta reserve. The worker treats a below-buyout 0x42 as a normal bid,
    // while an at/over-buyout 0x42 becomes the buyout win; both preserve the
    // same ledger delta semantics and avoid at-buyout 0x41 protocol faults.
    if (AhClassifyBidForward(bidId, 1u, 600u, reserveAmount) != uint16(IPC_PLAYER_BUYOUT) ||
        reserveAmount != 100u)
    {
        printf("ahforwardreserve FAIL: classify(raise delta)\n");
        pass = false;
    }
    // Holder "raise" to own bid: 0 -> inline legacy ERR_HIGHER_BID.
    if (AhClassifyBidForward(bidId, 1u, 500u, reserveAmount) != 0u)
    {
        printf("ahforwardreserve FAIL: classify(price <= own bid)\n");
        pass = false;
    }
    // Another player: BUYOUT, full reserve (worker adjudicates, spec I6).
    if (AhClassifyBidForward(bidId, 2u, 600u, reserveAmount) != uint16(IPC_PLAYER_BUYOUT) ||
        reserveAmount != 600u)
    {
        printf("ahforwardreserve FAIL: classify(other bidder)\n");
        pass = false;
    }

    // A raise-in-flight adds a SECOND live ROLE_BID row (the delta reserve);
    // exactly-one is then violated and the classifier falls back to BUYOUT
    // full reserve -- a value-safe over-reserve (transient, spec 12).
    std::string const deltaKey = "bid:" + std::to_string(bidId) + ":" +
                                 std::to_string(CustodyLedger::NextBidSeq(bidId));
    {
        CustodyDeferred def;
        CharacterDatabase.BeginTransaction();
        CustodyService::ReserveGold(def, 1u, NULL, 100u, deltaKey, bidId, ROLE_BID);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahforwardreserve FAIL: delta reserve txn\n");
            pass = false;
        }
    }
    if (CustodyLedger::GetSingleLiveBidRow(bidId, row))
    {
        printf("ahforwardreserve FAIL: two live rows not rejected\n");
        pass = false;
    }
    if (AhClassifyBidForward(bidId, 1u, 700u, reserveAmount) != uint16(IPC_PLAYER_BUYOUT) ||
        reserveAmount != 700u)
    {
        printf("ahforwardreserve FAIL: classify(ambiguous rows)\n");
        pass = false;
    }

    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` IN (990001, 990002)");

    if (pass)
    {
        printf("ahforwardreserve OK\n");
        return 0;
    }

    return 2;
}

/// SP-2: ReleaseGoldToWallet (online + offline re-credit) and the
/// applied-record idempotency helpers (WriteResolutionApplied / ResolutionApplied).
static int RunAhReleaseTest()
{
    bool pass = true;

    // Clean slate for this test's rows and a synthetic offline character.
    // NOTE: the applied-record key is "resolve:<uuid in DECIMAL>" (see
    // WriteResolutionApplied); UINT64_C(0x0000009900000001) below decimalises
    // to 657129996289, so the cleanup pattern must match that, not the hex
    // digits.
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:rel%%'"
        " OR `idem_key` = 'resolve:657129996289'");

    // ---- offline re-credit: seed a RESERVED row + a characters money row ----
    uint32 const offlineGuid = 970101u;
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `characters` WHERE `guid` = %u", offlineGuid);
    CharacterDatabase.DirectPExecute(
        "INSERT INTO `characters` (`guid`,`account`,`name`,`money`) "
        "VALUES (%u, 1, 'RelTestOff', 500)", offlineGuid);
    {
        CharacterDatabase.BeginTransaction();
        CustodyRow r;
        r.id = 0; r.idemKey = "test:rel:offline"; r.kind = CUSTODY_GOLD;
        r.role = ROLE_BID; r.state = CST_RESERVED; r.ownerGuid = offlineGuid;
        r.beneficiaryGuid = 0; r.amount = 250; r.itemGuid = 0;
        r.auctionId = 970101; r.createdTime = static_cast<uint64>(time(NULL));
        r.resolvedTime = 0;
        CustodyLedger::Insert(r);
        CharacterDatabase.CommitTransactionChecked();
    }
    {
        CustodyDeferred def;
        CharacterDatabase.BeginTransaction();
        CustodyService::ReleaseGoldToWallet(def, offlineGuid, NULL, 250,
                                            "test:rel:offline");
        bool ok = CharacterDatabase.CommitTransactionChecked();
        def.run();
        if (!ok)
        {
            printf("ahrelease FAIL: offline commit returned false\n");
            pass = false;
        }
        QueryResult* q = CharacterDatabase.PQuery(
            "SELECT `money` FROM `characters` WHERE `guid` = %u", offlineGuid);
        if (!q || q->Fetch()[0].GetUInt32() != 750u)  // 500 + 250
        {
            printf("ahrelease FAIL: offline money not credited (expected 750)\n");
            pass = false;
        }
        delete q;
        CustodyRow row;
        if (!CustodyLedger::Get("test:rel:offline", row) ||
            row.state != CST_TERMINAL_BACK)
        {
            printf("ahrelease FAIL: offline row not CST_TERMINAL_BACK\n");
            pass = false;
        }
    }

    // ---- applied-record: write once, detect duplicate, missing => false ----
    {
        uint64 const uuid = UINT64_C(0x0000009900000001);
        if (CustodyService::ResolutionApplied(uuid))
        {
            printf("ahrelease FAIL: applied-record present before write\n");
            pass = false;
        }
        CharacterDatabase.BeginTransaction();
        CustodyService::WriteResolutionApplied(970200u, uuid);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahrelease FAIL: applied-record commit returned false\n");
            pass = false;
        }
        if (!CustodyService::ResolutionApplied(uuid))
        {
            printf("ahrelease FAIL: applied-record not detected after write\n");
            pass = false;
        }
    }

    // Cleanup (see decimal-key NOTE above).
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:rel%%'"
        " OR `idem_key` = 'resolve:657129996289'");
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `characters` WHERE `guid` = %u", offlineGuid);

    if (pass)
    {
        printf("ahrelease OK\n");
    }
    return pass ? 0 : 1;
}

/// SP-2 self-test for AhHandlePlayerMutationResult: finalizes crafted worker
/// results against SEEDED custody rows + a seeded MutationPending entry. No live
/// worker, no world data. Recipients of AH mail / gold re-credits use guid 1,
/// which the test SEEDS as an offline `characters` row with an account (the
/// legacy AH mail path guards on GetPlayerAccountIdByGUID != 0, so a recipient
/// with no `characters` row is correctly skipped -- see RunAhReleaseTest); guid
/// 99999 stays an offline-nobody bidder. Covers the reconciliation that an
/// op=0x42 (buyout intent) MUT_OK is a WIN only when effectiveBid==buyout, else
/// a normal standing bid. Returns 0 on pass.
static int RunAhMutResultTest()
{
    bool pass = true;
    CharacterDatabase.AllowAsyncTransactions();
    sObjectMgr.SetHighestGuids();       // mail ids collide otherwise (RunMailTest)

    // Clean slate from any prior run (auction-id scoped -- covers both the
    // test:mut* keys and the hardcoded dep:/item: keys the buyout finalize uses).
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` IN (990001,990002,990003,990004,990005)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver` IN (1,2) AND `subject` LIKE '19019:%'");

    // Seed the offline recipient (guid 1) with an account + durable money so the
    // account-guarded AH mail path delivers and the offline gold re-credit UPDATE
    // has a row to hit. World data is NOT loaded under -t, so GetPlayer(1) is
    // NULL and every value motion takes the offline (durable-DB) branch.
    CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid`=1");
    CharacterDatabase.DirectExecute(
        "INSERT INTO `characters` (`guid`,`account`,`name`,`money`) "
        "VALUES (1, 1, 'AhMutTestRcv', 100000)");

    // Helper lambda: read guid 1's durable money.
    auto readMoney = []() -> uint64
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT `money` FROM `characters` WHERE `guid`=1"));
        return res ? res->Fetch()[0].GetUInt64() : 0;
    };

    // Helper lambda: read a custody row's state (255 = missing).
    auto rowState = [](char const* key) -> uint32
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT `state` FROM `custody_ledger` WHERE `idem_key`='%s'", key));
        return res ? res->Fetch()[0].GetUInt32() : 255u;
    };

    // Helper lambda: count mails matching a money+subject to receiver 1.
    auto mailCount = [](uint32 money, char const* subject) -> uint64
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`=1 AND `money`=%u AND `subject`='%s'",
            money, subject));
        return res ? res->Fetch()[0].GetUInt64() : 0;
    };

    MutationPendingMap& pend = sWorld.GetMutationPending();

    // ---- Part A: MUT_OK bid displacing a real prior bidder ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:mut:prior',0,1,0,1,0,777,0,990001,0,0),"
            "       ('test:mut:own',0,1,0,99999,0,900,0,990001,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (bid)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xA1ull;
        pm.playerGuidLow = 99999u;          // nonexistent char: offline, no account
        pm.op = uint16(IPC_PLAYER_BID);
        pm.auctionId = 990001u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 900u;
        pm.reserveKey = "test:mut:own";
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);

        PlayerMutationResult res;
        res.uuid = 0xA1ull;
        res.op = uint8(IPC_PLAYER_BID & 0xFFu);
        res.status = uint8(MUT_OK);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990001u;
        res.facts.houseId = 7;
        res.facts.itemTemplate = 19019u;
        res.facts.randomPropertyId = 0;
        res.facts.sellerGuid = 2u;
        res.facts.effectiveBid = 900u;
        res.facts.curBid = 900u;
        res.facts.curBidderGuid = 99999u;
        res.facts.priorBidderGuid = 1u;
        res.facts.priorBidAmount = 777u;
        AhHandlePlayerMutationResult(res);

        if (rowState("test:mut:prior") != 2u)
        {
            printf("ahmutresult FAIL: prior bid row not TERMINAL_BACK\n");
            pass = false;
        }
        if (rowState("test:mut:own") != 0u)
        {
            printf("ahmutresult FAIL: new live bid row must stay RESERVED\n");
            pass = false;
        }
        if (mailCount(777u, "19019:0:0") != 1u)
        {
            printf("ahmutresult FAIL: outbid refund mail missing\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xA1ull, gone))
        {
            printf("ahmutresult FAIL: OK pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part A2: MUT_OK sell -> no value motion, rows stay RESERVED ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:mut:dep',0,0,0,1,0,25,0,990002,0,0),"
            "       ('test:mut:item',1,3,0,1,0,0,424242,990002,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (sell)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xA5ull;
        pm.playerGuidLow = 1u;
        pm.op = uint16(IPC_PLAYER_SELL);
        pm.auctionId = 990002u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 25u;
        pm.reserveKey.clear();
        pm.itemKey = "test:mut:item";
        pm.depKey = "test:mut:dep";
        pend.Register(pm);

        PlayerMutationResult res;
        res.uuid = 0xA5ull;
        res.op = uint8(IPC_PLAYER_SELL & 0xFFu);
        res.status = uint8(MUT_OK);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990002u;
        res.facts.houseId = 7;
        res.facts.itemGuid = 424242u;
        res.facts.itemTemplate = 19019u;
        res.facts.sellerGuid = 1u;
        res.facts.deposit = 25u;
        AhHandlePlayerMutationResult(res);

        if (rowState("test:mut:dep") != 0u || rowState("test:mut:item") != 0u)
        {
            printf("ahmutresult FAIL: sell OK must leave dep+item RESERVED\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xA5ull, gone))
        {
            printf("ahmutresult FAIL: sell pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part A3: MUT_OK op=0x42 buyout WIN (effectiveBid == buyout) ----
    // The worker removed the row at buyout: seller paid, item to winner,
    // buyer reserve committed as proceeds, deposit returned, remainder released.
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:mut:bowin',0,1,0,99999,0,1000,0,990003,0,0),"
            "       ('dep:990003',0,0,0,1,0,50,0,990003,0,0),"
            "       ('item:990003',1,3,0,1,0,0,424243,990003,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (buyout-win)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xA8ull;
        pm.playerGuidLow = 99999u;          // buyer offline-nobody
        pm.op = uint16(IPC_PLAYER_BUYOUT);
        pm.auctionId = 990003u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 1000u;          // maxPrice
        pm.reserveKey = "test:mut:bowin";
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);

        PlayerMutationResult res;
        res.uuid = 0xA8ull;
        res.op = uint8(IPC_PLAYER_BUYOUT & 0xFFu);
        res.status = uint8(MUT_OK);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990003u;
        res.facts.houseId = 7;
        res.facts.itemGuid = 424243u;
        res.facts.itemTemplate = 19019u;
        res.facts.randomPropertyId = 0;
        res.facts.sellerGuid = 1u;          // seller has an account -> gets payout mail
        res.facts.deposit = 50u;
        res.facts.effectiveBid = 800u;
        res.facts.curBid = 800u;
        res.facts.curBidderGuid = 99999u;
        res.facts.buyout = 800u;            // effectiveBid == buyout -> WIN
        AhHandlePlayerMutationResult(res);

        if (rowState("test:mut:bowin") != 1u)
        {
            printf("ahmutresult FAIL: buyout-win reserve not TERMINAL_OK (proceeds)\n");
            pass = false;
        }
        if (rowState("dep:990003") != 2u)
        {
            printf("ahmutresult FAIL: buyout-win deposit not TERMINAL_BACK\n");
            pass = false;
        }
        if (rowState("item:990003") != 1u)
        {
            printf("ahmutresult FAIL: buyout-win item not TERMINAL_OK\n");
            pass = false;
        }
        if (mailCount(850u, "19019:0:2") != 1u)  // profit=800+50-cut(0 under -t)
        {
            printf("ahmutresult FAIL: buyout-win seller payout mail missing\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xA8ull, gone))
        {
            printf("ahmutresult FAIL: buyout-win pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part A4: MUT_OK op=0x42 committed by worker as a NORMAL BID ----
    // effectiveBid < buyout: the row stayed LIVE. The buyer reserve becomes the
    // standing bid (stays RESERVED); ONLY the displaced prior bidder is refunded;
    // no seller payout, no item delivery, no release.
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:mut:bobid',0,1,0,99999,0,500,0,990004,0,0),"
            "       ('test:mut:bobidprior',0,1,0,1,0,400,0,990004,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (buyout-as-bid)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xA9ull;
        pm.playerGuidLow = 99999u;          // buyer offline-nobody
        pm.op = uint16(IPC_PLAYER_BUYOUT);
        pm.auctionId = 990004u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 500u;           // reserved == effectiveBid -> release 0
        pm.reserveKey = "test:mut:bobid";
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);

        PlayerMutationResult res;
        res.uuid = 0xA9ull;
        res.op = uint8(IPC_PLAYER_BUYOUT & 0xFFu);
        res.status = uint8(MUT_OK);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990004u;
        res.facts.houseId = 7;
        res.facts.itemTemplate = 19019u;
        res.facts.randomPropertyId = 0;
        res.facts.sellerGuid = 2u;
        res.facts.effectiveBid = 500u;
        res.facts.curBid = 500u;
        res.facts.curBidderGuid = 99999u;
        res.facts.priorBidderGuid = 1u;
        res.facts.priorBidAmount = 400u;
        res.facts.buyout = 1000u;           // effectiveBid < buyout -> NOT a win
        AhHandlePlayerMutationResult(res);

        if (rowState("test:mut:bobid") != 0u)
        {
            printf("ahmutresult FAIL: buyout-as-bid reserve must stay RESERVED\n");
            pass = false;
        }
        if (rowState("test:mut:bobidprior") != 2u)
        {
            printf("ahmutresult FAIL: buyout-as-bid prior bidder not refunded\n");
            pass = false;
        }
        if (mailCount(400u, "19019:0:0") != 1u)
        {
            printf("ahmutresult FAIL: buyout-as-bid outbid refund mail missing\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xA9ull, gone))
        {
            printf("ahmutresult FAIL: buyout-as-bid pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part A5: MUT_OK cancel CONFIRM finalizes seller return + deposit ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('dep:990005',0,0,0,1,0,32,0,990005,0,0),"
            "       ('item:990005',1,3,0,1,0,0,424245,990005,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (cancel-confirm)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xAAull;
        pm.playerGuidLow = 1u;
        pm.op = uint16(IPC_PLAYER_CANCEL);
        pm.auctionId = 990005u;
        pm.state = uint8(PMUT_AWAIT_CONFIRM);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 0u;
        pm.reserveKey.clear();
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);

        PlayerMutationResult res;
        res.uuid = 0xAAull;
        res.op = uint8(IPC_PLAYER_CANCEL_CONFIRM & 0xFFu);
        res.status = uint8(MUT_OK);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990005u;
        res.facts.houseId = 7;
        res.facts.itemGuid = 424245u;
        res.facts.itemTemplate = 19019u;
        res.facts.randomPropertyId = 0;
        res.facts.sellerGuid = 1u;
        res.facts.deposit = 32u;
        AhHandlePlayerMutationResult(res);

        if (rowState("dep:990005") != 1u)
        {
            printf("ahmutresult FAIL: cancel-confirm deposit not TERMINAL_OK\n");
            pass = false;
        }
        if (rowState("item:990005") != 1u)
        {
            printf("ahmutresult FAIL: cancel-confirm item not TERMINAL_OK\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xAAull, gone))
        {
            printf("ahmutresult FAIL: cancel-confirm pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part B: MUT_REJECTED buyout -> ReleaseGoldToWallet (offline) ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:mut:rej',0,1,0,1,0,555,0,990001,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (rej)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xA2ull;
        pm.playerGuidLow = 1u;
        pm.op = uint16(IPC_PLAYER_BUYOUT);
        pm.auctionId = 990001u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 555u;
        pm.reserveKey = "test:mut:rej";
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);

        uint64 const before = readMoney();

        PlayerMutationResult res;
        res.uuid = 0xA2ull;
        res.op = uint8(IPC_PLAYER_BUYOUT & 0xFFu);
        res.status = uint8(MUT_REJECTED);
        res.reason = uint8(AUCTION_ERR_BID_INCREMENT);
        res.facts = MutationFacts();
        res.facts.auctionId = 990001u;
        AhHandlePlayerMutationResult(res);

        if (rowState("test:mut:rej") != 2u)
        {
            printf("ahmutresult FAIL: rejected row not TERMINAL_BACK\n");
            pass = false;
        }
        if (readMoney() != before + 555u)
        {
            printf("ahmutresult FAIL: rejected release not credited\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xA2ull, gone))
        {
            printf("ahmutresult FAIL: rejected pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part C: MUT_REJECTED_STALE cancel -> release cut + resolve ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('test:mut:cut',0,2,0,1,0,55,0,990001,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmutresult FAIL: seed commit (cut)\n");
            return 2;
        }

        PendingMutation pm;
        pm.uuid = 0xA3ull;
        pm.playerGuidLow = 1u;
        pm.op = uint16(IPC_PLAYER_CANCEL);
        pm.auctionId = 990001u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 0u;
        pm.reserveKey.clear();
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);
        pend.RearmConfirm(0xA3ull, uint32(time(NULL)));
        if (!pend.SetReserve(0xA3ull, 55u, "test:mut:cut"))
        {
            printf("ahmutresult FAIL: SetReserve\n");
            pass = false;
        }

        uint64 const before = readMoney();

        PlayerMutationResult res;
        res.uuid = 0xA3ull;
        res.op = uint8(IPC_PLAYER_CANCEL & 0xFFu);
        res.status = uint8(MUT_REJECTED_STALE);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990001u;
        AhHandlePlayerMutationResult(res);

        if (rowState("test:mut:cut") != 2u)
        {
            printf("ahmutresult FAIL: stale cut row not TERMINAL_BACK\n");
            pass = false;
        }
        if (readMoney() != before + 55u)
        {
            printf("ahmutresult FAIL: stale cut not credited\n");
            pass = false;
        }
        PendingMutation gone;
        if (pend.Peek(0xA3ull, gone))
        {
            printf("ahmutresult FAIL: stale pending not consumed\n");
            pass = false;
        }
    }

    // ---- Part D: unknown uuid -> loud protocol fault, no crash ----
    {
        PlayerMutationResult res;
        res.uuid = 0xDEADull;
        res.op = uint8(IPC_PLAYER_BID & 0xFFu);
        res.status = uint8(MUT_OK);
        res.reason = 0;
        res.facts = MutationFacts();
        AhHandlePlayerMutationResult(res);   // must only log
    }

    // ---- Part E: MUT_PREPARED with offline seller -> ABORT path consumes ----
    {
        PendingMutation pm;
        pm.uuid = 0xA6ull;
        pm.playerGuidLow = 99999u;          // offline -> can't debit cut -> ABORT
        pm.op = uint16(IPC_PLAYER_CANCEL);
        pm.auctionId = 990001u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = uint32(time(NULL));
        pm.reservedAmount = 0u;
        pm.reserveKey.clear();
        pm.itemKey.clear();
        pm.depKey.clear();
        pend.Register(pm);

        PlayerMutationResult res;
        res.uuid = 0xA6ull;
        res.op = uint8(IPC_PLAYER_CANCEL & 0xFFu);
        res.status = uint8(MUT_PREPARED);
        res.reason = 0;
        res.facts = MutationFacts();
        res.facts.auctionId = 990001u;
        res.facts.houseId = 7;
        res.facts.curBid = 500u;            // nonzero bid -> cut is owed -> gate engages
        res.facts.curBidderGuid = 1u;
        AhHandlePlayerMutationResult(res);   // sv==NULL under -t: frame skipped

        PendingMutation gone;
        if (pend.Peek(0xA6ull, gone))
        {
            printf("ahmutresult FAIL: PREPARED abort did not consume pending\n");
            pass = false;
        }
    }

    // ---- Part F: sweep tombstones a stale entry ----
    {
        PendingMutation pm;
        pm.uuid = 0xA7ull;
        pm.playerGuidLow = 99999u;
        pm.op = uint16(IPC_PLAYER_BID);
        pm.auctionId = 990001u;
        pm.state = uint8(PMUT_AWAIT_RESULT);
        pm.sentSec = 100u;                   // ancient
        pm.reservedAmount = 0u;
        pend.Register(pm);
        std::vector<uint64> inDoubt;
        pend.SweepToTombstones(uint32(time(NULL)), 10u, inDoubt);
        bool swept = false;
        for (size_t i = 0; i < inDoubt.size(); ++i)
        {
            if (inDoubt[i] == 0xA7ull)
            {
                swept = true;
            }
        }
        PendingMutation t;
        if (!swept || !pend.Peek(0xA7ull, t) || t.state != uint8(PMUT_TOMBSTONE))
        {
            printf("ahmutresult FAIL: sweep did not tombstone\n");
            pass = false;
        }
        AhNotifyMutationInDoubt(t);          // offline -> no packet; must not crash
        PendingMutation consumed;
        pend.Take(0xA7ull, consumed);        // clean the slot for re-runs
    }

    // Clean up.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` IN (990001,990002,990003,990004,990005)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver` IN (1,2) AND `subject` LIKE '19019:%'");
    CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid`=1");

    if (pass)
    {
        printf("ahmutresult OK\n");
        return 0;
    }
    return 2;
}

/// SP-2 Task 12 self-test for AhHandleResolveApply: applies crafted
/// worker-initiated resolutions (WON / EXPIRED_NOBID / CANCELLED_UNLOCK /
/// REPAIR_RETURN) against SEEDED custody rows -- no live worker, no world data.
/// Asserts each kind's ledger flips + value mails, the resolve:<uuid>
/// applied-record, and that a SECOND apply returns RES_DUPLICATE without
/// double-applying. Recipients use guid 1 (seeded offline with an account so the
/// account-guarded AH mail path delivers -- see RunAhMutResultTest). World data
/// is NOT loaded under -t, so GetAItem() is always NULL: the item legs take the
/// escrow-cache-miss (ledger-only) branch and no physical item mail is asserted.
/// Returns 0 on pass.
static int RunAhResolveTest()
{
    bool pass = true;
    CharacterDatabase.AllowAsyncTransactions();
    sObjectMgr.SetHighestGuids();       // mail ids collide otherwise (RunMailTest)

    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` IN (991001,991002,991003,991004,991005)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `subject` LIKE '19019:%'");
    CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid`=1");
    CharacterDatabase.DirectExecute(
        "INSERT INTO `characters` (`guid`,`account`,`name`,`money`) "
        "VALUES (1, 1, 'AhResTestRcv', 100000)");

    auto readMoney = []() -> uint64
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT `money` FROM `characters` WHERE `guid`=1"));
        return res ? res->Fetch()[0].GetUInt64() : 0;
    };
    auto rowState = [](char const* key) -> uint32
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT `state` FROM `custody_ledger` WHERE `idem_key`='%s'", key));
        return res ? res->Fetch()[0].GetUInt32() : 255u;
    };
    auto mailCount = [](uint32 money, char const* subject) -> uint64
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`=1 AND `money`=%u AND `subject`='%s'",
            money, subject));
        return res ? res->Fetch()[0].GetUInt64() : 0;
    };

    // ---- RESOLVE_WON: bid + dep + item terminal; seller payout mail ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('bid:991001:1',0,1,0,99999,0,800,0,991001,0,0),"
            "       ('dep:991001',0,0,0,1,0,50,0,991001,0,0),"
            "       ('item:991001',1,3,0,1,0,0,424242,991001,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahresolve FAIL: seed commit (won)\n");
            return 2;
        }

        ResolveApply ra;
        ra.uuid = 0xB1ull;
        ra.kind = uint8(RESOLVE_WON);
        ra.facts = MutationFacts();
        ra.facts.auctionId = 991001u;
        ra.facts.houseId = 7;
        ra.facts.itemGuid = 424242u;
        ra.facts.itemTemplate = 19019u;
        ra.facts.randomPropertyId = 0;
        ra.facts.sellerGuid = 1u;           // has an account -> gets payout mail
        ra.facts.deposit = 50u;
        ra.facts.effectiveBid = 800u;
        ra.facts.curBid = 800u;
        ra.facts.curBidderGuid = 99999u;    // offline-nobody winner
        ra.facts.buyout = 800u;

        if (AhHandleResolveApply(ra) != uint8(RES_APPLIED))
        {
            printf("ahresolve FAIL: WON not RES_APPLIED\n");
            pass = false;
        }
        if (!CustodyService::ResolutionApplied(0xB1ull))
        {
            printf("ahresolve FAIL: WON applied-record missing\n");
            pass = false;
        }
        if (rowState("bid:991001:1") != 1u)
        {
            printf("ahresolve FAIL: WON bid row not TERMINAL_OK\n");
            pass = false;
        }
        if (rowState("dep:991001") != 1u)
        {
            printf("ahresolve FAIL: WON dep row not TERMINAL_OK\n");
            pass = false;
        }
        if (rowState("item:991001") != 1u)
        {
            printf("ahresolve FAIL: WON item row not TERMINAL_OK\n");
            pass = false;
        }
        if (mailCount(850u, "19019:0:2") != 1u)   // profit = 800+50-cut(0 under -t)
        {
            printf("ahresolve FAIL: WON seller payout mail missing\n");
            pass = false;
        }

        // Duplicate: RES_DUPLICATE, no second payout mail, rows unchanged.
        if (AhHandleResolveApply(ra) != uint8(RES_DUPLICATE))
        {
            printf("ahresolve FAIL: WON second apply not RES_DUPLICATE\n");
            pass = false;
        }
        if (mailCount(850u, "19019:0:2") != 1u)
        {
            printf("ahresolve FAIL: WON duplicate double-applied payout mail\n");
            pass = false;
        }
    }

    // ---- RESOLVE_WON fail-closed [F2]: a REAL winner (curBidderGuid != 0) with
    // NO live bid row must pay/deliver NOTHING and return RES_FAILED (mirror
    // AhFinalizeBidOk), so the winner's RESERVED bid can never leak and the
    // resolution re-drives. Seed dep + item but NO bid row: pre-fix this path
    // silently skipped the bid terminal yet STILL paid the seller + wrote the
    // applied-record. (A legit BOT win has curBidderGuid == 0 and no bid row --
    // that proceed case is exercised by the REPAIR/bot-win paths.)
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('dep:991005',0,0,0,1,0,30,0,991005,0,0),"
            "       ('item:991005',1,3,0,1,0,0,424245,991005,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahresolve FAIL: seed commit (won fail-closed)\n");
            return 2;
        }

        ResolveApply ra;
        ra.uuid = 0xB5ull;
        ra.kind = uint8(RESOLVE_WON);
        ra.facts = MutationFacts();
        ra.facts.auctionId = 991005u;
        ra.facts.houseId = 7;
        ra.facts.itemGuid = 424245u;
        ra.facts.itemTemplate = 19019u;
        ra.facts.randomPropertyId = 0;
        ra.facts.sellerGuid = 1u;           // would get a payout mail if wrongly paid
        ra.facts.deposit = 30u;
        ra.facts.effectiveBid = 700u;
        ra.facts.curBid = 700u;
        ra.facts.curBidderGuid = 99999u;    // REAL winner, but NO live bid row seeded
        ra.facts.buyout = 700u;

        if (AhHandleResolveApply(ra) != uint8(RES_FAILED))
        {
            printf("ahresolve FAIL: WON-fail-closed not RES_FAILED\n");
            pass = false;
        }
        if (CustodyService::ResolutionApplied(0xB5ull))
        {
            printf("ahresolve FAIL: WON-fail-closed wrote applied-record\n");
            pass = false;
        }
        if (mailCount(730u, "19019:0:2") != 0u)    // seller must NOT be paid (700+30-cut0)
        {
            printf("ahresolve FAIL: WON-fail-closed paid the seller\n");
            pass = false;
        }
        if (rowState("dep:991005") != 0u)          // rolled back -> still RESERVED
        {
            printf("ahresolve FAIL: WON-fail-closed dep row not rolled back\n");
            pass = false;
        }
        if (rowState("item:991005") != 0u)         // rolled back -> still RESERVED
        {
            printf("ahresolve FAIL: WON-fail-closed item row not rolled back\n");
            pass = false;
        }
    }

    // ---- RESOLVE_EXPIRED_NOBID: deposit forfeit + item returned ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('dep:991002',0,0,0,1,0,40,0,991002,0,0),"
            "       ('item:991002',1,3,0,1,0,0,424244,991002,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahresolve FAIL: seed commit (expired)\n");
            return 2;
        }

        ResolveApply ra;
        ra.uuid = 0xB2ull;
        ra.kind = uint8(RESOLVE_EXPIRED_NOBID);
        ra.facts = MutationFacts();
        ra.facts.auctionId = 991002u;
        ra.facts.houseId = 7;
        ra.facts.itemGuid = 424244u;
        ra.facts.itemTemplate = 19019u;
        ra.facts.sellerGuid = 1u;
        ra.facts.deposit = 40u;

        if (AhHandleResolveApply(ra) != uint8(RES_APPLIED))
        {
            printf("ahresolve FAIL: EXPIRED not RES_APPLIED\n");
            pass = false;
        }
        if (rowState("dep:991002") != 1u)          // forfeit -> TERMINAL_OK
        {
            printf("ahresolve FAIL: EXPIRED deposit not forfeit (TERMINAL_OK)\n");
            pass = false;
        }
        if (rowState("item:991002") != 1u)         // returned -> TERMINAL_OK
        {
            printf("ahresolve FAIL: EXPIRED item not TERMINAL_OK\n");
            pass = false;
        }
        if (!CustodyService::ResolutionApplied(0xB2ull))
        {
            printf("ahresolve FAIL: EXPIRED applied-record missing\n");
            pass = false;
        }
        if (AhHandleResolveApply(ra) != uint8(RES_DUPLICATE))
        {
            printf("ahresolve FAIL: EXPIRED second apply not RES_DUPLICATE\n");
            pass = false;
        }
    }

    // ---- RESOLVE_CANCELLED_UNLOCK: cut released to seller, no item/bid move ----
    // The cut key is uuid-salted in production ("cut:<auc>:<uuid>"), so this
    // exercises the by-auction cut scan (AhFindLiveCutRow), not a point key.
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('cut:991003:12345',0,2,0,1,0,55,0,991003,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahresolve FAIL: seed commit (cancelled)\n");
            return 2;
        }

        ResolveApply ra;
        ra.uuid = 0xB3ull;
        ra.kind = uint8(RESOLVE_CANCELLED_UNLOCK);
        ra.facts = MutationFacts();
        ra.facts.auctionId = 991003u;
        ra.facts.sellerGuid = 1u;

        uint64 const before = readMoney();
        if (AhHandleResolveApply(ra) != uint8(RES_APPLIED))
        {
            printf("ahresolve FAIL: CANCELLED not RES_APPLIED\n");
            pass = false;
        }
        if (rowState("cut:991003:12345") != 2u)    // released -> TERMINAL_BACK
        {
            printf("ahresolve FAIL: CANCELLED cut not TERMINAL_BACK\n");
            pass = false;
        }
        if (readMoney() != before + 55u)
        {
            printf("ahresolve FAIL: CANCELLED cut not credited to seller\n");
            pass = false;
        }
        if (!CustodyService::ResolutionApplied(0xB3ull))
        {
            printf("ahresolve FAIL: CANCELLED applied-record missing\n");
            pass = false;
        }

        uint64 const afterFirst = readMoney();
        if (AhHandleResolveApply(ra) != uint8(RES_DUPLICATE))
        {
            printf("ahresolve FAIL: CANCELLED second apply not RES_DUPLICATE\n");
            pass = false;
        }
        if (readMoney() != afterFirst)
        {
            printf("ahresolve FAIL: CANCELLED duplicate double-credited\n");
            pass = false;
        }
    }

    // ---- RESOLVE_REPAIR_RETURN (bot displaced a player): prior-bidder refund ----
    {
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('bid:991004:1',0,1,0,1,0,400,0,991004,0,0)");
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahresolve FAIL: seed commit (repair)\n");
            return 2;
        }

        ResolveApply ra;
        ra.uuid = 0xB4ull;
        ra.kind = uint8(RESOLVE_REPAIR_RETURN);
        ra.facts = MutationFacts();
        ra.facts.auctionId = 991004u;
        ra.facts.houseId = 7;
        ra.facts.itemTemplate = 19019u;
        ra.facts.randomPropertyId = 0;
        ra.facts.sellerGuid = 2u;
        ra.facts.curBidderGuid = 0u;        // a bot now holds the top bid
        ra.facts.priorBidderGuid = 1u;      // displaced real player
        ra.facts.priorBidAmount = 400u;

        if (AhHandleResolveApply(ra) != uint8(RES_APPLIED))
        {
            printf("ahresolve FAIL: REPAIR not RES_APPLIED\n");
            pass = false;
        }
        if (rowState("bid:991004:1") != 2u)        // prior bid -> TERMINAL_BACK
        {
            printf("ahresolve FAIL: REPAIR prior bid not TERMINAL_BACK\n");
            pass = false;
        }
        if (mailCount(400u, "19019:0:0") != 1u)    // outbid refund to guid 1
        {
            printf("ahresolve FAIL: REPAIR prior-bidder refund mail missing\n");
            pass = false;
        }
        if (!CustodyService::ResolutionApplied(0xB4ull))
        {
            printf("ahresolve FAIL: REPAIR applied-record missing\n");
            pass = false;
        }
        if (AhHandleResolveApply(ra) != uint8(RES_DUPLICATE))
        {
            printf("ahresolve FAIL: REPAIR second apply not RES_DUPLICATE\n");
            pass = false;
        }
        if (mailCount(400u, "19019:0:0") != 1u)
        {
            printf("ahresolve FAIL: REPAIR duplicate double-refunded\n");
            pass = false;
        }
    }

    // Clean up.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` IN (991001,991002,991003,991004,991005)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `subject` LIKE '19019:%'");
    CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid`=1");

    if (pass)
    {
        printf("ahresolve OK\n");
        return 0;
    }
    return 2;
}

/// Route and conservation regressions for seller custody and bid custody as
/// independent dimensions. Uses only disposable Character DB fixtures.
static int RunAhCustodyRouteTest()
{
    bool pass = true;
    CharacterDatabase.AllowAsyncTransactions();
    sObjectMgr.LoadItemPrototypes();

    uint32 itemId = 2589u;
    if (!ObjectMgr::GetItemPrototype(itemId))
    {
        std::unique_ptr<QueryResult> result(WorldDatabase.Query(
            "SELECT `entry` FROM `item_template` "
            "WHERE `InventoryType`=0 AND `stackable`>1 ORDER BY `entry` LIMIT 1"));
        if (result)
        {
            itemId = result->Fetch()[0].GetUInt32();
        }
    }
    if (!ObjectMgr::GetItemPrototype(itemId))
    {
        printf("ahcustodyroute FAIL: no usable item prototype\n");
        return 2;
    }

    uint32 const sellerGuid = 9501u;
    uint32 const bidderGuid = 9502u;
    uint32 const otherBidderGuid = 9503u;
    uint64 const now = static_cast<uint64>(time(NULL));
    uint64 const oldTime = now > 7200u ? now - 7200u : 1u;
    std::vector<uint32> itemGuids;

    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail_items` WHERE `receiver` IN (9501,9502,9503)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver` IN (9501,9502,9503)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` BETWEEN 995100 AND 995199");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `auction` WHERE `id` BETWEEN 995100 AND 995199");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `item_instance` WHERE `owner_guid` IN (9501,9502,9503)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `characters` WHERE `guid` IN (9501,9502,9503)");
    CharacterDatabase.DirectExecute(
        "INSERT INTO `characters` (`guid`,`account`,`name`,`money`) VALUES "
        "(9501,9501,'AhRtSeller',1000),"
        "(9502,9502,'AhRtBidder',1000),"
        "(9503,9503,'AhRtOther',1000)");
    sObjectMgr.SetHighestGuids();

    AuctionHouseEntry house = {};
    house.houseId = 7;
    house.faction = 0;
    house.depositPercent = 5;
    house.cutPercent = 5;

    auto seedRow = [oldTime](std::string const& key, uint8 kind, uint8 role,
                             uint32 owner, uint32 amount, uint32 itemGuid,
                             uint32 auctionId)
    {
        CharacterDatabase.DirectPExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('%s',%u,%u,0,%u,0,%u,%u,%u," UI64FMTD ",0)",
            key.c_str(), uint32(kind), uint32(role), owner, amount,
            itemGuid, auctionId, oldTime);
    };

    auto createAuction = [&](uint32 auctionId, uint32 owner, uint32 bidder,
                             uint32 bid, uint32 buyout,
                             uint32 deposit) -> AuctionEntry*
    {
        Item* item = Item::CreateItem(itemId, 1);
        if (!item)
        {
            return NULL;
        }
        item->SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, owner));
        itemGuids.push_back(item->GetGUIDLow());

        AuctionEntry* auction = new AuctionEntry();
        auction->Id = auctionId;
        auction->itemGuidLow = item->GetGUIDLow();
        auction->itemTemplate = itemId;
        auction->itemCount = 1;
        auction->itemRandomPropertyId = 0;
        auction->owner = owner;
        auction->startbid = 10;
        auction->bid = bid;
        auction->buyout = buyout;
        auction->expireTime = static_cast<time_t>(now + HOUR);
        auction->bidder = bidder;
        auction->deposit = deposit;
        auction->auctionHouseEntry = &house;

        CharacterDatabase.BeginTransaction();
        item->SaveToDB();
        auction->SaveToDB();
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            delete item;
            delete auction;
            return NULL;
        }
        sAuctionMgr.AddAItem(item);
        return auction;
    };

    auto rowState = [](std::string const& key) -> uint32
    {
        CustodyRow row;
        return CustodyLedger::Get(key, row) ? uint32(row.state) : 255u;
    };
    auto auctionExists = [](uint32 auctionId) -> bool
    {
        return CustodyLedger::AuctionExists(auctionId);
    };
    auto mailCount = [itemId](uint32 receiver, MailAuctionAnswers answer) -> uint32
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail` WHERE `receiver`=%u "
            "AND `subject`='%u:0:%u'", receiver, itemId, uint32(answer)));
        return result ? result->Fetch()[0].GetUInt32() : 0u;
    };
    auto mailMoney = [itemId](uint32 receiver, MailAuctionAnswers answer) -> uint64
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT COALESCE(SUM(`money`),0) FROM `mail` WHERE `receiver`=%u "
            "AND `subject`='%u:0:%u'", receiver, itemId, uint32(answer)));
        return result ? result->Fetch()[0].GetUInt64() : 0u;
    };
    auto itemMailCount = [](uint32 receiver, uint32 itemGuid) -> uint32
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail_items` WHERE `receiver`=%u AND `item_guid`=%u",
            receiver, itemGuid));
        return result ? result->Fetch()[0].GetUInt32() : 0u;
    };
    auto characterMoney = [](uint32 guid) -> uint32
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT `money` FROM `characters` WHERE `guid`=%u", guid));
        return result ? result->Fetch()[0].GetUInt32() : 0u;
    };
    auto sellerRows = [](uint32 auctionId) -> uint32
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `custody_ledger` WHERE `auction_id`=%u "
            "AND `state`=0 AND (`idem_key` IN ('item:%u','dep:%u') "
            "OR `role` IN (%u,%u))",
            auctionId, auctionId, auctionId,
            uint32(ROLE_ITEM), uint32(ROLE_DEPOSIT)));
        return result ? result->Fetch()[0].GetUInt32() : 0u;
    };
    auto clearFixtureMail = []()
    {
        CharacterDatabase.DirectExecute(
            "DELETE FROM `mail_items` WHERE `receiver` IN (9501,9502,9503)");
        CharacterDatabase.DirectExecute(
            "DELETE FROM `mail` WHERE `receiver` IN (9501,9502,9503)");
    };

    // Exact runtime route matrix, including the seller-only unsold-expiry fact.
    seedRow("item:995101", CUSTODY_ITEM, ROLE_ITEM,
            sellerGuid, 0, 1, 995101);
    seedRow("dep:995101", CUSTODY_GOLD, ROLE_DEPOSIT,
            sellerGuid, 5, 0, 995101);
    seedRow("bid:995102:1", CUSTODY_GOLD, ROLE_BID,
            bidderGuid, 20, 0, 995102);
    seedRow("resolve:test:995103", CUSTODY_GOLD, ROLE_RESOLUTION,
            0, 0, 0, 995103);
    seedRow("botlist:test:995104", CUSTODY_ITEM, ROLE_RESOLUTION,
            AHBOT_SYSTEM_OWNER_GUID, 0, 4, 995104);
    seedRow("botlist:test:995105", CUSTODY_ITEM, ROLE_RESOLUTION,
            AHBOT_SYSTEM_OWNER_GUID, 0, 5, 995105);
    seedRow("bid:995105:1", CUSTODY_GOLD, ROLE_BID,
            bidderGuid, 50, 0, 995105);
    seedRow("botlist:test:995106", CUSTODY_ITEM, ROLE_RESOLUTION,
            AHBOT_SYSTEM_OWNER_GUID, 0, 6, 995106);
    seedRow("item:995106", CUSTODY_ITEM, ROLE_ITEM,
            AHBOT_SYSTEM_OWNER_GUID, 0, 6, 995106);

    struct RouteExpectation
    {
        uint32 auctionId;
        bool seller;
        bool bid;
    };
    RouteExpectation const routeExpectations[] = {
        { 995101, true,  false },
        { 995102, false, true  },
        { 995103, false, false },
        { 995104, false, false },
        { 995105, false, true  },
        { 995106, false, false },
    };
    for (size_t i = 0; i < sizeof(routeExpectations) / sizeof(routeExpectations[0]); ++i)
    {
        CustodyRouteState const route =
            CustodyLedger::GetRouteState(routeExpectations[i].auctionId);
        if (route.usesPlayerSellerCustody != routeExpectations[i].seller ||
            route.hasLiveBidCustody != routeExpectations[i].bid)
        {
            printf("ahcustodyroute FAIL: route matrix mismatch auction=%u\n",
                   routeExpectations[i].auctionId);
            pass = false;
        }
    }
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` BETWEEN 995100 AND 995109");

    // Marker-only first bid remains on the legacy path and creates no bid or
    // seller custody while debiting/persisting exactly once.
    {
        uint32 const auctionId = 995110;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, 0, 0, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: legacy first-bid fixture creation\n");
            pass = false;
        }
        else
        {
            seedRow("botlist:test:995110", CUSTODY_ITEM, ROLE_RESOLUTION,
                    AHBOT_SYSTEM_OWNER_GUID, 0, auction->itemGuidLow, auctionId);
            CustodyRouteState const route = CustodyLedger::GetRouteState(auctionId);
            WorldSession session(9502, std::shared_ptr<proto::IClientLink>(),
                                 std::shared_ptr<SessionMailbox>(), SEC_PLAYER,
                                 0, LOCALE_enUS);
            Player bidder(&session);
            session.SetPlayer(&bidder);
            bidder._Create(bidderGuid, HIGHGUID_PLAYER);
            bidder.SetMoney(1000);
            bool const active = auction->UpdateBid(100, &bidder);
            CharacterDatabase.BeginTransaction();
            CharacterDatabase.CommitTransactionChecked();

            std::unique_ptr<QueryResult> persisted(CharacterDatabase.PQuery(
                "SELECT `buyguid`,`lastbid` FROM `auction` WHERE `id`=%u",
                auctionId));
            std::unique_ptr<QueryResult> bidRows(CharacterDatabase.PQuery(
                "SELECT COUNT(*) FROM `custody_ledger` WHERE `auction_id`=%u "
                "AND `state`=0 AND `role`=%u",
                auctionId, uint32(ROLE_BID)));
            if (route.usesPlayerSellerCustody || route.hasLiveBidCustody ||
                !active || bidder.GetMoney() != 900 ||
                characterMoney(bidderGuid) != 900 || !persisted ||
                persisted->Fetch()[0].GetUInt32() != bidderGuid ||
                persisted->Fetch()[1].GetUInt32() != 100 || !bidRows ||
                bidRows->Fetch()[0].GetUInt32() != 0 ||
                rowState("botlist:test:995110") != CST_RESERVED)
            {
                printf("ahcustodyroute FAIL: marker-only first bid left legacy behavior\n");
                pass = false;
            }
            session.SetPlayer(NULL);

            Item* liveItem = sAuctionMgr.GetAItem(auction->itemGuidLow);
            sAuctionMgr.RemoveAItem(auction->itemGuidLow);
            delete liveItem;
            delete auction;
            CharacterDatabase.DirectPExecute(
                "DELETE FROM `auction` WHERE `id`=%u", auctionId);
            CharacterDatabase.DirectPExecute(
                "DELETE FROM `custody_ledger` WHERE `auction_id`=%u", auctionId);
        }
    }

    // Bid-winning expiry on a bot listing: seller and winner settlement remains
    // legacy-equivalent, while only the player bid row is terminalized.
    {
        clearFixtureMail();
        uint32 const auctionId = 995120;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, bidderGuid, 100, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: bid-only win fixture creation\n");
            pass = false;
        }
        else
        {
            uint32 const itemGuid = auction->itemGuidLow;
            std::string const markerKey = "botlist:test:995120";
            std::string const bidKey = "bid:995120:1";
            seedRow(markerKey, CUSTODY_ITEM, ROLE_RESOLUTION,
                    AHBOT_SYSTEM_OWNER_GUID, 0, itemGuid, auctionId);
            seedRow(bidKey, CUSTODY_GOLD, ROLE_BID,
                    bidderGuid, 100, 0, auctionId);
            CustodyRouteState const route = CustodyLedger::GetRouteState(auctionId);
            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            auction->AuctionBidWinningCustody(NULL, def,
                                              route.usesPlayerSellerCustody,
                                              route.hasLiveBidCustody, bidKey);
            bool const committed = CharacterDatabase.CommitTransactionChecked();
            if (committed)
            {
                def.run();
            }
            if (!committed || route.usesPlayerSellerCustody ||
                !route.hasLiveBidCustody || auctionExists(auctionId) ||
                rowState(bidKey) != CST_TERMINAL_OK ||
                rowState(markerKey) != CST_RESERVED || sellerRows(auctionId) != 0 ||
                mailCount(sellerGuid, AUCTION_SUCCESSFUL) != 1 ||
                mailMoney(sellerGuid, AUCTION_SUCCESSFUL) == 0 ||
                mailCount(bidderGuid, AUCTION_WON) != 1 ||
                itemMailCount(bidderGuid, itemGuid) != 1)
            {
                printf("ahcustodyroute FAIL: bid-only winning expiry conservation\n");
                pass = false;
            }
        }
    }

    // Same-bidder buyout preserves bot provenance, debits only the delta, and
    // terminalizes the existing bid row without inventing seller rows.
    {
        clearFixtureMail();
        uint32 const auctionId = 995121;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, bidderGuid, 40, 100, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: buyout fixture creation\n");
            pass = false;
        }
        else
        {
            uint32 const itemGuid = auction->itemGuidLow;
            std::string const markerKey = "botlist:test:995121";
            std::string const bidKey = "bid:995121:1";
            seedRow(markerKey, CUSTODY_ITEM, ROLE_RESOLUTION,
                    AHBOT_SYSTEM_OWNER_GUID, 0, itemGuid, auctionId);
            seedRow(bidKey, CUSTODY_GOLD, ROLE_BID,
                    bidderGuid, 40, 0, auctionId);

            CharacterDatabase.DirectPExecute(
                "UPDATE `characters` SET `money`=1000 WHERE `guid`=%u",
                bidderGuid);
            WorldSession session(9502, std::shared_ptr<proto::IClientLink>(),
                                 std::shared_ptr<SessionMailbox>(), SEC_PLAYER,
                                 0, LOCALE_enUS);
            Player bidder(&session);
            session.SetPlayer(&bidder);
            bidder._Create(bidderGuid, HIGHGUID_PLAYER);
            bidder.SetMoney(1000);

            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            bool const active = auction->UpdateBidCustody(
                100, &bidder, def, false, true, bidKey);
            bool const committed = CharacterDatabase.CommitTransactionChecked();
            if (committed)
            {
                def.run();
            }
            if (!committed || active || bidder.GetMoney() != 940 ||
                characterMoney(bidderGuid) != 940 || auctionExists(auctionId) ||
                rowState(bidKey) != CST_TERMINAL_OK ||
                rowState(markerKey) != CST_RESERVED || sellerRows(auctionId) != 0 ||
                mailCount(sellerGuid, AUCTION_SUCCESSFUL) != 1 ||
                itemMailCount(bidderGuid, itemGuid) != 1)
            {
                printf("ahcustodyroute FAIL: bid-only buyout conservation\n");
                pass = false;
            }
            session.SetPlayer(NULL);
        }
    }

    // Owner cancel on a bot listing refunds the bidder and returns the item,
    // while only bid custody transitions and the bot marker remains reserved.
    {
        clearFixtureMail();
        uint32 const auctionId = 995122;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, otherBidderGuid, 100, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: cancel fixture creation\n");
            pass = false;
        }
        else
        {
            uint32 const itemGuid = auction->itemGuidLow;
            std::string const markerKey = "botlist:test:995122";
            std::string const bidKey = "bid:995122:1";
            seedRow(markerKey, CUSTODY_ITEM, ROLE_RESOLUTION,
                    AHBOT_SYSTEM_OWNER_GUID, 0, itemGuid, auctionId);
            seedRow(bidKey, CUSTODY_GOLD, ROLE_BID,
                    otherBidderGuid, 100, 0, auctionId);

            CharacterDatabase.DirectPExecute(
                "UPDATE `characters` SET `money`=1000 WHERE `guid`=%u",
                sellerGuid);
            WorldSession session(9501, std::shared_ptr<proto::IClientLink>(),
                                 std::shared_ptr<SessionMailbox>(), SEC_PLAYER,
                                 0, LOCALE_enUS);
            Player seller(&session);
            session.SetPlayer(&seller);
            seller._Create(sellerGuid, HIGHGUID_PLAYER);
            seller.SetMoney(1000);
            uint32 const cut = auction->GetAuctionCut();

            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            auction->PrepareCancelCustody(&seller, def, false, true,
                                          bidKey, cut);
            bool const committed = CharacterDatabase.CommitTransactionChecked();
            if (committed)
            {
                def.run();
            }
            if (!committed || auctionExists(auctionId) ||
                seller.GetMoney() != 1000 - cut ||
                characterMoney(sellerGuid) != 1000 - cut ||
                rowState(bidKey) != CST_TERMINAL_BACK ||
                rowState(markerKey) != CST_RESERVED || sellerRows(auctionId) != 0 ||
                mailCount(otherBidderGuid, AUCTION_CANCELLED_TO_BIDDER) != 1 ||
                mailMoney(otherBidderGuid, AUCTION_CANCELLED_TO_BIDDER) != 100 ||
                mailCount(sellerGuid, AUCTION_CANCELED) != 1 ||
                itemMailCount(sellerGuid, itemGuid) != 1)
            {
                printf("ahcustodyroute FAIL: bid-only cancel conservation\n");
                pass = false;
            }
            session.SetPlayer(NULL);
            delete auction;
        }
    }

    // A same-bidder raise can meet player-seller custody layered over a legacy
    // bid. Debit only the delta, then represent the full standing bid in one row.
    {
        clearFixtureMail();
        uint32 const auctionId = 995124;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, bidderGuid, 40, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: legacy same-bid fixture creation\n");
            pass = false;
        }
        else
        {
            std::string const itemKey = "item:995124";
            std::string const depKey = "dep:995124";
            seedRow(itemKey, CUSTODY_ITEM, ROLE_ITEM,
                    sellerGuid, 0, auction->itemGuidLow, auctionId);
            seedRow(depKey, CUSTODY_GOLD, ROLE_DEPOSIT,
                    sellerGuid, 20, 0, auctionId);

            CharacterDatabase.DirectPExecute(
                "UPDATE `characters` SET `money`=1000 WHERE `guid`=%u",
                bidderGuid);
            WorldSession session(9502, std::shared_ptr<proto::IClientLink>(),
                                 std::shared_ptr<SessionMailbox>(), SEC_PLAYER,
                                 0, LOCALE_enUS);
            Player bidder(&session);
            session.SetPlayer(&bidder);
            bidder._Create(bidderGuid, HIGHGUID_PLAYER);
            bidder.SetMoney(1000);

            CustodyRouteState const route = CustodyLedger::GetRouteState(auctionId);
            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            bool const active = auction->UpdateBidCustody(
                60, &bidder, def, route.usesPlayerSellerCustody,
                route.hasLiveBidCustody, "");
            bool const committed = CharacterDatabase.CommitTransactionChecked();
            if (committed)
            {
                def.run();
            }

            std::unique_ptr<QueryResult> bidRows(CharacterDatabase.PQuery(
                "SELECT COUNT(*),COALESCE(MAX(`owner_guid`),0),"
                "COALESCE(MAX(`amount`),0) FROM `custody_ledger` "
                "WHERE `auction_id`=%u AND `state`=0 AND `role`=%u",
                auctionId, uint32(ROLE_BID)));
            Field* bidFields = bidRows ? bidRows->Fetch() : NULL;
            if (!committed || !active || !route.usesPlayerSellerCustody ||
                route.hasLiveBidCustody || bidder.GetMoney() != 980 ||
                characterMoney(bidderGuid) != 980 || !auctionExists(auctionId) ||
                auction->bidder != bidderGuid || auction->bid != 60 ||
                !bidFields || bidFields[0].GetUInt32() != 1 ||
                bidFields[1].GetUInt32() != bidderGuid ||
                bidFields[2].GetUInt32() != 60 ||
                rowState(itemKey) != CST_RESERVED ||
                rowState(depKey) != CST_RESERVED)
            {
                printf("ahcustodyroute FAIL: legacy same-bid custody promotion\n");
                pass = false;
            }
            session.SetPlayer(NULL);
            delete auction;
        }
    }

    // Replacing a legacy standing bid under player-seller custody preserves the
    // old bidder's mail refund and starts custody only for the replacement bid.
    {
        clearFixtureMail();
        uint32 const auctionId = 995125;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, otherBidderGuid, 40, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: legacy outbid fixture creation\n");
            pass = false;
        }
        else
        {
            std::string const itemKey = "item:995125";
            std::string const depKey = "dep:995125";
            seedRow(itemKey, CUSTODY_ITEM, ROLE_ITEM,
                    sellerGuid, 0, auction->itemGuidLow, auctionId);
            seedRow(depKey, CUSTODY_GOLD, ROLE_DEPOSIT,
                    sellerGuid, 20, 0, auctionId);

            CharacterDatabase.DirectPExecute(
                "UPDATE `characters` SET `money`=1000 WHERE `guid`=%u",
                bidderGuid);
            WorldSession session(9502, std::shared_ptr<proto::IClientLink>(),
                                 std::shared_ptr<SessionMailbox>(), SEC_PLAYER,
                                 0, LOCALE_enUS);
            Player bidder(&session);
            session.SetPlayer(&bidder);
            bidder._Create(bidderGuid, HIGHGUID_PLAYER);
            bidder.SetMoney(1000);

            CustodyRouteState const route = CustodyLedger::GetRouteState(auctionId);
            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            bool const active = auction->UpdateBidCustody(
                60, &bidder, def, route.usesPlayerSellerCustody,
                route.hasLiveBidCustody, "");
            bool const committed = CharacterDatabase.CommitTransactionChecked();
            if (committed)
            {
                def.run();
            }

            std::unique_ptr<QueryResult> bidRows(CharacterDatabase.PQuery(
                "SELECT COUNT(*),COALESCE(MAX(`owner_guid`),0),"
                "COALESCE(MAX(`amount`),0) FROM `custody_ledger` "
                "WHERE `auction_id`=%u AND `state`=0 AND `role`=%u",
                auctionId, uint32(ROLE_BID)));
            Field* bidFields = bidRows ? bidRows->Fetch() : NULL;
            if (!committed || !active || !route.usesPlayerSellerCustody ||
                route.hasLiveBidCustody || bidder.GetMoney() != 940 ||
                characterMoney(bidderGuid) != 940 || !auctionExists(auctionId) ||
                !bidFields || bidFields[0].GetUInt32() != 1 ||
                bidFields[1].GetUInt32() != bidderGuid ||
                bidFields[2].GetUInt32() != 60 ||
                mailCount(otherBidderGuid, AUCTION_OUTBIDDED) != 1 ||
                mailMoney(otherBidderGuid, AUCTION_OUTBIDDED) != 40 ||
                rowState(itemKey) != CST_RESERVED ||
                rowState(depKey) != CST_RESERVED)
            {
                printf("ahcustodyroute FAIL: legacy bidder replacement\n");
                pass = false;
            }
            session.SetPlayer(NULL);
            delete auction;
        }
    }

    // Bid-only custody never selects the unsold custody path. Even malformed
    // no-bid book facts retain the legacy item return and leave bid provenance.
    {
        clearFixtureMail();
        uint32 const auctionId = 995126;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, 0, 0, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: bid-only unsold fixture creation\n");
            pass = false;
        }
        else
        {
            uint32 const itemGuid = auction->itemGuidLow;
            std::string const markerKey = "botlist:test:995126";
            std::string const bidKey = "bid:995126:1";
            seedRow(markerKey, CUSTODY_ITEM, ROLE_RESOLUTION,
                    AHBOT_SYSTEM_OWNER_GUID, 0, itemGuid, auctionId);
            seedRow(bidKey, CUSTODY_GOLD, ROLE_BID,
                    bidderGuid, 50, 0, auctionId);
            CustodyRouteState const route = CustodyLedger::GetRouteState(auctionId);
            auction->expireTime = static_cast<time_t>(-1);
            AuctionHouseObject* houseMap = sAuctionMgr.GetAuctionsMap(&house);
            houseMap->AddAuction(auction);
            houseMap->Update();
            CharacterDatabase.BeginTransaction();
            CharacterDatabase.CommitTransactionChecked();

            bool const mapPresent = houseMap->GetAuction(auctionId) != NULL;
            bool const dbPresent = auctionExists(auctionId);
            uint32 const bidState = rowState(bidKey);
            uint32 const markerState = rowState(markerKey);
            uint32 const sellerRowCount = sellerRows(auctionId);
            uint32 const expiredMailCount = mailCount(sellerGuid, AUCTION_EXPIRED);
            uint32 const expiredItemCount = itemMailCount(sellerGuid, itemGuid);
            if (route.usesPlayerSellerCustody || !route.hasLiveBidCustody ||
                mapPresent || dbPresent || bidState != CST_RESERVED ||
                markerState != CST_RESERVED || sellerRowCount != 0 ||
                expiredMailCount != 1 || expiredItemCount != 1)
            {
                printf("ahcustodyroute FAIL: bid-only unsold routing "
                       "route=%u/%u map=%u db=%u states=%u/%u seller=%u mail=%u/%u\n",
                       uint32(route.usesPlayerSellerCustody),
                       uint32(route.hasLiveBidCustody), uint32(mapPresent),
                       uint32(dbPresent), bidState, markerState,
                       sellerRowCount, expiredMailCount, expiredItemCount);
                pass = false;
            }
        }
    }

    // Existing full player seller+bid custody still terminalizes all three
    // value rows while delivering the same seller and winner mails.
    {
        clearFixtureMail();
        uint32 const auctionId = 995123;
        AuctionEntry* auction = createAuction(
            auctionId, sellerGuid, bidderGuid, 100, 0, 20);
        if (!auction)
        {
            printf("ahcustodyroute FAIL: full custody fixture creation\n");
            pass = false;
        }
        else
        {
            uint32 const itemGuid = auction->itemGuidLow;
            std::string const itemKey = "item:995123";
            std::string const depKey = "dep:995123";
            std::string const bidKey = "bid:995123:1";
            seedRow(itemKey, CUSTODY_ITEM, ROLE_ITEM,
                    sellerGuid, 0, itemGuid, auctionId);
            seedRow(depKey, CUSTODY_GOLD, ROLE_DEPOSIT,
                    sellerGuid, 20, 0, auctionId);
            seedRow(bidKey, CUSTODY_GOLD, ROLE_BID,
                    bidderGuid, 100, 0, auctionId);

            CustodyDeferred def;
            CharacterDatabase.BeginTransaction();
            auction->AuctionBidWinningCustody(NULL, def, true, true, bidKey);
            bool const committed = CharacterDatabase.CommitTransactionChecked();
            if (committed)
            {
                def.run();
            }
            if (!committed || auctionExists(auctionId) ||
                rowState(itemKey) != CST_TERMINAL_OK ||
                rowState(depKey) != CST_TERMINAL_BACK ||
                rowState(bidKey) != CST_TERMINAL_OK ||
                mailCount(sellerGuid, AUCTION_SUCCESSFUL) != 1 ||
                mailCount(bidderGuid, AUCTION_WON) != 1 ||
                itemMailCount(bidderGuid, itemGuid) != 1)
            {
                printf("ahcustodyroute FAIL: full custody winning regression\n");
                pass = false;
            }
        }
    }

    for (size_t i = 0; i < itemGuids.size(); ++i)
    {
        Item* liveItem = sAuctionMgr.GetAItem(itemGuids[i]);
        if (liveItem)
        {
            sAuctionMgr.RemoveAItem(itemGuids[i]);
            delete liveItem;
        }
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `mail_items` WHERE `item_guid`=%u", itemGuids[i]);
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `item_instance` WHERE `guid`=%u", itemGuids[i]);
    }
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver` IN (9501,9502,9503)");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id` BETWEEN 995100 AND 995199");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `auction` WHERE `id` BETWEEN 995100 AND 995199");
    CharacterDatabase.DirectExecute(
        "DELETE FROM `characters` WHERE `guid` IN (9501,9502,9503)");

    if (pass)
    {
        printf("ahcustodyroute OK\n");
        return 0;
    }
    return 2;
}

/// Regression for a real SP-2 smoke failure: the worker committed a cancel and
/// removed the auction, but mangosd missed the terminal result before restart.
/// The pending map is then empty, so repair must replay from ah_worker_journal
/// and mail the orphaned item_instance back to the seller, not merely
/// terminalize the custody rows.
static int RunAhRepairRecoveryTest()
{
    bool pass = true;
    CharacterDatabase.AllowAsyncTransactions();

    sObjectMgr.LoadItemPrototypes();

    uint32 const ownerGuid = 1u;
    uint32 const auctionId = 992001u;
    uint64 const uuid = 0xABCD001ull;
    uint64 const oldTime = static_cast<uint64>(time(NULL)) > 7200u
        ? static_cast<uint64>(time(NULL)) - 7200u : 1u;

    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id`=%u", auctionId);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `ah_worker_journal` WHERE `auction_id`=%u OR `uuid`=%llu",
        auctionId, static_cast<unsigned long long>(uuid));
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `auction` WHERE `id`=%u", auctionId);
    CharacterDatabase.DirectExecute(
        "DELETE FROM `mail` WHERE `receiver`=1 AND `subject` LIKE '2589:%'");
    CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid`=1");
    CharacterDatabase.DirectExecute(
        "INSERT INTO `characters` (`guid`,`account`,`name`,`money`) "
        "VALUES (1, 1, 'AhRepairRcv', 100000)");

    sObjectMgr.SetHighestGuids();

    uint32 itemId = 2589u;      // Linen Cloth
    if (!ObjectMgr::GetItemPrototype(itemId))
    {
        std::unique_ptr<QueryResult> r(WorldDatabase.PQuery(
            "SELECT `entry` FROM `item_template` "
            "WHERE `InventoryType`=0 AND `stackable`>1 ORDER BY `entry` LIMIT 1"));
        if (r)
        {
            itemId = r->Fetch()[0].GetUInt32();
        }
    }
    if (!ObjectMgr::GetItemPrototype(itemId))
    {
        printf("ahrepair FAIL: no usable item prototype\n");
        return 2;
    }

    Item* item = Item::CreateItem(itemId, 1);
    if (!item)
    {
        printf("ahrepair FAIL: CreateItem returned NULL\n");
        return 2;
    }
    item->SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER, ownerGuid));
    uint32 const itemGuid = item->GetGUIDLow();

    CharacterDatabase.BeginTransaction();
    item->SaveToDB();
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        delete item;
        printf("ahrepair FAIL: item seed commit failed\n");
        return 2;
    }
    delete item;

    PlayerMutationResult journalRes;
    journalRes.uuid = uuid;
    journalRes.op = uint8(IPC_PLAYER_CANCEL & 0xFFu);
    journalRes.status = uint8(MUT_PREPARED);
    journalRes.reason = 0;
    journalRes.facts = MutationFacts();
    journalRes.facts.auctionId = auctionId;
    journalRes.facts.houseId = 7;
    journalRes.facts.itemGuid = itemGuid;
    journalRes.facts.itemTemplate = itemId;
    journalRes.facts.randomPropertyId = 0;
    journalRes.facts.sellerGuid = ownerGuid;
    journalRes.facts.deposit = 32u;

    ByteBuffer bb;
    journalRes.Encode(bb);
    std::string const factsHex = TestHexEncode(bb);

    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute(
        "INSERT INTO `custody_ledger` "
        "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
        "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
        "VALUES ('dep:%u',0,0,0,%u,0,32,0,%u," UI64FMTD ",0),"
        "       ('item:%u',1,3,0,%u,0,0,%u,%u," UI64FMTD ",0)",
        auctionId, ownerGuid, auctionId, oldTime,
        auctionId, ownerGuid, itemGuid, auctionId, oldTime);
    CharacterDatabase.PExecute(
        "INSERT INTO `ah_worker_journal` "
        "(`uuid`,`auction_id`,`kind`,`state`,`facts`,`created_time`,`resolved_time`) "
        "VALUES (%llu,%u,%u,1,'%s'," UI64FMTD "," UI64FMTD ")",
        static_cast<unsigned long long>(uuid), auctionId,
        uint32(IPC_PLAYER_CANCEL & 0xFFu), factsHex.c_str(), oldTime, oldTime);
    if (!CharacterDatabase.CommitTransactionChecked())
    {
        printf("ahrepair FAIL: recovery seed commit failed\n");
        pass = false;
    }

    TestCliCapture repairCapture;
    CliHandler repairCli(0, SEC_ADMINISTRATOR, &repairCapture, &TestCliPrint);
    if (!repairCli.ParseCommands("ah repair apply"))
    {
        printf("ahrepair FAIL: committed cancel command was not parsed\n");
        pass = false;
    }
    if (CountCliChunks(repairCapture,
            "mode=apply confirmed=2 pending=0 sweep-owned=0 repaired=2 skipped=0 failed=0") != 1u)
    {
        printf("ahrepair FAIL: committed cancel command summary mismatch\n");
        pass = false;
    }
    if (CountCliChunks(repairCapture, "replayed committed cancel") != 1u)
    {
        printf("ahrepair FAIL: repeated auction rows replayed journal more than once\n");
        pass = false;
    }

    auto rowState = [](char const* key) -> uint32
    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT `state` FROM `custody_ledger` WHERE `idem_key`='%s'", key));
        return res ? res->Fetch()[0].GetUInt32() : 255u;
    };

    std::string const depKey = "dep:" + std::to_string(auctionId);
    std::string const itemKey = "item:" + std::to_string(auctionId);
    if (rowState(depKey.c_str()) != CST_TERMINAL_OK)
    {
        printf("ahrepair FAIL: deposit not TERMINAL_OK\n");
        pass = false;
    }
    if (rowState(itemKey.c_str()) != CST_TERMINAL_OK)
    {
        printf("ahrepair FAIL: item custody not TERMINAL_OK\n");
        pass = false;
    }

    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `mail_items` WHERE `receiver`=%u AND `item_guid`=%u",
            ownerGuid, itemGuid));
        if (!res || res->Fetch()[0].GetUInt64() != 1u)
        {
            printf("ahrepair FAIL: returned item mail missing\n");
            pass = false;
        }
    }

    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT `checked` FROM `mail` m "
            "JOIN `mail_items` mi ON mi.`mail_id`=m.`id` "
            "WHERE mi.`receiver`=%u AND mi.`item_guid`=%u",
            ownerGuid, itemGuid));
        if (!res || !(res->Fetch()[0].GetUInt32() & MAIL_CHECK_MASK_COPIED))
        {
            printf("ahrepair FAIL: returned item mail missing copied mask\n");
            pass = false;
        }
    }

    {
        std::unique_ptr<QueryResult> res(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `character_inventory` WHERE `item`=%u", itemGuid));
        if (res && res->Fetch()[0].GetUInt64() != 0u)
        {
            printf("ahrepair FAIL: item should not be placed directly in inventory\n");
            pass = false;
        }
    }

    // Generic apply and force-forfeit both preserve reserved bot provenance
    // markers and their materialized item instances.
    Item* protectedItem = Item::CreateItem(itemId, 1);
    if (!protectedItem)
    {
        printf("ahrepair FAIL: protected marker item creation failed\n");
        pass = false;
    }
    else
    {
        protectedItem->SetOwnerGuid(ObjectGuid(HIGHGUID_PLAYER,
                                               AHBOT_SYSTEM_OWNER_GUID));
        uint32 const protectedItemGuid = protectedItem->GetGUIDLow();
        CharacterDatabase.BeginTransaction();
        protectedItem->SaveToDB();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('botlist:test:repair:protected',1,4,0,%u,0,0,%u,992002,"
            UI64FMTD ",0)",
            AHBOT_SYSTEM_OWNER_GUID, protectedItemGuid, oldTime);
        bool const markerSeeded = CharacterDatabase.CommitTransactionChecked();
        delete protectedItem;

        TestCliCapture markerApplyCapture;
        CliHandler markerApplyCli(0, SEC_ADMINISTRATOR,
                                  &markerApplyCapture, &TestCliPrint);
        markerApplyCli.ParseCommands("ah repair apply");
        TestCliCapture markerForceCapture;
        CliHandler markerForceCli(0, SEC_ADMINISTRATOR,
                                  &markerForceCapture, &TestCliPrint);
        markerForceCli.ParseCommands(
            "ah repair force-forfeit botlist:test:repair:protected");

        CustodyRow protectedMarker;
        std::unique_ptr<QueryResult> protectedItemRow(CharacterDatabase.PQuery(
            "SELECT 1 FROM `item_instance` WHERE `guid`=%u", protectedItemGuid));
        if (!markerSeeded ||
            !CustodyLedger::Get("botlist:test:repair:protected", protectedMarker) ||
            protectedMarker.state != CST_RESERVED || !protectedItemRow ||
            CountCliChunks(markerApplyCapture,
                "mode=apply confirmed=0 pending=0 sweep-owned=1 repaired=0 skipped=1 failed=0") != 1u ||
            CountCliChunks(markerForceCapture, "reserved bot marker") != 1u)
        {
            printf("ahrepair FAIL: apply or force-forfeit mutated bot marker/item\n");
            pass = false;
        }

        CharacterDatabase.DirectPExecute(
            "DELETE FROM `custody_ledger` WHERE `idem_key`='botlist:test:repair:protected'");
        CharacterDatabase.DirectPExecute(
            "DELETE FROM `item_instance` WHERE `guid`=%u", protectedItemGuid);
    }

    CharacterDatabase.DirectPExecute(
        "DELETE FROM `mail_items` WHERE `item_guid`=%u", itemGuid);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `item_instance` WHERE `guid`=%u", itemGuid);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `mail` WHERE `receiver`=%u AND `subject` LIKE '%u:%%'",
        ownerGuid, itemId);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `auction_id`=%u", auctionId);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `ah_worker_journal` WHERE `auction_id`=%u OR `uuid`=%llu",
        auctionId, static_cast<unsigned long long>(uuid));
    CharacterDatabase.DirectExecute("DELETE FROM `characters` WHERE `guid`=1");

    if (pass)
    {
        printf("ahrepair OK\n");
        return 0;
    }
    return 2;
}

/// SP-2 Task 13 self-test for the bot-sell materialization leg. Drives
/// AuctionIntentExecutor::TestMaterializeSell directly -- the live path reaches
/// it through Apply() -> ApplySell(), but that re-validation chain needs a fully
/// loaded world (item store, AH DBC, bot config) unavailable under -t, so the
/// seam exercises the durable leg in isolation. Asserts the durable contract:
///   * status INTENT_OK with nonzero itemGuid + auctionId,
///   * an item_instance row is minted with owner_guid == the bot lister,
///   * a botlist:<uuid> custody row (kind ITEM, role RESOLUTION, RESERVED)
///     records BOTH ids,
///   * NO `auction` row is inserted (the worker owns the book),
///   * a redelivered uuid replays the SAME ids and mints no second item.
/// Then exercises SweepOrphanMaterializations: a stranded bot-owned mint (no
/// auction row) is reaped, while an item that reached the book and was delivered
/// to a buyer (owner changed) is preserved. Returns 0 on pass.
static int RunAhMaterializeTest()
{
    bool pass = true;
    CharacterDatabase.AllowAsyncTransactions();

    // -t does not load the world; Item::CreateItem needs item prototypes.
    sObjectMgr.LoadItemPrototypes();

    const uint32 botGuid   = 990113u;   // synthetic bot lister low-guid
    const uint32 buyerGuid = 990199u;   // synthetic "delivered to" owner
    const uint64 uuid      = 0xD13ull;  // Task 13 test intent uuid
    std::string const key  = "botlist:" + std::to_string(uuid);

    // Synthetic sweep fixtures (huge guids so they never collide with real data
    // and are not reused as freshly-minted ids after SetHighestGuids).
    const uint32 orphanItem = 99911301u;   // still bot-owned -> reaped
    const uint32 soldItem   = 99911302u;   // delivered (owner changed) -> kept
    const uint32 orphanAuc  = 99900001u;
    const uint32 soldAuc    = 99900002u;

    // Clean slate BEFORE SetHighestGuids so leftover synthetic rows do not
    // inflate the auction-id / item-guid generators.
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` IN "
        "('%s','botlist:test:orphan','botlist:test:sold')", key.c_str());
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `item_instance` WHERE `owner_guid` IN (%u,%u)",
        botGuid, buyerGuid);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `item_instance` WHERE `guid` IN (%u,%u)",
        orphanItem, soldItem);

    sObjectMgr.SetHighestGuids();   // GenerateItemLowGuid / GenerateAuctionID

    // A valid, simple stackable non-equip item to mint. Fall back to a DB probe
    // so the test does not hardcode a build-specific id.
    uint32 itemId = 2589u;          // Linen Cloth (universal 1.12 item)
    if (!ObjectMgr::GetItemPrototype(itemId))
    {
        std::unique_ptr<QueryResult> r(WorldDatabase.PQuery(
            "SELECT `entry` FROM `item_template` "
            "WHERE `InventoryType`=0 AND `stackable`>1 ORDER BY `entry` LIMIT 1"));
        if (r)
        {
            itemId = r->Fetch()[0].GetUInt32();
        }
    }
    if (!ObjectMgr::GetItemPrototype(itemId))
    {
        printf("ahmaterialize FAIL: no usable item prototype"
               " (item_template empty?)\n");
        return 2;
    }

    SellIntent si;
    si.uuid = uuid;
    si.botGuid = botGuid;
    si.house = 2;               // not re-validated by the durable leg
    si.itemId = itemId;
    si.stack = 1;
    si.bid = 100u;
    si.buyout = 1000u;
    si.durationHrs = 24u;

    // ---- Part 1: first materialization mints + records + replies ids ----
    IpcMessage out;
    sAuctionIntentExecutor.TestMaterializeSell(si, out, uint32(time(NULL)));

    IntentResult res;
    if (out.op != IPC_INTENT_RESULT || !res.Decode(out.body))
    {
        printf("ahmaterialize FAIL: no IPC_INTENT_RESULT reply\n");
        return 2;
    }
    if (res.status != INTENT_OK)
    {
        printf("ahmaterialize FAIL: status %u != INTENT_OK\n", res.status);
        pass = false;
    }
    if (res.itemGuid == 0u)
    {
        printf("ahmaterialize FAIL: itemGuid is zero\n");
        pass = false;
    }
    if (res.auctionId == 0u)
    {
        printf("ahmaterialize FAIL: auctionId is zero\n");
        pass = false;
    }

    uint32 const itemGuid  = res.itemGuid;
    uint32 const auctionId = res.auctionId;

    // item_instance row minted with owner == bot lister.
    {
        std::unique_ptr<QueryResult> q(CharacterDatabase.PQuery(
            "SELECT `owner_guid` FROM `item_instance` WHERE `guid`=%u", itemGuid));
        if (!q)
        {
            printf("ahmaterialize FAIL: item_instance row missing\n");
            pass = false;
        }
        else if (q->Fetch()[0].GetUInt32() != botGuid)
        {
            printf("ahmaterialize FAIL: item owner_guid != bot\n");
            pass = false;
        }
    }

    // Durable botlist row records both ids in the expected shape.
    {
        CustodyRow row;
        if (!CustodyLedger::Get(key, row))
        {
            printf("ahmaterialize FAIL: botlist row missing\n");
            pass = false;
        }
        else
        {
            if (row.itemGuid != itemGuid)
            {
                printf("ahmaterialize FAIL: botlist item_guid mismatch\n");
                pass = false;
            }
            if (row.auctionId != auctionId)
            {
                printf("ahmaterialize FAIL: botlist auction_id mismatch\n");
                pass = false;
            }
            if (row.kind != CUSTODY_ITEM)
            {
                printf("ahmaterialize FAIL: botlist kind != CUSTODY_ITEM\n");
                pass = false;
            }
            if (row.role != ROLE_RESOLUTION)
            {
                printf("ahmaterialize FAIL: botlist role != ROLE_RESOLUTION\n");
                pass = false;
            }
            if (row.state != CST_RESERVED)
            {
                printf("ahmaterialize FAIL: botlist state != CST_RESERVED\n");
                pass = false;
            }
        }
    }

    // No `auction` row -- the worker is the sole book writer.
    {
        std::unique_ptr<QueryResult> q(CharacterDatabase.PQuery(
            "SELECT 1 FROM `auction` WHERE `id`=%u", auctionId));
        if (q)
        {
            printf("ahmaterialize FAIL: unexpected auction row inserted\n");
            pass = false;
        }
    }

    // ---- Part 2: redelivered uuid replays the SAME ids, no second mint ----
    {
        IpcMessage out2;
        sAuctionIntentExecutor.TestMaterializeSell(si, out2, uint32(time(NULL)));
        IntentResult res2;
        if (out2.op != IPC_INTENT_RESULT || !res2.Decode(out2.body))
        {
            printf("ahmaterialize FAIL: replay produced no reply\n");
            pass = false;
        }
        else
        {
            if (res2.status != INTENT_OK)
            {
                printf("ahmaterialize FAIL: replay status != INTENT_OK\n");
                pass = false;
            }
            if (res2.itemGuid != itemGuid)
            {
                printf("ahmaterialize FAIL: replay itemGuid changed\n");
                pass = false;
            }
            if (res2.auctionId != auctionId)
            {
                printf("ahmaterialize FAIL: replay auctionId changed\n");
                pass = false;
            }
        }
        std::unique_ptr<QueryResult> q(CharacterDatabase.PQuery(
            "SELECT COUNT(*) FROM `item_instance` WHERE `owner_guid`=%u", botGuid));
        if (q && q->Fetch()[0].GetUInt64() != 1u)
        {
            printf("ahmaterialize FAIL: replay minted a second item\n");
            pass = false;
        }
    }

    // ---- Part 3: orphan sweep reaps strays but spares delivered items ----
    {
        // Seed synthetic item_instance + botlist rows AFTER SetHighestGuids so
        // they never influence the generators. Both are "old" (past the 300s
        // grace) and have no `auction` row. The orphan item is still bot-owned;
        // the "sold" item's owner is the buyer (its listing reached the book and
        // later resolved) -- it must survive the sweep.
        CharacterDatabase.DirectPExecute(
            "INSERT INTO `item_instance` (`guid`,`owner_guid`,`data`,`text`) "
            "VALUES (%u,%u,'0',''),(%u,%u,'0','')",
            orphanItem, botGuid, soldItem, buyerGuid);
        CharacterDatabase.BeginTransaction();
        CharacterDatabase.PExecute(
            "INSERT INTO `custody_ledger` "
            "(`idem_key`,`kind`,`role`,`state`,`owner_guid`,`beneficiary_guid`,"
            "`amount`,`item_guid`,`auction_id`,`created_time`,`resolved_time`) "
            "VALUES ('botlist:test:orphan',1,4,0,%u,0,0,%u,%u,100,0),"
            "       ('botlist:test:sold',1,4,0,%u,0,0,%u,%u,100,0)",
            botGuid, orphanItem, orphanAuc, botGuid, soldItem, soldAuc);
        if (!CharacterDatabase.CommitTransactionChecked())
        {
            printf("ahmaterialize FAIL: sweep seed commit\n");
            return 2;
        }

        sAuctionIntentExecutor.SweepOrphanMaterializations(uint32(time(NULL)));

        std::unique_ptr<QueryResult> qo(CharacterDatabase.PQuery(
            "SELECT 1 FROM `item_instance` WHERE `guid`=%u", orphanItem));
        if (qo)
        {
            printf("ahmaterialize FAIL: orphan item not swept\n");
            pass = false;
        }

        std::unique_ptr<QueryResult> qs(CharacterDatabase.PQuery(
            "SELECT 1 FROM `item_instance` WHERE `guid`=%u", soldItem));
        if (!qs)
        {
            printf("ahmaterialize FAIL: delivered item wrongly destroyed\n");
            pass = false;
        }

        CustodyRow tmp;
        if (CustodyLedger::Get("botlist:test:orphan", tmp))
        {
            printf("ahmaterialize FAIL: orphan botlist row not deleted\n");
            pass = false;
        }
        if (CustodyLedger::Get("botlist:test:sold", tmp))
        {
            printf("ahmaterialize FAIL: sold botlist row not deleted\n");
            pass = false;
        }
    }

    // Clean up (Part-1 minted item survives the sweep; drop it + fixtures).
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` IN "
        "('%s','botlist:test:orphan','botlist:test:sold')", key.c_str());
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `item_instance` WHERE `owner_guid` IN (%u,%u)",
        botGuid, buyerGuid);
    CharacterDatabase.DirectPExecute(
        "DELETE FROM `item_instance` WHERE `guid` IN (%u,%u)",
        orphanItem, soldItem);

    if (pass)
    {
        printf("ahmaterialize OK\n");
        return 0;
    }
    return 1;
}

int RunMangosdTest(std::string const& name)
{
    if (name == "noop")
    {
        printf("noop OK\n");
        return 0;
    }

    if (name == "commit")
    {
        return RunCommitTest();
    }

    if (name == "mail")
    {
        return RunMailTest();
    }

    if (name == "custody")
    {
        return RunCustodyTest();
    }

    if (name == "ahowner")
    {
        return RunAhOwnerTest();
    }

    if (name == "ahusabilityref")
    {
        return RunAhUsabilityRefTest();
    }

    if (name == "ahbrowsehelper")
    {
        return RunAhBrowseHelperTest();
    }

    if (name == "ahbrowsepending")
    {
        return RunAhBrowsePendingTest();
    }

    if (name == "ahmutpending")
    {
        return RunAhMutPendingTest();
    }

    if (name == "ahforwardreserve")
    {
        return RunAhForwardReserveTest();
    }

    if (name == "ahrelease")
    {
        return RunAhReleaseTest();
    }

    if (name == "ahmutresult")
    {
        return RunAhMutResultTest();
    }

    if (name == "ahresolve")
    {
        return RunAhResolveTest();
    }

    if (name == "ahrepair")
    {
        return RunAhRepairRecoveryTest();
    }

    if (name == "ahcustodyroute")
    {
        return RunAhCustodyRouteTest();
    }

    if (name == "ahmaterialize")
    {
        return RunAhMaterializeTest();
    }

    printf("%s FAIL: unknown test\n", name.c_str());
    return 2;
}
