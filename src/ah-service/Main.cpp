/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file Main.cpp
 * @brief ah-service entry point.
 *
 * Modes:
 *   --selftest           Run an in-process loopback test (server + client,
 *                        full handshake + IPC_ECHO round-trip), then exit 0.
 *
 *   --poolcheck          Load config + open the read-only world DB, build the
 *                        seller item pool, print per-[quality][class] counts,
 *                        then exit (0 on success, 1 on any failure). Requires
 *                        --config <ah-service.conf>.
 *
 *   --port <p>           Connect to mangosd IPC server on this port.
 *   --secret <s>         Shared secret for handshake authentication
 *                        (manual-testing fallback only; the supervisor
 *                        passes the secret out-of-band via the
 *                        AH_SERVICE_SECRET environment variable, read first).
 *   --botguid <g>        Authoritative bot low-GUID resolved by mangosd. The
 *                        child STAMPS every emitted intent with this value so
 *                        the executor's GUID guard always matches mangosd's
 *                        GetAHBotId(). A value of 0 means mangosd has no valid
 *                        bot character; the child then exits non-zero.
 *   --config <path>      ah-service.conf path (infra keys + ahbot.conf path).
 *
 * Normal mode: load config + open DB(s) + build item pool + resolve the bot
 * brain BEFORE the IPC handshake, so "READY" implies the bot is OPERATIONAL.
 * On any required-setup failure the child logs and exits non-zero (the
 * supervisor backs off and restarts). Then connect, handshake, and loop
 * handling:
 *   IPC_HEARTBEAT  -> IPC_HEARTBEAT_ACK
 *   IPC_ECHO       -> IPC_ECHO_REPLY (body echoed back)
 *   IPC_SHUTDOWN   -> IPC_SHUTDOWN_ACK, then exit
 */

#include "IpcVersion.h"
#include "IpcChannel.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "AuctionIntents.h"
#include "Threading/Threading.h"
#include "Console.h"
#include "Config/Config.h"
#include "ServiceConfig.h"
#include "ServiceDatabase.h"
#include "ItemPool.h"
#include "MarketSnapshot.h"
#include "BotBrain.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

// ---------------------------------------------------------------------------
// Self-test: intent codec round-trip
// ---------------------------------------------------------------------------

/**
 * @brief Encode/decode round-trip test for all four AH intent structs.
 *
 * Each struct is filled with distinct non-zero sentinel values, encoded into
 * a fresh ByteBuffer, decoded into a second instance, and every field is
 * compared for equality.
 *
 * @return 0 on success, 1 on any field mismatch.
 */
