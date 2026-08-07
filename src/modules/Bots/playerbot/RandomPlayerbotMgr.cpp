#include <unordered_map>
#include <algorithm>
#include <vector>
#include <map>
#include <list>
#include "Config/Config.h"
#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "PlayerbotFactory.h"
#include "AccountMgr.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "PlayerbotAI.h"
#include "Player.h"
#include "AiFactory.h"
#include "GridDefines.h"
#include "Map.h"
#include "MapManager.h"
#include "LFGMgr.h"
#include "Group.h"
#include "Timer.h"


/**
 * RandomPlayerbotMgr is responsible for managing random player bots in the game.
 * It handles the creation, updating, and processing of these bots, ensuring they
 * behave in a way that simulates real player activity.
 */
RandomPlayerbotMgr::RandomPlayerbotMgr() : PlayerbotHolder(), processTicks(0), m_processBotCursor(0), m_starterZoneCountsPass(-1)
{
}

RandomPlayerbotMgr::~RandomPlayerbotMgr()
{
}

void RandomPlayerbotMgr::UpdateAIInternal(uint32 elapsed)
{
    SetNextCheckDelay(sPlayerbotAIConfig.randomBotUpdateInterval * 1000);

    if (!sPlayerbotAIConfig.randomBotAutologin || !sPlayerbotAIConfig.enabled)
    {
        return;
    }

    if (sPlayerbotAIConfig.randomBotKeepGroups)
    {
        if (!processTicks)
        {
            EnsureGroupedBotsOnline();
        }
        LoadGroupedBots();
    }

    if (!processTicks)
    {
        // The eviction backoff is a runtime throttle, not durable state, and persisting
        // it means a bot that had nowhere to go before a restart is still serving its
        // cooldown afterwards -- including across the very restart that installed the
        // fix for whatever stranded it. A fresh start is a fresh chance.
        CharacterDatabase.Execute("DELETE FROM `ai_playerbot_random_bots` WHERE `event` = 'evictcheck'");

        // A bot still sitting at level 1 with its randomize event banked never got through
        // RandomizeFirst: that is the one path which grants a level, and a bot that has it
        // has left level 1 behind. The banked event then suppresses every retry for as
        // long as MaxRandomRandomizeTime, which defaults to fourteen days, so an install
        // that hit this stays broken long after the code that caused it is gone -- naked
        // level 1 bots with no talents and no trainer spells, and no way back on their own.
        //
        // Dropping just those events lets the next pass randomize them properly. It is
        // safe to run every start: a healthy roster has almost nothing at level 1 holding
        // a banked event, and the worst case for one that does is being randomized sooner
        // than scheduled, which is the intended treatment anyway.
        CharacterDatabase.Execute(
            "DELETE `e` FROM `ai_playerbot_random_bots` `e` "
            "JOIN `characters` `c` ON `c`.`guid` = `e`.`bot` "
            "WHERE `e`.`event` = 'randomize' AND `e`.`owner` = 0 AND `c`.`level` = 1");
    }

    sLog.outBasic("Processing random bots...");

    uint32 cachedMin = GetEventValue(0, "config_min");
    uint32 cachedMax = GetEventValue(0, "config_max");

    if (cachedMin != sPlayerbotAIConfig.minRandomBots ||
        cachedMax != sPlayerbotAIConfig.maxRandomBots)
    {
        sLog.outString("Bot count range changed from %d-%d to %d-%d, regenerating target...",
            cachedMin, cachedMax,
            sPlayerbotAIConfig.minRandomBots, sPlayerbotAIConfig.maxRandomBots);

        SetEventValue(0, "bot_count", 0, 0);  // Invalidate
        SetEventValue(0, "config_min", sPlayerbotAIConfig.minRandomBots, 999999);
        SetEventValue(0, "config_max", sPlayerbotAIConfig.maxRandomBots, 999999);
    }

    int maxAllowedBotCount = GetEventValue(0, "bot_count");
    if (!maxAllowedBotCount)
    {
        maxAllowedBotCount = urand(sPlayerbotAIConfig.minRandomBots, sPlayerbotAIConfig.maxRandomBots);
        SetEventValue(0, "bot_count", maxAllowedBotCount,
            urand(sPlayerbotAIConfig.randomBotCountChangeMinInterval, sPlayerbotAIConfig.randomBotCountChangeMaxInterval));
    }

    list<uint32> bots = GetBots();

    // Seed the random-bot cache here, on the world thread, for every bot this pass knows
    // about. IsRandomBot is reached from map workers -- PlayerbotAI::UpdateAI, the trade
    // and grind values, AiFactory -- and on a miss it both queries the database and
    // inserts into this unordered_map. Two threads inserting, or one reading through
    // another's rehash, is undefined behaviour rather than a stale read. Making the world
    // thread fill it first means the map-worker path only ever finds, never writes.
    for (list<uint32>::const_iterator i = bots.begin(); i != bots.end(); ++i)
    {
        if (m_randomBotCache.find(*i) == m_randomBotCache.end())
        {
            m_randomBotCache[*i] = true;
        }
    }

    int botCount = bots.size();
    sLog.outBasic("Random bot roster %d, target %d (config %d-%d)", botCount, maxAllowedBotCount,
        sPlayerbotAIConfig.minRandomBots, sPlayerbotAIConfig.maxRandomBots);
    int allianceNewBots = 0, hordeNewBots = 0;
    int randomBotsPerInterval = (int)urand(sPlayerbotAIConfig.minRandomBotsPerInterval, sPlayerbotAIConfig.maxRandomBotsPerInterval);
    if (!processTicks)
    {
        if (sPlayerbotAIConfig.randomBotLoginAtStartup)
        {
            randomBotsPerInterval = bots.size();
        }
    }

    // Pace the pass against a wall-clock budget so a single update tick can not be
    // monopolised by a burst of bot adds/randomizes/saves. Work that does not fit in
    // the budget is carried over to the next (soon-rescheduled) pass.
    uint32 passStart = getMSTime();
    uint32 budgetMs = sPlayerbotAIConfig.randomBotProcessBudgetMs;
    bool overBudget = false;

    while (botCount++ < maxAllowedBotCount)
    {
        if (budgetMs && getMSTimeDiff(passStart, getMSTime()) >= budgetMs)
        {
            overBudget = true;
            break;
        }

        bool alliance = botCount % 2;
        uint32 bot = AddRandomBot(alliance);
        if (bot)
        {
            if (alliance)
            {
                allianceNewBots++;
            }
            else
            {
                hordeNewBots++;
            }

            bots.push_back(bot);
        }
        else
        {
            break;
        }
    }

    // Resume from the bot after the one last examined so a pass that stops early on
    // its time budget or per-interval cap still works its way through every bot over
    // successive passes instead of repeatedly re-processing the head of the list.
    int botProcessed = 0;
    list<uint32>::iterator i = bots.begin();
    if (m_processBotCursor)
    {
        list<uint32>::iterator cursor = find(bots.begin(), bots.end(), m_processBotCursor);
        if (cursor != bots.end())
        {
            i = cursor;
            if (++i == bots.end())
            {
                i = bots.begin();
            }
        }
    }

    size_t examined = 0;
    for (; examined < bots.size(); ++examined)
    {
        if (budgetMs && getMSTimeDiff(passStart, getMSTime()) >= budgetMs)
        {
            overBudget = true;
            break;
        }

        uint32 bot = *i;
        m_processBotCursor = bot;

        if (ProcessBot(bot))
        {
            botProcessed++;
        }

        if (botProcessed >= randomBotsPerInterval)
        {
            break;
        }

        if (++i == bots.end())
        {
            i = bots.begin();
        }
    }

    // Still work left this pass - come back quickly instead of waiting a full interval.
    if (overBudget && sPlayerbotAIConfig.randomBotCatchupInterval < sPlayerbotAIConfig.randomBotUpdateInterval)
    {
        SetNextCheckDelay(sPlayerbotAIConfig.randomBotCatchupInterval * 1000);
    }

    // Report examined as well as processed: every bot the pass walks past pays for its
    // event lookups whether or not it turns out to have work, so "processed" alone hides
    // most of what the budget actually went on.
    sLog.outString("%d bots processed, %u examined%s. %d alliance and %d horde bots added. %d bots online. Next check in %d seconds",
        botProcessed, (uint32)examined, overBudget ? " (budget reached, more pending)" : "", allianceNewBots, hordeNewBots, playerBots.size(),
        overBudget ? sPlayerbotAIConfig.randomBotCatchupInterval : sPlayerbotAIConfig.randomBotUpdateInterval);

    if (processTicks++ == 1)
    {
        PrintStats();
    }
}

uint32 RandomPlayerbotMgr::GetStartZoneForRace(uint32 race)
{
    std::map<uint32, uint32>::const_iterator cached = m_raceStartZones.find(race);
    if (cached != m_raceStartZones.end())
    {
        return cached->second;
    }

    uint32 zoneId = 0;
    for (uint32 cls = 1; cls < MAX_CLASSES; ++cls)
    {
        if (PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, cls))
        {
            zoneId = info->areaId;
            break;
        }
    }

    m_raceStartZones[race] = zoneId;
    return zoneId;
}

