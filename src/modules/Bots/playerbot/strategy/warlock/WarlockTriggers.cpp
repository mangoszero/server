#include "botpch.h"
#include "../../playerbot.h"
#include "WarlockTriggers.h"
#include "WarlockActions.h"

using namespace ai;

bool DemonArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !ai->HasAura("demon skin", target) &&
        !ai->HasAura("demon armor", target) &&
        !ai->HasAura("fel armor", target);
}

bool SpellstoneTrigger::IsActive()
{
    Item* offItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
    if (offItem)
    {
        string name = offItem->GetProto()->Name1;
        strToLower(name);
        if (name.find("spellstone") != string::npos)
            return false;
    }
    return AI_VALUE2(uint8, "item count", getName()) > 0;
}

bool TargetHasImmolateTrigger::IsActive()
{
    Unit* target = GetTarget();
    return target && ai->HasAura("immolate", target);
}
