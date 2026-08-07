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
            // Deliberately left at the default checkInterval of 1. Raising it to 5 was a real
            // CPU saving on slow-changing inventory and an unacceptable trade on the rest:
            // these values hand out Item* and list<Item*>, so a half-second-stale entry can
            // name an item the bot has since used or destroyed, and GiveConjuredFoodAction
            // dereferences what it is given. It also made the bot conjure repeatedly, since
            // the count it consults to decide whether it needs food stayed zero for five
            // ticks after it had already made some. Caching counts while re-querying the
            // pointers at execute time would get the saving safely; the interval alone
            // cannot.
            ItemCountValue(PlayerbotAI* ai) : Uint8CalculatedValue(ai, "item count"), InventoryItemValueBase(ai) {}

        public:
            virtual uint8 Calculate();
    };

    class InventoryItemValue : public CalculatedValue<list<Item*> >, public Qualified, InventoryItemValueBase
    {
        public:
            InventoryItemValue(PlayerbotAI* ai) : CalculatedValue<list<Item*> >(ai, "inventory items"), InventoryItemValueBase(ai) {}

        public:
            virtual list<Item*> Calculate();
    };
}
