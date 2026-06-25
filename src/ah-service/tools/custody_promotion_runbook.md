# AH Custody — In-Game Promotion-Gate Runbook

The offline gates (build, `-t custody`, multi-model code review) are all green. This
runbook is the **live** promotion gate from spec Section 8: do **not** flip
`AH.Service.Custody` default-on until every step here passes on a quiesced test realm.

Concrete values below are for this deployment (realm 0): world DB `mangos0`, character
DB `character0`, DB user `mangos`/`mangos`, server build commit `c08a6b77`. Adjust for
another realm.

> **Run on a TEST realm, never live-with-players.** A quiesced realm 0 (no other logins,
> AH bot off) is fine. Confirm the realm is running the **custody build (`c08a6b77`)** —
> the deployed `mangosd.conf` currently lacks the custody keys, which means the live
> binary may predate this work; verify before trusting any result.

---

## 1. One-time prerequisites

### 1a. Build / deploy
- The test realm must run server commit `c08a6b77` (PR-A…G complete). Verify boot, then:
  ```
  ( cd "E:/Mangos/WIP/Zero/AH_SubProcess/server_install" && \
    "E:/Mangos/WIP/Zero/AH_SubProcess/server_build/src/mangosd/RelWithDebInfo/mangosd.exe" -c mangosd.conf -t custody ); echo "EXIT=$?"
  ```
  Must print `custody OK` / `EXIT=0`.

### 1b. Database
- **Character DB:** `character0.custody_ledger` is already present and empty — no action.
- **World DB (`mangos0`) — apply the two pending AH command migrations** (idempotent,
  version-gated; current tip `22/5/1`):
  ```
  mysql -umangos -pmangos mangos0 < "E:/Mangos/WIP/Zero/AH_SubProcess/database/World/Updates/Rel22/Rel22_05_002_AH_Console_Commands.sql"
  mysql -umangos -pmangos mangos0 < "E:/Mangos/WIP/Zero/AH_SubProcess/database/World/Updates/Rel22/Rel22_05_003_AH_Repair_Command.sql"
  ```
  Each prints `* UPDATE COMPLETE *`; re-running prints `* UPDATE SKIPPED *`. Confirm tip is
  now `22/5/3`.
- **Create the two differential clones from `character0`** (both inherit `custody_ledger`):
  ```
  mysqladmin -umangos -pmangos create character0_custody_off
  mysqladmin -umangos -pmangos create character0_custody_on
  mysqldump -umangos -pmangos character0 | mysql -umangos -pmangos character0_custody_off
  mysqldump -umangos -pmangos character0 | mysql -umangos -pmangos character0_custody_on
  ```

### 1c. Config (test realm `mangosd.conf`)
- Add the custody keys (the deployed conf lacks them):
  ```
  AH.Service.Custody = 0
  AH.Service.CustodyCrashAt = ""
  ```
- Quiesce: disable the AH bot, no other logins, pin/normalize the clock if you can.
- Enable packet logging so opcode order can be compared:
  ```
  WorldLogFile = "world.log"
  ```
- Point `CharacterDatabaseInfo` at the clone under test (swap between
  `character0_custody_off` and `character0_custody_on` per run). World DB stays `mangos0`.

---

## 2. Seeded fixture (record everything)

1. One named seller char (e.g. `Custodyseller`) with a large known gold balance.
2. One named buyer/bidder char (e.g. `Custodybuyer`) with a large known gold balance.
3. A handful of known items in the seller's bags — note each `item_instance.guid` and
   template. Include at least one stackable and one non-stackable.
4. Record baselines (run against the clone you are about to drive):
   ```sql
   SELECT MAX(id) AS max_mail_id FROM mail;
   SELECT MAX(id) AS max_auction_id FROM auction;
   SELECT guid, money FROM characters WHERE guid IN (<seller_guid>, <buyer_guid>);
   ```
5. Write the workload as an exact, deterministic **checklist** (fixed gold amounts, fixed
   items, fixed bid/buyout values) so it can be replayed identically on both clones.

---

## 3. The A/B differential (core test)

**Goal:** the same scripted workload on clone A (`Custody=0`) and clone B (`Custody=1`)
must produce byte-identical player-visible state.

**Method (single server, two passes):**
1. Restore both clones fresh from the seeded fixture (so both start identical).
2. Pass A — set `CharacterDatabaseInfo → character0_custody_off`, `AH.Service.Custody = 0`,
   boot, run the full workload checklist, log out, shut down. Keep `world.log` as `world_A.log`.
