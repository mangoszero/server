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

#ifndef AH_SERVICE_ITEM_POOL_H
#define AH_SERVICE_ITEM_POOL_H

#include "Common.h"
#include "AhBotDefines.h"
#include "ServiceConfig.h"
#include "ServiceDatabase.h"

#include <vector>

/**
 * @file ItemPool.h
 * @brief Child-owned port of AuctionBotSeller's item-pool build.
 *
 * This is a faithful reimplementation of AuctionBotSeller::Initialize()
 * (src/game/AuctionHouseBot/AuctionHouseBot.cpp). It builds the seller's pool
 * of auctionable item IDs, bucketed by [quality][class], applying the same
 * filter predicates and config thresholds the in-process bot uses.
 *
 * The ONLY sanctioned deviation: the child has no @c sItemStorage, so item
 * prototypes are sourced by a direct @c item_template query instead of the
 * in-process storage. The filter logic is otherwise identical.
 */

/// A pool of item IDs (mirror of the source's @c ItemPool typedef).
typedef std::vector<uint32> ItemIdPool;

/**
 * @brief Builds and holds the seller's auctionable item pool.
 */
class ItemPool
{
    public:
        /**
         * @brief Construct against a loaded config and an open read-only DB.
         * @param config Loaded AH bot config (filter thresholds + toggles)
         * @param db     Open read-only world database
         */
        ItemPool(const ServiceConfig& config, ServiceDatabase& db);

        /**
         * @brief Build the pool from item_template + filter lists.
         *
         * Mirrors AuctionBotSeller::Initialize(): loads the npc-vendor and
         * loot filter lists, then iterates item prototypes applying the bot's
         * filters into @c m_ItemPool[quality][class].
         *
         * @return true if at least one item was added, false otherwise (the
         *         source disables the seller when zero items qualify).
         */
        bool Build();

        /**
         * @brief Print per-[quality][class] counts and the grand total.
         */
        void LogSummary() const;

        /**
         * @brief Total number of item IDs across all buckets.
         */
        uint32 GetTotalCount() const;

        /**
         * @brief Access a single [quality][class] bucket.
         */
        const ItemIdPool& GetBucket(uint32 quality, uint32 itemClass) const
        {
            return m_ItemPool[quality][itemClass];
        }

    private:
        /**
         * @brief Minimal item prototype: only the fields the filter tests.
         *
         * Sourced from item_template instead of sItemStorage.
         */
        struct ItemProto
        {
            uint32 entry;
            uint32 itemClass;
            uint32 subClass;
            uint32 quality;
            uint32 flags;
            uint32 buyPrice;
            uint32 sellPrice;
            uint32 itemLevel;
            uint32 requiredLevel;
            uint32 requiredSkillRank;
            uint32 bonding;
            uint32 lockId;
        };

        const ServiceConfig& m_config;
        ServiceDatabase& m_db;
        ItemIdPool m_ItemPool[AH_MAX_AUCTION_QUALITY][AH_MAX_ITEM_CLASS];

        // Non-copyable.
        ItemPool(const ItemPool&);
        ItemPool& operator=(const ItemPool&);
};

#endif // AH_SERVICE_ITEM_POOL_H
