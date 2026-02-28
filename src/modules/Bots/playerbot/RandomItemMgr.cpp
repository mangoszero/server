#include "../botpch.h"
#include "playerbot.h"
#include "PlayerbotAIConfig.h"
#include "RandomItemMgr.h"

#include "../../modules/Bots/ahbot/AhBot.h"
#include "DatabaseEnv.h"
#include "PlayerbotAI.h"

#include "../../modules/Bots/ahbot/AhBotConfig.h"

/**
 * @brief Case-insensitive string search.
 *
 * @param str1 The string to search in.
 * @param str2 The string to search for.
 * @return char* Pointer to the first occurrence of str2 in str1, or nullptr if not found.
 */
char * strstri (const char* str1, const char* str2);

/**
 * @brief Predicate to filter items for guild tasks.
 */
class RandomItemGuildTaskPredicate : public RandomItemPredicate
{
public:
    /**
     * @brief Apply the predicate to an item prototype.
     *
     * @param proto The item prototype.
     * @return true If the item matches the predicate.
     * @return false Otherwise.
     */
    virtual bool Apply(ItemPrototype const* proto)
    {
        if (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_QUEST_ITEM ||
                proto->Bonding == BIND_WHEN_USE)
        {
            return false;
        }

        if (proto->Quality < ITEM_QUALITY_NORMAL)
        {
            return false;
        }

        if ((proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON) && proto->Quality >= ITEM_QUALITY_RARE)
        {
            return true;
        }

        if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_CONSUMABLE)
        {
            return true;
        }

        return false;
    }
};

/**
 * @brief Predicate to filter items for guild task rewards.
 */
class RandomItemGuildTaskRewardPredicate : public RandomItemPredicate
{
public:
    /**
     * @brief Construct a new RandomItemGuildTaskRewardPredicate object.
     *
     * @param equip Whether the item is for equipping.
     * @param rare Whether the item is rare.
     */
    RandomItemGuildTaskRewardPredicate(bool equip, bool rare) { this->equip = equip; this->rare = rare;}

    /**
     * @brief Apply the predicate to an item prototype.
     *
     * @param proto The item prototype.
     * @return true If the item matches the predicate.
     * @return false Otherwise.
     */
    virtual bool Apply(ItemPrototype const* proto)
    {
        if (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_QUEST_ITEM ||
                proto->Bonding == BIND_WHEN_USE)
        {
            return false;
        }

        if (proto->Class == ITEM_CLASS_QUEST)
        {
            return false;
        }

        if (equip)
        {
            uint32 desiredQuality = rare ? ITEM_QUALITY_RARE : ITEM_QUALITY_UNCOMMON;
            if (proto->Quality < desiredQuality)
            {
                return false;
            }

            if (proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON)
            {
                return true;
            }
        }
        else
        {
            if (proto->Quality < ITEM_QUALITY_UNCOMMON)
            {
                return false;
            }

            if (proto->Class == ITEM_CLASS_TRADE_GOODS || proto->Class == ITEM_CLASS_CONSUMABLE)
            {
                return true;
            }
        }

        return false;
    }

private:
    bool equip;
    bool rare;
};

/**
 * @brief Construct a new RandomItemMgr object and initialize predicates.
 */
RandomItemMgr::RandomItemMgr()
{
    predicates[RANDOM_ITEM_GUILD_TASK] = new RandomItemGuildTaskPredicate();
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_EQUIP_GREEN] = new RandomItemGuildTaskRewardPredicate(true, false);
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_EQUIP_BLUE] = new RandomItemGuildTaskRewardPredicate(true, true);
    predicates[RANDOM_ITEM_GUILD_TASK_REWARD_TRADE] = new RandomItemGuildTaskRewardPredicate(false, false);
}

/**
 * @brief Destroy the RandomItemMgr object and clean up predicates.
 */
RandomItemMgr::~RandomItemMgr()
{
    for (map<RandomItemType, RandomItemPredicate*>::iterator i = predicates.begin(); i != predicates.end(); ++i)
    {
        delete i->second;
    }

    predicates.clear();
}

/**
 * @brief Handle console command for random item manager.
 *
 * @param handler The chat handler.
 * @param args The command arguments.
 * @return true If the command was handled successfully.
 * @return false Otherwise.
 */
bool RandomItemMgr::HandleConsoleCommand(ChatHandler* handler, char const* args)
{
    if (!args || !*args)
    {
        sLog.outError( "Usage: rnditem");
        return false;
    }

    return false;
}

/**
 * @brief Query the random item list based on type and predicate.
 *
 * @param type The random item type.
 * @param predicate The predicate to filter items.
 * @return RandomItemList The list of items matching the query.
 */
RandomItemList RandomItemMgr::Query(RandomItemType type, RandomItemPredicate* predicate)
{
    RandomItemList &list = cache[type];
    if (list.empty())
    {
        list = cache[type] = Query(type);
    }

    RandomItemList result;
    for (RandomItemList::iterator i = list.begin(); i != list.end(); ++i)
    {
        uint32 itemId = *i;
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (predicate && !predicate->Apply(proto))
        {
            continue;
        }

        result.push_back(itemId);
    }

    return result;
}

/**
 * @brief Query the random item list based on type.
 *
 * @param type The random item type.
 * @return RandomItemList The list of items matching the query.
 */
RandomItemList RandomItemMgr::Query(RandomItemType type)
{
    RandomItemList items;

    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Duration & 0x80000000)
        {
            continue;
        }

        if (sAhBotConfig.ignoreItemIds.find(proto->ItemId) != sAhBotConfig.ignoreItemIds.end())
        {
            continue;
        }

        if (strstri(proto->Name1, "qa") || strstri(proto->Name1, "test") || strstri(proto->Name1, "deprecated"))
        {
            continue;
        }

        if ((proto->RequiredLevel && proto->RequiredLevel > sAhBotConfig.maxRequiredLevel) || proto->ItemLevel > sAhBotConfig.maxItemLevel)
        {
            continue;
        }

        if (predicates[type] && !predicates[type]->Apply(proto))
        {
            continue;
        }

        if (!auctionbot.GetSellPrice(proto))
        {
            continue;
        }

        items.push_back(itemId);
    }

    if (items.empty())
    {
        sLog.outError( "no items available for random item query %u", type);
    }
    return items;
}

/**
 * @brief Get a random item based on type and predicate.
 *
 * @param type The random item type.
 * @param predicate The predicate to filter items.
 * @return uint32 The item ID of the random item.
 */
uint32 RandomItemMgr::GetRandomItem(RandomItemType type, RandomItemPredicate* predicate)
{
    RandomItemList const& list = Query(type, predicate);
    if (list.empty())
    {
        return 0;
    }

    uint32 index = urand(0, list.size() - 1);
    uint32 itemId = list[index];

    return itemId;
}

