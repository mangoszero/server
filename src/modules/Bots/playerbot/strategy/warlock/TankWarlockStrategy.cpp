#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "WarlockMultipliers.h"
#include "TankWarlockStrategy.h"

using namespace ai;

class GenericWarlockStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        GenericWarlockStrategyActionNodeFactory()
        {
            creators["summon voidwalker"] = &summon_voidwalker;
            creators["summon felguard"] = &summon_felguard;
        }
    private:
        static ActionNode* summon_voidwalker(PlayerbotAI* ai)
        {
            return new ActionNode ("summon voidwalker",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("drain soul"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* summon_felguard(PlayerbotAI* ai)
        {
            return new ActionNode ("summon felguard",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("summon voidwalker"), NULL),
                /*C*/ NULL);
        }
};

TankWarlockStrategy::TankWarlockStrategy(PlayerbotAI* ai) : GenericWarlockStrategy(ai)
{
    actionNodeFactories.Add(new GenericWarlockStrategyActionNodeFactory());
}

NextAction** TankWarlockStrategy::getDefaultActions()
{
    // "shoot" is wand-dependent and therefore not a floor at all -- a wandless warlock on this
    // spec had no guaranteed attack at any level. Shadow Bolt is the warlock's level 1
    // trainable nuke and needs no equipment; melee covers the case where even that cannot be
    // cast, which for a tanking pet-class means out of mana or with something already in its
    // face. Shoot stays on top so a warlock that has a wand still prefers it.
    return NextAction::array(0,
        new NextAction("shoot", 10.0f),
        new NextAction("shadow bolt", 9.0f),
        new NextAction("melee in range", 8.0f),
        NULL);
}

void TankWarlockStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericWarlockStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "no pet",
        NextAction::array(0, new NextAction("summon felguard", 50.0f), NULL)));
}
