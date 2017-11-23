#include "botpch.h"
#include "../../playerbot.h"
#include "ItemForSpellValue.h"

using namespace ai;

#ifndef WIN32
inline int strcmpi(const char* s1, const char* s2)
{
    for (; *s1 && *s2 && (toupper(*s1) == toupper(*s2)); ++s1, ++s2);
    return *s1 - *s2;
}
#endif

Item* ItemForSpellValue::Calculate()
{
    uint32 spellid = atoi(qualifier.c_str());
    if (!spellid)
        return nullptr;

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid );
    if (!spellInfo)
        return nullptr;

    Item* itemForSpell = nullptr;
    Player* trader = bot->GetTrader();
    if (trader)
    {
        itemForSpell = trader->GetTradeData()->GetItem(TRADE_SLOT_NONTRADED);
        if (itemForSpell && itemForSpell->IsFitToSpellRequirements(spellInfo))
            return itemForSpell;
    }

    // Workaround as some spells have no item mask (e.g. shaman weapon enhancements)
    if (!strcmpi(spellInfo->SpellName[0], "rockbiter weapon") ||
            !strcmpi(spellInfo->SpellName[0], "flametongue weapon") ||
            !strcmpi(spellInfo->SpellName[0], "earthliving weapon") ||
            !strcmpi(spellInfo->SpellName[0], "frostbrand weapon") ||
            !strcmpi(spellInfo->SpellName[0], "windfury weapon"))
    {
        itemForSpell = GetItemFitsToSpellRequirements(EQUIPMENT_SLOT_MAINHAND, spellInfo);
        if (itemForSpell && itemForSpell->GetProto()->Class == ITEM_CLASS_WEAPON)
            return itemForSpell;

        itemForSpell = GetItemFitsToSpellRequirements(EQUIPMENT_SLOT_OFFHAND, spellInfo);
        if (itemForSpell && itemForSpell->GetProto()->Class == ITEM_CLASS_WEAPON)
            return itemForSpell;

        return nullptr;
    }

    for( uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; slot++ ) {
        itemForSpell = GetItemFitsToSpellRequirements(slot, spellInfo);
        if (itemForSpell)
            return itemForSpell;
    }
    return nullptr;
}

Item* ItemForSpellValue::GetItemFitsToSpellRequirements(uint8 slot, SpellEntry const *spellInfo)
{
    Item* const itemForSpell = bot->GetItemByPos( INVENTORY_SLOT_BAG_0, slot );
    if (!itemForSpell || itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
        return nullptr;

    if (itemForSpell->IsFitToSpellRequirements(spellInfo))
        return itemForSpell;

    return nullptr;
}
