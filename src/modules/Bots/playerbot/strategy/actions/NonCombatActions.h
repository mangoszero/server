#pragma once

#include "../Action.h"
#include "UseItemAction.h"
#include "../../PlayerbotAIConfig.h"
#include "../ItemVisitors.h"

#include <algorithm>

namespace ai
{
    class DrinkAction : public UseItemAction
    {
        public:
            DrinkAction(PlayerbotAI* ai) : UseItemAction(ai, "drink") {}

            virtual bool Execute(Event event);
            virtual bool isPossible();
            virtual bool isUseful();
    };

    class EatAction : public UseItemAction
    {
        public:
            EatAction(PlayerbotAI* ai) : UseItemAction(ai, "food") {}

            virtual bool Execute(Event event);
            virtual bool isPossible();
            virtual bool isUseful();
    };
}
