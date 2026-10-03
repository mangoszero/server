# AH service module: known defects

Open defects in `src/modules/AhService/` (world side, IPC, worker), from the
static review of the auction module split (2026-10-03). Numbers are the
review's. #7 and #8 are listed here as far as they concern standing the
in-process bot down for the worker; the part of #7 that is the bot's alone is
in `src/modules/AuctionHouseBot/KNOWN_DEFECTS.md`. Line numbers are as of the
review and may drift.

All of them predate the module split: the service code was moved unchanged, and
where a defect runs through the core seam the core behaves as it did before
(`AuctionHouseObject::Update` returned on an unknown custody route and skipped a
mismatched row; `UpdateBid` dropped a generated bid on a custody failure). None
of it is in a server built without `BUILD_AH_SERVICE` or run with `Enable = 0`.

Two of the review's flags mean different things:

- `OwnsAuctionRows()` is `IsWriteAuthority()`, latched for the whole run.
- `TakesOverListings()` is `IsWorkerActive()`, which drops as soon as the child
  is unhealthy.

Several defects below are the gap between them.

## Value and items

### 1. Worker-initiated wins pay `effectiveBid` that mangosd never escrowed

- Severity: bug (most severe)
- Where: `world/AhServiceHandlers.cpp:1396` and `:2430` (`AhHandleResolveApply`, `RESOLVE_WON`)

The seller is paid `f.effectiveBid + f.deposit - cut` from the worker's facts.
Nothing requires `effectiveBid == curBid`, `effectiveBid <=` the reserved bid
row, or the intrinsic-value ceiling the advisory executor applies
(`ComputeBotValueCeiling`). The player-mutation finalize (`AhFinalizeBidOk`)
does require them; this path does not.

- Bot win (`curBidderGuid == 0`, no live bid row): `effectiveBid` is free. A
  child past the handshake sends `RESOLVE_WON` with any amount; mangosd mails it
  to the real seller and destroys the item.
- Real bidder: the bid row must match `curBid`, so the bidder paid that much;
  the mail still carries `effectiveBid`, and the difference is minted.

The child is documented as untrusted, `IPC_RESOLVE_APPLY` is on the reliable
lane, and the default secret is `changeme` (`WorkerSupervisor::Start` warns and
still binds `127.0.0.1`), so any local process that completes the handshake can
send the frame.

Fix direction: pay `min(effectiveBid, reserved bid or deposit-backed buyout)`
from the ledger row, apply the executor's ceiling before a bot-win mail, reject
the resolve when `effectiveBid` disagrees with the row.

### 13. A rolled-back custody transaction forgets unrelated item changes

- Severity: bug
- Where: `world/CustodyService.cpp:59`, `src/game/Object/PlayerSave.cpp:625`,
  `src/game/Object/ItemPersistence.cpp:258`, `world/AhServiceHandlers.cpp:595`

`ReserveGold` debits the player and calls `SaveInventoryAndGoldToDB` inside the
open character transaction. That saves the whole item-update queue: each queued
item becomes `ITEM_UNCHANGED` and the queue is cleared. If the checked commit
fails, the SQL rolls back but only the gold is restored (`AhForwardBid` line
599, the in-map custody bid at line 734). A just-looted `ITEM_NEW` item or an
`ITEM_CHANGED` stack is no longer dirty, the next save skips it, and after relog
it is gone. Needs a non-empty queue at reserve time.

Fix direction: save only the gold in these transactions, or snapshot and
restore each item's state and queue position when the commit fails.

### 14. A missing prior-bid row is consumed as a finished success

- Severity: bug
- Where: `world/AhServiceHandlers.cpp:1815` and `:1937`, `world/CustodyLedger.cpp:271`

`AhHandlePlayerMutationResult` takes the pending slot before finalize.
`AhFinalizeBidOk` requires a live `ROLE_BID` row when `priorBidderGuid != 0`;
`GetSingleLiveBidRow` returns false on a query error and on any count but one,
and the function then logs and returns true ("done"). Nothing is redriven, the
worker already committed and will not resend, the previous bidder is not mailed,
and the new reserve stays `RESERVED`. The cancel live-row check at line 1937
does the same. Listings that predate custody hit it every time
(`AhForwardBid` reserves without looking at a live bid row; the worker copies
`buyguid` from the auction row).

Fix direction: return false when the row is missing or the query failed; for a
real legacy bid, mail `priorBidAmount` from the auction row, then commit.

### 15. Process death drops in-flight player mutations, and boot does not replay them

