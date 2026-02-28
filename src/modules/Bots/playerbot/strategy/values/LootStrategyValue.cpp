#include "botpch.h"
#include "../../playerbot.h"
#include "LootStrategyValue.h"
#include "../values/ItemUsageValue.h"

using namespace ai;
using namespace std;

namespace ai
{
    /**
     * @brief Normal loot strategy class.
     * Inherits from LootStrategyBase and defines the normal loot behavior.
     */
    class NormalLootStrategy : public LootStrategyBase
    {
    public:
        /**
         * @brief Determines if an item can be looted based on normal strategy.
         *
         * @param proto The item prototype.
         * @param context The AI object context.
         * @return true if the item can be looted, false otherwise.
         */
        virtual bool CanLoot(ItemPrototype const *proto, AiObjectContext *context)
        {
            ostringstream out; out << proto->ItemId;
            ItemUsage usage = AI_VALUE2(ItemUsage, "item usage", out.str());
            return usage != ITEM_USAGE_NONE && proto->Bonding != BIND_WHEN_PICKED_UP;
        }

        /**
         * @brief Gets the name of the loot strategy.
         *
         * @return string The name of the loot strategy.
         */
        virtual string GetName() { return "normal"; }
    };

    /**
     * @brief Gray loot strategy class.
     * Inherits from NormalLootStrategy and defines the gray loot behavior.
     */
    class GrayLootStrategy : public NormalLootStrategy
    {
    public:
        /**
         * @brief Determines if an item can be looted based on gray strategy.
         *
         * @param proto The item prototype.
         * @param context The AI object context.
         * @return true if the item can be looted, false otherwise.
         */
        virtual bool CanLoot(ItemPrototype const *proto, AiObjectContext *context)
        {
            return NormalLootStrategy::CanLoot(proto, context) || proto->Quality == ITEM_QUALITY_POOR;
        }

        /**
         * @brief Gets the name of the loot strategy.
         *
         * @return string The name of the loot strategy.
         */
        virtual string GetName() { return "gray"; }
    };

    /**
     * @brief Disenchant loot strategy class.
     * Inherits from NormalLootStrategy and defines the disenchant loot behavior.
     */
    class DisenchantLootStrategy : public NormalLootStrategy
    {
    public:
        /**
         * @brief Determines if an item can be looted based on disenchant strategy.
         *
         * @param proto The item prototype.
         * @param context The AI object context.
         * @return true if the item can be looted, false otherwise.
         */
        virtual bool CanLoot(ItemPrototype const *proto, AiObjectContext *context)
        {
            return NormalLootStrategy::CanLoot(proto, context) ||
                    (proto->Quality >= ITEM_QUALITY_UNCOMMON && proto->Bonding != BIND_WHEN_PICKED_UP &&
                    (proto->Class == ITEM_CLASS_ARMOR || proto->Class == ITEM_CLASS_WEAPON));
        }

        /**
         * @brief Gets the name of the loot strategy.
         *
         * @return string The name of the loot strategy.
         */
        virtual string GetName() { return "disenchant"; }
    };

    /**
     * @brief All loot strategy class.
     * Inherits from LootStrategyBase and defines the all loot behavior.
     */
    class AllLootStrategy : public LootStrategyBase
    {
    public:
        /**
         * @brief Determines if an item can be looted based on all strategy.
         *
         * @param proto The item prototype.
         * @param context The AI object context.
         * @return true if the item can be looted, false otherwise.
         */
        virtual bool CanLoot(ItemPrototype const *proto, AiObjectContext *context)
        {
            return true;
        }

        /**
         * @brief Gets the name of the loot strategy.
         *
         * @return string The name of the loot strategy.
         */
        virtual string GetName() { return "all"; }
    };
} // namespace ai

// Initialize static members representing different loot strategies
ai::LootStrategyBase *ai::LootStrategyValue::normal = new ai::NormalLootStrategy();
ai::LootStrategyBase *ai::LootStrategyValue::gray = new ai::GrayLootStrategy();
ai::LootStrategyBase *ai::LootStrategyValue::disenchant = new ai::DisenchantLootStrategy();
ai::LootStrategyBase *ai::LootStrategyValue::all = new ai::AllLootStrategy();

/**
 * @brief Get an instance of LootStrategyBase by name.
 *
 * @param name Name of the loot strategy.
 * @return LootStrategyBase* Pointer to the loot strategy instance.
 */
ai::LootStrategyBase* ai::LootStrategyValue::instance(string name)
{
    if (name == "*" || name == "all")
    {
        return all;
    }

    if (name == "g" || name == "gray")
    {
        return gray;
    }

    if (name == "d" || name == "e" || name == "disenchant" || name == "enchant")
    {
        return disenchant;
    }

    return normal;
}

