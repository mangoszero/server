#pragma once

#include "../Strategy.h"
#include "../NamedObjectContext.h"

namespace ai
{
    class RogueSapStrategy : public Strategy
    {
    public:
        RogueSapStrategy(PlayerbotAI* ai) : Strategy(ai) {}

        virtual string getName() { return "sap"; }

        virtual NextAction** getDefaultActions()
        {
            return NextAction::array(0,
                new NextAction("stealth", ACTION_HIGH + 3),
                new NextAction("reach melee", ACTION_HIGH + 2),
                new NextAction("sap", ACTION_HIGH + 1),
                new NextAction("end sap", ACTION_NORMAL + 1),
                new NextAction("follow master", ACTION_NORMAL),
                NULL);
        }
    };
}
