#include <algorithm>
#include "botpch.h"
#include "playerbot.h"
#include "ahbot/AhBot.h"
#include "PlayerbotFactory.h"
#include "Pet.h"
#include "SQLStorages.h"
#include "ItemPrototype.h"
#include "PlayerbotAIConfig.h"
#include "AccountMgr.h"
#include "DBCStore.h"
#include "SharedDefines.h"

using namespace ai;
using namespace std;

// List of trade skills available for player bots
uint32 PlayerbotFactory::tradeSkills[] =
{
    SKILL_ALCHEMY,
    SKILL_ENCHANTING,
    SKILL_SKINNING,
    SKILL_TAILORING,
#if !defined(CLASSIC)
    SKILL_JEWELCRAFTING,
#endif
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

/**
 * Refreshes the player bot's attributes and equipment.
 */
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

/**
 * Randomizes the player bot's attributes and equipment without incremental changes.
 */
void PlayerbotFactory::CleanRandomize()
{
    Randomize(false);
}

/**
 * Prepares the player bot for randomization by setting initial attributes and flags.
 */
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

/**
 * Randomizes the player bot's attributes and equipment.
 * @param incremental Whether to apply incremental changes.
 */
void PlayerbotFactory::Randomize(bool incremental)
{
    sLog.outDetail("Preparing to randomize...");
    Prepare();

    sLog.outDetail("Resetting player...");
    bot->resetTalents(true);
    ClearSpells();
    ClearInventory();
    bot->SaveToDB();

    sLog.outDetail("Initializing quests...");
    InitQuests();
    // quest rewards boost bot level, so reduce back
    bot->SetLevel(level);
    ClearInventory();
    bot->SetUInt32Value(PLAYER_XP, 0);
    CancelAuras();
    bot->SaveToDB();

    sLog.outDetail("Initializing spells (step 1)...");
    InitAvailableSpells();

    sLog.outDetail("Initializing skills (step 1)...");
    InitSkills();
    InitTradeSkills();

    sLog.outDetail("Initializing talents...");
    InitTalents();

    sLog.outDetail("Initializing spells (step 2)...");
    InitAvailableSpells();
    InitSpecialSpells();

    sLog.outDetail("Initializing Quest Spells...");
    InitQuestSpells();

    // A third trainer pass, and it has to be here rather than folded into the two above.
    //
    // Quest wrappers teach rank ONE of several abilities -- Sunder Armor, Intercept, Maul,
    // Searing Totem, Healing Stream Totem -- and a trainer refuses to sell rank two while
    // rank one is missing (GetTrainerSpellState rejects it on the spell_chain predecessor).
    // Both passes above ran before those quest leaves existed, so a level 60 warrior sat on
    // Sunder Armor rank 1 with ranks 2-5 available at 22/34/46/58 and unreachable, and a
    // level 60 druid on Maul rank 1 out of seven. Running the trainer once more, after the
    // quest spells are in, is what lets those chains climb.
    sLog.outDetail("Initializing spells (step 3, post-quest ranks)...");
    InitAvailableSpells();

    sLog.outDetail("Initializing mounts...");
    InitMounts();

    sLog.outDetail("Initializing skills (step 2)...");
    UpdateTradeSkills();
    bot->SaveToDB();

    sLog.outDetail("Initializing equipment...");
    InitEquipment(incremental);

    sLog.outDetail("Initializing bags...");
    InitBags();

    sLog.outDetail("Initializing ammo...");
    InitAmmo();

    sLog.outDetail("Initializing food...");
    InitFood();

    sLog.outDetail("Initializing potions...");
    InitPotions();

    sLog.outDetail("Initializing second equipment set...");
    InitSecondEquipmentSet();

    sLog.outDetail("Initializing inventory...");
    InitInventory();

    sLog.outDetail("Initializing pet...");
    InitPet();

    // Rebuild the strategy set now that the bot is a different character than when its AI
    // was created.
    //
    // CONTRACT: this DISCARDS deliberate per-bot strategy overrides -- a master's "+stay",
    // "+runaway" or "+passive" does not survive a randomize. That is intended rather than
    // overlooked: randomizing re-rolls the bot's level, talents, spellbook and gear, so it
    // is not the character those commands were given to, and carrying the old set forward is
    // what left a shaman playing a caster it no longer was. No current caller applies an
    // override adjacent to this call. If overrides ever need to survive, store them as a
    // delta and re-apply AFTER the fresh defaults -- do not restore the whole stale set.
    //
    // AiFactory picks strategies from talents and known spells, and it ran BEFORE this
    // function cleared the spellbook, re-rolled talents and re-learned everything. Without
    // this the choice is a snapshot of the bot's previous life: a shaman randomized into
    // Enhancement keeps the caster set it was given while untalented, and a druid that has
    // just learned Bear Form keeps the caster set chosen when it had none. PlayerbotAI::Reset
    // is NOT enough -- it re-initialises the existing strategies rather than re-asking
    // AiFactory which ones to have; only ResetStrategies does that.
    if (PlayerbotAI* ai = bot->GetPlayerbotAI())
    {
        ai->ResetStrategies();
    }

    sLog.outDetail("Saving to DB...");
    bot->SetMoney(urand(level * 1000, level * 5 * 1000));
    bot->SaveToDB();
    sLog.outDetail("Done.");
}

/**
 * Initializes the player bot's pet.
 */
void PlayerbotFactory::InitPet()
{
    if (bot->getClass() != CLASS_HUNTER)
    {
        return;
    }

    Pet* pet = bot->GetPet();

    // If not summoned, try loading existing pet from DB before creating a new one
    if (!pet)
    {
        Pet* loadPet = new Pet;
        if (loadPet->LoadPetFromDB(bot, 0))
        {
            pet = bot->GetPet();
            if (!pet)
            {
                delete loadPet;
            }
        }
        else
        {
            delete loadPet;
        }
    }

    if (!pet)
    {
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

            uint32 guid = map->GenerateLocalLowGuid(HIGHGUID_PET);
            uint32 pet_number = sObjectMgr.GeneratePetNumber();
            CreatureCreatePos pos(map, bot->GetPositionX(), bot->GetPositionY(), bot->GetPositionZ(), bot->GetOrientation());
            pet = new Pet(HUNTER_PET);

            if (!pet->Create(guid, pos, co, pet_number))
            {
                delete pet;
                pet = NULL;
                continue;
            }
            pet->GetCharmInfo()->SetPetNumber(pet_number, true);
            pet->SetOwnerGuid(bot->GetObjectGuid());
            pet->SetCreatorGuid(bot->GetObjectGuid());
            pet->setFaction(bot->getFaction());
            pet->SetLevel(bot->getLevel());
            pet->setPetType(HUNTER_PET);
            pet->SetCanModifyStats(true);
            pet->InitStatsForLevel(bot->getLevel());
            pet->SetUInt32Value(UNIT_FIELD_PET_NAME_TIMESTAMP, uint32(time(NULL)));
            pet->SetUInt32Value(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_NONE);
            pet->SetLoyaltyLevel(BEST_FRIEND);
            pet->ModifyLoyalty(pet->GetStartLoyaltyPoints(BEST_FRIEND));
            pet->SetUInt32Value(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP_ATTACKABLE | UNIT_FLAG_RESTING);
            pet->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_RENAME); // Allow renaming
            pet->SetPowerType(POWER_FOCUS);
            pet->SetMaxPower(POWER_HAPPINESS, pet->GetCreatePowers(POWER_HAPPINESS));
            pet->SetPower(POWER_HAPPINESS, pet->GetMaxPower(POWER_HAPPINESS) / 2);
            if (bot->IsPvP())
            {
                pet->SetPvP(true);
            }
            map->Add((Creature*)pet);
            pet->AIM_Initialize();
            pet->InitPetCreateSpells();
            pet->LearnPetPassives();
            pet->CastPetAuras(true);
            pet->SetHealth(pet->GetMaxHealth());
            pet->SetPower(POWER_FOCUS, pet->GetMaxPower(POWER_FOCUS));
            bot->SetPet(pet);
            break;
        }
    }

    if (!pet)
    {
        sLog.outError("Cannot create pet for bot %s", bot->GetName());
        return;
    }

    // Teach all pet trainer skills appropriate for this pet's level.
    QueryResult* trainerSpells = WorldDatabase.PQuery(
            "SELECT DISTINCT nt.spell"
            " FROM npc_trainer nt"
            " JOIN creature_template ct ON nt.entry = ct.Entry AND ct.TrainerType = 3"
            " WHERE nt.reqlevel <= %u",
        pet->getLevel());

    if (trainerSpells)
    {
        do
        {
            uint32 trainerSpellId = (*trainerSpells)[0].GetUInt32();
            SpellEntry const* trainerSpellInfo = sSpellStore.LookupEntry(trainerSpellId);
            if (!trainerSpellInfo)
            {
                continue;
            }

            for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (trainerSpellInfo->Effect[i] == SPELL_EFFECT_LEARN_PET_SPELL &&
                    trainerSpellInfo->EffectTriggerSpell[i])
                {
                    pet->learnSpell(trainerSpellInfo->EffectTriggerSpell[i]);
                    break;
                }
            }
        }
        while (trainerSpells->NextRow());
        delete trainerSpells;
    }

    CreatureInfo const* cInfo = pet->GetCreatureInfo();
    if (cInfo && cInfo->Family)
    {
        CreatureFamilyEntry const* cFamily = sCreatureFamilyStore.LookupEntry(cInfo->Family);
        if (cFamily && (cFamily->SkillLine[0] || cFamily->SkillLine[1]))
        {
            for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
            {
                SkillLineAbilityEntry const* slab = sSkillLineAbilityStore.LookupEntry(j);
                if (!slab || !slab->Spell)
                {
                    continue;
                }

                if (slab->SkillLine != cFamily->SkillLine[0] && slab->SkillLine != cFamily->SkillLine[1])
                {
                    continue;
                }

                SpellEntry const* spellInfo = sSpellStore.LookupEntry(slab->Spell);
                if (!spellInfo || IsPassiveSpell(spellInfo))
                {
                    continue;
                }

                if (spellInfo->SpellLevel > pet->getLevel())
                {
                    continue;
                }
                pet->learnSpell(slab->Spell);
            }
        }
    }

    for (PetSpellMap::const_iterator itr = pet->m_spells.begin(); itr != pet->m_spells.end(); ++itr)
    {
        if (itr->second.state == PETSPELL_REMOVED)
        {
            continue;
        }

        uint32 spellId = itr->first;
        if (IsPassiveSpell(spellId))
        {
            continue;
        }

        pet->ToggleAutocast(spellId, true);
    }

    pet->SavePetToDB(PET_SAVE_AS_CURRENT);
}

