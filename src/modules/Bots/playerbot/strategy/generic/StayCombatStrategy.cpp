#include "botpch.h"
#include "playerbot.h"
#include "StayCombatStrategy.h"

using namespace ai;

NextAction** StayCombatStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("stay combat", 1.0f), NULL);
}