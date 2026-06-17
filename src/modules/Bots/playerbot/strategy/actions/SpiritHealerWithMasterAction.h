#pragma once

#include "../Action.h"

namespace ai
{
    class SpiritHealerWithMasterAction : public Action
    {
        public:
            SpiritHealerWithMasterAction(PlayerbotAI* ai) : Action(ai, "spirit healer with master") {}

        public:
            virtual bool Execute(Event event);
    };
}