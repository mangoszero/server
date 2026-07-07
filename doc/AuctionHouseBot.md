Auction house bot
-----------------
For testing purposes and low population home servers, *mangos* provides an
auction house bot, which will provide a set of items on the auction houses based
on various configuration settings.

Configuration
-------------
Edit `ahbot.conf` to match your requirements. You can fine tune the amount of
items, the pricing, the range of item quality, and many other settings.

Please note that the default auction house bot settings will disable auction
generation. This is done by intention to prevent putting up a wrong range of
auctions for your server.

Notes
-----
The following rules apply when using the auction house bot:

* If an auction expires, auctions are deleted quietly.
* the bot will not buy its own items, and will not receive mail from the AH or
  receive returned mails.

The AH bot runs as a reserved *system* character named "AuctionHouse" (low-GUID
0xFFFFFFFE) -- there is NO real character to create or maintain, and it
regenerates automatically after a database wipe. The name is reserved from
players and cannot be mailed. Do not create a real character named
"AuctionHouse". To use a real character instead, set
AuctionHouseBot.CharacterName to that character's name in ahbot.conf.

Out-of-Process Run Mode (ah-service)
-------------------------------------
The auction house bot can be run in one of two modes:

**In-process (default):** The bot runs inside mangosd itself. This is the
classic mode and requires no extra configuration. It is enabled as long as
`AH.Service.Enabled = 0` in `mangosd.conf`.

**Out-of-process service:** The bot runs as a separate `ah-service` child
process. Set `AH.Service.Enabled = 1` in `mangosd.conf` to activate this
mode. When the service is active, the in-process bot stands down automatically
(dual-bot guard prevents double-posting). If the service crashes or loses its
connection, the in-process bot resumes automatically until the service
reconnects.

Architecture
~~~~~~~~~~~~
The child process reads a market snapshot and item pool from the database, then
emits *intents* (auction creation/cancellation proposals) to mangosd over a
local TCP channel. mangosd re-validates every intent against live state before
applying it and deduplicates by UUID (executor uuid-dedup), so the child never
touches gold, items, or auctions directly. The channel uses a simple IPC
framing protocol (proto v1); no DB credentials cross the wire.

The ah-service console window is hidden by default on Windows. The `ah console
show` and `ah console hide` commands, issued from the mangosd server console,
toggle visibility at runtime.

Security (trust model and least-privilege DB account)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The child process is **untrusted**. mangosd re-validates every intent against
live state precisely because it does not trust the child; the child only ever
*advises*. That trust boundary only holds for the IPC channel, however.

The child's DB connections are described as "read-only", but that only means
the child *issues* nothing but `SELECT` statements -- it is a property of the
child's code, **not** an enforced credential restriction. The connection
string in `WorldDatabaseInfo` / `CharacterDatabaseInfo` is an ordinary MySQL
user/password. If you copy mangosd's full read/write credentials into it, a
compromised or malfunctioning child could `INSERT`/`UPDATE`/`DELETE` auctions,
characters, or gold **directly in the database**, completely bypassing the
mangosd executor authority that the whole intent-re-validation design exists to
provide.

To make the adversarial model actually hold, provision a **dedicated
least-privilege MySQL account** for the child with `SELECT`-only grants, and use
it in `WorldDatabaseInfo` / `CharacterDatabaseInfo` instead of mangosd's
credentials. With a SELECT-only account, a compromised child *cannot* write the
DB at all: the only path left to it is the IPC channel, where mangosd validates
everything. Without it, this least-privilege grant is the only thing standing
between a compromised child and a direct-DB-write bypass of the executor.

Example grants (run as a MySQL admin; adjust db names, host, and password to
match your deployment). The child only needs to read the world tables the item
pool and snapshot consult and the character `auction` / `characters` tables:

```sql
CREATE USER 'ahbot_ro'@'localhost' IDENTIFIED BY 'change-me';

-- Minimal: only the tables the child reads.
GRANT SELECT ON `mangos0`.`item_template` TO 'ahbot_ro'@'localhost';
GRANT SELECT ON `mangos0`.`npc_vendor`    TO 'ahbot_ro'@'localhost';
-- (plus any loot tables the seller filter consults, e.g.
--  creature_loot_template, etc.)
GRANT SELECT ON `character0`.`auction`    TO 'ahbot_ro'@'localhost';
GRANT SELECT ON `character0`.`characters` TO 'ahbot_ro'@'localhost';

-- Or, more simply, SELECT-only on the whole schemas:
GRANT SELECT ON `mangos0`.*    TO 'ahbot_ro'@'localhost';
GRANT SELECT ON `character0`.* TO 'ahbot_ro'@'localhost';

FLUSH PRIVILEGES;
```

