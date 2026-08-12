#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "GenericHunterStrategy.h"
#include "HunterAiObjectContext.h"

using namespace ai;

class GenericHunterStrategyActionNodeFactory : public NamedObjectFactory<ActionNode>
{
    public:
        GenericHunterStrategyActionNodeFactory()
        {
            creators["rapid fire"] = &rapid_fire;
            creators["boost"] = &rapid_fire;
            creators["aspect of the pack"] = &aspect_of_the_pack;
            creators["feign death"] = &feign_death;
            creators["wing clip"] = &wing_clip;
        }
    private:
        static ActionNode* rapid_fire(PlayerbotAI* ai)
        {
            return new ActionNode ("rapid fire",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("readiness"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* aspect_of_the_pack(PlayerbotAI* ai)
        {
            return new ActionNode ("aspect of the pack",
                /*P*/ NULL,
                /*A*/ NextAction::array(0, new NextAction("aspect of the cheetah"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* feign_death(PlayerbotAI* ai)
        {
            return new ActionNode ("feign death",
                /*P*/ NULL,
                // Feign Death is level 30, and until then this node's only alternative was
                // "flee" -- so every threat spike on a hunter below 30 became running away
                // rather than doing something about it. Shed threat the way a young hunter
                // actually can: slow the thing down, or hit it, before resorting to flight.
                /*A*/ NextAction::array(0,
                    new NextAction("concussive shot"),
                    new NextAction("hunter melee"),
                    new NextAction("flee"), NULL),
                /*C*/ NULL);
        }
        static ActionNode* wing_clip(PlayerbotAI* ai)
        {
            return new ActionNode ("wing clip",
                /*P*/ NULL,
                /*A*/ NULL,
                /*C*/ NULL);
        }
};

GenericHunterStrategy::GenericHunterStrategy(PlayerbotAI* ai) : RangedCombatStrategy(ai)
{
    actionNodeFactories.Add(new GenericHunterStrategyActionNodeFactory());
}

void GenericHunterStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    RangedCombatStrategy::InitTriggers(triggers);

    // Concussive Shot and Scatter Shot are both registered actions that nothing ever
    // triggered. They matter because this cascade fires at fifteen yards while everything
    // in it that could act needed five or fewer -- wing clip, mongoose bite, disengage and
    // the melee fallback -- so the whole eight-to-fifteen yard band had no answer at all.
    // A snare at range is what a hunter actually does there: slow the thing down, then use
    // the reposition below to open the distance again. Concussive Shot is level 8 and
    // available to every hunter; Scatter Shot is a Marksmanship talent, so it sits behind
    // it and simply fails to cast for a hunter that has not taken it.
    //
    // Melee now sits ABOVE the reposition, and that ordering is the point.
    //
    // Everything above the reposition is level- or talent-gated -- Intimidation is a
    // Beast Mastery talent, Scatter Shot is Marksmanship, Concussive Shot is level 8,
    // Wing Clip level 12 -- so a young hunter reached "hunter ensure ranged position" at
    // 50.0 as the first thing it could actually do, every single tick. Since backing away
    // never stops being possible, it outranked "hunter melee" at 48.5 forever: the hunter
    // shot once, was closed on, and then ran for the rest of the fight without firing.
    // Observed live on a petless hunter that never attacked again after its opener.
    //
    // Putting melee above the reposition costs nothing when the ranged options work,
    // because they all outrank both, and an out-of-range melee action declines on its own
    // in the five-to-eight yard band. What it buys is a hunter that hits back.
    //
    // This ordering is only safe because HunterMeleeAction::Execute reports whether it
    // actually started the attack. It used to return true unconditionally, and a successful
    // action ends the engine tick -- so melee sitting above the reposition would simply have
    // reversed the starvation, leaving the hunter white-swinging forever and never opening
    // range again. It now succeeds once, on the engage, and declines while auto-attack runs.
    // Do not raise anything else above the reposition without checking it can decline.
    triggers.push_back(new TriggerNode(
            "enemy too close for spell",
        NextAction::array(0,
        new NextAction("intimidation", 52.0f),
        new NextAction("concussive shot", 51.6f),
        new NextAction("scatter shot", 51.3f),
        new NextAction("wing clip", 51.0f),
        new NextAction("mongoose bite", 50.5f),
        new NextAction("hunter melee", 50.1f),
        new NextAction("hunter ensure ranged position", 50.0f),
        new NextAction("disengage", 49.0f),
        new NextAction("flee", 48.0f),
        NULL)));

    triggers.push_back(new TriggerNode(
            "medium threat",
        NextAction::array(0, new NextAction("feign death", 52.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "has feign death",
        NextAction::array(0, new NextAction("remove feign death", 53.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "hunters pet low health",
        NextAction::array(0, new NextAction("mend pet", 60.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "rapid fire",
        NextAction::array(0, new NextAction("rapid fire", 55.0f), NULL)));

    triggers.push_back(new TriggerNode(
            "bestial wrath",
        NextAction::array(0, new NextAction("bestial wrath", 55.0f), NULL)));
}