uint32 RandomPlayerbotMgr::PickForStarterZoneQuota(vector<uint32>& bots)
{
    uint32 quota = sPlayerbotAIConfig.randomBotStarterZoneQuota;
    if (!quota || !sPlayerbotAIConfig.randomBotStarterZonePct)
    {
        return 0;
    }

    // How many residents each starting zone currently has in the ACTIVE roster. A
    // percentage decides who MAY live in a starting zone; only this decides who actually
    // does, because admission draws uniformly from every free character of a faction and
    // nothing made it prefer the ones that would populate an empty zone. On a roster of
    // 450 with 101 active, that left Teldrassil and Mulgore with none at all -- not
    // because placement failed, but because no qualifying night elf or tauren was ever
    // selected to log in.
    // Recomputed at most once per pass rather than once per admission: a pass that adds
    // several bots was otherwise running this same aggregate for each of them. The tally
    // is then kept current in memory as bots are chosen, so a pass admitting several
    // still spreads them across zones instead of sending them all to the same one.
    if (m_starterZoneCountsPass != processTicks || m_starterZoneCounts.empty())
    {
        m_starterZoneCounts.clear();
        m_starterZoneCountsPass = processTicks;

        std::map<uint32, uint32> present;
        QueryResult* results = CharacterDatabase.PQuery(
            "SELECT `c`.`race`, COUNT(*) FROM `ai_playerbot_random_bots` `e` "
            "JOIN `characters` `c` ON `c`.`guid` = `e`.`bot` "
            "WHERE `e`.`event` = 'add' AND `e`.`owner` = 0 AND (`e`.`bot` %% 100) < '%u' "
            "GROUP BY `c`.`race`",
            sPlayerbotAIConfig.randomBotStarterZonePct);

        if (results)
        {
            do
            {
                Field* fields = results->Fetch();
                uint32 zone = GetStartZoneForRace(fields[0].GetUInt32());
                if (zone)
                {
                    present[zone] += fields[1].GetUInt32();
                }
            } while (results->NextRow());
            delete results;
        }

        m_starterZoneCounts = present;
    }

    std::map<uint32, uint32>& present = m_starterZoneCounts;

    // Take the emptiest zone that this candidate pool can actually fill, so the six
    // starting zones fill evenly rather than whichever race happens to be drawn first.
    uint32 bestBot = 0;
    uint32 bestShortfall = 0;

    for (vector<uint32>::const_iterator i = bots.begin(); i != bots.end(); ++i)
    {
        uint32 guid = *i;
        if ((guid % 100) >= sPlayerbotAIConfig.randomBotStarterZonePct)
        {
            continue;   // not a resident, cannot help a quota
        }

        uint32 zone = m_botStartZones.count(guid) ? m_botStartZones[guid] : 0;
        if (!zone)
        {
            continue;
        }

        uint32 have = present.count(zone) ? present[zone] : 0;
        if (have >= quota)
        {
            continue;
        }

        uint32 shortfall = quota - have;
        if (shortfall > bestShortfall)
        {
            bestShortfall = shortfall;
            bestBot = guid;
        }
    }

    if (bestBot)
    {
        // Count it immediately. The caller is about to admit this bot, and the next call
        // in the same pass must see the zone as one fuller or it would keep choosing the
        // same one until the tally is rebuilt next pass.
        ++present[m_botStartZones[bestBot]];
        sLog.outDetail("Starter-zone quota short by %u; admitting resident bot %u", bestShortfall, bestBot);
    }

    return bestBot;
}

uint32 RandomPlayerbotMgr::AddRandomBot(bool alliance)
{
    vector<uint32> bots = GetFreeBots(alliance);
    if (bots.size() == 0)
    {
        sLog.outBasic("No free %s bots to add (%u bot accounts known)",
            alliance ? "alliance" : "horde", (uint32)sPlayerbotAIConfig.randomBotAccounts.size());
        return 0;
    }

    // Fill a starving starting zone before drawing at random, and only then.
    uint32 bot = PickForStarterZoneQuota(bots);
    if (!bot)
    {
        int index = urand(0, bots.size() - 1);
        bot = bots[index];
    }
    SetEventValue(bot, "add", 1, urand(sPlayerbotAIConfig.minRandomBotInWorldTime, sPlayerbotAIConfig.maxRandomBotInWorldTime));
    uint32 randomTime = 30 + urand(sPlayerbotAIConfig.randomBotUpdateInterval, sPlayerbotAIConfig.randomBotUpdateInterval * 3);
    ScheduleRandomize(bot, randomTime);
    sLog.outDetail("Random bot %d added", bot);
    return bot;
}

void RandomPlayerbotMgr::ScheduleRandomize(uint32 bot, uint32 time)
{
    SetEventValue(bot, "randomize", 1, time);
    SetEventValue(bot, "logout", 1, time + 30 + urand(sPlayerbotAIConfig.randomBotUpdateInterval, sPlayerbotAIConfig.randomBotUpdateInterval * 3));
}

void RandomPlayerbotMgr::ScheduleTeleport(uint32 bot)
{
    SetEventValue(bot, "teleport", 1, 60 + urand(sPlayerbotAIConfig.randomBotUpdateInterval, sPlayerbotAIConfig.randomBotUpdateInterval * 3));
}

bool RandomPlayerbotMgr::ProcessBot(uint32 bot)
{
    uint32 isValid = GetEventValue(bot, "add");
    if (!isValid)
    {
        Player* player = GetPlayerBot(bot);
        if (!player || !player->GetGroup())
        {
            if (sPlayerbotAIConfig.randomBotKeepGroups && m_groupedBots.count(bot))
            {
                SetEventValue(bot, "add", 1, sPlayerbotAIConfig.maxRandomBotInWorldTime);
            }
            else
            {
                sLog.outDetail("Bot %d expired", bot);
                SetEventValue(bot, "add", 0, 0);
            }
        }
        return true;
    }

    if (!GetPlayerBot(bot))
    {
        sLog.outDetail("Bot %d logged in", bot);
        AddPlayerBot(bot, 0);
        if (!GetEventValue(bot, "online"))
        {
            SetEventValue(bot, "online", 1, sPlayerbotAIConfig.minRandomBotInWorldTime);
        }
        return true;
    }

    Player* player = GetPlayerBot(bot);
    if (!player)
    {
        return false;
    }

    PlayerbotAI* ai = player->GetPlayerbotAI();
    if (!ai)
    {
        return false;
    }

    if (player->GetGroup())
    {
        sLog.outDetail("Skipping bot %d as it is in group", bot);
        return false;
    }

    if (player->IsDead())
    {
        if (!GetEventValue(bot, "dead"))
        {
            sLog.outDetail("Setting dead flag for bot %d", bot);
            uint32 randomTime = urand(sPlayerbotAIConfig.minRandomBotReviveTime, sPlayerbotAIConfig.maxRandomBotReviveTime);
            SetEventValue(bot, "dead", 1, randomTime);
            // "Revive a minute before the dead flag lapses" -- but both are uint32, so any
            // revive time under 60 wrapped this to about 136 years. The revive event then
            // never expired, the branch below never ran, and the bot stayed a ghost until
            // the dead flag lapsed and reset the pair, forever. Nobody hit it because the
            // shipped minimum is exactly 60; anyone lowering it to get bots back on their
            // feet sooner would have got the precise opposite.
            SetEventValue(bot, "revive", 1, randomTime > 60 ? randomTime - 60 : 0);
            return false;
        }

        if (!GetEventValue(bot, "revive"))
        {
            sLog.outDetail("Reviving dead bot %d", bot);
            SetEventValue(bot, "dead", 0, 0);
            SetEventValue(bot, "revive", 0, 0);
            RandomTeleport(player, player->GetMapId(), player->GetPositionX(), player->GetPositionY(), player->GetPositionZ());
            return true;
        }

        return false;
    }

    uint32 randomize = GetEventValue(bot, "randomize");
    if (!randomize)
    {
        sLog.outDetail("Randomizing bot %d", bot);
        // Bank the event only when the bot was actually randomized. Spending it either way
        // is what let one failed pass cost a bot its level, gear, talents and trainer
        // spells for up to a fortnight, because nothing looks at it again until the event
        // lapses. Same rule the teleport branches already follow: only spend the event if
        // the thing it records really happened.
        if (!Randomize(player))
        {
            sLog.outDetail("Randomizing bot %d did not take; will retry", bot);
            return true;
        }

        uint32 randomTime = urand(sPlayerbotAIConfig.minRandomBotRandomizeTime, sPlayerbotAIConfig.maxRandomBotRandomizeTime);
        ScheduleRandomize(bot, randomTime);
        return true;
    }

    uint32 logout = GetEventValue(bot, "logout");
    if (!logout)
    {
        sLog.outDetail("Logging out bot %d", bot);
        LogoutPlayerBot(bot);
        SetEventValue(bot, "logout", 1, sPlayerbotAIConfig.maxRandomBotInWorldTime);
        return true;
    }

    // A resident that has outgrown the band is put back to the start here, before
    // anything else can move it. IncreaseLevel already does this, but IncreaseLevel only
    // runs when the randomize event lapses, which is somewhere between two hours and a
    // fortnight away -- so it never governs real levelling. Bots earn ordinary kill,
    // quest and exploration experience, and Player::GiveXP calls GiveLevel straight out
    // on the map worker without consulting this manager at all. A resident rolled to 10
    // therefore reaches 11 by simply playing, and at 11 both home-confinement tests stop
    // applying, so the very next eviction or teleport event sends it off to a
    // level-appropriate zone somewhere else entirely. That is the drain: not the manager
    // levelling residents out, but residents levelling themselves out from underneath it.
    //
    // Repaired on the world thread, where the roster is owned, rather than by hooking the
    // level-up on the map worker.
    if (IsStarterZoneResident(player) &&
        sPlayerbotAIConfig.randomBotHomeZoneMaxLevel &&
        player->getLevel() > sPlayerbotAIConfig.randomBotHomeZoneMaxLevel)
    {
        sLog.outDetail("Resident bot %d outgrew the starting band at level %u; returning it",
            bot, player->getLevel());
        RandomizeStarterResident(player);
        return true;
    }

    // No timer gates this check, so a bot with nowhere valid to go re-ran the whole
    // search on every pass: a hundred game_tele draws and a GetZoneLevel query apiece,
    // once a minute, for as long as it stood there. Only a failure needs the backoff --
    // an eviction that worked leaves the bot somewhere this same check accepts, so
    // holding a cooldown over it would only delay the next legitimate move.
    if (!GetEventValue(bot, "evictcheck") &&
        !IsZoneSafeForBot(player, player->GetMapId(), player->GetPositionX(),
        player->GetPositionY(), player->GetPositionZ()))
    {
        sLog.outDetail("Bot %d is in unsafe zone, forcing teleport", bot);
        if (RandomTeleportForLevel(player))
        {
            SetEventValue(bot, "teleport", 1, sPlayerbotAIConfig.maxRandomBotInWorldTime);
        }
        else
        {
            SetEventValue(bot, "evictcheck", 1, 10 * sPlayerbotAIConfig.randomBotUpdateInterval);
        }
        return true;
    }

    // Check if bot level is outside configured min/max range
    uint32 botLevel = player->getLevel();
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    }
    if (botLevel < sPlayerbotAIConfig.randomBotMinLevel || botLevel > maxLevel)
    {
        sLog.outDetail("Bot %d level %d is outside valid range (%d-%d), scheduling immediate re-randomization",
            bot, botLevel, sPlayerbotAIConfig.randomBotMinLevel, maxLevel);
        ScheduleRandomize(bot, 0);
        return true;
    }

    uint32 teleport = GetEventValue(bot, "teleport");
    if (!teleport)
    {
        sLog.outDetail("Random teleporting bot %d", bot);
        // ee37c57e made the eviction branch above wait for a successful teleport before
        // recording one, and left this branch still banking it either way -- so a bot
        // that could not be placed had its next move suppressed for maxRandomBotInWorldTime
        // anyway. Same rule here: only spend the event if the bot actually moved.
        if (RandomTeleportForLevel(ai->GetBot()))
        {
            SetEventValue(bot, "teleport", 1, sPlayerbotAIConfig.maxRandomBotInWorldTime);
        }
        return true;
    }

    return false;
}

