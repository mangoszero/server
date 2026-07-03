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

#include "BotBrain.h"
#include "Common.h"
#include "Utilities/Util.h"
#include "Log/Log.h"

#include <ctime>
#include <limits>
#include <map>
#include <vector>

// ---------------------------------------------------------------------------
// SellerHouseConfig / BuyerHouseConfig
// ---------------------------------------------------------------------------

SellerHouseConfig::SellerHouseConfig()
    : houseType(0), lastMissedItem(0), minTime(0), maxTime(0)
{
    for (uint32 q = 0; q < AH_MAX_AUCTION_QUALITY; ++q)
    {
        amountPerQuality[q] = 0;
        priceRatioPerQuality[q] = 0;
        for (uint32 c = 0; c < AH_MAX_ITEM_CLASS; ++c)
        {
            quantity[q][c] = 0;
            amountPerClass[q][c] = 0;
            missItems[q][c] = 0;
        }
    }
}

uint32 SellerHouseConfig::GetMinTime() const
{
    // Mirror of AHB_Seller_Config::GetMinTime().
    if (minTime < 1)
    {
        return 1;
    }
    else if ((maxTime) && (minTime > maxTime))
    {
        return maxTime;
    }
    else
    {
        return minTime;
    }
}

BuyerHouseConfig::BuyerHouseConfig()
    : houseType(0), buyerEnabled(false), buyerPriceRatio(0), factionChance(0)
{
}

// ---------------------------------------------------------------------------
// BotBrain
// ---------------------------------------------------------------------------

BotBrain::BotBrain(const ServiceConfig& config, const ItemPool& pool,
                   const MarketSnapshot& snapshot, uint32 botGuid,
                   uint32 runId)
    : m_config(config),
      m_pool(pool),
      m_snapshot(snapshot),
      m_botGuid(botGuid),
      m_runId(runId),
      m_seq(0),
      m_operationSelector(0),
      m_sellerEnabled(false),
      m_buyerEnabled(false)
{
    for (uint32 i = 0; i < AH_MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        m_sellerHouse[i].houseType = static_cast<uint8>(i);
        m_buyerHouse[i].houseType = static_cast<uint8>(i);
    }
}

// ---------------------------------------------------------------------------
// uuid stamping: high 32 bits = run-id, low 32 bits = per-child sequence.
// Sequence starts at 1 (incremented before use) per the Task 7/8 contract.
// ---------------------------------------------------------------------------

uint64 BotBrain::NextUuid()
{
    ++m_seq;
    return (static_cast<uint64>(m_runId) << 32) | static_cast<uint64>(m_seq);
}

void BotBrain::SeedSeqPast(uint32 maxSeq)
{
    // maxSeq = highest low-half seq [1, 0x7FFFFFFF] already persisted for this
    // runId (0 = none). Skip m_seq past it so the next NextUuid() (which does
    // ++m_seq) mints maxSeq+1 and cannot re-collide with a retained bot uuid.
    // 0x7FFFFFFF sentinel: never advance to where ++m_seq would spill into the
    // MutationHandler minter's high half [0x80000000+].
    if (maxSeq != 0u && maxSeq != 0x7FFFFFFFu && m_seq < maxSeq)
    {
        m_seq = maxSeq;
    }
}

// ---------------------------------------------------------------------------
// Config helpers (mirror AuctionBotConfig::getConfigItemAmountRatio etc.)
// ---------------------------------------------------------------------------

uint32 BotBrain::GetItemAmountRatio(uint8 houseType) const
{
    switch (houseType)
    {
        case AH_AUCTION_HOUSE_ALLIANCE:
            return m_config.getConfig(
                AHBOT_CONFIG_UINT32_ALLIANCE_ITEM_AMOUNT_RATIO);
        case AH_AUCTION_HOUSE_HORDE:
            return m_config.getConfig(
                AHBOT_CONFIG_UINT32_HORDE_ITEM_AMOUNT_RATIO);
        default:
            return m_config.getConfig(
                AHBOT_CONFIG_UINT32_NEUTRAL_ITEM_AMOUNT_RATIO);
    }
}

bool BotBrain::GetBuyerEnabledFor(uint8 houseType) const
{
    switch (houseType)
    {
        case AH_AUCTION_HOUSE_ALLIANCE:
            return m_config.getConfig(
                AHBOT_CONFIG_BOOL_BUYER_ALLIANCE_ENABLED);
        case AH_AUCTION_HOUSE_HORDE:
            return m_config.getConfig(
                AHBOT_CONFIG_BOOL_BUYER_HORDE_ENABLED);
        default:
            return m_config.getConfig(
                AHBOT_CONFIG_BOOL_BUYER_NEUTRAL_ENABLED);
    }
}

// ---------------------------------------------------------------------------
// SnapshotConfig - shared config-snapshot logic for Initialize/Reinitialize.
//
// Reads every non-pool tuning value from m_config and writes it into the
// per-house seller/buyer config tables.  Does NOT touch m_operationSelector
// or m_buyerLastChecked[], so it is safe to call at any point during the
// service loop without disturbing rotation state or the recheck throttle.
//
// Fields re-snapshotted on every call:
//   m_sellerEnabled
//   m_buyerEnabled
//   m_sellerHouse[*]: amountPerQuality[], priceRatioPerQuality[],
//                     quantity[][], amountPerClass[][], minTime, maxTime
//   m_buyerHouse[*]:  buyerEnabled, buyerPriceRatio, factionChance
//
// Pool-affecting thresholds (ItemLevel/ReqLevel/ReqSkill/Bind/
// Items.Vendor/Loot/Misc/Include/Exclude) are consumed only by
// ItemPool::Build(), which runs once at startup and is NOT repeated here.
// ---------------------------------------------------------------------------

