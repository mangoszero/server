#include "botpch.h"
#include "../../playerbot.h"
#include "MageTriggers.h"
#include "MageActions.h"

using namespace ai;

bool MageArmorTrigger::IsActive()
{
    Unit* target = GetTarget();
    return !ai->HasAura("ice armor", target) &&
        !ai->HasAura("frost armor", target) &&
        !ai->HasAura("molten armor", target) &&
        !ai->HasAura("mage armor", target);
}

bool PartyMemberNeedsSustenanceTrigger::IsActive()
{
    uint32 now = getMSTime();
    uint32 interval = m_lastResult ? ACTIVE_SCAN_MS : IDLE_SCAN_MS;
    if (now - m_lastScanTime < interval)
        return m_lastResult;

    m_lastScanTime = now;
    m_lastResult = false;

    Group* group = bot->GetGroup();
    if (!group)
        return false;

    for (GroupReference* gref = group->GetFirstMember(); gref; gref = gref->next())
    {
        Player* player = gref->getSource();
        if (!player || player == bot || !player->IsAlive())
            continue;
        if (player->GetMapId() != bot->GetMapId())
            continue;
        if (!bot->IsWithinDist(player, sPlayerbotAIConfig.sightDistance))
            continue;
        if (m_spellCategory == SPELLCATEGORY_DRINK && player->GetPowerType() != POWER_MANA)
            continue;
        FindConjuredFoodVisitor visitor(player, m_spellCategory);
        bool hasItem = InventoryAction::FindPlayerItem(player, &visitor) != 0;
        if (!hasItem)
        {
            m_lastResult = true;
            return true;
        }
    }

    return false;
}
