# SP-2 Write-Authority A/B Differential Procedure

This runbook is the deterministic substitute for a multi-realm soak (spec v3 sec 9).
It proves the out-of-process **write-authority** path reaches the *same
player-visible end-state and the same per-session packet sequence* as the legacy
in-process path, for a fixed serialized workload. Run it only on disposable DB
clones or a dedicated test realm; never against production.

The acceptance bar (spec sec 9, I12) is deliberately NOT "byte-identical at every
instant" -- the async worker boundary makes that unattainable. The two guarantees
are: **identical end-state projection** and **identical per-session packet
sequence**, with the scoped in-flight transient window of section 12 documented
below.

## What the two runs are

| Run | `AH.Service.WriteAuthority` | `AH.Service.Custody` | Book writer |
| --- | --- | --- | --- |
| A (legacy) | `0` | `1` | mangosd (in-process) |
| B (write-authority) | `1` | `1` | the out-of-process worker |

Both runs read and write the same shared `auction` table schema; only the single
writer swaps. `Custody = 1` in BOTH runs so the escrow ledger is populated
identically and the diff is apples-to-apples (enabling B with `Custody = 0` is
unsupported -- see `doc/AuctionHouseBot.md`).

## Determinism preconditions (must hold for BOTH runs)

1. **Bot OFF.** `AH.Service.Enabled = 0` for the zero-diff gate. Bot correctness is
   covered separately by the section 10 invariant checks and the
   `.ai_tools/ah_custody_harness` pattern; a live bot injects non-deterministic
   listings that would swamp the diff.
2. **Deterministic auction-ID allocation.** mangosd is the sole auction-ID
   allocator in both modes (decision 8), so `GenerateAuctionID` advances in the
   same order for the same serialized workload. Seed both clones from the SAME
   `character.auctioncounter` / item-guid high-water mark so IDs line up 1:1.
3. **Strictly serialized workload.** One action at a time, no overlap; drain the
   worker (MutationPending empty + no RESOLVING / CANCEL_PREPARED / INTENT_PENDING
   journal rows + no in-flight materialization) between actions in run B.
4. **Two fixed player sessions**, named characters, fixed starting gold, fixed
   items, no other logins.
5. **Pinned clock window** (or a recorded one) so expiry fires at the same logical
   point in both runs.
6. **Packet logging on**: set `WorldLogFile` and keep one log per session per run.

## The fixed serialized workload

Drive exactly this sequence, identically, against run A then run B (two named
sellers/bidders S and B1/B2):

1. **list** -- S lists item X (start bid, buyout, deposit auto-computed).
2. **list** -- S lists item Y (buyout only).
3. **bid** -- B1 bids the minimum on X.
4. **bid (raise)** -- B1 raises its own bid on X (same-bidder raise leg).
5. **bid (outbid)** -- B2 outbids B1 on X (prior-bidder refund leg).
6. **buyout** -- B1 buys out Y at buyout price (buyout-win leg).
7. **buyout-as-bid** -- B2 places a below-buyout "buyout" on X (commits as a normal
   bid, row stays live -- spec 4.1).
8. **cancel** -- S cancels X (nonzero-cut cancel leg; S pays the auction cut, B2 is
   refunded). **NOTE:** this is the leg the `-t` harness cannot cover; it MUST be
   observed here end-to-end (see `doc/AuctionHouseBot.md` OPERATOR WARNING 3).
9. **expire** -- advance the clock so any remaining live listing hits the
   expiry/win tick (no-bid expiry: deposit forfeit + item back to seller).

Extended rows to add once the base 9 are clean (spec sec 9): 50-cap rejection,
cancel-too-poor ABORT, buyout-over-bid cap, Eluna OnAdd/OnRemove parity, and an
IPC-version-mismatch hard-fail.

## Capturing and diffing end-state

After each run reaches quiescence (workload complete + worker drained + all mail
delivered), snapshot the three authoritative tables and the packet logs.

```sql
-- End-state projection: run against clone A and clone B, then diff.
SELECT id, houseid, itemguid, item_template, itemowner, buyoutprice,
       startbid, lastbid, buyguid, deposit, time
  FROM auction ORDER BY id;

SELECT guid, itemEntry, owner_guid, count
  FROM item_instance
 WHERE guid IN (/* the workload's item guids */) ORDER BY guid;

-- Custody ledger: terminal state + role, NOT the volatile row id / timestamps.
SELECT idem_key, kind, role, state, owner_guid, beneficiary_guid, amount,
       item_guid, auction_id
  FROM custody_ledger ORDER BY auction_id, idem_key;

-- Mail end-state (money + items delivered): compare counts + payloads by subject.
SELECT receiver, sender, subject, money, has_items
  FROM mail ORDER BY receiver, id;
```

