#include "botpch.h"
#include "playerbot.h"
#include "StayCircleStrategy.h"

using namespace ai;


NextAction** StayCircleStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("stay circle", 50.0f), NULL);
}