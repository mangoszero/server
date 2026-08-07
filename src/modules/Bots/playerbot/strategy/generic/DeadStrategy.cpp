#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "../Strategy.h"
#include "DeadStrategy.h"

using namespace ai;

void DeadStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    // Trigger name changed from "dead" to "bot dead" because of collision with AI_VALUE2(bool, "dead", ...))
    //
    // The spirit healer is the fallback, and without it a random bot had no way back on
    // its feet at all. "revive from corpse" refuses unless the corpse is within
    // SpellDistance, and after the core auto-releases a dead bot its ghost is standing at
    // the graveyard while its corpse is wherever it fell -- a gap nothing closes, because
    // no action walks a ghost to its body and MoveTo would refuse the distance anyway.
    // Every other route to a spirit healer is master-driven: "spirit healer with master"
    // needs the master's CMSG_SPIRIT_HEALER_ACTIVATE, and the bare "spirit healer" action
    // was reachable only as a typed chat command. An ungrouped random bot has neither, so
    // it simply stood at the graveyard until RandomPlayerbotMgr's revive timer resurrected
    // it minutes later -- which is what "hanging around the starting-zone graveyard"
    // actually was.
    //
    // Lower relevance so reclaiming the body is preferred, and it only gets its turn on
    // the ticks where the corpse action has already declined. Note that nothing here
    // costs resurrection sickness the way the real spirit healer would: both actions end
    // at PlayerbotChatHandler::revive, which is the GM .revive command. Preferring the
    // corpse is therefore about behaving like a player where the body is actually
    // reclaimable, not about avoiding a penalty.
    triggers.push_back(new TriggerNode(
        "bot dead",
        NextAction::array(0,
            new NextAction("revive from corpse", relevance),
            new NextAction("spirit healer", relevance - 1.0f),
            NULL)));

    triggers.push_back(new TriggerNode(
        "resurrect request",
        NextAction::array(0, new NextAction("accept resurrect", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "master released spirit",
        NextAction::array(0, new NextAction("release spirit with master", relevance), NULL)));

    triggers.push_back(new TriggerNode(
        "master spirit healer",
        NextAction::array(0, new NextAction("spirit healer with master", relevance), NULL)));
}

DeadStrategy::DeadStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{}