void BotBrain::SnapshotConfig()
{
    // --- Seller ---
    // The seller is enabled only if its master toggle is set AND the bot
    // GUID resolved (a guid-0 owner is invalid -> no seller intents).
    m_sellerEnabled =
        m_config.getConfig(AHBOT_CONFIG_BOOL_SELLER_ENABLED) &&
        (m_botGuid != 0);

    if (m_config.getConfig(AHBOT_CONFIG_BOOL_SELLER_ENABLED) &&
        (m_botGuid == 0))
    {
        sLog.outError(
            "BotBrain: seller enabled but bot GUID unresolved"
            " (check AuctionHouseBot.CharacterName) - not selling.");
    }

    for (uint32 i = 0; i < AH_MAX_AUCTION_HOUSE_TYPE; ++i)
    {
        // Mirror LoadConfig: only load houses with a non-zero amount ratio.
        if (GetItemAmountRatio(static_cast<uint8>(i)))
        {
            LoadSellerHouse(m_sellerHouse[i]);
        }
    }

    // --- Buyer ---
    // Reset the aggregate flag before re-checking each house.
    m_buyerEnabled = false;

    // A Bid/Buyout intent carries the bot's GUID and the buyer's candidate
    // predicate keys on ownerGuid == botGuid, so the buyer also needs the
    // GUID resolved; with guid 0 the emitted intents would be invalid.
    if (m_config.getConfig(AHBOT_CONFIG_BOOL_BUYER_ENABLED) &&
        (m_botGuid != 0))
    {
        for (uint32 i = 0; i < AH_MAX_AUCTION_HOUSE_TYPE; ++i)
        {
            m_buyerHouse[i].buyerEnabled =
                GetBuyerEnabledFor(static_cast<uint8>(i));
            if (m_buyerHouse[i].buyerEnabled)
            {
                LoadBuyerHouse(m_buyerHouse[i]);
                m_buyerEnabled = true;
            }
        }
    }
    else if (m_config.getConfig(AHBOT_CONFIG_BOOL_BUYER_ENABLED) &&
             (m_botGuid == 0))
    {
        sLog.outError(
            "BotBrain: buyer enabled but bot GUID unresolved"
            " (check AuctionHouseBot.CharacterName) - not buying.");
    }
    else
    {
        // Buyer master toggle off: clear all per-house buyer flags.
        for (uint32 i = 0; i < AH_MAX_AUCTION_HOUSE_TYPE; ++i)
        {
            m_buyerHouse[i].buyerEnabled = false;
        }
    }
}

// ---------------------------------------------------------------------------
// Initialize - build per-house seller/buyer config tables.
//
// Mirrors AuctionBotSeller::LoadConfig + LoadSellerValues + LoadItemsQuantity
// and AuctionBotBuyer::LoadConfig + LoadBuyerValues. CLASSIC build: the GEM /
// GENERIC item classes live behind #if !defined(CLASSIC) upstream and are not
// set here (they stay 0).
// ---------------------------------------------------------------------------

void BotBrain::Initialize()
{
    SnapshotConfig();
}

// ---------------------------------------------------------------------------
// Reinitialize - re-snapshot non-pool config after a live reload.
//
// Called after ServiceConfig::Reload() succeeds (GM "ahbot reload" path).
// Re-runs SnapshotConfig() so all tuning values are current; does NOT
// rebuild the item pool or touch runtime state (m_operationSelector,
// m_buyerLastChecked[]).
// ---------------------------------------------------------------------------

void BotBrain::Reinitialize()
{
    SnapshotConfig();
}

void BotBrain::LoadBuyerHouse(BuyerHouseConfig& cfg)
{
    // Mirror of AuctionBotBuyer::LoadBuyerValues.
    uint32 factionChance;
    switch (cfg.houseType)
    {
        case AH_AUCTION_HOUSE_ALLIANCE:
            cfg.buyerPriceRatio =
                m_config.getConfig(AHBOT_CONFIG_UINT32_ALLIANCE_PRICE_RATIO)
                + 50;
            factionChance = m_config.getConfig(
                AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_ALLIANCE);
            break;
        case AH_AUCTION_HOUSE_HORDE:
            cfg.buyerPriceRatio =
                m_config.getConfig(AHBOT_CONFIG_UINT32_HORDE_PRICE_RATIO)
                + 50;
            factionChance = m_config.getConfig(
                AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_HORDE);
            break;
        default:
            cfg.buyerPriceRatio =
                m_config.getConfig(AHBOT_CONFIG_UINT32_NEUTRAL_PRICE_RATIO)
                + 50;
            factionChance = m_config.getConfig(
                AHBOT_CONFIG_UINT32_BUYER_CHANCE_RATIO_NEUTRAL);
            break;
    }
    cfg.factionChance = 5000 * factionChance;
}

