#include "botpch.h"
#include "../../playerbot.h"
#include "CcReachTargetValue.h"

using namespace ai;

Unit* CcReachTargetValue::Get()
{
    if (selection.IsEmpty())
    {
        return NULL;
    }

    return sObjectAccessor.GetUnit(*bot, selection);
}

void CcReachTargetValue::Set(Unit* unit)
{
    selection = unit ? unit->GetObjectGuid() : ObjectGuid();
}
