#pragma once

#include "../Action.h"
#include "MovementActions.h"

namespace ai
{
    class StayActionBase : public MovementAction {
    public:
        StayActionBase(PlayerbotAI* ai, string name) : MovementAction(ai, name) {}

    protected:
        void Stay();
        bool StayLine(vector<Player*> line, float diff, float cx, float cy, float cz, float orientation, float range);
        bool StaySingleLine(vector<Player*> line, float diff, float cx, float cy, float cz, float orientation, float range);
    };

    class StayAction : public StayActionBase {
    public:
        StayAction(PlayerbotAI* ai) : StayActionBase(ai, "stay") {}
        virtual bool Execute(Event event);
        virtual bool isUseful();
    };

    class StayCircleAction : public StayActionBase {
    public:
        StayCircleAction(PlayerbotAI* ai) : StayActionBase(ai, "stay circle") {}
        virtual bool Execute(Event event);
    };

    class StayCombatAction : public StayActionBase {
    public:
        StayCombatAction(PlayerbotAI* ai) : StayActionBase(ai, "stay combat") {}
        virtual bool Execute(Event event);
    };

    class StayLineAction : public StayActionBase {
    public:
        StayLineAction(PlayerbotAI* ai) : StayActionBase(ai, "stay line") {}
        virtual bool Execute(Event event);
    };
}