void BotBrain::LoadSellerHouse(SellerHouseConfig& cfg)
{
    // === LoadItemsQuantity: per-quality amounts (ratio-scaled) ============
    const uint32 ratio = GetItemAmountRatio(cfg.houseType);

    cfg.amountPerQuality[AH_ITEM_QUALITY_POOR] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_GREY_AMOUNT) * ratio
        / 100;
    cfg.amountPerQuality[AH_ITEM_QUALITY_NORMAL] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_WHITE_AMOUNT) * ratio
        / 100;
    cfg.amountPerQuality[AH_ITEM_QUALITY_UNCOMMON] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_GREEN_AMOUNT) * ratio
        / 100;
    cfg.amountPerQuality[AH_ITEM_QUALITY_RARE] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_BLUE_AMOUNT) * ratio
        / 100;
    cfg.amountPerQuality[AH_ITEM_QUALITY_EPIC] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_PURPLE_AMOUNT) * ratio
        / 100;
    cfg.amountPerQuality[AH_ITEM_QUALITY_LEGENDARY] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_ORANGE_AMOUNT) * ratio
        / 100;
    cfg.amountPerQuality[AH_ITEM_QUALITY_ARTIFACT] =
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEM_YELLOW_AMOUNT) * ratio
        / 100;

    // === LoadItemsQuantity: per-[quality][class] quantity table ===========
    // Verbatim mirror of the source's SetItemsQuantityPerClass block. A 0
    // means "do not produce that class-color combination" (avoids selecting
    // non-existent class-color items).
    const uint32 cConsum =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_CONSUMABLE_AMOUNT);
    const uint32 cContain =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_CONTAINER_AMOUNT);
    const uint32 cWeapon =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_WEAPON_AMOUNT);
    const uint32 cArmor =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_ARMOR_AMOUNT);
    const uint32 cReagent =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_REAGENT_AMOUNT);
    const uint32 cProj =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_PROJECTILE_AMOUNT);
    const uint32 cTrade =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_TRADEGOOD_AMOUNT);
    const uint32 cRecipe =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_RECIPE_AMOUNT);
    const uint32 cQuiver =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_QUIVER_AMOUNT);
    const uint32 cQuest =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_QUEST_AMOUNT);
    const uint32 cKey =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_KEY_AMOUNT);
    const uint32 cMisc =
        m_config.getConfig(AHBOT_CONFIG_UINT32_CLASS_MISC_AMOUNT);

    uint32 (&q)[AH_MAX_AUCTION_QUALITY][AH_MAX_ITEM_CLASS] = cfg.quantity;

    // GREY
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_CONSUMABLE]  = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_CONTAINER]   = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_REAGENT]     = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_PROJECTILE]  = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_TRADE_GOODS] = cTrade;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_RECIPE]      = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_QUIVER]      = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_QUEST]       = cQuest;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_KEY]         = 0;
    q[AH_ITEM_QUALITY_POOR][AH_ITEM_CLASS_MISC]        = cMisc;

    // WHITE
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_CONSUMABLE]  = cConsum;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_CONTAINER]   = cContain;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_REAGENT]     = cReagent;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_PROJECTILE]  = cProj;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_TRADE_GOODS] = cTrade;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_RECIPE]      = cRecipe;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_QUIVER]      = cQuiver;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_QUEST]       = cQuest;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_KEY]         = cKey;
    q[AH_ITEM_QUALITY_NORMAL][AH_ITEM_CLASS_MISC]        = cMisc;

    // GREEN
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_CONSUMABLE]  = cConsum;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_CONTAINER]   = cContain;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_REAGENT]     = 0;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_PROJECTILE]  = cProj;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_TRADE_GOODS] = cTrade;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_RECIPE]      = cRecipe;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_QUIVER]      = cQuiver;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_QUEST]       = cQuest;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_KEY]         = cKey;
    q[AH_ITEM_QUALITY_UNCOMMON][AH_ITEM_CLASS_MISC]        = cMisc;

    // BLUE
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_CONSUMABLE]  = cConsum;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_CONTAINER]   = cContain;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_REAGENT]     = 0;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_PROJECTILE]  = cProj;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_TRADE_GOODS] = cTrade;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_RECIPE]      = cRecipe;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_QUIVER]      = cQuiver;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_QUEST]       = cQuest;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_KEY]         = 0;
    q[AH_ITEM_QUALITY_RARE][AH_ITEM_CLASS_MISC]        = cMisc;

    // PURPLE
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_CONSUMABLE]  = cConsum;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_CONTAINER]   = cContain;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_REAGENT]     = 0;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_PROJECTILE]  = cProj;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_TRADE_GOODS] = cTrade;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_RECIPE]      = cRecipe;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_QUIVER]      = 0;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_QUEST]       = cQuest;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_KEY]         = 0;
    q[AH_ITEM_QUALITY_EPIC][AH_ITEM_CLASS_MISC]        = cMisc;

    // ORANGE
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_CONSUMABLE]  = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_CONTAINER]   = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_REAGENT]     = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_PROJECTILE]  = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_TRADE_GOODS] = cTrade;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_RECIPE]      = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_QUIVER]      = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_QUEST]       = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_KEY]         = 0;
    q[AH_ITEM_QUALITY_LEGENDARY][AH_ITEM_CLASS_MISC]        = 0;

    // YELLOW
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_CONSUMABLE]  = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_CONTAINER]   = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_WEAPON]      = cWeapon;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_ARMOR]       = cArmor;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_REAGENT]     = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_PROJECTILE]  = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_TRADE_GOODS] = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_RECIPE]      = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_QUIVER]      = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_QUEST]       = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_KEY]         = 0;
    q[AH_ITEM_QUALITY_ARTIFACT][AH_ITEM_CLASS_MISC]        = 0;

    // === LoadItemsQuantity: derive per-class desired amount ================
    // indice = amountPerQuality / sum(all class amounts); then
    // SetItemsAmountPerClass multiplies the indice by each class quantity.
    const uint32 classSum =
        cConsum + cContain + cWeapon
        + 0 /* GEM (CLASSIC) */ + cArmor + cReagent
        + cProj + cTrade + 0 /* GENERIC (CLASSIC) */
        + cRecipe + cQuiver + cQuest
        + cKey + cMisc;

    for (uint32 j = 0; j < AH_MAX_AUCTION_QUALITY; ++j)
    {
        // Guard against div-by-zero (the source divides unconditionally; a
        // zero classSum would crash, so all class amounts being zero means
        // no items wanted for any class).
        uint32 indice = (classSum != 0)
            ? (cfg.amountPerQuality[j] / classSum)
            : 0;
        for (uint32 i = 0; i < AH_MAX_ITEM_CLASS; ++i)
        {
            // Mirror SetItemsAmountPerClass: amount = indice * quantity.
            cfg.amountPerClass[j][i] = indice * cfg.quantity[j][i];
        }
    }

    // === LoadSellerValues: price ratio per quality + min/max time =========
    uint32 priceRatio;
    switch (cfg.houseType)
    {
        case AH_AUCTION_HOUSE_ALLIANCE:
            priceRatio = m_config.getConfig(
                AHBOT_CONFIG_UINT32_ALLIANCE_PRICE_RATIO);
            break;
        case AH_AUCTION_HOUSE_HORDE:
            priceRatio = m_config.getConfig(
                AHBOT_CONFIG_UINT32_HORDE_PRICE_RATIO);
            break;
        default:
            priceRatio = m_config.getConfig(
                AHBOT_CONFIG_UINT32_NEUTRAL_PRICE_RATIO);
            break;
    }
    for (uint32 j = 0; j < AH_MAX_AUCTION_QUALITY; ++j)
    {
        cfg.priceRatioPerQuality[j] = priceRatio;
    }

    // Mirror SetMinTime / SetMaxTime (raw config values; GetMinTime() applies
    // the >= 1 / <= maxTime clamp at use time).
    cfg.minTime = m_config.getConfig(AHBOT_CONFIG_UINT32_MINTIME);
    cfg.maxTime = m_config.getConfig(AHBOT_CONFIG_UINT32_MAXTIME);
}