/**
 * Clears the player bot's spells.
 */
void PlayerbotFactory::ClearSpells()
{
    list<uint32> spells;
    for (PlayerSpellMap::iterator itr = bot->GetSpellMap().begin(); itr != bot->GetSpellMap().end(); ++itr)
    {
        uint32 spellId = itr->first;
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled || IsPassiveSpell(spellId))
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

/**
 * Initializes the player bot's spells.
 */
void PlayerbotFactory::InitSpells()
{
    for (int i = 0; i < 15; i++)
    {
        InitAvailableSpells();
    }
}

/**
 * Initializes the player bot's talents.
 */
void PlayerbotFactory::InitTalents()
{
    uint32 point = urand(0, 100);
    uint8 cls = bot->getClass();
    uint32 p1 = sPlayerbotAIConfig.specProbability[cls][0];
    uint32 p2 = p1 + sPlayerbotAIConfig.specProbability[cls][1];

    uint32 specNo = (point < p1 ? 0 : (point < p2 ? 1 : 2));

    // One pass cannot spend a full budget: InitTalents(specNo) caps itself at five points
    // and three attempts per talent row, so a level 60 with 51 points always came back with
    // most of them unspent. Those leftovers used to be poured into a DIFFERENT tree --
    // literally 2 - specNo -- and since GetPlayerSpecTab later reads whichever tree holds
    // the most points, the bot ended up being whatever the leftovers landed in rather than
    // what was rolled.
    //
    // That is why no Holy paladin has ever existed here despite a 20% roll. Two live
    // examples at the time of writing, both level 52: Lyneat and Mikkileay, each holding
    // 2 points in Holy and 11 in Retribution -- rolled Holy, spent what one pass allowed,
    // and had the rest tipped into Retribution, which is then the spec the AI honours.
    //
    // So keep filling the tree that was actually chosen, stopping when a pass places
    // nothing more, and only spill into another tree once this one genuinely cannot take
    // any more points.
    uint32 remaining = bot->GetFreeTalentPoints();
    while (remaining)
    {
        InitTalents(specNo);

        uint32 afterPass = bot->GetFreeTalentPoints();
        if (afterPass == remaining)
        {
            break;
        }

        remaining = afterPass;
    }

    // "2 - specNo" reads like the opposite tree and is not: for specNo 1 it evaluates to 1,
    // the tree the loop above has just finished filling and which therefore cannot take
    // another point, so a middle-spec bot kept its leftovers forever. Walk the other two
    // trees instead, and stop as soon as one of them accepts something.
    for (uint32 other = 0; other < 3 && bot->GetFreeTalentPoints(); ++other)
    {
        if (other == specNo)
        {
            continue;
        }

        uint32 before = bot->GetFreeTalentPoints();
        InitTalents(other);
        if (bot->GetFreeTalentPoints() != before)
        {
            break;
        }
    }
}

/**
 * Visitor class for destroying items in the player bot's inventory.
 */
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

/**
 * Checks if the player bot can equip the specified armor item.
 * @param proto The item prototype.
 * @return True if the player bot can equip the item, false otherwise.
 */
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
        if (!proto->ItemStat[j].ItemStatValue)
        {
            continue;
        }

        AddItemStats(proto->ItemStat[j].ItemStatType, sp, ap, tank);
    }

    return CheckItemStats(sp, ap, tank);
}

