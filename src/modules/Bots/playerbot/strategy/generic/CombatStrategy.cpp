#include "botpch.h"
#include "../../playerbot.h"
#include "CombatStrategy.h"

using namespace ai;

void CombatStrategy::InitTriggers(list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "invalid target",
        NextAction::array(0, new NextAction("drop target", ACTION_HIGH + 9), NULL)));

    triggers.push_back(new TriggerNode(
            "weapons are saved",
        NextAction::array(0, new NextAction("restore weapons", ACTION_HIGH), NULL)));

    // Every combat build needs this, not just the melee ones it used to be copied into.
    // Nothing else turns a bot in combat: the module moves with MovePoint, whose
    // generator carries no facing, and never with the chase generator that would set it
    // every tick. So orientation after a move is the direction of travel, and a bot that
    // was ALREADY in range when the fight started never moves at all -- MoveTo returns
    // early inside contactDistance -- and therefore never turns. That is a priest standing
    // beside a nightsaber it never faces.
    //
    // It costs a caster its auto-attack, which is not free damage to throw away: melee
    // swings are refused outside a 120 degree arc and the refusal re-arms the swing timer
    // at 100ms, so an unfacing bot in melee range retries ten times a second forever.
    // Spells mostly do not care -- facing is per-spell through spell_facing -- so this is
    // about the weapon, and about not spinning on a failing check.
    //
    // The trigger's own cone is 60 degrees, stricter than the core's 120 for a swing and
    // 180 for a spell, so it corrects before either refuses. SetFacingTargetAction sets a
    // global-cooldown delay of its own, so it cannot spam.
    triggers.push_back(new TriggerNode(
            "not facing target",
        NextAction::array(0, new NextAction("set facing", ACTION_NORMAL + 7), NULL)));
}