bool RandomPlayerbotMgr::RandomTeleport(Player* bot, vector<WorldLocation> &locs)
{
    if (bot->IsBeingTeleported())
    {
        return false;
    }

    if (locs.empty())
    {
        sLog.outError("Cannot teleport bot %s - no locations available", bot->GetName());
        return false;
    }

    for (int attemtps = 0; attemtps < 10; ++attemtps)
    {
        int index = urand(0, locs.size() - 1);
        WorldLocation loc = locs[index];
        float x = loc.coord_x + urand(0, sPlayerbotAIConfig.grindDistance) - sPlayerbotAIConfig.grindDistance / 2;
        float y = loc.coord_y + urand(0, sPlayerbotAIConfig.grindDistance) - sPlayerbotAIConfig.grindDistance / 2;
        float z = loc.coord_z;

        Map* map = sMapMgr.FindMap(loc.mapid);
        if (!map)
        {
            continue;
        }

        const TerrainInfo * terrain = map->GetTerrain();
        if (!terrain)
        {
            continue;
        }

        // Snap before judging, not after. x and y have just been jittered by up to half a
        // grindDistance -- fifty yards with the shipped setting -- while z is still the
        // height of an anchor that far away, so every test run against it was asking about
        // a point in mid-air or underground. StaticFloor then searched a column centred on
        // that same wrong height and returned whatever surface it found there, which under
        // a building is the ground beneath it. That is how a bot ends up inside a structure
        // or below one, and it is what a player teleporting to it falls through.
        std::optional<float> floor = terrain->StaticFloor(x, y, 0.5f + z);
        if (!floor)
        {
            continue;
        }

        z = 0.05f + *floor;

        AreaTableEntry const* area = sAreaStore.LookupEntry(terrain->GetAreaId(x, y, z));
        if (!area)
        {
            continue;
        }

        // Now that z is where the bot would actually stand. IsOutdoors reads the WMO group
        // flags at the point given, so asking it here is what rejects a floor inside a
        // building or the ground under one -- the previous order asked it about the sky
        // above the roof and was satisfied.
        if (!terrain->IsOutdoors(x, y, z) ||
            terrain->IsUnderWater(x, y, z) ||
            terrain->IsInWater(x, y, z))
        {
            continue;
        }

        sLog.outDetail("Random teleporting bot %s to %s %f,%f,%f", bot->GetName(), area->AreaName_lang[0], x, y, z);

        // ProcessBot judges the spot the bot is standing on, not the anchor it was
        // sampled around, and the two are up to randomBotTeleportDistance/2 plus a
        // grindDistance/2 jitter apart. Vetting only the anchor lets an eviction drop
        // the bot somewhere the next pass evicts it from again, a minute later.
        if (!IsZoneSafeForBot(bot, loc.mapid, x, y, z))
        {
            continue;
        }

        bot->GetMotionMaster()->Clear();
        bot->TeleportTo(loc.mapid, x, y, z, 0);
        return true;
    }

    sLog.outError("Cannot teleport bot %s - no locations available", bot->GetName());
    return false;
}

uint32 RandomPlayerbotMgr::FindStartSubArea(uint32 mapId, uint32 zoneId, float x, float y, float z)
{
    if (!zoneId)
    {
        return 0;
    }

    Map* map = const_cast<Map*>(sMapMgr.FindMap(mapId));
    if (!map || !map->GetTerrain())
    {
        return 0;
    }

    // The newbie sub-area is whichever one holds the ordinary creatures closest to where
    // the race begins. Elites are excluded for the same reason they are excluded
    // everywhere else here: a level 1 is not going to be fighting them, so an area known
    // only by its elites is not the area we are looking for.
    uint32 best = 0;
    float bestDist = 0.0f;

    CreatureDataMap const* creatureDataMap = sObjectMgr.GetCreatureDataMap();
    for (CreatureDataMap::const_iterator itr = creatureDataMap->begin(); itr != creatureDataMap->end(); ++itr)
    {
        CreatureData const& data = itr->second;
        if (data.mapid != mapId)
        {
            continue;
        }

        CreatureInfo const* cInfo = sObjectMgr.GetCreatureTemplate(data.id);
        if (!cInfo || cInfo->Rank > CREATURE_ELITE_NORMAL)
        {
            continue;
        }

        float dx = data.posX - x;
        float dy = data.posY - y;
        float dist = dx * dx + dy * dy;
        if (best && dist >= bestDist)
        {
            continue;
        }

        uint32 spawnArea = 0;
        uint32 spawnZone = 0;
        map->GetTerrain()->GetZoneAndAreaId(spawnZone, spawnArea, data.posX, data.posY, data.posZ);

        // Only a genuine sub-area of the race's own starting zone qualifies. A spawn out
        // in the open zone answers with the zone itself and tells us nothing new.
        if (spawnZone != zoneId || spawnArea == zoneId || !spawnArea)
        {
            continue;
        }

        best = spawnArea;
        bestDist = dist;
    }

    return best;
}

RandomPlayerbotMgr::RacialStart RandomPlayerbotMgr::GetRacialStart(Player* bot)
{
    uint32 race = bot->getRace();

    std::map<uint32, RacialStart>::const_iterator cached = m_racialStarts.find(race);
    if (cached != m_racialStarts.end())
    {
        return cached->second;
    }

    // playercreateinfo is per race AND class, but the starting position is a property of
    // the race: all eight agree across every class they can be. Take the bot's own class
    // and fall back to scanning for any class the race has, so a race whose create info is
    // incomplete for one class still resolves.
    PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, bot->getClass());
    for (uint32 cls = 1; !info && cls < MAX_CLASSES; ++cls)
    {
        info = sObjectMgr.GetPlayerInfo(race, cls);
    }

    RacialStart start;
    start.zoneId = info ? info->areaId : 0;
    start.areaId = 0;

    // playercreateinfo's zone column is the zone, not the sub-area a new character
    // actually opens their eyes in: a night elf gets Teldrassil, not Shadowglen, and the
    // sub-area is recorded nowhere.
    //
    // Asking the create position alone is not enough, and that cost a run. The undead
    // start at 1676,1678 inside the Deathknell crypt, and that point answers Tirisfal
    // Glades rather than Deathknell, so the sub-area came back equal to the zone and no
    // Deathknell landing pool was ever built. Take the position's own answer when it
    // names a genuine sub-area, and otherwise ask which sub-area the nearest newbie
    // creatures actually stand in -- which is the better question regardless, because a
    // landing site is a creature spawn.
    if (info)
    {
        Map* map = const_cast<Map*>(sMapMgr.FindMap(info->mapId));
        if (map && map->GetTerrain())
        {
            uint32 atPoint = map->GetTerrain()->GetAreaId(info->positionX, info->positionY, info->positionZ);
            if (atPoint && atPoint != start.zoneId)
            {
                start.areaId = atPoint;
            }
            else
            {
                start.areaId = FindStartSubArea(info->mapId, start.zoneId,
                    info->positionX, info->positionY, info->positionZ);
            }
        }
    }

    m_racialStarts[race] = start;
    return start;
}

uint32 RandomPlayerbotMgr::GetRacialStartZone(Player* bot)
{
    return GetRacialStart(bot).zoneId;
}

bool RandomPlayerbotMgr::RandomTeleportHome(Player* bot)
{
    RacialStart start = GetRacialStart(bot);
    if (!start.zoneId)
    {
        return false;
    }

    if (m_areaCreatureStatsMap.empty())
    {
        CalculateAreaCreatureStats();
    }

    // A brand new character spends its first few levels inside the one sub-area --
    // Shadowglen, Northshire, Coldridge Valley -- and only then works outward into the
    // zone around it, so the pool it draws from narrows the same way. Preference, not
    // requirement: if the sub-area cannot take the bot, the zone still can, and a bot
    // placed slightly too far out is enormously better than one that cannot be placed at
    // all. Camp Narache carries 17 landing sites against Mulgore's 1779, so a sub-area
    // running out of usable ground is the ordinary case rather than the exotic one.
    uint32 tightest = 0;
    if (start.areaId && sPlayerbotAIConfig.randomBotHomeAreaMaxLevel &&
        bot->getLevel() <= sPlayerbotAIConfig.randomBotHomeAreaMaxLevel)
    {
        tightest = start.areaId;
    }

    if (tightest)
    {
        std::map<uint32, std::vector<WorldLocation> >::iterator sub = m_homeZoneAnchors.find(tightest);
        if (sub != m_homeZoneAnchors.end() && !sub->second.empty() && RandomTeleport(bot, sub->second))
        {
            Refresh(bot);
            return true;
        }
    }

    std::map<uint32, std::vector<WorldLocation> >::iterator anchors = m_homeZoneAnchors.find(start.zoneId);
    if (anchors == m_homeZoneAnchors.end() || anchors->second.empty())
    {
        return false;
    }

    // Drawing from a pool that is already confined to the right zone, rather than sifting
    // the whole game_tele list for the handful of entries that happen to land in it. The
    // generic search draws blind from a map's worth of anchors, so once a bot is confined
    // to one zone the odds of hitting it are poor enough that a run of a hundred attempts
    // can still come up empty -- which is how the last over-strict filter produced
    // "Cannot teleport bot" for every bot on the roster.
    //
    // Refresh on success for the same reason the generic teleport path does it: a bot
    // arrives with whatever health, mana, durability and combat references it had where it
    // left, and without this the home path was the one route that skipped the cleanup.
    if (RandomTeleport(bot, anchors->second))
    {
        Refresh(bot);
        return true;
    }

    return false;
}

