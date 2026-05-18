#include "PricingStrategy.h"
#include "Category.h"
#include "ItemBag.h"
#include "AhBotConfig.h"
#include "../../shared/Database/DatabaseEnv.h"
#include "AhBot.h"

using namespace ahbot;

uint32 PricingStrategy::GetSellPrice(ItemPrototype const* proto, uint32 auctionHouse)
{
    uint32 now = time(0);
    double price = sAhBotConfig.GetItemPriceMultiplier(proto->Name1) *
        auctionbot.GetCategoryMultiplier(category->GetName()) *
        GetRarityPriceMultiplier(proto->ItemId) *
        GetCategoryPriceMultiplier(now, auctionHouse) *
        GetItemPriceMultiplier(proto, now, auctionHouse) *
        sAhBotConfig.GetSellPriceMultiplier(category->GetName()) *
        GetDefaultSellPrice(proto);
    return (uint32)price;
}

double PricingStrategy::GetMarketPrice(uint32 itemId, uint32 auctionHouse)
{
    double marketPrice = 0;

    QueryResult* results = CharacterDatabase.PQuery("SELECT `price` FROM `ahbot_price` WHERE `item` = '%u' AND `auction_house` = '%u'", itemId, auctionHouse);
    if (results)
    {
        marketPrice = results->Fetch()[0].GetFloat();
        delete results;
    }

    return marketPrice;
}

uint32 PricingStrategy::GetBuyPrice(ItemPrototype const* proto, uint32 auctionHouse)
{
    double marketPrice = GetMarketPrice(proto->ItemId, auctionHouse);

    if (marketPrice > 0)
    {
        return marketPrice;
    }

    uint32 untilTime = time(0) - 3600 * 12;
    double price = sAhBotConfig.GetItemPriceMultiplier(proto->Name1) *
        auctionbot.GetCategoryMultiplier(category->GetName()) *
        GetRarityPriceMultiplier(proto->ItemId) *
        GetCategoryPriceMultiplier(untilTime, auctionHouse) *
        GetItemPriceMultiplier(proto, untilTime, auctionHouse) *
        sAhBotConfig.GetBuyPriceMultiplier(category->GetName()) *
        GetDefaultBuyPrice(proto);
    return (uint32)price;
}

string PricingStrategy::ExplainSellPrice(ItemPrototype const* proto, uint32 auctionHouse)
{
    ostringstream out;

    uint32 untilTime = time(0);
    out << sAhBotConfig.GetItemPriceMultiplier(proto->Name1) << " (item const) * " <<
        auctionbot.GetCategoryMultiplier(category->GetName()) << " (random) * " <<
        GetRarityPriceMultiplier(proto->ItemId) << " (rariry) * " <<
        GetCategoryPriceMultiplier(untilTime, auctionHouse) << " (category) * " <<
        GetItemPriceMultiplier(proto, untilTime, auctionHouse) << " (item) * " <<
        sAhBotConfig.GetSellPriceMultiplier(category->GetName()) << " (sell) * " <<
        GetDefaultSellPrice(proto) << " (price)";
    return out.str();
}

string PricingStrategy::ExplainBuyPrice(ItemPrototype const* proto, uint32 auctionHouse)
{
    ostringstream out;

    double marketPrice = GetMarketPrice(proto->ItemId, auctionHouse);
    if (marketPrice > 0)
    {
        out << marketPrice << " (market)";
        return out.str();
    }

    uint32 untilTime = time(0) - 3600 * 12;
    out << sAhBotConfig.GetItemPriceMultiplier(proto->Name1) << " (item const) * " <<
        auctionbot.GetCategoryMultiplier(category->GetName()) << " (random) * " <<
        GetRarityPriceMultiplier(proto->ItemId) << " (rarity) * " <<
        GetCategoryPriceMultiplier(untilTime, auctionHouse) << " (category) * " <<
        GetItemPriceMultiplier(proto, untilTime, auctionHouse) << " (item) * " <<
        sAhBotConfig.GetBuyPriceMultiplier(category->GetName()) << " (buy) * " <<
        GetDefaultBuyPrice(proto) << " (price)";
    return out.str();
}