- Severity: bug
- Where: `world/AhServiceHandlers.cpp:2973`, `world/AhServiceModule.cpp:70`,
  `world/CustodyReconciler.cpp:336`

`MutationPendingMap` and the redrive list are in memory.
`AhReconcileOnReconnect` returns at once when the map is empty; `Start` only
calls `InitializeRouting`. A crash after `MaybeCrash("post-reserve-pre-forward")`
leaves a committed debit and `RESERVED` row with no pending slot; after restart
nothing reconciles it, and the hourly scan only reports `ORPHAN_PLAYER`. A crash
after the worker committed a player buyout and before `AhFinalizeBidOk` loses
the refund and the item delivery the same way (`PrimeResolvingFromJournal`
replays `JRN_RESOLVING` only).

Fix direction: on startup walk reserved player rows against the journal:
finalize when committed, release and mail when absent, hold when the journal
query fails.

### 16. `RESOLVE_REPAIR_RETURN` acks a displaced bidder who was not refunded

- Severity: bug
- Where: `world/AhServiceHandlers.cpp:2529`

A bot bid that displaced a real bidder (`curBidderGuid == 0`,
`priorBidderGuid != 0`) refunds only when `GetSingleLiveBidRow` succeeds; on a
query error, zero rows or two rows it still writes `resolve:<uuid>` and returns
`RES_APPLIED`. The worker will not resend and the displaced player's copper
stays `RESERVED`. `RESOLVE_WON` treats the same miss as `RES_FAILED` (line 2463).

Fix direction: `RES_FAILED` when a prior bidder was declared and the row is
missing or ambiguous; write the applied-record only after the refund.

## Expiry and settlement

### 2. A failed custody probe latches "rows may exist" and freezes expiry

- Severity: bug
- Where: `world/CustodyLedger.cpp:115`, `world/AhServiceBook.cpp:791`,
  `src/game/Object/AuctionHouseMgr.cpp:677`

`Start` always calls `CustodyLedger::InitializeRouting`, custody on or off.

```cpp
s_mayHaveReservedRows = !result || result->Fetch()[0].GetUInt32() != 0u;
```

`Query()` returns null when the statement fails and when `custody_ledger` does
not exist, so the latch turns true; every later `GetRouteState` gets null again
and reports `known=false`. `AhExpireAuction` answers `AUCTION_EXPIRY_STOP`, the
core leaves the house, and the next tick stops on the same row: player auctions
in that house never expire. Fires with `Enable = 1` and custody off whenever
the SP-2 table was not migrated or the probe fails once at boot. (Before the
split this hit every server, since the service was in the core.)

Fix direction: a failed probe must not latch; with custody off a failed probe
reports a known empty route; `STOP` should mean "retry this auction", not
"abandon the house".

### 3. Write authority with no running worker disables expiry and the fallback bot

- Severity: bug
- Where: `world/AhServiceModule.cpp:162`, `src/game/WorldHandlers/World.cpp:1165`,
  `src/modules/AuctionHouseBot/AuctionHouseBotModule.cpp:54`

`OwnsAuctionRows()` stays true while the child is dead, so `World::Update`
skips `sAuctionMgr.Update()` for the run. `WorkerSupervisor::Start` returns
false when `AuctionHouseBot.CharacterName` does not resolve and does not retry;
the module deletes the supervisor. The bot's `Start` already skipped
`Initialize()`, so buyer and seller stay null, and `AhBlocksHello` keeps the
auction window shut. Listings, bids and deposits never settle.

Fix direction: `OwnsAuctionRows()` true only while a write-authority child is
up, or a failed `Start` clears write authority for the run and lets the core
expire and the bot post.

### 4. Generated bids are swallowed when the route probe fails

- Severity: bug
- Where: `world/AhServiceBook.cpp:878`, `src/game/Object/AuctionHouseMgr.cpp:1069`

`AhSettleGeneratedBid` returns true ("handled") with nothing applied when the
route is unknown (#2), the live row does not match, `BeginTransaction` fails, or
the checked commit fails. `UpdateBid` then skips the core bid; the in-process
buyer ignores the result, so the bid or buyout vanishes. The executor's
`AhPlaceGeneratedBid` reports `REASON_TRANSACTION` without `Remember`, which at
least lets the child retry.

Fix direction: return false unless this call applied the bid; keep the
fail-closed return only where the core path would double-spend a live custody
row.

### 5. A mismatched custody row quarantines an expired auction forever

- Severity: bug
- Where: `world/AhServiceBook.cpp:813`, `src/game/Object/AuctionHouseMgr.cpp:681`

A live bid row that does not match `auction.bidder` / `auction.bid` makes
`AhExpireAuction` log and return `AUCTION_EXPIRY_DONE`. The auction stays in the
map, past its expiry, and takes the same branch every tick; the item and the
bidder's gold never move and nothing queues `ah repair`. (`DONE` after a failed
commit is different: the next tick can succeed.)

