#include "../../botpch.h"
#include "../playerbot.h"
#include "AiObject.h"

AiObject::AiObject(PlayerbotAI* ai) :
    PlayerbotAIAware(ai),
    bot(ai->GetBot()),
    context(ai->GetAiObjectContext()),
    chat(ai->GetChatHelper())
{
}

/**
 * Returns the bot master's player instance.
 */
Player* AiObject::GetMaster()
{
    return ai->GetMaster();
}
