#include "botpch.h"
#include "../../playerbot.h"
#include "DropQuestAction.h"


using namespace ai;

bool DropQuestAction::Execute(Event event)
{
    string link = event.getParam();

    PlayerbotChatHandler ch(bot);
    if (!ch.dropQuest(link))
    {
        ostringstream out; out << "Could not drop quest: " << link;
        ai->TellMaster(out);
        return false;
    }

    ai->TellMaster("Quest removed");
    return true;
}