bool RandomPlayerbotMgr::RandomTeleportForLevel(Player* bot)
{
    // A bot young enough to still be in its own newbie zone never gets sent anywhere
    // else. The generic search below would happily draw an anchor on the far continent,
    // and IsZoneSafeForBot would then reject it for exactly this reason, so going
    // straight to the home pool saves the wasted attempts as well as getting it right.
    if (sPlayerbotAIConfig.randomBotHomeZoneMaxLevel &&
        bot->getLevel() <= sPlayerbotAIConfig.randomBotHomeZoneMaxLevel)
    {
        if (RandomTeleportHome(bot))
        {
            return true;
        }
        // Falling through on failure is deliberate: a missing pool must not strand the
        // bot where it is. The generic search still applies the same home-zone rule
        // through IsZoneSafeForBot, so it cannot place the bot outside its zone -- it
        // just costs more attempts to find a spot.
    }

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        int index = urand(0, sPlayerbotAIConfig.randomBotMaps.size() - 1);
        uint32 mapId = sPlayerbotAIConfig.randomBotMaps[index];

        vector<GameTele const*> locs;
        GameTeleMap const& teleMap = sObjectMgr.GetGameTeleMap();
        for (GameTeleMap::const_iterator itr = teleMap.begin(); itr != teleMap.end(); ++itr)
        {
            GameTele const* tele = &itr->second;
            if (tele->mapId == mapId)
            {
                locs.push_back(tele);
            }
        }

        if (locs.empty())
        {
            continue;
        }

        index = urand(0, locs.size() - 1);
        if (index >= locs.size())
        {
            return false;
        }

        GameTele const* tele = locs[index];
        uint32 level = GetZoneLevel(tele->mapId, tele->position_x, tele->position_y, tele->position_z);
        // The zone has to suit the bot from both directions. Only the upper bound was
        // checked, so a level 52 bot could be anchored in Elwynn and then be evicted
        // from it on the next pass for sitting outside the level band. Bound the zone
        // level itself rather than vetting the anchor against the bot's level: almost
        // no game_tele sits in an area whose creature stats bracket a given bot, so
        // doing it there rejects every one of the 100 attempts and costs a
        // GetZoneLevel query each time. The landing point is the authoritative check.
        if ((level > bot->getLevel() + sPlayerbotAIConfig.randomBotTeleLevel) ||
          (level + sPlayerbotAIConfig.randomBotTeleLevel < bot->getLevel()) ||
          (level < sPlayerbotAIConfig.randomBotMinLevel) ||
          (!IsZoneSafeForBot(bot, tele->mapId, tele->position_x, tele->position_y, tele->position_z, level)))
        {
            continue;
        }

        if (RandomTeleport(bot, tele->mapId, tele->position_x, tele->position_y, tele->position_z))
        {
            return true;
        }
    }

    sLog.outError("Cannot teleport bot %s - no locations available", bot->GetName());
    return false;
}

bool RandomPlayerbotMgr::RandomTeleport(Player* bot, uint32 mapId, float teleX, float teleY, float teleZ)
{
    vector<WorldLocation> locs;
    QueryResult* results = WorldDatabase.PQuery("SELECT `position_x`, `position_y`, `position_z` FROM `creature` WHERE `map` = '%u' AND ABS(`position_x` - '%f') < '%u' AND ABS(`position_y` - '%f') < '%u'",
        mapId, teleX, sPlayerbotAIConfig.randomBotTeleportDistance / 2, teleY, sPlayerbotAIConfig.randomBotTeleportDistance / 2);
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            float x = fields[0].GetFloat();
            float y = fields[1].GetFloat();
            float z = fields[2].GetFloat();
            // Offer the sampler none but safe candidates, as this did before the
            // spawn-query rewrite. Rejecting after the draw instead gives the ten
            // attempts nothing to find when the anchor sits near a band edge.
            if (!IsZoneSafeForBot(bot, mapId, x, y, z))
            {
                continue;
            }
            WorldLocation loc(mapId, x, y, z, 0);
            locs.push_back(loc);
        } while (results->NextRow());
        delete results;
    }

    // Refresh regardless: the dead-bot path calls this to revive in place, and a bot
    // that could not be relocated still has to come back alive where it stands.
    bool moved = RandomTeleport(bot, locs);
    Refresh(bot);
    return moved;
}

bool RandomPlayerbotMgr::Randomize(Player* bot)
{
    if (bot->getLevel() == 1)
    {
        return RandomizeFirst(bot);
    }

    IncreaseLevel(bot);
    return true;
}

void RandomPlayerbotMgr::IncreaseLevel(Player* bot)
{
    uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    uint32 botCap = sPlayerbotAIConfig.randomBotMaxLevel;
    if (botCap > maxLevel)
    {
        botCap = maxLevel;
    }

    // A resident that has outgrown the starting band goes back to the beginning rather
    // than graduating out of the zone. This only governs the manager's own slow
    // increment; it is not the real guard, because it runs only when the randomize event
    // lapses. Levelling through ordinary play is caught by the equivalent check in
    // ProcessBot, which runs every pass.
    if (IsStarterZoneResident(bot) &&
        sPlayerbotAIConfig.randomBotHomeZoneMaxLevel &&
        bot->getLevel() >= sPlayerbotAIConfig.randomBotHomeZoneMaxLevel)
    {
        RandomizeStarterResident(bot);
        return;
    }

    if (bot->getLevel() >= botCap)
    {
        RandomizeFirst(bot);
        return;
    }

    uint32 level = min((uint32)(bot->getLevel() + 1), maxLevel);
    PlayerbotFactory factory(bot, level);
    if (bot->GetGuildId())
    {
        factory.Refresh();
    }
    else
    {
        factory.Randomize();
    }
    RandomTeleportForLevel(bot);
}

bool RandomPlayerbotMgr::IsStarterZoneResident(Player* bot)
{
    if (!sPlayerbotAIConfig.randomBotStarterZonePct)
    {
        return false;
    }

    // Decided from the bot's own guid rather than rolled, so it is the same answer every
    // time it is asked -- across a randomize, a relog and a restart. A rolled residency
    // would move a bot in and out of its starting zone every time it levelled, which is
    // the opposite of the point. It also needs no storage and no extra event.
    return (bot->GetGUIDLow() % 100) < sPlayerbotAIConfig.randomBotStarterZonePct;
}

bool RandomPlayerbotMgr::RandomizeStarterResident(Player* bot)
{
    uint32 band = sPlayerbotAIConfig.randomBotHomeZoneMaxLevel;
    if (!band)
    {
        band = 10;
    }

    uint32 level = urand(1, band);
    sLog.outDetail("Bot %s is a starting-zone resident; randomizing at level %u",
        bot->GetName(), level);

    PlayerbotFactory factory(bot, level);
    factory.CleanRandomize();

    // Home first, because that is the whole intent. The generic search is the fallback
    // and, being level-banded, it lands them somewhere a bot of this level belongs anyway.
    if (!RandomTeleportHome(bot))
    {
        RandomTeleportForLevel(bot);
    }

    return true;
}

bool RandomPlayerbotMgr::RandomizeFirst(Player* bot)
{
    // A share of the roster lives in the zone its race starts in and stays there. Without
    // this the starting zones drain within minutes of a restart: the confinement rules
    // hold a bot in its home zone only while it IS low level, and RandomizeFirst rolls a
    // level from a randomly chosen destination, so every bot promptly levels out of the
    // band and leaves. Watched happen on a fresh roster -- Teldrassil emptied as its bots
    // were levelled and teleported away one after another.
    if (IsStarterZoneResident(bot))
    {
        return RandomizeStarterResident(bot);
    }

    bool randomized = false;
    uint32 maxLevel = sPlayerbotAIConfig.randomBotMaxLevel;
    if (maxLevel > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    }

    for (int attempt = 0; attempt < 100; ++attempt)
    {
        int index = urand(0, sPlayerbotAIConfig.randomBotMaps.size() - 1);
        uint32 mapId = sPlayerbotAIConfig.randomBotMaps[index];

        vector<GameTele const*> locs;
        GameTeleMap const & teleMap = sObjectMgr.GetGameTeleMap();
        for (GameTeleMap::const_iterator itr = teleMap.begin(); itr != teleMap.end(); ++itr)
        {
            GameTele const* tele = &itr->second;
            if (tele->mapId == mapId)
            {
                locs.push_back(tele);
            }
        }
        if (locs.empty()) // no safe locations found, so try another map
        {
            continue;
        }

        index = urand(0, locs.size() - 1);
        if (index >= locs.size())
        {
            // Leaves the loop rather than the function, so the bot still reaches the
            // randomize-in-place fallback below instead of walking away untouched.
            break;
        }
        GameTele const* tele = locs[index];
        uint32 level = GetZoneLevel(tele->mapId, tele->position_x, tele->position_y, tele->position_z);
        if (level > maxLevel + 5)
        {
            continue;
        }

        level = min(level, maxLevel);
        if (!level)
        {
            level = 1;
        }

        // only create a high level mob if they are in a high level zone
        if ((urand(0, 100) < 100 * sPlayerbotAIConfig.randomBotMaxLevelChance) && level >= 40)
        {
            level = maxLevel;
        }

        if (level < sPlayerbotAIConfig.randomBotMinLevel)
        {
            continue;
        }

        if (!IsZoneSafeForBot(bot, tele->mapId, tele->position_x, tele->position_y, tele->position_z, level))
        {
            continue;
        }

        // Everything above is a cheap check, so the search costs little however many
        // attempts it takes. CleanRandomize is the opposite -- talents, spells, inventory,
        // equipment and four SaveToDB -- so it runs exactly once, after a destination has
        // passed every test, and the loop ends whether or not the move then succeeds.
        //
        // It sat inside the retry until now, which meant a teleport that returned false
        // re-randomized the bot and went round again: up to a hundred full re-gears for
        // one bot inside a single pass, and the 50ms budget is only checked between bots,
        // so the tail was a multi-second stall of the world thread. RandomTeleport
        // returning false for something as ordinary as an in-flight IsBeingTeleported made
        // that reachable, not theoretical.
        //
        // If the placement does fail the bot keeps its new level and stays put; the
        // eviction branch in ProcessBot will judge it and move it on a later pass, which
        // is the same mechanism that handles every other badly-placed bot.
        PlayerbotFactory factory(bot, level);
        factory.CleanRandomize();
        RandomTeleport(bot, tele->mapId, tele->position_x, tele->position_y, tele->position_z);
        randomized = true;
        break;
    }

    // The search is allowed to find nothing, and when it does the bot must still be
    // given a level, talents, spells and gear. Leaving without randomizing is how an
    // entire roster ended up at level 1 wearing its create-info shirt: the loop failed a
    // hundred times, fell out here, and the caller banked the "randomize" event anyway,
    // so nothing tried again for up to a fortnight. A bot that cannot be placed somewhere
    // chosen is still randomized where it stands, and the eviction branch in ProcessBot
    // moves it later like any other badly-placed bot.
    if (!randomized)
    {
        uint32 level = urand(sPlayerbotAIConfig.randomBotMinLevel, maxLevel);
        if (!level)
        {
            level = 1;
        }

        sLog.outDetail("No destination passed for bot %s; randomizing in place at level %u",
            bot->GetName(), level);

        PlayerbotFactory factory(bot, level);
        factory.CleanRandomize();
        RandomTeleportForLevel(bot);
        randomized = true;
    }

    if (bot->getLevel() > maxLevel)
    {
        uint32 newLevel =  urand(sPlayerbotAIConfig.randomBotMinLevel, maxLevel);
        PlayerbotFactory factory(bot, newLevel);
        factory.CleanRandomize();
        RandomTeleportForLevel(bot);
    }

    return randomized;
}

