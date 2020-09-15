#include "botpch.h"
#include "../../playerbot.h"
#include "AttackAction.h"
#include "MovementGenerator.h"
#include "CreatureAI.h"
#include "../../LootObjectStack.h"

using namespace ai;

bool AttackAction::Execute(Event event)
{
    Unit* target = GetTarget();

    if (!target)
    {
        return false;
    }

    return Attack(target);
}

bool AttackMyTargetAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    ObjectGuid guid = master->GetSelectionGuid();
    if (!guid)
    {
        if (verbose) ai->TellMaster("You have no target");
        {
            return false;
        }
    }

    return Attack(ai->GetUnit(guid));
}

bool AttackAction::Attack(Unit* target)
{
    MotionMaster &mm = *bot->GetMotionMaster();
    if (mm.GetCurrentMovementGeneratorType() == FLIGHT_MOTION_TYPE || bot->IsTaxiFlying())
    {
        if (verbose) ai->TellMaster("I cannot attack in flight");
        {
            return false;
        }
    }

    if (!target)
    {
        if (verbose) ai->TellMaster("I have no target");
        {
            return false;
        }
    }

    ostringstream msg;
    msg << target->GetName();
    if (bot->IsFriendlyTo(target))
    {
        msg << " is friendly to me";
        if (verbose) ai->TellMaster(msg.str());
        {
            return false;
        }
    }
    if (!bot->IsWithinLOSInMap(target))
    {
        msg << " is not on my sight";
        if (verbose) ai->TellMaster(msg.str());
        {
            return false;
        }
    }

    if (bot->IsMounted())
    {
        WorldPacket emptyPacket;
        bot->GetSession()->HandleCancelMountAuraOpcode(emptyPacket);
    }

    ObjectGuid guid = target->GetObjectGuid();
    bot->SetSelectionGuid(target->GetObjectGuid());

    Unit* oldTarget = context->GetValue<Unit*>("current target")->Get();
    context->GetValue<Unit*>("old target")->Set(oldTarget);

    context->GetValue<Unit*>("current target")->Set(target);
    context->GetValue<LootObjectStack*>("available loot")->Get()->Add(guid);

    Pet* pet = bot->GetPet();
    if (pet)
    {
        CreatureAI* creatureAI = ((Creature*)pet)->AI();
        if (creatureAI)
        {
            creatureAI->AttackStart(target);
        }
    }

    bot->Attack(target, true);
    ai->ChangeEngine(BOT_STATE_COMBAT);
    return true;
}

bool AttackDuelOpponentAction::isUseful()
{
    return AI_VALUE(Unit*, "duel target");
}

bool AttackDuelOpponentAction::Execute(Event event)
{
    return Attack(AI_VALUE(Unit*, "duel target"));
}
