#include "botpch.h"
#include "ObjectLookup.h"
#include "../../playerbot.h"
#include "CcReachTargetValue.h"

using namespace ai;

Unit* CcReachTargetValue::Get()
{
    if (selection.IsEmpty())
    {
        return NULL;
    }

    return ObjectLookup::GetUnit(*bot, selection);
}

void CcReachTargetValue::Set(Unit* unit)
{
    selection = unit ? unit->GetObjectGuid() : ObjectGuid();
}