uint32 RandomPlayerbotMgr::GetZoneLevel(uint32 mapId, float teleX, float teleY, float teleZ)
{
    uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);

    // This was an AVG() over a creature x creature_template join whose ABS() predicates
    // no index can serve, so it scanned every spawn on the continent -- and
    // RandomTeleportForLevel calls it once per attempt, up to a hundred times for a
    // single teleport. CalculateAreaCreatureStats already holds the same levels per area
    // in memory, built once from the object manager without touching the database. It is
    // also the map IsZoneSafeForBot judges by, so sourcing both from it makes the anchor
    // filter and the safety check agree instead of measuring two different things.
    if (m_areaCreatureStatsMap.empty())
    {
        CalculateAreaCreatureStats();
    }

    uint32 level = 0;
    Map* map = sMapMgr.FindMap(mapId);
    const TerrainInfo* terrain = map ? map->GetTerrain() : NULL;
    if (terrain)
    {
        // Per position, not per grid cell -- see the note in IsZoneSafeForBot.
        uint32 areaId = terrain->GetAreaId(teleX, teleY, teleZ);

        std::map<uint32, AreaCreatureStats>::const_iterator statsItr = m_areaCreatureStatsMap.find(areaId);
        if (statsItr != m_areaCreatureStatsMap.end() && statsItr->second.creatureCount > 0)
        {
            level = urand(statsItr->second.minLevel, statsItr->second.maxLevel);
        }
    }

    if (!level)
    {
        // What the query path did when it had nothing to average. An area with no stats
        // is one IsZoneSafeForBot rejects anyway, so this only costs a wasted attempt.
        level = urand(1, maxLevel);
    }

    if (level > maxLevel)
    {
        level = maxLevel;
    }

    return level;
}

void RandomPlayerbotMgr::Refresh(Player* bot)
{
    sLog.outDetail("Refreshing bot %s", bot->GetName());
    if (bot->IsDead())
    {
        // Resurrect the bot if it is dead
        PlayerbotChatHandler ch(bot);
        ch.revive(*bot);
        bot->GetPlayerbotAI()->ResetStrategies();
    }

    // Reset the bot's AI
    bot->GetPlayerbotAI()->Reset();

    // Clear all hostile references and combat states
    HostileReference *ref = bot->GetHostileRefManager().getFirst();
    while (ref)
    {
        ThreatManager *threatManager = ref->getSource();
        Unit *unit = threatManager->getOwner();
        float threat = ref->getThreat();

        unit->RemoveAllAttackers();
        unit->ClearInCombat();

        ref = ref->next();
    }

    bot->RemoveAllAttackers();
    bot->ClearInCombat();

    // Repair all items, set health and power to maximum, and enable PvP
    bot->DurabilityRepairAll(false, 1.0f);
    bot->SetHealthPercent(100);
    bot->SetPvP(true);

    if (bot->GetMaxPower(POWER_MANA) > 0)
    {
        bot->SetPower(POWER_MANA, bot->GetMaxPower(POWER_MANA));
    }

    if (bot->GetMaxPower(POWER_ENERGY) > 0)
    {
        bot->SetPower(POWER_ENERGY, bot->GetMaxPower(POWER_ENERGY));
    }
}

bool RandomPlayerbotMgr::IsRandomBot(Player* bot)
{
    if (!bot) return false;
    return IsRandomBot(bot->GetGUIDLow());
}

bool RandomPlayerbotMgr::IsRandomBot(uint32 bot)
{
    std::unordered_map<uint32, bool>::iterator it = m_randomBotCache.find(bot);
    if (it != m_randomBotCache.end())
    {
        return it->second;
    }

    // This is the one entry point map workers reach -- PlayerbotAI::UpdateAI, the trade
    // and grind values, AiFactory -- so it must not leave a new entry behind in
    // m_eventValueCache. Recording the miss there is a world-thread optimisation for
    // ProcessBot, which asks after several events per bot per pass; from here it would
    // add unsynchronised writes to a std::map that the world thread is reading and
    // writing at the same time, and a concurrent rebalance is a crash rather than a stale
    // read. Both reviewers flagged this independently.
    bool value = (GetEventValue(bot, "add", false) != 0);
    m_randomBotCache[bot] = value;
    return value;
}

list<uint32> RandomPlayerbotMgr::GetBots()
{
    list<uint32> bots;

    // Query the database to get the list of random bots
    QueryResult* results = CharacterDatabase.Query(
            "SELECT `bot` FROM `ai_playerbot_random_bots` WHERE `owner` = 0 AND `event` = 'add'");

    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            uint32 bot = fields[0].GetUInt32();
            bots.push_back(bot);
        } while (results->NextRow());
        delete results;
    }

    return bots;
}

vector<uint32> RandomPlayerbotMgr::GetFreeBots(bool alliance)
{
    set<uint32> bots;
    QueryResult* results = CharacterDatabase.PQuery(
            "SELECT `bot` FROM `ai_playerbot_random_bots` WHERE `event` = 'add'"
        );

    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            uint32 bot = fields[0].GetUInt32();
            bots.insert(bot);
        } while (results->NextRow());
        delete results;
    }

    vector<uint32> guids;
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        if (!sAccountMgr.GetCharactersCount(accountId))
        {
            continue;
        }

        QueryResult *result = CharacterDatabase.PQuery("SELECT `guid`, `race` FROM `characters` WHERE `account` = '%u'", accountId);
        if (!result)
        {
            continue;
        }

        do
        {
            Field* fields = result->Fetch();
            uint32 guid = fields[0].GetUInt32();
            uint32 race = fields[1].GetUInt32();
            if (bots.find(guid) == bots.end() &&
                ((alliance && IsAlliance(race)) || ((!alliance && !IsAlliance(race)))))
            {
                guids.push_back(guid);
                // Remembered here because the race is already in hand; the quota pass
                // would otherwise have to query it back per candidate.
                m_botStartZones[guid] = GetStartZoneForRace(race);
            }
        } while (result->NextRow());
        delete result;
    }

    return guids;
}

bool RandomPlayerbotMgr::IsZoneSafeForBot(Player* bot, uint32 mapId, float x, float y, float z, uint32 useLevel)
{
    Map* map = sMapMgr.FindMap(mapId);
    if (!map)
    {
        return false;
    }

    TerrainInfo const* terrain = map->GetTerrain();

    if (!terrain)
    {
        return false;
    }

    // Resolved per position rather than cached per grid cell. A cell is SIZE_OF_GRID_CELL
    // wide -- about 33 yards -- and area borders do not follow the grid, so a cell can
    // straddle two areas. Caching by cell let whichever position happened to be asked
    // first decide the area for every later position in it, which is harmless for a
    // rough level band and actively wrong for a faction or neutral-hub decision: it can
    // hand a border cell of the Barrens to Ratchet's neutrality, or hide Ratchet behind
    // the Barrens' owner. GetAreaId is a terrain lookup, and now that this path no longer
    // issues SQL it is not worth trading correctness to skip it.
    uint32 areaId = 0;
    uint32 zoneId = 0;
    terrain->GetZoneAndAreaId(zoneId, areaId, x, y, z);

    AreaTableEntry const* area = sAreaStore.LookupEntry(areaId);
    if (!area)
    {
        return true;
    }

    // A level 6 gnome standing in Teldrassil is a thing the game permits and a thing no
    // player does: reaching another race's starting zone at that level means crossing a
    // continent, which in practice meant being dragged there by somebody higher. The
    // random manager has no such story to tell, so below the threshold a bot is confined
    // to the zone its own race actually starts in. Compared against the zone rather than
    // the leaf area, so Coldridge Valley, Shadowglen, Deathknell and the rest all resolve
    // to the parent the racial start position names.
    // Judged at useLevel when the caller supplied one, exactly as the creature band below
    // is. That is not a detail: RandomizeFirst picks a destination, derives the level the
    // bot is ABOUT to be set to, and only then runs CleanRandomize to grant that level
    // with its talents, trainer spells and gear. Asking bot->getLevel() there asks about a
    // freshly created level 1, so every destination outside its racial start zone was
    // refused, all hundred attempts failed, and CleanRandomize never ran at all -- leaving
    // the entire roster stuck at level 1 with no gear and nothing but its create-info
    // spells. 840 of 900 bots sat at level 1 because of it.
    uint32 homeLevel = useLevel ? useLevel : bot->getLevel();
    if (sPlayerbotAIConfig.randomBotHomeZoneMaxLevel &&
        homeLevel <= sPlayerbotAIConfig.randomBotHomeZoneMaxLevel)
    {
        RacialStart start = GetRacialStart(bot);
        if (start.zoneId && zoneId != start.zoneId)
        {
            return false;
        }

        // The sub-area preference deliberately does NOT appear here. It belongs in
        // RandomTeleportHome, which chooses where to put a bot, and not in the test that
        // decides whether where a bot already stands is acceptable. Making it a rejection
        // criterion stranded 30 of 127 bots on the first run that used it: a level 1
        // undead could not be placed in Deathknell, because the racial start position
        // resolves to Tirisfal rather than to the sub-area and so no Deathknell pool was
        // ever built, and could not be placed anywhere else in Tirisfal either, because
        // the open zone's creature band starts around level 5 and the tolerance is 3. With
        // nowhere to go it re-ran the whole hundred-attempt search every pass and logged
        // "Cannot teleport bot" 56 times. Tauren hit the same wall against Camp Narache.
        // A confinement that can leave a bot with no legal position anywhere must be a
        // preference expressed when placing, never an invariant enforced by eviction.
    }

    // GetAreaId answers with the most specific area, and in AreaTable.dbc it is the
    // parent zone that carries the faction, not the sub-area a bot actually stands in:
    // Teldrassil is 2, but Shadowglen, Dolanaar and Aldrassil inside it are all 0.
    // The same holds for Northshire Valley, Coldridge Valley, Valley of Trials, Camp
    // Narache and Deathknell. Reading only the leaf therefore skipped the faction test
    // exactly where new players are, so a level 5 Horde bot in Shadowglen came back
    // perfectly safe. Walk up to the first ancestor that declares an owner.
    // A hub both sides may use keeps its own answer of "nobody owns this" rather than
    // inheriting the zone around it, so it falls through to the guard-presence test
    // below -- where its bruisers, being hostile to neither side, exclude nobody.
    uint32 factionMask = area->FactionGroupMask;
    bool neutralHub = m_neutralHubAreas.find(area->ID) != m_neutralHubAreas.end();
    // The depth cap is a guard against a cyclic ParentAreaID, not a real limit: the
    // shipped 1.12 AreaTable has no self-references and a longest chain of two. A modded
    // or corrupt DBC could otherwise spin this loop forever on the world thread.
    {
        AreaTableEntry const* scope = area;
        for (int depth = 0;
             !neutralHub && factionMask == AREATEAM_NONE && scope && scope->ParentAreaID && depth < 8;
             ++depth)
        {
            scope = sAreaStore.LookupEntry(scope->ParentAreaID);
            if (scope)
            {
                factionMask = scope->FactionGroupMask;
            }
        }
    }

    if (factionMask != AREATEAM_NONE)
    {
        bool botIsAlliance = IsAlliance(bot->getRace());
        if (botIsAlliance && factionMask != AREATEAM_ALLY)
        {
            return false;
        }

        if (!botIsAlliance && factionMask != AREATEAM_HORDE)
        {
            return false;
        }
    }
    else // area->team == AREATEAM_NONE: check for opposing-faction guard presence
    {
        bool botIsAlliance = IsAlliance(bot->getRace());
        if (botIsAlliance && m_hordeGuardAreas.find(area->ID) != m_hordeGuardAreas.end())
        {
            return false;   // Alliance bot in Horde-guarded contested area
        }
        if (!botIsAlliance && m_allianceGuardAreas.find(area->ID) != m_allianceGuardAreas.end())
        {
            return false;   // Horde bot in Alliance-guarded contested area
        }
    }

    if (m_areaCreatureStatsMap.empty()) // calculate stats if not done yet
    {
        const_cast<RandomPlayerbotMgr*>(this)->CalculateAreaCreatureStats();
    }

    std::map<uint32, AreaCreatureStats>::const_iterator statsItr = m_areaCreatureStatsMap.find(area->ID);
    AreaCreatureStats const* stats = (statsItr != m_areaCreatureStatsMap.end()) ? &statsItr->second : nullptr;
    if (stats && stats->creatureCount > 0)
    {
        uint8 botLevel = useLevel ? useLevel : bot->getLevel();
        uint8 tolerance = sPlayerbotAIConfig.randomBotTeleLevel;
        if (botLevel < stats->minLevel - tolerance || botLevel > stats->maxLevel + tolerance)
        {
            return false;
        }
        return true;
    }
    return false;
}

