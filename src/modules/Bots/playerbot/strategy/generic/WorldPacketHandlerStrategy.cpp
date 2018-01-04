#include "botpch.h"
#include "../../playerbot.h"
#include "WorldPacketHandlerStrategy.h"

using namespace ai;

void WorldPacketHandlerStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "group invite",
        NextAction::array(0, new NextAction("accept invitation", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "group set leader",
        NextAction::array(0, new NextAction("leader", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "not enough money",
        NextAction::array(0, new NextAction("tell not enough money", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "not enough reputation",
        NextAction::array(0, new NextAction("tell not enough reputation", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "cannot equip",
        NextAction::array(0, new NextAction("tell cannot equip", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "use game object",
        NextAction::array(0,
            new NextAction("add loot", relevance),
            new NextAction("use meeting stone", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "gossip hello",
        NextAction::array(0,
            new NextAction("trainer", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "activate taxi",
        NextAction::array(0, new NextAction("remember taxi", relevance), new NextAction("taxi", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "taxi done",
        NextAction::array(0, new NextAction("taxi", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "trade status",
        NextAction::array(0, new NextAction("accept trade", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "area trigger",
        NextAction::array(0, new NextAction("reach area trigger", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "within area trigger",
        NextAction::array(0, new NextAction("area trigger", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "loot response",
        NextAction::array(0, new NextAction("store loot", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "item push result",
        NextAction::array(0, new NextAction("query item usage", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "ready check finished",
        NextAction::array(0, new NextAction("finish ready check", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "no possible targets",
        NextAction::array(0, new NextAction("lfg join", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "seldom",
        NextAction::array(0, new NextAction("lfg leave", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "lfg proposal",
        NextAction::array(0, new NextAction("lfg accept", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "lfg proposal active",
        NextAction::array(0, new NextAction("lfg accept", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "often",
        NextAction::array(0, new NextAction("security check", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "guild invite",
        NextAction::array(0, new NextAction("guild accept", relevance), nullptr)));
}

WorldPacketHandlerStrategy::WorldPacketHandlerStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{
    supported.push_back("loot roll");
    supported.push_back("check mount state");
    supported.push_back("quest objective completed");
    supported.push_back("party command");
    supported.push_back("ready check");
    supported.push_back("uninvite");
    supported.push_back("lfg role check");
}


void ReadyCheckStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    triggers.push_back(new TriggerNode(
        "timer",
        NextAction::array(0, new NextAction("ready check", relevance), nullptr)));
}
