#include "MangosdTest.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include "Chat.h"
#include "Mail.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "AuctionHouseMgr.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "AuctionHouseBot/CustodyDeferred.h"
#include "AuctionHouseBot/CustodyLedger.h"
#include "AuctionHouseBot/CustodyService.h"
#include <cstdio>
#include <ctime>
#include <memory>

static void TestCliPrint(void* /*arg*/, char const* /*text*/)
{
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
    // residue, this third BeginTransaction() would trip MANGOS_ASSERT(!m_pTrans)
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

    if (pass)
    {
        printf("mail OK\n");
        return 0;
    }

    return 2;
}

/// CRUD round-trip test for CustodyLedger: Insert, Get, HasRows, SetState,
/// LoadNonTerminal, DeleteTerminalOlderThan.  Returns 0 on pass.
static int RunCustodyTest()
{
    bool pass = true;

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

    // HasRows(999) must be true; HasRows(424242) must be false.
    if (!CustodyLedger::HasRows(999))
    {
        printf("custody FAIL: step 2 HasRows(999) returned false\n");
        pass = false;
    }
    if (CustodyLedger::HasRows(424242))
    {
        printf("custody FAIL: step 2 HasRows(424242) returned true (unexpected)\n");
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

    // ================================================================ reconcile
    // Task 13: ReconcileScan flags custody drift and DeleteTerminalOlderThan
    // prunes only old terminal rows.
    static AuctionHouseEntry testHouse = { 7, 0, 0, 0 };
    AuctionHouseObject* testAuctions = sAuctionMgr.GetAuctionsMap(AUCTION_HOUSE_NEUTRAL);
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
    testAuctions->RemoveAuction(liveAuctionId);
    testAuctions->RemoveAuction(missingItemAuctionId);
    testAuctions->RemoveAuction(duplicateBidAuctionId);

    AuctionEntry* liveAuction = new AuctionEntry;
    liveAuction->Id = liveAuctionId;
    liveAuction->itemGuidLow = 880002;
    liveAuction->itemTemplate = 25;
    liveAuction->itemCount = 1;
    liveAuction->itemRandomPropertyId = 0;
    liveAuction->owner = 1001;
    liveAuction->startbid = 10;
    liveAuction->bid = 0;
    liveAuction->buyout = 0;
    liveAuction->expireTime = time(NULL) + HOUR;
    liveAuction->bidder = 0;
    liveAuction->deposit = 5;
    liveAuction->auctionHouseEntry = &testHouse;
    testAuctions->AddAuction(liveAuction);

    AuctionEntry* missingItemAuction = new AuctionEntry;
    missingItemAuction->Id = missingItemAuctionId;
    missingItemAuction->itemGuidLow = 880003;
    missingItemAuction->itemTemplate = 25;
    missingItemAuction->itemCount = 1;
    missingItemAuction->itemRandomPropertyId = 0;
    missingItemAuction->owner = 1002;
    missingItemAuction->startbid = 10;
    missingItemAuction->bid = 0;
    missingItemAuction->buyout = 0;
    missingItemAuction->expireTime = time(NULL) + HOUR;
    missingItemAuction->bidder = 0;
    missingItemAuction->deposit = 5;
    missingItemAuction->auctionHouseEntry = &testHouse;
    testAuctions->AddAuction(missingItemAuction);

    AuctionEntry* duplicateBidAuction = new AuctionEntry;
    duplicateBidAuction->Id = duplicateBidAuctionId;
    duplicateBidAuction->itemGuidLow = 880006;
    duplicateBidAuction->itemTemplate = 25;
    duplicateBidAuction->itemCount = 1;
    duplicateBidAuction->itemRandomPropertyId = 0;
    duplicateBidAuction->owner = 1006;
    duplicateBidAuction->startbid = 10;
    duplicateBidAuction->bid = 77;
    duplicateBidAuction->buyout = 0;
    duplicateBidAuction->expireTime = time(NULL) + HOUR;
    duplicateBidAuction->bidder = 2006;
    duplicateBidAuction->deposit = 5;
    duplicateBidAuction->auctionHouseEntry = &testHouse;
    testAuctions->AddAuction(duplicateBidAuction);

    CharacterDatabase.BeginTransaction();
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
        std::vector<CustodyRow> drift;
        CustodyService::ReconcileScan(true, drift);
        bool sawOrphan = false;
        bool sawCleanLive = false;
        bool sawMissingItem = false;
        bool sawDuplicateBid1 = false;
        bool sawDuplicateBid2 = false;
        for (size_t i = 0; i < drift.size(); ++i)
        {
            if (drift[i].idemKey == "test:recon:orphan")
            {
                sawOrphan = true;
            }
            if (drift[i].auctionId == liveAuctionId)
            {
                sawCleanLive = true;
            }
            if (drift[i].idemKey == "item:970003")
            {
                sawMissingItem = true;
            }
            if (drift[i].idemKey == "bid:970006:1" && drift[i].id != 0)
            {
                sawDuplicateBid1 = true;
            }
            if (drift[i].idemKey == "bid:970006:2" && drift[i].id != 0)
            {
                sawDuplicateBid2 = true;
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
        if (!sawDuplicateBid1 || !sawDuplicateBid2)
        {
            printf("custody FAIL: reconcile did not surface duplicate live bid rows\n");
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

    testAuctions->RemoveAuction(liveAuctionId);
    testAuctions->RemoveAuction(missingItemAuctionId);
    testAuctions->RemoveAuction(duplicateBidAuctionId);
    delete liveAuction;
    delete missingItemAuction;
    delete duplicateBidAuction;
    CharacterDatabase.BeginTransaction();
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

    printf("ahowner OK\n");
    return 0;
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

    printf("%s FAIL: unknown test\n", name.c_str());
    return 2;
}