QueryResult* RandomPlayerbotMgr::QueryGroupedBots()
{
    if (sPlayerbotAIConfig.randomBotAccounts.empty())
    {
        return nullptr;
    }

    ostringstream os;
    bool first = true;
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); ++i)
    {
        if (!first)
        {
            os << ",";
        }
        os << *i;
        first = false;
    }

    return CharacterDatabase.PQuery(
            "SELECT gm.`memberGuid` FROM `group_member` gm "
            "INNER JOIN `characters` c ON gm.`memberGuid` = c.`guid` "
            "INNER JOIN `groups` g ON gm.`groupId` = g.`groupId` "
            "WHERE c.`account` IN (%s)",
        os.str().c_str());
}

void RandomPlayerbotMgr::LoadGroupedBots()
{
    m_groupedBots.clear();
    QueryResult* result = QueryGroupedBots();
    if (!result)
    {
        return;
    }

    do
    {
        Field* fields = result->Fetch();
        m_groupedBots.insert(fields[0].GetUInt32());
    } while (result->NextRow());
    delete result;
}

void RandomPlayerbotMgr::EnsureGroupedBotsOnline()
{
    QueryResult* result = QueryGroupedBots();
    if (!result)
    {
        return;
    }

    uint32 count = 0;
    do
    {
        Field* fields = result->Fetch();
        uint32 botGuid = fields[0].GetUInt32();
        if (!GetEventValue(botGuid, "add"))
        {
            SetEventValue(botGuid, "add", 1, sPlayerbotAIConfig.maxRandomBotInWorldTime);
            count++;
        }
    } while (result->NextRow());
    delete result;

    if (count > 0)
    {
        sLog.outString("Queued %u grouped bot(s) for login at startup", count);
    }
}

uint32 RandomPlayerbotMgr::GetEventValue(uint32 bot, string event, bool cacheMisses)
{
    uint32 value = 0;
    auto key = std::make_pair(bot, event);
    auto it = m_eventValueCache.find(key);
    if (it != m_eventValueCache.end())
    {
        if ((time(0) - it->second.lastChangeTime) < it->second.validIn)
        {
            return it->second.value;
        }
    }

    // Query the database to get the event value for the specified bot
    QueryResult* results = CharacterDatabase.PQuery(
            "SELECT `value`, `time`, `validIn` FROM `ai_playerbot_random_bots` WHERE `owner` = 0 AND `bot` = '%u' AND `event` = '%s'",
        bot, event.c_str());

    if (results)
    {
        Field* fields = results->Fetch();
        value = fields[0].GetUInt32();
        uint32 lastChangeTime = fields[1].GetUInt32();
        uint32 validIn = fields[2].GetUInt32();
        if ((time(0) - lastChangeTime) >= validIn)
        {
            value = 0;
        }
        m_eventValueCache[key] = {value, lastChangeTime, validIn};
        delete results;
    }

    if (!value && cacheMisses)
    {
        // Record the miss. A row that does not exist never reached the line above at
        // all, and one that has already expired can never satisfy the freshness test
        // again because its stored timestamp does not move -- so either way every
        // later call fell through to a fresh synchronous SELECT. Most events are
        // absent for a healthy bot, and ProcessBot asks after several of them per bot
        // per pass, so that was the bulk of what the update budget was buying. Any
        // SetEventValue overwrites this entry, so a real value is never masked.
        m_eventValueCache[key] = {0, (uint32)time(0), sPlayerbotAIConfig.randomBotUpdateInterval};
    }

    return value;
}

uint32 RandomPlayerbotMgr::SetEventValue(uint32 bot, string event, uint32 value, uint32 validIn)
{
    // Delete the existing event value for the specified bot
    CharacterDatabase.PExecute("DELETE FROM `ai_playerbot_random_bots` WHERE `owner` = 0 and `bot` = '%u' and `event` = '%s'",
        bot, event.c_str());
    if (value)
    {
        // Insert the new event value for the specified bot
        CharacterDatabase.PExecute(
                "INSERT INTO `ai_playerbot_random_bots` (`owner`, `bot`, `time`, `validIn`, `event`, `value`) VALUES ('%u', '%u', '%u', '%u', '%s', '%u')",
            0, bot, (uint32)time(0), validIn, event.c_str(), value);
    }

    if (event == "add")
        m_randomBotCache[bot] = (value != 0);

    m_eventValueCache[std::make_pair(bot, event)] = {value, (uint32)time(0), validIn};
    return value;
}

