#include "botpch.h"
#include "../../playerbot.h"
#include "TaxiAction.h"
#include "../values/LastMovementValue.h"

using namespace ai;

bool TaxiAction::Execute(Event event)
{
    ai->RemoveShapeshift();

    LastMovement& movement = context->GetValue<LastMovement&>("last movement")->Get();

    WorldPacket& p = event.getPacket();
    if (!p.empty() && p.GetOpcode() == CMSG_MOVE_SPLINE_DONE)
    {
        WorldPacket p1(p);
        p1.rpos(0);
        bot->GetSession()->HandleMoveSplineDoneOpcode(p1);
        movement.taxiNodes.clear();
        movement.Set(NULL);

        if (bot->IsTaxiFlying() && bot->m_taxi.empty())
        {
            // ensure a clean drop off bird when landing
            bot->GetMotionMaster()->Clear();

            float x = bot->GetPositionX();
            float y = bot->GetPositionY();
            float z = bot->GetPositionZ();
            float o = bot->GetOrientation();

            float ground = z;
            if (Map* map = bot->GetMap())
            {
                float terrainZ = map->GetHeight(x, y, z);
                if (terrainZ > INVALID_HEIGHT && terrainZ < z)
                    ground = terrainZ;
            }

            if (ground < z)
            {
                bot->m_movementInfo.SetMovementFlags(MOVEFLAG_FALLING);
                bot->m_movementInfo.SetFallTime(0);
                bot->SetFallInformation(0, z);

                WorldPacket jumpPkt(MSG_MOVE_JUMP, 64);
                jumpPkt << bot->GetPackGUID();
                bot->m_movementInfo.Write(jumpPkt);
                bot->SendMessageToSet(&jumpPkt, false);

                bot->m_movementInfo.RemoveMovementFlag(MOVEFLAG_FALLING);
                bot->m_movementInfo.ChangePosition(x, y, ground, o);

                WorldPacket landPkt(MSG_MOVE_FALL_LAND, 64);
                landPkt << bot->GetPackGUID();
                bot->m_movementInfo.Write(landPkt);
                bot->SendMessageToSet(&landPkt, false);

                bot->SetFallInformation(0, ground);
            }

            bot->GetMap()->PlayerRelocation(bot, x, y, ground, o);
        }

        return true;
    }

    list<ObjectGuid> units = *context->GetValue<list<ObjectGuid> >("nearest npcs");
    for (list<ObjectGuid>::iterator i = units.begin(); i != units.end(); i++)
    {
        Creature *npc = bot->GetNPCIfCanInteractWith(*i, UNIT_NPC_FLAG_FLIGHTMASTER);
        if (!npc)
        {
            continue;
        }

        if (movement.taxiNodes.empty())
        {
            ostringstream out;
            out << "I will order the taxi from " << npc->GetName() << ". Please start flying, then instruct me again";
            ai->TellMaster(out);
            return true;
        }

        if (!bot->ActivateTaxiPathTo(movement.taxiNodes, npc))
        {
            ai->TellMaster("I can't fly with you");
            return false;
        }

        return true;
    }

    ai->TellMaster("Cannot find any flightmaster to talk");
    return false;
}
