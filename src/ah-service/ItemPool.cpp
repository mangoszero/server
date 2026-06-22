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

#include "ItemPool.h"
#include "Log/Log.h"

#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

ItemPool::ItemPool(const ServiceConfig& config, ServiceDatabase& db)
    : m_config(config), m_db(db)
{
}

bool ItemPool::Build()
{
    std::vector<uint32> npcItems;     // Items sold by NPC vendors
    std::vector<uint32> lootItems;    // Items obtained from loot
    std::vector<uint32> includeItems; // Forced-include items
    std::vector<uint32> excludeItems; // Forced-exclude items

    sLog.outString("AHBot seller filters:");

    // Forced include items from configuration.
    {
        std::stringstream includeStream(m_config.GetAHBotIncludes());
        std::string temp;
        while (getline(includeStream, temp, ','))
        {
            includeItems.push_back(atoi(temp.c_str()));
        }
    }

    // Forced exclude items from configuration.
    {
        std::stringstream excludeStream(m_config.GetAHBotExcludes());
        std::string temp;
        while (getline(excludeStream, temp, ','))
        {
            excludeItems.push_back(atoi(temp.c_str()));
        }
    }
    sLog.outString("Forced Inclusion %zu items", includeItems.size());
    sLog.outString("Forced Exclusion %zu items", excludeItems.size());

    // NPC vendor items for filtering.
    sLog.outString("Loading npc vendor items for filter..");
    if (QueryResult* result = m_db.World().Query(
            "SELECT DISTINCT `item` FROM `npc_vendor` WHERE `maxcount` = 0"))
    {
        do
        {
            Field* fields = result->Fetch();
            npcItems.push_back(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }
    sLog.outString("Npc vendor filter has %zu items", npcItems.size());

    // Loot items for filtering. CLASSIC build: prospecting_loot_template is
    // excluded (matches the upstream #if !defined(CLASSIC) UNION member).
    sLog.outString("Loading loot items for filter..");
    if (QueryResult* result = m_db.World().PQuery(
            "SELECT `item` FROM `creature_loot_template` UNION "
            "SELECT `item` FROM `disenchant_loot_template` UNION "
            "SELECT `item` FROM `fishing_loot_template` UNION "
            "SELECT `item` FROM `gameobject_loot_template` UNION "
            "SELECT `item` FROM `item_loot_template` UNION "
            "SELECT `item` FROM `pickpocketing_loot_template` UNION "
            "SELECT `item` FROM `skinning_loot_template`"))
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 entry = fields[0].GetUInt32();
            if (!entry)
            {
                continue;
            }

            lootItems.push_back(fields[0].GetUInt32());
        }
        while (result->NextRow());
        delete result;
    }
    sLog.outString("Loot filter has %zu items", lootItems.size());

    sLog.outString("Sorting and cleaning items for AHBot seller...");

    uint32 itemsAdded = 0;

    // Substitution: source iterates sItemStorage and skips null prototypes;
    // here the prototypes come straight from item_template (existing rows
    // only), which is the same set. Only the fields the filter tests are
    // selected.
    QueryResult* protoResult = m_db.World().Query(
        "SELECT `entry`, `class`, `subclass`, `Quality`, `Flags`, "
        "`BuyPrice`, `SellPrice`, `ItemLevel`, `RequiredLevel`, "
        "`RequiredSkillRank`, `bonding`, `lockid`, `stackable` "
        "FROM `item_template`");

    if (!protoResult)
    {
        sLog.outError("ah-service: item_template query returned null;"
                      " seller has no items.");
        return false;
    }

    do
    {
        Field* fields = protoResult->Fetch();

        ItemProto proto;
        proto.entry             = fields[0].GetUInt32();
        proto.itemClass         = fields[1].GetUInt32();
        proto.subClass          = fields[2].GetUInt32();
        proto.quality           = fields[3].GetUInt32();
        proto.flags             = fields[4].GetUInt32();
        proto.buyPrice          = fields[5].GetUInt32();
        proto.sellPrice         = fields[6].GetUInt32();
        proto.itemLevel         = fields[7].GetUInt32();
        proto.requiredLevel     = fields[8].GetUInt32();
        proto.requiredSkillRank = fields[9].GetUInt32();
        proto.bonding           = fields[10].GetUInt32();
        proto.lockId            = fields[11].GetUInt32();
        proto.stackable         = fields[12].GetUInt32();

        const uint32 itemID = proto.entry;

        // Skip items with too high quality.
        if (proto.quality >= AH_MAX_AUCTION_QUALITY)
        {
            continue;
        }

        // Forced exclude filter.
        bool isExcludeItem = false;
        for (size_t i = 0; (i < excludeItems.size() && (!isExcludeItem)); ++i)
        {
            if (itemID == excludeItems[i])
            {
                isExcludeItem = true;
            }
        }
        if (isExcludeItem)
        {
            continue;
        }

        // Forced include filter.
        bool isForcedIncludeItem = false;
        for (size_t i = 0;
             (i < includeItems.size() && (!isForcedIncludeItem)); ++i)
        {
            if (itemID == includeItems[i])
            {
                isForcedIncludeItem = true;
            }
        }
        if (isForcedIncludeItem)
        {
            m_ItemPool[proto.quality][proto.itemClass].push_back(itemID);
            ItemSellInfo info;
            info.buyPrice  = proto.buyPrice;
            info.sellPrice = proto.sellPrice;
            info.stackable = proto.stackable;
            m_SellInfo[itemID] = info;
            ++itemsAdded;
            continue;
        }

        // Bonding filters.
        switch (proto.bonding)
        {
            case AH_NO_BIND:
                if (!m_config.getConfig(AHBOT_CONFIG_BOOL_BIND_NO))
                {
                    continue;
                }
                break;
            case AH_BIND_WHEN_PICKED_UP:
                if (!m_config.getConfig(AHBOT_CONFIG_BOOL_BIND_PICKUP))
                {
                    continue;
                }
                break;
            case AH_BIND_WHEN_EQUIPPED:
                if (!m_config.getConfig(AHBOT_CONFIG_BOOL_BIND_EQUIP))
                {
                    continue;
                }
                break;
            case AH_BIND_WHEN_USE:
                if (!m_config.getConfig(AHBOT_CONFIG_BOOL_BIND_USE))
                {
                    continue;
                }
                break;
            case AH_BIND_QUEST_ITEM:
                if (!m_config.getConfig(AHBOT_CONFIG_BOOL_BIND_QUEST))
                {
                    continue;
                }
                break;
            default:
                continue;
        }

        // Price filter.
        if (m_config.getConfig(AHBOT_CONFIG_BOOL_BUYPRICE_SELLER))
        {
            if (proto.buyPrice == 0)
            {
                continue;
            }
        }
        else
        {
            if (proto.sellPrice == 0)
            {
                continue;
            }
        }

        // Vendor filter.
        if (!m_config.getConfig(AHBOT_CONFIG_BOOL_ITEMS_VENDOR))
        {
            bool isVendorItem = false;
            for (size_t i = 0; (i < npcItems.size()) && (!isVendorItem); ++i)
            {
                if (itemID == npcItems[i])
                {
                    isVendorItem = true;
                }
            }
            if (isVendorItem)
            {
                continue;
            }
        }

        // Loot filter.
        if (!m_config.getConfig(AHBOT_CONFIG_BOOL_ITEMS_LOOT))
        {
            bool isLootItem = false;
            for (size_t i = 0; (i < lootItems.size()) && (!isLootItem); ++i)
            {
                if (itemID == lootItems[i])
                {
                    isLootItem = true;
                }
            }
            if (isLootItem)
            {
                continue;
            }
        }

        // Miscellaneous filter.
        if (!m_config.getConfig(AHBOT_CONFIG_BOOL_ITEMS_MISC))
        {
            bool isVendorItem = false;
            bool isLootItem = false;

            for (size_t i = 0; (i < npcItems.size()) && (!isVendorItem); ++i)
            {
                if (itemID == npcItems[i])
                {
                    isVendorItem = true;
                }
            }

            for (size_t i = 0; (i < lootItems.size()) && (!isLootItem); ++i)
            {
                if (itemID == lootItems[i])
                {
                    isLootItem = true;
                }
            }

            if ((!isLootItem) && (!isVendorItem))
            {
                continue;
            }
        }

        // Item class/subclass specific filters.
        switch (proto.itemClass)
        {
            case AH_ITEM_CLASS_ARMOR:
            case AH_ITEM_CLASS_WEAPON:
            {
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MIN_ITEM_LEVEL))
                {
                    if (proto.itemLevel < value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MAX_ITEM_LEVEL))
                {
                    if (proto.itemLevel > value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MIN_REQ_LEVEL))
                {
                    if (proto.requiredLevel < value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MAX_REQ_LEVEL))
                {
                    if (proto.requiredLevel > value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MIN_SKILL_RANK))
                {
                    if (proto.requiredSkillRank < value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MAX_SKILL_RANK))
                {
                    if (proto.requiredSkillRank > value)
                    {
                        continue;
                    }
                }
                break;
            }
            case AH_ITEM_CLASS_RECIPE:
            case AH_ITEM_CLASS_CONSUMABLE:
            case AH_ITEM_CLASS_PROJECTILE:
            {
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MIN_REQ_LEVEL))
                {
                    if (proto.requiredLevel < value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MAX_REQ_LEVEL))
                {
                    if (proto.requiredLevel > value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MIN_SKILL_RANK))
                {
                    if (proto.requiredSkillRank < value)
                    {
                        continue;
                    }
                }
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_ITEM_MAX_SKILL_RANK))
                {
                    if (proto.requiredSkillRank > value)
                    {
                        continue;
                    }
                }
                break;
            }
            case AH_ITEM_CLASS_MISC:
                // CLASSIC build: the ITEM_SUBCLASS_JUNK_MOUNT skill/level
                // filter lives behind #if !defined(CLASSIC) upstream and is
                // not applied here.
                if (proto.flags & AH_ITEM_FLAG_LOOTABLE)
                {
                    // Skip any not-locked lootable items (mostly quest
                    // specific or reward cases).
                    if (!proto.lockId)
                    {
                        continue;
                    }

                    if (!m_config.getConfig(AHBOT_CONFIG_BOOL_LOCKBOX_ENABLED))
                    {
                        continue;
                    }
                }

                break;
            case AH_ITEM_CLASS_TRADE_GOODS:
            {
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_MIN_ITEM_LEVEL))
                {
                    if (proto.itemLevel < value)
                    {
                        continue;
                    }
                }

                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_MAX_ITEM_LEVEL))
                {
                    if (proto.itemLevel > value)
                    {
                        continue;
                    }
                }

                break;
            }
            case AH_ITEM_CLASS_CONTAINER:
            case AH_ITEM_CLASS_QUIVER:
            {
                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_CLASS_CONTAINER_MIN_ITEM_LEVEL))
                {
                    if (proto.itemLevel < value)
                    {
                        continue;
                    }
                }

                if (uint32 value = m_config.getConfig(
                        AHBOT_CONFIG_UINT32_CLASS_CONTAINER_MAX_ITEM_LEVEL))
                {
                    if (proto.itemLevel > value)
                    {
                        continue;
                    }
                }

                break;
            }

            default:
                continue;
        }

        // Add item to the pool.
        m_ItemPool[proto.quality][proto.itemClass].push_back(itemID);
        ItemSellInfo info;
        info.buyPrice  = proto.buyPrice;
        info.sellPrice = proto.sellPrice;
        info.stackable = proto.stackable;
        m_SellInfo[itemID] = info;
        ++itemsAdded;
    }
    while (protoResult->NextRow());

    delete protoResult;

    if (!itemsAdded)
    {
        sLog.outError("AuctionHouseBot seller not have items, disabled.");
        return false;
    }

    sLog.outString("AuctionHouseBot seller will use %u items to fill auction"
                   " house (according your config choices)", itemsAdded);
    return true;
}

