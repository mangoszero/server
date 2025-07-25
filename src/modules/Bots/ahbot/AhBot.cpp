#include "ace/Task.h"
#include "Category.h"
#include "ItemBag.h"
#include "AhBot.h"
#include "World.h"
#include "Config.h"
#include "Chat.h"
#include "AhBotConfig.h"
#include "AuctionHouseMgr.h"
#include "WorldSession.h"
#include "Player.h"
#include "ObjectAccessor.h"
#include "ObjectGuid.h"
#include "ObjectMgr.h"
#include "playerbot/PlayerbotAIConfig.h"
#include "AccountMgr.h"
#include "playerbot/playerbot.h"


using namespace ahbot;

bool Player::MinimalLoadFromDB( QueryResult *result, uint32 guid )
{
    bool delete_result = true;
    if (!result)
    {
        //                                        0     1           2           3           4    5          6          7
        result = CharacterDatabase.PQuery("SELECT `name`, `position_x`, `position_y`, `position_z`, `map`, `totaltime`, `leveltime`, `at_login` FROM `characters` WHERE `guid` = '%u'",guid);
        if (!result)
        {
            return false;
        }
    }
    else
    {
        delete_result = false;
    }

    Field *fields = result->Fetch();

    // overwrite possible wrong/corrupted guid
    Object::_Create( guid, 0, HIGHGUID_PLAYER );

    m_name = fields[0].GetCppString();

    Relocate(fields[1].GetFloat(),fields[2].GetFloat(),fields[3].GetFloat());
    SetLocationMapId(fields[4].GetUInt32());

    m_Played_time[PLAYED_TIME_TOTAL] = fields[5].GetUInt32();
    m_Played_time[PLAYED_TIME_LEVEL] = fields[6].GetUInt32();

    m_atLoginFlags = fields[7].GetUInt32();

    if (delete_result)
    {
        delete result;
    }

    for (int i = 0; i < PLAYER_SLOTS_COUNT; ++i)
    {
        m_items[i] = NULL;
    }

    if (HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
    {
        m_deathState = DEAD;
    }

    return true;
}

bool ChatHandler::HandleAhBotCommand(char* args)
{
    auctionbot.HandleCommand(args);
    return true;
}

INSTANTIATE_SINGLETON_1( ahbot::AhBot );

uint32 AhBot::auctionIds[MAX_AUCTIONS] = {1,6,7};
uint32 AhBot::auctioneers[MAX_AUCTIONS] = {79707,4656,23442};
map<uint32, uint32> AhBot::factions;

void AhBot::Init()
{
    sLog.outString("Initializing AhBot by ike3");

    if (!sAhBotConfig.Initialize())
    {
        return;
    }

    factions[1] = 1;
    factions[2] = 1;
    factions[3] = 1;
    factions[4] = 2;
    factions[5] = 2;
    factions[6] = 2;
    factions[7] = 3;

    availableItems.Init();

    sLog.outString("AhBot configuration loaded");
}

AhBot::~AhBot()
{
}

ObjectGuid AhBot::GetAHBplayerGUID()
{
    return ObjectGuid(sAhBotConfig.guid);
}

class AhbotThread: public ACE_Task <ACE_MT_SYNCH>
{
private:
    AhBot* bot;
public:
    AhbotThread(AhBot* bot) : bot(bot) {}
    int svc(void) { bot->ForceUpdate(); return 0; }
};

void AhBot::Update()
{
    time_t now = time(0);

    if (now < nextAICheckTime)
    {
        return;
    }

    if (updating)
    {
        return;
    }

    nextAICheckTime = time(0) + sAhBotConfig.updateInterval;

    AhbotThread *thread = new AhbotThread(this);
    thread->activate();
}

void AhBot::ForceUpdate()
{
    if (!sAhBotConfig.enabled)
    {
        return;
    }

    if (updating)
    {
        return;
    }

    sLog.outString("AhBot is now checking auctions");
    updating = true;

    if (!allBidders.size())
    {
        LoadRandomBots();
    }

    if (!allBidders.size())
    {
        sLog.outError("Ahbot is disabled but there is no bidders available");
        return;
    }

    CheckCategoryMultipliers();

    int answered = 0, added = 0;
    for (int i = 0; i < MAX_AUCTIONS; i++)
    {
        InAuctionItemsBag inAuctionItems(auctionIds[i]);
        inAuctionItems.Init(true);

        for (int j = 0; j < CategoryList::instance.size(); j++)
        {
            Category* category = CategoryList::instance[j];
            answered += Answer(i, category, &inAuctionItems);
            added += AddAuctions(i, category, &inAuctionItems);
        }
    }

    CleanupHistory();

    sLog.outString("AhBot auction check finished. %d auctions answered, %d new auctions added. Next check in %d seconds",
            answered, added, sAhBotConfig.updateInterval);
    updating = false;
}

struct SortByPricePredicate
{
    bool operator()(AuctionEntry* const & a, AuctionEntry* const & b) const
    {
        if (a->startbid == b->startbid)
        {
            return a->buyout < b->buyout;
        }

        return a->startbid < b->startbid;
    }
};

vector<AuctionEntry*> AhBot::LoadAuctions(const AuctionHouseObject::AuctionEntryMap& auctionEntryMap,
        Category*& category, int& auction)
{
    vector<AuctionEntry*> entries;
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
    {
        AuctionEntry *entry = itr->second;
        if (IsBotAuction(entry->owner) || IsBotAuction(entry->bidder))
        {
            continue;
        }

        Item *item = sAuctionMgr.GetAItem(entry->itemGuidLow);
        if (!item)
        {
            continue;
        }

        if (!category->Contains(item->GetProto()))
        {
            continue;
        }

        uint32 price = category->GetPricingStrategy()->GetBuyPrice(item->GetProto(), auctionIds[auction]);
        if (!price || !item->GetCount())
        {
            sLog.outDetail("%s (x%d) in auction %d: price cannot be determined",
                    item->GetProto()->Name1, item->GetCount(), auctionIds[auction]);
            continue;
        }

        entries.push_back(entry);
    }
    sort(entries.begin(), entries.end(), SortByPricePredicate());
    return entries;
}

void AhBot::FindMinPrice(const AuctionHouseObject::AuctionEntryMap& auctionEntryMap, AuctionEntry*& entry, Item*& item, uint32* minBid,
        uint32* minBuyout)
{
    *minBid = 0;
    *minBuyout = 0;
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
    {
        AuctionEntry *other = itr->second;
        if (other->owner == entry->owner)
        {
            continue;
        }

        Item *otherItem = sAuctionMgr.GetAItem(other->itemGuidLow);
        if (!otherItem || !otherItem->GetCount() || otherItem->GetProto()->ItemId != item->GetProto()->ItemId)
        {
            continue;
        }

        uint32 startbid = other->startbid / otherItem->GetCount() * item->GetCount();
        uint32 bid = other->bid / otherItem->GetCount() * item->GetCount();
        uint32 buyout = other->buyout / otherItem->GetCount() * item->GetCount();

        if (!bid && startbid && (!*minBid || *minBid > startbid))
        {
            *minBid = startbid;
        }

        if (bid && (*minBid || *minBid > bid))
        {
            *minBid = bid;
        }

        if (buyout && (!*minBuyout || *minBuyout > buyout))
        {
            *minBuyout = buyout;
        }
    }
}

int AhBot::Answer(int auction, Category* category, ItemBag* inAuctionItems)
{
    const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
    if (!ahEntry)
    {
        return 0;
    }

    int answered = 0;
    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    const AuctionHouseObject::AuctionEntryMap& auctionEntryMap = auctionHouse->GetAuctions();
    int64 availableMoney = GetAvailableMoney(auctionIds[auction]);

    vector<AuctionEntry*> entries = LoadAuctions(auctionEntryMap, category, auction);
    for (vector<AuctionEntry*>::iterator itr = entries.begin(); itr != entries.end(); ++itr)
    {
        AuctionEntry *entry = *itr;

        Item *item = sAuctionMgr.GetAItem(entry->itemGuidLow);
        if (!item || !item->GetCount())
        {
            continue;
        }

        const ItemPrototype* proto = item->GetProto();
        vector<uint32> items = availableItems.Get(category);
        if (find(items.begin(), items.end(), proto->ItemId) == items.end())
        {
            sLog.outDetail("%s (x%d) in auction %d: unavailable item",
                    item->GetProto()->Name1, item->GetCount(), auctionIds[auction]);
            continue;
        }

        uint32 answerCount = GetAnswerCount(proto->ItemId, auctionIds[auction], sAhBotConfig.itemBuyMaxInterval);
        uint32 maxAnswerCount = category->GetMaxAllowedItemAuctionCount(proto);
        if (maxAnswerCount && answerCount > maxAnswerCount)
        {
            sLog.outDetail("%s (x%d) in auction %d: answer count %d > %d (max)",
                    item->GetProto()->Name1, item->GetCount(), auctionIds[auction], answerCount, maxAnswerCount);
            continue;
        }

        if (proto->RequiredLevel > sAhBotConfig.maxRequiredLevel || proto->ItemLevel > sAhBotConfig.maxItemLevel)
        {
            sLog.outDetail("%s (x%d) in auction %d: above max required or item level",
                    item->GetProto()->Name1, item->GetCount(), auctionIds[auction]);
            continue;
        }

        uint32 price = category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]);
        if (!price)
        {
            sLog.outDetail("%s (x%d) in auction %d: cannot determine price",
                    item->GetProto()->Name1, item->GetCount(), auctionIds[auction]);
            continue;
        }

        uint32 bidPrice = item->GetCount() * price;
        uint32 buyoutPrice = item->GetCount() * urand(price, 4 * price / 3);

        uint32 curPrice = entry->bid;
        if (!curPrice)
        {
            curPrice = entry->startbid;
        }
        if (!curPrice)
        {
            curPrice = entry->buyout;
        }

        if (curPrice > buyoutPrice)
        {
            sLog.outDetail("%s (x%d) in auction %d: price %d > %d (buyout price)",
                    proto->Name1, item->GetCount(), auctionIds[auction], curPrice, buyoutPrice);
            continue;
        }

        if (availableMoney < curPrice)
        {
            sLog.outDetail("%s (x%d) in auction %d: price %d > %d (available money)",
                    proto->Name1, item->GetCount(), auctionIds[auction], curPrice, availableMoney);
            continue;
        }

        uint32 minBid = 0, minBuyout = 0;
        FindMinPrice(auctionEntryMap, entry, item, &minBid, &minBuyout);

        if (minBid && entry->bid && minBid < entry->bid)
        {
            sLog.outDetail("%s (x%d) in auction %d: %d (bid) > %d (minBid)",
                    proto->Name1, item->GetCount(), auctionIds[auction], entry->bid, minBid);
            continue;
        }

        if (minBid && entry->startbid && minBid < entry->startbid)
        {
            sLog.outDetail("%s (x%d) in auction %d: %d (startbid) > %d (minBid)",
                    proto->Name1, item->GetCount(), auctionIds[auction], entry->startbid, minBid);
            continue;
        }

        double priceLevel = (double)curPrice / (double)buyoutPrice;
        uint32 buytime = GetBuyTime(entry->Id, proto->ItemId, auctionIds[auction], category, priceLevel);
        if (time(0) < buytime)
        {
            sLog.outDetail("%s (x%d) in auction %d: will buy/bid in %d seconds",
                    proto->Name1, item->GetCount(), auctionIds[auction], buytime - time(0));
            continue;
        }

        uint32 bidder = GetRandomBidder(auctionIds[auction]);
        if (!bidder)
        {
            sLog.outError("No bidders for auction %d", auctionIds[auction]);
            break;
        }

        entry->bidder = bidder;
        entry->bid = curPrice + urand(1, 1 + bidPrice / 10);
        availableMoney -= curPrice;

        updateMarketPrice(item->GetProto()->ItemId, entry->buyout / item->GetCount(), auctionIds[auction]);

        if ((entry->buyout && (entry->bid >= entry->buyout || 100 * (entry->buyout - entry->bid) / price < 25)) &&
                !(minBuyout && entry->buyout && minBuyout < entry->buyout))
        {
            sLog.outDetail("AhBot %d won %s (x%d) in auction %d for %d",
                    bidder, item->GetProto()->Name1, item->GetCount(), auctionIds[auction], entry->buyout);

            entry->bid = entry->buyout;
            entry->AuctionBidWinning(NULL);
        }
        else
        {
            sLog.outDetail("AhBot %d placed bid %d for %s (x%d) in auction %d",
                    bidder, entry->bid, item->GetProto()->Name1, item->GetCount(), auctionIds[auction]);

            CharacterDatabase.PExecute("UPDATE `auction` SET `buyguid` = '%u',`lastbid` = '%u' WHERE `id` = '%u'",
                entry->bidder, entry->bid, entry->Id);
            AddToHistory(entry, AHBOT_WON_BID);
        }

        CharacterDatabase.PExecute("DELETE FROM `ahbot_history` WHERE `item` = '%u' AND `won` = 4 AND `auction_house` = '%u' ",
                proto->ItemId, factions[auctionIds[auction]]);

        answered++;
    }

    return answered;
}

