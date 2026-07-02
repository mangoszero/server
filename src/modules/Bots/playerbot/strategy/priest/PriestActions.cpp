#include "botpch.h"
#include "../../playerbot.h"
#include "PriestActions.h"

using namespace ai;

NextAction** CastAbolishDiseaseAction::getAlternatives()
{
    return NextAction::merge(NextAction::array(0, new NextAction("cure disease"), NULL), CastSpellAction::getAlternatives());
}

NextAction** CastAbolishDiseaseOnPartyAction::getAlternatives()
{
    return NextAction::merge(NextAction::array(0, new NextAction("cure disease on party"), NULL), CastSpellAction::getAlternatives());
}

bool CastHolyNovaAction::isUseful()
{
    if (!CastSpellAction::isUseful() || ai->HasAura("shadowform", AI_VALUE(Unit*, "self target")))
    {
        return false;
    }
    if (!ai->HasStrategy("cautious"))
    {
        return true;
    }
    return !ai->HasNonCombatantInRange(10.0f);
}
