#pragma once

#include "../Action.h"

namespace ai
{
    class ReleaseSpiritAction : public Action {
        public:
            ReleaseSpiritAction(PlayerbotAI* ai) : Action(ai, "release") {}

        public:
            virtual bool Execute(Event event)
            {
                if (bot->IsAlive() || bot->GetCorpse())
                {
                    return false;
                }

                // "follow master", not "follow". The latter is an action and a chat trigger
                // name, but no StrategyContext registers it, so removeStrategy found nothing
                // and the intended removal never happened. Every other site gets this right.
                ai->ChangeStrategy("-follow master,+stay", BOT_STATE_NON_COMBAT);

                WorldPacket* packet = new WorldPacket(CMSG_REPOP_REQUEST);
                bot->GetSession()->QueuePacket(packet);
                return true;
            }
    };
}
