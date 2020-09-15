#include "botpch.h"
#include "playerbot.h"
#include "ahbot/AhBot.h"
#include "PlayerbotFactory.h"
#include "SQLStorages.h"
#include "ItemPrototype.h"
#include "PlayerbotAIConfig.h"
#include "AccountMgr.h"
#include "DBCStore.h"
#include "SharedDefines.h"


using namespace ai;
using namespace std;

uint32 PlayerbotFactory::tradeSkills[] =
{
    SKILL_ALCHEMY,
    SKILL_ENCHANTING,
    SKILL_SKINNING,
    SKILL_TAILORING,
    SKILL_LEATHERWORKING,
    SKILL_ENGINEERING,
    SKILL_HERBALISM,
    SKILL_MINING,
    SKILL_BLACKSMITHING,
    SKILL_COOKING,
    SKILL_FIRST_AID,
    SKILL_FISHING
};

void PlayerbotFactory::Randomize()
{
    Randomize(true);
}

void PlayerbotFactory::Refresh()
{
    Prepare();
    InitEquipment(true);
    InitAmmo();
    InitFood();
    InitPotions();

    uint32 money = urand(level * 1000, level * 5 * 1000);
    if (bot->GetMoney() < money)
    {
        bot->SetMoney(money);
    }
    bot->SaveToDB();
}

void PlayerbotFactory::CleanRandomize()
{
    Randomize(false);
}

void PlayerbotFactory::Prepare()
{
    if (!itemQuality)
    {
        if (level <= 10)
        {
            itemQuality = urand(ITEM_QUALITY_NORMAL, ITEM_QUALITY_UNCOMMON);
        }
        else if (level <= 20)
        {
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_RARE);
        }
        else if (level <= 40)
        {
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_EPIC);
        }
        else if (level < 60)
        {
            itemQuality = urand(ITEM_QUALITY_UNCOMMON, ITEM_QUALITY_EPIC);
        }
        else
        {
            itemQuality = urand(ITEM_QUALITY_RARE, ITEM_QUALITY_EPIC);
        }
    }

    if (bot->IsDead())
    {
        bot->ResurrectPlayer(1.0f, false);
    }

    bot->CombatStop(true);
    bot->SetLevel(level);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_HELM);
    bot->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_HIDE_CLOAK);
}

void PlayerbotFactory::Randomize(bool incremental)
{
    Prepare();

    bot->resetTalents(true);
    ClearSpells();
    ClearInventory();
    bot->SaveToDB();

    InitQuests();
    // quest rewards boost bot level, so reduce back
    bot->SetLevel(level);
    ClearInventory();
    bot->SetUInt32Value(PLAYER_XP, 0);
    CancelAuras();
    bot->SaveToDB();

    InitAvailableSpells();
    InitSkills();
    InitTradeSkills();
    InitTalents();
    InitAvailableSpells();
    InitSpecialSpells();
    InitMounts();
    UpdateTradeSkills();
    bot->SaveToDB();

    InitEquipment(incremental);
    InitBags();
    InitAmmo();
    InitFood();
    InitPotions();
    InitSecondEquipmentSet();
    InitInventory();
    bot->SetMoney(urand(level * 1000, level * 5 * 1000));
    bot->SaveToDB();

    InitPet();
    bot->SaveToDB();
}

