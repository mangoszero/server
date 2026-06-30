# AH Custody Crash-Injection Procedure

This procedure verifies the default-off player auction custody seams against the
spec Section 8 crash matrix. Run it only on disposable DB clones or a dedicated test
realm. Never set `AH.Service.CustodyCrashAt` on a live realm.

## Harness Wiring

The crash harness is wired in this branch. The expected audit command is:

```sh
git --no-pager grep -n "CrashPhase\|CustodyCrashAt\|MaybeCrash\|_exit(3)"
```

The relevant hits are:

```text
src/game/AuctionHouseBot/CustodyService.cpp:344:std::string CustodyService::CrashPhase()
src/game/AuctionHouseBot/CustodyService.cpp:346:    return sConfig.GetStringDefault("AH.Service.CustodyCrashAt", "");
src/game/AuctionHouseBot/CustodyService.cpp:349:void CustodyService::MaybeCrash(std::string const& phase)
src/game/AuctionHouseBot/CustodyService.cpp:353:        sLog.outError("custody crash-injection: _exit(3) at phase '%s' (TEST ONLY)", phase.c_str());
src/game/AuctionHouseBot/CustodyService.cpp:355:        _exit(3);
src/game/WorldHandlers/AuctionHouseHandler.cpp:577:        CustodyService::MaybeCrash("pre-commit");
src/game/WorldHandlers/AuctionHouseHandler.cpp:645:        CustodyService::MaybeCrash("pre-deferred");
src/game/WorldHandlers/AuctionHouseHandler.cpp:823:        CustodyService::MaybeCrash("pre-commit");
src/game/WorldHandlers/AuctionHouseHandler.cpp:826:            CustodyService::MaybeCrash("pre-deferred");
src/game/WorldHandlers/AuctionHouseHandler.cpp:1031:        CustodyService::MaybeCrash("pre-commit");
src/game/WorldHandlers/AuctionHouseHandler.cpp:1034:            CustodyService::MaybeCrash("pre-deferred");
src/game/Object/AuctionHouseMgr.cpp:1042:                    CustodyService::MaybeCrash("pre-commit");
src/game/Object/AuctionHouseMgr.cpp:1045:                        CustodyService::MaybeCrash("pre-deferred");
src/game/Object/AuctionHouseMgr.cpp:1077:                    CustodyService::MaybeCrash("pre-commit");
src/game/Object/AuctionHouseMgr.cpp:1080:                        CustodyService::MaybeCrash("pre-deferred");
src/mangosd/mangosd.conf.dist.in:1838:AH.Service.CustodyCrashAt = ""
```

`pre-commit` runs after the last custody DB write and immediately before
`CommitTransactionChecked()`. `pre-deferred` runs only after a successful checked
commit and immediately before `def.run()`. Matching either phase logs, flushes
stdout, then calls `_exit(3)`.

| Seam | Entry point | `pre-commit` | `pre-deferred` |
| --- | --- | --- | --- |
| S1 create | `HandleAuctionSellItem` custody branch | before the inverted checked commit | on the commit-success path before the normal command-result fall-through |
| S2/S3 bid/buyout | `HandleAuctionPlaceBid` custody branch | before checked commit | before `def.run()` |
| S5 cancel | `HandleAuctionRemoveItem` custody branch | before checked commit | before `def.run()` |
| S4 win | `AuctionHouseObject::Update` bid-winner custody branch | before checked commit | before `def.run()` |
| S6 expire | `AuctionHouseObject::Update` no-bidder custody branch | before checked commit | before `def.run()` |

## Required Setup

1. Work on cloned Character/World DBs, not production.
2. Start from a seeded, quiesced fixture: named test characters, fixed items,
   fixed auction ids where practical, AH bot disabled unless the matrix row
   explicitly needs a bot case, no other player logins, and a pinned or recorded
   clock window.
3. Enable packet logging with `WorldLogFile` and keep one log per run.
4. Set `AH.Service.Custody = 1`.
5. Ensure the auction under test is custody-managed when the row requires it:
   it must have `custody_ledger` rows, because the live gate is
   `AH.Service.Custody && CustodyLedger::HasRows(auction_id)`.
6. Record baseline counts before the seam:

```sql
SELECT MAX(id) AS max_mail_id, COUNT(*) AS mail_rows FROM mail;
SELECT COUNT(*) AS auction_rows FROM auction;
SELECT COUNT(*) AS live_custody_rows
  FROM custody_ledger WHERE auction_id = <auction_id> AND state = 0;
```

## Per-Phase Procedure

Run each matrix row twice, once with each crash phase.

1. Set `AH.Service.CustodyCrashAt = "pre-commit"` or
   `AH.Service.CustodyCrashAt = "pre-deferred"` in the test `mangosd.conf`.
2. Start the test realm and drive exactly one seam action.
3. Confirm the process exits with code `3` and the log contains
   `custody crash-injection: _exit(3) at phase '<phase>'`.