Fix direction: report `DONE` only when the auction left the map; otherwise skip
the id and keep going, and queue a repair.

### 7. `.ahbot reload` builds agents that post into a book the core is not expiring

- Severity: bug
- Where: `src/modules/AuctionHouseBot/AuctionHouseBot.cpp:2696` (`ReloadAllConfig`
  -> `InitializeAgents`), `src/modules/AuctionHouseBot/AuctionHouseBotModule.cpp:73`

Under write authority the bot's `Start` skips `Initialize()`, so its agents stay
null. `.ahbot reload` builds them anyway, and the bot tick asks only
`TakesOverListings()`. When the worker turns unhealthy the rebuilt seller
inserts `auction` rows that the core does not expire (`OwnsAuctionRows()` is
still true) and the worker's book does not know.

Fix direction: refuse to create agents, and refuse to post, while
`OwnsAuctionRows()` is true.

### 8. The bot stands down on a 20-second sample, so one cycle double-posts

- Severity: bug
- Where: `src/modules/AuctionHouseBot/AuctionHouseBotModule.cpp:39` and `:73`

The bot reads `TakesOverListings()` only every 20 s (the same cadence as the
old `WUPDATE_AHBOT` timer), while worker sell intents are applied on every world
tick. A worker that turns healthy just after a bot tick still gets one full
`sAuctionBot.Update()` up to 20 s later, against the table the worker writes;
the reverse edge leaves up to 20 s with no bot at all.

Fix direction: sample `TakesOverListings()` on every world tick and skip
`sAuctionBot.Update()` in that tick while the worker is active.

### 11. Reload can turn custody off while write authority stays on

- Severity: bug
- Where: `world/AhService.cpp:80`

Startup forces write authority off unless custody is on and
`ah_worker_journal` exists. The reload branch keeps write authority from the
live settings but takes `custody` from the file, so `AH.Service.Custody = 0`
on reload leaves write authority without custody; expiry stays skipped and the
startup invariant is gone. `Enable` is read once at startup: reload does not
rebuild the module list.

Fix direction: repeat the startup check on reload, and refuse to clear custody
while write authority is latched.

### 17. Write-authority bot listings expire on a clock that can still be zero

- Severity: bug
- Where: `worker/MutationHandler.cpp:1470`, `worker/Main.cpp:4229` and `:4670`,
  `ipc/IpcReliable.h:33`

`m_gameTimeNow` starts at 0 and changes only on `IPC_GAMETIME`, which is not on
the reliable lane; the bot cadence does not wait for it. `OnBotSellResult` stores
`GameTime() + si.durationHrs * 3600`, and ignores `Rate.Auction.Time` (the
advisory `AddAuctionByGuid` applies it to `time(NULL)`). A dropped first
gametime frame stores an expiry a few hours after the epoch, and the next tick
with a real clock resolves it at once. Under write authority this is the only
expiry clock.

Fix direction: gametime on the reliable lane, no bot sells before the first
frame, and the absolute expiry `AddAuctionByGuid` would store.

### 20. An `INTENT_OK` whose auction id is already in the book strands the new item

- Severity: latent
- Where: `worker/MutationHandler.cpp:1436`, `world/AuctionIntentExecutor.cpp:674`

`MaterializeSell` writes `item_instance` and a `botlist:<uuid>` row, not
`auction`. If `m_book.Find(auctionId)` already hits, `OnBotSellResult` erases
the pending sell without `RetireIntentPending`; the journal stays
`INTENT_PENDING`. The orphan sweep skips ids that are live auctions, and a
restart replays and drops the same intent again.

Fix direction: treat the collision as a failed commit and keep the pending sell,
or retire it so the sweep sees the item.

## Bot brain (worker)

### 18. The ported seller's price range overflows `uint32`

- Severity: latent
- Where: `worker/BotBrain.cpp:691` (`SetPricesOfItem`)

The worker's copy of the in-process seller's pricing. `PriceRatio` is `uint32`,
so `buyp * stackcnt * ratio` is computed in 32 bits and wraps for an expensive
stack (a price near zero); `temp_buyp + randrange` can also pass 2^32 and be
converted to `uint32`, which is undefined. Under write authority that price is
persisted into `auction`. Fixed in the in-process bot; this copy is not.

