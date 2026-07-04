#pragma once

#include "../Action.h"

namespace ai
{
    class RestoreWeaponsAction : public Action
    {
        public:
            RestoreWeaponsAction(PlayerbotAI* ai) : Action(ai, "restore weapons") {}
            virtual bool Execute(Event event);
            virtual bool isUseful();

        private:
            bool EquipFromBag(Item* pItem);
    };
}
