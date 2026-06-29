#include "botpch.h"
#include "../../playerbot.h"
#include "PositionAction.h"
#include "../values/PositionValue.h"

using namespace ai;

bool PositionAction::Execute(Event event)
{
    string qualifier = event.getParam();
    if (qualifier.empty())
    {
        return false;
    }

    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    ai::Position& pos = context->GetValue<ai::Position&>("position", qualifier)->Get();
    pos.Set( master->GetPositionX(), master->GetPositionY(), master->GetPositionZ());

    ostringstream out; out << "Position " << qualifier << " is set";
    ai->TellMaster(out);
    return true;
}

bool MoveToPositionAction::Execute(Event event)
{
    ai::Position& pos = context->GetValue<ai::Position&>("position", qualifier)->Get();
    if (!pos.isSet())
    {
        ostringstream out; out << "Position " << qualifier << " is not set";
        ai->TellMaster(out);
        return false;
    }
    if (IsAggroPosition(pos.x, pos.y))
    {
        ai->TellMaster("Warning: that position is near hostile creatures.");
    }
    return MoveTo(bot->GetMapId(), pos.x, pos.y, pos.z, true);
}

bool GotoAction::Execute(Event event)
{
    if (!m_positionName.empty() && getMSTime() >= m_deadline)
    {
        m_positionName.clear();
        m_deadline = 0;
        return false;
    }

    string param = event.getParam();
    if (param.empty())
    {
        return false;
    }

    string posName = param;
    uint32 seconds = 5;
    size_t space = param.find(' ');
    if (space != string::npos)
    {
        posName = param.substr(0, space);
        string timeStr = param.substr(space + 1);
        if (!timeStr.empty())
        {
            seconds = max(1, atoi(timeStr.c_str()));
        }
    }

    if (posName == m_positionName)
    {
        return MoveTo(m_targetMapId, m_targetX, m_targetY, m_targetZ, true);
    }

    ai::Position& pos = context->GetValue<ai::Position&>("position", posName)->Get();
    if (!pos.isSet())
    {
        ostringstream out; out << "Position " << posName << " is not set";
        ai->TellMaster(out);
        return false;
    }

    m_positionName = posName;
    m_targetMapId = bot->GetMapId();
    m_targetX = (float)pos.x;
    m_targetY = (float)pos.y;
    m_targetZ = (float)pos.z;
    m_deadline = getMSTime() + seconds * 1000;

    return MoveTo(m_targetMapId, m_targetX, m_targetY, m_targetZ, true);
}

