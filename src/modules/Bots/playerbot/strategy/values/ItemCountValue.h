#pragma once
#include "../Value.h"
#include "../ItemVisitors.h"
#include "../actions/InventoryAction.h"

namespace ai
{
    class InventoryItemValueBase : public InventoryAction
    {
        public:
            InventoryItemValueBase(PlayerbotAI* ai) : InventoryAction(ai, "empty") {}
            virtual bool Execute(Event event) { return false; }

        protected:
            list<Item*> Find(string qualifier);
    };

    class ItemCountValue : public Uint8CalculatedValue, public Qualified, InventoryItemValueBase
    {
        public:
            // Every read walked the whole of the bot's bags and equipment. Food, drink, soul
            // shard and reagent triggers all consult this, so at checkInterval 1 it was tens of
            // thousands of item-visitor passes a second across the fleet. Inventory does not
            // change inside a quarter of a second in any way a bot needs to react to.
            ItemCountValue(PlayerbotAI* ai) : Uint8CalculatedValue(ai, "item count", 5), InventoryItemValueBase(ai) {}

        public:
            virtual uint8 Calculate();
    };

    class InventoryItemValue : public CalculatedValue<list<Item*> >, public Qualified, InventoryItemValueBase
    {
        public:
            InventoryItemValue(PlayerbotAI* ai) : CalculatedValue<list<Item*> >(ai, "inventory items", 5), InventoryItemValueBase(ai) {}

        public:
            virtual list<Item*> Calculate();
    };
}
