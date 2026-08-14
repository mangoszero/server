#include "botpch.h"
#include "../../playerbot.h"
#include "AreaTriggerAction.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool ReachAreaTriggerAction::Execute(Event event)
{
    uint32 triggerId;
    WorldPacket p(event.getPacket());
    p.rpos(0);
    p >> triggerId;

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(triggerId);
    if (!atEntry)
    {
        return false;
    }

    AreaTrigger const* at = sObjectMgr.GetAreaTrigger(triggerId);
    if (at && at->condition && !sObjectMgr.IsPlayerMeetToCondition(at->condition, bot, bot->GetMap(), NULL, CONDITION_AREA_TRIGGER))
    {
        ai->TellMaster("I won't follow: I don't meet the conditions");
        return false;
    }

    WorldPacket p1(CMSG_AREATRIGGER);
    p1 << triggerId;
    p1.rpos(0);
    bot->GetSession()->HandleAreaTriggerOpcode(p1);
    return true;
}

bool AreaTriggerAction::Execute(Event event)
{
    LastMovement& movement = context->GetValue<LastMovement&>("last movement")->Get();

    uint32 triggerId = movement.lastAreaTrigger;
    movement.lastAreaTrigger = 0;

    AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(triggerId);
    if (!atEntry)
    {
        return false;
    }

    AreaTrigger const* at = sObjectMgr.GetAreaTrigger(triggerId);
    if (!at)
    {
        return true;
    }

    ai->ChangeStrategy("-follow master,+stay", BOT_STATE_NON_COMBAT);

    // StopMoving alone stopped the bot at its SERVER position, which UpdateSplineMovement
    // only refreshes every POSITION_UPDATE_DELAY, so the declared stop could be most of a
    // 400ms step behind where the client had it. StopMovement computes the real spline
    // position first, so both ends agree on where the bot came to rest.
    ai->StopMovement();

    WorldPacket p(CMSG_AREATRIGGER);
    p << triggerId;
    p.rpos(0);
    bot->GetSession()->HandleAreaTriggerOpcode(p);
    ai->TellMaster("Hello");
    return true;
}
