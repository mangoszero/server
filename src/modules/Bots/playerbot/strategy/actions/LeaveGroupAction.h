#pragma once

#include "../Action.h"
#include "../../PlayerbotAIConfig.h"
#include "../../RandomPlayerbotMgr.h"

namespace ai
{
    class LeaveGroupAction : public Action
    {
        public:
            LeaveGroupAction(PlayerbotAI* ai, string name = "leave") : Action(ai, name) {}

            virtual bool Execute(Event event)
            {
                if (bot->GetGroup())
                {
                    ai->TellMaster("Goodbye!", PLAYERBOT_SECURITY_TALK);
                }

                ai->ResetStrategies();

                WorldPacket p;
                string member = bot->GetName();
                p << uint32(PARTY_OP_LEAVE) << member << uint32(0);
                bot->GetSession()->HandleGroupDisbandOpcode(p);

                if (sRandomPlayerbotMgr.IsRandomBot(bot))
                {
                    bot->GetPlayerbotAI()->SetMaster(NULL);
                    sRandomPlayerbotMgr.ScheduleTeleport(bot->GetGUIDLow());
                    sRandomPlayerbotMgr.SetLootAmount(bot, 0);
                }

                ai->ChangeStrategy("-follow master", BOT_STATE_NON_COMBAT);
                ai->ChangeStrategy("-follow master", BOT_STATE_DEAD);
                ai->ChangeStrategy("-follow master", BOT_STATE_COMBAT);

                if (bot->GetMotionMaster()->GetCurrentMovementGeneratorType() != FLIGHT_MOTION_TYPE && !bot->IsTaxiFlying())
                {
                    bot->GetMotionMaster()->Clear();
                    bot->GetMotionMaster()->MoveIdle();
                    bot->clearUnitState(UNIT_STAT_CHASE);
                    bot->clearUnitState(UNIT_STAT_FOLLOW);
                }

                ai->ChangeStrategy("+stay", BOT_STATE_DEAD);
                return true;
            }
    };

    class PartyCommandAction : public LeaveGroupAction
    {
        public:
            PartyCommandAction(PlayerbotAI* ai) : LeaveGroupAction(ai, "party command") {}

            virtual bool Execute(Event event)
            {
                WorldPacket& p = event.getPacket();
                p.rpos(0);
                uint32 operation;
                string member;

                p >> operation >> member;

                if (operation != PARTY_OP_LEAVE)
                {
                    return false;
                }

                Player* master = GetMaster();
                if (master && member == master->GetName())
                {
                    return LeaveGroupAction::Execute(event);
                }

                return false;
            }
    };

    class UninviteAction : public LeaveGroupAction
    {
        public:
            UninviteAction(PlayerbotAI* ai) : LeaveGroupAction(ai, "uninvite") {}

            virtual bool Execute(Event event)
            {
                WorldPacket& p = event.getPacket();
                p.rpos(0);

                bool match = false;
                if (p.GetOpcode() == CMSG_GROUP_UNINVITE)
                {
                    string membername;
                    p >> membername;
                    match = (bot->GetName() == membername);
                }
                else
                {
                    ObjectGuid guid;
                    p >> guid;
                    match = (bot->GetObjectGuid() == guid);
                }

                if (match)
                {
                    return LeaveGroupAction::Execute(event);
                }

                return false;
            }
    };

    class DisbandAction : public LeaveGroupAction
    {
        public:
            DisbandAction(PlayerbotAI* ai) : LeaveGroupAction(ai, "disband") {}

            virtual bool Execute(Event event)
            {
                return LeaveGroupAction::Execute(event);
            }
    };
}
