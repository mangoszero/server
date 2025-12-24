#pragma once

#include "strategy/actions/InventoryAction.h"

class Player;
class PlayerbotMgr;
class ChatHandler;

using namespace std;
using ai::InventoryAction;

/**
 * @brief Factory class for creating and managing player bots.
 * This class provides methods to initialize and randomize player bots, including their equipment, skills, spells, and inventory.
 */
class PlayerbotFactory : public InventoryAction
{
public:
    /**
     * @brief Constructor for PlayerbotFactory.
     * @param bot Pointer to the player bot.
     * @param level The level of the player bot.
     * @param itemQuality The quality of items to be equipped by the player bot.
     */
    PlayerbotFactory(Player* bot, uint32 level, uint32 itemQuality = 0) :
        bot(bot), level(level), itemQuality(itemQuality), InventoryAction(bot->GetPlayerbotAI(), "factory") {}

    /**
     * @brief Gets a random bot's GUID.
     * @return The GUID of a random bot.
     */
    static ObjectGuid GetRandomBot();

    /**
     * @brief Cleans and randomizes the player bot.
     */
    void CleanRandomize();

    /**
     * @brief Randomizes the player bot.
     */
    void Randomize();

    /**
     * @brief Refreshes the player bot.
     */
    void Refresh();

private:
    /**
     * @brief Randomizes the player bot with an option for incremental changes.
     * @param incremental If true, randomizes incrementally.
     */
    void Randomize(bool incremental);

    /**
     * @brief Prepares the player bot for randomization.
     */
    void Prepare();

    /**
     * @brief Initializes the second equipment set for the player bot.
     */
    void InitSecondEquipmentSet();

    /**
     * @brief Initializes the equipment for the player bot.
     * @param incremental If true, initializes incrementally.
     */
    void InitEquipment(bool incremental);

    /**
     * @brief Checks if the player bot can equip a given item.
     * @param proto Pointer to the item prototype.
     * @param desiredQuality The desired quality of the item.
     * @return True if the item can be equipped, false otherwise.
     */
    bool CanEquipItem(ItemPrototype const* proto, uint32 desiredQuality);

    /**
     * @brief Checks if the player bot can equip an unseen item.
     * @param slot The slot to equip the item in.
     * @param dest The destination slot.
     * @param item The item ID.
     * @return True if the item can be equipped, false otherwise.
     */
    bool CanEquipUnseenItem(uint8 slot, uint16 &dest, uint32 item);

    /**
     * @brief Initializes the skills for the player bot.
     */
    void InitSkills();

    /**
     * @brief Initializes the trade skills for the player bot.
     */
    void InitTradeSkills();

    /**
     * @brief Updates the trade skills for the player bot.
     */
    void UpdateTradeSkills();

    /**
     * @brief Sets a random skill for the player bot.
     * @param id The skill ID.
     */
    void SetRandomSkill(uint16 id);

    /**
     * @brief Initializes the spells for the player bot.
     */
    void InitSpells();

    /**
     * @brief Clears the spells for the player bot.
     */
    void ClearSpells();

    /**
     * @brief Initializes the available spells for the player bot.
     */
    void InitAvailableSpells();

    /**
     * @brief Initializes the special spells for the player bot.
     */
    void InitSpecialSpells();

    /**
     * @brief Initializes spells not taught by trainers.
     */
    void InitQuestSpells();

    /**
     * @brief Initializes the talents for the player bot.
     */
    void InitTalents();

    /**
     * @brief Initializes the talents for the player bot based on a specific spec.
     * @param specNo The spec number.
     */
    void InitTalents(uint32 specNo);

    /**
     * @brief Initializes the quests for the player bot.
     */
    void InitQuests();

    /**
     * @brief Initializes the pet for the player bot.
     */
    void InitPet();

    /**
     * @brief Clears the inventory of the player bot.
     */
    void ClearInventory();

    /**
     * @brief Initializes the ammo for the player bot.
     */
    void InitAmmo();

    /**
     * @brief Initializes the mounts for the player bot.
     */
    void InitMounts();

    /**
     * @brief Initializes the potions for the player bot.
     */
    void InitPotions();

    /**
     * @brief Initializes the food for the player bot.
     */
    void InitFood();

    /**
     * @brief Checks if the player bot can equip a given armor item.
     * @param proto Pointer to the item prototype.
     * @return True if the armor can be equipped, false otherwise.
     */
    bool CanEquipArmor(ItemPrototype const* proto);

    /**
     * @brief Checks if the player bot can equip a given weapon item.
     * @param proto Pointer to the item prototype.
     * @return True if the weapon can be equipped, false otherwise.
     */
    bool CanEquipWeapon(ItemPrototype const* proto);

    /**
     * @brief Enchants a given item for the player bot.
     * @param item Pointer to the item.
     */
    void EnchantItem(Item* item);

    /**
     * @brief Adds item stats to the player bot.
     * @param mod The stat modifier.
     * @param sp The spell power stat.
     * @param ap The attack power stat.
     * @param tank The tank stat.
     */
    void AddItemStats(uint32 mod, uint8 &sp, uint8 &ap, uint8 &tank);

    /**
     * @brief Checks if the item stats are within desired ranges.
     * @param sp The spell power stat.
     * @param ap The attack power stat.
     * @param tank The tank stat.
     * @return True if the item stats are within desired ranges, false otherwise.
     */
    bool CheckItemStats(uint8 sp, uint8 ap, uint8 tank);

    /**
     * @brief Cancels all auras on the player bot.
     */
    void CancelAuras();

    /**
     * @brief Checks if a given item is a desired replacement for the player bot.
     * @param item Pointer to the item.
     * @return True if the item is a desired replacement, false otherwise.
     */
    bool IsDesiredReplacement(Item* item);

    /**
     * @brief Initializes the bags for the player bot.
     */
    void InitBags();

    /**
     * @brief Initializes the inventory for the player bot.
     */
    void InitInventory();

    /**
     * @brief Initializes the trade inventory for the player bot.
     */
    void InitInventoryTrade();

    /**
     * @brief Initializes the equipped inventory for the player bot.
     */
    void InitInventoryEquip();

    /**
     * @brief Initializes the skill inventory for the player bot.
     */
    void InitInventorySkill();

    /**
     * @brief Stores an item in the player bot's inventory.
     * @param itemId The item ID.
     * @param count The item count.
     * @return Pointer to the stored item.
     */
    Item* StoreItem(uint32 itemId, uint32 count);
    void InitGlyphs();

private:
    Player* bot;
    uint32 level;
    uint32 itemQuality;
    static uint32 tradeSkills[];
};
