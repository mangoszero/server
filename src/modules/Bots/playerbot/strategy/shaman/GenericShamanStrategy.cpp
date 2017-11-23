#include "botpch.h"
#include "../../playerbot.h"
#include "ShamanMultipliers.h"
#include "HealShamanStrategy.h"

using namespace ai;

class GenericShamanStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
public:
    GenericShamanStrategyActionNodeFactory()
    {
        creators["flametongue weapon"] = &flametongue_weapon;
        creators["frostbrand weapon"] = &frostbrand_weapon;
        creators["windfury weapon"] = &windfury_weapon;
        creators["lesser healing wave"] = &lesser_healing_wave;
        creators["lesser healing wave on party"] = &lesser_healing_wave_on_party;
        creators["chain heal"] = &chain_heal;
        creators["riptide"] = &riptide;
        creators["chain heal on party"] = &chain_heal_on_party;
        creators["riptide on party"] = &riptide_on_party;
        creators["earth shock"] = &earth_shock;
    }
private:
    static ActionNode* earth_shock(PlayerbotAI* ai)
    {
        return new ActionNode ("earth shock",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("flame shock"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* flametongue_weapon(PlayerbotAI* ai)
    {
        return new ActionNode ("flametongue weapon",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("frostbrand weapon"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* frostbrand_weapon(PlayerbotAI* ai)
    {
        return new ActionNode ("frostbrand weapon",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("rockbiter weapon"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* windfury_weapon(PlayerbotAI* ai)
    {
        return new ActionNode ("windfury weapon",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("rockbiter weapon"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* lesser_healing_wave(PlayerbotAI* ai)
    {
        return new ActionNode ("lesser healing wave",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("healing wave"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* lesser_healing_wave_on_party(PlayerbotAI* ai)
    {
        return new ActionNode ("lesser healing wave on party",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("healing wave on party"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* chain_heal(PlayerbotAI* ai)
    {
        return new ActionNode ("chain heal",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("lesser healing wave"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* riptide(PlayerbotAI* ai)
    {
        return new ActionNode ("riptide",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("healing wave"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* chain_heal_on_party(PlayerbotAI* ai)
    {
        return new ActionNode ("chain heal on party",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("lesser healing wave on party"), nullptr),
            /*C*/ nullptr);
    }
    static ActionNode* riptide_on_party(PlayerbotAI* ai)
    {
        return new ActionNode ("riptide on party",
            /*P*/ nullptr,
            /*A*/ NextAction::array(0, new NextAction("healing wave on party"), nullptr),
            /*C*/ nullptr);
    }
};

GenericShamanStrategy::GenericShamanStrategy(PlayerbotAI* ai) : CombatStrategy(ai)
{
    actionNodeFactories.Add(new GenericShamanStrategyActionNodeFactory());
}

void GenericShamanStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    CombatStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "wind shear",
        NextAction::array(0, new NextAction("wind shear", 23.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "wind shear on enemy healer",
        NextAction::array(0, new NextAction("wind shear on enemy healer", 23.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "purge",
        NextAction::array(0, new NextAction("purge", 10.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member medium health",
        NextAction::array(0, new NextAction("lesser healing wave on party", 25.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "party member low health",
        NextAction::array(0, new NextAction("riptide on party", 25.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium aoe heal",
        NextAction::array(0, new NextAction("chain heal", 27.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "medium health",
        NextAction::array(0, new NextAction("lesser healing wave", 26.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "low health",
        NextAction::array(0, new NextAction("riptide", 26.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "heroism",
        NextAction::array(0, new NextAction("heroism", 31.0f), nullptr)));

    triggers.push_back(new TriggerNode(
        "bloodlust",
        NextAction::array(0, new NextAction("bloodlust", 30.0f), nullptr)));
}

void ShamanBuffDpsStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "lightning shield",
        NextAction::array(0, new NextAction("lightning shield", 22.0f), nullptr)));
}

void ShamanBuffManaStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "water shield",
        NextAction::array(0, new NextAction("water shield", 22.0f), nullptr)));
}
