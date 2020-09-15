#include "botpch.h"
#include "../../playerbot.h"
#include "StayActions.h"
#include "../values/LastMovementValue.h"
#include "MovementGenerator.h"

using namespace ai;

void StayActionBase::Stay()
{
    AI_VALUE(LastMovement&, "last movement").Set(NULL);

    MotionMaster &mm = *bot->GetMotionMaster();
    if (mm.GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE || bot->IsTaxiFlying())
    {
        return;
    }

    mm.Clear();
    mm.MoveIdle();
    bot->clearUnitState( UNIT_STAT_CHASE );
    bot->clearUnitState( UNIT_STAT_FOLLOW );

    if (!bot->IsStandState())
    {
        bot->SetStandState(UNIT_STAND_STATE_STAND);
    }
}

bool StayActionBase::StayLine(vector<Player*> line, float diff, float cx, float cy, float cz, float orientation, float range)
{
    if (line.size() < 5)
    {
        return StaySingleLine(line, diff, cx, cy, cz, orientation, range);
    }

    int lines = ceil((double)line.size() / 5.0);
    for (int i = 0; i < lines; i++)
    {
        float radius = range * i;
        float x = cx + cos(orientation) * radius;
        float y = cy + sin(orientation) * radius;
        vector<Player*> singleLine;
        for (int j = 0; j < 5 && !line.empty(); j++)
        {
            singleLine.push_back(line[line.size() - 1]);
            line.pop_back();
        }

        bool ok = StaySingleLine(singleLine, diff, x, y, cz, orientation, range);
        if (ok)
        {
            return true;
        }
    }

    return false;
}

bool StayActionBase::StaySingleLine(vector<Player*> line, float diff, float cx, float cy, float cz, float orientation, float range)
{
    Stay();

    float count = line.size();
    float angle = orientation - M_PI / 2.0f;
    float x = cx + cos(angle) * (range * floor(count / 2.0f) + diff);
    float y = cy + sin(angle) * (range * floor(count / 2.0f) + diff);

    int index = 0;
    for (vector<Player*>::iterator i = line.begin(); i != line.end(); i++)
    {
        Player* member = *i;

        if (member == bot)
        {
            float angle = orientation + M_PI / 2.0f;
            float radius = range * index;

            return MoveTo(bot->GetMapId(), x + cos(angle) * radius, y + sin(angle) * radius, cz);
        }

        index++;
    }

    return false;
}

bool StayAction::Execute(Event event)
{
    Stay();

    return true;
}

bool StayAction::isUseful()
{
    return !AI_VALUE2(bool, "moving", "self target");
}

bool StayCircleAction::Execute(Event event)
 {
Stay();

float range = 2.0f;

Unit* target = AI_VALUE(Unit*, "current target");
Player* master = GetMaster();
if (!target)
{
    target = master;
}

if (!target)
{
    return false;
}

switch (bot->getClass())
 {
case CLASS_HUNTER:
case CLASS_MAGE:
case CLASS_PRIEST:
case CLASS_WARLOCK:
range = sPlayerbotAIConfig.fleeDistance;
break;
case CLASS_DRUID:
if (!ai->IsTank(bot))
{
    range = sPlayerbotAIConfig.fleeDistance;
}
break;
case CLASS_SHAMAN:
if (ai->IsHeal(bot))
{
    range = sPlayerbotAIConfig.fleeDistance;
}
break;
}

float x = target->GetPositionX();
float y = target->GetPositionY();
float z = target->GetPositionZ();
float angle = GetFollowAngle();

return MoveTo(bot->GetMapId(), x + cos(angle) * range, y + sin(angle) * range, z);
}

bool StayLineAction::Execute(Event event)
 {
Group* group = bot->GetGroup();
if (!group)
{
    return false;
}

float range = 2.0f;

Player* master = GetMaster();
if (!master)
{
    return false;
}

float x = master->GetPositionX();
float y = master->GetPositionY();
float z = master->GetPositionZ();
float orientation = master->GetOrientation();

vector<Player*> players;
GroupReference *gref = group->GetFirstMember();
while (gref)
{
Player* member = gref->getSource();
if (member != master)
{
    players.push_back(member);
}

gref = gref->next();
}

players.insert(players.begin() + group->GetMembersCount() / 2, master);

return StayLine(players, 0.0f, x, y, z, orientation, range);
}

bool StayCombatAction::Execute(Event event)
 {
Group* group = bot->GetGroup();
if (!group)
{
    return false;
}

float range = 2.0f;

Player* master = GetMaster();
if (!master)
{
    return false;
}

float x = master->GetPositionX();
float y = master->GetPositionY();
float z = master->GetPositionZ();
float orientation = master->GetOrientation();

vector<Player*> tanks;
vector<Player*> dps;
GroupReference *gref = group->GetFirstMember();
while (gref)
{
Player* member = gref->getSource();
if (member != master)
 {
if (ai->IsTank(member))
{
    tanks.push_back(member);
}
else
{
    dps.push_back(member);
}
}

gref = gref->next();
}

if (ai->IsTank(master))
{
    tanks.insert(tanks.begin() + (tanks.size() + 1) / 2, master);
}
else
{
    dps.insert(dps.begin() + (dps.size() + 1) / 2, master);
}

switch (rand() % 50)
 {
case 5:
ai->TellMaster("Keep your eyes open!");
break;
case 15:
ai->TellMaster("Stay alert!");
break;
case 30:
ai->TellMaster("I hear something, keep order!");
break;
}

if (ai->IsTank(bot) && ai->IsTank(master))
 {
StayLine(tanks, 0.0f, x, y, z, orientation, range);
return true;
}
if (!ai->IsTank(bot) && !ai->IsTank(master))
 {
StayLine(dps, 0.0f, x, y, z, orientation, range);
return true;
}
if (ai->IsTank(bot) && !ai->IsTank(master))
{
float diff = tanks.size() % 2 == 0 ? -range / 2.0f : 0.0f;
StayLine(tanks, diff, x + cos(orientation) * range, y + sin(orientation) * range, z, orientation, range);
return true;
}
if (!ai->IsTank(bot) && ai->IsTank(master))
 {
float diff = dps.size() % 2 == 0 ? -range / 2.0f : 0.0f;
StayLine(dps, diff, x - cos(orientation) * range, y - sin(orientation) * range, z, orientation, range);
return true;
}
return true;
}
