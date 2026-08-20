#pragma once
#include "../Value.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class NearestGameObjects : public ObjectGuidListCalculatedValue
    {
        public:
            // The interval matters more here than anywhere else in the module. Gather
            // strategy pairs "no possible targets" with "add gathering loot", so a bot
            // with nothing hostile inside sightDistance -- the bot that is standing
            // still -- ran this full grid search every world tick, twenty times a
            // second. Five matches NearestUnitsValue and is still a quarter-second.
            NearestGameObjects(PlayerbotAI* ai, float range = sPlayerbotAIConfig.sightDistance) :
            ObjectGuidListCalculatedValue(ai, "nearest game objects", 5), range(range) {}

        protected:
            virtual list<ObjectGuid> Calculate();

        private:
            float range;
    };
}
