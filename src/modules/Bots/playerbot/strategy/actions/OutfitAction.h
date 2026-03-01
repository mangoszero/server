#pragma once

#include "../Action.h"
#include "../../LootObjectStack.h"
#include "EquipAction.h"
#include "InventoryAction.h"

namespace ai
{
    class OutfitAction : public EquipAction {
    public:
        OutfitAction(PlayerbotAI* ai) : EquipAction(ai, "outfit") {}
        virtual bool Execute(Event event);

    private:
        string parseName(string outfit);
        ItemIds parseItems(string &outfit);

        void List();
        ItemIds Find(string name);
        void Save(string name, ItemIds outfit);
        void Update(string name);
    };

}
