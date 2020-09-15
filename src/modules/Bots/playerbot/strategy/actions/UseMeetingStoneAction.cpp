#include "botpch.h"
#include "../../playerbot.h"
#include "UseMeetingStoneAction.h"

#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "../../PlayerbotAIConfig.h"

namespace MaNGOS
{
    class GameObjectByGuidInRangeCheck
    {
    public:
        GameObjectByGuidInRangeCheck(WorldObject const* obj, ObjectGuid guid, float range) : i_obj(obj), i_range(range), i_guid(guid) {}
        WorldObject const& GetFocusObject() const { return *i_obj; }
        bool operator()(GameObject* u)
        {
            if (u && i_obj->IsWithinDistInMap(u, i_range) && u->isSpawned() && u->GetGOInfo() && u->GetObjectGuid() == i_guid)
            {
                return true;
            }

            return false;
        }
    private:
        WorldObject const* i_obj;
        float i_range;
        ObjectGuid i_guid;
    };
};

bool UseMeetingStoneAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    WorldPacket p(event.getPacket());
    p.rpos(0);
    ObjectGuid guid;
    p >> guid;

    if (master->GetSelectionGuid() && master->GetSelectionGuid() != bot->GetObjectGuid())
    {
        return false;
    }

    if (!master->GetSelectionGuid() && master->GetGroup() != bot->GetGroup())
    {
        return false;
    }

    if (master->IsBeingTeleported())
    {
        return false;
    }

    if (bot->IsInCombat())
    {
        ai->TellMasterNoFacing("I am in combat");
        return false;
    }

    list<GameObject*> targets;

    MaNGOS::GameObjectByGuidInRangeCheck u_check(master, guid, sPlayerbotAIConfig.sightDistance);
    MaNGOS::GameObjectListSearcher<MaNGOS::GameObjectByGuidInRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(master, searcher, sPlayerbotAIConfig.sightDistance);

    GameObject* gameObject = NULL;
    for(list<GameObject*>::iterator i = targets.begin(); i != targets.end(); i++)
    {
        GameObject* go = *i;
        if (go && go->isSpawned())
        {
            gameObject = go;
            break;
        }
    }

    if (!gameObject)
    {
        return false;
    }

    const GameObjectInfo* goInfo = gameObject->GetGOInfo();
    if (!goInfo || goInfo->type != GAMEOBJECT_TYPE_SUMMONING_RITUAL)
    {
        return false;
    }

    return Teleport();
}


bool SummonAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    if (master->GetSession()->GetSecurity() < SEC_GAMEMASTER)
    {
        ai->TellMasterNoFacing("You cannot summon me");
        return false;
    }

    return Teleport();
}

bool SummonAction::Teleport()
{
    Player* master = GetMaster();
    if (!master->IsBeingTeleported())
    {
        float followAngle = GetFollowAngle();
        for (float angle = followAngle - M_PI; angle <= followAngle + M_PI; angle += M_PI / 4)
        {
            uint32 mapId = master->GetMapId();
            float x = master->GetPositionX() + cos(angle) * sPlayerbotAIConfig.followDistance;
            float y = master->GetPositionY()+ sin(angle) * sPlayerbotAIConfig.followDistance;
            float z = master->GetPositionZ();
            if (master->IsWithinLOS(x, y, z))
            {
                bot->GetMotionMaster()->Clear();
                bot->TeleportTo(mapId, x, y, z, 0);
                return true;
            }
        }
    }

    PlayerbotChatHandler ch(master);
    if (!ch.teleport(*bot))
    {
        ai->TellMasterNoFacing("You cannot summon me");
        return false;
    }

    return true;
}
