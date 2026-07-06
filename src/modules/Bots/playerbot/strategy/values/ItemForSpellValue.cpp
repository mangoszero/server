#include "botpch.h"
#include "../../playerbot.h"
#include "ItemForSpellValue.h"

using namespace ai;

#ifndef WIN32
inline int strcmpi(const char* s1, const char* s2)
{
    for (; *s1 && *s2 && (toupper(*s1) == toupper(*s2)); ++s1, ++s2);
    {
        return *s1 - *s2;
    }
}
#endif

Item* ItemForSpellValue::Calculate()
{
    uint32 spellid = atoi(qualifier.c_str());
    if (!spellid)
    {
        return NULL;
    }

    SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellid );
    if (!spellInfo)
    {
        return NULL;
    }

    Item* itemForSpell = NULL;
    Player* trader = bot->GetTrader();
    if (trader)
    {
        itemForSpell = trader->GetTradeData()->GetItem(TRADE_SLOT_NONTRADED);
        if (itemForSpell && itemForSpell->IsFitToSpellRequirements(spellInfo))
        {
            return itemForSpell;
        }
    }

    // Workaround as some spells have no item mask (e.g. shaman weapon enhancements)
    if (!strcmpi(spellInfo->Name_lang[0], "rockbiter weapon") ||
        !strcmpi(spellInfo->Name_lang[0], "flametongue weapon") ||
        !strcmpi(spellInfo->Name_lang[0], "earthliving weapon") ||
        !strcmpi(spellInfo->Name_lang[0], "frostbrand weapon") ||
        !strcmpi(spellInfo->Name_lang[0], "windfury weapon"))
    {
        itemForSpell = GetItemFitsToSpellRequirements(EQUIPMENT_SLOT_MAINHAND, spellInfo);
        if (itemForSpell && itemForSpell->GetProto()->Class == ITEM_CLASS_WEAPON)
        {
            return itemForSpell;
        }

        itemForSpell = GetItemFitsToSpellRequirements(EQUIPMENT_SLOT_OFFHAND, spellInfo);
        if (itemForSpell && itemForSpell->GetProto()->Class == ITEM_CLASS_WEAPON)
        {
            return itemForSpell;
        }

        return NULL;
    }

    // Feed Pet: search bags for food items instead of equipped items
    if (!itemForSpell && spellid == SPELL_ID_FEED_PET)
    {
        Pet* pet = bot->GetPet();
        if (pet && pet->IsAlive())
        {
            Item* bestFood = NULL;
            uint32 lowestScore = 0x7fffffff;

            // Main backpack
            for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
            {
                Item* item = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
                uint32 itemScore = GetPetFoodScore(pet, item, spellInfo);
                if(itemScore > 0 && itemScore < lowestScore)
                {
                    bestFood = item;
                    lowestScore = itemScore;
                }
            }

            // Bags
            for (uint8 bag = INVENTORY_SLOT_BAG_START; bag < INVENTORY_SLOT_BAG_END; ++bag)
            {
                Bag* pBag = (Bag*)bot->GetItemByPos(INVENTORY_SLOT_BAG_0, bag);
                if (!pBag)
                {
                    continue;
                }
                for (uint32 slot = 0; slot < pBag->GetBagSize(); ++slot)
                {
                    Item* item = pBag->GetItemByPos(slot);
                    uint32 itemScore = GetPetFoodScore(pet, item, spellInfo);
                    if(itemScore > 0 && itemScore < lowestScore)
                    {
                        bestFood = item;
                        lowestScore = itemScore;
                    }
                }
            }

            if (bestFood)
            {
                return bestFood;
            }
        }
    }

    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; slot++ )
    {
        itemForSpell = GetItemFitsToSpellRequirements(slot, spellInfo);
        if (itemForSpell)
        {
            return itemForSpell;
        }
    }
    return NULL;
}

uint32 ItemForSpellValue::GetPetFoodScore(Pet *pet, Item *item, SpellEntry const *spellInfo)
{
    if (!item || !pet)
    {
        return 0;
    }
    ItemPrototype const* proto = item->GetProto();
    if (!proto || !item->IsFitToSpellRequirements(spellInfo) ||
        !pet->HaveInDiet(proto) ||
        !pet->GetCurrentFoodBenefitLevel(proto->ItemLevel))
    {
        return 0;
    }
    return (proto->ItemLevel * 1000) + proto->BuyPrice;
}

Item* ItemForSpellValue::GetItemFitsToSpellRequirements(uint8 slot, SpellEntry const *spellInfo)
{
    Item* const itemForSpell = bot->GetItemByPos( INVENTORY_SLOT_BAG_0, slot );
    if (!itemForSpell || itemForSpell->GetEnchantmentId(TEMP_ENCHANTMENT_SLOT))
    {
        return NULL;
    }

    if (itemForSpell->IsFitToSpellRequirements(spellInfo))
    {
        return itemForSpell;
    }

    return NULL;
}