4. Reset `AH.Service.CustodyCrashAt = ""`, keep `AH.Service.Custody = 1`, and
   reboot the test realm.
5. Assert the recovery state:
   - `pre-commit`: the transaction did not commit and the pre-seam DB state is
     unchanged -- for S1 (create) NO auction row exists (it was never inserted);
     for S2-S6 the existing auction row is unchanged. There are zero new `mail`
     rows after the recorded `MAX(mail.id)`, and `WorldLogFile` has no phantom
     auction notification/command-result packets from the killed seam.
   - `pre-deferred`: the transaction committed, deferred live effects did not
     run before process death, and reboot/re-resolution reaches the same final
     player-visible projection as the no-crash custody run.
6. Let the seam re-resolve with crash injection off.
7. Run `src/ah-service/tools/custody_diff.sql` against the gate-off and gate-on
   clones, and compare `WorldLogFile` opcode order for:
   `SMSG_AUCTION_OWNER_NOTIFICATION`, `SMSG_AUCTION_BIDDER_NOTIFICATION`,
   `SMSG_AUCTION_REMOVED_NOTIFICATION`, and `SMSG_AUCTION_COMMAND_RESULT`.

## Forced Rollback Case

Also test the non-crash false-return path from `CommitTransactionChecked()`:

1. Force a statement failure inside the open custody transaction, for example by
   injecting a duplicate unique `custody_ledger.idem_key` in a disposable test
   build or by adapting the existing `-t custody` duplicate-key case.
2. Drive the seam without `AH.Service.CustodyCrashAt`.
3. Assert `CommitTransactionChecked()` returns false to the seam.
4. Assert no deferred effects ran: no new mail, no new live auction packets, no
   success-only item disposal, and any synchronous in-memory debit or auction
   mutation was restored before the DB-error result.
5. Re-run the seam normally and confirm it resolves identically.

The stock selftest covers the shared checked-commit rollback primitive:

```sh
cd E:/Mangos/WIP/Zero/AH_SubProcess/server_install
E:/Mangos/WIP/Zero/AH_SubProcess/server_build/src/mangosd/RelWithDebInfo/mangosd.exe -c mangosd.conf -t custody
```

That is necessary but not sufficient; the seam-level forced rollback must still
be exercised against online players or a disposable runtime harness.

## Spec Section 8 Matrix

Each row must pass the gate-off/gate-on differential, both crash phases, packet
order comparison, and the no-phantom-mail/no-phantom-packet assertions above.

| Case | Fixture | Expected player-visible result |
| --- | --- | --- |
| Same-bidder raise | Existing custody auction where bidder raises own bid | Debits only `newbid - oldbid`; no refund mail; auction bid becomes `newbid`. |
| Full-price reject | Same bidder has `wallet >= delta && wallet < newbid` | Rejected before DB write; no ledger, gold, mail, auction, or packet drift. |
| Same-bidder top-up to buyout | Same bidder raises to buyout | Full-price guard still applies; final win projection matches legacy. |
| Buyout with prior bidder | A real prior bidder exists, then another player buys out | Prior bidder receives old-bid refund mail; buyer pays buyout; seller/winner mail and auction removal match legacy order. |
| Player outbids bot-bid auction | `bid > 0`, `bidder == 0` | New bidder pays full bid; no refund mail to bidder zero. |
| Player cancels bot-bid auction | Owner cancels auction with `bid > 0`, `bidder == 0` | Owner pays cut; no bidder refund mail; item return and command result match legacy. |
| Player buys bot-created auction | Auction has no custody rows | Per-auction gate routes legacy path; custody ledger is not consulted. |
| Bot wins custody auction | Custody auction resolves with `bidder == 0` | Seller is paid, auction is removed, item is destroyed as legacy would do. |
| Cancel cannot afford cut | Owner lacks auction cut on bid auction | Guard rejects before DB write; no mail, item, ledger, or auction mutation. |
| Offline winner/owner destroy | Winner or owner account lookup fails/offline branch is taken | Item/money cleanup follows the legacy durable rows; no live pointer dependency. |
| Live custody auction missing a row | Delete or corrupt a required non-terminal custody row | Reconcile flags drift; seam fails closed instead of minting or losing value. |
| Concurrent observer | Second client lists/searches or uses `ah console` mid-resolution | Observer sees a coherent pre- or post-resolution state; no duplicate/phantom packet order. |

## Promotion Gate

Do not flip `AH.Service.Custody` default-on until all of these are green:

- `custody_diff.sql` reports zero player-visible diffs for the full matrix.
- Crash injection passes both phases for every custody seam above.
- Forced checked-commit rollback produces no deferred effects and restores live
  state.
- Concurrent observer checks are green.
- A live soak with the feature still default-off shows zero custody drift and no
  player-visible divergence.