void PlayerbotFactory::InitPet()
{
    Pet* pet = bot->GetPet();
    if (!pet)
    {
        if (bot->getClass() != CLASS_HUNTER)
        {
            return;
        }

        Map* map = bot->GetMap();
        if (!map)
        {
            return;
        }

        vector<uint32> ids;
        for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
        {
            CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
            if (!co || !co->isTameable())
            {
                continue;
            }

            if (co->MinLevel > bot->getLevel())
            {
                continue;
            }

            PetLevelInfo const* petInfo = sObjectMgr.GetPetLevelInfo(co->Entry, bot->getLevel());
            if (!petInfo)
            {
                continue;
            }

            ids.push_back(id);
        }

        if (ids.empty())
        {
            sLog.outError("No pets available for bot %s (%d level)", bot->GetName(), bot->getLevel());
            return;
        }

        for (int i = 0; i < 100; i++)
        {
            int index = urand(0, ids.size() - 1);
            CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(ids[index]);

            PetLevelInfo const* petInfo = sObjectMgr.GetPetLevelInfo(co->Entry, bot->getLevel());
            if (!petInfo)
            {
                continue;
            }

            uint32 guid = map->GenerateLocalLowGuid(HIGHGUID_PET);
            CreatureCreatePos pos(map, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
            pet = new Pet(HUNTER_PET);
            if (!pet->Create(guid, pos, co, 0))
            {
                delete pet;
                pet = NULL;
                continue;
            }

            pet->SetOwnerGuid(bot->GetObjectGuid());
            pet->SetCreatorGuid(bot->GetObjectGuid());
            pet->setFaction(bot->getFaction());
            pet->SetLevel(bot->getLevel());
            bot->SetPet(pet);

            sLog.outDetail("Bot %s: assign pet %d (%d level)", bot->GetName(), co->Entry, bot->getLevel());
            pet->SavePetToDB(PET_SAVE_AS_CURRENT);
            break;
        }
    }

    if (!pet)
    {
        sLog.outError("Cannot create pet for bot %s", bot->GetName());
        return;
    }

    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if(itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        uint32 spellId = itr->first;
        if(IsPassiveSpell(spellId))
        {
            continue;
        }

        pet->ToggleAutocast(spellId, true);
    }
}

void PlayerbotFactory::ClearSpells()
{
    list<uint32> spells;
    for(PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        if(itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
        {
            continue;
        }

        spells.push_back(spellId);
    }

    for (list<uint32>::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        bot->removeSpell(*i);
    }
}

void PlayerbotFactory::InitSpells()
{
    for (int i = 0; i < 15; i++)
    {
        InitAvailableSpells();
    }
}

void PlayerbotFactory::InitTalents()
{
    uint32 point = urand(0, 100);
    uint8 cls = bot->getClass();
    uint32 p1 = sPlayerbotAIConfig.specProbability[cls][0];
    uint32 p2 = p1 + sPlayerbotAIConfig.specProbability[cls][1];

    uint32 specNo = (point < p1 ? 0 : (point < p2 ? 1 : 2));
    InitTalents(specNo);

    if (bot->GetFreeTalentPoints())
    {
        InitTalents(2 - specNo);
    }
}


class DestroyItemsVisitor : public IterateItemsVisitor
{
public:
    DestroyItemsVisitor(Player* bot) : IterateItemsVisitor(), bot(bot) {}

    virtual bool Visit(Item* item)
    {
        uint32 id = item->GetProto()->ItemId;
        if (CanKeep(id))
        {
            keep.insert(id);
            return true;
        }

        bot->DestroyItem(item->GetBagSlot(), item->GetSlot(), true);
        return true;
    }

private:
    bool CanKeep(uint32 id)
    {
        if (keep.find(id) != keep.end())
        {
            return false;
        }

        if (sPlayerbotAIConfig.IsInRandomQuestItemList(id))
        {
            return true;
        }

        ItemPrototype const* proto = sItemStorage.LookupEntry<ItemPrototype>(id);
        if (proto->Class == ITEM_CLASS_MISC && proto->SubClass == ITEM_SUBCLASS_JUNK)
        {
            return true;
        }

        return false;
    }

private:
    Player* bot;
    set<uint32> keep;

};

bool PlayerbotFactory::CanEquipArmor(ItemPrototype const* proto)
{
    if (bot->HasSkill(SKILL_SHIELD) && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
    {
        return true;
    }

    if (bot->HasSkill(SKILL_PLATE_MAIL))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_PLATE)
        {
            return false;
        }
    }
    else if (bot->HasSkill(SKILL_MAIL))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_MAIL)
        {
            return false;
        }
    }
    else if (bot->HasSkill(SKILL_LEATHER))
    {
        if (proto->SubClass != ITEM_SUBCLASS_ARMOR_LEATHER)
        {
            return false;
        }
    }

    if (proto->Quality <= ITEM_QUALITY_NORMAL)
    {
        return true;
    }

    uint8 sp = 0, ap = 0, tank = 0;
    for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
    {
        // for ItemStatValue != 0
        if(!proto->ItemStat[j].ItemStatValue)
        {
            continue;
        }

        AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
    }

    return CheckItemStats(sp, ap, tank);
}

bool PlayerbotFactory::CheckItemStats(uint8 sp, uint8 ap, uint8 tank)
{
    switch (bot->getClass())
    {
    case CLASS_PRIEST:
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (!sp || ap > sp || tank > sp)
        {
            return false;
        }
        break;
    case CLASS_PALADIN:
    case CLASS_WARRIOR:
        if ((!ap && !tank) || sp > ap || sp > tank)
        {
            return false;
        }
        break;
    case CLASS_HUNTER:
    case CLASS_ROGUE:
        if (!ap || sp > ap || sp > tank)
        {
            return false;
        }
        break;
    }

    return sp || ap || tank;
}

void PlayerbotFactory::AddItemStats(uint32 mod, uint8 &sp, uint8 &ap, uint8 &tank)
{
    switch (mod)
    {
        //FOEREAPER
    //case ITEM_MOD_HIT_RATING:
    //case ITEM_MOD_CRIT_RATING:
    //case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    //case ITEM_MOD_HEALTH_REGEN:
    case ITEM_MOD_MANA:
    case ITEM_MOD_INTELLECT:
    case ITEM_MOD_SPIRIT:
    //case ITEM_MOD_MANA_REGENERATION:
    //case ITEM_MOD_SPELL_POWER:
    //case ITEM_MOD_SPELL_PENETRATION:
    //case ITEM_MOD_HIT_SPELL_RATING:
    //case ITEM_MOD_CRIT_SPELL_RATING:
    //case ITEM_MOD_HASTE_SPELL_RATING:
        sp++;
        break;
    }

    switch (mod)
    {
    //case ITEM_MOD_HIT_RATING:
    //case ITEM_MOD_CRIT_RATING:
    //case ITEM_MOD_HASTE_RATING:
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    //case ITEM_MOD_HEALTH_REGEN:
    //case ITEM_MOD_DEFENSE_SKILL_RATING:
    //case ITEM_MOD_DODGE_RATING:
    //case ITEM_MOD_PARRY_RATING:
    //case ITEM_MOD_BLOCK_RATING:
    //case ITEM_MOD_HIT_TAKEN_MELEE_RATING:
    //case ITEM_MOD_HIT_TAKEN_RANGED_RATING:
    //case ITEM_MOD_HIT_TAKEN_SPELL_RATING:
    //case ITEM_MOD_CRIT_TAKEN_MELEE_RATING:
    //case ITEM_MOD_CRIT_TAKEN_RANGED_RATING:
    //case ITEM_MOD_CRIT_TAKEN_SPELL_RATING:
    //case ITEM_MOD_HIT_TAKEN_RATING:
    //case ITEM_MOD_CRIT_TAKEN_RATING:
    //case ITEM_MOD_RESILIENCE_RATING:
    //case ITEM_MOD_BLOCK_VALUE:
        tank++;
        break;
    }

    switch (mod)
    {
    case ITEM_MOD_HEALTH:
    case ITEM_MOD_STAMINA:
    //case ITEM_MOD_HEALTH_REGEN:
    case ITEM_MOD_AGILITY:
    case ITEM_MOD_STRENGTH:
    //case ITEM_MOD_HIT_MELEE_RATING:
    //case ITEM_MOD_HIT_RANGED_RATING:
    //case ITEM_MOD_CRIT_MELEE_RATING:
    //case ITEM_MOD_CRIT_RANGED_RATING:
    //case ITEM_MOD_HASTE_MELEE_RATING:
    //case ITEM_MOD_HASTE_RANGED_RATING:
    //case ITEM_MOD_HIT_RATING:
    //case ITEM_MOD_CRIT_RATING:
    //case ITEM_MOD_HASTE_RATING:
    //case ITEM_MOD_EXPERTISE_RATING:
    //case ITEM_MOD_ATTACK_POWER:
    //case ITEM_MOD_RANGED_ATTACK_POWER:
    //case ITEM_MOD_ARMOR_PENETRATION_RATING:
        ap++;
        break;
    }
}

