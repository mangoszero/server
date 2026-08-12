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
    // Run the whole way back, not 150 yards of it.
    //
    // ReactDistance is the right bound on ordinary "should I walk over to that" decisions, but
    // a ghost heading for its own body is not making that decision -- a player runs the entire
    // zone -- and the cap meant every long corpse run was refused on every tick, so the bot
    // never took a step and fell through to the graveyard healer instead. Hence
    // ignoreReactDistance for this call and this call only.
    //
    // Two guards, because an earlier attempt at this was blocked for lacking them:
    //
    // CROSS-FRAME. GetDistance is frame-aware and returns INFINITY across maps or instances, so
    // distance arithmetic is meaningless for a bot that died in an instance and released at the
    // entrance-map graveyard. Test the frame explicitly and decline, rather than letting a
    // nonsense number decide -- declining falls through to the healer, which is correct.
    //
    // ShareFrame, not GetMapId. A frame carries the instance id as well as the map, so comparing
    // map ids alone let a same-map/different-instance corpse through -- which then fed INFINITY
    // into the allowance arithmetic and handed its coordinates to the wrong instance.
    //
    // UNROUTABLE DESTINATION. Unbounded distance means the pathfinder can be handed a route it
    // cannot lay, across a ravine, a cliff or water. MoveTo has already returned true by then --
    // it returns as soon as it installs the point generator, and routing only fails later -- so
    // without a bound the same impossible destination is reissued forever and the bot stands
    // still. The bound is a per-corpse time allowance, and running out of it repops the ghost to
    // a graveyard. Both of those shapes are load-bearing; see the header for what they replaced.
    if (!bot->Where().ShareFrame(corpse->Where()))
    {
        return false;
    }

    float corpseDistance = corpse->GetDistance(bot);
    if (corpseDistance > sPlayerbotAIConfig.spellDistance)
    {
        // Start a fresh deadline whenever this is a different corpse from the one last run.
        // Keying on the guid is what stops a give-up from following the bot into its next
        // death: the state belongs to one corpse, not to the bot.
        if (m_corpseRunGuid != corpse->GetObjectGuid())
        {
            m_corpseRunGuid = corpse->GetObjectGuid();
            m_corpseRunStarted = time(0);
            m_corpseRunGaveUp = false;

            // Budget this run from its own length at the bot's live speed. GetSpeed picks up
            // both the server's configured run rate and the ghost bonus the core applies on
            // death, so a wisp gets the wisp's allowance.
            // Only guard against divide-by-zero; do not clamp away a slow server. The rate is
            // configurable down to 0.1, and clamping at 1.0 would have budgeted a crawling ghost
            // as if it ran.
            float speed = bot->GetSpeed(MOVE_RUN);
            if (speed < 0.1f)
            {
                speed = 0.1f;
            }

            // Three times the straight-line time, because a real route is never straight, plus
            // a flat minute so short runs are not tight. Floored at two minutes.
            //
            // The fifteen-minute ceiling is an explicit ABANDONMENT POLICY, not a proven bound.
            // It truncates the 3x margin on the longest runs -- Land's End Beach to Gadgetzan is
            // 3188 yards, 364s of straight-line ghost running, so 900s is only 2.47x -- and a
            // server configured near the minimum run rate would need far longer still. That is
            // deliberate: past this point the bot stops walking and takes a graveyard revive,
            // which is a worse outcome than arriving but a far better one than a ghost that
            // never gets up. Raise it if bots are seen abandoning runs they should finish.
            uint32 allowance = (uint32)((corpseDistance / speed) * 3.0f) + 60;
            if (allowance < 120) { allowance = 120; }
            if (allowance > 900) { allowance = 900; }
            m_corpseRunAllowance = allowance;
        }

        if (time(0) - m_corpseRunStarted > (time_t)m_corpseRunAllowance)
        {
            // Out of budget. That does NOT necessarily mean the route was impossible -- a valid
            // but slow or heavily detoured run expires the same way, which is why the allowance
            // is a policy rather than a proof. Either way the answer is the same: move the ghost
            // to a graveyard rather than simply declining, so the spirit healer becomes
            // reachable. Declining on the spot is what left a wedged ghost with no way up at
            // all. Exactly once per corpse; see the flag's note in the header.
            if (!m_corpseRunGaveUp)
            {
                m_corpseRunGaveUp = true;
                bot->RepopAtGraveyard();
            }

            return false;
        }

        // Two independent guards, because neither alone is enough.
        //
        // requireRoute is the one that makes this SAFE. PathFinder answers a destination whose
        // mmap tile is not resident -- which a corpse a thousand yards away usually is -- with
        // BuildShortcut, marked PATHFIND_NORMAL | PATHFIND_NOT_USING_PATH rather than NOPATH.
        // Nothing rejects that by default: MOVE_REQUIRE_PATH tests Failed(), which is NOPATH
        // alone, and a plain MovePoint does not set even that. The result is a terrain-snapped
        // straight line laid clean through cliffs, walls and buildings for the whole distance --
        // which is what a bot "porting around the area rather than running" actually is.
        // MOVE_REQUIRE_ROUTE tests Routed(), which excludes NOT_USING_PATH too, so an unroutable
        // goal now means the bot does not move rather than moving through the world.
        //
        // The leg cap is a REACH heuristic, not a safety one, and must not be mistaken for a
        // residency bound: mmap tiles follow 533-yard grids and load by grid lifecycle, so no
        // constant leg length can guarantee the endpoint's tile is loaded -- the bot may be
        // standing a yard from the boundary. A shorter leg is simply likelier to land in a
        // resident tile, so it gets a real route more often and the run reaches further before
        // the allowance runs out. When it does not, requireRoute refuses and the graveyard
        // fallback takes over.
        float x = corpse->GetPositionX();
        float y = corpse->GetPositionY();
        float z = corpse->GetPositionZ();

        if (corpseDistance > sPlayerbotAIConfig.reactDistance)
        {
            float ratio = sPlayerbotAIConfig.reactDistance / corpseDistance;
            x = bot->GetPositionX() + (x - bot->GetPositionX()) * ratio;
            y = bot->GetPositionY() + (y - bot->GetPositionY()) * ratio;
            z = bot->GetPositionZ() + (z - bot->GetPositionZ()) * ratio;
        }

        return MoveTo(corpse->GetMapId(), x, y, z, false, true, true);
    }

    // Arrived. Drop the guid so a later death starts a fresh deadline.
    m_corpseRunGuid = ObjectGuid();
    m_corpseRunStarted = 0;

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
    // The strategy repair that dying requires is NOT done here. It lives on the dead->alive
    // transition at the top of PlayerbotAI::UpdateAIInternal, which every resurrection reaches --
    // including the ones this file does not implement, such as accepting another player's
    // resurrect or a master activating a spirit healer. Repairing per revive site missed those.
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

    // Wait out the corpse reclaim delay before taking the healer, when there is a corpse to
    // wait for. Without this a bot that died anywhere in the zone was revived the instant its
    // ghost arrived -- the core drops every ghost at the graveyard's resurrection point, and
    // a spirit healer stands right there -- so a contested or high-level zone became a cycle:
    // die, release, restored immediately, walk back, die again. Observed in the Hinterlands as
    // 102 releases in three minutes, with bots stacked on the resurrection point.
    //
    // A player is held there by that same delay, so honouring it is what the client-side
    // behaviour actually looks like. This deliberately does NOT gate on a corpse the bot does
    // not have: a ghost with no corpse row -- what a restart leaves behind -- still takes the
    // healer immediately, which is the case this action exists for.
    //
    // Note this is a pacing fix only. It does not apply resurrection sickness or durability
    // loss, because that needs the CMSG_SPIRIT_HEALER_ACTIVATE path, which was tried and
    // blocked: it needs a corpseless route, a range margin against the core's strict InReach,
    // and an alive-recheck before the queued packet is dispatched.
    if (Corpse* corpse = bot->GetCorpse())
    {
        time_t reclaimTime = corpse->GetGhostTime() + bot->GetCorpseReclaimDelay(corpse->GetType() == CORPSE_RESURRECTABLE_PVP);
        if (reclaimTime > time(0))
        {
            return false;
        }
    }

    list<ObjectGuid> npcs = AI_VALUE(list<ObjectGuid>, "nearest npcs");
    for (list<ObjectGuid>::iterator i = npcs.begin(); i != npcs.end(); i++)
    {
        Unit* unit = ai->GetUnit(*i);
        if (unit && unit->IsSpiritHealer())
        {
            // REVERTED, and the reason is worth keeping. This briefly sent
            // CMSG_SPIRIT_HEALER_ACTIVATE instead -- the path a real client takes, reaching
            // SendSpiritResurrect for half health WITH resurrection sickness, 25% durability
            // loss and corpse bones -- to stop bots cycling endlessly at a contested graveyard.
            // Two defects killed it:
            //   * the core needs interaction range, so the bot had to walk in; but
            //     IsMovingAllowed rejects a dead bot with no corpse, which is exactly the case
            //     this action serves, leaving such a ghost stuck forever;
            //   * the range test disagreed with the core's strict InReach at exactly 5.0 yards,
            //     so a bot held at that boundary queued a packet the core discarded, every tick.
            //
            // Two premises behind it were also wrong: GM revive is NOT full health
            // (HandleReviveCommand uses ResurrectPlayer(0.5f)), and sickness would not have
            // fixed the loop anyway -- it is skipped below Death.SicknessLevel (default 11),
            // and AttackAnythingAction never checks for it, so bots would grind on at 75%
            // reduced stats regardless. Any retry needs a corpseless path, a range margin, an
            // alive-recheck before dispatch, and sickness-aware target selection.
            PlayerbotChatHandler ch(bot);
            if (! ch.revive(*bot))
            {
                ai->TellMaster(".. could not be revived ..");
                return false;
            }
            context->GetValue<Unit*>("current target")->Set(NULL);
            bot->SetSelectionGuid(ObjectGuid());
            // Strategy repair happens on the dead->alive transition, not here. See above.
            return true;
        }
    }

    ai->TellMaster("Cannot find any spirit healer nearby");
    return false;
}
