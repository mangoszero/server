#include "botpch.h"
#include "../../playerbot.h"
#include "../../PlayerbotAIConfig.h"
#include "SpiritHealerWithMasterAction.h"

using namespace ai;

bool SpiritHealerWithMasterAction::Execute(Event event)
{
    if (bot->IsAlive() || !bot->GetCorpse())
    {
        return false;
    }

    Player* master = ai->GetMaster();
    if (!master || !bot->GetGroup() || bot->GetGroup() != master->GetGroup())
    {
        return false;
    }

    WorldPacket p(event.getPacket());
    p.rpos(0);
    ObjectGuid guid;
    p >> guid;

    Unit* spiritHealer = ai->GetUnit(guid);
    if (!spiritHealer || !spiritHealer->IsSpiritHealer())
    {
        return false;
    }

    if (!bot->IsWithinDistInMap(spiritHealer, sPlayerbotAIConfig.spellDistance))
    {
        return false;
    }

    WorldPacket* packet = new WorldPacket(CMSG_SPIRIT_HEALER_ACTIVATE, 8);
    *packet << guid;
    bot->GetSession()->QueuePacket(packet);

    context->GetValue<Unit*>("current target")->Set(NULL);
    bot->SetSelectionGuid(ObjectGuid());
    ai->ChangeEngine(BOT_STATE_NON_COMBAT);
    return true;
}
