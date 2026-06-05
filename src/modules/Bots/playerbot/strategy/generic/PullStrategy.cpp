#include "botpch.h"
#include "../../playerbot.h"
#include "PullStrategy.h"
#include "../StrategyMultiplier.h"

using namespace ai;

class PullStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        PullStrategyActionNodeFactory()
        {
            creators["watch target approach"] = &watch_target_approach;
            creators["start pull"] = &start_pull;
            creators["ranged pull"] = &ranged_pull;
            creators["reach shoot range"] = &reach_shoot_range;
            creators["reach master"] = &reach_master;
        }

    private:
        static ActionNode* watch_target_approach(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("watch target approach",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node->persist(2000);
        }
        static ActionNode* start_pull(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("start pull",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node;
        }
        static ActionNode* ranged_pull(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("ranged pull",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node->persist(6000);
        }
        static ActionNode* reach_shoot_range(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("reach shoot range",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node->persist(3000);
        }
        static ActionNode* reach_master(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("reach master",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node->persist(3000);
        }
};

PullStrategy::PullStrategy(PlayerbotAI* ai, string action) : RangedCombatStrategy(ai)
{
    this->action = action;
    ai->GetAiObjectContext()->GetValue<float>("last target position")->Set(0);
    actionNodeFactories.Add(new PullStrategyActionNodeFactory());
}

NextAction** PullStrategy::getDefaultActions()
{
    return NextAction::array(0,
            new NextAction("start pull", 160.0f),
            new NextAction("reach shoot range", 150.0f),
            new NextAction(action, 140.0f),
            new NextAction("reach master", 130.0f),
            new NextAction("set facing", 120.0f),
            new NextAction("watch target approach", 110.0f),
            new NextAction("end pull", 100.0f), NULL);
}

void PullStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    RangedCombatStrategy::InitTriggers(triggers);
}

void PullStrategy::InitMultipliers(std::list<Multiplier*> &multipliers)
{
    StrategyMultiplier *multiplier = new StrategyMultiplier(ai, this);
    multiplier->AddAction("shoot bow");
    multiplier->AddAction("shoot crossbow");
    multiplier->AddAction("shoot gun");
    multiplier->AddAction("throw");
    multiplier->AddAction("shoot");
    multiplier->AddAction("auto shot");
    multiplier->AddAction("-pull");
    multiplier->AddAction("-wait for pull");
    multipliers.push_back(multiplier);
    RangedCombatStrategy::InitMultipliers(multipliers);
}

class WaitForPullStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        WaitForPullStrategyActionNodeFactory()
        {
            creators["watch group pull"] = &watch_group_pull;
            creators["end wait for pull"] = &end_wait_for_pull;
        }

    private:
        static ActionNode* watch_group_pull(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("watch group pull",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node->persist(2000);
        }
        static ActionNode* end_wait_for_pull(PlayerbotAI* ai)
        {
            ActionNode* node = new ActionNode ("end wait for pull",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
            return node;
        }
};

WaitForPullStrategy::WaitForPullStrategy(PlayerbotAI* ai) : CombatStrategy(ai)
{
    actionNodeFactories.Add(new WaitForPullStrategyActionNodeFactory());
}

NextAction** WaitForPullStrategy::getDefaultActions()
{
    return NextAction::array(0,
            new NextAction("watch group pull", 160.0f),
            new NextAction("end wait for pull", 150.0f), NULL);
}

void WaitForPullStrategy::InitMultipliers(std::list<Multiplier*> &multipliers)
{
    StrategyMultiplier *multiplier = new StrategyMultiplier(ai, this);
    multiplier->AddAction("-wait for pull");
    multipliers.push_back(multiplier);
    CombatStrategy::InitMultipliers(multipliers);
}