Then set, in `ah-service.conf`:

```
WorldDatabaseInfo     = "127.0.0.1;3306;ahbot_ro;change-me;mangos0"
CharacterDatabaseInfo = "127.0.0.1;3306;ahbot_ro;change-me;character0"
```

mangosd.conf keys (service mode)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
These keys are only active when `AH.Service.Enabled = 1`:

* ``AH.Service.Enabled``       - Set to 1 to launch ah-service as a child
  process instead of running the bot in-process. Default: 0.
* ``AH.Service.Path``          - Path to the ah-service executable.
* ``AH.Service.Port``          - TCP port for the IPC channel (default: 5760).
* ``AH.Service.Secret``        - Shared secret used to authenticate the child
  during the IPC handshake.
* ``AH.Service.Config``        - Path to ah-service.conf passed to the child on
  startup.
* ``AH.Service.IntentTtlSec``  - How long (seconds) the executor retains UUID
  records for deduplication. Default: 900.

Player-auction custody escrow
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Player auction custody is an in-process escrow ledger for player auction gold
and items. It is independent of the out-of-process bot switch above:
``AH.Service.Custody`` can remain off while ``AH.Service.Enabled`` is on, and
can be enabled for player seams even when the bot still runs in-process.

The relevant ``mangosd.conf`` keys are:

* ``AH.Service.Custody`` - Default: 0. When 0, player auctions use today's
  direct legacy path. When 1, only auctions that already carry custody ledger
  rows use the custody path.
* ``AH.Service.CustodyCrashAt`` - Default: empty. Test-only crash injection for
  disposable realms. Values are ``pre-commit`` and ``pre-deferred``; never set
  this on a live realm.

The custody gate is per-auction. ``CustodyLedger::HasRows(auction_id)`` marks
an auction as custody-managed, so existing pre-gate auctions and bot-created
auctions with no custody rows continue through the legacy path. This lets a
realm drain old auctions while the feature remains default-off and reversible.

If reconciliation reports custody drift, use the server console command
``ah repair``. It is console-only and scoped to custody-ledger drift; it does
not repair legacy auction tears. The default form is dry-run. ``ah repair
apply`` terminalizes supported orphan custody rows without disbursing gold or
re-mailing items, which avoids minting value from a row whose live auction is
gone. ``ah repair force-forfeit <idem_key>`` is the explicit escape hatch after
manual verification; it terminalizes the named drift row without disbursement
or item mail.

Before enabling custody on a real realm, validate it first on a database clone
and confirm ``ah repair`` reports zero drift after normal list, bid, buyout,
cancel, and expire flows. Keep the setting default-off until the realm's live
auction behavior has been observed cleanly.

ah-service.conf keys
~~~~~~~~~~~~~~~~~~~~~
The child process uses its own configuration file (``ah-service.conf``):

* ``WorldDatabaseInfo``             - Connection string for the world database.
  Use a SELECT-only DB account here (see Security above), not mangosd's
  credentials.
* ``WorldDatabaseConnections``      - Number of world DB connections (the child
  only issues SELECTs on them).
* ``CharacterDatabaseInfo``         - Connection string for the character
  database. Use a SELECT-only DB account here (see Security above).
* ``CharacterDatabaseConnections``  - Number of character DB connections (the
  child only issues SELECTs on them).

Both databases must reside on the same MySQL server instance; the market
snapshot uses a cross-database JOIN between the world and character schemas.
The child's DB connections are **read-only by usage**: it only ever issues
`SELECT` queries. That is a code-discipline guarantee, not an enforced
credential restriction (see Security below).

* ``AhBot.ConfigPath``              - Path to ``ahbot.conf`` (pricing, amounts,
  quality ranges, and other tuning settings).
* ``Console.ShowOnStartup``         - Set to 1 to show the ah-service console
  window on startup (Windows only). Default: 0 (hidden).

