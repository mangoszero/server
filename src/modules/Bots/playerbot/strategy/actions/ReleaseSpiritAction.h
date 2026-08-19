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
                // The ghost flag has to be tested as well as the corpse, and it is the case
                // this action is MOST likely to meet. A bot loaded as a ghost whose corpse row
                // is gone -- a restart with an empty corpse table -- is alive-false and
                // corpse-null, so both tests above pass and this action runs. It then queues
                // CMSG_REPOP_REQUEST, which WorldSession::HandleRepopRequestOpcode discards on
                // the spot because PLAYER_FLAGS_GHOST is already set (MiscHandler.cpp:89), and
                // returns true anyway. Sitting at the top of the dead ladder at relevance + 1,
                // a success here means the engine never reaches "spirit healer" one rung below
                // -- the fallback that exists for precisely this bot. It also re-applied
                // "-follow master,+stay" every tick for as long as it lasted.
                if (bot->IsAlive() || bot->GetCorpse() ||
                    bot->HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_GHOST))
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
