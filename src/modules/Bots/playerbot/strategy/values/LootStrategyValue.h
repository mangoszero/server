#pragma once
#include "../../LootObjectStack.h"
#include "../Value.h"

namespace ai
{
    /**
     * @brief Base class for loot strategies.
     */
    class LootStrategyBase
    {
    public:
        LootStrategyBase() {}
        virtual ~LootStrategyBase() {}

        /**
         * @brief Determines if an item can be looted.
         *
         * @param proto The item prototype.
         * @param context The AI object context.
         * @return true if the item can be looted, false otherwise.
         */
        virtual bool CanLoot(ItemPrototype const *proto, AiObjectContext *context) = 0;

        /**
         * @brief Gets the name of the loot strategy.
         *
         * @return string The name of the loot strategy.
         */
        virtual string GetName() = 0;
    };

    /**
     * @brief Class representing a value for loot strategy.
     * Inherits from ManualSetValue with a pointer to LootStrategyBase.
     */
    class LootStrategyValue : public ManualSetValue<LootStrategyBase*>
    {
    public:
        /**
         * @brief Construct a new Loot Strategy Value object
         *
         * @param ai Pointer to the PlayerbotAI object.
         */
        LootStrategyValue(PlayerbotAI* ai) : ManualSetValue<LootStrategyBase*>(ai, normal) {}

        /**
         * @brief Destroy the Loot Strategy Value object
         * Deletes the default value.
         */
        virtual ~LootStrategyValue() { delete defaultValue; }

        // Static members representing different loot strategies
        static LootStrategyBase *normal, *gray, *all, *disenchant;

        /**
         * @brief Get an instance of LootStrategyBase by name.
         *
         * @param name Name of the loot strategy.
         * @return LootStrategyBase* Pointer to the loot strategy instance.
         */
        static LootStrategyBase* instance(string name);
    };
}
