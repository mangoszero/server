#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "MageMultipliers.h"
#include "ArcaneMageStrategy.h"

using namespace ai;

class ArcaneMageStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        ArcaneMageStrategyActionNodeFactory()
        {
            creators["arcane blast"] = &arcane_blast;
            creators["arcane barrage"] = &arcane_barrage;
            creators["arcane missiles"] = &arcane_missiles;
        }

    private:
        static ActionNode* arcane_blast(PlayerbotAI* ai)
        {
            return new ActionNode ("arcane blast",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("arcane missiles"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* arcane_barrage(PlayerbotAI* ai)
        {
            return new ActionNode ("arcane barrage",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("arcane missiles"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* arcane_missiles(PlayerbotAI* ai)
        {
            return new ActionNode ("arcane missiles",
                /*P*/ NULL,
                // Fireball, not shoot -- see the frostbolt node in GenericMageStrategy. Arcane
                // Barrage does not exist in 1.12 and Arcane Missiles is level 8, so this chain
                // is the one a low-level arcane mage actually walks, and routing it through a
                // wand it does not own is what left it unable to attack at all.
                /*A*/ NextAction::array(0, new NextAction("fireball"), NULL),
                /*C*/ NULL);
        }
};

ArcaneMageStrategy::ArcaneMageStrategy(PlayerbotAI* ai) : GenericMageStrategy(ai)
{
    actionNodeFactories.Add(new ArcaneMageStrategyActionNodeFactory());
}

NextAction** ArcaneMageStrategy::getDefaultActions()
{
    // Arcane Barrage is a Wrath spell and Arcane Blast is TBC; neither exists in 1.12, so the
    // opener never fires here. Its fallback chain then ran arcane missiles (level 8) and shoot
    // (needs a wand), which leaves an arcane mage below level 8 with literally nothing it can
    // cast -- it stood in combat with "no actions executed" until something else killed its
    // target. Fire opens on fireball (level 1) and Frost on frostbolt (level 4); arcane was the
    // only spec with no low-level entry point. Fireball is trainable by every mage at level 1,
    // so it is the correct floor, and it sits below the arcane spells so a mage that CAN cast
    // them still prefers them.
    return NextAction::array(0,
        new NextAction("arcane barrage", 10.0f),
        new NextAction("arcane missiles", 9.0f),
        new NextAction("fireball", 8.0f),
        new NextAction("shoot", 7.0f),
        new NextAction("melee in range", 6.0f),
        NULL);
}

void ArcaneMageStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericMageStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "arcane blast",
        NextAction::array(0, new NextAction("arcane blast", 15.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "missile barrage",
        NextAction::array(0, new NextAction("arcane missiles", 15.0f), NULL)));
}