// ---------------------------------------------------------------------------
// Orchestration - mirror of AuctionHouseBot::Update().
//
// Rotates m_operationSelector over 0..2*MAX_HOUSE-1, doing ONE seller- or
// buyer-house operation per call and stopping at the first that did work.
// ---------------------------------------------------------------------------

void BotBrain::RunOneOperation(std::vector<EmittedIntent>& out)
{
    if (!m_sellerEnabled && !m_buyerEnabled)
    {
        return;
    }

    for (uint32 count = 0;
         count < 2 * AH_MAX_AUCTION_HOUSE_TYPE; ++count)
    {
        bool successStep = false;

        if (m_operationSelector < AH_MAX_AUCTION_HOUSE_TYPE)
        {
            if (m_sellerEnabled)
            {
                successStep = SellerUpdate(
                    static_cast<uint8>(m_operationSelector), out);
            }
        }
        else
        {
            if (m_buyerEnabled)
            {
                successStep = BuyerUpdate(
                    static_cast<uint8>(
                        m_operationSelector - AH_MAX_AUCTION_HOUSE_TYPE),
                    out);
            }
        }

        ++m_operationSelector;
        if (m_operationSelector >= 2 * AH_MAX_AUCTION_HOUSE_TYPE)
        {
            m_operationSelector = 0;
        }

        // One successful update per call.
        if (successStep)
        {
            break;
        }
    }
    // (The in-process Update() also calls PurgeMailedItems() here; that is a
    // DB-mutating cleanup owned by mangosd, so the child does not do it.)
}

// ---------------------------------------------------------------------------
// SellerUpdate - mirror of AuctionBotSeller::Update().
// ---------------------------------------------------------------------------

