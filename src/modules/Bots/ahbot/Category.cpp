#include "Category.h"
#include "ItemBag.h"
#include "AhBotConfig.h"
#include "PricingStrategy.h"

using namespace ahbot;

/**
 * @brief Get the stack count for an item based on its quality.
 *
 * @param proto The item prototype.
 * @return uint32 The stack count.
 */
uint32 Category::GetStackCount(ItemPrototype const* proto)
{
    if (proto->Quality > ITEM_QUALITY_UNCOMMON)
    {
        return 1;
    }

    return urand(1, proto->GetMaxStackSize());
}

/**
 * @brief Get the maximum allowed auction count for a specific item.
 *
 * @param proto The item prototype.
 * @return uint32 The maximum allowed auction count.
 */
uint32 Category::GetMaxAllowedItemAuctionCount(ItemPrototype const* proto)
{
    return 0;
}

/**
 * @brief Get the maximum allowed auction count for the category.
 *
 * @return uint32 The maximum allowed auction count.
 */
uint32 Category::GetMaxAllowedAuctionCount()
{
    return sAhBotConfig.GetMaxAllowedAuctionCount(GetName());
}

/**
 * @brief Get the pricing strategy for the category.
 *
 * @return PricingStrategy* The pricing strategy.
 */
PricingStrategy* Category::GetPricingStrategy()
{
    if (pricingStrategy)
    {
        return pricingStrategy;
    }

    ostringstream out; out << "AhBot.PricingStrategy." << GetName();
    string name = sAhBotConfig.GetStringDefault(out.str().c_str(), "default");
    return pricingStrategy = PricingStrategyFactory::Create(name, this);
}

/**
 * @brief Construct a new QualityCategoryWrapper object.
 *
 * @param category The base category.
 * @param quality The item quality.
 */
QualityCategoryWrapper::QualityCategoryWrapper(Category* category, uint32 quality) : Category(), quality(quality), category(category)
{
    ostringstream out; out << category->GetName() << ".";
    switch (quality)
    {
    case ITEM_QUALITY_POOR:
        out << "gray";
        break;
    case ITEM_QUALITY_NORMAL:
        out << "white";
        break;
    case ITEM_QUALITY_UNCOMMON:
        out << "green";
        break;
    case ITEM_QUALITY_RARE:
        out << "blue";
        break;
    default:
        out << "epic";
        break;
    }

    combinedName = out.str();
}

/**
 * @brief Check if the category contains the specified item.
 *
 * @param proto The item prototype.
 * @return true If the category contains the item.
 * @return false Otherwise.
 */
bool QualityCategoryWrapper::Contains(ItemPrototype const* proto)
{
    return proto->Quality == quality && category->Contains(proto);
}

/**
 * @brief Get the maximum allowed auction count for the quality category.
 *
 * @return uint32 The maximum allowed auction count.
 */
uint32 QualityCategoryWrapper::GetMaxAllowedAuctionCount()
{
    uint32 count = sAhBotConfig.GetMaxAllowedAuctionCount(combinedName);
    return count > 0 ? count : category->GetMaxAllowedAuctionCount();
}

/**
 * @brief Get the maximum allowed auction count for a specific item in the quality category.
 *
 * @param proto The item prototype.
 * @return uint32 The maximum allowed auction count.
 */
uint32 QualityCategoryWrapper::GetMaxAllowedItemAuctionCount(ItemPrototype const* proto)
{
    return category->GetMaxAllowedItemAuctionCount(proto);
}