bool ItemPool::GetSellInfo(uint32 itemId, ItemSellInfo& out) const
{
    std::map<uint32, ItemSellInfo>::const_iterator itr =
        m_SellInfo.find(itemId);
    if (itr == m_SellInfo.end())
    {
        return false;
    }
    out = itr->second;
    return true;
}

uint32 ItemPool::GetTotalCount() const
{
    uint32 total = 0;
    for (uint32 q = 0; q < AH_MAX_AUCTION_QUALITY; ++q)
    {
        for (uint32 c = 0; c < AH_MAX_ITEM_CLASS; ++c)
        {
            total += uint32(m_ItemPool[q][c].size());
        }
    }
    return total;
}

void ItemPool::LogSummary() const
{
    sLog.outString("Items loaded      \tGrey\tWhite\tGreen\tBlue\tPurple"
                   "\tOrange\tYellow");
    for (uint32 c = 0; c < AH_MAX_ITEM_CLASS; ++c)
    {
        sLog.outString("class %-2u          \t%zu\t%zu\t%zu\t%zu\t%zu"
                       "\t%zu\t%zu", c,
                       m_ItemPool[0][c].size(), m_ItemPool[1][c].size(),
                       m_ItemPool[2][c].size(), m_ItemPool[3][c].size(),
                       m_ItemPool[4][c].size(), m_ItemPool[5][c].size(),
                       m_ItemPool[6][c].size());
    }
    sLog.outString("AHBot seller item pool grand total: %u items",
                   GetTotalCount());
}
