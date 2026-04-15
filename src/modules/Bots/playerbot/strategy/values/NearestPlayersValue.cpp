#include "botpch.h"
#include "../../playerbot.h"
#include "NearestPlayersValue.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"

using namespace ai;
using namespace MaNGOS;

void NearestPlayersValue::FindUnits(list<Unit*> &targets)
{
    AnyFriendlyUnitInObjectRangeCheck u_check(bot, range);
    UnitListSearcher<AnyFriendlyUnitInObjectRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(bot, searcher, range);
}

bool NearestPlayersValue::AcceptUnit(Unit* unit)
{
    return dynamic_cast<Player*>(unit) &&
           unit->GetObjectGuid() != bot->GetObjectGuid() &&
           unit->IsAlive();
}