uint32 AhBot::GetTime(string category, uint32 id, uint32 auctionHouse, uint32 type)
{
    QueryResult* results = CharacterDatabase.PQuery("SELECT MAX(`buytime`) FROM `ahbot_history` WHERE `item` = '%u' AND `won` = '%u' AND `auction_house` = '%u' AND `category` = '%s'",
        id, type, factions[auctionHouse], category.c_str());

    if (!results)
    {
        return 0;
    }

    Field* fields = results->Fetch();
    uint32 result = fields[0].GetUInt32();
    delete results;

    return result;
}

void AhBot::SetTime(string category, uint32 id, uint32 auctionHouse, uint32 type, uint32 value)
{
    CharacterDatabase.PExecute("DELETE FROM `ahbot_history` WHERE `item` = '%u' AND `won` = '%u' AND `auction_house` = '%u' AND `category` = '%s'",
        id, type, factions[auctionHouse], category.c_str());

    CharacterDatabase.PExecute("INSERT INTO `ahbot_history` (`buytime`, `item`, `bid`, `buyout`, `category`, `won`, `auction_house`) "
        "VALUES ('%u', '%u', '%u', '%u', '%s', '%u', '%u')",
        value, id, 0, 0,
        category.c_str(), type, factions[auctionHouse]);
}