3. Pass B — set `CharacterDatabaseInfo → character0_custody_on`, `AH.Service.Custody = 1`,
   boot, run the **identical** checklist, shut down. Keep `world.log` as `world_B.log`.
   - On clone B, **list the items while `Custody=1`** so the auctions are born with custody
     rows (the gate is per-auction: `Custody && CustodyLedger::HasRows`). Then bid/buyout/
     cancel/expire on those auctions exercise the custody seams.

**Workload checklist (the spec Section 8 matrix — see `custody_crash_test.md` for the full
table):** list an item (online + offline seller); first bid; outbid a real prior bidder;
same-bidder raise; same-bidder top-up to buyout; buyout with a prior bidder; full-price
reject (`wallet >= delta && wallet < newbid`); player outbids a bot-bid auction; player
cancels a bot-bid auction; cancel no-bid; cancel with bid; cancel-cannot-afford-cut; let one
unsold auction expire; let one bid auction resolve to a winner; offline winner/owner.

**Diff:**
```sql
-- in the mysql client:
SET @clone_a := 'character0_custody_off';
SET @clone_b := 'character0_custody_on';
SET @compare_absolute_mail_times := 0;     -- 1 only if the clock was pinned
SET @compare_absolute_auction_times := 0;  -- 1 only if the clock was pinned
SOURCE E:/Mangos/WIP/Zero/AH_SubProcess/server/src/ah-service/tools/custody_diff.sql;
```
**Pass = every `*_DIFF` section returns zero rows.** If `MAIL_DIFF` is clean but
`MAIL_ITEMS_DIFF` is not, `mail.id` drifted between clones — re-verify the idle-snapshot
precondition (re-clone and rerun).

**Packet order:** confirm the same `SMSG_AUCTION_OWNER_NOTIFICATION` /
`SMSG_AUCTION_BIDDER_NOTIFICATION` / `SMSG_AUCTION_REMOVED_NOTIFICATION` /
`SMSG_AUCTION_COMMAND_RESULT` sequence in `world_A.log` vs `world_B.log`.

---

## 4. Crash-injection (per seam) — see `custody_crash_test.md`

For each seam (S1 list, S2/S3 bid/buyout, S4 win, S5 cancel, S6 expire) and **each phase**
(`pre-commit`, `pre-deferred`), on a fresh clone with `AH.Service.Custody = 1`:

1. Set `AH.Service.CustodyCrashAt = "pre-commit"` (then `"pre-deferred"`).
2. Boot, drive exactly that one seam action.
3. Confirm the process exits with code **3** and the log shows
   `custody crash-injection: _exit(3) at phase '<phase>'`.
4. Reset `AH.Service.CustodyCrashAt = ""`, keep `Custody = 1`, reboot, and assert recovery:
   - `pre-commit`: nothing committed. Pre-seam DB state unchanged — **for S1 no auction row
     exists** (never inserted); for S2–S6 the existing auction row is unchanged. Zero new
     `mail` rows past the recorded `MAX(mail.id)`; no phantom packets in the log.
   - `pre-deferred`: the txn committed; the deferred effects did not run before death; reboot
     re-resolution reaches the same final projection as the no-crash run.
5. **Forced rollback** (no crash): force a statement failure in the open custody txn (e.g.
   adapt the `-t custody` duplicate-`idem_key` case) and assert `CommitTransactionChecked()`
   returns false → no deferred effects, restored live state, DB-error result.

---

## 5. Concurrent observer

While a seam resolves, have a second client list/search the AH (or run `ah console`).
Confirm it sees a coherent pre- or post-resolution state — no duplicate/phantom packets,
no mid-resolution tear.

---

## 6. `ah repair` smoke (drift detection)

From the **server console** (not in-game):
- `ah repair` on a clean realm prints `0 custody-ledger drift row(s) found`.
- To prove detection: delete a required non-terminal row of a live custody auction
  (`DELETE FROM custody_ledger WHERE idem_key = 'item:<id>'`), then `ah repair` flags it.
  `ah repair apply` terminalizes orphan rows **without disbursing gold or re-mailing items**;
  `ah repair force-forfeit <idem_key>` is the explicit human-verified hatch. Neither mints value.

---

## 7. Promotion decision

Flip `AH.Service.Custody = 1` (default-on) only when **all** are green:
- Section 3 differential: zero `*_DIFF` rows for the full matrix + identical packet order.
- Section 4: both crash phases pass for every seam + forced-rollback restores state.
- Section 5: concurrent observer clean.
- A **default-off soak** on the real realm shows zero custody drift (`ah repair --dry-run`
  / reconcile logs) and zero player-visible divergence.

Because the gate is per-auction, flipping the default makes only **new** auctions custody-
managed; pre-existing auctions drain through the legacy path. Roll back instantly by setting
`AH.Service.Custody = 0` — no data migration either way.
