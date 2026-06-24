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