uint32 AhBot::GetBuyTime(uint32 entry, uint32 itemId, uint32 auctionHouse, Category*& category, double priceLevel)
{
    uint32 entryTime = GetTime("entry", entry, auctionHouse, AHBOT_WON_DELAY);
    if (entryTime > time(0))
    {
        return entryTime;
    }

    uint32 result = entryTime;

    string categoryName = category->GetName();
    uint32 categoryTime = GetTime(categoryName, 0, auctionHouse, AHBOT_WON_DELAY);
    uint32 itemTime = GetTime("item", itemId, auctionHouse, AHBOT_WON_DELAY);

    if (categoryTime < time(0))
    {
        categoryTime = time(0);
    }
    if (itemTime < time(0))
    {
        itemTime = time(0);
    }

    double rarity = category->GetPricingStrategy()->GetRarityPriceMultiplier(itemId);
    categoryTime += urand(sAhBotConfig.itemBuyMinInterval, sAhBotConfig.itemBuyMaxInterval) * priceLevel;
    itemTime += urand(sAhBotConfig.itemBuyMinInterval, sAhBotConfig.itemBuyMaxInterval) * priceLevel / rarity;
    entryTime = max(categoryTime, itemTime);

    SetTime(categoryName, 0, auctionHouse, AHBOT_WON_DELAY, categoryTime);
    SetTime("item", itemId, auctionHouse, AHBOT_WON_DELAY, itemTime);
    SetTime("entry", entry, auctionHouse, AHBOT_WON_DELAY, entryTime);

    return result ? result : entryTime;
}

