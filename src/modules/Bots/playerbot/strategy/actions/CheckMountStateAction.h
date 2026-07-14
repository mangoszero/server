#pragma once

#include "InventoryAction.h"
#include "MovementActions.h"
#include "../values/LastMovementValue.h"

namespace ai
{
    class CheckMountStateAction : public InventoryAction {
        public:
            CheckMountStateAction(PlayerbotAI* ai) : InventoryAction(ai, "check mount state") {}

            virtual bool Execute(Event event);

        private:
            bool Mount();
    };
}