/**
 * Checks if the player bot's item stats are valid.
 * @param sp The spell power stat.
 * @param ap The attack power stat.
 * @param tank The tank stat.
 * @return True if the item stats are valid, false otherwise.
 */
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

/**
 * Adds item stats to the player bot.
 * @param mod The stat modifier.
 * @param sp The spell power stat.
 * @param ap The attack power stat.
 * @param tank The tank stat.
 */
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

/**
 * Checks if the player bot can equip the specified weapon item.
 * @param proto The item prototype.
 * @return True if the player bot can equip the item, false otherwise.
 */
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

/**
 * Checks if the player bot can equip the specified item.
 * @param proto The item prototype.
 * @param desiredQuality The desired quality of the item.
 * @return True if the player bot can equip the item, false otherwise.
 */
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

/**
 * Initializes the player bot's equipment.
 * @param incremental Whether to apply incremental changes.
 */
namespace
{
    typedef std::vector<ItemPrototype const*> ProtoList;

    /**
     * Every item prototype that could ever be equipped, resolved once.
     *
     * InitEquipment used to iterate 0..sItemStorage.GetMaxEntry() -- 24,283 ids -- calling
     * sObjectMgr.GetItemPrototype on each, for every equipment slot and again for every
     * quality tier it fell back through. Only 14,422 of those ids exist, so two in five
     * lookups returned nothing, and only 8,923 belong to a class that can be equipped.
     *
     * None of that filtering depends on the bot, and item prototypes are fixed once the
     * world has loaded, so it is answered once for the process and shared. What remains in
     * the per-bot loop is the part that genuinely varies: level, quality, armour type,
     * weapon type and the final CanEquipUnseenItem check.
     */
    ProtoList const& GetEquippableProtos()
    {
        static ProtoList equippable;
        static bool built = false;

        if (!built)
        {
            built = true;
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
                {
                    continue;
                }

                equippable.push_back(proto);
            }

            sLog.outString(">> [Playerbots] %u equippable item prototypes indexed for bot gearing",
                (uint32)equippable.size());
        }

        return equippable;
    }
}