Fix direction: as in `AuctionHouseBot.cpp`: 64-bit math, saturate to `uint32`
before `urand`.

### 19. In-flight bot sells are invisible to the seller, and a stale snapshot keeps selling

- Severity: latent
- Where: `worker/BotBrain.cpp:606`, `worker/MarketSnapshot.cpp:121` and `:230`

`SetStat` counts bot rows in the last SQL snapshot; `BotSellBegin` does not
insert `auction` until `OnBotSellResult` commits, and nothing subtracts
`m_pendingSells`, so a pending materialization is counted missing and listed
again. `Refresh` returns on a failed query without clearing the vectors, and
`Healthy()` stays true for five more failures after one success, so the seller
fills a house against a market it can no longer see.

Fix direction: subtract pending sells in `SetStat`; `Healthy()` false on the
first failed refresh, or no listing until a refresh replaces the vectors.

## IPC and supervisor

### 6. One pre-auth TCP connection stalls the IPC acceptor for good

- Severity: bug
- Where: `ipc/IpcThread.cpp:96`, `ipc/IpcServerHandler.cpp:65`, `ipc/IpcSocket.cpp:109`

The accept loop runs `ReceiveLoop` on the acceptor thread, the backlog is 1,
and there is no deadline for `IPC_HELLO`. A local process that connects to
`127.0.0.1:<AH.Service.Port>` and sends nothing holds the only accepted socket;
the real child is never accepted, every respawn fails, and `ServiceActive()`
stays false. Under write authority #3 follows. No secret is needed.

Fix direction: bound `WAIT_HELLO` and `WAIT_READY` to a few seconds, close and
accept again; run the receive loop off the accept thread.

### 9. Frames past the per-call budget are destroyed, not deferred

- Severity: bug
- Where: `ipc/WorkerSupervisor.cpp:608` and `:670`

In `DrainInboundProtocol` the per-call caps (`WS_DRAIN_APP_PER_CALL` 256,
`WS_DRAIN_BROWSE_PER_CALL` 128) only `break` the `switch`; the loop keeps
popping and every later frame of an exhausted class is counted in
`m_appDropped` and discarded. Browse results past 128 are lost and the player
gets "AH unavailable" after the TTL. `IPC_INTENT_BID` / `IPC_INTENT_BUYOUT`
share this lane, and `BotBrain` marks the auction checked when it emits
(`BotBrain.cpp:1182`), so a dropped bid is not retried for minutes.

Fix direction: on budget exhaustion stop popping or push the frame back; put
bid and buyout on the reliable lane or clear `lastChecked` until `INTENT_OK`.

### 10. Socket close races the world-thread send

- Severity: bug (data race, undefined behavior)
- Where: `ipc/IpcServerHandler.cpp:203` and `:361`, `ipc/IpcSocket.cpp:211` and `:256`

`SendFrame` holds `m_sendMtx` across `SendAll`, which reads `m_fd`; `OnClose`
(reactor thread) calls `m_sock.Close()` without it, writing `m_fd` and closing
the descriptor. A concurrent `send` can hit a descriptor already reused by the
next accept. Same pattern in `IpcClientHandler` on the worker side. (The comment
in `WorkerSupervisor.h` about a benign `m_childExited` race describes a second
world thread this process does not have.)

Fix direction: `OnClose` takes `m_sendMtx` before `Close`; `SendAll` uses a
descriptor copied under the lock; `shutdown` under the same lock.

### 12. In-doubt sweep wraps when the clock steps backward

- Severity: latent
- Where: `world/MutationPending.cpp:129`

`nowSec - sentSec <= ttlSec` is `uint32`; a backward clock step wraps it and
tombstones every pending mutation at once. The player gets
`AUCTION_ERR_DATABASE`; the reservation stays non-terminal, so no value is
minted.

Fix direction: signed difference, or ignore entries whose `sentSec` is in the future.

### 21. A child that exits is respawned on the next world tick

- Severity: bug
- Where: `ipc/WorkerSupervisor.cpp:442`

`m_nextRetryAt` moves forward only when `SpawnChild` itself fails; a child that
starts and then exits resets the backoff and leaves `m_nextRetryAt` in the past.
A worker that dies in `main` (bad config, empty pool) is re-spawned every tick.

Fix direction: arm `m_nextRetryAt = now + m_backoffSec` when the child is marked
dead; clear the backoff only after a healthy heartbeat.

### 22. An unhealthy gap keeps staged bot intents and applies them later

- Severity: bug
- Where: `ipc/WorkerSupervisor.cpp:646`, `world/AhServiceWorld.cpp:429`

