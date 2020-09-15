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