void PlayerbotFactory::InitEquipment(bool incremental)
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor, ITERATE_ALL_ITEMS);

    map<uint8, vector<uint32> > items;
    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
        {
            continue;
        }

        uint32 desiredQuality = itemQuality;
        if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL)
        {
            desiredQuality--;
        }

        do
        {
            // Walk the pre-built list of equippable prototypes rather than every id from 0
            // to GetMaxEntry(). The bound is 24,283 but only 14,422 of those ids exist, so
            // two in five iterations resolved to nothing, and only 8,923 are of a class
            // that can be equipped at all -- and this loop runs once per slot and again per
            // quality tier, so the waste was multiplied roughly sixty-four times over. The
            // list is bot-independent and the prototypes do not change at runtime, so it is
            // built once for the process and shared by every bot.
            ProtoList const& equippable = GetEquippableProtos();
            for (ProtoList::const_iterator protoItr = equippable.begin(); protoItr != equippable.end(); ++protoItr)
            {
                ItemPrototype const* proto = *protoItr;
                uint32 itemId = proto->ItemId;

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
                {
                    continue;
                }

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

    for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
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

            if (incremental && !IsDesiredReplacement(oldItem))
            {
                continue;
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

/**
 * Checks if the given item is a desired replacement for the current item.
 * @param item The current item.
 * @return True if the item is a desired replacement, false otherwise.
 */
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

/**
 * Initializes the second equipment set for the player bot.
 */
void PlayerbotFactory::InitSecondEquipmentSet()
{
    // Skip for classes that do not need a second equipment set
    if (bot->getClass() == CLASS_MAGE || bot->getClass() == CLASS_WARLOCK || bot->getClass() == CLASS_PRIEST)
    {
        return;
    }

    map<uint32, vector<uint32> > items;

    uint32 desiredQuality = itemQuality;
    while (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL)
    {
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

/**
 * Initializes the bags for the player bot.
 */
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

/**
 * Enchants the given item for the player bot.
 * @param item The item to enchant.
 */
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

        int32 requiredLevel = (int32)entry->BaseLevel;
        if (requiredLevel && (requiredLevel > itemLevel || requiredLevel < itemLevel - 35))
        {
            continue;
        }

        if (entry->MaxLevel && level > entry->MaxLevel)
        {
            continue;
        }

        uint32 spellLevel = entry->SpellLevel;
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
            if (!enchant || enchant->Flags != PERM_ENCHANTMENT_SLOT)
            {
                continue;
            }

            uint8 sp = 0, ap = 0, tank = 0;
            for (int i = 0; i < 3; ++i)
            {
                if (enchant->Effect[i] != ITEM_ENCHANTMENT_TYPE_STAT)
                {
                    continue;
                }

                AddItemStats(enchant->EffectArg[i], sp, ap, tank);
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

/**
 * Checks if the player bot can equip the specified unseen item.
 * @param slot The equipment slot.
 * @param dest The destination slot.
 * @param item The item ID.
 * @return True if the player bot can equip the item, false otherwise.
 */
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
        /** case 1:
         *      SetRandomSkill(SKILL_HERBALISM);
         *      SetRandomSkill(SKILL_INSCRIPTION);
         *      break;
         *  case 2:
         *      SetRandomSkill(SKILL_MINING);
         *      SetRandomSkill(SKILL_JEWELCRAFTING);
         *      break;
         */
        case 1://3:
            SetRandomSkill(firstSkills[urand(0, firstSkills.size() - 1)]);
            SetRandomSkill(secondSkills[urand(0, secondSkills.size() - 1)]);
            break;
    }
}

/**
 * Updates the trade skills for the player bot.
 */
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

/**
 * Initializes the skills for the player bot based on its class and level.
 */
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

/**
 * Sets a random skill value for the player bot.
 * @param id The skill ID.
 */
void PlayerbotFactory::SetRandomSkill(uint16 id)
{
    uint32 maxValue = level * 5;
    uint32 curValue = urand(maxValue - level, maxValue);
    bot->SetSkill(id, curValue, maxValue);
}

/**
 * Initializes the available spells for the player bot.
 */
void PlayerbotFactory::InitAvailableSpells()
{
    // Learn default spells for the bot
    bot->learnDefaultSpells();

    // Iterate through all creature entries
    for (uint32 id = 0; id < sCreatureStorage.GetMaxEntry(); ++id)
    {
        CreatureInfo const* co = sCreatureStorage.LookupEntry<CreatureInfo>(id);
        if (!co)
        {
            continue;
        }

        // Check if the creature is a trainer for tradeskills or class
        if (co->TrainerType != TRAINER_TYPE_TRADESKILLS && co->TrainerType != TRAINER_TYPE_CLASS)
        {
            continue;
        }

        // Check if the trainer's class matches the bot's class
        if (co->TrainerType == TRAINER_TYPE_CLASS && co->TrainerClass != bot->getClass())
        {
            continue;
        }

        uint32 trainerId = co->TrainerTemplateId;
        if (!trainerId)
        {
            trainerId = co->Entry;
        }

        // Get the trainer's spells
        TrainerSpellData const* trainer_spells = sObjectMgr.GetNpcTrainerTemplateSpells(trainerId);
        if (!trainer_spells)
        {
            trainer_spells = sObjectMgr.GetNpcTrainerSpells(trainerId);
        }

        if (!trainer_spells)
        {
            continue;
        }

        // Iterate through the trainer's spells and learn them if the bot meets the requirements
        for (TrainerSpellMap::const_iterator itr = trainer_spells->spellList.begin(); itr != trainer_spells->spellList.end(); ++itr)
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

            // Bots already carrying a half-learned wrapper are NOT repaired here, and this is
            // deliberate. Accepting TRAINER_SPELL_GRAY was tried and removed. Gray is certainly
            // reachable -- Randomize runs this twice and ClearSpells keeps passives, so the
            // second pass sees what the first learned -- but by then the pass above has already
            // learned every effect, so handling gray repairs nothing. What it cannot do is fix
            // damage a bot arrived with: no reachable caller reaches this code without
            // ClearSpells first (InitSpells, which would be non-destructive, has no caller in
            // the repository at all). It also would not have been safe if wired up:
            // GetTrainerSpellState returns gray on EffectTriggerSpell[0] alone, BEFORE the
            // class, level, rank, skill and profession-cap checks, so gray is not proof of
            // eligibility to learn the rest.
            //
            // Existing damage repairs itself instead through the normal randomisation cycle,
            // which rebuilds spells from scratch and now learns every effect. Measured on this
            // server: 41 paladins missing 21084, 3 characters missing 13240, 0 bots in guilds.
            // The normal IncreaseLevel guild branch takes Refresh instead of Randomize, and the
            // `update` command calls Refresh for any bot; neither path reaches this function.
            // Guild bots do still reach it through the level-one, starter-resident and level-cap
            // paths, which run CleanRandomize. If a non-destructive repair is ever wanted, it
            // needs its own bounded entry point and its own eligibility check, not this loop.
            ai->CastSpell(tSpell->spell, bot);
            const SpellEntry* spellInfo = sSpellStore.LookupEntry(tSpell->spell);
            if (spellInfo)
            {
                for (int ei = 0; ei < MAX_EFFECT_INDEX; ++ei)
                {
                    if (spellInfo->Effect[ei] == SPELL_EFFECT_LEARN_SPELL &&
                        spellInfo->EffectTriggerSpell[ei] &&
                        !bot->HasSpell(spellInfo->EffectTriggerSpell[ei]))
                    {
                        // Every learn effect, not just the first. A trainer entry is a wrapper
                        // spell and several teach more than one thing; breaking here learned the
                        // first and silently dropped the rest, leaving a half-trained bot the
                        // trainer would never revisit -- GetTrainerSpellState only tests
                        // EffectTriggerSpell[0], so the wrapper reads as already known.
                        //
                        // Observed on paladins: wrapper 10321 at level 4 teaches Judgement
                        // (20271) AND the judgement-capable Seal of Righteousness (21084). Only
                        // Judgement was learned, so the bot kept casting it over the level-1
                        // "Non Judgement" seal 20154, whose CalculateSimpleValue carries no
                        // derived spell -- the core then reached CastSpell(..., 0, true) and
                        // logged "unknown spell id 0", 736 times across 32 bots in half an hour,
                        // each one spending Judgement's mana and 10-second cooldown for nothing.
                        // 41 paladins had 20271; not one had 21084.
                        bot->learnSpell(spellInfo->EffectTriggerSpell[ei], false);
                    }
                }
            }
        }
    }
}

/**
 * Initializes special spells for the player bot.
 */
void PlayerbotFactory::InitSpecialSpells()
{
    // Iterate through the list of random bot spell IDs and learn each spell
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotSpellIds.begin(); i != sPlayerbotAIConfig.randomBotSpellIds.end(); ++i)
    {
        uint32 spellId = *i;
        bot->learnSpell(spellId, false);
    }
}

/**
 * Learn everything a wrapper spell teaches, rather than the one leaf someone remembered.
 *
 * A quest that rewards an ability rewards a WRAPPER spell, and several teach more than one
 * thing. Listing the leaves by hand here is how bots ended up with Bear Form but no Growl
 * and no Maul -- a druid that shapeshifted and then had no attack in the form. Asking the
 * wrapper what it teaches cannot drift from the data the way a hand-kept list does, and it
 * picks up any leaf a future data change adds.
 *
 * This is the same defect, and the same shape of fix, as the trainer loop in
 * InitAvailableSpells: both learned only the first SPELL_EFFECT_LEARN_SPELL and stopped.
 */
static void LearnWrapperSpells(Player* bot, uint32 wrapperId)
{
    const SpellEntry* wrapper = sSpellStore.LookupEntry(wrapperId);
    if (!wrapper)
    {
        return;
    }

    for (int ei = 0; ei < MAX_EFFECT_INDEX; ++ei)
    {
        if (wrapper->Effect[ei] == SPELL_EFFECT_LEARN_SPELL &&
            wrapper->EffectTriggerSpell[ei] &&
            !bot->HasSpell(wrapper->EffectTriggerSpell[ei]))
        {
            bot->learnSpell(wrapper->EffectTriggerSpell[ei], false);
        }
    }
}

/**
 * Initializes quest-reward and auto-learned spells for the player bot.
 * These are class-specific spells that aren't learned from trainers.
 */
void PlayerbotFactory::InitQuestSpells()
{
    uint8 cls = bot->getClass();
    uint32 level = bot->getLevel();

    switch (cls)
    {
        case CLASS_HUNTER:
            if (level >= 10)
            {
                if (!bot->HasSpell(883))   bot->learnSpell(883, false);   // Call Pet
                if (!bot->HasSpell(982))   bot->learnSpell(982, false);   // Revive Pet
                if (!bot->HasSpell(1515))  bot->learnSpell(1515, false);  // Tame Beast
                if (!bot->HasSpell(6991))  bot->learnSpell(6991, false);  // Feed Pet
                if (!bot->HasSpell(5149))  bot->learnSpell(5149, false);  // Beast Training
            }
            if (level >= 12)
            {
                if (!bot->HasSpell(136))   bot->learnSpell(136, false);   // Mend Pet
            }
            break;

        case CLASS_WARLOCK:
            if (level >= 1)
            {
                if (!bot->HasSpell(688))   bot->learnSpell(688, false);   // Summon Imp
            }
            if (level >= 10)
            {
                if (!bot->HasSpell(697))   bot->learnSpell(697, false);   // Summon Voidwalker
            }
            if (level >= 20)
            {
                if (!bot->HasSpell(712))   bot->learnSpell(712, false);   // Summon Succubus
            }
            if (level >= 30)
            {
                if (!bot->HasSpell(691))   bot->learnSpell(691, false);   // Summon Felhunter
            }
            break;

        case CLASS_WARRIOR:
            if (level >= 10)
            {
                // Wrapper 8121 teaches Defensive Stance (71), Sunder Armor (7386) AND
                // Taunt (355). Only the stance was learned, so a protection warrior held
                // no threat abilities whatsoever -- and its Devastate -> Sunder Armor
                // fallback resolved to nothing, leaving white attacks as its only damage.
                LearnWrapperSpells(bot, 8121);
                if (!bot->HasSpell(71))    bot->learnSpell(71, false);    // Defensive Stance
            }
            if (level >= 30)
            {
                // Wrapper 8616 teaches Berserker Stance (2458) and Intercept (20252).
                LearnWrapperSpells(bot, 8616);
                if (!bot->HasSpell(2458))  bot->learnSpell(2458, false);  // Berserker Stance
            }
            break;

        case CLASS_DRUID:
            if (level >= 10)
            {
                // Wrapper 19179 teaches Bear Form (5487), Growl (6795) AND Maul (6807).
                // Learning only the form is why bears had no attack and no taunt: the bot
                // shapeshifted correctly and then had nothing to do in the form.
                LearnWrapperSpells(bot, 19179);
                if (!bot->HasSpell(5487))  bot->learnSpell(5487, false);  // Bear Form
            }
            if (level >= 16)
            {
                if (!bot->HasSpell(1066))  bot->learnSpell(1066, false);  // Aquatic Form
            }
            if (level >= 20)
            {
                if (!bot->HasSpell(768))   bot->learnSpell(768, false);   // Cat Form
            }
            if (level >= 30)
            {
                if (!bot->HasSpell(783))   bot->learnSpell(783, false);   // Travel Form
            }
            if (level >= 40)
            {
                if (!bot->HasSpell(9634))  bot->learnSpell(9634, false);  // Dire Bear Form
            }
            break;

        case CLASS_PALADIN:
            if (level >= 12)
            {
                if (!bot->HasSpell(7328))  bot->learnSpell(7328, false);  // Redemption
            }
            if (level >= 40)
            {
                if (!bot->HasSpell(13819)) bot->learnSpell(13819, false); // Summon Warhorse
            }
            if (level >= 60)
            {
                if (!bot->HasSpell(23214)) bot->learnSpell(23214, false); // Summon Charger
            }
            // Horde-specific
            if (bot->GetTeam() == HORDE && level >= 50)
            {
                if (!bot->HasSpell(31892)) bot->learnSpell(31892, false); // Seal of Blood
            }
            break;

        case CLASS_SHAMAN:
            // Totem quests - these vary by race/level but add common ones
            if (level >= 10)
            {
                if (!bot->HasSpell(8012))
                {
                    bot->learnSpell(8012, false);  // Purge (often quest)
                }

                // Wrapper 2075 teaches Searing Totem (3599), which no code path learned at
                // all -- so a shaman's damage totem simply never existed. spell_chain makes
                // 3599 the first rank, so without it no later rank can be trained either.
                LearnWrapperSpells(bot, 2075);
                if (!bot->HasSpell(3599))  bot->learnSpell(3599, false);  // Searing Totem
            }
            if (level >= 20)
            {
                // Wrapper 5396 teaches Healing Stream Totem (5394), likewise never learned,
                // and likewise the first rank of its chain.
                LearnWrapperSpells(bot, 5396);
                if (!bot->HasSpell(5394))  bot->learnSpell(5394, false);  // Healing Stream Totem
            }
            break;
        case CLASS_PRIEST:
            break;
        case CLASS_ROGUE:
            break;
        case CLASS_MAGE:
            break;
        default:
            break;
    }
}

/**
 * Initializes the talents for the player bot based on the specified specialization number.
 * @param specNo The specialization number.
 */
void PlayerbotFactory::InitTalents(uint32 specNo)
{
    uint32 classMask = bot->getClassMask();

    // Map to store spells by talent row
    map<uint32, vector<TalentEntry const*> > spells;
    for (uint32 i = 0; i < sTalentStore.GetNumRows(); ++i)
    {
        TalentEntry const *talentInfo = sTalentStore.LookupEntry(i);
        if (!talentInfo)
        {
            continue;
        }

        TalentTabEntry const *talentTabInfo = sTalentTabStore.LookupEntry( talentInfo->TalentTab );
        if (!talentTabInfo || talentTabInfo->OrderIndex != specNo)
        {
            continue;
        }

        if ( (classMask & talentTabInfo->ClassMask) == 0 )
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

/**
 * Retrieves a random bot from the list of random bot accounts.
 * @return The GUID of the random bot.
 */
ObjectGuid PlayerbotFactory::GetRandomBot()
{
    vector<ObjectGuid> guids;
    // Iterate through the list of random bot accounts
    for (list<uint32>::iterator i = sPlayerbotAIConfig.randomBotAccounts.begin(); i != sPlayerbotAIConfig.randomBotAccounts.end(); i++)
    {
        uint32 accountId = *i;
        // Check if the account has any characters
        if (!sAccountMgr.GetCharactersCount(accountId))
        {
            continue;
        }

        // Query the database for character GUIDs associated with the account
        QueryResult *result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `account` = '%u'", accountId);
        if (!result)
        {
            continue;
        }

        // Add the character GUIDs to the list if they are not already in use
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

    // Return a random GUID from the list
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

/**
 * Clears the inventory of the player bot.
 */
void PlayerbotFactory::ClearInventory()
{
    DestroyItemsVisitor visitor(bot);
    IterateItems(&visitor);
}

/**
 * Initializes the ammo for the player bot.
 */
void PlayerbotFactory::InitAmmo()
{
    // Check if the bot's class requires ammo
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
    // Determine the type of ammo required based on the ranged weapon
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

    // Query the database for the highest level ammo that the bot can use
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
        // Add the ammo to the bot's inventory
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

/**
 * Initializes the mounts for the player bot.
 */
void PlayerbotFactory::InitMounts()
{
    // This used to group every SPELL_AURA_MOUNTED spell by speed and then learn one from
    // EVERY group, twice over, with no check of level, class, race or riding skill. Two
    // consequences, both confirmed against the live character_spell table: all 200 bots
    // knew 3363 Summon Riding Gryphon -- a 499% flyer, the sole member of its speed group,
    // on characters as low as level 1 -- and class mounts leaked across classes, with the
    // Paladin Warhorse and Warlock Felsteed turning up on priests, rogues and druids.
    //
    // Riding skill is the real gate and InitSkills has already set it from the bot's level:
    // 75 buys the 60% mount at 40, 150 buys the 100% mount at 60.
    uint32 riding = bot->GetSkillValue(SKILL_RIDING);
    if (riding < 75)
    {
        return;
    }

    // Effect base points carry speed-minus-one, so a 60% mount reads 59 and a 100% reads 99.
    // Anything above that is a flying mount, which 1.12 does not have.
    const int32 maxIncrease = (riding >= 150) ? 99 : 59;

    const uint32 raceMask = bot->getRaceMask();
    const uint32 classMask = bot->getClassMask();

    map<int32, vector<uint32> > spells;

    for (uint32 spellId = 0; spellId < sSpellStore.GetNumRows(); ++spellId)
    {
        SpellEntry const *spellInfo = sSpellStore.LookupEntry(spellId);
        if (!spellInfo || spellInfo->EffectAura[0] != SPELL_AURA_MOUNTED)
        {
            continue;
        }

        if (GetSpellCastTime(spellInfo) < 500 || GetSpellDuration(spellInfo) != -1)
        {
            continue;
        }

        int32 effect = max(spellInfo->EffectBasePoints[1], spellInfo->EffectBasePoints[2]);
        if (effect < 50 || effect > maxIncrease)
        {
            continue;
        }

        // Class mounts carry a SkillLineAbility row that names the class -- Warhorse is
        // classMask 2, Felsteed is 256 -- so where one exists it must be honoured. Racial
        // and vendor mounts have no row at all in 1.12 (Brown Horse 458 and the rest), so
        // an absent row cannot mean "forbidden" or every bot would end up on foot.
        SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(spellId);
        bool restricted = false;
        bool permitted = false;
        for (SkillLineAbilityMap::const_iterator i = bounds.first; i != bounds.second; ++i)
        {
            SkillLineAbilityEntry const* ability = i->second;
            if (!ability->RaceMask && !ability->ClassMask)
            {
                continue;
            }

            restricted = true;
            if (ability->RaceMask && !(ability->RaceMask & raceMask))
            {
                continue;
            }
            if (ability->ClassMask && !(ability->ClassMask & classMask))
            {
                continue;
            }

            permitted = true;
            break;
        }

        if (restricted && !permitted)
        {
            continue;
        }

        spells[effect].push_back(spellId);
    }

    // One mount, at the best speed this bot has the skill for -- not one from every tier.
    if (spells.empty())
    {
        return;
    }

    vector<uint32>& ids = spells.rbegin()->second;
    if (!ids.empty())
    {
        bot->learnSpell(ids[urand(0, ids.size() - 1)], false);
    }
}

/**
 * Initializes the potions for the player bot.
 */
void PlayerbotFactory::InitPotions()
{
    map<uint32, vector<uint32> > items;
    // Iterate through all item entries and find potions that the bot can use
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

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
        {
            continue;
        }

        // Add the potion to the list of items
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

    // Add a random potion to the bot's inventory
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

/**
 * Initializes the food for the player bot.
 */
void PlayerbotFactory::InitFood()
{
    map<uint32, vector<uint32> > items;
    // Iterate through all item entries and find food that the bot can use
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

        if (proto->Area || proto->Map || proto->RequiredCityRank || proto->RequiredHonorRank)
        {
            continue;
        }

        items[proto->Spells[0].SpellCategory].push_back(itemId);
    }

    // Add a random food item to the bot's inventory
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

/**
 * Cancels all auras on the player bot.
 */
void PlayerbotFactory::CancelAuras()
{
    bot->RemoveAllAuras();
}

/**
 * Initializes the inventory for the player bot.
 */
void PlayerbotFactory::InitInventory()
{
    // Reagents first, so a full bag of random trade goods cannot crowd out the one item that
    // makes a wired-up ability actually castable.
    InitInventoryReagents();
    InitInventoryTrade();
    InitInventoryEquip();
    InitInventorySkill();
}

/**
 * @brief Supplies the reagents a bot's own spells require.
 *
 * A registered action whose spell needs an item the bot never receives is only a quieter kind
 * of dead end: it resolves, passes its usefulness test, and is then refused by CanCastSpell
 * every time. Druid Rebirth is the case that prompted this -- "party member dead" routes to
 * it correctly, but a factory-built druid carried none of the seeds it consumes, so no bot
 * druid could resurrect anyone.
 *
 * The reagent is read from the spell the bot ACTUALLY knows rather than mapped from its
 * level. Rank and level are not interchangeable here: a bot can be past a rank's level and
 * still not know it, and the DBC is the only thing that knows which seed a given rank eats.
 */
bool PlayerbotFactory::ProvisionSpellReagent(uint32 spellId, uint32 desiredStock)
{
    if (!bot->HasSpell(spellId))
    {
        return false;
    }

    const SpellEntry* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return false;
    }

    // Known, but consumes nothing. Still counts as handled so the caller stops looking at
    // weaker ranks.
    if (!spellInfo->Reagent[0] || !spellInfo->ReagentCount[0])
    {
        return true;
    }

    // Never fewer than one cast's worth, and normally a working stock. TakeReagents consumes
    // the item on every successful cast and Refresh does not replenish, so provisioning the
    // DBC quantity alone buys exactly one use for the bot's entire life. StoreItem clamps to
    // the item's own maximum stack size.
    const uint32 count = std::max(desiredStock, (uint32)spellInfo->ReagentCount[0]);

    if (!bot->HasItemCount(spellInfo->Reagent[0], count))
    {
        StoreItem(spellInfo->Reagent[0], count);
    }

    return true;
}

void PlayerbotFactory::InitInventoryReagents()
{
    // Highest known rank first: that is the one the bot will actually cast, and its reagent
    // is read from the DBC rather than guessed from the bot's level. Rank and level are not
    // interchangeable -- a bot can be past a rank's level and still not know it.
    static const uint32 rebirthRanks[]  = { 20748, 20747, 20742, 20739, 20484 };
    static const uint32 vanishRanks[]   = { 1857, 1856 };
    static const uint32 arcaneBrill[]   = { 23028 };
    static const uint32 gBlessMight[]   = { 25916, 25782 };
    static const uint32 gBlessWisdom[]  = { 25918, 25894 };
    static const uint32 waterBreathing[] = { 131 };
    static const uint32 waterWalking[]   = { 546 };

    struct ReagentSpell
    {
        uint8 cls;
        const uint32* ranks;
        size_t count;
        uint32 stock;
    };

    // Stock reflects how the ability is used. Vanish and Rebirth are consumed in ordinary
    // play and are wired into combat and party behaviour, so they carry a working stack;
    // the group buffs are cast rarely and degrade to their lesser versions, so they carry
    // fewer.
    static const ReagentSpell reagentSpells[] =
    {
        { CLASS_DRUID,   rebirthRanks, sizeof(rebirthRanks) / sizeof(uint32), 20 },
        { CLASS_ROGUE,   vanishRanks,  sizeof(vanishRanks)  / sizeof(uint32), 20 },
        { CLASS_MAGE,    arcaneBrill,  sizeof(arcaneBrill)  / sizeof(uint32), 10 },
        { CLASS_PALADIN, gBlessMight,  sizeof(gBlessMight)  / sizeof(uint32), 10 },
        { CLASS_PALADIN, gBlessWisdom, sizeof(gBlessWisdom) / sizeof(uint32), 10 },
        // Shaman utility. Both are registered actions with live triggers, and both consume
        // class-15 reagent items that InitInventoryTrade cannot supply, so without this they
        // were wired up and permanently uncastable.
        { CLASS_SHAMAN,  waterBreathing, sizeof(waterBreathing) / sizeof(uint32), 10 },
        { CLASS_SHAMAN,  waterWalking,   sizeof(waterWalking)   / sizeof(uint32), 10 }
    };

    const uint8 cls = bot->getClass();

    for (size_t i = 0; i < sizeof(reagentSpells) / sizeof(reagentSpells[0]); ++i)
    {
        if (reagentSpells[i].cls != cls)
        {
            continue;
        }

        for (size_t r = 0; r < reagentSpells[i].count; ++r)
        {
            if (ProvisionSpellReagent(reagentSpells[i].ranks[r], reagentSpells[i].stock))
            {
                break;
            }
        }
    }
}

/**
 * Initializes the skill-related items in the player bot's inventory.
 */
void PlayerbotFactory::InitInventorySkill()
{
    if (bot->HasSkill(SKILL_MINING))
    {
        StoreItem(2901, 1); // Mining Pick
    }
#if !defined(CLASSIC)
    if (bot->HasSkill(SKILL_JEWELCRAFTING))
    {
        StoreItem(20815, 1); // Jeweler's Kit
        StoreItem(20824, 1); // Simple Grinder
    }
#endif
    if (bot->HasSkill(SKILL_BLACKSMITHING) || bot->HasSkill(SKILL_ENGINEERING))
    {
        StoreItem(5956, 1); // Blacksmith Hammer
    }
    if (bot->HasSkill(SKILL_ENGINEERING))
    {
        StoreItem(6219, 1); // Arclight Spanner
    }
    if (bot->HasSkill(SKILL_ENCHANTING))
    {
        StoreItem(16207, 1); // Runed Arcanite Rod
    }
    /** if (bot->HasSkill(SKILL_INSCRIPTION)) {
     *      StoreItem(39505, 1); // Virtuoso Inking Set
     *  }
     */
    if (bot->HasSkill(SKILL_SKINNING))
    {
        StoreItem(7005, 1); // Skinning Knife
    }
}

/**
 * Stores an item in the player bot's inventory.
 * @param itemId The item ID.
 * @param count The quantity of the item.
 * @return The stored item.
 */
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

/**
 * Initializes the trade-related items in the player bot's inventory.
 */
void PlayerbotFactory::InitInventoryTrade()
{
    vector<uint32> ids;
    // Iterate through all item entries and find trade goods that the bot can use
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

    // Add a random trade good item to the bot's inventory
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

/**
 * Initializes the equipment for the player bot.
 */
void PlayerbotFactory::InitInventoryEquip()
{
    vector<uint32> ids;

    // Determine the desired quality of items
    uint32 desiredQuality = itemQuality;
    if (urand(0, 100) < 100 * sPlayerbotAIConfig.randomGearLoweringChance && desiredQuality > ITEM_QUALITY_NORMAL)
    {
        desiredQuality--;
    }

    // Iterate through all item entries and find items that the bot can equip
    for (uint32 itemId = 0; itemId < sItemStorage.GetMaxEntry(); ++itemId)
    {
        ItemPrototype const* proto = sObjectMgr.GetItemPrototype(itemId);
        if (!proto)
        {
            continue;
        }

        // Check if the item is armor or weapon and if it can be equipped by the bot
        if (proto->Class != ITEM_CLASS_ARMOR && proto->Class != ITEM_CLASS_WEAPON || (proto->Bonding == BIND_WHEN_PICKED_UP ||
            proto->Bonding == BIND_WHEN_USE))
        {
            continue;
        }

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

    // Add a random number of items to the bot's inventory
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