A `healthy == 0` heartbeat clears `ServiceActive()` and lets the in-process bot
resume, but `ClearStagedFrames` runs only on death and spawn. Frames keep
moving into `m_pendingFrames`; the next healthy ack hands the backlog, including
`IPC_INTENT_SELL`, to `AhHandleInbound` on top of what the in-process bot posted.

Fix direction: drop staged bot intents on the transition to unhealthy; keep
player-mutation results on the reliable lane.

### 23. POSIX `waitpid` errors are treated as death, and the pid is then forgotten

- Severity: latent
- Where: `ipc/IpcProcess.cpp:176` and `:189`

`Running()` is true only when `waitpid(WNOHANG)` returns 0, so `-1`/`EINTR`
reads as "exited"; `Reap` clears `m_pid` without checking. A live child becomes
unkillable, a second worker is spawned, the acceptor is still inside the old
child's `ReceiveLoop`, and the orphan keeps its database sessions. Windows uses
`WaitForSingleObject` and is not affected.

Fix direction: only `r > 0` is death; `EINTR` is still running; never forget a
pid that was not reaped.

### 24. POSIX `fork` plus `setenv` can wedge the child before `exec`

- Severity: latent (deadlock)
- Where: `ipc/IpcProcess.cpp:149`

After `fork` the child calls `setenv` and builds a `std::vector`, both taking
the allocator lock another parent thread may have held at fork time; the child
hangs before `execv`, is killed after 60 s, and #21 repeats that at once.
Windows (`CreateProcessA`) is not affected.

Fix direction: build `argv` / `envp` before `fork` and call only `execve` in the
child, or use `posix_spawn`.

### 25. `IpcServer::Start` reports success when the listen thread has already failed

- Severity: bug
- Where: `ipc/IpcChannel.cpp:82`, `ipc/IpcThread.cpp:69`

`Start` returns true without waiting for `Listen`; a taken port only logs inside
the thread. The supervisor spawns the child against a dead acceptor and the
restart loop repeats until shutdown.

Fix direction: publish the listen result, return false on bind/listen failure,
do not spawn.

### 26. A failed database connect logs the password

- Severity: bug
- Where: `worker/ServiceDatabase.cpp:99` and `:164`

On `Initialize` failure the worker logs the whole
`host;port;user;password;database` string.

Fix direction: log host, port, user and database only.

## Architectural notes

- The README's trust model (the child never writes gold, items or auctions; a
  SELECT-only account makes that true) holds only without write authority.
  With `WriteAuthority = 1` the worker writes `auction` and mangosd pays value
  from worker facts, which the SELECT grant and the intent ceiling do not cover.
  #1 is the concrete hole.
- The reliable lane (`IpcLink::reliableInbound`, `m_pendingReliableFrames`) is
  unbounded so mutation frames are never dropped, and `IPC_INTENT_SELL` and
  `IPC_RESOLVE_APPLY` ride it: a child past the handshake can grow mangosd's
  memory without limit. An unknown opcode is buffered up to `IPC_MAX_FRAME`
  (1 MiB) by `RejectOversizeForOp` (`IpcServerHandler.cpp:173`) and then
  dropped without closing.
- On POSIX the child is started with `execv` and the listen socket has no
  `SOCK_CLOEXEC` (`IpcSocket.cpp:96`): the worker inherits mangosd's listen fd
  and MySQL session fds, and closing the listener in the parent does not unbind
  it while the child lives.
- The fallback between service and bot is two booleans sampled at different
  rates (#3, #7, #8, #22). One owner flag, updated when the child really turns
  healthy or dies, should drive "core expires", "bot may post" and "hello is
  open" together; restart needs a real backoff (#21) or the flag flaps.

## Priority order (from the review)

1. Bind `RESOLVE_WON` payout to the ledger and the value ceiling (#1); the same
   for a repair that skips the refund (#16).
2. Stop custody transactions from saving the item queue, and restore it if the
   commit fails (#13).
3. Do not consume a mutation whose prior-bid row is missing (#14); replay
   reserved rows after a restart (#15).
4. Stop a failed probe from freezing expiry (#2); do not leave
   `OwnsAuctionRows()` set while the write-authority child is down (#3, #7).
5. Handshake deadline on the acceptor (#6), close under the send lock (#10), no
   spawn when listen failed (#25).
6. Return "not handled" from the generated-bid and expiry hooks unless the
   auction was settled (#4, #5).
7. A real game-time expiry for write-authority listings (#17), and a backoff for
   a crashing child (#21) so an unhealthy gap cannot replay staged sells (#22).
