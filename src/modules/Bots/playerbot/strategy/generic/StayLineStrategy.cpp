#include "botpch.h"
#include "playerbot.h"
#include "StayLineStrategy.h"

using namespace ai;

NextAction** StayLineStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("stay line", 50.0f), NULL);
}