uint32 AhBot::GetSellTime(uint32 itemId, uint32 auctionHouse, Category*& category)
{
    uint32 itemSellTime = GetTime("item", itemId, auctionHouse, AHBOT_SELL_DELAY);
    uint32 itemBuyTime = GetTime("item", itemId, auctionHouse, AHBOT_WON_DELAY);
    uint32 itemTime = max(itemSellTime, itemBuyTime);

    if (itemTime > time(0))
    {
        return itemTime;
    }

    uint32 result = itemTime;

    string categoryName = category->GetName();
    uint32 categorySellTime = GetTime(categoryName, 0, auctionHouse, AHBOT_SELL_DELAY);
    uint32 categoryBuyTime = GetTime(categoryName, 0, auctionHouse, AHBOT_WON_DELAY);
    uint32 categoryTime = max(categorySellTime, categoryBuyTime);

    if (categoryTime < time(0))
    {
        categoryTime = time(0);
    }
    if (itemTime < time(0))
    {
        itemTime = time(0);
    }

    double rarity = category->GetPricingStrategy()->GetRarityPriceMultiplier(itemId);
    categoryTime += urand(sAhBotConfig.itemSellMinInterval, sAhBotConfig.itemSellMaxInterval);
    itemTime += urand(sAhBotConfig.itemSellMinInterval, sAhBotConfig.itemSellMaxInterval) * rarity;
    itemTime = max(itemTime, categoryTime);

    SetTime(categoryName, 0, auctionHouse, AHBOT_SELL_DELAY, categoryTime);
    SetTime("item", itemId, auctionHouse, AHBOT_SELL_DELAY, itemTime);

    return result ? result : itemTime;
}