bool PlayerbotFactory::CanEquipWeapon(ItemPrototype const* proto)
{
    switch (bot->getClass())
    {
    case CLASS_PRIEST:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE)
            return false;
        break;
    case CLASS_MAGE:
    case CLASS_WARLOCK:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_WAND &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_WARRIOR:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
            return false;
        break;
    case CLASS_PALADIN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
            return false;
        break;
    case CLASS_SHAMAN:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_DRUID:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_STAFF)
            return false;
        break;
    case CLASS_HUNTER:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD2 &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW)
            return false;
        break;
    case CLASS_ROGUE:
        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_GUN &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_CROSSBOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_BOW &&
                proto->SubClass != ITEM_SUBCLASS_WEAPON_THROWN)
            return false;
        break;
    }

    return true;
}

bool PlayerbotFactory::CanEquipItem(ItemPrototype const* proto, uint32 desiredQuality)
{
    if (proto->Duration & 0x80000000)
    {
        return false;
    }

    if (proto->Quality != desiredQuality)
    {
        return false;
    }

    if (proto->Bonding == BIND_QUEST_ITEM || proto->Bonding == BIND_WHEN_USE)
    {
        return false;
    }

    if (proto->Class == ITEM_CLASS_CONTAINER)
    {
        return true;
    }

    uint32 requiredLevel = proto->RequiredLevel;
    if (!requiredLevel)
    {
        return false;
    }

    uint32 level = bot->getLevel();
    uint32 delta = 2;
    if (level < 15)
    {
        delta = urand(7, 15);
    }
    else if (proto->Class == ITEM_CLASS_WEAPON || proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
    {
        delta = urand(2, 3);
    }
    else if (!(level % 10) || (level % 10) == 9)
    {
        delta = 2;
    }
    else if (level < 40)
    {
        delta = urand(5, 10);
    }
    else if (level < 60)
    {
        delta = urand(3, 7);
    }
    else if (level < 70)
    {
        delta = urand(2, 5);
    }
    else if (level < 80)
    {
        delta = urand(2, 4);
    }

    if (desiredQuality > ITEM_QUALITY_NORMAL &&
            (requiredLevel > level || requiredLevel < level - delta))
        return false;

    for (uint32 gap = 60; gap <= 80; gap += 10)
    {
        if (level > gap && requiredLevel <= gap)
        {
            return false;
        }
    }

    return true;
}

void PlayerbotFactory::InitEquipment(bool incremental)
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor, ITERATE_ALL_ITEMS);

    map<uint8, vector<uint32> > items;
    for(uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
        {
            continue;
        }

        uint32 desiredQuality = itemQuality;
        if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
        {
            desiredQuality--;
        }
        }

        do
        {
            for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
            {
                ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
                if (!proto)
                {
                    continue;
                }

                if (proto->Class != ITEM_CLASS_WEAPON &&
                    proto->Class != ITEM_CLASS_ARMOR &&
                    proto->Class != ITEM_CLASS_CONTAINER &&
                    proto->Class != ITEM_CLASS_PROJECTILE)
                    continue;

                if (!CanEquipItem(proto, desiredQuality))
                {
                    continue;
                }

                if (proto->Class == ITEM_CLASS_ARMOR && (
                    slot == EQUIPMENT_SLOT_HEAD ||
                    slot == EQUIPMENT_SLOT_SHOULDERS ||
                    slot == EQUIPMENT_SLOT_CHEST ||
                    slot == EQUIPMENT_SLOT_WAIST ||
                    slot == EQUIPMENT_SLOT_LEGS ||
                    slot == EQUIPMENT_SLOT_FEET ||
                    slot == EQUIPMENT_SLOT_WRISTS ||
                    slot == EQUIPMENT_SLOT_HANDS) && !CanEquipArmor(proto))
                        continue;

                if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(proto))
                {
                    continue;
                }

                if (slot == EQUIPMENT_SLOT_OFFHAND && bot->getClass() == CLASS_ROGUE && proto->Class != ITEM_CLASS_WEAPON)
                {
                    continue;
                }

                uint16 dest = 0;
                if (CanEquipUnseenItem(slot, dest, itemId))
                {
                    items[slot].push_back(itemId);
                }
            }
        } while (items[slot].empty() && desiredQuality-- > ITEM_QUALITY_NORMAL);
    }

    for(uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
        {
            continue;
        }

        vector<uint32>& ids = items[slot];
        if (ids.empty())
        {
            sLog.outDetail("%s: no items to equip for slot %d", bot->GetName(), slot);
            continue;
        }

        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];
            Item* oldItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);

            if (incremental && !IsDesiredReplacement(oldItem)) {
            {
                continue;
            }
            }

            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
            {
                continue;
            }

            if (oldItem)
            {
                bot->RemoveItem(INVENTORY_SLOT_BAG_0, slot, true);
                oldItem->DestroyForPlayer(bot);
            }

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                bot->AutoUnequipOffhandIfNeed();
                EnchantItem(newItem);
                break;
            }
        }
    }
}