bool BotBrain::SellerUpdate(uint8 houseType,
                            std::vector<EmittedIntent>& out)
{
    if (GetItemAmountRatio(houseType) > 0)
    {
        if (SetStat(m_sellerHouse[houseType]))
        {
            addNewAuctions(m_sellerHouse[houseType], out);
        }
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// SetStat - REIMPLEMENTED against MarketSnapshot.
//
// Source (cpp:2215) scans the live sAuctionMgr map for the house, counting
// auctions owned by the bot per [Quality][Class], then fills MissItems =
// max(0, AmountOfItems - found). We do the SAME, reading the snapshot's
// records (ownerGuid == botGuid) and the record's stored quality/itemClass
// (already joined from item_template), instead of dereferencing a live Item.
// ---------------------------------------------------------------------------

uint32 BotBrain::SetStat(SellerHouseConfig& cfg)
{
    // Count items in AH owned by the bot, per [quality][class].
    uint32 itemsInAH[AH_MAX_AUCTION_QUALITY][AH_MAX_ITEM_CLASS];
    for (uint32 j = 0; j < AH_MAX_AUCTION_QUALITY; ++j)
    {
        for (uint32 i = 0; i < AH_MAX_ITEM_CLASS; ++i)
        {
            itemsInAH[j][i] = 0;
        }
    }

    const std::vector<AuctionRecord>& house =
        m_snapshot.GetHouse(cfg.houseType);
    for (size_t r = 0; r < house.size(); ++r)
    {
        const AuctionRecord& rec = house[r];
        // Count only auctions owned by the bot (the source's filter), and
        // only those whose quality/class fall in the tracked ranges.
        if (rec.ownerGuid != m_botGuid)
        {
            continue;
        }
        if (rec.quality < AH_MAX_AUCTION_QUALITY &&
            rec.itemClass < AH_MAX_ITEM_CLASS)
        {
            ++itemsInAH[rec.quality][rec.itemClass];
        }
    }

    // Fill MissItems = max(0, AmountOfItems - found); accumulate total.
    uint32 count = 0;
    for (uint32 j = 0; j < AH_MAX_AUCTION_QUALITY; ++j)
    {
        for (uint32 i = 0; i < AH_MAX_ITEM_CLASS; ++i)
        {
            // Mirror SetMissedItemsPerClass.
            if (cfg.amountPerClass[j][i] > itemsInAH[j][i])
            {
                cfg.missItems[j][i] =
                    cfg.amountPerClass[j][i] - itemsInAH[j][i];
            }
            else
            {
                cfg.missItems[j][i] = 0;
            }
            count += cfg.missItems[j][i];
        }
    }

    cfg.lastMissedItem = count;
    return count;
}

// ---------------------------------------------------------------------------
// getRandomArray - PORTED VERBATIM (cpp:2279).
// ---------------------------------------------------------------------------

bool BotBrain::getRandomArray(
    const SellerHouseConfig& cfg, RandomArray& ra,
    const std::vector<std::vector<uint32> >& addedItem) const
{
    ra.clear();
    bool Ok = false;

    for (uint32 j = 0; j < AH_MAX_AUCTION_QUALITY; ++j)
    {
        for (uint32 i = 0; i < AH_MAX_ITEM_CLASS; ++i)
        {
            if ((cfg.missItems[j][i] > addedItem[j][i]) &&
                !m_pool.GetBucket(j, i).empty())
            {
                RandomArrayEntry miss_item;
                miss_item.color = j;
                miss_item.itemclass = i;
                ra.push_back(miss_item);
                Ok = true;
            }
        }
    }
    return Ok;
}

// ---------------------------------------------------------------------------
// SetPricesOfItem - PORTED VERBATIM (cpp:2313).
// ---------------------------------------------------------------------------

void BotBrain::SetPricesOfItem(const SellerHouseConfig& cfg, uint32& buyp,
                               uint32& bidp, uint32 stackcnt,
                               uint32 itemQuality) const
{
    double temp_buyp = buyp * stackcnt *
        (itemQuality < AH_MAX_AUCTION_QUALITY
            ? cfg.priceRatioPerQuality[itemQuality] : 1);

    double randrange = temp_buyp * 0.4;

    uint32 buypMin = (uint32)temp_buyp - (uint32)randrange;
    uint32 buypMax = ((uint32)temp_buyp + (uint32)randrange) < temp_buyp
        ? std::numeric_limits<uint32>::max() : temp_buyp + randrange;

    buyp = (urand(buypMin, buypMax) / 100) + 1;

    double urandrange = buyp * 40;
    double temp_bidp = buyp * 50;
    uint32 bidPmin = (uint32)temp_bidp - (uint32)urandrange;
    uint32 bidPmax = ((uint32)temp_bidp + (uint32)urandrange) < temp_bidp
        ? std::numeric_limits<uint32>::max() : temp_bidp + urandrange;

    bidp = (urand(bidPmin, bidPmax) / 100) + 1;
}

// ---------------------------------------------------------------------------
// addNewAuctions - mirror of cpp:2448; emits SellIntents instead of creating
// live auctions. The item selection (getRandomArray + random pick from the
// pool) and price math are identical; CreateItem/AddAuctionByGuid become a
// SellIntent the executor (Task 9) applies.
// ---------------------------------------------------------------------------

void BotBrain::addNewAuctions(SellerHouseConfig& cfg,
                              std::vector<EmittedIntent>& out)
{
    uint32 items;

    // Boost vs normal items-per-cycle (mirror).
    if (cfg.lastMissedItem >
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_BOOST))
    {
        items = m_config.getConfig(
            AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_BOOST);
    }
    else
    {
        items = m_config.getConfig(
            AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_NORMAL);
    }

    RandomArray randArray;
    std::vector<std::vector<uint32> > itemsAdded(
        AH_MAX_AUCTION_QUALITY, std::vector<uint32>(AH_MAX_ITEM_CLASS, 0));

    while (getRandomArray(cfg, randArray, itemsAdded) && (items > 0))
    {
        --items;

        // Random category from the missed-items table.
        uint32 pos = urand(0, randArray.size() - 1);
        const uint32 color = randArray[pos].color;
        const uint32 itemclass = randArray[pos].itemclass;

        // Random item from that [color][class] bucket.
        const ItemIdPool& bucket = m_pool.GetBucket(color, itemclass);
        uint32 itemID = bucket[urand(0, bucket.size() - 1)];
        ++itemsAdded[color][itemclass];

        if (!itemID)
        {
            continue;
        }

        // Look up the item's seller fields (replaces GetItemPrototype).
        ItemSellInfo sell;
        if (!m_pool.GetSellInfo(itemID, sell))
        {
            continue;
        }

        // Intentionally guards the source's urand(1,0) degenerate case:
        // the source calls urand(1, GetMaxStackSize()) raw, which is
        // urand(1,0) when stackable==0, producing undefined behaviour.
        // Clamping to 1 here is safe hardening — do NOT revert to raw.
        uint32 maxStack = sell.stackable ? sell.stackable : 1;
        uint32 stackCount = urand(1, maxStack);

        uint32 buyoutPrice;
        uint32 bidPrice = 0;

        if (m_config.getConfig(AHBOT_CONFIG_BOOL_BUYPRICE_SELLER))
        {
            buyoutPrice = sell.buyPrice * stackCount;
        }
        else
        {
            buyoutPrice = sell.sellPrice * stackCount;
        }

        SetPricesOfItem(cfg, buyoutPrice, bidPrice, stackCount, color);

        // Source: AddAuctionByGuid(..., urand(min,max) * HOUR, bid, buyout).
        // We emit durationHrs = urand(min,max); Task 9 multiplies by HOUR.
        // GetMinTime() clamps (>= 1, <= maxTime); maxTime is the raw config.
        uint32 durationHrs = urand(cfg.GetMinTime(), cfg.maxTime);

        EmittedIntent ei;
        ei.kind = EmittedIntent::KIND_SELL;
        ei.sell.uuid        = NextUuid();
        ei.sell.botGuid     = m_botGuid;
        ei.sell.house       = cfg.houseType;
        ei.sell.itemId      = itemID;
        ei.sell.stack       = stackCount;
        ei.sell.bid         = bidPrice;
        ei.sell.buyout      = buyoutPrice;
        ei.sell.durationHrs = durationHrs;
        out.push_back(ei);
    }
}

// ===========================================================================
// Buyer
// ===========================================================================

// ---------------------------------------------------------------------------
// IsBuyableEntry - PORTED VERBATIM (cpp:1130).
// ---------------------------------------------------------------------------

bool BotBrain::IsBuyableEntry(uint32 buyoutPrice, double InGame_BuyPrice,
                              double MaxBuyablePrice, uint32 MinBuyPrice,
                              uint32 MaxChance, uint32 ChanceRatio) const
{
    uint32 Chance = 0;

    if (buyoutPrice <= MinBuyPrice)
    {
        if (buyoutPrice <= MaxBuyablePrice)
        {
            Chance = MaxChance;
        }
        else
        {
            if ((buyoutPrice > 0) && (MaxBuyablePrice > 0))
            {
                double ratio = buyoutPrice / MaxBuyablePrice;
                if (ratio < 10)
                {
                    Chance = MaxChance - (ratio * (MaxChance / 10));
                }
                else
                {
                    Chance = 1;
                }
            }
        }
    }
    else if (buyoutPrice <= InGame_BuyPrice)
    {
        if (buyoutPrice <= MaxBuyablePrice)
        {
            Chance = MaxChance / 5;
        }
        else
        {
            if ((buyoutPrice > 0) && (MaxBuyablePrice > 0))
            {
                double ratio = buyoutPrice / MaxBuyablePrice;
                if (ratio < 10)
                {
                    Chance = (MaxChance / 5) - (ratio * (MaxChance / 50));
                }
                else
                {
                    Chance = 1;
                }
            }
        }
    }
    else if (buyoutPrice <= MaxBuyablePrice)
    {
        Chance = MaxChance / 10;
    }
    else
    {
        if ((buyoutPrice > 0) && (MaxBuyablePrice > 0))
        {
            double ratio = buyoutPrice / MaxBuyablePrice;
            if (ratio < 10)
            {
                Chance = (MaxChance / 5) - (ratio * (MaxChance / 50));
            }
            else
            {
                Chance = 0;
            }
        }
        else
        {
            Chance = 0;
        }
    }
    uint32 RandNum = urand(1, ChanceRatio);
    if (RandNum <= Chance)
    {
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// IsBidableEntry - PORTED VERBATIM (cpp:1223).
// ---------------------------------------------------------------------------

bool BotBrain::IsBidableEntry(uint32 bidPrice, double InGame_BuyPrice,
                              double MaxBidablePrice, uint32 MinBidPrice,
                              uint32 MaxChance, uint32 ChanceRatio) const
{
    uint32 Chance = 0;

    if (bidPrice <= MinBidPrice)
    {
        if ((InGame_BuyPrice != 0) &&
            (bidPrice < (InGame_BuyPrice - (InGame_BuyPrice / 30))))
        {
            Chance = MaxChance;
        }
        else
        {
            if (bidPrice < MaxBidablePrice)
            {
                double ratio = MaxBidablePrice / bidPrice;
                if (ratio < 3)
                {
                    Chance = ((MaxChance / 500) * ratio);
                }
                else
                {
                    Chance = (MaxChance / 500);
                }
            }
        }
    }
    else if (bidPrice < (InGame_BuyPrice - (InGame_BuyPrice / 30)))
    {
        Chance = (MaxChance / 10);
    }
    else
    {
        if (bidPrice < MaxBidablePrice)
        {
            double ratio = MaxBidablePrice / bidPrice;
            if (ratio < 4)
            {
                Chance = ((MaxChance / 1000) * ratio);
            }
            else
            {
                Chance = (MaxChance / 1000);
            }
        }
    }
    uint32 RandNum = urand(1, ChanceRatio);
    if (RandNum <= Chance)
    {
        return true;
    }
    else
    {
        return false;
    }
}

// ---------------------------------------------------------------------------
// BuyerUpdate - mirror of AuctionBotBuyer::Update().
// ---------------------------------------------------------------------------

bool BotBrain::BuyerUpdate(uint8 houseType, std::vector<EmittedIntent>& out)
{
    if (!m_buyerHouse[houseType].buyerEnabled)
    {
        return false;
    }
    addNewAuctionBuyerBotBid(m_buyerHouse[houseType], out);
    return true;
}

// ---------------------------------------------------------------------------
// addNewAuctionBuyerBotBid - REIMPLEMENTED against MarketSnapshot.
//
// Folds the source's GetBuyableEntry (cpp:1020) + PrepareListOfEntry
// (cpp:1100) + addNewAuctionBuyerBotBid (cpp:1306) into a single pass
// over the house's snapshot:
//
//   1. Build SameItemInfo (per-itemId average/min buy/bid prices) exactly as
//      GetBuyableEntry does, from EVERY record in the house.
//   2. Build the candidate set with the SAME predicates GetBuyableEntry uses
//      (skip bot-owned auctions unless a player bid on them; otherwise take
//      any auction that has a bid+bidder, or any with no bid at all).
//   3. PRUNE the cross-tick LastChecked map: remove any entry whose auctionId
//      is not present in the current snapshot (mirrors PrepareListOfEntry's
//      stale-CheckedEntry pruning, cpp:1100).
//   4. SKIP candidates whose LastChecked is within the configured recheck
//      interval (AHBOT_CONFIG_UINT32_BUYER_RECHECK_INTERVAL minutes, read
//      live so a GM reload takes effect immediately; mirrors cpp:1337).
//      Set LastChecked = now when a candidate IS evaluated.
//   5. Derive BuyCycles from the POST-skip (not raw) candidate count, matching
//      the source's BuyCycles derivation from config.CheckedEntry.size().
//   6. For each evaluated candidate, run the IDENTICAL decision logic from
//      addNewAuctionBuyerBotBid (price computation, IsBuyable/IsBidable,
//      bid-vs-buyout coin flip), emitting Bid/Buyout intents.
//
// The cross-tick LastChecked map lives in m_buyerLastChecked[houseType]
// (a BotBrain member) so it persists between ticks exactly as
// AHB_Buyer_Config::CheckedEntry does in the in-process bot.
//
// Wall-clock time(NULL) is used for 'now'.  The source uses server gametime,
// but gametime advances 1:1 with wall-clock; a 20-minute throttle is coarse
// enough that the difference is irrelevant.
// ---------------------------------------------------------------------------

void BotBrain::addNewAuctionBuyerBotBid(BuyerHouseConfig& cfg,
                                        std::vector<EmittedIntent>& out)
{
    const std::vector<AuctionRecord>& house =
        m_snapshot.GetHouse(cfg.houseType);

    // Wall-clock 'now' — see rationale above re: gametime equivalence.
    time_t now = time(NULL);

    // Convenience reference to the persistent per-house LastChecked map.
    std::map<uint32, time_t>& lastChecked =
        m_buyerLastChecked[cfg.houseType];

    // --- Step 1+2: build SameItemInfo + candidate list (GetBuyableEntry) ---
    struct BuyerItemInfo
    {
        BuyerItemInfo()
            : ItemCount(0), BuyPrice(0.0), BidPrice(0.0),
              MinBuyPrice(0), MinBidPrice(0) {}
        uint32 ItemCount;
        double BuyPrice;
        double BidPrice;
        uint32 MinBuyPrice;
        uint32 MinBidPrice;
    };
    std::map<uint32, BuyerItemInfo> sameItemInfo;

    // snapshotIds: the set of auctionIds present this tick (for pruning).
    std::map<uint32, bool> snapshotIds;

    // candidates: indices into 'house' that pass the selection predicates.
    std::vector<size_t> candidates;

    for (size_t r = 0; r < house.size(); ++r)
    {
        const AuctionRecord& rec = house[r];

        // Guard: itemCount is the divisor used throughout (item->GetCount()).
        if (rec.itemCount == 0)
        {
            continue;
        }

        snapshotIds[rec.id] = true;

        BuyerItemInfo& bi = sameItemInfo[rec.itemId];
        ++bi.ItemCount;
        bi.BuyPrice = bi.BuyPrice + (rec.buyout / rec.itemCount);
        bi.BidPrice = bi.BidPrice + (rec.startBid / rec.itemCount);
        if (rec.buyout != 0)
        {
            if (rec.buyout / rec.itemCount < bi.MinBuyPrice)
            {
                bi.MinBuyPrice = rec.buyout / rec.itemCount;
            }
            else if (bi.MinBuyPrice == 0)
            {
                bi.MinBuyPrice = rec.buyout / rec.itemCount;
            }
        }
        if (rec.startBid / rec.itemCount < bi.MinBidPrice)
        {
            bi.MinBidPrice = rec.startBid / rec.itemCount;
        }
        else if (bi.MinBidPrice == 0)
        {
            bi.MinBidPrice = rec.startBid / rec.itemCount;
        }

        // Candidate-selection predicates (verbatim from GetBuyableEntry).
        if (rec.ownerGuid == m_botGuid)
        {
            // Bot-owned: only a candidate if a player has bid on it.
            if ((rec.curBid != 0) && rec.bidderGuid)
            {
                candidates.push_back(r);
            }
        }
        else
        {
            if (rec.curBid != 0)
            {
                if (rec.bidderGuid)
                {
                    candidates.push_back(r);
                }
            }
            else
            {
                candidates.push_back(r);
            }
        }
    }

    // --- Step 3: prune stale LastChecked entries (PrepareListOfEntry) ---
    // Drop any auctionId that is no longer in the snapshot (sold/expired).
    // Mirrors: for (itr = CheckedEntry.begin(); ...) if (!LastExist) erase.
    for (std::map<uint32, time_t>::iterator it = lastChecked.begin();
         it != lastChecked.end(); )
    {
        if (snapshotIds.find(it->first) == snapshotIds.end())
        {
            it = lastChecked.erase(it);
        }
        else
        {
            ++it;
        }
    }

    if (candidates.empty())
    {
        return;
    }

    // --- Step 4: apply recheck throttle; collect post-skip candidates ---
    // Mirrors cpp:1337: skip if (LastChecked != 0) && (Now-LastChecked) <=
    // recheckSecs (config-driven; live read so GM reload takes effect).
    // BuyCycles is derived from the post-skip count.
    std::vector<size_t> eligible;
    eligible.reserve(candidates.size());
    for (size_t c = 0; c < candidates.size(); ++c)
    {
        const AuctionRecord& rec = house[candidates[c]];
        std::map<uint32, time_t>::iterator lc = lastChecked.find(rec.id);
        if (lc != lastChecked.end() && lc->second != 0)
        {
            // Read live from config (minutes) and convert to seconds, so a
            // GM reload of AuctionHouseBot.Buyer.Recheck.Interval takes
            // effect on the next buyer tick without a service restart.
            const time_t recheckSecs = static_cast<time_t>(
                m_config.getConfig(
                    AHBOT_CONFIG_UINT32_BUYER_RECHECK_INTERVAL)) * 60;
            if ((now - lc->second) <= recheckSecs)
            {
                // Still within the configured recheck window — skip.
                continue;
            }
        }
        eligible.push_back(candidates[c]);
    }

    if (eligible.empty())
    {
        return;
    }

    // --- BuyCycles cap derived from PRE-recheck candidate count ---
    // The boost-vs-normal decision mirrors the in-process source which bases it
    // on the full market/CheckedEntry count (the broader tracked-candidate
    // pool), NOT the per-tick post-throttle subset. Using the post-skip
    // eligible count causes under-buying in a busy market: most auctions are
    // recently-checked and throttled out this tick, so eligible is small and
    // the child incorrectly picks NORMAL. The decision must use candidates
    // (pre-recheck), while the actual work loop still iterates the
    // post-skip eligible set capped at buyCycles.
    uint32 buyCycles;
    if (candidates.size() >
        m_config.getConfig(AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_BOOST))
    {
        buyCycles = m_config.getConfig(
            AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_BOOST);
    }
    else
    {
        buyCycles = m_config.getConfig(
            AHBOT_CONFIG_UINT32_ITEMS_PER_CYCLE_NORMAL);
    }

    // --- Step 5: per-candidate decision (verbatim math) ---
    for (size_t c = 0; c < eligible.size(); ++c)
    {
        if (buyCycles == 0)
        {
            break;
        }

        const AuctionRecord& rec = house[eligible[c]];

        // Mark evaluated — mirrors: auctionEval.LastChecked = Now (cpp:1453).
        lastChecked[rec.id] = now;

        uint32 MaxChance = 5000;

        // BasePrice = (buyprice? BuyPrice : SellPrice) * count.
        uint32 BasePrice =
            m_config.getConfig(AHBOT_CONFIG_BOOL_BUYPRICE_BUYER)
                ? rec.vendorBuyPrice : rec.vendorSellPrice;
        BasePrice *= rec.itemCount;

        double MaxBuyablePrice = (BasePrice * cfg.buyerPriceRatio) / 100;

        uint32 buyoutPrice = rec.buyout / rec.itemCount;

        // bidPrice = the FINAL TOTAL bid to emit, bidPriceByItem = the
        // per-item value the IsBidable decision uses.
        //
        // WIRE CONTRACT: BidIntent.bidAmount is the new TOTAL bid, not the
        // increment. The executor (AuctionIntentExecutor::ApplyBid) rejects
        // anything below auction->bid + GetAuctionOutBid() and below startbid
        // as REASON_STALE_BID, mirroring the canonical player handler
        // (AuctionHouseHandler.cpp:480 "price < auction->bid +
        // auction->GetAuctionOutBid()"). The in-process buyer
        // (AuctionHouseBot.cpp:1371) hands the bare GetAuctionOutBid()
        // INCREMENT to PlaceBidToEntry -> UpdateBid, which works in-process
        // only because UpdateBid has no floor checks (it assigns bid = newbid
        // directly). Across the wire we must emit the total the executor's
        // floors expect, so add the increment to the current bid here.
        uint32 bidPrice;
        uint32 bidPriceByItem;
        if (rec.curBid >= rec.startBid)
        {
            // GetAuctionOutBid(): (bid / 100) * 5, min 1.
            uint32 outbid = (rec.curBid / 100) * 5;
            if (!outbid)
            {
                outbid = 1;
            }
            // FINAL TOTAL = current bid + minimum outbid increment. This
            // satisfies bidAmount >= auction->bid + GetAuctionOutBid() and
            // (since outbid >= 1) bidAmount > auction->bid, i.e. it actually
            // outbids a contested auction instead of being rejected as stale.
            bidPrice = rec.curBid + outbid;
            bidPriceByItem = rec.curBid / rec.itemCount;
        }
        else
        {
            // No prior bid (curBid < startBid, e.g. curBid == 0): startBid is
            // itself the final total. The executor accepts it because
            // minBid = bid + GetAuctionOutBid() = 0 + 1 = 1 <= startBid and
            // bidAmount == startBid satisfies the startbid floor.
            bidPrice = rec.startBid;
            bidPriceByItem = rec.startBid / rec.itemCount;
        }

        double InGame_BuyPrice;
        uint32 minBidPrice;
        uint32 minBuyPrice;
        std::map<uint32, BuyerItemInfo>::iterator si =
            sameItemInfo.find(rec.itemId);
        if (si == sameItemInfo.end())
        {
            InGame_BuyPrice = 0;
            minBidPrice = 0;
            minBuyPrice = 0;
        }
        else
        {
            const BuyerItemInfo& sb = si->second;
            if (sb.ItemCount == 1)
            {
                // If only one item exists it can be bought at a high price.
                MaxBuyablePrice = MaxBuyablePrice * 5;
            }
            InGame_BuyPrice = sb.BuyPrice / sb.ItemCount;
            minBidPrice = sb.MinBidPrice;
            minBuyPrice = sb.MinBuyPrice;
        }

        // Max Bidable price = 70% of max buyable price (- 1/30).
        double MaxBidablePrice = MaxBuyablePrice - (MaxBuyablePrice / 30);

        if (rec.ownerGuid == m_botGuid)
        {
            // Player placed a bid on a bot auction: 1/5 chance to react.
            MaxChance = MaxChance / 5;
        }

        bool doBid = false;
        bool doBuyout = false;

        if (rec.buyout != 0)   // directly buyable
        {
            if (IsBuyableEntry(buyoutPrice, InGame_BuyPrice, MaxBuyablePrice,
                               minBuyPrice, MaxChance, cfg.factionChance))
            {
                if (IsBidableEntry(bidPriceByItem, InGame_BuyPrice,
                                   MaxBidablePrice, minBidPrice,
                                   MaxChance / 2, cfg.factionChance))
                {
                    if (urand(0, 5) == 0)
                    {
                        doBid = true;
                    }
                    else
                    {
                        doBuyout = true;
                    }
                }
                else
                {
                    doBuyout = true;
                }
            }
            else
            {
                if (IsBidableEntry(bidPriceByItem, InGame_BuyPrice,
                                   MaxBidablePrice, minBidPrice,
                                   MaxChance / 2, cfg.factionChance))
                {
                    doBid = true;
                }
            }
        }
        else   // buyout == 0: only bids are possible
        {
            if (IsBidableEntry(bidPriceByItem, InGame_BuyPrice,
                               MaxBidablePrice, minBidPrice, MaxChance,
                               cfg.factionChance))
            {
                doBid = true;
            }
        }

        // D3: if the computed bid would meet or exceed a non-zero buyout, the
        // executor rejects it as REASON_BOUGHT_OUT and the auction is then
        // throttled as "checked" so it is never bid on or bought this run.
        // Mirror the in-process source: a bid >= buyout becomes a buyout.
        // Only promote when the buyout path is also viable (rec.buyout != 0
        // and the IsBuyable check passed, i.e. doBuyout would have been set
        // had the IsBuyable branch been entered). If only doBid is set (the
        // buyout check failed or the listing is bid-only) and the bid would
        // reach/exceed a non-zero buyout, skip this auction rather than emitting
        // a bid that the executor will reject; the buyout path or a later tick
        // will handle it when the price is more favourable.
        if (doBid && rec.buyout != 0 && bidPrice >= rec.buyout)
        {
            if (doBuyout)
            {
                // Bid was going to be the secondary choice (urand branch);
                // the amount would reach the buyout — just buyout directly.
                doBid = false;
                // doBuyout remains true; falls through to the doBuyout emit.
            }
            else
            {
                // Buyout value check did not pass; skip this auction entirely.
                doBid = false;
            }
        }

        if (doBuyout)
        {
            EmittedIntent ei;
            ei.kind = EmittedIntent::KIND_BUYOUT;
            ei.buyout.uuid      = NextUuid();
            ei.buyout.botGuid   = m_botGuid;
            ei.buyout.auctionId = rec.id;
            out.push_back(ei);
        }
        else if (doBid)
        {
            EmittedIntent ei;
            ei.kind = EmittedIntent::KIND_BID;
            ei.bid.uuid      = NextUuid();
            ei.bid.botGuid   = m_botGuid;
            ei.bid.auctionId = rec.id;
            ei.bid.bidAmount = bidPrice;
            out.push_back(ei);
        }

        --buyCycles;
    }
}
