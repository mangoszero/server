#include "botpch.h"
#include "../../playerbot.h"
#include "FollowActions.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool FollowLineAction::Execute(Event event)
{
    return Follow(AI_VALUE(Unit*, "line target"), sPlayerbotAIConfig.followDistance, 0.0f);
}

bool FollowMasterAction::Execute(Event event)
{
    return Follow(AI_VALUE(Unit*, "master target"));
}

bool FollowMasterAction::isUseful()
{
    return (AI_VALUE2(float, "distance", "master target") > sPlayerbotAIConfig.followDistance && !AI_VALUE(bool, "can loot")) ||
        transportBoardingDelayTime > 0;
}

bool ReachMasterAction::Execute(Event event)
{
    if (!FollowMasterAction::Execute(event))
    {
        return false;
    }
    return isUseful();
}
bool ReachMasterAction::isUseful()
{
    return (AI_VALUE2(float, "distance", "master target") - 2.0 > sPlayerbotAIConfig.followDistance &&
            AI_VALUE2(float, "distance", "master target") < sPlayerbotAIConfig.reactDistance) ||
                transportBoardingDelayTime > 0;
}

bool FollowMasterRandomAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    float range = rand() % 10 + 2;
    float angle = GetFollowAngle();
    float x = master->GetPositionX() + cos(angle) * range;
    float y = master->GetPositionY() + sin(angle) * range;
    float z = master->GetPositionZ();

    return MoveTo(master->GetMapId(), x, y, z);
}