bool PlayerbotFactory::IsDesiredReplacement(Item* item)
{
    if (!item)
    {
        return true;
    }

    ItemPrototype const* proto = item->GetProto();
    int delta = 1 + (80 - bot->getLevel()) / 10;
    return (int)bot->getLevel() - (int)proto->RequiredLevel > delta;
}

void PlayerbotFactory::InitSecondEquipmentSet()
{
    if (bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_WARLOCK || bot->getClass() == CLASS_PRIEST)
    {
        return;
    }

    map<uint32, vector<uint32> > items;

    uint32 desiredQuality = itemQuality;
    while (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
        desiredQuality--;
    }

    do
    {
        for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
        {
            ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
            if (!proto)
            {
                continue;
            }

            if (!CanEquipItem(proto, desiredQuality))
            {
                continue;
            }

            if (proto->Class == ITEM_CLASS_WEAPON)
            {
                if (!CanEquipWeapon(proto))
                {
                    continue;
                }

                Item* existingItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
                if (existingItem)
                {
                    switch (existingItem->GetProto()->SubClass)
                    {
                    case ITEM_SUBCLASS_WEAPON_AXE:
                    case ITEM_SUBCLASS_WEAPON_DAGGER:
                    case ITEM_SUBCLASS_WEAPON_FIST:
                    case ITEM_SUBCLASS_WEAPON_MACE:
                    case ITEM_SUBCLASS_WEAPON_SWORD:
                        if (proto->SubClass == ITEM_SUBCLASS_WEAPON_AXE || proto->SubClass == ITEM_SUBCLASS_WEAPON_DAGGER ||
                            proto->SubClass == ITEM_SUBCLASS_WEAPON_FIST || proto->SubClass == ITEM_SUBCLASS_WEAPON_MACE ||
                            proto->SubClass == ITEM_SUBCLASS_WEAPON_SWORD)
                            continue;
                        break;
                    default:
                        if (proto->SubClass != ITEM_SUBCLASS_WEAPON_AXE && proto->SubClass != ITEM_SUBCLASS_WEAPON_DAGGER &&
                            proto->SubClass != ITEM_SUBCLASS_WEAPON_FIST && proto->SubClass != ITEM_SUBCLASS_WEAPON_MACE &&
                            proto->SubClass != ITEM_SUBCLASS_WEAPON_SWORD)
                            continue;
                        break;
                    }
                }
            }
            else if (proto->Class == ITEM_CLASS_ARMOR && proto->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
            {
                if (!CanEquipArmor(proto))
                {
                    continue;
                }

                Item* existingItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_OFFHAND);
                if (existingItem && existingItem->GetProto()->SubClass == ITEM_SUBCLASS_ARMOR_SHIELD)
                {
                    continue;
                }
            }
            else
            {
                continue;
            }

            items[proto->Class].push_back(itemId);
        }
    } while (items[ITEM_CLASS_ARMOR].empty() && items[ITEM_CLASS_WEAPON].empty() && desiredQuality-- > ITEM_QUALITY_NORMAL);

    for (map<uint32, vector<uint32> >::iterator i = items.begin(); i != items.end(); ++i)
    {
        vector<uint32>& ids = i->second;
        if (ids.empty())
        {
            sLog.outDetail("%s: no items to make second equipment set for slot %d", bot->GetName(), i->first);
            continue;
        }

        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];

            Item* newItem = bot->StoreNewItemInInventorySlot(newItemId, 1);
            if (newItem)
            {
                EnchantItem(newItem);
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                break;
            }
        }
    }
}

void PlayerbotFactory::InitBags()
{
    vector<uint32> ids;

    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto || proto->Class != ITEM_CLASS_CONTAINER)
        {
            continue;
        }

        if (!CanEquipItem(proto, ITEM_QUALITY_NORMAL))
        {
            continue;
        }

        ids.push_back(itemId);
    }

    if (ids.empty())
    {
        sLog.outError("%s: no bags found", bot->GetName());
        return;
    }

    for (uint8 slot = INVENTORY_SLOT_BAG_START; slot < INVENTORY_SLOT_BAG_END; ++slot)
    {
        for (int attempts = 0; attempts < 15; attempts++)
        {
            uint32 index = urand(0, ids.size() - 1);
            uint32 newItemId = ids[index];

            uint16 dest;
            if (!CanEquipUnseenItem(slot, dest, newItemId))
            {
                continue;
            }

            Item* newItem = bot->EquipNewItem(dest, newItemId, true);
            if (newItem)
            {
                newItem->AddToWorld();
                newItem->AddToUpdateQueueOf(bot);
                break;
            }
        }
    }
}

