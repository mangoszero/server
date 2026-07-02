#include "botpch.h"
#include "../../playerbot.h"
#include "PetitionSignAction.h"

using namespace std;
using namespace ai;

bool PetitionSignAction::Execute(Event event)
{
    Player* master = GetMaster();
    if (!master)
    {
        return false;
    }

    WorldPacket p(event.getPacket());
    p.rpos(0);

    ObjectGuid petitionGuid;
    p >> petitionGuid;

    ObjectGuid ownerGuid;
    p >> ownerGuid;

    if (!ai->GetSecurity()->CheckLevelFor(PLAYERBOT_SECURITY_INVITE, false, master, true))
    {
        return false;
    }

    if (bot->GetGuildId())
    {
        ai->TellMaster("Sorry, I am in a guild already");
        return false;
    }

    WorldPacket packet(CMSG_PETITION_SIGN);
    packet << petitionGuid;
    packet << uint8(0);
    bot->GetSession()->HandlePetitionSignOpcode(packet);
    return true;
}