double PricingStrategy::GetRarityPriceMultiplier(uint32 itemId)
{
    double result = 1.0;

    QueryResult* results = WorldDatabase.PQuery(
        "SELECT MAX(`ChanceOrQuestChance`) FROM ( "
        "SELECT `ChanceOrQuestChance` FROM `gameobject_loot_template` WHERE `item` = '%u' "
        //"union select ChanceOrQuestChance from spell_loot_template where item = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `disenchant_loot_template` WHERE `item` = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `fishing_loot_template` WHERE `item` = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `item_loot_template` WHERE `item` = '%u' "
        //"union select ChanceOrQuestChance from milling_loot_template where `item` = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `pickpocketing_loot_template` WHERE `item` = '%u' "
        //"union select ChanceOrQuestChance from prospecting_loot_template where `item` = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `reference_loot_template` WHERE `item` = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `skinning_loot_template` WHERE `item` = '%u' "
        "UNION SELECT `ChanceOrQuestChance` FROM `creature_loot_template` WHERE `item` = '%u' "
        "UNION SELECT 0 "
        ") a",
        itemId,itemId,itemId,itemId,itemId,itemId,itemId,itemId,itemId,itemId,itemId);

    if (results)
    {
        Field* fields = results->Fetch();
        float chance = fields[0].GetFloat();

        if (chance > 0 && chance <= 90.0)
        {
            result = sqrt((100.0 - chance) / 10.0);
        }

        delete results;
    }

    return result >= 1.0 ? result : 1.0;
}


double PricingStrategy::GetCategoryPriceMultiplier(uint32 untilTime, uint32 auctionHouse)
{
    double result = 1.0;

    QueryResult* results = CharacterDatabase.PQuery(
        "SELECT COUNT(*) FROM (SELECT ROUND(`buytime`/3600/24/5) AS days FROM `ahbot_history` WHERE `category` = '%s' AND `won` = '1' AND `buytime` <= '%u' AND `auction_house` = '%u' GROUP BY `days`) q",
        category->GetName().c_str(), untilTime, AhBot::factions[auctionHouse]);
    if (results)
    {
        Field* fields = results->Fetch();
        uint32 count = fields[0].GetUInt32();

        if (count)
        {
            result += count;
        }

        delete results;
    }

    return result;
}

double PricingStrategy::GetMultiplier(double count, double firstBuyTime, double lastBuyTime)
{
    double k1 = (double)count / (double)((time(0) - firstBuyTime) / 3600 / 24 + 1);
    double k2 = (double)count / (double)((time(0) - lastBuyTime) / 3600 / 24 + 1);
    return max(1.0, k1 + k2) * sAhBotConfig.priceMultiplier;
}

double PricingStrategy::GetItemPriceMultiplier(ItemPrototype const* proto, uint32 untilTime, uint32 auctionHouse)
{
    double result = 1.0;

    QueryResult* results = CharacterDatabase.PQuery(
        "SELECT count(*) FROM (SELECT ROUND(`buytime`/3600/24/5) AS `days` FROM `ahbot_history` WHERE `won` = '1' AND `item` = '%u' AND `buytime` <= '%u' AND `auction_house` = '%u' GROUP BY `days`) q",
        proto->ItemId, untilTime, AhBot::factions[auctionHouse]);
    if (results)
    {
        Field* fields = results->Fetch();
        uint32 count = fields[0].GetUInt32();

        if (count)
        {
            result += count;
        }

        delete results;
    }

    return result;
}

uint32 PricingStrategy::ApplyQualityMultiplier(ItemPrototype const* proto, uint32 price)
{
    if (proto->Quality == ITEM_QUALITY_POOR)
    {
        return price;
    }

    return (uint32)(price * sqrt((double)proto->Quality) * sAhBotConfig.priceQualityMultiplier);
}

uint32 PricingStrategy::GetDefaultBuyPrice(ItemPrototype const* proto)
{
    uint32 price = 0;

    if (proto->SellPrice)
    {
        price = proto->SellPrice;
    }
    if (proto->BuyPrice)
    {
        price = max(price, proto->BuyPrice / 4);
    }

    uint32 level = max(proto->ItemLevel, proto->RequiredLevel);
    if (proto->Class == ITEM_CLASS_QUEST)
    {
        double result = 1.0;

        QueryResult* results = WorldDatabase.PQuery(
            "SELECT MAX(`QuestLevel`), MAX(`MinLevel`) FROM `quest_template` WHERE `ReqItemId1` = %u OR `ReqItemId2` = %u OR `ReqItemId3` = %u OR `ReqItemId4` = %u",
            proto->ItemId, proto->ItemId, proto->ItemId, proto->ItemId);
        if (results)
        {
            Field* fields = results->Fetch();
            level = max(fields[0].GetUInt32(), fields[1].GetUInt32());
            delete results;
        }
    }
    price = max(price, sAhBotConfig.defaultMinPrice * level * level / 10);
    price = max(price, (uint32)100);

    return ApplyQualityMultiplier(proto, price) * sAhBotConfig.priceMultiplier;
}

uint32 PricingStrategy::GetDefaultSellPrice(ItemPrototype const* proto)
{
    return GetDefaultBuyPrice(proto);
}


uint32 BuyOnlyRarePricingStrategy::GetBuyPrice(ItemPrototype const* proto, uint32 auctionHouse)
{
    if (proto->Quality < ITEM_QUALITY_RARE)
    {
        return 0;
    }

    return PricingStrategy::GetBuyPrice(proto, auctionHouse);
}

