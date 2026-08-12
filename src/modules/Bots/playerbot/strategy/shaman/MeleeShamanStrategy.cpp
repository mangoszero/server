#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "ShamanMultipliers.h"
#include "MeleeShamanStrategy.h"

using namespace ai;

class MeleeShamanStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        MeleeShamanStrategyActionNodeFactory()
        {
            creators["stormstrike"] = &stormstrike;
            creators["magma totem"] = &magma_totem;
        }
    private:
        static ActionNode* stormstrike(PlayerbotAI* ai)
        {
            return new ActionNode ("stormstrike",
                /*P*/ NULL,
                // Earth Shock, not Lava Lash. Lava Lash is a Wrath ability with no action
                // registered for it, so this alternative was a silent NULL -- and since
                // Stormstrike is a level 40 thirty-one-point talent, that NULL was the
                // ENTIRE outcome for every enhancement shaman below 40 or without the
                // talent. Earth Shock is level 4 and registered.
                /*A*/ NextAction::array(0, new NextAction("earth shock"), NULL),
                /*C*/ NULL);
        }
        // The "lava lash" node is gone with its last reference. It wrapped an action name
        // that is not registered anywhere -- Lava Lash is a Wrath ability -- so anything
        // routed through it resolved to nothing. Leaving the node in place would keep that
        // trap available to the next person who saw the name and assumed it worked.
        static ActionNode* magma_totem(PlayerbotAI* ai)
        {
            return new ActionNode ("magma totem",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NextAction::array(0,
                    new NextAction("fire nova", ACTION_NORMAL), NULL));
        }
};

MeleeShamanStrategy::MeleeShamanStrategy(PlayerbotAI* ai) : GenericShamanStrategy(ai)
{
    actionNodeFactories.Add(new MeleeShamanStrategyActionNodeFactory());
}

NextAction** MeleeShamanStrategy::getDefaultActions()
{
    // Stormstrike alone was the whole rotation, and it is a level 40 thirty-one-point
    // talent -- so an enhancement shaman that did not have it had exactly one action, that
    // action always failed, and its alternative was an unregistered NULL. It stood in melee
    // doing nothing but white attacks for forty levels. Earth Shock (level 4) is the real
    // enhancement filler in 1.12, with melee underneath it so there is always something.
    return NextAction::array(0,
        new NextAction("stormstrike", 10.0f),
        new NextAction("earth shock", 9.0f),
        new NextAction("melee", 8.0f),
        NULL);
}

void MeleeShamanStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    GenericShamanStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "shaman weapon",
        NextAction::array(0, new NextAction("windfury weapon", 22.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "searing totem",
        NextAction::array(0, new NextAction("searing totem", 22.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "shock",
        NextAction::array(0, new NextAction("earth shock", 20.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "enemy too close for melee",
        NextAction::array(0, new NextAction("move out of enemy contact", ACTION_NORMAL + 8), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("strength of earth totem", ACTION_LIGHT_HEAL), NULL)));
}

void MeleeAoeShamanStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "enemy out of melee",
        NextAction::array(0, new NextAction("reach melee", ACTION_NORMAL + 8), NULL)));

    triggers.push_back(new TriggerNode(
            "magma totem",
        NextAction::array(0, new NextAction("magma totem", 26.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "medium aoe",
        NextAction::array(0, new NextAction("fire nova", 25.0f), NULL)));
}
