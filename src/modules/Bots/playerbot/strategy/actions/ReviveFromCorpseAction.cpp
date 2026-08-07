#include "botpch.h"
#include "Corpse.h"
#include "../../playerbot.h"
#include "ReviveFromCorpseAction.h"
#include "../../PlayerbotFactory.h"
#include "../../PlayerbotAIConfig.h"

using namespace ai;

bool ReviveFromCorpseAction::Execute(Event event)
{
    Corpse* corpse = bot->GetCorpse();
    if (!corpse)
    {
        return false;
    }

    // Walk back to the body when it is out of reclaim range. Refusing outright is what
    // left ghosts standing where they died: reclaim needs the corpse within
    // SpellDistance, the core drops a ghost at the graveyard rather than at its corpse,
    // and the spirit healer fallback only helps if one is within sight -- which, for a bot
    // killed out in the world, it is not. Observed on a night elf standing ten yards from
    // the harpies that killed it with an empty "nearest npcs" list, whispering that it
    // could not find a spirit healer.
    //
    // MoveTo refuses anything beyond ReactDistance, so a corpse further than that still
    // cannot be reached and the manager's revive timer remains the backstop for it.
    if (corpse->GetDistance(bot) > sPlayerbotAIConfig.spellDistance)
    {
        return MoveTo(corpse->GetMapId(), corpse->GetPositionX(), corpse->GetPositionY(), corpse->GetPositionZ());
    }

    time_t reclaimTime = corpse->GetGhostTime() + bot->GetCorpseReclaimDelay( corpse->GetType()==CORPSE_RESURRECTABLE_PVP );
    if (reclaimTime > time(0))
    {
        return false;
    }

    PlayerbotChatHandler ch(bot);
    if (! ch.revive(*bot))
    {
        ai->TellMaster(".. could not be revived ..");
        return false;
    }
    context->GetValue<Unit*>("current target")->Set(NULL);
    bot->SetSelectionGuid(ObjectGuid());
    return true;
}

bool SpiritHealerAction::Execute(Event event)
{
    // Being dead is the whole precondition. This used to require a corpse as well, which
    // is backwards: the spirit healer is precisely what you use when there is no corpse to
    // go back to. A bot that dies and is still dead across a restart comes back as a ghost
    // with nothing in the corpse table -- observed with every dead bot on the server at
    // once, the table holding zero rows -- so the corpse test rejected exactly the bots
    // that had no other way up, and they stood at the graveyard as wisps until the random
    // manager's timer resurrected them minutes later.
    if (!bot->IsDead())
    {
        return false;
    }

    list<ObjectGuid> npcs = AI_VALUE(list<ObjectGuid>, "nearest npcs");
    for (list<ObjectGuid>::iterator i = npcs.begin(); i != npcs.end(); i++)
    {
        Unit* unit = ai->GetUnit(*i);
        if (unit && unit->IsSpiritHealer())
        {
            PlayerbotChatHandler ch(bot);
            if (! ch.revive(*bot))
            {
                ai->TellMaster(".. could not be revived ..");
                return false;
            }
            context->GetValue<Unit*>("current target")->Set(NULL);
            bot->SetSelectionGuid(ObjectGuid());
            return true;
        }
    }

    ai->TellMaster("Cannot find any spirit healer nearby");
    return false;
}