int AhBot::AddAuctions(int auction, Category* category, ItemBag* inAuctionItems)
{
    vector<uint32>& inAuction = inAuctionItems->Get(category);

    int32 maxAllowedAuctionCount = categoryMaxAuctionCount[category->GetName()];
    if (inAuctionItems->GetCount(category) >= maxAllowedAuctionCount)
    {
        return 0;
    }

    int added = 0;
    vector<uint32> available = availableItems.Get(category);
    for (int32 i = 0; i <= maxAllowedAuctionCount && available.size() > 0 && inAuctionItems->GetCount(category) < maxAllowedAuctionCount; ++i)
    {
        uint32 index = urand(0, available.size() - 1);
        uint32 itemId = available[index];

        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        int32 maxAllowedItems = category->GetMaxAllowedItemAuctionCount(proto);
        if (maxAllowedItems && inAuctionItems->GetCount(category, proto->ItemId) >= maxAllowedItems)
        {
            continue;
        }

        uint32 sellTime = GetSellTime(proto->ItemId, auctionIds[auction], category);
        if (time(0) < sellTime)
        {
            sLog.outDetail("%s in auction %d: will add in %d seconds",
                    proto->Name1, auctionIds[auction], sellTime - time(0));
            continue;
        }
        else if (time(0) - sellTime > sAhBotConfig.maxSellInterval)
        {
            sLog.outDetail("%s in auction %d: too old (%d secs)",
                    proto->Name1, auctionIds[auction], time(0) - sellTime);
            continue;
        }

        inAuctionItems->Add(proto);
        added += AddAuction(auction, category, proto);
    }

    return added;
}

int AhBot::AddAuction(int auction, Category* category, ItemPrototype const* proto)
{
    uint32 price = category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction]);

    sLog.outDebug("AddAuction: market price adjust");
    updateMarketPrice(proto->ItemId, price, auctionIds[auction]);

    price = category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]);

    uint32 stackCount = category->GetStackCount(proto);
    if (!price || !stackCount)
    {
        return 0;
    }

    if (urand(0, 100) <= sAhBotConfig.underPriceProbability * 100)
    {
        price = price * 100 / urand(100, 200);
    }

    uint32 bidPrice = stackCount * price;
    uint32 buyoutPrice = stackCount * urand(price, 4 * price / 3);

    Item* item = Item::CreateItem(proto->ItemId, stackCount);
    if (!item)
    {
        return 0;
    }

    uint32 randomPropertyId = Item::GenerateItemRandomPropertyId(proto->ItemId);
    if (randomPropertyId)
    {
        item->SetItemRandomProperties(randomPropertyId);
    }

    AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
    if (!ahEntry)
    {
        return 0;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);

    uint32 owner = GetRandomBidder(auctionIds[auction]);
    if (!owner)
    {
        sLog.outError("No bidders for auction %d", auctionIds[auction]);
        return 0;
    }

    AuctionEntry* auctionEntry = new AuctionEntry;
    auctionEntry->Id = sObjectMgr.GenerateAuctionID();
    auctionEntry->itemGuidLow = item->GetGUIDLow();
    auctionEntry->itemTemplate = item->GetEntry();
    auctionEntry->owner = owner;
    auctionEntry->startbid = bidPrice;
    auctionEntry->buyout = buyoutPrice;
    auctionEntry->bidder = 0;
    auctionEntry->bid = 0;
    auctionEntry->deposit = 0;
    auctionEntry->expireTime = (time_t) (urand(8, 24) * 60 * 60 + time(NULL));
    auctionEntry->auctionHouseEntry = ahEntry;
    item->SaveToDB();

    sAuctionMgr.AddAItem(item);


    auctionHouse->AddAuction(auctionEntry);

    auctionHouse->AddAuction(auctionEntry);
    auctionEntry->SaveToDB();

    sLog.outDetail("AhBot %d added %d of %s to auction %d for %d..%d", owner, stackCount, proto->Name1, auctionIds[auction], bidPrice, buyoutPrice);
    return 1;
}

