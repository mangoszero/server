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

mangosd.conf keys (service mode)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
These keys are only active when `AH.Service.Enabled = 1`:

* ``AH.Service.Enabled``       - Set to 1 to launch ah-service as a child
  process instead of running the bot in-process. Default: 0.
* ``AH.Service.Path``          - Path to the ah-service executable.
* ``AH.Service.Port``          - TCP port for the IPC channel (default: 17878).
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
* ``WorldDatabaseConnections``      - Number of read-only world DB connections.
* ``CharacterDatabaseInfo``         - Connection string for the character
  database.
* ``CharacterDatabaseConnections``  - Number of read-only character DB
  connections.

Both databases must reside on the same MySQL server instance; the market
snapshot uses a cross-database JOIN between the world and character schemas.
The connections are opened read-only by the child.

* ``AhBot.ConfigPath``              - Path to ``ahbot.conf`` (pricing, amounts,
  quality ranges, and other tuning settings).
* ``Console.ShowOnStartup``         - Set to 1 to show the ah-service console
  window on startup (Windows only). Default: 0 (hidden).

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