void PlayerbotFactory::EnchantItem(Item* item)
{
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance)
    {
        return;
    }

    if (bot->getLevel() < urand(40, 50))
    {
        return;
    }

    ItemPrototype const* proto = item->GetProto();
    int32 itemLevel = proto->ItemLevel;

    vector<uint32> ids;
    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const *entry = sSpellStore.LookupEntry(id);
        if (!entry)
        {
            continue;
        }

        int32 requiredLevel = (int32)entry->baseLevel;
        if (requiredLevel && (requiredLevel > itemLevel || requiredLevel < itemLevel - 35))
        {
            continue;
        }

        if (entry->maxLevel && level > entry->maxLevel)
        {
            continue;
        }

        uint32 spellLevel = entry->spellLevel;
        if (spellLevel && (spellLevel > level || spellLevel < level - 10))
        {
            continue;
        }

        for (int j = 0; j < 3; ++j)
        {
            if (entry->Effect[j] != SPELL_EFFECT_ENCHANT_ITEM)
            {
                continue;
            }

            uint32 enchant_id = entry->EffectMiscValue[j];
            if (!enchant_id)
            {
                continue;
            }

            SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(enchant_id);
            if (!enchant || enchant->slot != PERM_ENCHANTMENT_SLOT)
            {
                continue;
            }

            uint8 sp = 0, ap = 0, tank = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (enchant->type[i] != ITEM_ENCHANTMENT_TYPE_STAT)
                {
                    continue;
                }

                AddItemStats(enchant->spellid[i], sp, ap, tank);
            }

            if (!CheckItemStats(sp, ap, tank))
            {
                continue;
            }

            if (!item->IsFitToSpellRequirements(entry))
            {
                continue;
            }

            ids.push_back(enchant_id);
        }
    }

    if (ids.empty())
    {
        sLog.outDetail("%s: no enchantments found for item %d", bot->GetName(), item->GetProto()->ItemId);
        return;
    }

    int index = urand(0, ids.size() - 1);
    uint32 id = ids[index];

    SpellItemEnchantmentEntry const* enchant = sSpellItemEnchantmentStore.LookupEntry(id);
    if (!enchant)
    {
        return;
    }

    bot->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, false);
    item->SetEnchantment(PERM_ENCHANTMENT_SLOT, id, 0, 0);
    bot->ApplyEnchantment(item, PERM_ENCHANTMENT_SLOT, true);
}

bool PlayerbotFactory::CanEquipUnseenItem(uint8 slot, uint16 &dest, uint32 item)
{
    dest = 0;
    Item *pItem = Item::CreateItem(item, 1, bot);
    if (pItem)
    {
        InventoryResult result = bot->CanEquipItem(slot, dest, pItem, true, false);
        pItem->RemoveFromUpdateQueueOf(bot);
        delete pItem;
        return result == EQUIP_ERR_OK;
    }

    return false;
}

void PlayerbotFactory::InitTradeSkills()
{
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        bot->SetSkill(tradeSkills[i], 0, 0);
    }

    vector<uint32> firstSkills;
    vector<uint32> secondSkills;
    switch (bot->getClass())
    {
    case CLASS_WARRIOR:
    case CLASS_PALADIN:
        firstSkills.push_back(SKILL_MINING);
        secondSkills.push_back(SKILL_BLACKSMITHING);
        secondSkills.push_back(SKILL_ENGINEERING);
        break;
    case CLASS_SHAMAN:
    case CLASS_DRUID:
    case CLASS_HUNTER:
    case CLASS_ROGUE:
        firstSkills.push_back(SKILL_SKINNING);
        secondSkills.push_back(SKILL_LEATHERWORKING);
        break;
    default:
        firstSkills.push_back(SKILL_TAILORING);
        secondSkills.push_back(SKILL_ENCHANTING);
    }

    SetRandomSkill(SKILL_FIRST_AID);
    SetRandomSkill(SKILL_FISHING);
    SetRandomSkill(SKILL_COOKING);

    switch (urand(0, 1))
    {
    case 0:
        SetRandomSkill(SKILL_HERBALISM);
        SetRandomSkill(SKILL_ALCHEMY);
        break;
    /*case 1:
        SetRandomSkill(SKILL_HERBALISM);
        SetRandomSkill(SKILL_INSCRIPTION);
        break;
    case 2:
        SetRandomSkill(SKILL_MINING);
        SetRandomSkill(SKILL_JEWELCRAFTING);
        break;*/
    case 1://3:
        SetRandomSkill(firstSkills[urand(0, firstSkills.size() - 1)]);
        SetRandomSkill(secondSkills[urand(0, secondSkills.size() - 1)]);
        break;
    }
}

void PlayerbotFactory::UpdateTradeSkills()
{
    for (int i = 0; i < sizeof(tradeSkills) / sizeof(uint32); ++i)
    {
        if (bot->GetSkillValue(tradeSkills[i]) == 1)
        {
            bot->SetSkill(tradeSkills[i], 0, 0);
        }
    }
}

void PlayerbotFactory::InitSkills()
{
    uint32 maxValue = level * 5;
    SetRandomSkill(SKILL_DEFENSE);
    SetRandomSkill(SKILL_SWORDS);
    SetRandomSkill(SKILL_AXES);
    SetRandomSkill(SKILL_BOWS);
    SetRandomSkill(SKILL_GUNS);
    SetRandomSkill(SKILL_MACES);
    SetRandomSkill(SKILL_2H_SWORDS);
    SetRandomSkill(SKILL_STAVES);
    SetRandomSkill(SKILL_2H_MACES);
    SetRandomSkill(SKILL_2H_AXES);
    SetRandomSkill(SKILL_DAGGERS);
    SetRandomSkill(SKILL_THROWN);
    SetRandomSkill(SKILL_CROSSBOWS);
    SetRandomSkill(SKILL_WANDS);
    SetRandomSkill(SKILL_POLEARMS);
    SetRandomSkill(SKILL_FIST_WEAPONS);

    if (bot->getLevel() >= 70)
    {
        bot->SetSkill(SKILL_RIDING, 300, 300);
    }
    else if (bot->getLevel() >= 60)
    {
        bot->SetSkill(SKILL_RIDING, 225, 225);
    }
    else if (bot->getLevel() >= 40)
    {
        bot->SetSkill(SKILL_RIDING, 150, 150);
    }
    else if (bot->getLevel() >= 20)
    {
        bot->SetSkill(SKILL_RIDING, 75, 75);
    }
    else
    {
        bot->SetSkill(SKILL_RIDING, 0, 0);
    }

    uint32 skillLevel = bot->getLevel() < 40 ? 0 : 1;
    switch (bot->getClass())
    {
    //case CLASS_DEATH_KNIGHT:
    case CLASS_WARRIOR:
    case CLASS_PALADIN:
        bot->SetSkill(SKILL_PLATE_MAIL, skillLevel, skillLevel);
        break;
    case CLASS_SHAMAN:
    case CLASS_HUNTER:
        bot->SetSkill(SKILL_MAIL, skillLevel, skillLevel);
    }
}