void RandomPlayerbotMgr::CalculateAreaCreatureStats()
{
    sLog.outString(">> [Playerbots] Calculating area creature statistics...");

    std::map<uint32, std::vector<uint8>> areaLevels;

    m_allianceGuardAreas.clear();
    m_hordeGuardAreas.clear();
    m_neutralHubAreas.clear();
    m_homeZoneAnchors.clear();

    // The eight playable races start in six zones, and inside those, six sub-areas.
    // Collecting both up front means the spawn loop below can decide in one set lookup
    // whether a spawn is worth remembering as a landing site, instead of keeping
    // positions for all 4000-odd areas. Both granularities go into the same set: a spawn
    // is recorded against whichever of the two its own area matches, and a spawn inside
    // Shadowglen matches the sub-area while one in Teldrassil proper matches the zone.
    std::set<uint32> startZones;
    for (uint32 race = 1; race < MAX_RACES; ++race)
    {
        for (uint32 cls = 1; cls < MAX_CLASSES; ++cls)
        {
            PlayerInfo const* info = sObjectMgr.GetPlayerInfo(race, cls);
            if (!info)
            {
                continue;
            }

            if (info->areaId)
            {
                startZones.insert(info->areaId);
            }

            // Must resolve the sub-area exactly as GetRacialStart does, or the pool is
            // built for one area while bots are sent to another. That mismatch is what
            // left Deathknell with no landing sites: this side asked the create position
            // and got Tirisfal, so no undead pool was ever built.
            Map* startMap = const_cast<Map*>(sMapMgr.FindMap(info->mapId));
            if (startMap && startMap->GetTerrain())
            {
                uint32 atPoint = startMap->GetTerrain()->GetAreaId(info->positionX, info->positionY, info->positionZ);
                uint32 startArea = (atPoint && atPoint != info->areaId)
                    ? atPoint
                    : FindStartSubArea(info->mapId, info->areaId, info->positionX, info->positionY, info->positionZ);
                if (startArea)
                {
                    startZones.insert(startArea);
                }
            }
            break;
        }
    }

    uint32 getAreaIdCalls = 0;
    uint32 totalCreatures = 0;

    CreatureDataMap const* creatureDataMap = sObjectMgr.GetCreatureDataMap();
    for (CreatureDataMap::const_iterator itr = creatureDataMap->begin(); itr != creatureDataMap->end(); ++itr)
    {
        CreatureData const& data = itr->second;
        CreatureInfo const* cInfo = sObjectMgr.GetCreatureTemplate(data.id);

        if (!cInfo)
        {
            continue;
        }

        totalCreatures++;

        // Per spawn, not per grid cell. This pass is what decides which area owns a guard,
        // a neutral hub and a level band, so a cell that straddles a border must not have
        // its first-asked creature answer for the rest. One extra terrain lookup per spawn,
        // once at boot, is a fair price for classifying the right area.
        Map* map = const_cast<Map*>(sMapMgr.FindMap(data.mapid));
        if (!map || !map->GetTerrain())
        {
            continue;
        }

        uint32 areaId = 0;
        uint32 zoneId = 0;
        map->GetTerrain()->GetZoneAndAreaId(zoneId, areaId, data.posX, data.posY, data.posZ);
        getAreaIdCalls++;

        if (areaId == 0)
        {
            continue;
        }

        // Landing sites for the home-zone rule. A creature spawn is a better anchor than
        // a game_tele: there are thousands of them spread through the zone rather than a
        // handful clustered on the roads, and being a spawn point it is by construction
        // somewhere the terrain will actually hold a player. Elites are excluded for the
        // same reason they are excluded from the level band below -- landing a level 4
        // beside one is not a place a bot should be put.
        if (cInfo->Rank <= CREATURE_ELITE_NORMAL)
        {
            WorldLocation site(data.mapid, data.posX, data.posY, data.posZ, 0.0f);

            // Recorded against both granularities where they differ. A spawn in
            // Shadowglen belongs to the Shadowglen pool a level 1 draws from AND to the
            // Teldrassil pool a level 8 draws from, because Shadowglen is part of
            // Teldrassil; a spawn out in Teldrassil proper belongs only to the latter.
            if (zoneId && startZones.find(zoneId) != startZones.end())
            {
                m_homeZoneAnchors[zoneId].push_back(site);
            }

            if (areaId != zoneId && startZones.find(areaId) != startZones.end())
            {
                m_homeZoneAnchors[areaId].push_back(site);
            }
        }

        // A contested-guard faction is how the world data marks a hub both sides may
        // use: the Steamwheedle bruisers in Ratchet, Booty Bay, Gadgetzan and Everlook
        // carry it and are hostile to neither player faction, while no faction guard
        // does -- 8 of 314 faction templates have the flag at all. Recording the area
        // stops it inheriting an owner from the zone around it, which is what would
        // otherwise hand Ratchet to the Horde along with the rest of the Barrens.
        // Deliberately not gated on CREATURE_FLAG_EXTRA_GUARD: Ratchet's and
        // Gadgetzan's bruisers carry that flag but Booty Bay's and Everlook's do not.
        if (FactionTemplateEntry const* hubFaction = sFactionTemplateStore.LookupEntry(cInfo->FactionAlliance))
        {
            if (hubFaction->IsContestedGuardFaction())
            {
                m_neutralHubAreas.insert(areaId);
            }
        }

        // Guard area detection: classify guards by faction hostility
        if (cInfo->ExtraFlags & CREATURE_FLAG_EXTRA_GUARD)
        {
            FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->FactionAlliance);
            if (factionTemplate && !factionTemplate->IsContestedGuardFaction() &&
                !(factionTemplate->EnemyGroup & FACTION_MASK_PLAYER))
            {
                if (factionTemplate->EnemyGroup & FACTION_MASK_HORDE)
                    m_allianceGuardAreas.insert(areaId);
                if (factionTemplate->EnemyGroup & FACTION_MASK_ALLIANCE)
                    m_hordeGuardAreas.insert(areaId);
            }
            continue;
        }

        // Skip questgivers, vendors, and non-attackable creatures
        if (cInfo->NpcFlags != 0 || cInfo->UnitFlags & UNIT_FLAG_NON_ATTACKABLE)
        {
            continue;
        }

        // And skip elites, for the same reason GrindTargetValue refuses to attack them.
        // This map decides whether an area suits a bot's level; that one decides what the
        // bot may then fight. While the two disagreed, an area could be judged safe on the
        // strength of creatures the bot is forbidden to touch -- a solo bot placed among a
        // camp of level 54 elites, which it cannot pull and which are perfectly willing to
        // pull it. Judge an area by the content a bot can actually take on.
        if (cInfo->Rank > CREATURE_ELITE_NORMAL)
        {
            continue;
        }

        uint8 avgLevel = (cInfo->MinLevel + cInfo->MaxLevel) / 2;
        areaLevels[areaId].push_back(avgLevel);
    }

    uint32 statsCount = 0;
    for (std::map<uint32, std::vector<uint8>>::iterator itr = areaLevels.begin(); itr != areaLevels.end(); ++itr)
    {
        std::vector<uint8>& levels = itr->second;
        if (levels.size() < 10) // need at least 10 creatures to have meaningful statistics
        {
            continue;
        }

        std::sort(levels.begin(), levels.end());

        // to avoid outliers, use 25th and 75th percentiles
        size_t p25 = levels.size() / 4;
        size_t p75 = (levels.size() * 3) / 4;

        AreaCreatureStats& stats = m_areaCreatureStatsMap[itr->first];
        stats.minLevel = levels[p25];
        stats.maxLevel = levels[p75];
        stats.creatureCount = levels.size();
        ++statsCount;
    }

    sLog.outString(">> [Playerbots] Calculated spawn stats for %u areas", statsCount);

    // Worth saying out loud at boot: an empty pool silently sends every low-level bot
    // down the generic search path instead, which still confines it correctly but is
    // slower and can fail. If a starting zone reports 0 here, the rule is not working.
    for (std::map<uint32, std::vector<WorldLocation> >::const_iterator itr = m_homeZoneAnchors.begin();
         itr != m_homeZoneAnchors.end(); ++itr)
    {
        AreaTableEntry const* zone = sAreaStore.LookupEntry(itr->first);
        sLog.outString(">> [Playerbots] Starting %s %s (%u): %u landing sites",
            (zone && zone->ParentAreaID) ? "sub-area" : "zone",
            zone ? zone->AreaName_lang[0] : "?", itr->first, (uint32)itr->second.size());
    }
}

