#include "botpch.h"
#include "../../playerbot.h"
#include "GossipHelloAction.h"


using namespace ai;

bool GossipHelloAction::Execute(Event event)
{
    ObjectGuid guid;

    WorldPacket &p = event.getPacket();
    if (p.empty())
    {
        Player* master = GetMaster();
        if (master)
        {
            guid = master->GetSelectionGuid();
        }
    }
    else
    {
        p.rpos(0);
        p >> guid;
    }

    if (!guid)
    {
        return false;
    }

    Creature *pCreature = bot->GetNPCIfCanInteractWith(guid, UNIT_NPC_FLAG_NONE);
    if (!pCreature)
    {
        DEBUG_LOG ("[PlayerbotMgr]: HandleMasterIncomingPacket - Received  CMSG_GOSSIP_HELLO %s not found or you can't interact with him.", guid.GetString().c_str());
        return false;
    }

    GossipMenuItemsMapBounds pMenuItemBounds = sObjectMgr.GetGossipMenuItemsMapBounds(pCreature->GetCreatureInfo()->GossipMenuId);
    if (pMenuItemBounds.first == pMenuItemBounds.second)
    {
        return false;
    }

    WorldPacket p1;
    p1 << guid;
    bot->GetSession()->HandleGossipHelloOpcode(p1);
    bot->SetFacingToObject(pCreature);

    ostringstream out; out << "--- " << pCreature->GetName() << " ---";
    ai->TellMasterNoFacing(out.str());

    GossipMenu& menu = bot->PlayerTalkClass->GetGossipMenu();
    unsigned int i = 0, loops = 0;
    set<uint32> alreadyTalked;
    while (i < menu.MenuItemCount() && loops++ < 100)
    {
        GossipMenuItem const& item = menu.GetItem(i);
        ai->TellMasterNoFacing(item.m_gMessage);

        if (item.m_gOptionId < 1000 && item.m_gOptionId != GOSSIP_OPTION_GOSSIP)
        {
            i++;
            continue;
        }

        WorldPacket p1;
        std::string code;
        p1 << guid << menu.GetMenuId() << i << code;
        bot->GetSession()->HandleGossipSelectOptionOpcode(p1);

        i = 0;
    }

    bot->TalkedToCreature(pCreature->GetEntry(), pCreature->GetObjectGuid());
    return true;
}
