#include "botpch.h"
#include "../../playerbot.h"
#include "PullActions.h"
#include "ReachTargetActions.h"

using namespace ai;

bool WatchTargetApproachAction::Execute(Event event)
{
    bot->StopMoving();
    return isUseful();
}

void WatchTargetApproachAction::Reset()
{
    context->GetValue<float>("last target position")->Set(0);
}

bool WatchTargetApproachAction::isUseful()
{
    Unit* target = GetTarget();
    if (target == bot)
    {
        target = context->GetValue<Unit*>("old target")->Get();
        context->GetValue<Unit*>("current target")->Set(target);
    }
    if (!target || !target->IsAlive())
    {
        Reset();
        return false;
    }
    float lastDistance = context->GetValue<float>("last target position")->Get();
    if (lastDistance == 0)
    {
        lastDistance = sPlayerbotAIConfig.sightDistance + sPlayerbotAIConfig.meleeDistance;
        context->GetValue<float>("last target position")->Set(lastDistance);
    }

    float currentDistance = bot->GetDistance(target);
    if (currentDistance <= sPlayerbotAIConfig.meleeDistance ||
      currentDistance > sPlayerbotAIConfig.sightDistance + sPlayerbotAIConfig.meleeDistance)
    {
        Reset();
        return false;
    }

    if (currentDistance < lastDistance)
    {
        context->GetValue<float>("last target position")->Set(currentDistance);
        SetPersistenceStartTime(getMSTime());
    }
    return true;
}

bool StartPullAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || !bot->GetGroup())
    {
        return false;
    }
    if (!ai->HasStrategy("pull", BOT_STATE_COMBAT))
    {
        ai->ChangeStrategy("+pull", BOT_STATE_COMBAT);
    }
    Group *group = bot->GetGroup();
    if (group)
    {
        for (GroupReference *gref = group->GetFirstMember(); gref; gref = gref->next())
        {
            Player* member = gref->getSource();
            if (!member)
            {
                continue;
            }
            float currentDistance = bot->GetDistance(member);
            if (member != bot &&
                 member->GetPlayerbotAI() &&
                !member->GetPlayerbotAI()->HasStrategy("pull", BOT_STATE_COMBAT) &&
                !member->GetPlayerbotAI()->HasStrategy("wait for pull", BOT_STATE_COMBAT) &&
                currentDistance < sPlayerbotAIConfig.reactDistance)
                member->GetPlayerbotAI()->ChangeStrategy("+wait for pull",BOT_STATE_COMBAT);
        }
    }
    return true;
}

bool WatchGroupPullAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
    {
        return false;
    }
    Player *tank = target->ToPlayer();
    if (!tank || !tank->GetPlayerbotAI())
    {
        return false;
    }
    bot->StopMoving();
    SetPersistenceStartTime(getMSTime());
    return isUseful();
}

bool WatchGroupPullAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive())
    {
        return false;
    }
    Player *tank = target->ToPlayer();
    if (!tank || !tank->GetPlayerbotAI())
    {
        return false;
    }
    float distance = bot->GetDistance(tank);
    if (distance > sPlayerbotAIConfig.reactDistance)
    {
        return false;
    }

    if (!tank->GetPlayerbotAI()->HasStrategy("pull", BOT_STATE_COMBAT))
    {
        return false;
    }
    return true;
}

RangedPullAction::RangedPullAction(PlayerbotAI* ai) : CastSpellAction(ai, "ranged pull")
{
    string spell = ReachShootRangeAction::GetShootSpell(bot);
    if (spell.length() > 0)
    {
        this->spell = spell;
    }
}

bool RangedPullAction::isUseful()
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || target->IsInCombat())
    {
        return false;
    }
    // does not really matter if castspellaction::isuseful, because the point is aggro.
    // returning castspellaction::isuseful, will return false when spell is in progress,
    //   causing subsequent actions to stomp on it.
    return true;
}

bool RangedPullAction::Execute(Event event)
{
    Unit* target = GetTarget();
    if (!target || !target->IsAlive() || target->IsInCombat())
    {
        return false;
    }
    if (CastSpellAction::isUseful())
    {
        CastSpellAction::Execute(event);
    }
    // The point with this action is to use a ranged pull to get aggro, and
    // so this action must continue to return true from execute until aggro
    // is achieved.  Otherwise, the pull spellcast itself gets stomped
    // by some subsequent action before aggro is achieved.
    return isUseful();
}
