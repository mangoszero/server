#include "botpch.h"
#include "../../playerbot.h"
#include "CurrentTargetValue.h"

using namespace ai;

Unit* CurrentTargetValue::Get()
{
    

    if (selection.IsEmpty())
        return nullptr;

    Unit* unit = sObjectAccessor.GetUnit(*bot, selection);
    if (unit && !bot->IsWithinLOSInMap(unit))
        return nullptr;

    return unit;
}

void CurrentTargetValue::Set(Unit* target)
{
    selection = target ? target->GetObjectGuid() : ObjectGuid(); 
}