void AhBot::HandleCommand(string command)
{
    if (!sAhBotConfig.enabled)
    {
        return;
    }

    if (command == "expire")
    {
        for (int i = 0; i < MAX_AUCTIONS; i++)
        {
            Expire(i);
        }

        return;
    }

    if (command == "stats")
    {
        for (int i = 0; i < MAX_AUCTIONS; i++)
        {
            PrintStats(i);
        }

        return;
    }

    if (command == "update")
    {
        AhbotThread *thread = new AhbotThread(this);
        thread->activate();
        return;
    }

    uint32 itemId = atoi(command.c_str());
    if (!itemId)
    {
        sLog.outString("ahbot stats - show short summary");
        sLog.outString("ahbot expire - expire all auctions");
        sLog.outString("ahbot update - update all auctions");
        sLog.outString("ahbot <itemId> - show item price");
        return;
    }

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto)
    {
        return;
    }

    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (category->Contains(proto))
        {
            vector<uint32> items = availableItems.Get(category);
            if (find(items.begin(), items.end(), proto->ItemId) == items.end())
            {
                continue;
            }

            ostringstream out;
            out << proto->Name1 << " (" << category->GetDisplayName() << "), "
                    << category->GetMaxAllowedAuctionCount() << "x" << category->GetMaxAllowedItemAuctionCount(proto)
                    << "x" << category->GetStackCount(proto) << " max"
                    << "\n";
            for (int auction = 0; auction < MAX_AUCTIONS; auction++)
            {
                const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
                out << "--- auction house " << auctionIds[auction] << "(faction: " << factions[auctionIds[auction]] << ", money: "
                    << GetAvailableMoney(auctionIds[auction])
                    << ") ---\n";

                out << "sell: " << category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction])
                    << " ("  << category->GetPricingStrategy()->ExplainSellPrice(proto, auctionIds[auction]) << ")"
                    << "\n";

                out << "buy: " << category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction])
                    << " ("  << category->GetPricingStrategy()->ExplainBuyPrice(proto, auctionIds[auction]) << ")"
                    << "\n";
            }
            sLog.outString(out.str().c_str());
            break;
        }
    }
}

void AhBot::Expire(int auction)
{
    if (!sAhBotConfig.enabled)
    {
        return;
    }

    AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
    if (!ahEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);

    AuctionHouseObject::AuctionEntryMap const& auctions = auctionHouse->GetAuctions();
    AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctions.begin();

    int count = 0;
    while (itr != auctions.end())
    {
        if (IsBotAuction(itr->second->owner))
        {
            itr->second->expireTime = sWorld.GetGameTime();
            count++;
        }

        ++itr;
    }

    CharacterDatabase.PExecute("DELETE FROM `ahbot_category`");
    sLog.outString("%d auctions marked as expired in auction %d", count, auctionIds[auction]);
}

void AhBot::PrintStats(int auction)
{
    if (!sAhBotConfig.enabled)
    {
        return;
    }

    AuctionHouseEntry const* ahEntry = sAuctionHouseStore.LookupEntry(auctionIds[auction]);
    if (!ahEntry)
    {
        return;
    }

    AuctionHouseObject* auctionHouse = sAuctionMgr.GetAuctionsMap(ahEntry);
    AuctionHouseObject::AuctionEntryMap const& auctions = auctionHouse->GetAuctions();

    sLog.outString("%d auctions available on auction house %d", auctions.size(), auctionIds[auction]);
}

void AhBot::AddToHistory(AuctionEntry* entry, uint32 won)
{
    if (!sAhBotConfig.enabled || !entry)
    {
        return;
    }

    if (!IsBotAuction(entry->owner) && !IsBotAuction(entry->bidder))
    {
        return;
    }

    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(entry->itemTemplate);
    if (!proto)
    {
        return;
    }

    string category = "";
    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        if (CategoryList::instance[i]->Contains(proto))
        {
            category = CategoryList::instance[i]->GetName();
            break;
        }
    }

    if (!won)
    {
        won = AHBOT_WON_PLAYER;
        if (IsBotAuction(entry->bidder))
        {
            won = AHBOT_WON_SELF;
        }
    }

    sLog.outDebug("AddToHistory: market price adjust");
    int count = entry->itemCount ? entry->itemCount : 1;
    updateMarketPrice(proto->ItemId, entry->buyout / count, entry->auctionHouseEntry->houseId);

    uint32 now = time(0);
    CharacterDatabase.PExecute("INSERT INTO `ahbot_history` (`buytime`, `item`, `bid`, `buyout`, `category`, `won`, `auction_house`) "
        "VALUES ('%u', '%u', '%u', '%u', '%s', '%u', '%u')",
        now, entry->itemTemplate, entry->bid ? entry->bid : entry->startbid, entry->buyout,
        category.c_str(), won, factions[entry->auctionHouseEntry->houseId]);
}