void PlayerbotFactory::SetRandomSkill(uint16 id)
{
    uint32 maxValue = level * 5;
    uint32 curValue = urand(maxValue - level, maxValue);
    bot->SetSkill(id, curValue, maxValue);

}

void PlayerbotFactory::InitAvailableSpells()
{
    bot->learnDefaultSpells();

    for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
    {
        CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!co)
        {
            continue;
        }

        if (co->TrainerType != TRAINER_TYPE_TRADESKILLS && co->TrainerType != TRAINER_TYPE_CLASS)
        {
            continue;
        }

        if (co->TrainerType == TRAINER_TYPE_CLASS && co->TrainerClass != bot->getClass())
        {
            continue;
        }

        uint32 trainerId = co->TrainerTemplateId;
        if (!trainerId)
        {
            trainerId = co->Entry;
        }

        TrainerSpellData const* trainer_spells = sObjectMgr.GetNpcTrainerTemplateSpells(trainerId);
        if (!trainer_spells)
        {
            trainer_spells = sObjectMgr.GetNpcTrainerSpells(trainerId);
        }

        if (!trainer_spells)
        {
            continue;
        }

        for (TrainerSpellMap::const_iterator itr =  trainer_spells->spellList.begin(); itr !=  trainer_spells->spellList.end(); ++itr)
        {
            TrainerSpell const* tSpell = &itr->second;

            if (!tSpell)
            {
                continue;
            }

            uint32 reqLevel = 0;

            reqLevel = tSpell->isProvidedReqLevel ? tSpell->reqLevel : std::max(reqLevel, tSpell->reqLevel);
            TrainerSpellState state = bot->GetTrainerSpellState(tSpell, reqLevel);
            if (state != TRAINER_SPELL_GREEN)
            {
                continue;
            }

            ai->CastSpell(tSpell->spell, bot);
        }
    }
}

void PlayerbotFactory::InitSpecialSpells()
{
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotSpellIds.begin(); i != sPlayerbotAIConfig.randomBotSpellIds.end(); ++i)
    {
        uint32 spellId = *i;
        bot->learnSpell(spellId, false);
    }
}

void PlayerbotFactory::InitTalents(uint32 specNo)
{
    uint32 classMask = bot->getClassMask();

    map<uint32, vector<TalentEntry const*> > spells;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if(!talentInfo)
        {
            continue;
        }

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry( talentInfo->TalentTab );
        if(!talentTabInfo || talentTabInfo->tabpage != specNo)
        {
            continue;
        }

        if( (classMask & talentTabInfo->ClassMask) == 0 )
        {
            continue;
        }

        spells[talentInfo->Row].push_back(talentInfo);
    }

    uint32 freePoints = bot->GetFreeTalentPoints();
    for (map<uint32, vector<TalentEntry const*> >::iterator i = spells.begin(); i != spells.end(); ++i)
    {
        vector<TalentEntry const*> &spells = i->second;
        if (spells.empty())
        {
            sLog.outError("%s: No spells for talent row %d", bot->GetName(), i->first);
            continue;
        }

        int attemptCount = 0;
        while (!spells.empty() && (int)freePoints - (int)bot->GetFreeTalentPoints() < 5 && attemptCount++ < 3 && bot->GetFreeTalentPoints())
        {
            int index = urand(0, spells.size() - 1);
            TalentEntry const *talentInfo = spells[index];
            for (int rank = 0; rank < MAX_TALENT_RANK && bot->GetFreeTalentPoints(); ++rank)
            {
                uint32 spellId = talentInfo->RankID[rank];
                if (!spellId)
                {
                    continue;
                }
                bot->learnSpell(spellId, false);
                bot->UpdateFreeTalentPoints(false);
            }
            spells.erase(spells.begin() + index);
        }

        freePoints = bot->GetFreeTalentPoints();
    }
}

