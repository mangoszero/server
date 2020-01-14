#pragma once
#include "../Trigger.h"
#include "../values/LastMovementValue.h"

namespace ai
{
    class WithinAreaTrigger : public Trigger {
    public:
        WithinAreaTrigger(PlayerbotAI* ai) : Trigger(ai, "within area trigger") {}

        virtual bool IsActive()
        {


            LastMovement& movement = context->GetValue<LastMovement&>("last movement")->Get();
            if (!movement.lastAreaTrigger)
            {
                return false;
            }

            AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(movement.lastAreaTrigger);
            if(!atEntry)
            {
                return false;
            }

            AreaTrigger const* at = sObjectMgr.GetAreaTrigger(movement.lastAreaTrigger);
            if (!at)
            {
                return false;
            }

            return IsPointInAreaTriggerZone(atEntry, bot->GetMapId(), bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), 0.5f);
        }
    };
}