uint32 AhBot::GetAnswerCount(uint32 itemId, uint32 auctionHouse, uint32 withinTime)
{
    uint32 count = 0;

    QueryResult* results = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `ahbot_history` WHERE "
        "`item` = '%u' AND `won` in (2, 3) AND `auction_house` = '%u' AND `buytime` > '%u'",
        itemId, factions[auctionHouse], time(0) - withinTime);
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            count = fields[0].GetUInt32();
        } while (results->NextRow());

        delete results;
    }

    return count;
}

void AhBot::CleanupHistory()
{
    uint32 when = time(0) - 3600 * 24 * sAhBotConfig.historyDays;
    CharacterDatabase.PExecute("DELETE FROM `ahbot_history` WHERE `buytime` < '%u'", when);
}

uint32 AhBot::GetAvailableMoney(uint32 auctionHouse)
{
    int64 result = sAhBotConfig.alwaysAvailableMoney;

    map<uint32, uint32> data;
    data[AHBOT_WON_PLAYER] = 0;
    data[AHBOT_WON_SELF] = 0;

    const AuctionHouseEntry* ahEntry = sAuctionHouseStore.LookupEntry(auctionHouse);
    QueryResult* results = CharacterDatabase.PQuery(
        "SELECT `won`, SUM(`bid`) FROM `ahbot_history` WHERE `auction_house` = '%u' GROUP BY `won` HAVING `won` > 0 ORDER BY `won`",
        factions[auctionHouse]);
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            data[fields[0].GetUInt32()] = fields[1].GetUInt32();

        } while (results->NextRow());

        delete results;
    }

    results = CharacterDatabase.PQuery(
        "SELECT max(`buytime`) FROM `ahbot_history` WHERE `auction_house` = '%u' AND `won` = '2'",
        factions[auctionHouse]);
    if (results)
    {
        Field* fields = results->Fetch();
        uint32 lastBuyTime = fields[0].GetUInt32();
        uint32 now = time(0);
        if (lastBuyTime && now > lastBuyTime)
        {
            result += (now - lastBuyTime) / 3600 / 24 * sAhBotConfig.alwaysAvailableMoney;
        }

        delete results;
    }

    AuctionHouseObject::AuctionEntryMap const& auctionEntryMap = sAuctionMgr.GetAuctionsMap(ahEntry)->GetAuctions();
    for (AuctionHouseObject::AuctionEntryMap::const_iterator itr = auctionEntryMap.begin(); itr != auctionEntryMap.end(); ++itr)
    {
        AuctionEntry *entry = itr->second;
        if (!IsBotAuction(entry->bidder))
        {
            continue;
        }

        result -= entry->bid;
    }

    result += (data[AHBOT_WON_PLAYER] - data[AHBOT_WON_SELF]);
    return result < 0 ? 0 : (uint32)result;
}

void AhBot::CheckCategoryMultipliers()
{
    QueryResult* results = CharacterDatabase.PQuery("SELECT `category`, `multiplier`, `max_auction_count`, `expire_time` FROM `ahbot_category`");
    if (results)
    {
        do
        {
            Field* fields = results->Fetch();
            categoryMultipliers[fields[0].GetCppString()] = fields[1].GetFloat();
            categoryMaxAuctionCount[fields[0].GetCppString()] = fields[2].GetInt32();
            categoryMultiplierExpireTimes[fields[0].GetCppString()] = fields[3].GetUInt64();

        } while (results->NextRow());

        delete results;
    }

    CharacterDatabase.PExecute("DELETE FROM `ahbot_category`");

    for (int i = 0; i < CategoryList::instance.size(); i++)
    {
        string name = CategoryList::instance[i]->GetName();
        if (categoryMultiplierExpireTimes[name] <= time(0) || categoryMultipliers[name] <= 0)
        {
            categoryMultipliers[name] = (double)urand(20, 100) / 20.0;
            uint32 maxAllowedAuctionCount = CategoryList::instance[i]->GetMaxAllowedAuctionCount();
            categoryMaxAuctionCount[name] = urand(maxAllowedAuctionCount / 2, maxAllowedAuctionCount);
            categoryMultiplierExpireTimes[name] = time(0) + urand(4, 7) * 3600 * 24;
        }

        CharacterDatabase.PExecute("INSERT INTO `ahbot_category` (`category`, `multiplier`, `max_auction_count`, `expire_time`) "
                "VALUES ('%s', '%f', '%u', '%u')",
                name.c_str(), categoryMultipliers[name], categoryMaxAuctionCount[name], categoryMultiplierExpireTimes[name]);
    }
}