For the **packet sequence**, extract each session's ordered opcode stream from
`WorldLogFile` and diff the ORDER of these opcodes per session:

- `SMSG_AUCTION_COMMAND_RESULT`
- `SMSG_AUCTION_OWNER_NOTIFICATION`
- `SMSG_AUCTION_BIDDER_NOTIFICATION`
- `SMSG_AUCTION_REMOVED_NOTIFICATION`

## What MUST be zero vs what is EXPECTED to differ

### Must-be-zero (any diff here fails the gate)

- **`auction`** end-state: identical surviving rows (same ids, owners, bids,
  buyers, deposits) after the workload. The book projection is the core guarantee.
- **`item_instance`** end-state: every workload item is owned by the same final
  holder (winner, or seller on expiry/cancel) with the same stack count. No
  duplicated or vanished items.
- **`custody_ledger`** terminal projection: every `idem_key` reaches the same
  terminal `state` (TERMINAL_OK / TERMINAL_BACK) with the same `amount`, `role`,
  and `owner_guid`. (The row `id` and timestamps are volatile -- exclude them.)
- **Mail end-state**: same receivers get the same money + item payloads (winner
  item, seller proceeds, outbid/cancel refunds). No lost or duplicated gold/items.
- **Per-session packet ORDER**: the relative order of the four AUCTION SMSGs above,
  within each session, is identical.

### Expected divergences (documented, accepted -- spec section 12)

Items 1-5 are the ONLY differences allowed in the base workload; each is invisible
in the must-be-zero end-state and visible only to a concurrent observer or in
absolute timing. Item 6 is a KNOWN FUNCTIONAL divergence (it DOES change end-state)
that is deferred to SP-3 -- the base workload above deliberately avoids it, and a
run that exercises it must account for it by hand:

1. **Rejected/in-doubt debit-then-recredit.** A rejected bid/buyout in run B
   debits then re-credits gold the legacy path never touched. Zero net in
   end-state; visible only to a concurrent observer mid-flight.
2. **Result latency.** `SMSG_AUCTION_COMMAND_RESULT` arrives a few ticks later in
   run B than the synchronous legacy reply. The *order* is preserved (must-be-zero
   above); only absolute arrival time differs.
3. **Sell transient.** In run B the item leaves the seller's bags at reserve and
   the listing appears at worker-commit; an owner-list browse in the gap shows
   neither. A momentary window only; end-state identical.
4. **In-doubt UX.** A timed-out mutation reports `AUCTION_ERR_DATABASE` and may
   still complete afterwards (mail arrives later). If it completes, end-state
   matches; the transient error result is the accepted divergence.
5. **[v3] Lock-loss reply timing + double result.** An op that loses to an
   in-flight prepare/resolve lock (e.g. bid vs in-flight cancel) gets its
   legacy-style rejection at reply latency rather than instantly, and a
   reject-then-ABORT cancel sequence can emit two command results where legacy
   emitted one. Accepted.
6. **[fixed during live smoke] Current high bidder's Buyout routes through
   `IPC_PLAYER_BUYOUT`.** The live SP-2 smoke found that a current high bidder
   clicking Buyout was being forwarded as `IPC_PLAYER_BID`, which the worker
   correctly rejected as a bid at/over buyout. The fixed classifier now routes
   same-bidder top-ups as `IPC_PLAYER_BUYOUT` while keeping the delta reserve:
   below-buyout 0x42 remains a normal bid, and at/over-buyout 0x42 becomes the
   immediate win. The mandatory pre-enable live smoke still exercises "current
   high bidder clicks Buyout" to prove the fixed path removes the auction and
   settles seller/winner/prior-bid custody.

Server-side-only effects that are NOT player-visible (e.g. GM-log lines that the
worker emits process-side rather than in mangosd, and Eluna `OnAdd`/`OnRemove`
hook *timing* -- the hooks still fire, at the finalize position from worker facts)
are out of the player-visible bar and are not diffed here.

## Pass criteria

The differential passes when, for the full workload:

- all four must-be-zero categories diff to empty between clone A and clone B, and
- every observed difference is attributable to one of the six documented
  divergences above (divergence 6 only if the run exercised a current-high-bidder
  buyout, which the base workload avoids).

Record the diffs (or their absence) per workload row. This runbook plus the
`doc/AuctionHouseBot.md` crash-injection procedure together constitute the
deterministic acceptance gate for enabling `WriteAuthority` on a solo realm.
