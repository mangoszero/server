#include "botpch.h"
#include "../../playerbot.h"
#include "WarlockActions.h"
#include "../actions/InventoryAction.h"

using namespace ai;

bool CastCreateSpellstoneAction::isUseful()
{
    FindNamedItemVisitor visitor("spellstone");
    return CastBuffSpellAction::isUseful() &&
           InventoryAction::FindPlayerItem(bot, &visitor) == nullptr &&
           AI_VALUE2(uint8, "item count", "soul shard") > 1 &&
           !bot->IsTwoHandUsed() &&
           bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == nullptr;
}

bool CastCreateFirestoneAction::isUseful()
{
    FindNamedItemVisitor visitor("firestone");
    return CastBuffSpellAction::isUseful() &&
           InventoryAction::FindPlayerItem(bot, &visitor) == nullptr &&
           AI_VALUE2(uint8, "item count", "soul shard") > 1 &&
           !bot->IsTwoHandUsed() &&
           bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == nullptr;
}

bool EquipSpellstoneAction::isUseful()
{
    FindNamedItemVisitor visitor("spellstone");
    return InventoryAction::FindPlayerItem(bot, &visitor) != nullptr &&
           !bot->IsTwoHandUsed() &&
           bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) == nullptr;
}

bool EquipSpellstoneAction::Execute(Event event)
{
    FindNamedItemVisitor visitor("spellstone");
    Item* item = InventoryAction::FindPlayerItem(bot, &visitor);
    if (!item ||
        bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND) != nullptr ||
        bot->IsTwoHandUsed())
    {
        return false;
    }

    uint8 bagIndex = item->GetBagSlot();
    uint8 slot = item->GetSlot();

    WorldPacket* const packet = new WorldPacket(CMSG_AUTOEQUIP_ITEM, 2);
    *packet << bagIndex << slot;
    bot->GetSession()->QueuePacket(packet);
    return true;
}

bool CastRainOfFireAction::isUseful()
{
    if (!CastSpellAction::isUseful())
    {
        return false;
    }
    if (!ai->HasStrategy("cautious"))
    {
        return true;
    }
    Unit* target = AI_VALUE(Unit*, "current target");
    if (!target || ai->HasNonCombatantInRange(8.0f,
        target->GetPositionX(), target->GetPositionY(), target->GetPositionZ()))
    {
        return false;
    }
    return true;
}

/**
 * @brief The best Create Healthstone rank this warlock actually knows.
 *
 * Walked strongest first, so a level 58 warlock makes a Major and a level 10 one makes a
 * Minor. Falls back to the unsuffixed name, which is what the whole action used to be.
 */
string CastCreateHealthstoneAction::BestKnownRank()
{
    static const char* ranks[] =
    {
        "create healthstone (major)",
        "create healthstone (greater)",
        "create healthstone",
        "create healthstone (lesser)",
        "create healthstone (minor)"
    };

    for (size_t i = 0; i < sizeof(ranks) / sizeof(ranks[0]); ++i)
    {
        if (AI_VALUE2(uint32, "spell id", ranks[i]))
        {
            return ranks[i];
        }
    }

    return "create healthstone";
}

bool CastCreateHealthstoneAction::isPossible()
{
    // CanCastSpell, not merely "is the rank known".
    //
    // The base class runs a full preflight -- cooldown, power, reagents, free bag space --
    // and checking only that the spell exists throws all of that away. A warlock out of mana
    // or with full bags would then report this relevance-15 action as possible on every
    // tick, delaying or suppressing the lower-priority non-combat work it actually needed,
    // such as sitting down to drink. The range check the base also performs is immaterial
    // here because the target is the caster.
    return ai->CanCastSpell(BestKnownRank(), bot);
}

bool CastCreateHealthstoneAction::Execute(Event event)
{
    return ai->CastSpell(BestKnownRank(), bot);
}
