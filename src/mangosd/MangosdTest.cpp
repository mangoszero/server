#include "MangosdTest.h"
#include "Log.h"
#include "Database/DatabaseEnv.h"
#include <cstdio>
#include <memory>

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

    // Clean up.
    CharacterDatabase.DirectExecute(
        "DELETE FROM `custody_ledger` WHERE `idem_key` LIKE 'test:commit%'");

    if (pass)
    {
        printf("commit OK\n");
        return 0;
    }

    return 2;
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

    printf("%s FAIL: unknown test\n", name.c_str());
    return 2;
}