void AhBot::updateMarketPrice(uint32 itemId, double price, uint32 auctionHouse)
{
    double marketPrice = 0;

    QueryResult* results = CharacterDatabase.PQuery("SELECT `price` FROM `ahbot_price` WHERE `item` = '%u' AND `auction_house` = '%u'", itemId, auctionHouse);
    if (results)
    {
        marketPrice = results->Fetch()[0].GetFloat();
        delete results;
    }

    if (marketPrice > 0)
    {
        marketPrice = (marketPrice + price) / 2;
    }
    else
    {
        marketPrice = price;
    }

    CharacterDatabase.PExecute("DELETE FROM `ahbot_price` WHERE `item` = '%u' AND `auction_house` = '%u'", itemId, auctionHouse);
    CharacterDatabase.PExecute("INSERT INTO `ahbot_price` (`item`, `price`, `auction_house`) VALUES ('%u', '%lf', '%u')", itemId, marketPrice, auctionHouse);
}

bool AhBot::IsBotAuction(uint32 bidder)
{
    return allBidders.find(bidder) != allBidders.end();
}

uint32 AhBot::GetRandomBidder(uint32 auctionHouse)
{
    vector<uint32> guids = bidders[factions[auctionHouse]];
    if (guids.empty())
    {
        return 0;
    }

    int index = urand(0, guids.size() - 1);
    return guids[index];
}

void AhBot::LoadRandomBots()
{
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
            uint32 auctionHouse = PlayerbotAI::IsOpposing(race, RACE_HUMAN) ? 2 : 1;
            bidders[auctionHouse].push_back(guid);
            bidders[3].push_back(guid);
            allBidders.insert(guid);
        } while (result->NextRow());
        delete result;
    }

    if (allBidders.empty() && sAhBotConfig.guid)
    {
        uint32 guid = sAhBotConfig.guid;
        allBidders.insert(guid);
        for (int i = 1; i <= 3; i++)
        {
            bidders[i].push_back(guid);
        }
    }

    sLog.outDetail("{A=%d,H=%d,N=%d} bidders loaded", bidders[1].size(), bidders[2].size(), bidders[3].size());
}

int32 AhBot::GetSellPrice(ItemPrototype const* proto)
{
    if (!sAhBotConfig.enabled)
    {
        return 0;
    }

    int32 maxPrice = 0;
    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (!category->Contains(proto))
        {
            continue;
        }

        for (int auction = 0; auction < MAX_AUCTIONS; auction++)
        {
            int32 price = (int32)category->GetPricingStrategy()->GetSellPrice(proto, auctionIds[auction]);
            if (!price)
            {
                price = (int32)category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]);
            }

            if (price > maxPrice)
            {
                maxPrice = price;
            }
        }
    }

    return maxPrice;
}

int32 AhBot::GetBuyPrice(ItemPrototype const* proto)
{
    if (!sAhBotConfig.enabled)
    {
        return 0;
    }

    int32 maxPrice = 0;
    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (!category->Contains(proto))
        {
            continue;
        }

        for (int auction = 0; auction < MAX_AUCTIONS; auction++)
        {
            int32 price = (int32)category->GetPricingStrategy()->GetBuyPrice(proto, auctionIds[auction]);
            if (!price)
            {
                continue;
            }

            if (price > maxPrice)
            {
                maxPrice = price;
            }
        }
    }

    return maxPrice;
}

double AhBot::GetRarityPriceMultiplier(const ItemPrototype* proto)
{
    if (!sAhBotConfig.enabled)
    {
        return 1.0;
    }

    for (int i=0; i<CategoryList::instance.size(); i++)
    {
        Category* category = CategoryList::instance[i];
        if (!category->Contains(proto))
        {
            continue;
        }

        return category->GetPricingStrategy()->GetRarityPriceMultiplier(proto->ItemId);
    }

    return 1.0;

}
