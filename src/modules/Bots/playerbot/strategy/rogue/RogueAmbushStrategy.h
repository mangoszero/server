#pragma once

#include "../Strategy.h"

namespace ai
{
    class RogueAmbushStrategy : public Strategy
    {
    public:
        RogueAmbushStrategy(PlayerbotAI* ai) : Strategy(ai) {}
        virtual string getName() { return "ambush"; }

        virtual NextAction** getDefaultActions()
        {
            // Stealth first; once stealthed, cheap shot moves to target (via reach melee
            // prerequisite) and opens. End ambush fires once combat begins.
            return NextAction::array(0,
                new NextAction("stealth", 105.0f),
                new NextAction("reach melee", 104.5f),
                new NextAction("cheap shot", 104.0f),
                new NextAction("end ambush", 103.0f),
                new NextAction("sinister strike", 100.0f),
                NULL);
        }
    };
}