Out-of-process service - deployment & operational notes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**1. Use a dedicated bot character.**
Point ``AuctionHouseBot.CharacterName`` at a character on a **separate account
that you never log into**. The bot owns every auction it creates. If you log
in as that character you inherit all of them in your Auctions tab, and the
1.12 client's owner-items list is unpaginated -- hundreds of owned auctions
in one house will crash the client (issue #132). Rule: never log into the bot
character while the service (or the in-process bot) is active. Normal players
are unaffected; the public browse list is paged.

**2. Stock-target sizing.**
The default ``AuctionHouseBot.Items.Amount.*`` targets (White 2000 / Green 2500
/ Blue 1500 / Purple 1000, roughly 7 000 total, ~2 300 per faction house)
populate a large auction house. Tune these down for low-population or
development realms. The bot boost-fills toward the target then maintains it,
so a first-run burst can be large if the house starts empty.

**3. Performance and DB durability.**
The mangosd executor applies each auction synchronously on the world thread
(one character-DB transaction per auction -- the same mechanism the in-process
bot uses). A large initial fill under the default MariaDB durability setting
(``innodb_flush_log_at_trx_commit = 1``, one fsync per commit) can cause brief
world-tick hitches during the burst. For development or test deployments,
``innodb_flush_log_at_trx_commit = 2`` (commit to OS page cache) smooths this;
a larger ``innodb_buffer_pool_size`` or SSD data directory also helps. In
production, keep stock amounts moderate to avoid a large fill on first start.

**4. Read-only DB account (child-side trust boundary).**
The child process issues only ``SELECT`` statements. Provision it a dedicated
least-privilege MySQL account with ``SELECT``-only grants (see the Security
section above) rather than copying mangosd's full read/write credentials.
With a ``SELECT``-only account a compromised or malfunctioning child cannot
write the database at all -- the only path remaining is the IPC channel, where
mangosd re-validates everything.

GM commands under the service
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
The `ahbot` GM command set is extended when the service is running:

* ``ahbot status``   - Displays live auction counts and executor statistics
  (intents applied, rejected, and deduplicated).
* ``ahbot reload``   - Live-reloads the child's tuning configuration (prices,
  ratios, amounts, timers, and enable/disable toggles). Note: pool-affecting
  filters (item level, quality, class, bind type, and include/exclude lists)
  take effect only after a full service restart.
* ``ahbot rebuild``  - Rebuilds the item pool, as in in-process mode.

Custody repair command
~~~~~~~~~~~~~~~~~~~~~~
The ``ah repair`` command is available from the mangosd console for the player
auction custody ledger. It reports custody drift by default, mutates only with
``ah repair apply`` or ``ah repair force-forfeit <idem_key>``, and never mints
gold or sends replacement item mail from an orphan row.

### Worker AH browse path

When the ah-service worker is connected and healthy, mangosd forwards AH browse
/ owner-list / bidder-list requests to it over IPC (IPC_BROWSE_QUERY) and
assembles the client reply from IPC_BROWSE_RESULT on a later world tick. The
worker reproduces the full CanUseItem usable filter and full entry data
(enchant/suffix/charges decoded from item_instance). On ENABLE_ELUNA builds the
OnCanUseItem Lua hook is deferred to mangosd's world thread (the worker returns
the un-paginated usable set; mangosd runs the hook + paginates). Player
mutations (sell/bid/buyout/cancel) remain fully in-process.

**Coordinator model (worker-mandatory reads).** Once an ah-service worker is the
configured AH authority, mangosd holds no AH read state of its own. If the worker
is down, times out, errors, or is overloaded (queue-full / oversize), mangosd
returns **"AH temporarily unavailable"** — a center-screen notification flash plus
a red system chat line, and an empty list on any in-flight browse so the client
does not hang — and serves **nothing** in-process. While the worker is down the
auction **window will not open** at all: the open path (auctioneer click, gossip
option, `.auction` GM commands) is gated, so the player just gets the message. (A
window already open when the worker dies stays open — 1.12 has no close-window
opcode — and its next browse returns the unavailable reply.) A worker fault
therefore degrades only the AH, never the realm. (If **no** worker is configured
at all, the legacy single-process in-process AH is used as before.)
Set `CharacterDatabaseConnections >= 2` in ah-service.conf so the browse thread's
SELECTs do not serialize behind the bot snapshot.