ObjectGuid PlayerbotFactory::GetRandomBot()
{
    vector<ObjectGuid> guids;
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        if (!sAccountMgr.GetCharactersCount(accountId))
        {
            continue;
        }

        QueryResult *result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account` = '%u'", accountId);
        if (!result)
        {
            continue;
        }

        do
        {
            Field* fields = result->Fetch();
            ObjectGuid guid = ObjectGuid(fields[0].GetUInt64());
            if (!sObjectMgr.GetPlayer(guid))
            {
                guids.push_back(guid);
            }
        } while (result->NextRow());

        delete result;
    }

    if (guids.empty())
    {
        return ObjectGuid();
    }

    int index = urand(0, guids.size() - 1);
    return guids[index];
}

void PlayerbotFactory::InitQuests()
{
    QueryResult *results = WorldDatabase.PQuery("SELECT `entry`, `RequiredClasses`, `RequiredRaces` FROM `quest_template` WHERE `QuestLevel` = -1 and `MinLevel` <= '%u'",
            bot->getLevel());
    if (!results)
    {
        return;
    }

    list<uint32> ids;
    do
    {
        Field* fields = results->Fetch();
        uint32 questId = fields[0].GetUInt32();
        uint32 requiredClasses = fields[1].GetUInt32();
        uint32 requiredRaces = fields[2].GetUInt32();
        if ((requiredClasses & bot->getClassMask()) && (requiredRaces & bot->getRaceMask()))
        {
            ids.push_back(questId);
        }
    } while (results->NextRow());

    delete results;

    for (int i = 0; i < 15; i++)
    {
        for (list<uint32>::iterator i = ids.begin(); i != ids.end(); ++i)
        {
            uint32 questId = *i;
            Quest const *quest = sObjectMgr.GetQuestTemplate(questId);

            bot->SetQuestStatus(questId, QUEST_STATUS_NONE);

            if (!bot->SatisfyQuestClass(quest, false) ||
                    !bot->SatisfyQuestRace(quest, false) ||
                    !bot->SatisfyQuestStatus(quest, false))
                continue;

            if (quest->IsRepeatable())
            {
                continue;
            }

            bot->SetQuestStatus(questId, QUEST_STATUS_COMPLETE);
            bot->RewardQuest(quest, 0, bot, false);
            ClearInventory();
        }
    }
}

void PlayerbotFactory::ClearInventory()
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor);
}

void PlayerbotFactory::InitAmmo()
{
    if (bot->getClass() != CLASS_HUNTER && bot->getClass() != CLASS_ROGUE && bot->getClass() != CLASS_WARRIOR)
    {
        return;
    }

    Item* const pItem = bot->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_RANGED);
    if (!pItem)
    {
        return;
    }

    uint32 subClass = 0;
    switch (pItem->GetProto()->SubClass)
    {
    case ITEM_SUBCLASS_WEAPON_GUN:
        subClass = ITEM_SUBCLASS_BULLET;
        break;
    case ITEM_SUBCLASS_WEAPON_BOW:
    case ITEM_SUBCLASS_WEAPON_CROSSBOW:
        subClass = ITEM_SUBCLASS_ARROW;
        break;
    }

    if (!subClass)
    {
        return;
    }

    QueryResult *results = WorldDatabase.PQuery("select max(`entry`), max(`RequiredLevel`) from `item_template` where `class` = '%u' and `subclass` = '%u' and `RequiredLevel` <= '%u'",
            ITEM_CLASS_PROJECTILE, subClass, bot->getLevel());
    if (!results)
    {
        return;
    }

    Field* fields = results->Fetch();
    if (fields)
    {
        uint32 entry = fields[0].GetUInt32();
        for (int i = 0; i < 5; i++)
        {
            Item* newItem = bot->StoreNewItemInInventorySlot(entry, 1000);
            if (newItem)
            {
                newItem->AddToUpdateQueueOf(bot);
            }
        }
        bot->SetAmmo(entry);
    }

    delete results;
}

void PlayerbotFactory::InitMounts()
{
    map<int32, vector<uint32> > spells;

    for (uint32 spellId = 0; spellId < sSpellStore.GetNumRows(); ++spellId)
    {
        SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId);
        if (!spellInfo || spellInfo->EffectApplyAuraName[0] != SPELL_AURA_MOUNTED)
        {
            continue;
        }

        if (GetSpellCastTime(spellInfo) < 500 || GetSpellDuration(spellInfo) != -1)
        {
            continue;
        }

        int32 effect = max(spellInfo->EffectBasePoints[1], spellInfo->EffectBasePoints[2]);
        if (effect < 50)
        {
            continue;
        }

        spells[effect].push_back(spellId);
    }

    for (uint32 type = 0; type < 2; ++type)
    {
        for (map<int32, vector<uint32> >::iterator i = spells.begin(); i != spells.end(); ++i)
        {
            int32 effect = i->first;
            vector<uint32>& ids = i->second;
            uint32 index = urand(0, ids.size() - 1);
            if (index >= ids.size())
            {
                continue;
            }

            bot->learnSpell(ids[index], false);
        }
    }
}

void PlayerbotFactory::InitPotions()
{
    map<uint32, vector<uint32> > items;
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_CONSUMABLE ||
            proto->SubClass != ITEM_SUBCLASS_POTION ||
            proto->Spells[0].SpellCategory != 4 ||
            proto->Bonding != NO_BIND)
            continue;

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
        {
            continue;
        }

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
        {
            continue;
        }

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
        {
            continue;
        }

        for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; j++)
        {
            const SpellEntry* const spellInfo = sSpellStore.LookupEntry(proto->Spells[j].SpellId);
            if (!spellInfo)
            {
                continue;
            }

            for (int i = 0 ; i < 3; i++)
            {
                if (spellInfo->Effect[i] == SPELL_EFFECT_HEAL || spellInfo->Effect[i] == SPELL_EFFECT_ENERGIZE)
                {
                    items[spellInfo->Effect[i]].push_back(itemId);
                    break;
                }
            }
        }
    }

    uint32 effects[] = { SPELL_EFFECT_HEAL, SPELL_EFFECT_ENERGIZE };
    for (int i = 0; i < sizeof(effects) / sizeof(uint32); ++i)
    {
        uint32 effect = effects[i];
        vector<uint32>& ids = items[effect];
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        uint32 itemId = ids[index];
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        Item* newItem = bot->StoreNewItemInInventorySlot(itemId, urand(1, proto->GetMaxStackSize()));
        if (newItem)
        {
            newItem->AddToUpdateQueueOf(bot);
        }
   }
}

void PlayerbotFactory::InitFood()
{
    map<uint32, vector<uint32> > items;
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_CONSUMABLE ||
            proto->SubClass != ITEM_SUBCLASS_FOOD ||
            (proto->Spells[0].SpellCategory != 11 && proto->Spells[0].SpellCategory != 59) ||
            proto->Bonding != NO_BIND)
            continue;

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
        {
            continue;
        }

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
        {
            continue;
        }

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
        {
            continue;
        }

        items[proto->Spells[0].SpellCategory].push_back(itemId);
    }

    uint32 categories[] = { 11, 59 };
    for (int i = 0; i < sizeof(categories) / sizeof(uint32); ++i)
    {
        uint32 category = categories[i];
        vector<uint32>& ids = items[category];
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        uint32 itemId = ids[index];
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        Item* newItem = bot->StoreNewItemInInventorySlot(itemId, urand(1, proto->GetMaxStackSize()));
        if (newItem)
        {
            newItem->AddToUpdateQueueOf(bot);
        }
   }
}


void PlayerbotFactory::CancelAuras()
{
    bot->RemoveAllAuras();
}

void PlayerbotFactory::InitInventory()
{
    InitInventoryTrade();
    InitInventoryEquip();
    InitInventorySkill();
}

void PlayerbotFactory::InitInventorySkill()
{
    if (bot->HasSkill(SKILL_MINING)) {
    {
        StoreItem(2901, 1); // Mining Pick
    }
    }
    /*if (bot->HasSkill(SKILL_JEWELCRAFTING)) {
        StoreItem(20815, 1); // Jeweler's Kit
        StoreItem(20824, 1); // Simple Grinder
    }*/
    if (bot->HasSkill(SKILL_BLACKSMITHING) || bot->HasSkill(SKILL_ENGINEERING)) {
    {
        StoreItem(5956, 1); // Blacksmith Hammer
    }
    }
    if (bot->HasSkill(SKILL_ENGINEERING)) {
    {
        StoreItem(6219, 1); // Arclight Spanner
    }
    }
    if (bot->HasSkill(SKILL_ENCHANTING)) {
    {
        StoreItem(16207, 1); // Runed Arcanite Rod
    }
    }
    /*if (bot->HasSkill(SKILL_INSCRIPTION)) {
        StoreItem(39505, 1); // Virtuoso Inking Set
    }*/
    if (bot->HasSkill(SKILL_SKINNING)) {
    {
        StoreItem(7005, 1); // Skinning Knife
    }
    }
}

Item* PlayerbotFactory::StoreItem(uint32 itemId, uint32 count)
{
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    Item* newItem = bot->StoreNewItemInInventorySlot(itemId, min(count, proto->GetMaxStackSize()));
    if (newItem)
    {
        newItem->AddToUpdateQueueOf(bot);
    }

    return newItem;
}

void PlayerbotFactory::InitInventoryTrade()
{
    vector<uint32> ids;
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_TRADE_GOODS || proto->Bonding != NO_BIND)
        {
            continue;
        }

        if (proto->ItemLevel < bot->getLevel())
        {
            continue;
        }

        if (proto->RequiredLevel > bot->getLevel() || proto->RequiredLevel < bot->getLevel() - 10)
        {
            continue;
        }

        if (proto->RequiredSkill && !bot->HasSkill(proto->RequiredSkill))
        {
            continue;
        }

        ids.push_back(itemId);
    }

    if (ids.empty())
    {
        sLog.outError("No trade items available for bot %s (%d level)", bot->GetName(), bot->getLevel());
        return;
    }

    uint32 index = urand(0, ids.size() - 1);
    if (index >= ids.size())
    {
        return;
    }

    uint32 itemId = ids[index];
    ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
    if (!proto)
    {
        return;
    }

    uint32 count = 1, stacks = 1;
    switch (proto->Quality)
    {
    case ITEM_QUALITY_NORMAL:
        count = proto->GetMaxStackSize();
        stacks = urand(1, 7) / auctionbot.GetRarityPriceMultiplier(proto);
        break;
    case ITEM_QUALITY_UNCOMMON:
        stacks = 1;
        count = urand(1, proto->GetMaxStackSize());
        break;
    case ITEM_QUALITY_RARE:
        stacks = 1;
        count = urand(1, min(uint32(3), proto->GetMaxStackSize()));
        break;
    }

    for (uint32 i = 0; i < stacks; i++)
    {
        StoreItem(itemId, count);
    }
}

void PlayerbotFactory::InitInventoryEquip()
{
    vector<uint32> ids;

    uint32 desiredQuality = itemQuality;
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL) {
    {
        desiredQuality--;
    }
    }

    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON || (proto->Bonding == BIND_WHEN_PICKED_UP ||
                proto->Bonding == BIND_WHEN_USE))
            continue;

        if (proto->Class == ITEM_CLASS_ARMOR && !CanEquipArmor(proto))
        {
            continue;
        }

        if (proto->Class == ITEM_CLASS_WEAPON && !CanEquipWeapon(proto))
        {
            continue;
        }

        if (!CanEquipItem(proto, desiredQuality))
        {
            continue;
        }

        ids.push_back(itemId);
    }

    int maxCount = urand(0, 3);
    int count = 0;
    for (int attempts = 0; attempts < 15; attempts++)
    {
        uint32 index = urand(0, ids.size() - 1);
        if (index >= ids.size())
        {
            continue;
        }

        uint32 itemId = ids[index];
        if (StoreItem(itemId, 1) && count++ >= maxCount)
        {
            break;
        }
   }
}
