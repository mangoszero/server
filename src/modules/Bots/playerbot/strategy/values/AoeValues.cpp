#include "botpch.h"
#include "../../playerbot.h"
#include "AoeValues.h"

#include "../../PlayerbotAIConfig.h"
using namespace ai;

list<ObjectGuid> FindMaxDensity(Player* bot)
{
    list<ObjectGuid> units = *bot->GetPlayerbotAI()->GetAiObjectContext()->GetValue<list<ObjectGuid> >("possible targets");
    map<ObjectGuid, list<ObjectGuid> > groups;
    int maxCount = 0;
    ObjectGuid maxGroup;
    for (list<ObjectGuid>::iterator i = units.begin(); i != units.end(); ++i)
    {
        Unit* unit = bot->GetPlayerbotAI()->GetUnit(*i);
        if (!unit)
        {
            continue;
        }

        for (list<ObjectGuid>::iterator j = units.begin(); j != units.end(); ++j)
        {
            Unit* other = bot->GetPlayerbotAI()->GetUnit(*j);
            if (!other)
            {
                continue;
            }

            float d = unit->GetDistance2d(other);
            if (d <= sPlayerbotAIConfig.aoeRadius * 2)
            {
                groups[*i].push_back(*j);
            }
        }

        if (maxCount < groups[*i].size())
        {
            maxCount = groups[*i].size();
            maxGroup = *i;
        }
    }

    if (!maxCount)
    {
        return list<ObjectGuid>();
    }

    return groups[maxGroup];
}

WorldLocation AoePositionValue::Calculate()
{
    list<ObjectGuid> group = FindMaxDensity(bot);
    if (group.empty())
    {
        return WorldLocation();
    }

    float x1 = FLT_MAX, y1 = FLT_MAX, x2 = -FLT_MAX, y2 = -FLT_MAX;
    for (list<ObjectGuid>::iterator i = group.begin(); i != group.end(); ++i)
    {
        Unit* unit = bot->GetPlayerbotAI()->GetUnit(*i);
        if (!unit)
        {
            continue;
        }

        if (unit->GetPositionX() < x1) x1 = unit->GetPositionX();
        if (unit->GetPositionX() > x2) x2 = unit->GetPositionX();
        if (unit->GetPositionY() < y1) y1 = unit->GetPositionY();
        if (unit->GetPositionY() > y2) y2 = unit->GetPositionY();
    }
    if (x1 == FLT_MAX)
    {
        return WorldLocation();
    }
    float x = (x1 + x2) / 2;
    float y = (y1 + y2) / 2;
    float z = bot->GetPositionZ();
    bot->UpdateGroundPositionZ(x, y, z);
    return WorldLocation(bot->GetMapId(), x, y, z, 0);
}

uint8 AoeCountValue::Calculate()
{
    return FindMaxDensity(bot).size();
}
