#include "botpch.h"
#include "../../playerbot.h"
#include "../../RandomPlayerbotMgr.h"
#include "SecurityCheckAction.h"

using namespace ai;

bool SecurityCheckAction::isUseful()
{
    return sRandomPlayerbotMgr.IsRandomBot(bot) && ai->GetMaster() && ai->GetMaster()->GetSession()->GetSecurity() < SEC_GAMEMASTER;
}

bool SecurityCheckAction::Execute(Event event)
{
    // Derive "already suspended" from the strategies themselves rather than a member flag.
    // A flag desyncs, and dangerously: any of the external ResetStrategies callers -- master
    // relog (RandomPlayerbotMgr::OnPlayerLogout/OnPlayerLogin), group join/leave, duel accept,
    // death and resurrect -- rebuilds every engine from AiFactory and strips the lock, but
    // cannot clear a private bool. The action would then believe it had already suspended a
    // bot that is in fact running normally under bad loot settings, skip the apply branch, and
    // never warn the master again. That is the inverse of the bug this was fixing and it
    // silently removes the protection. Testing the exact combination this action applies keeps
    // the two in step, and is specific enough not to fire on a player-set "+passive" alone.
    bool locked = ai->HasStrategy("passive", BOT_STATE_NON_COMBAT) &&
                  ai->HasStrategy("passive", BOT_STATE_COMBAT) &&
                  ai->HasStrategy("stay", BOT_STATE_NON_COMBAT);

    Group* group = bot->GetGroup();
    if (group)
    {
        LootMethod method = group->GetLootMethod();
        ItemQualities threshold = group->GetLootThreshold();
        if (method == MASTER_LOOT || method == FREE_FOR_ALL || threshold > ITEM_QUALITY_UNCOMMON)
        {
            if (!locked)
            {
                ai->TellMaster("I won't do anything until you change loot type to group loot with green threshold");
                ai->ChangeStrategy("+passive,+stay", BOT_STATE_NON_COMBAT);
                ai->ChangeStrategy("+passive,+stay", BOT_STATE_COMBAT);
            }
            return true;
        }
    }

    if (locked)
    {
        // This was a one-way door: once the master fixed the loot settings isUseful() went
        // false, so nothing ever removed what had been added, and the bot stayed passive and
        // stationary until something else happened to call ResetStrategies.
        //
        // Restoring is more than "-passive,-stay". Adding "stay" evicted whatever movement
        // strategy the bot held -- "follow master" or "move random" -- through sibling
        // eviction in MovementStrategyContext, and removing "stay" does not bring it back.
        // A bot restored that way is no longer parked but still cannot follow or wander.
        // ResetStrategies rebuilds every engine from AiFactory for the bot's current group
        // state, which is the only complete restore.
        ai->TellMaster("Loot looks fine now, I'll get back to it");
        ai->ResetStrategies();
    }

    return false;
}