static int RunIntentCodecSelfTest()
{
    // --- SellIntent ---
    {
        SellIntent a;
        a.uuid        = UINT64_C(0xDEADBEEF00000001);
        a.botGuid     = 0x00AA0001u;
        a.house       = 2u;
        a.itemId      = 0x00001234u;
        a.stack       = 20u;
        a.bid         = 10000u;
        a.buyout      = 50000u;
        a.durationHrs = 48u;

        ByteBuffer buf;
        a.Encode(buf);

        SellIntent b;
        if (!b.Decode(buf))
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent::Decode"
                    " returned false\n");
            return 1;
        }

        if (b.uuid != a.uuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent uuid"
                    " mismatch\n");
            return 1;
        }
        if (b.botGuid != a.botGuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent botGuid"
                    " mismatch\n");
            return 1;
        }
        if (b.house != a.house)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent house"
                    " mismatch\n");
            return 1;
        }
        if (b.itemId != a.itemId)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent itemId"
                    " mismatch\n");
            return 1;
        }
        if (b.stack != a.stack)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent stack"
                    " mismatch\n");
            return 1;
        }
        if (b.bid != a.bid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent bid"
                    " mismatch\n");
            return 1;
        }
        if (b.buyout != a.buyout)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent buyout"
                    " mismatch\n");
            return 1;
        }
        if (b.durationHrs != a.durationHrs)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: SellIntent durationHrs"
                    " mismatch\n");
            return 1;
        }
    }

    // --- BidIntent ---
    {
        BidIntent a;
        a.uuid      = UINT64_C(0xDEADBEEF00000002);
        a.botGuid   = 0x00AA0002u;
        a.auctionId = 0x00009999u;
        a.bidAmount = 75000u;

        ByteBuffer buf;
        a.Encode(buf);

        BidIntent b;
        if (!b.Decode(buf))
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BidIntent::Decode"
                    " returned false\n");
            return 1;
        }

        if (b.uuid != a.uuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BidIntent uuid"
                    " mismatch\n");
            return 1;
        }
        if (b.botGuid != a.botGuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BidIntent botGuid"
                    " mismatch\n");
            return 1;
        }
        if (b.auctionId != a.auctionId)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BidIntent auctionId"
                    " mismatch\n");
            return 1;
        }
        if (b.bidAmount != a.bidAmount)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BidIntent bidAmount"
                    " mismatch\n");
            return 1;
        }
    }

    // --- BuyoutIntent ---
    {
        BuyoutIntent a;
        a.uuid      = UINT64_C(0xDEADBEEF00000003);
        a.botGuid   = 0x00AA0003u;
        a.auctionId = 0x0000ABCDu;

        ByteBuffer buf;
        a.Encode(buf);

        BuyoutIntent b;
        if (!b.Decode(buf))
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BuyoutIntent::Decode"
                    " returned false\n");
            return 1;
        }

        if (b.uuid != a.uuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BuyoutIntent uuid"
                    " mismatch\n");
            return 1;
        }
        if (b.botGuid != a.botGuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BuyoutIntent botGuid"
                    " mismatch\n");
            return 1;
        }
        if (b.auctionId != a.auctionId)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: BuyoutIntent auctionId"
                    " mismatch\n");
            return 1;
        }
    }

    // --- IntentResult ---
    {
        IntentResult a;
        a.uuid   = UINT64_C(0xDEADBEEF00000004);
        a.status = static_cast<uint8>(INTENT_REJECTED);
        a.reason = static_cast<uint8>(REASON_NO_FUNDS);

        ByteBuffer buf;
        a.Encode(buf);

        IntentResult b;
        if (!b.Decode(buf))
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: IntentResult::Decode"
                    " returned false\n");
            return 1;
        }

        if (b.uuid != a.uuid)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: IntentResult uuid"
                    " mismatch\n");
            return 1;
        }
        if (b.status != a.status)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: IntentResult status"
                    " mismatch\n");
            return 1;
        }
        if (b.reason != a.reason)
        {
            fprintf(stderr,
                    "intent codec selftest FAILED: IntentResult reason"
                    " mismatch\n");
            return 1;
        }
    }

    printf("intent codec selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: in-process loopback
// ---------------------------------------------------------------------------

/**
 * @brief Run a full in-process loopback self-test.
 *
 * Starts IpcServer + IpcClient in-process on 127.0.0.1:17878, performs
 * the handshake, sends IPC_ECHO, waits for IPC_ECHO_REPLY, then shuts
 * down both sides.
 *
 * @return 0 on success, 1 on failure.
 */
static int RunSelfTest()
{
    printf("ah-service selftest starting (proto v%u)\n", IPC_PROTOCOL_VERSION);
    fflush(stdout);

    const char*  host   = "127.0.0.1";
    const uint16 port   = 17878;
    const char*  secret = "selftest-secret";

    // --- Start server ---
    IpcServer srv;
    if (!srv.Start(host, port, secret))
    {
        fprintf(stderr, "selftest FAILED: IpcServer::Start failed\n");
        return 1;
    }

    // Give the acceptor a moment to bind before the client connects.
    ACE_Based::Thread::Sleep(50);

    // --- Connect client ---
    IpcClient cli;
    if (!cli.Connect(host, port, secret))
    {
        fprintf(stderr, "selftest FAILED: IpcClient::Connect failed\n");
        srv.Stop();
        return 1;
    }

    // --- Wait for handshake (up to 3 s) ---
    const int maxWaitMs = 3000;
    int waited = 0;
    while (!srv.Connected() || !cli.Connected())
    {
        if (waited >= maxWaitMs)
        {
            fprintf(stderr, "selftest FAILED: handshake timed out"
                            " (srv=%s cli=%s)\n",
                    srv.Connected() ? "live" : "not-live",
                    cli.Connected() ? "live" : "not-live");
            cli.Stop();
            srv.Stop();
            return 1;
        }
        ACE_Based::Thread::Sleep(20);
        waited += 20;
    }

    printf("selftest: handshake complete in ~%d ms\n", waited);
    fflush(stdout);

    // --- Send IPC_ECHO from server to client ---
    IpcMessage echo;
    echo.op = IPC_ECHO;
    const char* payload = "ping-selftest";
    echo.body.append(reinterpret_cast<const uint8*>(payload), strlen(payload));

    if (!srv.SendFrame(echo))
    {
        fprintf(stderr, "selftest FAILED: srv.SendFrame(IPC_ECHO) failed\n");
        cli.Stop();
        srv.Stop();
        return 1;
    }

    // The client main loop handles IPC_ECHO -> IPC_ECHO_REPLY automatically.
    // We need to run it briefly: pump the client inbound queue by running
    // a mini-loop here, then check for IPC_ECHO_REPLY on the server side.

    // Client loop: handle one IPC_ECHO.
    const int echoWaitMs = 2000;
    int echoWaited = 0;
    bool gotReply = false;

    while (!gotReply)
    {
        // Let client process its inbound queue (IPC_ECHO -> send reply).
        IpcMessage clientMsg;
        if (cli.PopInbound(clientMsg))
        {
            if (clientMsg.op == IPC_ECHO)
            {
                // Client side: send IPC_ECHO_REPLY with same body.
                IpcMessage reply;
                reply.op   = IPC_ECHO_REPLY;
                reply.body = clientMsg.body;
                cli.SendFrame(reply);
            }
        }

        // Check server side for the reply.
        IpcMessage srvMsg;
        if (srv.PopInbound(srvMsg))
        {
            if (srvMsg.op == IPC_ECHO_REPLY)
            {
                gotReply = true;
                break;
            }
        }

        if (echoWaited >= echoWaitMs)
        {
            break;
        }

        ACE_Based::Thread::Sleep(20);
        echoWaited += 20;
    }

    cli.Stop();
    srv.Stop();

    if (!gotReply)
    {
        fprintf(stderr, "selftest FAILED: no IPC_ECHO_REPLY after %d ms\n",
                echoWaitMs);
        return 1;
    }

    printf("ipc selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Pool check: build the seller item pool and report counts
// ---------------------------------------------------------------------------

/**
 * @brief Foundation check for Task 8a.
 *
 * Loads the AH bot config, opens the child's read-only world-DB connection,
 * builds the seller item pool, and prints per-[quality][class] counts. No
 * decisions or intents are made (that is Task 8c).
 *
 * @param cfgPath Path to ah-service.conf (already validated non-null by the
 *                caller); loaded into sConfig for the infra keys.
 * @return 0 on success, 1 on any failure.
 */
static int RunPoolCheck(const char* cfgPath)
{
    if (cfgPath == nullptr)
    {
        fprintf(stderr, "ah-service: --poolcheck requires --config <path>\n");
        return 1;
    }

    if (!sConfig.SetSource(cfgPath))
    {
        fprintf(stderr, "ah-service: could not load config '%s'\n", cfgPath);
        return 1;
    }

    ServiceConfig config;
    if (!config.Initialize())
    {
        fprintf(stderr, "ah-service: ServiceConfig::Initialize failed\n");
        return 1;
    }

    ServiceDatabase db;
    if (!db.Init())
    {
        fprintf(stderr, "ah-service: ServiceDatabase::Init failed\n");
        return 1;
    }

    ItemPool pool(config, db);
    bool built = pool.Build();
    pool.LogSummary();

    db.Shutdown();

    if (!built)
    {
        fprintf(stderr, "ah-service: item pool is empty\n");
        return 1;
    }

    printf("ah-service poolcheck OK (%u items)\n", pool.GetTotalCount());
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Snapshot check: read the live auction tables and report counts
// ---------------------------------------------------------------------------

/**
 * @brief Foundation check for Task 8b.
 *
 * Loads the AH bot config, opens the child's read-only world-DB and
 * character-DB connections, takes one MarketSnapshot, and prints per-house
 * counts plus a sample record.  No decisions or intents are made.
 *
 * @param cfgPath Path to ah-service.conf (already validated non-null by the
 *                caller); loaded into sConfig for the infra keys.
 * @return 0 on success, 1 on any failure.
 */
static int RunSnapCheck(const char* cfgPath)
{
    if (cfgPath == nullptr)
    {
        fprintf(stderr,
                "ah-service: --snapcheck requires --config <path>\n");
        return 1;
    }

    if (!sConfig.SetSource(cfgPath))
    {
        fprintf(stderr, "ah-service: could not load config '%s'\n",
                cfgPath);
        return 1;
    }

    ServiceDatabase db;
    if (!db.Init())
    {
        fprintf(stderr, "ah-service: ServiceDatabase::Init failed\n");
        return 1;
    }

    if (!db.InitCharacter())
    {
        fprintf(stderr,
                "ah-service: ServiceDatabase::InitCharacter failed\n");
        db.Shutdown();
        return 1;
    }

    MarketSnapshot snap(db);
    snap.Refresh();

    db.Shutdown();

    const uint32 alliance = static_cast<uint32>(
        snap.GetHouse(AH_AUCTION_HOUSE_ALLIANCE).size());
    const uint32 horde = static_cast<uint32>(
        snap.GetHouse(AH_AUCTION_HOUSE_HORDE).size());
    const uint32 neutral = static_cast<uint32>(
        snap.GetHouse(AH_AUCTION_HOUSE_NEUTRAL).size());

    printf("ah-service snapcheck: alliance=%u horde=%u neutral=%u"
           " total=%u\n",
           alliance, horde, neutral, snap.TotalCount());

    if (snap.TotalCount() > 0)
    {
        const AuctionRecord& sample =
            snap.GetHouse(0).empty()
            ? (snap.GetHouse(1).empty()
               ? snap.GetHouse(2).front()
               : snap.GetHouse(1).front())
            : snap.GetHouse(0).front();
        printf("ah-service snapcheck sample: id=%u house=%u"
               " item=%u count=%u buyout=%u\n",
               sample.id, sample.houseType, sample.itemId,
               sample.itemCount, sample.buyout);
    }

    if (!snap.Healthy())
    {
        fprintf(stderr, "ah-service snapcheck: DB unhealthy"
                        " (%u consecutive failures)\n",
                snap.ConsecutiveFailures());
        return 1;
    }

    printf("ah-service snapcheck OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Intent emission helpers (shared by dry-run and normal mode)
// ---------------------------------------------------------------------------

/// Map an EmittedIntent kind to its IPC opcode.
static uint16 IntentOpcode(const EmittedIntent& ei)
{
    switch (ei.kind)
    {
        case EmittedIntent::KIND_SELL:   return IPC_INTENT_SELL;
        case EmittedIntent::KIND_BID:    return IPC_INTENT_BID;
        case EmittedIntent::KIND_BUYOUT: return IPC_INTENT_BUYOUT;
        default:                         return 0;
    }
}

/// Log one intent (opcode + key fields), splitting the 64-bit uuid into its
/// run-id high word and sequence low word so the run-id stamping is visible.
static void LogIntent(const EmittedIntent& ei)
{
    switch (ei.kind)
    {
        case EmittedIntent::KIND_SELL:
            printf("  SellIntent   uuid=%08x:%08x bot=%u house=%u item=%u"
                   " stack=%u bid=%u buyout=%u durHrs=%u\n",
                   static_cast<unsigned>(ei.sell.uuid >> 32),
                   static_cast<unsigned>(ei.sell.uuid & 0xFFFFFFFFu),
                   ei.sell.botGuid, ei.sell.house, ei.sell.itemId,
                   ei.sell.stack, ei.sell.bid, ei.sell.buyout,
                   ei.sell.durationHrs);
            break;
        case EmittedIntent::KIND_BID:
            printf("  BidIntent    uuid=%08x:%08x bot=%u auction=%u"
                   " bidAmount=%u\n",
                   static_cast<unsigned>(ei.bid.uuid >> 32),
                   static_cast<unsigned>(ei.bid.uuid & 0xFFFFFFFFu),
                   ei.bid.botGuid, ei.bid.auctionId, ei.bid.bidAmount);
            break;
        case EmittedIntent::KIND_BUYOUT:
            printf("  BuyoutIntent uuid=%08x:%08x bot=%u auction=%u\n",
                   static_cast<unsigned>(ei.buyout.uuid >> 32),
                   static_cast<unsigned>(ei.buyout.uuid & 0xFFFFFFFFu),
                   ei.buyout.botGuid, ei.buyout.auctionId);
            break;
        default:
            break;
    }
}

/// Encode one intent into an IpcMessage body in wire order.
static void EncodeIntent(const EmittedIntent& ei, IpcMessage& msg)
{
    msg.op = static_cast<IpcOpcode>(IntentOpcode(ei));
    switch (ei.kind)
    {
        case EmittedIntent::KIND_SELL:   ei.sell.Encode(msg.body);   break;
        case EmittedIntent::KIND_BID:    ei.bid.Encode(msg.body);    break;
        case EmittedIntent::KIND_BUYOUT: ei.buyout.Encode(msg.body); break;
        default:                                                     break;
    }
}

// ---------------------------------------------------------------------------
// Dry-run: decisions without IPC
// ---------------------------------------------------------------------------

/**
 * @brief Foundation check for Task 8c.
 *
 * Loads config + pool + snapshot, resolves the bot GUID, then runs the bot's
 * rotated operations and LOGS the intents it would emit (without sending).
 * Uses a synthetic run-id (no IPC handshake) so the uuid stamping is still
 * exercised and visible. No DB mutation; no IPC.
 *
 * @param cfgPath Path to ah-service.conf (already validated non-null).
 * @return 0 on success, 1 on any failure.
 */
static int RunDryRun(const char* cfgPath)
{
    if (cfgPath == nullptr)
    {
        fprintf(stderr, "ah-service: --dryrun requires --config <path>\n");
        return 1;
    }

    if (!sConfig.SetSource(cfgPath))
    {
        fprintf(stderr, "ah-service: could not load config '%s'\n", cfgPath);
        return 1;
    }

    ServiceConfig config;
    if (!config.Initialize())
    {
        fprintf(stderr, "ah-service: ServiceConfig::Initialize failed\n");
        return 1;
    }

    ServiceDatabase db;
    if (!db.Init())
    {
        fprintf(stderr, "ah-service: ServiceDatabase::Init failed\n");
        return 1;
    }
    if (!db.InitCharacter())
    {
        fprintf(stderr,
                "ah-service: ServiceDatabase::InitCharacter failed\n");
        db.Shutdown();
        return 1;
    }

    ItemPool pool(config, db);
    if (!pool.Build())
    {
        fprintf(stderr, "ah-service: item pool empty - cannot dry-run\n");
        db.Shutdown();
        return 1;
    }

    MarketSnapshot snap(db);
    snap.Refresh();

    // Resolve the bot GUID from the character DB (escaped name lookup).
    const uint32 botGuid =
        db.ResolveCharacterGuid(config.GetBotCharacterName());
    printf("ah-service dryrun: bot character '%s' -> guid %u\n",
           config.GetBotCharacterName().c_str(), botGuid);
    if (botGuid == 0)
    {
        printf("ah-service dryrun: WARNING bot GUID unresolved -"
               " seller/buyer intents will be suppressed\n");
    }

    // Synthetic run-id for dry-run (normal mode uses IpcClient::RunId()).
    // A distinctive non-zero value so the run-id high word is obvious in the
    // logged uuids.
    const uint32 runId = 0xD00DBEEFu;

    BotBrain brain(config, pool, snap, botGuid, runId);
    brain.Initialize();

    printf("ah-service dryrun: seller=%s buyer=%s run-id=0x%08x\n",
           brain.SellerEnabled() ? "on" : "off",
           brain.BuyerEnabled() ? "on" : "off",
           runId);

    // Run a full rotation (2 * MAX_HOUSE operations) so every house's
    // seller AND buyer step is exercised in one dry-run.
    uint32 totalIntents = 0;
    for (uint32 step = 0; step < 2 * AH_MAX_AUCTION_HOUSE_TYPE; ++step)
    {
        std::vector<EmittedIntent> intents;
        brain.RunOneOperation(intents);
        if (!intents.empty())
        {
            printf("ah-service dryrun: operation produced %u intent(s):\n",
                   static_cast<unsigned>(intents.size()));
            for (size_t i = 0; i < intents.size(); ++i)
            {
                LogIntent(intents[i]);
                ++totalIntents;
            }
        }
    }

    db.Shutdown();

    printf("ah-service dryrun OK (%u intents logged)\n", totalIntents);
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Normal child loop
// ---------------------------------------------------------------------------

static void PrintUsage(const char* argv0)
{
    fprintf(stderr,
            "Usage: %s --port <port> --secret <secret>"
            " [--botguid <guid>] [--config <path>]\n"
            "       %s --selftest\n"
            "       %s --poolcheck --config <path>\n"
            "       %s --snapcheck --config <path>\n"
            "       %s --dryrun --config <path>\n",
            argv0, argv0, argv0, argv0, argv0);
}

int main(int argc, char** argv)
{
    printf("ah-service (ipc proto v%u) starting\n", IPC_PROTOCOL_VERSION);

    // --- Parse arguments ---
    bool selfTest  = false;
    bool poolCheck = false;
    bool snapCheck = false;
    bool dryRun    = false;
    uint16 port   = 0;
    const char* secret  = nullptr;
    const char* cfgPath = nullptr;
    // botGuid is the AUTHORITATIVE bot identity resolved by mangosd. It is
    // parsed here and stamped onto every emitted intent so the executor's
    // GUID guard can never silently reject (and silently stall) the bot.
    bool   botGuidGiven = false;
    uint32 argBotGuid   = 0;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--selftest") == 0)
        {
            selfTest = true;
        }
        else if (strcmp(argv[i], "--poolcheck") == 0)
        {
            poolCheck = true;
        }
        else if (strcmp(argv[i], "--snapcheck") == 0)
        {
            snapCheck = true;
        }
        else if (strcmp(argv[i], "--dryrun") == 0)
        {
            dryRun = true;
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            port = static_cast<uint16>(atoi(argv[++i]));
        }
        else if (strcmp(argv[i], "--secret") == 0 && i + 1 < argc)
        {
            secret = argv[++i];
        }
        else if (strcmp(argv[i], "--botguid") == 0 && i + 1 < argc)
        {
            argBotGuid   = static_cast<uint32>(strtoul(argv[++i], nullptr, 10));
            botGuidGiven = true;
        }
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            cfgPath = argv[++i];
        }
    }

    if (selfTest)
    {
        int rc = RunIntentCodecSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        return RunSelfTest();
    }

    if (poolCheck)
    {
        return RunPoolCheck(cfgPath);
    }

    if (snapCheck)
    {
        return RunSnapCheck(cfgPath);
    }

    if (dryRun)
    {
        return RunDryRun(cfgPath);
    }

    // --- Resolve the shared secret (C4: env first, then --secret) ---
    // The supervisor passes the secret OUT-OF-BAND in AH_SERVICE_SECRET so it
    // never appears on the child argv (readable via /proc/<pid>/cmdline or the
    // Win32 command line by any local account). --secret remains a manual-
    // testing fallback only, used when the env var is absent.
    const char* envSecret = getenv("AH_SERVICE_SECRET");
    if (envSecret != nullptr && envSecret[0] != '\0')
    {
        secret = envSecret;
    }

    if (port == 0 || secret == nullptr)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    // --- C3: --botguid is the AUTHORITATIVE bot identity ---
    // mangosd resolved this (sAuctionBotConfig.GetAHBotId()). If it is 0 the
    // realm has no valid bot character, so the child cannot function: every
    // intent would be rejected by the executor's GetAHBotId()==0 guard while
    // the in-process fallback stays suppressed. Exit non-zero so the operator
    // sees the misconfiguration rather than a silent total stall.
    if (!botGuidGiven || argBotGuid == 0)
    {
        fprintf(stderr, "ah-service: --botguid is 0 or missing - mangosd has"
                        " no valid AH bot character; exiting (cannot emit"
                        " intents that pass the executor GUID guard)\n");
        return 1;
    }

    // --- Install parent-death guard (POSIX: prctl SIGTERM; Windows: no-op) ---
    Console_InstallParentDeathGuard();

    // --- Load config (C1: REQUIRED before the handshake) ---
    // The infra config (DB strings, ahbot.conf path, cadence, Console toggle)
    // must load before any bot setup. A missing --config or unreadable file
    // means the child can do nothing useful, so exit non-zero rather than
    // heartbeat as a hollow "ready" service that emits nothing.
    if (cfgPath == nullptr)
    {
        fprintf(stderr, "ah-service: --config is required in normal mode -"
                        " exiting\n");
        return 1;
    }
    if (!sConfig.SetSource(cfgPath))
    {
        fprintf(stderr, "ah-service: could not load config '%s' - exiting\n",
                cfgPath);
        return 1;
    }

    const bool showConsole =
        sConfig.GetBoolDefault("Console.ShowOnStartup", false);
    Console_Show(showConsole);

    // Dry-run can also be requested via config (logs instead of sends).
    const bool emitDryRun = sConfig.GetBoolDefault("AhBot.DryRun", false);
    // Bot tick cadence: matches the in-process WUPDATE_AHBOT (20s); override
    // via config for testing.
    const uint32 tickIntervalMs = static_cast<uint32>(
        sConfig.GetIntDefault("AhBot.UpdateIntervalMs", 20000));

    // -------------------------------------------------------------------
    // C1: ALL required bot setup runs BEFORE the IPC handshake so that
    // "READY" genuinely means the bot is OPERATIONAL. The supervisor uses
    // ServiceActive() (READY + heartbeats) to SUPPRESS the in-process bot;
    // if any of this failed after READY, mangosd would see a live service
    // that emits nothing and BOTH bots would be silent. On ANY failure
    // here we log and exit non-zero; the supervisor backs off + restarts.
    //
    // The only thing NOT available yet is the per-spawn run-id (assigned by
    // mangosd in the handshake). It is used solely for uuid stamping in the
    // loop, so the BotBrain is constructed just after Connect() with it; no
    // failable setup depends on it.
    // -------------------------------------------------------------------
    ServiceConfig   botConfig;
    ServiceDatabase botDb;

    if (!botConfig.Initialize())
    {
        fprintf(stderr, "ah-service: ServiceConfig::Initialize failed -"
                        " exiting (cannot run bot)\n");
        return 1;
    }

    if (!botDb.Init() || !botDb.InitCharacter())
    {
        fprintf(stderr, "ah-service: bot DB init failed -"
                        " exiting (cannot run bot)\n");
        botDb.Shutdown();
        return 1;
    }

    // C3: the parent-passed --botguid is AUTHORITATIVE. Re-resolve from the
    // child's own character DB ONLY as a drift diagnostic: if it differs from
    // mangosd's value we log loudly but still STAMP intents with the parent
    // guid (argBotGuid), so config drift can never silently fail the
    // executor's GUID guard. We already rejected argBotGuid == 0 above.
    const uint32 botGuid = argBotGuid;
    const uint32 resolvedGuid =
        botDb.ResolveCharacterGuid(botConfig.GetBotCharacterName());
    if (resolvedGuid != botGuid)
    {
        fprintf(stderr, "ah-service: WARNING bot GUID DRIFT - mangosd"
                        " --botguid=%u but child resolved '%s' -> %u;"
                        " using AUTHORITATIVE parent guid %u for all intents\n",
                botGuid, botConfig.GetBotCharacterName().c_str(),
                resolvedGuid, botGuid);
    }

    // Build the item pool (C1: REQUIRED). An empty pool means the seller has
    // nothing to list; consistent with --poolcheck / --dryrun, treat it as a
    // setup failure and exit so the supervisor restarts (the misconfig is
    // visible rather than a hollow ready service).
    ItemPool* botPool = new ItemPool(botConfig, botDb);
    if (!botPool->Build())
    {
        fprintf(stderr, "ah-service: item pool empty -"
                        " exiting (seller has no items)\n");
        delete botPool;
        botDb.Shutdown();
        return 1;
    }

    MarketSnapshot* botSnap = new MarketSnapshot(botDb);
    botSnap->Refresh();

    // --- Normal mode: connect to mangosd IPC server ---
    // Setup above succeeded, so completing the handshake (READY) now truthfully
    // advertises an OPERATIONAL bot.
    IpcClient cli;
    if (!cli.Connect("127.0.0.1", port, secret))
    {
        fprintf(stderr, "ah-service: IpcClient::Connect failed\n");
        delete botSnap;
        delete botPool;
        botDb.Shutdown();
        return 1;
    }

    printf("ah-service: connecting to mangosd on port %u\n", port);

    // Wait for handshake (up to 10 s).
    const int handshakeTimeoutMs = 10000;
    int waited = 0;
    while (!cli.Connected())
    {
        if (waited >= handshakeTimeoutMs)
        {
            fprintf(stderr, "ah-service: handshake timed out - exiting\n");
            cli.Stop();
            delete botSnap;
            delete botPool;
            botDb.Shutdown();
            return 1;
        }
        ACE_Based::Thread::Sleep(50);
        waited += 50;
    }

    printf("ah-service: handshake complete - run-id %u - entering service loop\n",
           cli.RunId());

    // Brain construction needs the handshake-assigned run-id (uuid high word);
    // it performs no DB/IO and cannot fail.
    BotBrain* botBrain = new BotBrain(botConfig, *botPool, *botSnap, botGuid,
                                      cli.RunId());
    botBrain->Initialize();
    printf("ah-service: bot ready (guid=%u seller=%s buyer=%s dryrun=%s"
           " tick=%ums)\n",
           botGuid,
           botBrain->SellerEnabled() ? "on" : "off",
           botBrain->BuyerEnabled() ? "on" : "off",
           emitDryRun ? "on" : "off", tickIntervalMs);

    // --- Service loop ---
    volatile bool stop = false;
    uint32 sinceTickMs = 0;       ///< Accumulator toward the bot cadence.
    bool   backoffNext = false;   ///< Skip next tick after IPC_QUEUE_FULL.

    while (!stop)
    {
        IpcMessage msg;
        while (cli.PopInbound(msg))
        {
            switch (msg.op)
            {
                case IPC_HEARTBEAT:
                {
                    IpcMessage ack;
                    ack.op = IPC_HEARTBEAT_ACK;
                    cli.SendFrame(ack);
                    break;
                }
                case IPC_ECHO:
                {
                    IpcMessage reply;
                    reply.op   = IPC_ECHO_REPLY;
                    reply.body = msg.body;
                    cli.SendFrame(reply);
                    break;
                }
                case IPC_CONSOLE:
                {
                    uint8 show = 0;
                    if (msg.body.size() >= 1)
                    {
                        msg.body >> show;
                    }
                    Console_Show(show != 0);
                    break;
                }
                case IPC_SHUTDOWN:
                {
                    printf("ah-service: received IPC_SHUTDOWN - exiting\n");
                    IpcMessage ack;
                    ack.op = IPC_SHUTDOWN_ACK;
                    cli.SendFrame(ack);
                    stop = true;
                    break;
                }
                case IPC_INTENT_RESULT:
                {
                    IntentResult res;
                    if (res.Decode(msg.body))
                    {
                        printf("ah-service: intent result uuid=%08x:%08x"
                               " status=%u reason=%u\n",
                               static_cast<unsigned>(res.uuid >> 32),
                               static_cast<unsigned>(res.uuid & 0xFFFFFFFFu),
                               res.status, res.reason);
                    }
                    break;
                }
                case IPC_QUEUE_FULL:
                {
                    // mangosd's apply queue is full: back off one tick.
                    printf("ah-service: IPC_QUEUE_FULL - backing off one"
                           " bot cycle\n");
                    backoffNext = true;
                    break;
                }
                case IPC_GAMETIME:
                    // Deliberate no-op: mangosd sends gametime each heartbeat;
                    // BotBrain uses time(NULL) directly, so we intentionally
                    // ignore this opcode.  An explicit case keeps the default
                    // warning reserved for genuinely unknown opcodes.
                    break;
                case IPC_GMCMD:
                {
                    GmCmd gc;
                    gc.cmd = 0;
                    uint8 ok = 0;
                    bool decoded = gc.Decode(msg.body);
                    if (decoded)
                    {
                        if (gc.cmd == static_cast<uint8>(GMCMD_RELOAD))
                        {
                            if (botConfig.Reload())
                            {
                                if (botBrain != nullptr)
                                {
                                    botBrain->Reinitialize();
                                    ok = 1u;
                                }
                                else
                                {
                                    printf("ah-service: GMCMD_RELOAD config"
                                           " loaded but brain absent -"
                                           " reporting failure\n");
                                }
                            }
                            printf("ah-service: GMCMD_RELOAD %s\n",
                                   ok ? "OK" : "FAILED");
                        }
                        else
                        {
                            printf("ah-service: IPC_GMCMD unknown cmd %u"
                                   " - replying FAIL\n",
                                   static_cast<unsigned>(gc.cmd));
                        }
                    }
                    else
                    {
                        printf("ah-service: IPC_GMCMD decode failed\n");
                    }
                    // Always reply so mangosd can log the result.
                    GmCmdResult gcr;
                    gcr.cmd = gc.cmd;
                    gcr.ok  = ok;
                    IpcMessage reply;
                    reply.op = IPC_GMCMD_RESULT;
                    gcr.Encode(reply.body);
                    cli.SendFrame(reply);
                    break;
                }
                default:
                    printf("ah-service: unhandled IPC opcode %u"
                           " - ignored\n",
                           static_cast<unsigned>(msg.op));
                    break;
            }
        }

        if (!cli.Connected())
        {
            fprintf(stderr, "ah-service: connection lost - exiting\n");
            stop = true;
            break;
        }

        // --- Bot cadence tick ---
        if (botBrain != nullptr && !stop)
        {
            sinceTickMs += 10;
            if (sinceTickMs >= tickIntervalMs)
            {
                sinceTickMs = 0;

                if (backoffNext)
                {
                    // Skip this cycle once after an IPC_QUEUE_FULL.
                    backoffNext = false;
                }
                else
                {
                    // Refresh the market view, then run ONE rotated op.
                    botSnap->Refresh();
                    if (botSnap->Healthy())
                    {
                        std::vector<EmittedIntent> intents;
                        botBrain->RunOneOperation(intents);

                        for (size_t i = 0; i < intents.size(); ++i)
                        {
                            if (emitDryRun)
                            {
                                LogIntent(intents[i]);
                            }
                            else
                            {
                                IpcMessage out;
                                EncodeIntent(intents[i], out);
                                cli.SendFrame(out);
                            }
                        }

                        if (!intents.empty())
                        {
                            printf("ah-service: bot tick emitted %u"
                                   " intent(s)%s\n",
                                   static_cast<unsigned>(intents.size()),
                                   emitDryRun ? " (dry-run, logged)" : "");
                        }
                    }
                    else
                    {
                        fprintf(stderr, "ah-service: snapshot unhealthy"
                                        " (%u failures) - not emitting\n",
                                botSnap->ConsecutiveFailures());
                    }
                }
            }
        }

        ACE_Based::Thread::Sleep(10);
    }

    delete botBrain;
    delete botSnap;
    delete botPool;
    botDb.Shutdown();

    cli.Stop();
    printf("ah-service: shutdown complete\n");
    return 0;
}
