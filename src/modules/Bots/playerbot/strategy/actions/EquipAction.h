#pragma once

#include "../Action.h"
#include "InventoryAction.h"

namespace ai
{
    class EquipAction : public InventoryAction {
    public:
        EquipAction(PlayerbotAI* ai) : InventoryAction(ai, "equip") {}
        virtual bool Execute(Event event);

    private:
        void EquipItem(FindItemVisitor* visitor);
        void EquipItem(Item& item);
    };

}
