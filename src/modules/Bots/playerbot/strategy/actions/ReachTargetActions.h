#pragma once

#include "../Action.h"
#include "MovementActions.h"
#include "../../PlayerbotAIConfig.h"

namespace ai
{
    class ReachTargetAction : public MovementAction
    {
    public:
        ReachTargetAction(PlayerbotAI* ai, string name, float distance) : MovementAction(ai, name)
        {
            this->distance = distance;
        }
        virtual bool Execute(Event event)
        {
            return MoveTo(AI_VALUE(Unit*, "current target"), distance);
        }
        virtual bool isUseful()
        {
            return AI_VALUE2(float, "distance", "current target") > distance;
        }
        virtual string GetTargetName() { return "current target"; }

    protected:
        float distance;
    };

    class CastReachTargetSpellAction : public CastSpellAction
    {
    public:
        CastReachTargetSpellAction(PlayerbotAI* ai, string spell, float distance) : CastSpellAction(ai, spell)
        {
            this->distance = distance;
        }
        virtual bool isUseful()
        {
            return AI_VALUE2(float, "distance", "current target") > distance;
        }

    protected:
        float distance;
    };

    class ReachMeleeAction : public ReachTargetAction
    {
    public:
        ReachMeleeAction(PlayerbotAI* ai) : ReachTargetAction(ai, "reach melee", sPlayerbotAIConfig.meleeDistance) {}

        virtual bool isUseful()
        {
            return AI_VALUE2(float, "distance", "current target") > distance + sPlayerbotAIConfig.contactDistance + bot->GetObjectBoundingRadius();
        }
    };

    class BackOffAction : public ReachTargetAction
    {
    public:
        BackOffAction(PlayerbotAI* ai) : ReachTargetAction(ai, "back off", sPlayerbotAIConfig.meleeDistance) {}

        virtual bool isUseful()
        {
            return AI_VALUE2(float, "distance", "current target") < distance + sPlayerbotAIConfig.contactDistance + bot->GetObjectBoundingRadius();
        }
    };

    class ReachSpellAction : public ReachTargetAction
    {
    public:
        ReachSpellAction(PlayerbotAI* ai) : ReachTargetAction(ai, "reach spell", sPlayerbotAIConfig.spellDistance) {}
        virtual bool Execute(Event event)
        {
            distance = AI_VALUE(float, "reach spell distance");
            return ReachTargetAction::Execute(event);
        }
        virtual bool isUseful()
        {
            distance = AI_VALUE(float, "reach spell distance");
            return ReachTargetAction::isUseful();
        }
    };
}
