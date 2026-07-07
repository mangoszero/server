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
#include "IpcClientHandler.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "IpcReliable.h"
#include "AuctionIntents.h"
#include "PlayerMutations.h"
#include "BrowseMessages.h"
#include "BrowseHandler.h"
#include "Usability.h"
#include "Threading/Threading.h"
#include "Console.h"
#include "Config/Config.h"
#include "ServiceConfig.h"
#include "ServiceDatabase.h"
#include "Journal.h"
#include "ItemPool.h"
#include "MarketSnapshot.h"
#include "BotBrain.h"
#include "ItemInstanceFields.h"
#include "AuctionBook.h"
#include "MutationHandler.h"

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
        a.uuid     = UINT64_C(0xDEADBEEF00000004);
        a.status   = static_cast<uint8>(INTENT_REJECTED);
        a.reason   = static_cast<uint8>(REASON_NO_FUNDS);
        a.itemGuid = 0u;
        a.auctionId = 0u;

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

    // --- BrowseQuery (variable-length: profile + name + lists + recipe set) ---
    {
        BrowseQuery a;
        a.queryId          = UINT64_C(0x00000001DEADBEEF);
        a.kind             = static_cast<uint8>(BROWSE_LIST);
        a.house            = 2u;
        a.allHouses        = 0u;
        a.itemClass        = 4u;
        a.itemSubClass     = 0xFFFFFFFFu;
        a.inventoryType    = 0xFFFFFFFFu;
        a.quality          = 3u;
        a.levelmin         = 10u;
        a.levelmax         = 60u;
        a.usable           = 1u;
        a.deferEluna       = 1u;
        a.listfrom         = 50u;
        a.localeIndex      = 2;             // V3: a real LocaleConstant (frFR) wire value (v4-verify R1)
        a.requesterGuidLow = 777u;
        a.minMountLevel    = 40u;
        a.minEpicMountLevel= 60u;
        a.searchedName     = "Thunderfury";
        a.outbidIds.push_back(101u);
        a.outbidIds.push_back(202u);
        a.knownRecipeCastSpells.push_back(7411u);
        a.profile.classId   = 1u;
        a.profile.raceId    = 1u;
        a.profile.level     = 60u;
        a.profile.honorRank = 6u;
        SkillRank sr; sr.skillId = 43u; sr.rank = 300u;
        a.profile.skills.push_back(sr);
        a.profile.knownSpells.push_back(12345u);
        RepStanding rs; rs.factionId = 21u; rs.rank = 5u;
        a.profile.reps.push_back(rs);

        ByteBuffer buf;
        a.Encode(buf);
        BrowseQuery b;
        if (!b.Decode(buf))
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseQuery::Decode false\n");
            return 1;
        }
        if (buf.rpos() != buf.size())
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseQuery trailing bytes\n");
            return 1;
        }
        if (b.queryId != a.queryId || b.kind != a.kind || b.house != a.house ||
            b.allHouses != a.allHouses || b.itemClass != a.itemClass ||
            b.itemSubClass != a.itemSubClass || b.inventoryType != a.inventoryType ||
            b.quality != a.quality || b.levelmin != a.levelmin ||
            b.levelmax != a.levelmax || b.usable != a.usable ||
            b.deferEluna != a.deferEluna || b.listfrom != a.listfrom ||
            b.localeIndex != a.localeIndex || b.requesterGuidLow != a.requesterGuidLow ||
            b.minMountLevel != a.minMountLevel || b.minEpicMountLevel != a.minEpicMountLevel ||
            b.searchedName != a.searchedName ||
            b.outbidIds.size() != 2u || b.knownRecipeCastSpells.size() != 1u ||
            b.profile.honorRank != a.profile.honorRank ||
            b.profile.skills.size() != 1u || b.profile.knownSpells.size() != 1u ||
            b.profile.reps.size() != 1u)
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseQuery field mismatch\n");
            return 1;
        }
        if (b.outbidIds[0] != 101u || b.outbidIds[1] != 202u ||
            b.knownRecipeCastSpells[0] != 7411u ||
            b.profile.skills[0].skillId != 43u || b.profile.skills[0].rank != 300u ||
            b.profile.knownSpells[0] != 12345u ||
            b.profile.reps[0].factionId != 21u || b.profile.reps[0].rank != 5u)
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseQuery list-element mismatch\n");
            return 1;
        }
    }

    // --- BrowseResult (variable-length: N entries; elunaPending/tooMany) ---
    {
        BrowseResult a;
        a.queryId      = UINT64_C(0x00000001DEADBEEF);
        a.kind         = static_cast<uint8>(BROWSE_LIST);
        a.elunaPending = 1u;
        a.tooMany      = 0u;
        a.totalcount   = 7u;
        BrowseEntry e;
        e.id=5u; e.itemEntry=19019u; e.enchantId=2u; e.randomPropId=0u;
        e.suffixFactor=0u; e.count=1u; e.charges=-1; e.ownerGuidLow=4u;
        e.startbid=100u; e.outbid=5u; e.buyout=5000u; e.timeLeftMs=720000u;
        e.bidderGuidLow=9u; e.curBid=120u;
        a.entries.push_back(e);

        ByteBuffer buf;
        a.Encode(buf);
        BrowseResult b;
        if (!b.Decode(buf))
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseResult::Decode false\n");
            return 1;
        }
        if (buf.rpos() != buf.size())
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseResult trailing bytes\n");
            return 1;
        }
        if (b.queryId != a.queryId || b.kind != a.kind ||
            b.elunaPending != 1u || b.tooMany != 0u ||
            b.totalcount != 7u || b.entries.size() != 1u || b.Count() != 1u)
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseResult header mismatch\n");
            return 1;
        }
        const BrowseEntry& g = b.entries[0];
        if (g.id!=5u || g.itemEntry!=19019u || g.enchantId!=2u || g.count!=1u ||
            g.charges!=-1 || g.ownerGuidLow!=4u || g.startbid!=100u || g.outbid!=5u ||
            g.buyout!=5000u || g.timeLeftMs!=720000u || g.bidderGuidLow!=9u || g.curBid!=120u)
        {
            fprintf(stderr, "intent codec selftest FAILED: BrowseEntry field mismatch\n");
            return 1;
        }
    }

    // --- Strict decode: declared entry-count exceeding the bytes present must
    //     be rejected, not silently truncated (I7). ---
    {
        ByteBuffer bad;
        bad << UINT64_C(1) << uint8(BROWSE_LIST) << uint8(0) << uint8(0) << uint8(0)
            << uint32(0) /*totalcount*/ << uint32(3) /*claims 3, supplies 0*/;
        BrowseResult b;
        if (b.Decode(bad))
        {
            fprintf(stderr, "intent codec selftest FAILED: oversized entry-count accepted\n");
            return 1;
        }
    }

    // --- Strict decode: invalid kind rejected. ---
    {
        ByteBuffer bad;
        bad << UINT64_C(1) << uint8(BROWSE_KIND_MAX) << uint8(0) << uint8(0) << uint8(0)
            << uint32(0) << uint32(0);
        BrowseResult b;
        if (b.Decode(bad))
        {
            fprintf(stderr, "intent codec selftest FAILED: invalid kind accepted\n");
            return 1;
        }
    }

    // --- I8: worker rejects oversize / unknown inbound frames before staging ---
    {
        // A BROWSE_QUERY just over its MAXLEN must be rejected.
        if (IpcClientHandler::InboundFrameAcceptable(
                IPC_BROWSE_QUERY, uint32(BrowseQuery::MAX_WIRE) + 1u))
        {
            fprintf(stderr, "intent codec selftest FAILED: oversize browse frame accepted\n");
            return 1;
        }
        // A well-sized BROWSE_RESULT must be accepted.
        if (!IpcClientHandler::InboundFrameAcceptable(IPC_BROWSE_RESULT, 64u))
        {
            fprintf(stderr, "intent codec selftest FAILED: valid browse frame rejected\n");
            return 1;
        }
        // An unknown opcode must be rejected.
        if (IpcClientHandler::InboundFrameAcceptable(0xFFFFu, 0u))
        {
            fprintf(stderr, "intent codec selftest FAILED: unknown opcode accepted\n");
            return 1;
        }
    }

    // --- item_instance.data blob decode (full-parity seam) ---
    {
        // Build a synthetic blob long enough to index word 44. Fill ascending
        // word values so each decoded field has a distinct, checkable number,
        // then override the specific fields we read.
        std::string blob;
        for (uint32 w = 0; w < 60u; ++w)
        {
            char num[16];
            // word value = w*1000 so it is unmistakable; overrides below.
            snprintf(num, sizeof(num), "%u", w * 1000u);
            if (w)
            {
                blob += ' ';
            }
            blob += num;
        }
        // Overrides at the exact offsets:
        //   word 14 stack=5, 16 charges=-3, 22 enchant=2564, 43 suffix=77,
        //   44 randomprop = (uint32)(-9) to test signed read.
        // Rebuild with overrides for clarity.
        std::string words[60];
        for (uint32 w = 0; w < 60u; ++w)
        {
            char num[16];
            snprintf(num, sizeof(num), "%u", w * 1000u);
            words[w] = num;
        }
        words[14] = "5";
        words[16] = "-3";
        words[22] = "2564";
        words[43] = "77";
        words[44] = "-9";
        blob.clear();
        for (uint32 w = 0; w < 60u; ++w)
        {
            if (w)
            {
                blob += ' ';
            }
            blob += words[w];
        }

        ItemInstanceFields f = AhItemBlob::Decode(blob);
        if (!f.valid || f.stackCount != 5u || f.charges != -3 ||
            f.enchantId != 2564u || f.suffixFactor != 77u || f.randomPropId != -9)
        {
            fprintf(stderr, "item blob selftest FAILED:"
                    " valid=%d stack=%u charges=%d ench=%u suffix=%u rand=%d\n",
                    f.valid ? 1 : 0, unsigned(f.stackCount), int(f.charges),
                    unsigned(f.enchantId), unsigned(f.suffixFactor),
                    int(f.randomPropId));
            return 1;
        }
        // A blob too short to reach word 44 must be marked invalid (no OOB).
        ItemInstanceFields g = AhItemBlob::Decode("1 2 3");
        if (g.valid)
        {
            fprintf(stderr, "item blob selftest FAILED: short blob marked valid\n");
            return 1;
        }
    }

    // --- FilterAndPaginate: name filter + page slice + totalcount + defer ---
    {
        std::vector<BrowseRow> rows;
        for (uint32 i = 0; i < 120; ++i)
        {
            BrowseRow r;
            r.entry = BrowseEntry();
            r.entry.id        = i + 1;
            r.entry.itemEntry = 1000u + i;
            r.itemClass = 4u; r.itemSubClass = 0u; r.inventoryType = 0u;
            r.quality = 2u; r.requiredLevel = 1u;
            r.allowableClass = 0xFFFFFFFFu; r.allowableRace = 0xFFFFFFFFu;
            r.name = (i % 2 == 0) ? "Iron Sword" : "Copper Axe";
            r.reqSkill = 0u; r.reqSkillRank = 0u; r.reqSpell = 0u;
            r.reqHonorRank = 0u; r.reqRepFaction = 0u; r.reqRepRank = 0u;
            r.itemProficiencySkill = 0u; r.castSpellId = 0u;
            rows.push_back(r);
        }

        BrowseQuery q;
        q.queryId = 1u; q.kind = static_cast<uint8>(BROWSE_LIST);
        q.house = 0u; q.allHouses = 0u;
        q.itemClass = 0xFFFFFFFFu; q.itemSubClass = 0xFFFFFFFFu;
        q.inventoryType = 0xFFFFFFFFu; q.quality = 0xFFFFFFFFu;
        q.levelmin = 0u; q.levelmax = 0u; q.usable = 0u; q.deferEluna = 0u;
        q.listfrom = 50u; q.localeIndex = 0; q.requesterGuidLow = 0u;   // 0 = enUS / no overlay (V3)
        q.searchedName = "sword";
        // A capable profile so the usable filter (RowUsable -> AhUsability::IsUsable)
        // is deterministic. The test rows require only level 1 and allow all
        // classes, so any valid class at level 60 makes every survivor usable.
        // Without this the profile scalars + mount levels are read UNINITIALIZED on
        // the usable=1 path -- harmless on MSVC, garbage on GCC/Clang (all rows
        // filtered -> defer set size 0). (minMountLevel/minEpicMountLevel now
        // default to 0 in BrowseQuery; the profile scalars default to 0 too.)
        q.profile.classId = 1u;   // any valid class; rows are allowableClass = all
        q.profile.raceId  = 1u;
        q.profile.level   = 60u;

        BrowseResult res = BrowseHandler::FilterAndPaginate(rows, q);
        if (res.totalcount != 60u || res.elunaPending != 0u || res.tooMany != 0u)
        {
            fprintf(stderr, "browse selftest FAILED: totalcount=%u (exp 60)\n",
                    unsigned(res.totalcount));
            return 1;
        }
        if (res.Count() != 10u || res.entries.size() != 10u)
        {
            fprintf(stderr, "browse selftest FAILED: page count=%u (exp 10)\n",
                    unsigned(res.Count()));
            return 1;
        }
        if (res.entries[0].id != 101u)   // 51st "sword" row = even index 100, id 101
        {
            fprintf(stderr, "browse selftest FAILED: first paged id=%u (exp 101)\n",
                    unsigned(res.entries[0].id));
            return 1;
        }

        // deferEluna: all 60 survivors returned un-paginated, elunaPending=1.
        BrowseQuery qd = q; qd.deferEluna = 1u; qd.usable = 1u; qd.listfrom = 0u;
        BrowseResult rd = BrowseHandler::FilterAndPaginate(rows, qd);
        if (rd.elunaPending != 1u || rd.tooMany != 0u ||
            rd.totalcount != 60u || rd.entries.size() != 60u)
        {
            fprintf(stderr, "browse selftest FAILED: defer set size=%u (exp 60)\n",
                    unsigned(rd.entries.size()));
            return 1;
        }

        // OWNER: no filters/pagination -> all 120 rows.
        BrowseQuery qo = q;
        qo.kind = static_cast<uint8>(BROWSE_OWNER);
        qo.searchedName.clear(); qo.listfrom = 0u;
        BrowseResult ro = BrowseHandler::FilterAndPaginate(rows, qo);
        if (ro.Count() != 120u || ro.totalcount != 120u || ro.entries.size() != 120u)
        {
            fprintf(stderr, "browse selftest FAILED: owner count=%u (exp 120)\n",
                    unsigned(ro.Count()));
            return 1;
        }

        // Over-cap deferred-Eluna (decision #2): >cap survivors -> the worker
        // declines with tooMany (no entries) so mangosd sends "AH unavailable",
        // rather than serving an approximate short page.
        std::vector<BrowseRow> many;
        for (uint32 i = 0; i < 1200u; ++i)
        {
            BrowseRow r;
            r.entry = BrowseEntry();
            r.entry.id        = i + 1;
            r.entry.itemEntry = 1000u + i;
            r.itemClass = 4u; r.itemSubClass = 0u; r.inventoryType = 0u;
            r.quality = 2u; r.requiredLevel = 1u;
            r.allowableClass = 0xFFFFFFFFu; r.allowableRace = 0xFFFFFFFFu;
            r.name = "Iron Sword";
            r.reqSkill = 0u; r.reqSkillRank = 0u; r.reqSpell = 0u;
            r.reqHonorRank = 0u; r.reqRepFaction = 0u; r.reqRepRank = 0u;
            r.itemProficiencySkill = 0u; r.castSpellId = 0u;
            many.push_back(r);
        }
        BrowseQuery qbig = q;          // LIST, searchedName "sword" (matches all)
        qbig.deferEluna = 1u; qbig.usable = 0u; qbig.listfrom = 100u;
        BrowseResult rbig = BrowseHandler::FilterAndPaginate(many, qbig);
        if (rbig.tooMany != 1u || rbig.elunaPending != 0u ||
            rbig.entries.size() != 0u || rbig.totalcount != 0u)
        {
            fprintf(stderr, "browse selftest FAILED: over-cap should decline with"
                    " tooMany (tm=%u ep=%u n=%u total=%u)\n",
                    unsigned(rbig.tooMany), unsigned(rbig.elunaPending),
                    unsigned(rbig.entries.size()), unsigned(rbig.totalcount));
            return 1;
        }
    }

    // --- AhUsability::IsUsable verdicts (full parity minus Eluna) ---
    {
        PlayerProfile warrior;
        warrior.classId = 1u; warrior.raceId = 1u; warrior.level = 40u;
        warrior.honorRank = 2u;
        SkillRank swords; swords.skillId = 43u; swords.rank = 200u;
        warrior.skills.push_back(swords);
        RepStanding rep; rep.factionId = 609u; rep.rank = 5u; // Honored
        warrior.reps.push_back(rep);

        ItemUsabilityReq ok;
        ok.itemClass = 2u; ok.allowableClass = 0xFFFFFFFFu; ok.allowableRace = 0xFFFFFFFFu;
        ok.requiredLevel = 30u; ok.itemId = 12345u;
        ok.requiredSkill = 43u; ok.requiredSkillRank = 150u; ok.requiredSpell = 0u;
        ok.requiredHonorRank = 0u; ok.requiredRepFaction = 0u; ok.requiredRepRank = 0u;
        ok.itemProficiencySkill = 43u;
        const uint32 MM = 40u, EM = 60u;

        if (!AhUsability::IsUsable(warrior, ok, MM, EM))
        { fprintf(stderr, "usability FAILED: baseline\n"); return 1; }

        ItemUsabilityReq c;
        c = ok; c.allowableClass = 0x80u;       // mage-only
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: class gate\n"); return 1; }
        c = ok; c.allowableRace = 0x02u;        // orc-only
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: race gate\n"); return 1; }
        c = ok; c.requiredLevel = 60u;
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: level gate\n"); return 1; }
        c = ok; c.requiredSkill = 44u;          // skill absent
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: skill-missing\n"); return 1; }
        c = ok; c.requiredSkillRank = 300u;
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: skill-rank\n"); return 1; }
        c = ok; c.requiredSpell = 999u;
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: required-spell\n"); return 1; }
        c = ok; c.requiredHonorRank = 5u;       // player has rank 2
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: honor gate\n"); return 1; }
        c = ok; c.itemProficiencySkill = 44u;   // proficiency absent
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: proficiency\n"); return 1; }
        c = ok; c.requiredRepFaction = 609u; c.requiredRepRank = 7u; // need Exalted, have 5
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: reputation gate\n"); return 1; }
        c = ok; c.requiredRepFaction = 609u; c.requiredRepRank = 4u; // need 4, have 5
        if (!AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: reputation pass\n"); return 1; }

        // Mount-level override: a regular-mount itemId (1132) with a high base
        // RequiredLevel must drop to MM(40). Player level 40 -> usable.
        c = ok; c.itemId = 1132u; c.requiredLevel = 60u;
        if (!AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: regular-mount override\n"); return 1; }
        // Epic-mount itemId (12302) -> EM(60). Player 40 -> not usable.
        c = ok; c.itemId = 12302u; c.requiredLevel = 30u;
        if (AhUsability::IsUsable(warrior, c, MM, EM))
        { fprintf(stderr, "usability FAILED: epic-mount override\n"); return 1; }

        // D6/V1: proficiency-skill table mirrors Item::GetSkill().
        if (AhUsability::GetItemProficiencySkill(2u, 7u) != 43u ||  // 1H sword
            AhUsability::GetItemProficiencySkill(4u, 1u) != 415u || // cloth
            AhUsability::GetItemProficiencySkill(4u, 6u) != 433u || // shield
            AhUsability::GetItemProficiencySkill(2u, 17u) != 253u|| // V1: SPEAR->ASSASSINATION (not 633)
            AhUsability::GetItemProficiencySkill(0u, 0u) != 0u   || // non-equip
            AhUsability::GetItemProficiencySkill(2u, 99u) != 0u)    // OOB subclass
        { fprintf(stderr, "usability FAILED: proficiency table\n"); return 1; }
    }

    printf("intent codec selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 authoritative book (load gates, re-mark, admission)
// ---------------------------------------------------------------------------

/**
 * @brief Build one clean RawAuctionRow for book/mutation selftests.
 *
 * itemGuid = 5000 + id; item 2589 x20, startbid 100, buyout 50000, deposit 15,
 * no bidder. Shared by RunBookSelfTest and the Task 5/6 mutation selftests.
 */
static RawAuctionRow MakeRawRow(uint32 id, uint8 houseId, uint32 owner)
{
    RawAuctionRow r;
    r.row.id               = id;
    r.row.houseId          = houseId;
    r.row.itemGuid         = 5000u + id;
    r.row.itemTemplate     = 2589u;
    r.row.itemCount        = 20u;
    r.row.randomPropertyId = 0;
    r.row.owner            = owner;
    r.row.buyout           = 50000u;
    r.row.expireTime       = 2000000000u;
    r.row.bidder           = 0u;
    r.row.bid              = 0u;
    r.row.startbid         = 100u;
    r.row.deposit          = 15u;
    r.row.state            = BOOK_LIVE;
    r.itemExists   = true;
    r.blobValid    = true;
    r.itemEntry    = 2589u;
    r.itemStack    = 20u;
    r.itemRandProp = 0;
    return r;
}

/**
 * @brief Pure in-memory book tests: load gates 1-4, journal re-mark,
 *        one-ACTIVE invariant, the spec 4.3b admission matrix, house
 *        grouping for the 50-cap, and the memory mutators/rollbacks.
 *
 * @return 0 on success, 1 on any failure.
 */
static int RunBookSelfTest()
{
    // --- ItemInstanceFields: new entry field (word 3, OBJECT_FIELD_ENTRY) ---
    {
        std::string blob;
        for (int w = 0; w < 45; ++w)
        {
            char buf[16];
            sprintf(buf, "%d", w * 10);
            if (w != 0)
            {
                blob += " ";
            }
            blob += buf;
        }
        ItemInstanceFields f = AhItemBlob::Decode(blob);
        if (!f.valid || f.entry != 30u)
        {
            fprintf(stderr, "book selftest FAILED: blob entry decode\n");
            return 1;
        }
    }

    // --- load gating + gate-C adoption ---
    {
        std::vector<RawAuctionRow> rows;
        rows.push_back(MakeRawRow(1u, 1u, 100u));            // clean
        RawAuctionRow noItem = MakeRawRow(2u, 1u, 100u);     // gate A: no item row
        noItem.itemExists = false;
        rows.push_back(noItem);
        RawAuctionRow badBlob = MakeRawRow(3u, 1u, 100u);    // gate A: undecodable blob
        badBlob.blobValid = false;
        rows.push_back(badBlob);
        rows.push_back(MakeRawRow(4u, 9u, 100u));            // gate B: houseid 9
        RawAuctionRow drift = MakeRawRow(5u, 7u, 100u);      // gate C: adopt item data
        drift.itemStack    = 5u;
        drift.itemEntry    = 2590u;
        drift.itemRandProp = 7;
        rows.push_back(drift);

        AuctionBook book(NULL);
        std::vector<AhJournal::JournalRow> noJournal;
        if (!book.BuildFromRows(rows, noJournal))
        {
            fprintf(stderr, "book selftest FAILED: clean build returned false\n");
            return 1;
        }
        if (book.Size() != 2u)
        {
            fprintf(stderr, "book selftest FAILED: size=%u (exp 2)\n",
                    static_cast<unsigned>(book.Size()));
            return 1;
        }
        if (book.Orphans().size() != 3u ||
            book.Orphans()[0].kind != ORPHAN_MISSING_ITEM ||
            book.Orphans()[1].kind != ORPHAN_MISSING_ITEM ||
            book.Orphans()[2].kind != ORPHAN_BAD_HOUSE)
        {
            fprintf(stderr, "book selftest FAILED: orphan classification\n");
            return 1;
        }
        BookRow* adopted = book.Find(5u);
        if (adopted == NULL || adopted->itemCount != 5u ||
            adopted->itemTemplate != 2590u || adopted->randomPropertyId != 7)
        {
            fprintf(stderr, "book selftest FAILED: gate-C adoption\n");
            return 1;
        }
    }

    // --- journal re-mark + one-ACTIVE-per-auction invariant ---
    {
        std::vector<RawAuctionRow> rows;
        rows.push_back(MakeRawRow(10u, 1u, 100u));
        rows.push_back(MakeRawRow(11u, 1u, 100u));
        rows.push_back(MakeRawRow(12u, 1u, 100u));
        rows.push_back(MakeRawRow(13u, 1u, 100u));

        std::vector<AhJournal::JournalRow> jr;
        AhJournal::JournalRow j;
        j.uuid = UINT64_C(0x0000000100000001);
        j.auctionId = 10u;
        j.kind = static_cast<uint8>(RESOLVE_WON);            // TERMINAL kind
        j.state = AhJournal::JRN_RESOLVING;
        j.facts = "";
        j.createdTime = 1u;
        j.resolvedTime = 0u;
        jr.push_back(j);
        j.uuid = UINT64_C(0x0000000100000002);
        j.auctionId = 11u;
        j.kind = 0x43u;
        j.state = AhJournal::JRN_CANCEL_PREPARED;
        jr.push_back(j);
        j.uuid = UINT64_C(0x0000000100000003);
        j.auctionId = 12u;
        j.kind = 0u;
        j.state = AhJournal::JRN_INTENT_PENDING;
        jr.push_back(j);
        j.uuid = UINT64_C(0x0000000100000004);
        j.auctionId = 999u;                                  // no book row: tolerated
        j.kind = static_cast<uint8>(RESOLVE_WON);
        j.state = AhJournal::JRN_RESOLVING;
        jr.push_back(j);
        // F1 regression: a NON-terminal kind (CANCELLED_UNLOCK / REPAIR_RETURN)
        // must leave the row LIVE across a boot reload -- re-marking it
        // RESOLVING would freeze a live listing (unbiddable/unbuyable/
        // uncancellable) until the next restart.
        j.uuid = UINT64_C(0x0000000100000006);
        j.auctionId = 13u;
        j.kind = static_cast<uint8>(RESOLVE_CANCELLED_UNLOCK);
        j.state = AhJournal::JRN_RESOLVING;
        jr.push_back(j);

        AuctionBook book(NULL);
        if (!book.BuildFromRows(rows, jr))
        {
            fprintf(stderr, "book selftest FAILED: re-mark build returned false\n");
            return 1;
        }
        if (book.Find(10u)->state != BOOK_RESOLVING)
        {
            fprintf(stderr, "book selftest FAILED: JRN_RESOLVING re-mark"
                            " (terminal kind)\n");
            return 1;
        }
        if (book.Find(11u)->state != BOOK_CANCEL_PREPARED)
        {
            fprintf(stderr, "book selftest FAILED: JRN_CANCEL_PREPARED re-mark\n");
            return 1;
        }
        if (book.Find(12u)->state != BOOK_LIVE)
        {
            fprintf(stderr, "book selftest FAILED: INTENT_PENDING must not mark\n");
            return 1;
        }
        if (book.Find(13u)->state != BOOK_LIVE)
        {
            fprintf(stderr, "book selftest FAILED: F1 regression - non-terminal"
                            " JRN_RESOLVING (CANCELLED_UNLOCK) must stay"
                            " BOOK_LIVE, not BOOK_RESOLVING\n");
            return 1;
        }

        // Invariant: a second ACTIVE journal row on auction 10 refuses the load.
        j.uuid = UINT64_C(0x0000000100000005);
        j.auctionId = 10u;
        j.kind = 0x43u;
        j.state = AhJournal::JRN_CANCEL_PREPARED;
        jr.push_back(j);
        AuctionBook book2(NULL);
        if (book2.BuildFromRows(rows, jr))
        {
            fprintf(stderr, "book selftest FAILED: invariant violation accepted\n");
            return 1;
        }
    }

    // --- admission matrix (spec 4.3b): op x {absent, LIVE, PREPARED, RESOLVING} ---
    {
        BookRow live = MakeRawRow(20u, 1u, 100u).row;
        BookRow prepared = live;
        prepared.state = BOOK_CANCEL_PREPARED;
        BookRow resolving = live;
        resolving.state = BOOK_RESOLVING;

        if (AuctionBook::Admit(OP_BID, NULL) != BOOK_ERR_BID_OWN ||
            AuctionBook::Admit(OP_BUYOUT, NULL) != BOOK_ERR_BID_OWN ||
            AuctionBook::Admit(OP_CANCEL_PREPARE, NULL) != BOOK_ERR_DATABASE)
        {
            fprintf(stderr, "book selftest FAILED: admission vs absent row\n");
            return 1;
        }
        if (AuctionBook::Admit(OP_BID, &live) != BOOK_ERR_OK ||
            AuctionBook::Admit(OP_BUYOUT, &live) != BOOK_ERR_OK ||
            AuctionBook::Admit(OP_CANCEL_PREPARE, &live) != BOOK_ERR_OK)
        {
            fprintf(stderr, "book selftest FAILED: admission vs LIVE\n");
            return 1;
        }
        if (AuctionBook::Admit(OP_BID, &prepared) != BOOK_ERR_BID_OWN ||
            AuctionBook::Admit(OP_BUYOUT, &prepared) != BOOK_ERR_BID_OWN ||
            AuctionBook::Admit(OP_CANCEL_PREPARE, &prepared) != BOOK_ERR_DATABASE)
        {
            fprintf(stderr, "book selftest FAILED: admission vs CANCEL_PREPARED\n");
            return 1;
        }
        if (AuctionBook::Admit(OP_BID, &resolving) != BOOK_ERR_BID_OWN ||
            AuctionBook::Admit(OP_BUYOUT, &resolving) != BOOK_ERR_BID_OWN ||
            AuctionBook::Admit(OP_CANCEL_PREPARE, &resolving) != BOOK_ERR_DATABASE)
        {
            fprintf(stderr, "book selftest FAILED: admission vs RESOLVING\n");
            return 1;
        }
    }

    // --- house grouping (50-cap scope) + memory mutators/rollbacks ---
    {
        AuctionBook book(NULL);
        BookRow r = MakeRawRow(30u, 1u, 100u).row;
        book.Insert(r);
        r.id = 31u;
        r.houseId = 2u;
        book.Insert(r);                                      // same alliance group
        r.id = 32u;
        r.houseId = 7u;
        book.Insert(r);                                      // neutral group
        r.id = 33u;
        r.houseId = 4u;
        book.Insert(r);                                      // horde group
        r.id = 34u;
        r.houseId = 1u;
        r.owner = 200u;
        book.Insert(r);                                      // other owner
        if (book.CountOwned(100u, 3u) != 2u)                 // houseid 3 -> alliance group
        {
            fprintf(stderr, "book selftest FAILED: CountOwned alliance group\n");
            return 1;
        }
        if (book.CountOwned(100u, 7u) != 1u || book.CountOwned(100u, 5u) != 1u)
        {
            fprintf(stderr, "book selftest FAILED: CountOwned neutral/horde\n");
            return 1;
        }

        book.UpdateBid(30u, 777u, 4242u);
        if (book.Find(30u)->bidder != 777u || book.Find(30u)->bid != 4242u)
        {
            fprintf(stderr, "book selftest FAILED: UpdateBid\n");
            return 1;
        }
        book.RollbackUpdateBid(30u, 0u, 0u);
        if (book.Find(30u)->bidder != 0u || book.Find(30u)->bid != 0u)
        {
            fprintf(stderr, "book selftest FAILED: RollbackUpdateBid\n");
            return 1;
        }
        BookRow saved = *book.Find(30u);
        book.Remove(30u);
        if (book.Find(30u) != NULL)
        {
            fprintf(stderr, "book selftest FAILED: Remove\n");
            return 1;
        }
        book.RollbackRemove(saved);
        if (book.Find(30u) == NULL)
        {
            fprintf(stderr, "book selftest FAILED: RollbackRemove\n");
            return 1;
        }
        book.RollbackInsert(31u);
        if (book.Find(31u) != NULL)
        {
            fprintf(stderr, "book selftest FAILED: RollbackInsert\n");
            return 1;
        }
    }

    printf("book selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 player-mutation handler (sell/bid/buyout)
// ---------------------------------------------------------------------------

/**
 * @brief Pure validation-branch matrix + full in-memory OnSell/OnBid/OnBuyout
 *        flows (NULL db: journal + txn steps are skipped as success).
 *
 * @return 0 on success, 1 on any failure.
 */
static int RunMutationSelfTest()
{
    // --- OutBidAmount: the 1.12 min-increment formula (AuctionHouseMgr.cpp:1502-1510) ---
    if (MutationHandler::OutBidAmount(0u) != 1u ||
        MutationHandler::OutBidAmount(99u) != 1u ||
        MutationHandler::OutBidAmount(100u) != 5u ||
        MutationHandler::OutBidAmount(2000u) != 100u)
    {
        fprintf(stderr, "mutation selftest FAILED: OutBidAmount\n");
        return 1;
    }

    // --- ValidateBid branch matrix (legacy order, AuctionHouseHandler.cpp:735-784) ---
    {
        BookRow live = MakeRawRow(1u, 1u, 100u).row;   // owner 100, startbid 100, buyout 50000
        live.bid    = 1000u;
        live.bidder = 300u;
        uint8 reason = 99u;

        if (MutationHandler::ValidateBid(NULL, 200u, 2000u, 0u, 0u, reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_OWN)
        {
            fprintf(stderr, "mutation selftest FAILED: bid vs absent\n");
            return 1;
        }
        BookRow prepared = live;
        prepared.state = BOOK_CANCEL_PREPARED;
        if (MutationHandler::ValidateBid(&prepared, 200u, 2000u, 0u, 0u,
                                          reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_OWN)
        {
            fprintf(stderr, "mutation selftest FAILED: bid vs prepared\n");
            return 1;
        }
        if (MutationHandler::ValidateBid(&live, 100u, 2000u, 0u, 0u, reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_OWN)
        {
            fprintf(stderr, "mutation selftest FAILED: bid-own\n");
            return 1;
        }
        if (MutationHandler::ValidateBid(&live, 200u, 2000u, 55u, 55u, reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_OWN)
        {
            fprintf(stderr, "mutation selftest FAILED: same-account\n");
            return 1;
        }
        if (MutationHandler::ValidateBid(&live, 200u, 1000u, 1u, 2u, reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_HIGHER_BID)
        {
            fprintf(stderr, "mutation selftest FAILED: price<=bid\n");
            return 1;
        }
        if (MutationHandler::ValidateBid(&live, 200u, 1001u, 1u, 2u, reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_INCREMENT)
        {
            fprintf(stderr, "mutation selftest FAILED: increment\n");
            return 1;
        }
        BookRow fresh = MakeRawRow(2u, 1u, 100u).row;  // bid 0, startbid 100
        if (MutationHandler::ValidateBid(&fresh, 200u, 50u, 1u, 2u,
                                          reason) != VALIDATE_REJECT_SILENT)
        {
            fprintf(stderr, "mutation selftest FAILED: below-startbid must be silent\n");
            return 1;
        }
        if (MutationHandler::ValidateBid(&live, 200u, 50000u, 1u, 2u,
                                          reason) != VALIDATE_REJECT_SILENT)
        {
            fprintf(stderr, "mutation selftest FAILED: bid at buyout must be silent misuse\n");
            return 1;
        }
        if (MutationHandler::ValidateBid(&live, 200u, 1050u, 1u, 2u, reason) != VALIDATE_ADMIT)
        {
            fprintf(stderr, "mutation selftest FAILED: valid bid rejected\n");
            return 1;
        }
    }

    // --- ValidateBuyout branch matrix ---
    {
        BookRow live = MakeRawRow(3u, 1u, 100u).row;
        live.bid    = 1000u;
        live.bidder = 300u;
        uint8 reason = 99u;
        if (MutationHandler::ValidateBuyout(NULL, 200u, 50000u, 0u, 0u,
                                            reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_OWN)
        {
            fprintf(stderr, "mutation selftest FAILED: buyout vs absent\n");
            return 1;
        }
        if (MutationHandler::ValidateBuyout(&live, 100u, 50000u, 0u, 0u,
                                            reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_OWN)
        {
            fprintf(stderr, "mutation selftest FAILED: self-buy\n");
            return 1;
        }
        // spec 4.1: a below-buyout IPC_PLAYER_BUYOUT is a NORMAL BID at maxPrice,
        // not a silent forwarder defect. 49999 beats bid 1000 and clears the
        // min-increment/startbid, so it admits (caller commits it as a bid).
        if (MutationHandler::ValidateBuyout(&live, 200u, 49999u, 1u, 2u, reason) != VALIDATE_ADMIT)
        {
            fprintf(stderr, "mutation selftest FAILED: below-buyout buyout must admit as bid\n");
            return 1;
        }
        // Below the min-increment on the bid leg -> BID_INCREMENT (bid 1000,
        // outbid 50, threshold 1050).
        if (MutationHandler::ValidateBuyout(&live, 200u, 1049u, 1u, 2u,
                                            reason) != VALIDATE_REJECT ||
            reason != BOOK_ERR_BID_INCREMENT)
        {
            fprintf(stderr, "mutation selftest FAILED: below-buyout buyout increment\n");
            return 1;
        }
        if (MutationHandler::ValidateBuyout(&live, 200u, 1050u, 1u, 2u, reason) != VALIDATE_ADMIT)
        {
            fprintf(stderr, "mutation selftest FAILED: below-buyout buyout "
                "at-increment must admit\n");
            return 1;
        }
        // Buyout-less row: maxPrice is always a bid (never a win); 60000 clears
        // the increment/startbid -> admit as bid.
        BookRow noBuyout = live;
        noBuyout.buyout = 0u;
        if (MutationHandler::ValidateBuyout(&noBuyout, 200u, 60000u, 1u, 2u,
                                            reason) != VALIDATE_ADMIT)
        {
            fprintf(stderr, "mutation selftest FAILED: buyout-less must admit as bid\n");
            return 1;
        }
        // Below-startbid on the bid leg is a legacy silent reject (fresh row:
        // bid 0, startbid 100, buyout 50000).
        BookRow freshBuyout = MakeRawRow(4u, 1u, 100u).row;
        if (MutationHandler::ValidateBuyout(&freshBuyout, 200u, 50u, 1u, 2u,
                                            reason) != VALIDATE_REJECT_SILENT)
        {
            fprintf(stderr, "mutation selftest FAILED: below-buyout "
                "below-startbid must be silent\n");
            return 1;
        }
        // At/over buyout is the removing WIN -> admit.
        if (MutationHandler::ValidateBuyout(&live, 200u, 50000u, 1u, 2u, reason) != VALIDATE_ADMIT)
        {
            fprintf(stderr, "mutation selftest FAILED: valid buyout rejected\n");
            return 1;
        }
    }

    // --- full flows, in-memory (NULL db: journal/txn skipped) ---
    {
        AuctionBook book(NULL);
        std::vector<RawAuctionRow> noRows;
        std::vector<AhJournal::JournalRow> noJournal;
        book.BuildFromRows(noRows, noJournal);
        MutationHandler handler(book, NULL, 1u);

        PlayerSellIntent sell;
        sell.uuid = UINT64_C(0x0000000200000001);
        sell.auctionId = 40u;
        sell.sellerGuid = 100u;
        sell.house = 7u;
        sell.itemGuid = 9000u;
        sell.itemTemplate = 2589u;
        sell.itemCount = 20u;
        sell.randomPropertyId = 0;
        sell.startbid = 100u;
        sell.buyout = 50000u;
        sell.deposit = 15u;
        sell.expireTime = 2000000000u;

        PlayerMutationResult r = handler.OnSell(sell);
        if (r.status != MUT_OK || r.op != 0x40u || book.Find(40u) == NULL ||
            r.facts.auctionId != 40u || r.facts.deposit != 15u ||
            r.facts.sellerGuid != 100u || r.facts.buyout != 50000u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnSell OK path\n");
            return 1;
        }

        r = handler.OnSell(sell);   // duplicate id -> protocol-fault reject
        if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_DATABASE)
        {
            fprintf(stderr, "mutation selftest FAILED: OnSell duplicate id\n");
            return 1;
        }

        // 50-cap: fill to 50 owned in the neutral group, assert the 51st rejects
        // (legacy AUCTION_ERR_DATABASE, AuctionHouseHandler.cpp:595-599).
        for (uint32 i = 0; i < 49u; ++i)
        {
            sell.uuid += 1u;
            sell.auctionId = 41u + i;
            sell.itemGuid  = 9001u + i;
            if (handler.OnSell(sell).status != MUT_OK)
            {
                fprintf(stderr, "mutation selftest FAILED: cap prefill %u\n", i);
                return 1;
            }
        }
        sell.uuid += 1u;
        sell.auctionId = 90u;
        sell.itemGuid = 9100u;
        r = handler.OnSell(sell);
        if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_DATABASE)
        {
            fprintf(stderr, "mutation selftest FAILED: 50-cap\n");
            return 1;
        }

        // OnBid: first bid, then a raise by another player (outbid facts).
        PlayerBidIntent bid;
        bid.uuid = UINT64_C(0x0000000200000100);
        bid.auctionId = 40u;
        bid.bidderGuid = 200u;
        bid.bidAmount = 100u;
        r = handler.OnBid(bid);
        if (r.status != MUT_OK || r.op != 0x41u ||
            r.facts.curBid != 100u || r.facts.curBidderGuid != 200u ||
            r.facts.priorBidderGuid != 0u || r.facts.priorBidAmount != 0u ||
            r.facts.effectiveBid != 100u || book.Find(40u)->bid != 100u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBid first bid\n");
            return 1;
        }
        bid.uuid += 1u;
        bid.bidderGuid = 201u;
        bid.bidAmount = 200u;
        r = handler.OnBid(bid);
        if (r.status != MUT_OK || r.facts.priorBidderGuid != 200u ||
            r.facts.priorBidAmount != 100u || r.facts.curBid != 200u ||
            r.facts.curBidderGuid != 201u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBid outbid facts\n");
            return 1;
        }

        // OnBuyout: row removed, effectiveBid = min(maxPrice, buyout) = buyout,
        // prior-bidder refund facts kept (spec 4.1 I4 / 4.5).
        PlayerBuyoutIntent bo;
        bo.uuid = UINT64_C(0x0000000200000200);
        bo.auctionId = 40u;
        bo.bidderGuid = 202u;
        bo.maxPrice = 60000u;
        r = handler.OnBuyout(bo);
        if (r.status != MUT_OK || r.op != 0x42u || book.Find(40u) != NULL ||
            r.facts.effectiveBid != 50000u || r.facts.curBid != 50000u ||
            r.facts.curBidderGuid != 202u ||
            r.facts.priorBidderGuid != 201u || r.facts.priorBidAmount != 200u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBuyout\n");
            return 1;
        }

        // REJECTED results still carry the row facts the AUCTION_ERR_HIGHER_BID
        // packet needs (spec 4.5).
        bid.uuid += 1u;
        bid.auctionId = 41u;
        bid.bidderGuid = 100u;   // owner bids own auction
        bid.bidAmount = 500u;
        r = handler.OnBid(bid);
        if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_BID_OWN ||
            r.facts.auctionId != 41u || r.facts.buyout != 50000u)
        {
            fprintf(stderr, "mutation selftest FAILED: REJECTED facts\n");
            return 1;
        }

        // OnBuyout below-buyout (spec 4.1): IPC_PLAYER_BUYOUT with maxPrice <
        // buyout is a NORMAL BID at maxPrice, NOT a silent reject. Auction 41 is
        // LIVE (owner 100, bid 0, buyout 50000, startbid 100).
        PlayerBuyoutIntent bbo;
        bbo.uuid = UINT64_C(0x0000000200000300);
        bbo.auctionId = 41u;
        bbo.bidderGuid = 205u;
        bbo.maxPrice = 5000u;   // below buyout 50000, above startbid 100
        r = handler.OnBuyout(bbo);
        if (r.status != MUT_OK || r.op != 0x42u || book.Find(41u) == NULL ||
            book.Find(41u)->state != BOOK_LIVE ||
            r.facts.effectiveBid != 5000u || r.facts.curBid != 5000u ||
            r.facts.curBidderGuid != 205u || r.facts.buyout != 50000u ||
            r.facts.priorBidderGuid != 0u || r.facts.priorBidAmount != 0u ||
            book.Find(41u)->bid != 5000u || book.Find(41u)->bidder != 205u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBuyout below-buyout commits as bid\n");
            return 1;
        }

        // Below-buyout buyout that also fails min-increment (bid 5000, outbid
        // 250, threshold 5250) -> BID_INCREMENT reject, row untouched.
        bbo.uuid += 1u;
        bbo.bidderGuid = 206u;
        bbo.maxPrice = 5100u;
        r = handler.OnBuyout(bbo);
        if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_BID_INCREMENT ||
            book.Find(41u) == NULL || book.Find(41u)->bid != 5000u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBuyout below-buyout "
                "min-increment reject\n");
            return 1;
        }

        // Below-buyout buyout that beats the current bid -> commits as a bid,
        // displacing bidder 205 into the prior* refund facts.
        bbo.uuid += 1u;
        bbo.bidderGuid = 207u;
        bbo.maxPrice = 6000u;
        r = handler.OnBuyout(bbo);
        if (r.status != MUT_OK || book.Find(41u) == NULL ||
            book.Find(41u)->state != BOOK_LIVE ||
            r.facts.effectiveBid != 6000u || r.facts.curBid != 6000u ||
            r.facts.curBidderGuid != 207u || r.facts.buyout != 50000u ||
            r.facts.priorBidderGuid != 205u || r.facts.priorBidAmount != 5000u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBuyout below-buyout "
                "outbid commits as bid\n");
            return 1;
        }

        // A genuine win now (maxPrice >= buyout) removes the row; effectiveBid ==
        // buyout is the win signal, prior* carry displaced bidder 207.
        bbo.uuid += 1u;
        bbo.bidderGuid = 208u;
        bbo.maxPrice = 55000u;
        r = handler.OnBuyout(bbo);
        if (r.status != MUT_OK || r.op != 0x42u || book.Find(41u) != NULL ||
            r.facts.effectiveBid != 50000u || r.facts.curBid != 50000u ||
            r.facts.curBidderGuid != 208u ||
            r.facts.priorBidderGuid != 207u || r.facts.priorBidAmount != 6000u)
        {
            fprintf(stderr, "mutation selftest FAILED: OnBuyout win after below-buyout bids\n");
            return 1;
        }
    }

    printf("mutation selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 cancel two-phase (prepare/confirm/abort/timeout)
// ---------------------------------------------------------------------------

/**
 * @brief In-memory cancel state machine: prepare facts + lock, admission of a
 *        bid against a prepared row, double-prepare, abort, stale confirm,
 *        prepare/confirm, timeout unlock via RESOLVE_CANCELLED_UNLOCK, and
 *        boot re-arm via AdoptActiveJournal.
 *
 * @return 0 on success, 1 on any failure.
 */
static int RunCancelSelfTest()
{
    AuctionBook book(NULL);
    std::vector<RawAuctionRow> noRows;
    std::vector<AhJournal::JournalRow> noJournal;
    book.BuildFromRows(noRows, noJournal);
    MutationHandler handler(book, NULL, 1u);

    // Seed one live listing (owner 100) with a standing bid of 100 by guid 200.
    PlayerSellIntent sell;
    sell.uuid = UINT64_C(0x0000000300000001);
    sell.auctionId = 60u;
    sell.sellerGuid = 100u;
    sell.house = 7u;
    sell.itemGuid = 9500u;
    sell.itemTemplate = 2589u;
    sell.itemCount = 20u;
    sell.randomPropertyId = 0;
    sell.startbid = 100u;
    sell.buyout = 50000u;
    sell.deposit = 15u;
    sell.expireTime = 2000000000u;
    if (handler.OnSell(sell).status != MUT_OK)
    {
        fprintf(stderr, "cancel selftest FAILED: seed sell\n");
        return 1;
    }
    PlayerBidIntent bid;
    bid.uuid = UINT64_C(0x0000000300000002);
    bid.auctionId = 60u;
    bid.bidderGuid = 200u;
    bid.bidAmount = 100u;
    if (handler.OnBid(bid).status != MUT_OK)
    {
        fprintf(stderr, "cancel selftest FAILED: seed bid\n");
        return 1;
    }

    // --- not-owner prepare -> the legacy cheater reject (err-database) ---
    PlayerCancelPrepare prep;
    prep.uuid = UINT64_C(0x0000000300000010);
    prep.auctionId = 60u;
    prep.sellerGuid = 999u;
    PlayerMutationResult r = handler.OnCancelPrepare(prep);
    if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_DATABASE)
    {
        fprintf(stderr, "cancel selftest FAILED: not-owner prepare\n");
        return 1;
    }

    // --- prepare -> MUT_PREPARED with {curBid, curBidder, deposit} facts ---
    prep.sellerGuid = 100u;
    r = handler.OnCancelPrepare(prep);
    if (r.status != MUT_PREPARED || r.op != 0x43u ||
        r.facts.curBid != 100u || r.facts.curBidderGuid != 200u ||
        r.facts.deposit != 15u ||
        book.Find(60u)->state != BOOK_CANCEL_PREPARED)
    {
        fprintf(stderr, "cancel selftest FAILED: prepare\n");
        return 1;
    }

    // --- bid vs prepared row -> legacy race-loser reject (spec 4.3b) ---
    bid.uuid += 1u;
    bid.bidAmount = 200u;
    r = handler.OnBid(bid);
    if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_BID_OWN)
    {
        fprintf(stderr, "cancel selftest FAILED: bid vs prepared\n");
        return 1;
    }

    // --- second prepare on a prepared row -> err-database ---
    PlayerCancelPrepare prep2 = prep;
    prep2.uuid = UINT64_C(0x0000000300000011);
    r = handler.OnCancelPrepare(prep2);
    if (r.status != MUT_REJECTED || r.reason != BOOK_ERR_DATABASE)
    {
        fprintf(stderr, "cancel selftest FAILED: double prepare\n");
        return 1;
    }

    // --- abort -> unlocked, auction untouched ---
    r = handler.OnCancelDecide(prep.uuid, 60u, false);
    if (r.status != MUT_OK || r.op != 0x48u || book.Find(60u) == NULL ||
        book.Find(60u)->state != BOOK_LIVE)
    {
        fprintf(stderr, "cancel selftest FAILED: abort\n");
        return 1;
    }

    // --- stale confirm (already aborted) -> MUT_REJECTED_STALE ---
    r = handler.OnCancelDecide(prep.uuid, 60u, true);
    if (r.status != MUT_REJECTED_STALE || r.op != 0x47u)
    {
        fprintf(stderr, "cancel selftest FAILED: stale confirm\n");
        return 1;
    }

    // --- prepare/confirm -> row removed, facts snapshot preserved ---
    prep.uuid = UINT64_C(0x0000000300000012);
    r = handler.OnCancelPrepare(prep);
    if (r.status != MUT_PREPARED)
    {
        fprintf(stderr, "cancel selftest FAILED: re-prepare\n");
        return 1;
    }
    r = handler.OnCancelDecide(prep.uuid, 60u, true);
    if (r.status != MUT_OK || r.op != 0x47u || book.Find(60u) != NULL ||
        r.facts.curBid != 100u || r.facts.curBidderGuid != 200u ||
        r.facts.deposit != 15u || r.facts.sellerGuid != 100u)
    {
        fprintf(stderr, "cancel selftest FAILED: confirm\n");
        return 1;
    }

    // --- timeout unlock on a fresh prepared listing ---
    sell.uuid = UINT64_C(0x0000000300000020);
    sell.auctionId = 61u;
    sell.itemGuid = 9501u;
    if (handler.OnSell(sell).status != MUT_OK)
    {
        fprintf(stderr, "cancel selftest FAILED: timeout seed sell\n");
        return 1;
    }
    prep.uuid = UINT64_C(0x0000000300000021);
    prep.auctionId = 61u;
    if (handler.OnCancelPrepare(prep).status != MUT_PREPARED)
    {
        fprintf(stderr, "cancel selftest FAILED: timeout prepare\n");
        return 1;
    }

    uint64 const now = static_cast<uint64>(time(NULL));
    handler.CheckPrepareTimeouts(now + 3u);      // inside T: still locked
    if (book.Find(61u)->state != BOOK_CANCEL_PREPARED)
    {
        fprintf(stderr, "cancel selftest FAILED: early timeout fired\n");
        return 1;
    }
    handler.CheckPrepareTimeouts(now + 30u);     // past T: unlock
    if (book.Find(61u)->state != BOOK_LIVE)
    {
        fprintf(stderr, "cancel selftest FAILED: timeout did not unlock\n");
        return 1;
    }

    ResolveApply ra;
    if (!handler.PopQueuedResolve(ra) || ra.kind != RESOLVE_CANCELLED_UNLOCK ||
        ra.facts.auctionId != 61u || (ra.uuid >> 32) != 1u ||
        (ra.uuid & 0xFFFFFFFFu) < 0x80000000u)
    {
        fprintf(stderr, "cancel selftest FAILED: queued unlock resolution\n");
        return 1;
    }
    if (handler.PopQueuedResolve(ra))
    {
        fprintf(stderr, "cancel selftest FAILED: queue not drained\n");
        return 1;
    }

    // --- late confirm after the timeout unlock -> MUT_REJECTED_STALE ---
    r = handler.OnCancelDecide(prep.uuid, 61u, true);
    if (r.status != MUT_REJECTED_STALE)
    {
        fprintf(stderr, "cancel selftest FAILED: post-unlock confirm\n");
        return 1;
    }

    // --- AdoptActiveJournal re-arms a lock after a synthetic restart ---
    {
        AuctionBook book2(NULL);
        std::vector<RawAuctionRow> rows;
        rows.push_back(MakeRawRow(70u, 7u, 100u));
        std::vector<AhJournal::JournalRow> jr;
        AhJournal::JournalRow j;
        j.uuid = UINT64_C(0x0000000400000001);
        j.auctionId = 70u;
        j.kind = 0x43u;
        j.state = AhJournal::JRN_CANCEL_PREPARED;
        j.facts = "";
        j.createdTime = 50u;
        j.resolvedTime = 0u;
        jr.push_back(j);
        if (!book2.BuildFromRows(rows, jr))
        {
            fprintf(stderr, "cancel selftest FAILED: restart build\n");
            return 1;
        }
        MutationHandler handler2(book2, NULL, 2u);
        handler2.AdoptActiveJournal(jr);
        PlayerMutationResult r2 = handler2.OnCancelDecide(j.uuid, 70u, true);
        if (r2.status != MUT_OK || book2.Find(70u) != NULL)
        {
            fprintf(stderr, "cancel selftest FAILED: adopted confirm\n");
            return 1;
        }
    }

    printf("cancel selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 wire codecs (PlayerMutations.h round-trips + truncation)
// ---------------------------------------------------------------------------

/**
 * @brief Encode each SP-2 wire struct, decode it back, and assert field
 *        equality; then assert a one-byte-short buffer fails Decode.
 * @return 0 on success, 1 on any mismatch.
 */
static int RunWireSelfTest()
{
    // MutationFacts round-trip (the shared value-snapshot payload).
    MutationFacts f;
    f.auctionId = 42u; f.houseId = 7u; f.itemGuid = 9000u; f.itemTemplate = 2589u;
    f.randomPropertyId = -3; f.itemCount = 20u; f.sellerGuid = 100u; f.deposit = 15u;
    f.effectiveBid = 500u; f.priorBidderGuid = 200u; f.priorBidAmount = 100u;
    f.curBidderGuid = 201u; f.curBid = 500u; f.buyout = 50000u;

    PlayerMutationResult res;
    res.uuid = UINT64_C(0x0000000100000002); res.op = 0x41u;
    res.status = MUT_OK; res.reason = 0u; res.facts = f;
    {
        ByteBuffer bb; res.Encode(bb);
        if (bb.size() != PlayerMutationResult::WIRE_SIZE)
        {
            fprintf(stderr, "wire selftest FAILED: result size %u\n",
                    static_cast<unsigned>(bb.size()));
            return 1;
        }
        PlayerMutationResult back;
        if (!back.Decode(bb) || back.uuid != res.uuid || back.op != res.op ||
            back.status != res.status || back.facts.effectiveBid != 500u ||
            back.facts.randomPropertyId != -3 || back.facts.buyout != 50000u ||
            back.facts.priorBidderGuid != 200u)
        {
            fprintf(stderr, "wire selftest FAILED: result round-trip\n");
            return 1;
        }
    }

    // Sell/bid/buyout/cancel intents.
    {
        PlayerSellIntent s;
        s.uuid = 1u; s.auctionId = 10u; s.sellerGuid = 100u; s.house = 7u;
        s.itemGuid = 9000u; s.itemTemplate = 2589u; s.itemCount = 20u;
        s.randomPropertyId = 5; s.startbid = 100u; s.buyout = 50000u;
        s.deposit = 15u; s.expireTime = 2000000000u;
        ByteBuffer bb; s.Encode(bb);
        PlayerSellIntent b;
        if (bb.size() != PlayerSellIntent::WIRE_SIZE || !b.Decode(bb) ||
            b.expireTime != s.expireTime || b.randomPropertyId != 5 ||
            b.house != 7u || b.deposit != 15u)
        {
            fprintf(stderr, "wire selftest FAILED: sell round-trip\n");
            return 1;
        }
    }
    {
        PlayerBidIntent bi; bi.uuid = 2u; bi.auctionId = 10u;
        bi.bidderGuid = 200u; bi.bidAmount = 250u;
        ByteBuffer bb; bi.Encode(bb);
        PlayerBidIntent b;
        if (bb.size() != PlayerBidIntent::WIRE_SIZE || !b.Decode(bb) ||
            b.bidAmount != 250u || b.bidderGuid != 200u)
        {
            fprintf(stderr, "wire selftest FAILED: bid round-trip\n");
            return 1;
        }
    }
    {
        PlayerBuyoutIntent bo; bo.uuid = 3u; bo.auctionId = 10u;
        bo.bidderGuid = 202u; bo.maxPrice = 60000u;
        ByteBuffer bb; bo.Encode(bb);
        PlayerBuyoutIntent b;
        if (bb.size() != PlayerBuyoutIntent::WIRE_SIZE || !b.Decode(bb) ||
            b.maxPrice != 60000u)
        {
            fprintf(stderr, "wire selftest FAILED: buyout round-trip\n");
            return 1;
        }
    }
    {
        PlayerCancelPrepare cp; cp.uuid = 4u; cp.auctionId = 10u; cp.sellerGuid = 100u;
        ByteBuffer bb; cp.Encode(bb);
        PlayerCancelPrepare b;
        if (bb.size() != PlayerCancelPrepare::WIRE_SIZE || !b.Decode(bb) ||
            b.sellerGuid != 100u)
        {
            fprintf(stderr, "wire selftest FAILED: cancel-prepare round-trip\n");
            return 1;
        }
    }
    {
        PlayerCancelDecide cd; cd.uuid = 5u; cd.auctionId = 10u;
        ByteBuffer bb; cd.Encode(bb);
        PlayerCancelDecide b;
        if (bb.size() != PlayerCancelDecide::WIRE_SIZE || !b.Decode(bb) ||
            b.auctionId != 10u)
        {
            fprintf(stderr, "wire selftest FAILED: cancel-decide round-trip\n");
            return 1;
        }
    }

    // Resolve leg.
    {
        ResolveApply ra; ra.uuid = UINT64_C(0x0000000100000080); ra.kind = RESOLVE_WON;
        ra.facts = f;
        ByteBuffer bb; ra.Encode(bb);
        ResolveApply b;
        if (bb.size() != ResolveApply::WIRE_SIZE || !b.Decode(bb) ||
            b.kind != RESOLVE_WON || b.facts.auctionId != 42u)
        {
            fprintf(stderr, "wire selftest FAILED: resolve-apply round-trip\n");
            return 1;
        }
        ResolveAck ack; ack.uuid = ra.uuid; ack.status = RES_DUPLICATE;
        ByteBuffer ab; ack.Encode(ab);
        ResolveAck ba;
        if (ab.size() != ResolveAck::WIRE_SIZE || !ba.Decode(ab) ||
            ba.status != RES_DUPLICATE)
        {
            fprintf(stderr, "wire selftest FAILED: resolve-ack round-trip\n");
            return 1;
        }
    }

    // Extended IntentResult (bot materialization reply): itemGuid/auctionId.
    {
        IntentResult ir; ir.uuid = 9u; ir.status = INTENT_OK; ir.reason = REASON_NONE;
        ir.itemGuid = 12345u; ir.auctionId = 777u;
        ByteBuffer bb; ir.Encode(bb);
        IntentResult b;
        if (bb.size() != 18u || IntentResult::WIRE_SIZE != 18u || !b.Decode(bb) ||
            b.itemGuid != 12345u || b.auctionId != 777u)
        {
            fprintf(stderr, "wire selftest FAILED: extended IntentResult\n");
            return 1;
        }
    }

    // Truncation guard: a one-byte-short buffer must fail Decode.
    {
        ByteBuffer bb; res.Encode(bb);
        ByteBuffer truncated;
        truncated.append(bb.contents(), bb.size() - 1u);
        PlayerMutationResult back;
        if (back.Decode(truncated))
        {
            fprintf(stderr, "wire selftest FAILED: truncation not caught\n");
            return 1;
        }
    }

    printf("wire selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 journal DAO CRUD (requires a character DB; skips if absent)
// ---------------------------------------------------------------------------

/**
 * @brief Round-trip the AhJournal DAO against the configured character DB:
 *        Insert (with a binary facts blob) -> Get -> SetState -> LoadActive
 *        membership -> DeleteAppliedOlderThan prune. Skips (returns 0) when
 *        no character DB is configured, mirroring the DB-less selftest rule.
 * @return 0 on success or skip, 1 on any assertion failure.
 */
static int RunJournalSelfTest()
{
    ServiceDatabase db;
    if (!db.InitCharacter())
    {
        printf("journal selftest SKIPPED (no character DB configured)\n");
        fflush(stdout);
        return 0;
    }

    // A binary facts payload with embedded NULs to prove hex-safety.
    std::string facts;
    facts.push_back('\x00'); facts.push_back('\x01'); facts.push_back('\xFF');
    facts.push_back('\x00'); facts.push_back('\x7A');

    uint64 const uuid = UINT64_C(0x00000009DEADBEEF);
    AhJournal::JournalRow row;
    row.uuid = uuid; row.auctionId = 12345u; row.kind = 0x41u;
    row.state = AhJournal::JRN_RESOLVING; row.facts = facts;
    row.createdTime = 1000u; row.resolvedTime = 0u;

    // Clean any prior run, then Insert inside a checked txn.
    db.Character().DirectPExecute(
        "DELETE FROM `ah_worker_journal` WHERE `uuid` = %llu",
        static_cast<unsigned long long>(uuid));
    db.Character().BeginTransaction();
    AhJournal::Insert(db, row);
    if (!db.Character().CommitTransactionChecked())
    {
        fprintf(stderr, "journal selftest FAILED: insert commit\n");
        return 1;
    }

    AhJournal::JournalRow got;
    if (!AhJournal::Get(db, uuid, got) || got.auctionId != 12345u ||
        got.kind != 0x41u || got.state != AhJournal::JRN_RESOLVING ||
        got.facts != facts || got.createdTime != 1000u)
    {
        fprintf(stderr, "journal selftest FAILED: get/round-trip (facts %u bytes)\n",
                static_cast<unsigned>(got.facts.size()));
        return 1;
    }

    // SetState -> APPLIED with a resolved_time.
    db.Character().BeginTransaction();
    AhJournal::SetState(db, uuid, AhJournal::JRN_APPLIED, 2000u);
    if (!db.Character().CommitTransactionChecked())
    {
        fprintf(stderr, "journal selftest FAILED: setstate commit\n");
        return 1;
    }
    if (!AhJournal::Get(db, uuid, got) || got.state != AhJournal::JRN_APPLIED ||
        got.resolvedTime != 2000u)
    {
        fprintf(stderr, "journal selftest FAILED: setstate read-back\n");
        return 1;
    }

    // LoadActive excludes JRN_APPLIED.
    {
        std::vector<AhJournal::JournalRow> active;
        AhJournal::LoadActive(db, active);
        for (size_t i = 0; i < active.size(); ++i)
        {
            if (active[i].uuid == uuid)
            {
                fprintf(stderr, "journal selftest FAILED: APPLIED row in LoadActive\n");
                return 1;
            }
        }
    }

    // DeleteAppliedOlderThan prunes it (resolved_time 2000 < cutoff 3000).
    AhJournal::DeleteAppliedOlderThan(db, 3000u);
    if (AhJournal::Get(db, uuid, got))
    {
        fprintf(stderr, "journal selftest FAILED: prune left the row\n");
        return 1;
    }

    printf("journal selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 resolve outbox (worker expiry/win tick, Task 7)
// ---------------------------------------------------------------------------

/**
 * @brief Recording double for MutationHandler: the DB seams append to an
 *        ordered trace instead of touching a live character DB, so the outbox
 *        state machine (mark-before-send, ack transitions, window, resend) is
 *        testable inside --selftest.
 */
class TestMutationHandler : public MutationHandler
{
    public:
        TestMutationHandler(AuctionBook& book, ServiceDatabase* db,
                            uint32 runId)
            : MutationHandler(book, db, runId),
              testBook(&book), failJournalInsert(false), failTerminalApply(false)
        {
        }

        std::vector<std::string> trace; ///< Ordered record of DB/send effects.
        AuctionBook* testBook;          ///< For PersistBotListing to seed (Task 8).
        bool failJournalInsert;         ///< Simulate a failed RESOLVING insert.
        bool failTerminalApply;         ///< Simulate a failed terminal txn.

    protected:
        virtual bool JournalInsertResolving(AhJournal::JournalRow const& row)
        {
            if (failJournalInsert)
            {
                return false;
            }
            char buf[48];
            snprintf(buf, sizeof(buf), "jrn-resolving:%u:k%u", row.auctionId,
                     static_cast<unsigned>(row.kind));
            trace.push_back(buf);
            return true;
        }

        virtual bool CommitTerminalApply(uint64 /*uuid*/, uint32 auctionId)
        {
            if (failTerminalApply)
            {
                return false;
            }
            char buf[48];
            snprintf(buf, sizeof(buf), "terminal-apply:%u", auctionId);
            trace.push_back(buf);
            return true;
        }

        virtual bool JournalMarkApplied(uint64 /*uuid*/)
        {
            trace.push_back("jrn-applied");
            return true;
        }

        virtual void QueueSend(IpcMessage const& msg)
        {
            std::string tag = "send:?";
            if (msg.op == IPC_RESOLVE_APPLY)
            {
                IpcMessage copy = msg;
                ResolveApply ra;
                if (ra.Decode(copy.body))
                {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "send:%u", ra.facts.auctionId);
                    tag = buf;
                }
            }
            else if (msg.op == IPC_INTENT_SELL)
            {
                tag = "send-intent-sell";
            }
            else if (msg.op == IPC_CONSOLE)
            {
                tag = "send-console";
            }
            trace.push_back(tag);
            MutationHandler::QueueSend(msg);
        }

        virtual bool PersistBotBidSimple(BookRow const& row,
                                         uint32 /*bidAmount*/,
                                         uint64 /*uuid*/)
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "bid-simple:%u", row.id);
            trace.push_back(buf);
            return true;
        }

        virtual bool PersistBotBidDisplacing(BookRow const& row,
                                             uint32 /*bidAmount*/,
                                             uint64 /*resolveUuid*/,
                                             std::string const& /*factsBlob*/)
        {
            char buf[48];
            snprintf(buf, sizeof(buf), "bid-displace:%u", row.id);
            trace.push_back(buf);
            return true;
        }

        virtual bool PersistIntentPending(AhJournal::JournalRow const& /*row*/)
        {
            trace.push_back("jrn-intent-pending");
            return true;
        }

        virtual bool PersistBotListing(BookRow const& row, uint64 /*uuid*/)
        {
            testBook->TestSeedRow(row);
            char buf[64];
            snprintf(buf, sizeof(buf), "commit-listing:%u:%u", row.id,
                     row.itemGuid);
            trace.push_back(buf);
            return true;
        }

        virtual bool RetireIntentPending(uint64 /*uuid*/)
        {
            trace.push_back("retire-intent");
            return true;
        }
};

/// Build a LIVE book row for outbox tests.
static BookRow MakeBookRow(uint32 id, uint64 expireTime, uint32 bidder,
                           uint32 bid)
{
    BookRow r;
    r.id               = id;
    r.houseId          = 7u;
    r.itemGuid         = 100000u + id;
    r.itemTemplate     = 2589u;
    r.itemCount        = 1u;
    r.randomPropertyId = 0;
    r.owner            = 42u;
    r.buyout           = 5000u;
    r.expireTime       = expireTime;
    r.bidder           = bidder;
    r.bid              = bid;
    r.startbid         = 100u;
    r.deposit          = 15u;
    r.state            = static_cast<uint8>(BOOK_LIVE);
    return r;
}

static int OutboxFail(const char* what)
{
    fprintf(stderr, "resolve outbox selftest FAILED: %s\n", what);
    return 1;
}

static int RunResolveOutboxSelfTest()
{
    ServiceDatabase dummyDb;   // ctor opens nothing; DB seams are overridden

    // --- A: mark-before-send ordering, kind selection, tick guards ---
    {
        AuctionBook book(NULL);
        book.TestSeedRow(MakeBookRow(1u, 1000u, 77u, 250u)); // player bid -> WON
        book.TestSeedRow(MakeBookRow(2u, 1000u, 0u, 0u));    // no bid -> EXPIRED_NOBID
        book.TestSeedRow(MakeBookRow(3u, 5000u, 0u, 0u));    // not yet expired

        TestMutationHandler h(book, &dummyDb, 0xAB000000u);
        h.Tick(2000u);

        std::vector<IpcMessage> out;
        h.TakeOutbound(out);

        // Per-row order is journal-mark THEN send (spec M1), ascending id.
        if (h.trace.size() != 4u ||
            h.trace[0] != "jrn-resolving:1:k0" || h.trace[1] != "send:1" ||
            h.trace[2] != "jrn-resolving:2:k1" || h.trace[3] != "send:2")
        {
            return OutboxFail("mark-before-send trace order");
        }
        if (out.size() != 2u || out[0].op != IPC_RESOLVE_APPLY ||
            out[1].op != IPC_RESOLVE_APPLY)
        {
            return OutboxFail("expected exactly 2 RESOLVE_APPLY frames");
        }
        ResolveApply ra;
        if (!ra.Decode(out[0].body) ||
            ra.kind != static_cast<uint8>(RESOLVE_WON) ||
            ra.facts.auctionId != 1u || ra.facts.curBidderGuid != 77u ||
            ra.facts.curBid != 250u || ra.facts.sellerGuid != 42u ||
            ra.facts.deposit != 15u || ra.facts.effectiveBid != 250u)
        {
            return OutboxFail("WON facts snapshot");
        }
        ResolveApply rb;
        if (!rb.Decode(out[1].body) ||
            rb.kind != static_cast<uint8>(RESOLVE_EXPIRED_NOBID) ||
            rb.facts.auctionId != 2u || rb.facts.curBid != 0u)
        {
            return OutboxFail("EXPIRED_NOBID facts snapshot");
        }
        if (book.Find(1u)->state != static_cast<uint8>(BOOK_RESOLVING) ||
            book.Find(2u)->state != static_cast<uint8>(BOOK_RESOLVING) ||
            book.Find(3u)->state != static_cast<uint8>(BOOK_LIVE))
        {
            return OutboxFail("row-state marks after tick");
        }
        if (h.ResolvingCount() != 2u || !h.HasActiveResolution(1u) ||
            h.HasActiveResolution(3u))
        {
            return OutboxFail("tracking after tick");
        }

        // A second tick mints nothing: both rows are already RESOLVING.
        h.Tick(2001u);
        out.clear();
        h.TakeOutbound(out);
        if (!out.empty())
        {
            return OutboxFail("re-mint on marked rows");
        }

        // A failed journal insert must suppress BOTH the send and the mark.
        book.TestSeedRow(MakeBookRow(4u, 1000u, 0u, 0u));
        h.failJournalInsert = true;
        h.Tick(2002u);
        h.failJournalInsert = false;
        out.clear();
        h.TakeOutbound(out);
        if (!out.empty() ||
            book.Find(4u)->state != static_cast<uint8>(BOOK_LIVE) ||
            h.HasActiveResolution(4u))
        {
            return OutboxFail("journal-fail must suppress send+mark");
        }
    }

    // --- A2: per-tick budget + un-acked window clamp (decision 10) ---
    {
        AuctionBook book(NULL);
        for (uint32 id = 1u; id <= 100u; ++id)
        {
            book.TestSeedRow(MakeBookRow(id, 1000u, 0u, 0u));
        }
        TestMutationHandler h(book, &dummyDb, 0xAB000000u);

        std::vector<IpcMessage> out;
        h.Tick(2000u);
        h.TakeOutbound(out);
        if (out.size() != 16u)   // RESOLVE_BUDGET_PER_TICK
        {
            return OutboxFail("per-tick budget clamp (expected 16)");
        }
        h.Tick(2001u);
        h.Tick(2002u);
        h.Tick(2003u);
        out.clear();
        h.TakeOutbound(out);
        if (out.size() != 48u || h.ResolvingCount() != 64u)
        {
            return OutboxFail("window fill (expected 64 un-acked)");
        }
        h.Tick(2004u);
        out.clear();
        h.TakeOutbound(out);
        if (!out.empty())
        {
            return OutboxFail("window clamp (no mints past 64 un-acked)");
        }
    }

    // --- B: ack transitions (APPLIED/DUPLICATE terminal + non-terminal,
    //        FAILED stays RESOLVING) ---
    {
        AuctionBook book(NULL);
        book.TestSeedRow(MakeBookRow(1u, 1000u, 77u, 250u));   // -> WON
        book.TestSeedRow(MakeBookRow(3u, 9999u, 55u, 300u));   // stays live
        TestMutationHandler h(book, &dummyDb, 0xAB000000u);
        h.Tick(2000u);                                          // mints WON:1

        // A non-terminal resolution (a bot-outbid refund shape) on row 3.
        MutationFacts f = MutationHandler::FactsFromRow(*book.Find(3u));
        const uint64 refundUuid = h.QueueResolution(
            3u, static_cast<uint8>(RESOLVE_REPAIR_RETURN), f, 2000u);
        if (refundUuid == 0u ||
            book.Find(3u)->state != static_cast<uint8>(BOOK_LIVE))
        {
            return OutboxFail("non-terminal queue must not mark the row");
        }

        std::vector<IpcMessage> out;
        h.TakeOutbound(out);
        ResolveApply won;
        if (out.size() != 2u || !won.Decode(out[0].body) ||
            won.kind != static_cast<uint8>(RESOLVE_WON))
        {
            return OutboxFail("expected WON + REPAIR_RETURN frames");
        }

        // FAILED: entry stays RESOLVING, book row untouched (never deleted).
        ResolveAck ack;
        ack.uuid   = won.uuid;
        ack.status = static_cast<uint8>(RES_FAILED);
        h.OnResolveAck(ack);
        if (h.ResolvingCount() != 2u ||
            book.Find(1u)->state != static_cast<uint8>(BOOK_RESOLVING))
        {
            return OutboxFail("FAILED ack must keep the entry RESOLVING");
        }

        // A failed LOCAL terminal txn also keeps the entry for retry.
        h.failTerminalApply = true;
        ack.status = static_cast<uint8>(RES_APPLIED);
        h.OnResolveAck(ack);
        h.failTerminalApply = false;
        if (h.ResolvingCount() != 2u || book.Find(1u) == nullptr)
        {
            return OutboxFail("failed terminal txn must keep the entry");
        }

        // APPLIED on a terminal kind: one txn (journal APPLIED + row DELETE),
        // then the in-memory row goes away.
        h.OnResolveAck(ack);
        if (h.trace.back() != "terminal-apply:1" || book.Find(1u) != nullptr ||
            h.ResolvingCount() != 1u || h.HasActiveResolution(1u))
        {
            return OutboxFail("APPLIED terminal transition");
        }

        // A second APPLIED for the same uuid is unknown now: loud log, no-op.
        h.OnResolveAck(ack);
        if (h.ResolvingCount() != 1u)
        {
            return OutboxFail("late duplicate ack must be a no-op");
        }

        // DUPLICATE on the non-terminal kind: journal-only, listing persists.
        ack.uuid   = refundUuid;
        ack.status = static_cast<uint8>(RES_DUPLICATE);
        h.OnResolveAck(ack);
        if (h.trace.back() != "jrn-applied" || book.Find(3u) == nullptr ||
            book.Find(3u)->state != static_cast<uint8>(BOOK_LIVE) ||
            h.ResolvingCount() != 0u || h.HasActiveResolution(3u))
        {
            return OutboxFail("DUPLICATE non-terminal transition");
        }
    }

    // --- C: resend-after-T + boot replay of RESOLVING journal rows ---
    {
        AuctionBook book(NULL);
        book.TestSeedRow(MakeBookRow(1u, 1000u, 0u, 0u));
        TestMutationHandler h(book, &dummyDb, 0xAB000000u);
        h.Tick(2000u);
        std::vector<IpcMessage> out;
        h.TakeOutbound(out);
        if (out.size() != 1u)
        {
            return OutboxFail("resend precondition mint");
        }

        h.ResendStaleResolving(2029u);   // age 29 < RESOLVE_RESEND_SEC
        out.clear();
        h.TakeOutbound(out);
        if (!out.empty())
        {
            return OutboxFail("no re-send before T");
        }

        h.ResendStaleResolving(2030u);   // age 30 >= RESOLVE_RESEND_SEC
        out.clear();
        h.TakeOutbound(out);
        if (out.size() != 1u || out[0].op != IPC_RESOLVE_APPLY)
        {
            return OutboxFail("re-send after T");
        }
        h.ResendStaleResolving(2031u);   // refreshed: age 1 -> nothing
        out.clear();
        h.TakeOutbound(out);
        if (!out.empty())
        {
            return OutboxFail("re-send must refresh lastSentSec");
        }

        // Boot priming: a fresh handler re-sends journal RESOLVING rows
        // immediately, byte-identical to the stored facts blob.
        ResolveApply src;
        src.uuid  = UINT64_C(0x0000000700000001);
        src.kind  = static_cast<uint8>(RESOLVE_WON);
        src.facts = MutationHandler::FactsFromRow(MakeBookRow(9u, 1000u,
                                                              77u, 250u));
        ByteBuffer blobBuf;
        src.Encode(blobBuf);
        std::string blob(reinterpret_cast<const char*>(blobBuf.contents()),
                         blobBuf.size());

        AhJournal::JournalRow jr;
        jr.uuid         = src.uuid;
        jr.auctionId    = 9u;
        jr.kind         = src.kind;
        jr.state        = static_cast<uint8>(AhJournal::JRN_RESOLVING);
        jr.facts        = blob;
        jr.createdTime  = 1500u;
        jr.resolvedTime = 0u;
        std::vector<AhJournal::JournalRow> active;
        active.push_back(jr);

        AuctionBook book2(NULL);
        TestMutationHandler h2(book2, &dummyDb, 0xAB000001u);
        h2.PrimeResolvingFromJournal(active);
        out.clear();
        h2.TakeOutbound(out);
        if (out.size() != 1u || out[0].op != IPC_RESOLVE_APPLY ||
            out[0].body.size() != blob.size() ||
            memcmp(out[0].body.contents(), blob.data(), blob.size()) != 0)
        {
            return OutboxFail("boot re-send must replay the blob verbatim");
        }
        if (h2.ResolvingCount() != 1u || !h2.HasActiveResolution(9u))
        {
            return OutboxFail("boot priming tracking");
        }
    }

    // --- [FIX A] cross-restart runId-reuse collision guard -----------------
    // A worker that restarts with a REUSED runId (the supervisor's runId is NOT
    // persistent -- it resets to 1) adopts surviving RESOLVING rows minted by
    // the previous run under that same runId, while the minter (m_nextSeq)
    // restarts at 0x80000000. Without the fix the next mint (0x80000000) re-uses
    // an already-in-flight uuid for a DIFFERENT auction -> mangosd answers
    // RES_DUPLICATE -> silent value loss. Adopt rows whose high-32 == this run's
    // runId with known low words, then assert the next minted worker uuid's low
    // word is strictly greater than the highest adopted low word.
    {
        uint32 const reuseRunId = 1u;                 // restart resets runId to 1

        // Two out-of-order low words prove max() semantics (skip past the
        // HIGHEST adopted low word, not merely the last one seen).
        uint32 const adoptedHi = 0x80000003u;         // highest adopted low word
        uint32 const adoptedLo = 0x80000001u;         // a lower one, seen after

        std::vector<AhJournal::JournalRow> areuse;
        uint32 const seqs[2] = { adoptedHi, adoptedLo };
        uint32 const aucs[2] = { 21u, 22u };
        for (size_t k = 0; k < 2u; ++k)
        {
            ResolveApply asrc;
            asrc.uuid  = (static_cast<uint64>(reuseRunId) << 32) | seqs[k];
            asrc.kind  = static_cast<uint8>(RESOLVE_WON);
            asrc.facts = MutationHandler::FactsFromRow(
                MakeBookRow(aucs[k], 1000u, 77u, 250u));
            ByteBuffer ab;
            asrc.Encode(ab);

            AhJournal::JournalRow ajr;
            ajr.uuid         = asrc.uuid;
            ajr.auctionId    = aucs[k];
            ajr.kind         = asrc.kind;
            ajr.state        = static_cast<uint8>(AhJournal::JRN_RESOLVING);
            ajr.facts        = std::string(
                reinterpret_cast<const char*>(ab.contents()), ab.size());
            ajr.createdTime  = 1500u;
            ajr.resolvedTime = 0u;
            areuse.push_back(ajr);
        }

        AuctionBook book3(NULL);
        TestMutationHandler h3(book3, &dummyDb, reuseRunId);
        h3.PrimeResolvingFromJournal(areuse);

        uint64 const minted = h3.NextUuid();
        if (static_cast<uint32>(minted >> 32) != reuseRunId)
        {
            return OutboxFail("FIX A: minted uuid runId mismatch");
        }
        // RED without the fix: m_nextSeq stays 0x80000000 -> minted low word
        // 0x80000000 <= adoptedHi. GREEN with the fix: minter skipped to
        // adoptedHi + 1.
        if (static_cast<uint32>(minted & 0xFFFFFFFFu) <= adoptedHi)
        {
            return OutboxFail("FIX A: minter did not skip past adopted low word");
        }
    }

    // --- [FIX A generalization] retained terminal-row minter seed ----------
    // FIX A only advances the minter past adopted JRN_RESOLVING rows, but
    // retained terminal JRN_APPLIED rows (state 3) linger until pruned and are
    // invisible to LoadActive (state IN 1,2,4,5) and to PrimeResolvingFromJournal.
    // Under a reused runId the minter must still skip past their seq, which
    // SeedMinterPast does from AhJournal::MaxSeqForRunId's max low-32. RED
    // without the fix: the minter stays at 0x80000000 <= the retained seq -> a
    // re-mint collides -> the journal INSERT trips a duplicate PRIMARY KEY.
    {
        uint32 const retainedSeq = 0x80000087u;         // max seq of a runId-1 APPLIED row

        AuctionBook book4(NULL);
        TestMutationHandler h4(book4, &dummyDb, 1u);     // reused runId, minter @ 0x80000000
        h4.SeedMinterPast(retainedSeq);
        if (static_cast<uint32>(h4.NextUuid() & 0xFFFFFFFFu) <= retainedSeq)
        {
            return OutboxFail("SeedMinterPast: minter not seeded past retained terminal seq");
        }

        // Empty-run case: MaxSeqForRunId returns 0 -> the minter must not move.
        AuctionBook book5(NULL);
        TestMutationHandler h5(book5, &dummyDb, 1u);
        h5.SeedMinterPast(0u);
        if (static_cast<uint32>(h5.NextUuid() & 0xFFFFFFFFu) != 0x80000000u)
        {
            return OutboxFail("SeedMinterPast(0): empty run must not move the minter");
        }
    }

    printf("resolve outbox selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: SP-2 bot fold-in (Task 8)
// ---------------------------------------------------------------------------

static int FoldFail(const char* what)
{
    fprintf(stderr, "bot fold-in selftest FAILED: %s\n", what);
    return 1;
}

static int RunBotFoldInSelfTest()
{
    ServiceDatabase dummyDb;

    // --- A: bot bid, no displacement -> direct-applied, no wire traffic ---
    {
        AuctionBook book(NULL);
        book.TestSeedRow(MakeBookRow(10u, 9999u, 0u, 0u));
        TestMutationHandler h(book, &dummyDb, 0xAB000000u);
        h.SetGameTime(500u);

        if (!h.OnBotBid(UINT64_C(0xAB00000000000001), 10u, 150u))
        {
            return FoldFail("first bot bid must be admitted");
        }
        BookRow* row = book.Find(10u);
        if (row->bidder != 0u || row->bid != 150u ||
            row->state != static_cast<uint8>(BOOK_LIVE))
        {
            return FoldFail("bot bid book write (bidder=0, bid=150, LIVE)");
        }
        if (h.trace.size() != 1u || h.trace[0] != "bid-simple:10")
        {
            return FoldFail("simple bot bid journals directly APPLIED");
        }
        std::vector<IpcMessage> out;
        h.TakeOutbound(out);
        if (!out.empty())
        {
            return FoldFail("no wire traffic for a non-displacing bot bid");
        }

        // Raise over the bot's own held bid: still simple (bidder 0 gets no
        // refund; contract 3).
        if (!h.OnBotBid(UINT64_C(0xAB00000000000002), 10u, 200u) ||
            book.Find(10u)->bid != 200u || h.trace.size() != 2u ||
            h.trace[1] != "bid-simple:10")
        {
            return FoldFail("bot raise over own bid stays simple");
        }

        // Admission rejects (legacy race-loser shapes, 4.3b).
        if (h.OnBotBid(UINT64_C(0xAB00000000000003), 10u, 200u))
        {
            return FoldFail("stale bot bid (<= current) must be rejected");
        }
        if (h.OnBotBid(UINT64_C(0xAB00000000000004), 10u, 5000u))
        {
            return FoldFail("bot bid >= buyout must be rejected (D3 shape)");
        }
        if (h.OnBotBid(UINT64_C(0xAB00000000000005), 999u, 100u))
        {
            return FoldFail("bot bid on a missing row must be rejected");
        }
    }

    // --- B: bot bid displacing a player -> non-terminal REPAIR_RETURN ---
    {
        AuctionBook book(NULL);
        book.TestSeedRow(MakeBookRow(11u, 9999u, 77u, 120u));
        TestMutationHandler h(book, &dummyDb, 0xAB000000u);
        h.SetGameTime(500u);

        if (!h.OnBotBid(UINT64_C(0xAB00000000000006), 11u, 150u))
        {
            return FoldFail("displacing bot bid must be admitted");
        }
        if (h.trace.size() != 2u || h.trace[0] != "bid-displace:11" ||
            h.trace[1] != "send:11")
        {
            return FoldFail("displacement co-commits book+journal BEFORE send");
        }
        std::vector<IpcMessage> out;
        h.TakeOutbound(out);
        ResolveApply ra;
        if (out.size() != 1u || out[0].op != IPC_RESOLVE_APPLY ||
            !ra.Decode(out[0].body) ||
            ra.kind != static_cast<uint8>(RESOLVE_REPAIR_RETURN) ||
            ra.facts.priorBidderGuid != 77u ||
            ra.facts.priorBidAmount != 120u ||
            ra.facts.curBidderGuid != 0u || ra.facts.curBid != 150u)
        {
            return FoldFail("REPAIR_RETURN refund facts");
        }
        BookRow* row = book.Find(11u);
        if (row->bidder != 0u || row->bid != 150u ||
            row->state != static_cast<uint8>(BOOK_LIVE) ||
            !h.HasActiveResolution(11u))
        {
            return FoldFail("displaced row stays LIVE with active resolution");
        }
        // Invariant guard: no second bot op while the refund is un-acked.
        if (h.OnBotBid(UINT64_C(0xAB00000000000007), 11u, 300u) ||
            h.OnBotBuyout(UINT64_C(0xAB00000000000008), 11u))
        {
            return FoldFail("bot ops must skip rows with an active resolution");
        }
        // The expiry tick must also skip the resolution-locked LIVE row.
        h.Tick(20000u);
        out.clear();
        h.TakeOutbound(out);
        if (!out.empty())
        {
            return FoldFail("tick must skip resolution-locked LIVE rows");
        }
        // Ack releases the lock; the listing persists (non-terminal).
        ResolveAck ack;
        ack.uuid   = ra.uuid;
        ack.status = static_cast<uint8>(RES_APPLIED);
        h.OnResolveAck(ack);
        if (h.HasActiveResolution(11u) || book.Find(11u) == nullptr)
        {
            return FoldFail("non-terminal refund ack keeps the listing");
        }
    }

    // --- C: bot buyout -> terminal WON resolution, bidder=0 ---
    {
        AuctionBook book(NULL);
        book.TestSeedRow(MakeBookRow(12u, 9999u, 88u, 200u));
        TestMutationHandler h(book, &dummyDb, 0xAB000000u);
        h.SetGameTime(500u);

        if (!h.OnBotBuyout(UINT64_C(0xAB00000000000009), 12u))
        {
            return FoldFail("bot buyout must be admitted");
        }
        std::vector<IpcMessage> out;
        h.TakeOutbound(out);
        ResolveApply ra;
        if (out.size() != 1u || !ra.Decode(out[0].body) ||
            ra.kind != static_cast<uint8>(RESOLVE_WON) ||
            ra.facts.curBidderGuid != 0u || ra.facts.curBid != 5000u ||
            ra.facts.effectiveBid != 5000u ||
            ra.facts.priorBidderGuid != 88u ||
            ra.facts.priorBidAmount != 200u)
        {
            return FoldFail("bot buyout WON facts (bidder=0, prior refund)");
        }
        if (book.Find(12u)->state != static_cast<uint8>(BOOK_RESOLVING))
        {
            return FoldFail("bot buyout marks the row RESOLVING");
        }
        ResolveAck ack;
        ack.uuid   = ra.uuid;
        ack.status = static_cast<uint8>(RES_APPLIED);
        h.OnResolveAck(ack);
        if (book.Find(12u) != nullptr)
        {
            return FoldFail("terminal WON ack deletes the book row");
        }
        // Bid-only listing: no buyout to execute.
        BookRow bidOnly = MakeBookRow(13u, 9999u, 0u, 0u);
        bidOnly.buyout = 0u;
        book.TestSeedRow(bidOnly);
        if (h.OnBotBuyout(UINT64_C(0xAB0000000000000A), 13u))
        {
            return FoldFail("bot buyout on a bid-only listing must be rejected");
        }
    }

    // --- D: direct-handler bot bid vs player bid admission (4.3b) ---
    // NULL db: this block calls the player OnBid path, whose non-virtual
    // LookupAccount() would otherwise fire a real PQuery on the unconnected
    // dummy DB (owner/bidder guids are non-zero). This matches the NULL-db
    // convention the Task-5 mutation/cancel selftests use for OnBid/OnSell.
    {
        AuctionBook book(NULL);
        BookRow prepared = MakeBookRow(21u, 9999u, 0u, 0u);
        prepared.state = static_cast<uint8>(BOOK_CANCEL_PREPARED);
        book.TestSeedRow(prepared);
        TestMutationHandler h(book, NULL, 0xAB000000u);
        h.SetGameTime(500u);

        if (h.OnBotBid(UINT64_C(0xAB0000000000000B), 21u, 500u))
        {
            return FoldFail("bot bid on CANCEL_PREPARED must be rejected");
        }
        PlayerBidIntent pb;
        pb.uuid       = UINT64_C(0x0000000500000001);
        pb.auctionId  = 21u;
        pb.bidderGuid = 55u;
        pb.bidAmount  = 500u;
        PlayerMutationResult pres = h.OnBid(pb);
        if (pres.status == static_cast<uint8>(MUT_OK))
        {
            return FoldFail("player bid on CANCEL_PREPARED must not be MUT_OK");
        }
    }

    // --- Bot minter cross-restart seed (BotBrain::SeedSeqPast) --------------
    // BotBrain::m_seq is the SECOND journal-PK minter (low half [1,0x7FFFFFFF]),
    // which also restarts at 0 on a runId-reuse restart; retained bot-sell /
    // simple-bot-bid rows would re-collide (duplicate PRIMARY KEY) unless boot
    // seeds it past the low-half retained max
    // (AhJournal::MaxSeqForRunId(highHalf=false)). Construction touches no DB --
    // ItemPool/MarketSnapshot only query on Build()/Refresh().
    {
        ServiceConfig  botCfg;
        ItemPool       botItemPool(botCfg, dummyDb);
        MarketSnapshot botMkt(dummyDb);
        BotBrain       brain(botCfg, botItemPool, botMkt, 77u /*botGuid*/, 1u /*runId*/);

        if (brain.CurrentSeq() != 0u)
        {
            return FoldFail("fresh BotBrain minter must start at seq 0");
        }
        // Retained low-half rows up to seq 150 -> skip the minter to 150 so the
        // next NextUuid() (++m_seq) mints 151, clear of the retained rows. RED
        // without the seed: m_seq stays 0 -> first mint 1 collides.
        brain.SeedSeqPast(150u);
        if (brain.CurrentSeq() != 150u)
        {
            return FoldFail("SeedSeqPast must skip the bot minter past the retained low-half seq");
        }
        // Monotonic: a lower or empty (0) seed never rewinds the minter.
        brain.SeedSeqPast(100u);
        brain.SeedSeqPast(0u);
        if (brain.CurrentSeq() != 150u)
        {
            return FoldFail("SeedSeqPast must never rewind the bot minter");
        }
    }

    printf("bot fold-in selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Self-test: reliable-lane classifier + unbounded over-cap survival
// ---------------------------------------------------------------------------
//
// [SP-2 decision 10] Assert IpcIsReliableOpcode routes exactly the
// mutation-class opcodes onto the reliable lane (and nothing else), and that
// the lane never drops a frame under over-capacity pressure - a browse flood on
// the bounded queue must never cost a value-bearing frame.
static int RunReliableLaneSelfTest()
{
    // --- classifier: mutation-class opcodes ARE reliable ---
    const uint16 reliable[] =
    {
        IPC_PLAYER_SELL, IPC_PLAYER_BID, IPC_PLAYER_BUYOUT, IPC_PLAYER_CANCEL,
        IPC_PLAYER_RESULT, IPC_RESOLVE_APPLY, IPC_RESOLVE_ACK,
        IPC_PLAYER_CANCEL_CONFIRM, IPC_PLAYER_CANCEL_ABORT,
        IPC_INTENT_SELL, IPC_INTENT_RESULT
    };
    for (size_t i = 0; i < sizeof(reliable) / sizeof(reliable[0]); ++i)
    {
        if (!IpcIsReliableOpcode(reliable[i]))
        {
            fprintf(stderr, "reliable lane selftest FAILED: opcode 0x%04X"
                            " not classified reliable\n",
                    static_cast<unsigned>(reliable[i]));
            return 1;
        }
    }

    // --- classifier: browse/heartbeat/gametime STAY on the bounded queue ---
    const uint16 bounded[] =
    {
        IPC_HEARTBEAT, IPC_BROWSE_QUERY, IPC_GAMETIME, IPC_BROWSE_RESULT,
        IPC_HEARTBEAT_ACK, IPC_GMCMD
    };
    for (size_t i = 0; i < sizeof(bounded) / sizeof(bounded[0]); ++i)
    {
        if (IpcIsReliableOpcode(bounded[i]))
        {
            fprintf(stderr, "reliable lane selftest FAILED: opcode 0x%04X"
                            " misclassified as reliable\n",
                    static_cast<unsigned>(bounded[i]));
            return 1;
        }
    }

    // --- over-cap survival: the unbounded lane never drops, preserves FIFO ---
    // Push well past the bounded inbound cap and confirm every frame survives in
    // order, proving a browse flood on the bounded queue cannot displace a
    // value-bearing frame. Stack-allocated link, exercised directly (never
    // Release()d): PushReliable/PopReliable/ReliableSize touch only the lane.
    IpcServerLink lane;
    const size_t N = IPC_INBOUND_QUEUE_CAP * 4u + 7u;
    for (size_t i = 0; i < N; ++i)
    {
        IpcMessage m;
        m.op = IPC_PLAYER_RESULT;
        m.generation = static_cast<uint32>(i);   // FIFO order marker
        lane.PushReliable(m);
    }
    if (lane.ReliableSize() != N)
    {
        fprintf(stderr, "reliable lane selftest FAILED: size %u (exp %u)\n",
                static_cast<unsigned>(lane.ReliableSize()),
                static_cast<unsigned>(N));
        return 1;
    }
    size_t got = 0;
    IpcMessage out;
    while (lane.PopReliable(out))
    {
        if (out.generation != static_cast<uint32>(got))
        {
            fprintf(stderr, "reliable lane selftest FAILED: FIFO order at %u"
                            " (got gen %u)\n",
                    static_cast<unsigned>(got),
                    static_cast<unsigned>(out.generation));
            return 1;
        }
        ++got;
    }
    if (got != N)
    {
        fprintf(stderr, "reliable lane selftest FAILED: drained %u (exp %u)\n",
                static_cast<unsigned>(got), static_cast<unsigned>(N));
        return 1;
    }

    printf("reliable lane selftest OK\n");
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

    // --- Browse dispatch wiring: query decode -> empty fetch result encode ---
    {
        BrowseQuery q;
        q.queryId = UINT64_C(0x00000002CAFEF00D);
        q.kind = static_cast<uint8>(BROWSE_LIST);
        q.house = 0u; q.allHouses = 0u;
        q.itemClass = 0xFFFFFFFFu; q.itemSubClass = 0xFFFFFFFFu;
        q.inventoryType = 0xFFFFFFFFu; q.quality = 0xFFFFFFFFu;
        q.levelmin = 0u; q.levelmax = 0u; q.usable = 0u; q.deferEluna = 0u;
        q.listfrom = 0u; q.localeIndex = 0; q.requesterGuidLow = 0u;
        q.minMountLevel = 40u; q.minEpicMountLevel = 60u;
        q.profile.classId = 1u; q.profile.raceId = 1u;
        q.profile.level = 1u; q.profile.honorRank = 0u;

        IpcMessage qm; qm.op = IPC_BROWSE_QUERY; q.Encode(qm.body);
        BrowseQuery decoded;
        if (!decoded.Decode(qm.body))
        {
            fprintf(stderr, "selftest FAILED: BrowseQuery loopback decode\n");
            cli.Stop(); srv.Stop(); return 1;
        }

        std::vector<BrowseRow> none;
        BrowseResult rr = BrowseHandler::FilterAndPaginate(none, decoded);
        IpcMessage rm; rm.op = IPC_BROWSE_RESULT; rr.Encode(rm.body);
        BrowseResult back;
        if (!back.Decode(rm.body) || back.queryId != q.queryId ||
            back.Count() != 0u || back.totalcount != 0u)
        {
            fprintf(stderr, "selftest FAILED: BrowseResult loopback mismatch\n");
            cli.Stop(); srv.Stop(); return 1;
        }

        // Fetch must exist with the (ServiceDatabase&, BrowseQuery, FetchStatus&)
        // signature. This cast is a compile-time symbol guard; never called in
        // --selftest (no live DB), so we suppress the unused-variable warning
        // by voiding it.
        (void)static_cast<BrowseResult(*)(ServiceDatabase&, const BrowseQuery&,
            FetchStatus&)>(&BrowseHandler::Fetch);
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
        int rc = RunWireSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunJournalSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunIntentCodecSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunBookSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunMutationSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunCancelSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunResolveOutboxSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunBotFoldInSelfTest();
        if (rc != 0)
        {
            return rc;
        }
        rc = RunReliableLaneSelfTest();
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

    // --- Install parent-death guard (POSIX: prctl PR_SET_PDEATHSIG = SIGUSR1;
    // Windows: no-op). PF3-C: SIGUSR1 (NOT SIGTERM) on purpose -- SIGTERM keeps
    // its default-terminate disposition so a stray/operator SIGTERM is never
    // swallowed; do not "simplify" this back to SIGTERM. ---
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

    // SP-2: write-authority book + mutation handler (main thread only -- the
    // serializer). The authority bit arrives in IPC_HELLO_ACK; without it the
    // worker must never touch the auction table (default-off gating). Frames
    // arriving between READY and this construction sit in the inbound queue
    // and are drained after it, so nothing is lost.
    AuctionBook*     ahBook    = nullptr;
    MutationHandler* ahHandler = nullptr;
    if (cli.WriteAuthority())
    {
        std::vector<AhJournal::JournalRow> activeJournal;
        AhJournal::LoadActive(botDb, activeJournal);
        ahBook = new AuctionBook(&botDb);
        if (!ahBook->LoadFromDb(botDb, activeJournal))
        {
            fprintf(stderr, "ah-service: authoritative book load failed -"
                            " exiting\n");
            delete ahBook;
            delete botBrain;
            delete botSnap;
            delete botPool;
            botDb.Shutdown();
            cli.Stop();
            return 1;
        }
        ahHandler = new MutationHandler(*ahBook, &botDb, cli.RunId());
        // Cross-restart uuid-collision guard (no duplicate-PK journal INSERT
        // after a runId-reuse restart). The supervisor resets runId to 1 each
        // mangosd restart, so BOTH journal-PK minters restart low and would
        // re-mint retained uuids. They partition this runId's low-32 space by
        // the high bit -- MutationHandler owns [0x80000000+] (player mutations,
        // resolves, bot buyout); BotBrain owns [1, 0x7FFFFFFF] (bot sells,
        // simple bot bids) -- and terminal JRN_APPLIED rows linger, invisible to
        // LoadActive, so EACH minter is seeded past its own half's retained max.
        // Fail closed if either seed query errors (an unsafe seed would re-open
        // the collision).
        uint32 playerMaxSeq = 0u;
        uint32 botMaxSeq    = 0u;
        if (!AhJournal::MaxSeqForRunId(botDb, cli.RunId(), true,  playerMaxSeq) ||
            !AhJournal::MaxSeqForRunId(botDb, cli.RunId(), false, botMaxSeq))
        {
            fprintf(stderr, "ah-service: journal minter-seed query failed -"
                            " refusing to mint (duplicate-PK risk); exiting\n");
            delete ahHandler;
            delete ahBook;
            delete botBrain;
            delete botSnap;
            delete botPool;
            botDb.Shutdown();
            cli.Stop();
            return 1;
        }
        ahHandler->SeedMinterPast(playerMaxSeq);
        botBrain->SeedSeqPast(botMaxSeq);
        ahHandler->AdoptActiveJournal(activeJournal);
        // SP-2 Task 7 (spec 4.3 at-least-once): re-send EVERY RESOLVING
        // journal entry immediately; the first loop pass drains the frames.
        ahHandler->PrimeResolvingFromJournal(activeJournal);
        // SP-2 Task 8 (spec 5.4): re-load + re-send in-flight bot-sell
        // materialization requests (JRN_INTENT_PENDING).
        ahHandler->PrimePendingSellsFromJournal(activeJournal);
        printf("ah-service: WRITE AUTHORITY active - book %u listing(s),"
               " %u orphan(s)\n",
               static_cast<unsigned>(ahBook->Size()),
               static_cast<unsigned>(ahBook->Orphans().size()));
    }

    // SP-1: dedicated browse thread (owns per-thread MySQL init in run()).
    BrowseThread* browseRunnable = new BrowseThread(botDb, cli);
    browseRunnable->incReference();
    ACE_Based::Thread browseThread(browseRunnable);

    // --- Service loop ---
    volatile bool stop = false;
    uint32 sinceTickMs = 0;       ///< Accumulator toward the bot cadence.
    bool   backoffNext = false;   ///< Skip next tick after IPC_QUEUE_FULL.

    // SP-2 Task 7: game-time clock + mutation-tick cadence. The tick runs on
    // IPC_GAMETIME-synced game time (spec 7/M1) and pauses while the link is
    // down: this loop exits on disconnect and the supervisor restarts the
    // process, so "paused" is structural. lastGametime advances only on
    // IPC_GAMETIME (sent with every supervisor heartbeat, uint32 LE body -
    // WorkerSupervisor.cpp), so expiry detection lags by at most one
    // heartbeat interval; acceptable vs the legacy minute-class sweep.
    uint32 lastGametime   = 0;
    bool   gametimeKnown  = false;
    uint32 sinceMutTickMs = 0;
    const uint32 mutTickMs = static_cast<uint32>(
        sConfig.GetIntDefault("AH.Service.TickMs", 1000));

    while (!stop)
    {
        IpcMessage msg;
        // [SP-2 decision 10] Prefer the UNBOUNDED reliable lane on every pop: a
        // reliable frame (player mutation command, resolve-ack, cancel
        // confirm/abort) is always taken before a bounded-queue frame, so a
        // browse flood can never starve or drop a mutation-class frame.
        while (cli.PopReliable(msg) || cli.PopInbound(msg))
        {
            switch (msg.op)
            {
                case IPC_HEARTBEAT:
                {
                    // OPEN-1: report operational health in the ack body so the
                    // supervisor's ServiceActive() reflects RUNTIME health, not
                    // just transport. Healthy == the brain is usable AND the
                    // market snapshot is healthy (>5 consecutive snapshot
                    // failures, a lost SELECT grant, or a bad cross-DB JOIN all
                    // trip MarketSnapshot::Healthy()). When unhealthy the child
                    // already STOPS emitting; reporting it here lets mangosd
                    // resume the in-process bot instead of stalling silently.
                    IpcMessage ack;
                    ack.op = IPC_HEARTBEAT_ACK;
                    const uint8 healthy =
                        (botBrain != nullptr && botSnap->Healthy()) ? 1u : 0u;
                    ack.body << healthy;
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
                        if (ahHandler != nullptr)
                        {
                            // SP-2 Task 8: bot-sell materialization reply.
                            ahHandler->OnBotSellResult(res.uuid, res.status,
                                                       res.reason, res.itemGuid,
                                                       res.auctionId);
                        }
                        printf("ah-service: intent result uuid=%08x:%08x"
                               " status=%u reason=%u\n",
                               static_cast<unsigned>(res.uuid >> 32),
                               static_cast<unsigned>(res.uuid & 0xFFFFFFFFu),
                               res.status, res.reason);
                    }
                    break;
                }
                case IPC_RESOLVE_ACK:
                {
                    // SP-2 Task 7: outbox ack (spec 4.3 / decision 10).
                    ResolveAck ra;
                    if (ahHandler != nullptr && ra.Decode(msg.body))
                    {
                        ahHandler->OnResolveAck(ra);
                    }
                    else if (ahHandler == nullptr)
                    {
                        printf("ah-service: IPC_RESOLVE_ACK without write"
                               " authority - ignored\n");
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
                case IPC_BROWSE_QUERY:
                {
                    BrowseQuery bq;
                    if (bq.Decode(msg.body))
                    {
                        if (!browseRunnable->Submit(bq))
                        {
                            // D4: Submit already replied tooMany=1; mangosd
                            // serves the in-process fallback NOW (no ~10s TTL).
                            printf("ah-service: browse queue full -"
                                   " sent tooMany (mangosd in-process)\n");
                        }
                    }
                    else
                    {
                        printf("ah-service: IPC_BROWSE_QUERY decode failed\n");
                    }
                    break;
                }
                case IPC_GAMETIME:
                {
                    // SP-2: feeds the expiry tick's clock (spec 7/M1). The
                    // bot brain still uses time(NULL) directly, unchanged.
                    uint32 gt = 0;
                    if (msg.body.size() >= 4u)
                    {
                        msg.body >> gt;
                        lastGametime  = gt;
                        gametimeKnown = true;
                        if (ahHandler != nullptr)
                        {
                            ahHandler->SetGameTime(gt);
                        }
                    }
                    break;
                }
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
                case IPC_PLAYER_SELL:
                {
                    PlayerSellIntent in;
                    if (ahHandler != nullptr && in.Decode(msg.body))
                    {
                        PlayerMutationResult res = ahHandler->OnSell(in);
                        IpcMessage reply;
                        reply.op = IPC_PLAYER_RESULT;
                        res.Encode(reply.body);
                        cli.SendFrame(reply);
                    }
                    else
                    {
                        fprintf(stderr, "ah-service: IPC_PLAYER_SELL %s\n",
                                (ahHandler == nullptr)
                                    ? "without write authority - ignored"
                                    : "decode failed");
                    }
                    break;
                }
                case IPC_PLAYER_BID:
                {
                    PlayerBidIntent in;
                    if (ahHandler != nullptr && in.Decode(msg.body))
                    {
                        PlayerMutationResult res = ahHandler->OnBid(in);
                        IpcMessage reply;
                        reply.op = IPC_PLAYER_RESULT;
                        res.Encode(reply.body);
                        cli.SendFrame(reply);
                    }
                    else
                    {
                        fprintf(stderr, "ah-service: IPC_PLAYER_BID %s\n",
                                (ahHandler == nullptr)
                                    ? "without write authority - ignored"
                                    : "decode failed");
                    }
                    break;
                }
                case IPC_PLAYER_BUYOUT:
                {
                    PlayerBuyoutIntent in;
                    if (ahHandler != nullptr && in.Decode(msg.body))
                    {
                        PlayerMutationResult res = ahHandler->OnBuyout(in);
                        IpcMessage reply;
                        reply.op = IPC_PLAYER_RESULT;
                        res.Encode(reply.body);
                        cli.SendFrame(reply);
                    }
                    else
                    {
                        fprintf(stderr, "ah-service: IPC_PLAYER_BUYOUT %s\n",
                                (ahHandler == nullptr)
                                    ? "without write authority - ignored"
                                    : "decode failed");
                    }
                    break;
                }
                case IPC_PLAYER_CANCEL:
                {
                    PlayerCancelPrepare in;
                    if (ahHandler != nullptr && in.Decode(msg.body))
                    {
                        PlayerMutationResult res = ahHandler->OnCancelPrepare(in);
                        IpcMessage reply;
                        reply.op = IPC_PLAYER_RESULT;
                        res.Encode(reply.body);
                        cli.SendFrame(reply);
                    }
                    else
                    {
                        fprintf(stderr, "ah-service: IPC_PLAYER_CANCEL %s\n",
                                (ahHandler == nullptr)
                                    ? "without write authority - ignored"
                                    : "decode failed");
                    }
                    break;
                }
                case IPC_PLAYER_CANCEL_CONFIRM:
                case IPC_PLAYER_CANCEL_ABORT:
                {
                    PlayerCancelDecide in;
                    if (ahHandler != nullptr && in.Decode(msg.body))
                    {
                        bool const confirm = (msg.op == IPC_PLAYER_CANCEL_CONFIRM);
                        PlayerMutationResult res =
                            ahHandler->OnCancelDecide(in.uuid, in.auctionId, confirm);
                        IpcMessage reply;
                        reply.op = IPC_PLAYER_RESULT;
                        res.Encode(reply.body);
                        cli.SendFrame(reply);
                    }
                    else
                    {
                        fprintf(stderr, "ah-service: cancel decide (op 0x%04x) %s\n",
                                static_cast<unsigned>(msg.op),
                                (ahHandler == nullptr)
                                    ? "without write authority - ignored"
                                    : "decode failed");
                    }
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

        // SP-2: cancel prepare-lock timeout sweep (same serializer thread as
        // the dispatch above -- no tick-vs-handler race by construction).
        if (ahHandler != nullptr)
        {
            ahHandler->CheckPrepareTimeouts(static_cast<uint64>(time(NULL)));
            // SP-2 Task 8: re-send / abandon in-flight bot-sell materializations.
            ahHandler->ResendStalePendingSells(static_cast<uint64>(time(NULL)));
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
                            if (ahHandler != nullptr)
                            {
                                // SP-2 Task 8: under WriteAuthority the bot no
                                // longer sends IPC_INTENT_BID/BUYOUT - the
                                // worker applies the value effect in-process
                                // (bidder=0). Bot sells still round-trip via the
                                // retained IPC_INTENT_SELL materialization leg.
                                EmittedIntent const& ei = intents[i];
                                if (ei.kind == EmittedIntent::KIND_BID)
                                {
                                    ahHandler->OnBotBid(ei.bid.uuid,
                                                        ei.bid.auctionId,
                                                        ei.bid.bidAmount);
                                }
                                else if (ei.kind == EmittedIntent::KIND_BUYOUT)
                                {
                                    ahHandler->OnBotBuyout(ei.buyout.uuid,
                                                           ei.buyout.auctionId);
                                }
                                else /* KIND_SELL */
                                {
                                    ahHandler->BotSellBegin(ei.sell);
                                }
                            }
                            else if (emitDryRun)
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

        // --- SP-2: expiry/win tick + resolve outbox cadence (Task 7) ---
        if (ahHandler != nullptr && !stop)
        {
            sinceMutTickMs += 10;
            if (sinceMutTickMs >= mutTickMs)
            {
                sinceMutTickMs = 0;
                if (gametimeKnown)
                {
                    ahHandler->Tick(lastGametime);
                    ahHandler->ResendStaleResolving(lastGametime);
                }
            }
            // Drain handler-queued frames (tick resolutions, re-sends, boot
            // replay, and Task-6 prepare-timeout unlocks) to the reliable lane.
            std::vector<IpcMessage> outFrames;
            ahHandler->TakeOutbound(outFrames);
            for (size_t i = 0; i < outFrames.size(); ++i)
            {
                cli.SendFrame(outFrames[i]);
            }
        }

        ACE_Based::Thread::Sleep(10);
    }

    // C4: stop the browse thread and JOIN before DB/client teardown so the
    // thread's per-thread MySQL handle (ThreadEnd) is released cleanly.
    browseRunnable->Stop();
    browseThread.wait();             // guaranteed join
    browseRunnable->decReference();  // may delete

    delete ahHandler;
    delete ahBook;
    delete botBrain;
    delete botSnap;
    delete botPool;
    botDb.Shutdown();

    cli.Stop();
    printf("ah-service: shutdown complete\n");
    return 0;
}
