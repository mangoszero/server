#pragma once

#include "../Action.h"

namespace ai
{
    class ReleaseSpiritWithMasterAction : public Action
    {
        public:
            ReleaseSpiritWithMasterAction(PlayerbotAI* ai) : Action(ai, "release spirit with master") {}

        public:
            virtual bool Execute(Event event)
            {
                if (bot->IsAlive() || bot->GetCorpse())
                {
                    return false;
                }

                Player* master = ai->GetMaster();
                if (!master || !bot->GetGroup() || bot->GetGroup() != master->GetGroup())
                {
                    return false;
                }

                ai->ChangeStrategy("-follow,+stay", BOT_STATE_NON_COMBAT);

                WorldPacket* packet = new WorldPacket(CMSG_REPOP_REQUEST);
                bot->GetSession()->QueuePacket(packet);
                return true;
            }
    };
}
