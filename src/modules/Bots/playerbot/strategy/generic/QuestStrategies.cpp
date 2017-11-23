#include "botpch.h"
#include "../../playerbot.h"
#include "QuestStrategies.h"

using namespace ai;

QuestStrategy::QuestStrategy(PlayerbotAI* ai) : PassTroughStrategy(ai)
{
    supported.push_back("accept quest");
}

void QuestStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    PassTroughStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "quest share",
        NextAction::array(0, new NextAction("accept quest share", relevance), nullptr)));
}


void DefaultQuestStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    QuestStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "use game object",
        NextAction::array(0,
            new NextAction("talk to quest giver", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "gossip hello",
        NextAction::array(0,
            new NextAction("talk to quest giver", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "complete quest",
        NextAction::array(0, new NextAction("talk to quest giver", relevance), nullptr)));
}

DefaultQuestStrategy::DefaultQuestStrategy(PlayerbotAI* ai) : QuestStrategy(ai)
{
}



void AcceptAllQuestsStrategy::InitTriggers(std::list<TriggerNode*> &triggers)
{
    QuestStrategy::InitTriggers(triggers);

    triggers.push_back(new TriggerNode(
        "use game object",
        NextAction::array(0,
            new NextAction("talk to quest giver", relevance), new NextAction("accept all quests", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "gossip hello",
        NextAction::array(0,
            new NextAction("talk to quest giver", relevance), new NextAction("accept all quests", relevance), nullptr)));

    triggers.push_back(new TriggerNode(
        "complete quest",
        NextAction::array(0, 
            new NextAction("talk to quest giver", relevance), new NextAction("accept all quests", relevance), nullptr)));
}

AcceptAllQuestsStrategy::AcceptAllQuestsStrategy(PlayerbotAI* ai) : QuestStrategy(ai)
{
}