**Over-cap deferred-Eluna (decision #2).** On an `ENABLE_ELUNA` realm with an
active `OnCanUseItem` veto, a single "usable"-filtered search that yields more
than ~1000 matches cannot be veto-checked exactly out-of-process, so the worker
**declines** it and mangosd returns "AH temporarily unavailable" rather than a
short or mis-counted page. This is rare (needs both >1000 surviving matches and a
bound Lua hook) and follows the coordinator's "correct-or-unavailable" contract —
**expected, not a regression** — so do not treat it as a service failure.

**Known limitation — Eluna `Player:SendAuctionMenu`.** The window-open gate lives
in `WorldSession::SendAuctionHello`. A Lua script that calls
`Player:SendAuctionMenu` builds `MSG_AUCTION_HELLO` directly and so bypasses that
gate: while the worker is down the window opens, but its first browse returns the
unavailable reply (an empty window). This affects only realms whose scripts call
that API, and the fix would mean patching the third-party Eluna module, so it is
left as a documented limitation.

### Worker write-authority path

`AH.Service.WriteAuthority` (in `mangosd.conf`, default `0`) hands the
out-of-process worker authority over the auction book. When set, the worker is
the **single writer** of the `auction` table: it inserts listings, applies
player bids/buyouts/cancels, and runs the expiry/win tick, while mangosd forwards
player mutations to it over the reliable IPC lane and applies the returned facts
against the in-process custody escrow ledger.

**Boot-latched flag.** The value is read once at startup (`LoadConfigSettings`
only reads it when `!reload`), so `.reload config` can never toggle it mid-run --
the process must be restarted to change it. This makes the "single book writer"
invariant a per-boot property that no runtime reload can violate.

**Hard requirement: `AH.Service.Custody = 1`.** The write path rides the custody
escrow ledger (deposit/bid reservations, item escrow, seller payout netting).
Enabling `WriteAuthority` with `Custody = 0` is unsupported.

**Worker DB grant delta.** On top of the SELECT-only reader grants in *Security*
above, a write-authority worker's DB account needs INSERT/UPDATE/DELETE on
exactly two Character-DB tables -- and nothing else:

```sql
GRANT INSERT, UPDATE, DELETE ON `character0`.`auction`           TO 'ahworker'@'localhost';
GRANT INSERT, UPDATE, DELETE ON `character0`.`ah_worker_journal` TO 'ahworker'@'localhost';
```

**Residual mangosd-side writers that stand down (spec section 5.7).** With
`WriteAuthority = 1` these in-process auction writers no-op so there is exactly
one book writer:

* `AuctionHouseMgr::LoadAuctions` boot repair (item-template mismatch UPDATE,
  missing-item / invalid-house row DELETE + return-mail) -- the worker's
  `LoadFromDb` gates and reports instead.
* `ObjectMgr::SetHighestGuids` auction-orphan `DELETE`.
* `World::Update` `WUPDATE_AUCTIONS` in-process expiry sweep (`sAuctionMgr.Update`)
  -- the worker runs the expiry/win tick; the returned-mail delivery still runs
  in-process in both modes.
* The in-process `AuctionHouseBot` startup (`sAuctionBot.Initialize`) -- the
  worker's bot brain drives listings; the in-process buyer/seller stay null.

Eluna AH hooks (`OnAdd` / `OnRemove`) still fire under write-authority: they are
raised at the finalize position from the worker's facts on a synthesized
transient `AuctionEntry`, so existing Lua AH scripts keep working.

> **OPERATOR WARNING 1 -- do not `.reload config` Custody off while
> WriteAuthority is on.** `WriteAuthority` is boot-latched but `Custody` is
> reloadable. Reloading with `AH.Service.Custody = 0` while the worker still
> holds write-authority bypasses the runtime custody invariant the write path
> depends on and can corrupt in-flight escrow. If you must disable custody,
> restart with `WriteAuthority = 0` as well.

> **OPERATOR WARNING 2 -- apply the `ah_worker_journal` migration BEFORE enabling
> WriteAuthority.** The worker records its committed mutations in
> `ah_worker_journal`; the mangosd-side reconcile-on-reconnect reads it to decide
> finalize-forward vs release. A missing table silently degrades reconcile to
> "release", which can double-credit a mutation that actually committed. Verify
> the migration is present first.

Before enabling write-authority on a real realm, validate it first on a database
clone with `AH.Service.WriteAuthority = 1` and `AH.Service.Custody = 1`. Exercise
listing, bidding, outbidding, same-bidder buyout, seller cancel with an auction
cut, expiry/win, worker reconnect, and mangosd restart. Then run `ah repair` and
confirm it reports zero custody drift.
