#include "botpch.h"
#include "../../playerbot.h"
#include "ItemUsageValue.h"

using namespace ai;

ItemUsage ItemUsageValue::Calculate()
{
    uint32 itemId = atoi(qualifier.c_str());
    if (!itemId)
    {
        return ITEM_USAGE_NONE;
    }

    const ItemPrototype* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto)
    {
        return ITEM_USAGE_NONE;
    }

    if (IsItemUsefulForSkill(proto))
    {
        return ITEM_USAGE_SKILL;
    }

    switch (proto->Class)
    {
    case ITEM_CLASS_KEY:
    case ITEM_CLASS_CONSUMABLE:
        return ITEM_USAGE_USE;
    }

    return QueryItemUsageForEquip(proto);
}

ItemUsage ItemUsageValue::QueryItemUsageForEquip(ItemPrototype const * item)
{
    if (bot->CanUseItem(item) != EQUIP_ERR_OK)
    {
        return ITEM_USAGE_NONE;
    }

    if (item->InventoryType == INVTYPE_NON_EQUIP)
    {
        return ITEM_USAGE_NONE;
    }

    Item *pItem = Item::CreateItem(item->ItemId, 1, bot);
    if (!pItem)
    {
        return ITEM_USAGE_NONE;
    }

    uint16 dest;
    InventoryResult result = bot->CanEquipItem(NULL_SLOT, dest, pItem, true, false);
    pItem->RemoveFromUpdateQueueOf(bot);
    delete pItem;

    if ( result != EQUIP_ERR_OK )
    {
        return ITEM_USAGE_NONE;
    }

    Item* existingItem = bot->GetItemByPos(dest);
    if (!existingItem)
    {
        return ITEM_USAGE_EQUIP;
    }

    const ItemPrototype* oldItem = existingItem->GetProto();
    if (oldItem->ItemLevel < item->ItemLevel && oldItem->ItemId != item->ItemId)
    {
        switch (item->Class)
        {
        case ITEM_CLASS_ARMOR:
            if (oldItem->SubClass <= item->SubClass) {
            {
                return ITEM_USAGE_REPLACE;
            }
            }
            break;
        default:
            return ITEM_USAGE_EQUIP;
        }
    }

    return ITEM_USAGE_NONE;
}

static bool IsClothMaterial(uint32 itemId, uint32 botSkill)
{
    static std::map<uint32, uint32> firstAidMaterials;
    static bool initialized = false;
    if (!initialized)
    {
        initialized = true;
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
            if (!entry || entry->skillId != SKILL_FIRST_AID)
                continue;
            SpellEntry const* spell = sSpellStore.LookupEntry(entry->spellId);
            if (!spell)
                continue;
            for (int r = 0; r < MAX_SPELL_REAGENTS; ++r)
            {
                if (spell->Reagent[r] <= 0)
                    continue;
                uint32 reagentId = (uint32)spell->Reagent[r];
                uint32 greyAt = entry->max_value;
                auto it = firstAidMaterials.find(reagentId);
                if (it == firstAidMaterials.end() || greyAt > it->second)
                    firstAidMaterials[reagentId] = greyAt;
            }
        }
    }
    auto it = firstAidMaterials.find(itemId);
    if (it == firstAidMaterials.end())
        return false;
    return it->second == 0 || botSkill < it->second;
}

static bool IsSkillMaterial(uint32 skillId, uint32 itemId)
{
    static std::map<uint32,std::set<uint32>> skillMaterials;
    if (skillMaterials[skillId].size()==0)
    {
        for (uint32 i = 0; i < sSkillLineAbilityStore.GetNumRows(); ++i)
        {
            SkillLineAbilityEntry const* entry = sSkillLineAbilityStore.LookupEntry(i);
            if (!entry || entry->skillId != skillId)
                continue;
            SpellEntry const* spell = sSpellStore.LookupEntry(entry->spellId);
            if (!spell)
                continue;
            for (int r = 0; r < MAX_SPELL_REAGENTS; ++r)
            {
                if (spell->Reagent[r] <= 0)
                    continue;
                skillMaterials[skillId].insert((uint32)spell->Reagent[r]);
            }
        }
    }
    return skillMaterials[skillId].count(itemId) > 0;
}

bool ItemUsageValue::IsItemUsefulForSkill(ItemPrototype const * proto)
{
    switch (proto->Class)
    {
    case ITEM_CLASS_TRADE_GOODS:
        switch (proto->SubClass)
        {
        case ITEM_SUBCLASS_PARTS:
        case ITEM_SUBCLASS_EXPLOSIVES:
        case ITEM_SUBCLASS_DEVICES:
            return bot->HasSkill(SKILL_ENGINEERING);
        }
        if (bot->HasSkill(SKILL_FIRST_AID) && IsClothMaterial(proto->ItemId, bot->GetSkillValue(SKILL_FIRST_AID)))
            return true;
        if (bot->HasSkill(SKILL_HERBALISM) && IsSkillMaterial(SKILL_ALCHEMY, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_ALCHEMY) && IsSkillMaterial(SKILL_ALCHEMY, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_TAILORING) && IsSkillMaterial(SKILL_TAILORING, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_SKINNING) && IsSkillMaterial(SKILL_LEATHERWORKING, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_LEATHERWORKING) && IsSkillMaterial(SKILL_LEATHERWORKING, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_MINING) && IsSkillMaterial(SKILL_MINING, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_BLACKSMITHING) && IsSkillMaterial(SKILL_BLACKSMITHING, proto->ItemId))
            return true;
        if (bot->HasSkill(SKILL_ENGINEERING) && IsSkillMaterial(SKILL_ENGINEERING, proto->ItemId))
            return true;
        break;
    case ITEM_CLASS_RECIPE:
        {
            if (bot->HasSpell(proto->Spells[2].SpellId))
            {
                break;
            }

            switch (proto->SubClass)
            {
            case ITEM_SUBCLASS_LEATHERWORKING_PATTERN:
                return bot->HasSkill(SKILL_LEATHERWORKING);
            case ITEM_SUBCLASS_TAILORING_PATTERN:
                return bot->HasSkill(SKILL_TAILORING);
            case ITEM_SUBCLASS_ENGINEERING_SCHEMATIC:
                return bot->HasSkill(SKILL_ENGINEERING);
            case ITEM_SUBCLASS_BLACKSMITHING:
                return bot->HasSkill(SKILL_BLACKSMITHING);
            case ITEM_SUBCLASS_COOKING_RECIPE:
                return bot->HasSkill(SKILL_COOKING);
            case ITEM_SUBCLASS_ALCHEMY_RECIPE:
                return bot->HasSkill(SKILL_ALCHEMY);
            case ITEM_SUBCLASS_FIRST_AID_MANUAL:
                return bot->HasSkill(SKILL_FIRST_AID);
            case ITEM_SUBCLASS_ENCHANTING_FORMULA:
                return bot->HasSkill(SKILL_ENCHANTING);
            case ITEM_SUBCLASS_FISHING_MANUAL:
                return bot->HasSkill(SKILL_FISHING);
            }
        }
    }
    return false;
}
