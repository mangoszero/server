#include "botpch.h"
#include "playerbot.h"
#include "FollowLineStrategy.h"

using namespace ai;

NextAction** FollowLineStrategy::getDefaultActions()
{
    return NextAction::array(0, new NextAction("follow line", 1.0f), NULL);
}