bool ChatHandler::HandlePlayerbotConsoleCommand(char* args)
{
    if (!sPlayerbotAIConfig.enabled)
    {
        PSendSysMessage("Playerbot system is currently disabled!");
        SetSentErrorMessage(true);
        return false;
    }

    if (!args || !*args)
    {
        sLog.outError("Usage: rndbot stats/update/reset/init/refresh/add/remove");
        return false;
    }

    string cmd = args;

    if (cmd == "reset")
    {
        // Reset all random bots
        CharacterDatabase.PExecute("DELETE FROM `ai_playerbot_random_bots`");
        sLog.outBasic("Random bots were reset for all players");
        return true;
    }
    else if (cmd == "stats")
    {
        // Print statistics of random bots
        sRandomPlayerbotMgr.PrintStats();
        return true;
    }
    else if (cmd == "update")
    {
        // Update the AI of random bots
        sRandomPlayerbotMgr.UpdateAIInternal(0);
        return true;
    }
    else if (cmd == "init" || cmd == "refresh")
    {
        sLog.outString("Randomizing bots for %d accounts", sPlayerbotAIConfig.randomBotAccounts.size());
        BarGoLink bar(sPlayerbotAIConfig.randomBotAccounts.size());
        for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); ++i)
        {
            uint32 account = *i;
            bar.step();
            if (QueryResult *results = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` where `account` = '%u'", account))
            {
                do
                {
                    Field* fields = results->Fetch();
                    ObjectGuid guid = ObjectGuid(fields[0].GetUInt64());
                    Player* bot = sObjectMgr.GetPlayer(guid, true);
                    if (!bot)
                    {
                        continue;
                    }

                    if (cmd == "init")
                    {
                        sLog.outDetail("Randomizing bot %s for account %u", bot->GetName(), account);
                        sRandomPlayerbotMgr.RandomizeFirst(bot);
                    }
                    else
                    {
                        sLog.outDetail("Refreshing bot %s for account %u", bot->GetName(), account);
                        bot->SetLevel(bot->getLevel() - 1);
                        sRandomPlayerbotMgr.IncreaseLevel(bot);
                    }
                    uint32 randomTime = urand(sPlayerbotAIConfig.minRandomBotRandomizeTime, sPlayerbotAIConfig.maxRandomBotRandomizeTime);
                    CharacterDatabase.PExecute("UPDATE `ai_playerbot_random_bots` SET `validIn` = '%u' WHERE `event` = 'randomize' AND `bot` = '%u'",
                        randomTime, bot->GetGUIDLow());
                    CharacterDatabase.PExecute("UPDATE `ai_playerbot_random_bots` SET `validIn` = '%u' WHERE `event` = 'logout' AND `bot` = '%u'",
                        sPlayerbotAIConfig.maxRandomBotInWorldTime, bot->GetGUIDLow());
                } while (results->NextRow());

                delete results;
            }
        }
        return true;
    }
    else
    {
        // Handle other playerbot commands
        list<string> messages = sRandomPlayerbotMgr.HandlePlayerbotCommand(args, NULL);
        for (list<string>::iterator i = messages.begin(); i != messages.end(); ++i)
        {
            sLog.outString(i->c_str());
        }
        return true;
    }

    return false;
}

void RandomPlayerbotMgr::HandleCommand(uint32 type, const string& text, Player& fromPlayer)
{
    // A public channel line arrives here once and is then handed to every random bot in the
    // world. That is a fan-out of one message to the whole fleet, so anything that answers
    // replies once per bot: a single "~who" in trade chat returned two hundred whispers to
    // whoever typed it. Per-bot security still applies underneath, but a broadcast is the
    // wrong shape for a public channel however well each bot behaves individually.
    if (type == CHAT_MSG_CHANNEL)
    {
        return;
    }

    // Handle commands for all player bots
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        bot->GetPlayerbotAI()->HandleCommand(type, text, fromPlayer);
    }
}

void RandomPlayerbotMgr::OnPlayerLogout(Player* player)
{
    // Handle player logout for all player bots
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        PlayerbotAI* ai = bot->GetPlayerbotAI();
        if (player == ai->GetMaster())
        {
            ai->SetMaster(NULL);
            ai->ResetStrategies();
        }
    }

    if (!player->GetPlayerbotAI())
    {
        vector<Player*>::iterator i = find(players.begin(), players.end(), player);
        if (i != players.end())
        {
            players.erase(i);
        }

        uint32 zone = player->GetZoneId();
        std::unordered_map<uint32, uint32>::iterator zi = m_playerZoneCounts.find(zone);
        if (zi != m_playerZoneCounts.end())
        {
            if (zi->second <= 1)
            {
                m_playerZoneCounts.erase(zi);
            }
            else
            {
                zi->second--;
            }
        }
    }
}

void RandomPlayerbotMgr::OnPlayerLogin(Player* player)
{
    // Handle player login for all player bots
    for (PlayerBotMap::const_iterator it = GetPlayerBotsBegin(); it != GetPlayerBotsEnd(); ++it)
    {
        Player* const bot = it->second;
        if (player == bot || player->GetPlayerbotAI())
        {
            continue;
        }

        Group* group = bot->GetGroup();
        if (!group)
        {
            continue;
        }

        for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->getSource();
            PlayerbotAI* ai = bot->GetPlayerbotAI();
            if (member == player && (!ai->GetMaster() || ai->GetMaster()->GetPlayerbotAI()))
            {
                ai->SetMaster(player);
                ai->ResetStrategies();
                ai->TellMaster("Hello");
                break;
            }
        }
    }

    if (!player->GetPlayerbotAI())
    {
        players.push_back(player);
        // do not add to m_playerZoneCounts, as OnPlayerZoneChange is called anyway
    }
}

void RandomPlayerbotMgr::OnPlayerZoneChange(Player* player, uint32 newZone)
{
    if (player->GetPlayerbotAI() ||
        player->GetSession()->GetRemoteAddress() == "bot")
    {
        // PlayerbotAI is not set before calling this on entry, so remote address chk
        return;
    }

    uint32 oldZone = player->GetCachedZoneId();
    if (oldZone == newZone)
    {
        return;
    }

    std::unordered_map<uint32, uint32>::iterator zi = m_playerZoneCounts.find(oldZone);
    if (zi != m_playerZoneCounts.end())
    {
        if (zi->second <= 1)
        {
            m_playerZoneCounts.erase(zi);
        }
        else
        {
            zi->second--;
        }
    }
    m_playerZoneCounts[newZone]++;
}

bool RandomPlayerbotMgr::HasRealPlayerInZone(uint32 zoneId) const
{
    std::unordered_map<uint32, uint32>::const_iterator zi = m_playerZoneCounts.find(zoneId);
    return zi != m_playerZoneCounts.end() && zi->second > 0;
}

Player* RandomPlayerbotMgr::GetRandomPlayer()
{
    // Get a random player from the list of players
    if (players.empty())
    {
        return NULL;
    }

    uint32 index = urand(0, players.size() - 1);
    return players[index];
}

ClassRoles RandomPlayerbotMgr::FillRoleMap(Player *bot, int &heal, int &dps, int &tank)
{
    int spec = AiFactory::GetPlayerSpecTab(bot);
    switch (bot->getClass())
    {
        case CLASS_DRUID:
            if (spec == 2)
            {
                heal++;
                return LFG_ROLE_HEALER;
            }
            break;
        case CLASS_PALADIN:
            if (spec == 1)
            {
                tank++;
                return LFG_ROLE_TANK;
            }
            else if (spec == 0)
            {
                heal++;
                return LFG_ROLE_HEALER;
            }
            break;
        case CLASS_PRIEST:
            if (spec != 2)
            {
                heal++;
                return LFG_ROLE_HEALER;
            }
            break;
        case CLASS_SHAMAN:
            if (spec == 2)
            {
                heal++;
                return LFG_ROLE_HEALER;
            }
            break;
        case CLASS_WARRIOR:
            if (spec == 2)
            {
                tank++;
                return LFG_ROLE_TANK;
            }
            break;
        default:
            break;
    }
    dps++;
    return LFG_ROLE_DPS;
}

void RandomPlayerbotMgr::PrintStats()
{
    sLog.outString("%d Random Bots online", playerBots.size());

    map<uint32, int> alliance, horde;
    for (uint32 i = 0; i < 10; ++i)
    {
        alliance[i] = 0;
        horde[i] = 0;
    }

    map<uint8, int> perRace, perClass;
    for (uint8 race = RACE_HUMAN; race < MAX_RACES; ++race)
    {
        perRace[race] = 0;
    }
    for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
    {
        perClass[cls] = 0;
    }

    int dps = 0, heal = 0, tank = 0, active = 0;
    for (PlayerBotMap::iterator i = playerBots.begin(); i != playerBots.end(); ++i)
    {
        Player* bot = i->second;
        if (IsAlliance(bot->getRace()))
        {
            alliance[bot->getLevel() / 10]++;
        }
        else
        {
            horde[bot->getLevel() / 10]++;
        }

        perRace[bot->getRace()]++;
        perClass[bot->getClass()]++;

        if (bot->GetPlayerbotAI()->IsActive())
        {
            active++;
        }

        FillRoleMap(bot, heal, dps, tank);
    }

    sLog.outString("Per level:");
    uint32 maxLevel = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    for (uint32 i = 0; i < 10; ++i)
    {
        if (!alliance[i] && !horde[i])
        {
            continue;
        }

        uint32 from = i * 10;
        uint32 to = min(from + 9, maxLevel);
        if (!from)
        {
            from = 1;
        }
        sLog.outString("    %d..%d: %d alliance, %d horde", from, to, alliance[i], horde[i]);
    }
    sLog.outString("Per race:");
    for (uint8 race = RACE_HUMAN; race < MAX_RACES; ++race)
    {
        if (perRace[race])
        {
            sLog.outString("    %s: %d", ChatHelper::formatRace(race).c_str(), perRace[race]);
        }
    }
    sLog.outString("Per class:");
    for (uint8 cls = CLASS_WARRIOR; cls < MAX_CLASSES; ++cls)
    {
        if (perClass[cls])
        {
            sLog.outString("    %s: %d", ChatHelper::formatClass(cls).c_str(), perClass[cls]);
        }
    }
    sLog.outString("Per role:");
    sLog.outString("    tank: %d", tank);
    sLog.outString("    heal: %d", heal);
    sLog.outString("    dps: %d", dps);

    sLog.outString("Active bots: %d", active);
}

double RandomPlayerbotMgr::GetBuyMultiplier(Player* bot)
{
    uint32 id = bot->GetObjectGuid();
    uint32 value = GetEventValue(id, "buymultiplier");
    if (!value)
    {
        value = urand(1, 120);
        uint32 validIn = urand(sPlayerbotAIConfig.minRandomBotsPriceChangeInterval, sPlayerbotAIConfig.maxRandomBotsPriceChangeInterval);
        SetEventValue(id, "buymultiplier", value, validIn);
    }

    return (double)value / 100.0;
}

double RandomPlayerbotMgr::GetSellMultiplier(Player* bot)
{
    uint32 id = bot->GetObjectGuid();
    uint32 value = GetEventValue(id, "sellmultiplier");
    if (!value)
    {
        value = urand(80, 250);
        uint32 validIn = urand(sPlayerbotAIConfig.minRandomBotsPriceChangeInterval, sPlayerbotAIConfig.maxRandomBotsPriceChangeInterval);
        SetEventValue(id, "sellmultiplier", value, validIn);
    }

    return (double)value / 100.0;
}

uint32 RandomPlayerbotMgr::GetLootAmount(Player* bot)
{
    uint32 id = bot->GetObjectGuid();
    return GetEventValue(id, "lootamount");
}

void RandomPlayerbotMgr::SetLootAmount(Player* bot, uint32 value)
{
    uint32 id = bot->GetObjectGuid();
    SetEventValue(id, "lootamount", value, 24 * 3600);
}

uint32 RandomPlayerbotMgr::GetTradeDiscount(Player* bot)
{
    Group* group = bot->GetGroup();
    return GetLootAmount(bot) / (group ? group->GetMembersCount() : 10);
}

void RandomPlayerbotMgr::HandleMeetingStoneClick(Player* player, GameObject* obj)
{
    if (!player || !obj)
    {
        return;
    }

    if (!sPlayerbotAIConfig.randomBotJoinLfg)
    {
        return;
    }

    Group* grp = player->GetGroup();
    if (!grp || !grp->IsLeader(player->GetObjectGuid()))
    {
        return;
    }

    float stoneX, stoneY, stoneZ;
    obj->GetPosition(stoneX, stoneY, stoneZ);
    GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(obj->GetEntry());
    uint32 minLevel = gInfo->meetingstone.minLevel;
    uint32 maxLevel = gInfo->meetingstone.maxLevel;

    uint32 mapId = obj->GetMapId();
    int healers = 0, tanks = 0, dpses = 0;
    vector<ClassRoles> missingRoles = {LFG_ROLE_TANK, LFG_ROLE_HEALER, LFG_ROLE_DPS, LFG_ROLE_DPS, LFG_ROLE_DPS};
    for (Group::member_citerator citr = grp->GetMemberSlots().begin(); citr != grp->GetMemberSlots().end(); ++citr)
    {
        Player* member = sObjectMgr.GetPlayer(citr->guid);
        // A member slot survives its player logging out, so GetPlayer returns NULL for
        // anyone offline. The null test below used to guard only the role tally, and the
        // very next line dereferenced the same pointer regardless -- so one offline member
        // in the leader's group crashed the server on a meeting-stone click.
        if (!member)
        {
            continue;
        }

        ClassRoles role = FillRoleMap(member, healers, dpses, tanks);
        auto it = find(missingRoles.begin(), missingRoles.end(), role);
        if (it != missingRoles.end())
        {
            missingRoles.erase(it);
        }

        if (!member->GetPlayerbotAI() || member == player)
        {
            continue;
        }

        if (!player->IsWithinDistInMap(member, sPlayerbotAIConfig.sightDistance))
        {
            member->GetMotionMaster()->Clear();
            member->TeleportTo(mapId, stoneX, stoneY, stoneZ, 0);
        }
    }

    if (grp->IsFull())
    {
        return;
    }

    if (missingRoles.size() == 0)
    {
        return;
    }

    uint32 team = player->GetTeam();
    std::list<Player*> eligibleBots;
    for(uint32_t zoneChk : {player->GetZoneId(), -1u})
    {
        if (missingRoles.size() == 0)
        {
            break;
        }

        for (auto botit = GetPlayerBotsBegin(); botit != GetPlayerBotsEnd(); ++botit)
        {
            Player* bot = botit->second;
            if (!bot || !bot->IsInWorld())
            {
                continue;
            }

            if (bot->GetGroup())
            {
                continue;
            }

            if (zoneChk != (uint32_t)-1 && bot->GetZoneId() != zoneChk)
            {
                continue;
            }

            if (bot->GetTeam() != team)
            {
                continue;
            }

            if(bot->getLevel() < minLevel || bot->getLevel() > maxLevel)
            {
                continue;
            }

            ClassRoles role = FillRoleMap(bot, healers, dpses, tanks);
            auto it = find(missingRoles.begin(), missingRoles.end(), role);
            if (it != missingRoles.end())
            {
                missingRoles.erase(it);
                eligibleBots.push_back(bot);
                if (missingRoles.size() == 0)
                {
                    break;
                }
            }
        }
    }

    for (Player* bot : eligibleBots)
    {
        if (grp->IsFull())
        {
            break;
        }

        grp->AddMember(bot->GetObjectGuid(), bot->GetName(), GROUP_LFG);

        if (PlayerbotAI* ai = bot->GetPlayerbotAI())
        {
            ai->SetMaster(player);
        }

        bot->GetMotionMaster()->Clear();
        bot->TeleportTo(mapId, stoneX, stoneY, stoneZ, 0);
    }
}
