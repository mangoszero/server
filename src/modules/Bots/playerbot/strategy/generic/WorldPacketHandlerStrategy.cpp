#include <list>
#include "botpch.h"
#include "../../playerbot.h"
#include "WorldPacketHandlerStrategy.h"

using namespace ai;

void WorldPacketHandlerStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
            "group invite",
        NextAction::array(0, new NextAction("accept invitation", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "group set leader",
        NextAction::array(0, new NextAction("leader", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "not enough money",
        NextAction::array(0, new NextAction("tell not enough money", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "not enough reputation",
        NextAction::array(0, new NextAction("tell not enough reputation", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "cannot equip",
        NextAction::array(0, new NextAction("tell cannot equip", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "use game object",
        NextAction::array(0,
        new NextAction("add loot", relevance),
        new NextAction("use meeting stone", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "gossip hello",
        NextAction::array(0,
        new NextAction("trainer", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "activate taxi",
        NextAction::array(0, new NextAction("remember taxi", relevance), new NextAction("taxi", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "taxi done",
        NextAction::array(0, new NextAction("taxi", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "trade status",
        NextAction::array(0, new NextAction("accept trade", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "area trigger",
        NextAction::array(0, new NextAction("reach area trigger", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "within area trigger",
        NextAction::array(0, new NextAction("area trigger", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "loot response",
        NextAction::array(0, new NextAction("store loot", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "item push result",
        NextAction::array(0, new NextAction("query item usage", relevance), NULL)));

    // The action is registered as "ready check finished", the same string as the trigger
    // above it -- FinishReadyCheckAction is bound to that name in
    // WorldPacketActionContext. Pushing "finish ready check" matched nothing, and an
    // unregistered action name is not an error: CreateActionNode builds a node with a null
    // action and the engine drops it silently, so a ready check was simply never answered.
    triggers.push_back(new TriggerNode(
            "ready check finished",
        NextAction::array(0, new NextAction("ready check finished", relevance), NULL)));

    // Four trigger nodes used to push "lfg join", "lfg leave" and "lfg accept"
    // here. No action is registered under any of those names (LfgActions.cpp is
    // entirely commented out and LfgActions.h declares nothing), and none heads an
    // ActionNode cascade, so every push was dropped silently -- "lfg join" and
    // "lfg leave" on every "no possible targets"/"seldom" firing. Random-bot LFG is
    // handled outside the strategy engine by RandomPlayerbotMgr under
    // AiPlayerbot.RandomBotJoinLfg; teaching bots to answer the 1.12 Meeting Stone
    // flow through actions would be a new feature, not a re-wiring.

    triggers.push_back(new TriggerNode(
            "often",
        NextAction::array(0, new NextAction("security check", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "petition sign",
        NextAction::array(0, new NextAction("petition sign", relevance), NULL)));

    triggers.push_back(new TriggerNode(
            "guild invite",
        NextAction::array(0, new NextAction("guild accept", relevance), NULL)));
}

WorldPacketHandlerStrategy::WorldPacketHandlerStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{
    supported.push_back("loot roll");
    supported.push_back("check mount state");
    supported.push_back("quest objective completed");
    supported.push_back("party command");
    supported.push_back("ready check");
    supported.push_back("uninvite");
    supported.push_back("disband");
    supported.push_back("lfg role check");
}

void ReadyCheckStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
            "timer",
        NextAction::array(0, new NextAction("ready check", relevance), NULL)));
}
