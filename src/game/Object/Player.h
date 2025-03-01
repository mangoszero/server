/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOS_H_PLAYER
#define MANGOS_H_PLAYER

#include "Common.h"
#include "ItemPrototype.h"
#include "Unit.h"
#include "Item.h"

#include "Database/DatabaseEnv.h"
#include "QuestDef.h"
#include "Group.h"
#include "Bag.h"
#include "WorldSession.h"
#include "Pet.h"
#include "MapReference.h"
#include "Util.h"                                           // for Tokens typedef
#include "ReputationMgr.h"
#include "BattleGround.h"
#include "DBCStores.h"
#include "SharedDefines.h"
#include "Chat.h"
#include "GMTicketMgr.h"

#include<vector>

struct Mail;
class Channel;
class DynamicObject;
class Creature;
class PlayerMenu;
class Transport;
class UpdateMask;
class SpellCastTargets;
class PlayerSocial;
class DungeonPersistentState;
class Spell;
class Item;

struct AreaTrigger;

#ifdef ENABLE_PLAYERBOTS
class PlayerbotAI;
class PlayerbotMgr;
#endif

typedef std::deque<Mail*> PlayerMails;

#define PLAYER_MAX_SKILLS           127
#define PLAYER_EXPLORED_ZONES_SIZE  64

// Note: SPELLMOD_* values is aura types in fact
enum SpellModType
{
    SPELLMOD_FLAT               = 107,                      // SPELL_AURA_ADD_FLAT_MODIFIER
    SPELLMOD_PCT                = 108                       // SPELL_AURA_ADD_PCT_MODIFIER
};

// 2^n internal values, they are never sent to the client
enum PlayerUnderwaterState
{
    UNDERWATER_NONE             = 0x00,
    UNDERWATER_INWATER          = 0x01,                     // terrain type is water and player is afflicted by it
    UNDERWATER_INLAVA           = 0x02,                     // terrain type is lava and player is afflicted by it
    UNDERWATER_INSLIME          = 0x04,                     // terrain type is lava and player is afflicted by it
    UNDERWATER_INDARKWATER      = 0x08,                     // terrain type is dark water and player is afflicted by it

    UNDERWATER_EXIST_TIMERS     = 0x10
};

enum BuyBankSlotResult
{
    ERR_BANKSLOT_FAILED_TOO_MANY    = 0,
    ERR_BANKSLOT_INSUFFICIENT_FUNDS = 1,
    ERR_BANKSLOT_NOTBANKER          = 2,
    ERR_BANKSLOT_OK                 = 3
};

enum PlayerSpellState
{
    PLAYERSPELL_UNCHANGED       = 0,
    PLAYERSPELL_CHANGED         = 1,
    PLAYERSPELL_NEW             = 2,
    PLAYERSPELL_REMOVED         = 3
};

// Structure to hold player spell information
struct PlayerSpell
{
    PlayerSpellState state : 8;  // State of the spell
    bool active            : 1;  // Show in spellbook
    bool dependent         : 1;  // Learned as result of another spell learn, skill grow, quest reward, etc
    bool disabled          : 1;  // First rank has been learned as a result of talent learn but currently talent unlearned, save max learned ranks
};

typedef UNORDERED_MAP<uint32, PlayerSpell> PlayerSpellMap;

// Spell modifier (used for modifying other spells)
struct SpellModifier
{
    SpellModifier() : op(SpellModOp()), type(SPELLMOD_FLAT), charges(0), value(0), spellId(0), lastAffected(NULL) {}


    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, uint64 _mask, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), lastAffected(NULL)
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, uint32 _spellId, ClassFamilyMask _mask, int16 _charges = 0)
        : op(_op), type(_type), charges(_charges), value(_value), mask(_mask), spellId(_spellId), lastAffected(NULL)
    {}

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, SpellEntry const* spellEntry, SpellEffectIndex eff, int16 _charges = 0);

    SpellModifier(SpellModOp _op, SpellModType _type, int32 _value, Aura const* aura, int16 _charges = 0);

    bool isAffectedOnSpell(SpellEntry const* spell) const;

    SpellModOp   op   : 8;  // Operation type
    SpellModType type : 8;  // Modifier type
    int16 charges     : 16; // Number of charges
    int32 value;            // Modifier value
    ClassFamilyMask mask;   // Class family mask
    uint32 spellId;         // Spell ID
    Spell const* lastAffected; // Last affected spell, used for cleanup delayed remove spellmods at spell success or restore charges at cast fail
};

typedef std::list<SpellModifier*> SpellModList;

// Structure to hold spell cooldown information
struct SpellCooldown
{
    time_t end;    // End time of the cooldown
    uint16 itemid; // Item ID associated with the cooldown
};

typedef std::map<uint32, SpellCooldown> SpellCooldowns;

enum TrainerSpellState
{
    TRAINER_SPELL_GREEN          = 0,
    TRAINER_SPELL_RED            = 1,
    TRAINER_SPELL_GRAY           = 2,
    TRAINER_SPELL_GREEN_DISABLED = 10 // Custom value, not sent to client: formally green but learn not allowed
};

enum ActionButtonUpdateState
{
    ACTIONBUTTON_UNCHANGED      = 0,
    ACTIONBUTTON_CHANGED        = 1,
    ACTIONBUTTON_NEW            = 2,
    ACTIONBUTTON_DELETED        = 3
};

enum ActionButtonType
{
    ACTION_BUTTON_SPELL         = 0x00,
    ACTION_BUTTON_C             = 0x01, // Click?
    ACTION_BUTTON_MACRO         = 0x40,
    ACTION_BUTTON_CMACRO        = ACTION_BUTTON_C | ACTION_BUTTON_MACRO,
    ACTION_BUTTON_ITEM          = 0x80
};

#define ACTION_BUTTON_ACTION(X) (uint32(X) & 0x00FFFFFF)
#define ACTION_BUTTON_TYPE(X)   ((uint32(X) & 0xFF000000) >> 24)
#define MAX_ACTION_BUTTON_ACTION_VALUE (0x00FFFFFF+1)

// Structure to hold action button information
struct ActionButton
{
    ActionButton() : packedData(0), uState(ACTIONBUTTON_NEW) {}

    uint32 packedData;               // Packed data containing action and type
    ActionButtonUpdateState uState;  // Update state of the action button

    // Helpers
    ActionButtonType GetType() const
    {
        return ActionButtonType(ACTION_BUTTON_TYPE(packedData));
    }
    uint32 GetAction() const
    {
        return ACTION_BUTTON_ACTION(packedData);
    }
    void SetActionAndType(uint32 action, ActionButtonType type)
    {
        uint32 newData = action | (uint32(type) << 24);
        if (newData != packedData || uState == ACTIONBUTTON_DELETED)
        {
            packedData = newData;
            if (uState != ACTIONBUTTON_NEW)
            {
                uState = ACTIONBUTTON_CHANGED;
            }
        }
    }
};

#define  MAX_ACTION_BUTTONS 120   // TBC 132 checked in 2.3.0

typedef std::map<uint8, ActionButton> ActionButtonList;

// Structure to hold player create info item
struct PlayerCreateInfoItem
{
    PlayerCreateInfoItem(uint32 id, uint32 amount) : item_id(id), item_amount(amount) {}

    uint32 item_id;     // Item ID
    uint32 item_amount; // Item amount
};

typedef std::list<PlayerCreateInfoItem> PlayerCreateInfoItems;

// Structure to hold player class level info
struct PlayerClassLevelInfo
{
    PlayerClassLevelInfo() : basehealth(0), basemana(0) {}
    uint16 basehealth; // Base health
    uint16 basemana;   // Base mana
};

// Structure to hold player class info
struct PlayerClassInfo
{
    PlayerClassInfo() : levelInfo(NULL) { }

    PlayerClassLevelInfo* levelInfo; // Level info array [level-1] 0..MaxPlayerLevel-1
};

// Structure to hold player level info
struct PlayerLevelInfo
{
    PlayerLevelInfo()
    {
        for (int i = 0; i < MAX_STATS; ++i)
        {
            stats[i] = 0;
        }
    }

    uint8 stats[MAX_STATS]; // Stats array
};

typedef std::list<uint32> PlayerCreateInfoSpells;

// Structure to hold player create info action
struct PlayerCreateInfoAction
{
    PlayerCreateInfoAction() : button(0), type(0), action(0) {}
    PlayerCreateInfoAction(uint8 _button, uint32 _action, uint8 _type) : button(_button), type(_type), action(_action) {}

    uint8 button;  // Button index
    uint8 type;    // Action type
    uint32 action; // Action ID
};

typedef std::list<PlayerCreateInfoAction> PlayerCreateInfoActions;

// Structure to hold player info
struct PlayerInfo
{
    // existence checked by displayId != 0             // existence checked by displayId != 0
    PlayerInfo() : displayId_m(0), displayId_f(0), levelInfo(NULL), areaId(0), mapId(0), orientation(0.0f), positionX(0.0f), positionY(0.0f), positionZ(0.0f) {}

    uint32 mapId;             // Map ID
    uint32 areaId;            // Area ID
    float positionX;          // Position X
    float positionY;          // Position Y
    float positionZ;          // Position Z
    float orientation;        // Orientation
    uint16 displayId_m;       // Display ID for male
    uint16 displayId_f;       // Display ID for female
    PlayerCreateInfoItems item; // Create info items
    PlayerCreateInfoSpells spell; // Create info spells
    PlayerCreateInfoActions action; // Create info actions

    PlayerLevelInfo* levelInfo; // Level info array [level-1] 0..MaxPlayerLevel-1
};

// Structure to hold PvP info
struct PvPInfo
{
    PvPInfo() : inHostileArea(false), endTimer(0) {}

    bool inHostileArea; // Is in hostile area
    time_t endTimer;    // End timer
};

// Structure to hold duel info
struct DuelInfo
{
    DuelInfo() : initiator(NULL), opponent(NULL), startTimer(0), startTime(0), outOfBound(0) {}

    Player* initiator; // Initiator player
    Player* opponent;  // Opponent player
    time_t startTimer; // Start timer
    time_t startTime;  // Start time
    time_t outOfBound; // Out of bound timer
};

// Structure to hold area information
struct Areas
{
    uint32 areaID;   // Area ID
    uint32 areaFlag; // Area flag
    float x1;        // X1 coordinate
    float x2;        // X2 coordinate
    float y1;        // Y1 coordinate
    float y2;        // Y2 coordinate
};

// Structure to hold enchant duration information
struct EnchantDuration
{
    EnchantDuration() : item(NULL), slot(MAX_ENCHANTMENT_SLOT), leftduration(0) {};
    EnchantDuration(Item* _item, EnchantmentSlot _slot, uint32 _leftduration) : item(_item), slot(_slot), leftduration(_leftduration) { MANGOS_ASSERT(item); };

    Item* item;             // Item pointer
    EnchantmentSlot slot;   // Enchantment slot
    uint32 leftduration;    // Left duration
};

typedef std::list<EnchantDuration> EnchantDurationList;
typedef std::list<Item*> ItemDurationList;

enum RaidGroupError
{
    ERR_RAID_GROUP_REQUIRED = 1,
    ERR_RAID_GROUP_FULL     = 2
};

enum DrunkenState
{
    DRUNKEN_SOBER               = 0,
    DRUNKEN_TIPSY               = 1,
    DRUNKEN_DRUNK               = 2,
    DRUNKEN_SMASHED             = 3
};

#define MAX_DRUNKEN             4

enum TYPE_OF_HONOR
{
    HONORABLE    = 1,
    DISHONORABLE = 2,
};

struct HonorCP
{
    uint8 victimType;
    uint32 victimID;
    float honorPoints;
    uint32 date;
    uint8 type;
    uint8 state;
    bool isKill;
};

/**
 * This structure contains info about a players pvp rank, ie: what's shown when we hit the H button
 * in the client to show our honor.
 * \todo What's the maxRP,minRP and positive fields?
 */
struct HonorRankInfo
{
    uint8 rank;      ///< Internal range [0..18]
    int8 visualRank; ///< Number visualized in rank bar [-4..14] 14 being High Warlord, -4 being Pariah)
    float maxRP;
    float minRP;
    bool positive;
};

enum HonorKillState
{
    HK_NEW = 0,
    HK_OLD = 1,
    HK_DELETED = 2,
    HK_UNCHANGED = 3
};

typedef std::list<HonorCP> HonorCPMap;

#define NEGATIVE_HONOR_RANK_COUNT 4
#define POSITIVE_HONOR_RANK_COUNT 15
#define HONOR_RANK_COUNT 19 // negative + positive ranks

enum PlayerFlags
{
    PLAYER_FLAGS_NONE                   = 0x00000000,
    PLAYER_FLAGS_GROUP_LEADER           = 0x00000001,
    PLAYER_FLAGS_AFK                    = 0x00000002,
    PLAYER_FLAGS_DND                    = 0x00000004,
    PLAYER_FLAGS_GM                     = 0x00000008,
    PLAYER_FLAGS_GHOST                  = 0x00000010,
    PLAYER_FLAGS_RESTING                = 0x00000020,
    PLAYER_FLAGS_UNK7                   = 0x00000040,       // admin?
    PLAYER_FLAGS_FFA_PVP                = 0x00000080,
    PLAYER_FLAGS_CONTESTED_PVP          = 0x00000100,       // Player has been involved in a PvP combat and will be attacked by contested guards
    PLAYER_FLAGS_IN_PVP                 = 0x00000200,
    PLAYER_FLAGS_HIDE_HELM              = 0x00000400,
    PLAYER_FLAGS_HIDE_CLOAK             = 0x00000800,
    PLAYER_FLAGS_PARTIAL_PLAY_TIME      = 0x00001000,       // played long time
    PLAYER_FLAGS_NO_PLAY_TIME           = 0x00002000,       // played too long time
    PLAYER_FLAGS_UNK15                  = 0x00004000,
    PLAYER_FLAGS_UNK16                  = 0x00008000,       // strange visual effect (2.0.1), looks like PLAYER_FLAGS_GHOST flag
    PLAYER_FLAGS_SANCTUARY              = 0x00010000,       // player entered sanctuary
    PLAYER_FLAGS_TAXI_BENCHMARK         = 0x00020000,       // taxi benchmark mode (on/off) (2.0.1)
    PLAYER_FLAGS_PVP_TIMER              = 0x00040000,       // 3.0.2, pvp timer active (after you disable pvp manually)
    PLAYER_FLAGS_XP_USER_DISABLED       = 0x02000000,
};

// used in (PLAYER_FIELD_BYTES, 0) byte values
enum PlayerFieldByteFlags
{
    PLAYER_FIELD_BYTE_TRACK_STEALTHED   = 0x02, // Track stealthed units
    PLAYER_FIELD_BYTE_RELEASE_TIMER     = 0x08, // Display time till auto release spirit
    PLAYER_FIELD_BYTE_NO_RELEASE_WINDOW = 0x10  // Display no "release spirit" window at all
};

// used in byte (PLAYER_FIELD_BYTES2,1) values
enum PlayerFieldByte2Flags
{
    PLAYER_FIELD_BYTE2_NONE              = 0x00, // No flags
    PLAYER_FIELD_BYTE2_DETECT_AMORE_0    = 0x02, // SPELL_AURA_DETECT_AMORE, not used as value and maybe not related to, but used in code as base for mask apply
    PLAYER_FIELD_BYTE2_DETECT_AMORE_1    = 0x04, // SPELL_AURA_DETECT_AMORE value 1
    PLAYER_FIELD_BYTE2_DETECT_AMORE_2    = 0x08, // SPELL_AURA_DETECT_AMORE value 2
    PLAYER_FIELD_BYTE2_DETECT_AMORE_3    = 0x10, // SPELL_AURA_DETECT_AMORE value 3
    PLAYER_FIELD_BYTE2_STEALTH           = 0x20, // Stealth mode
    PLAYER_FIELD_BYTE2_INVISIBILITY_GLOW = 0x40  // Invisibility glow effect
};

// Mirror timer types
enum MirrorTimerType
{
    FATIGUE_TIMER               = 0, // Fatigue timer
    BREATH_TIMER                = 1, // Breath timer
    FIRE_TIMER                  = 2  // Probably a mistake. More likely to be FEIGN_DEATH_TIMER
};

#define MAX_TIMERS              3
#define DISABLED_MIRROR_TIMER   -1

// 2^n values for player extra flags
enum PlayerExtraFlags
{
    // GM abilities
    PLAYER_EXTRA_GM_ON              = 0x0001, // GM mode on
    PLAYER_EXTRA_GM_ACCEPT_TICKETS  = 0x0002, // GM accepts tickets
    PLAYER_EXTRA_ACCEPT_WHISPERS    = 0x0004, // Accept whispers
    PLAYER_EXTRA_TAXICHEAT          = 0x0008, // Taxi cheat mode
    PLAYER_EXTRA_GM_INVISIBLE       = 0x0010, // GM invisible mode
    PLAYER_EXTRA_GM_CHAT            = 0x0020, // Show GM badge in chat messages
    PLAYER_EXTRA_AUCTION_NEUTRAL    = 0x0040, // Neutral auction access
    PLAYER_EXTRA_AUCTION_ENEMY      = 0x0080, // Enemy auction access, overwrites PLAYER_EXTRA_AUCTION_NEUTRAL

    // Other states
    PLAYER_EXTRA_PVP_DEATH          = 0x0100  // Store PvP death status until corpse creation
};

// 2^n values for at login flags
enum AtLoginFlags
{
    AT_LOGIN_NONE                 = 0x00, // No special actions at login
    AT_LOGIN_RENAME               = 0x01, // Rename character at login
    AT_LOGIN_RESET_SPELLS         = 0x02, // Reset spells at login
    AT_LOGIN_RESET_TALENTS        = 0x04, // Reset talents at login
    // AT_LOGIN_CUSTOMIZE         = 0x08, -- used in post-3.x
    // AT_LOGIN_RESET_PET_TALENTS = 0x10, -- used in post-3.x
    AT_LOGIN_FIRST                = 0x20  // First login flag
};

typedef std::map<uint32, QuestStatusData> QuestStatusMap;

// Offsets for quest slots
enum QuestSlotOffsets
{
    QUEST_ID_OFFSET             = 0,
    QUEST_COUNT_STATE_OFFSET    = 1,                        // including counters 6bits+6bits+6bits+6bits + state 8bits
    QUEST_TIME_OFFSET           = 2
};

#define MAX_QUEST_OFFSET 3

// State mask for quest slots
enum QuestSlotStateMask
{
    QUEST_STATE_NONE            = 0x0000, // No state
    QUEST_STATE_COMPLETE        = 0x0001, // Quest complete
    QUEST_STATE_FAIL            = 0x0002  // Quest failed
};

// States for skill updates
enum SkillUpdateState
{
    SKILL_UNCHANGED             = 0, // Skill unchanged
    SKILL_CHANGED               = 1, // Skill changed
    SKILL_NEW                   = 2, // New skill
    SKILL_DELETED               = 3  // Skill deleted
};

// Structure to hold skill status data
struct SkillStatusData
{
    SkillStatusData(uint8 _pos, SkillUpdateState _uState) : pos(_pos), uState(_uState) {}

    uint8 pos;              // Position of the skill
    SkillUpdateState uState; // Update state of the skill
};

typedef UNORDERED_MAP<uint32, SkillStatusData> SkillStatusMap;

// Player slots for items
enum PlayerSlots
{
    // First slot for item stored (in any way in player m_items data)
    PLAYER_SLOT_START           = 0,
    // Last+1 slot for item stored (in any way in player m_items data)
    PLAYER_SLOT_END             = 118,
    PLAYER_SLOTS_COUNT          = (PLAYER_SLOT_END - PLAYER_SLOT_START)
};

#define INVENTORY_SLOT_BAG_0    255

// Equipment slots (19 slots)
enum EquipmentSlots
{
    EQUIPMENT_SLOT_START        = 0,
    EQUIPMENT_SLOT_HEAD         = 0,  // Head slot
    EQUIPMENT_SLOT_NECK         = 1,  // Neck slot
    EQUIPMENT_SLOT_SHOULDERS    = 2,  // Shoulders slot
    EQUIPMENT_SLOT_BODY         = 3,  // Body slot
    EQUIPMENT_SLOT_CHEST        = 4,  // Chest slot
    EQUIPMENT_SLOT_WAIST        = 5,  // Waist slot
    EQUIPMENT_SLOT_LEGS         = 6,  // Legs slot
    EQUIPMENT_SLOT_FEET         = 7,  // Feet slot
    EQUIPMENT_SLOT_WRISTS       = 8,  // Wrists slot
    EQUIPMENT_SLOT_HANDS        = 9,  // Hands slot
    EQUIPMENT_SLOT_FINGER1      = 10, // First finger slot
    EQUIPMENT_SLOT_FINGER2      = 11, // Second finger slot
    EQUIPMENT_SLOT_TRINKET1     = 12, // First trinket slot
    EQUIPMENT_SLOT_TRINKET2     = 13, // Second trinket slot
    EQUIPMENT_SLOT_BACK         = 14, // Back slot
    EQUIPMENT_SLOT_MAINHAND     = 15, // Main hand slot
    EQUIPMENT_SLOT_OFFHAND      = 16, // Off hand slot
    EQUIPMENT_SLOT_RANGED       = 17, // Ranged slot
    EQUIPMENT_SLOT_TABARD       = 18, // Tabard slot
    EQUIPMENT_SLOT_END          = 19  // End of equipment slots
};

// Inventory slots (4 slots)
enum InventorySlots
{
    INVENTORY_SLOT_BAG_START    = 19, // Start of bag slots
    INVENTORY_SLOT_BAG_END      = 23  // End of bag slots
};

// Inventory pack slots (16 slots)
enum InventoryPackSlots
{
    INVENTORY_SLOT_ITEM_START   = 23, // Start of item slots
    INVENTORY_SLOT_ITEM_END     = 39  // End of item slots
};

// Bank item slots (28 slots)
enum BankItemSlots
{
    BANK_SLOT_ITEM_START        = 39,
    BANK_SLOT_ITEM_END          = 63
};

// Bank bag slots (7 slots)
enum BankBagSlots
{
    BANK_SLOT_BAG_START         = 63,
    BANK_SLOT_BAG_END           = 69
};

// Buy back slots (12 slots)
enum BuyBackSlots
{
    // stored in m_buybackitems
    BUYBACK_SLOT_START          = 69,
    BUYBACK_SLOT_END            = 81
};

// Key ring slots (32 slots)
enum KeyRingSlots
{
    KEYRING_SLOT_START          = 81,
    KEYRING_SLOT_END            = 97
};

// Structure to hold item position and count
struct ItemPosCount
{
    ItemPosCount(uint16 _pos, uint8 _count) : pos(_pos), count(_count) {}
    bool isContainedIn(std::vector<ItemPosCount> const& vec) const;

    uint16 pos;  // Position of the item
    uint8 count; // Count of the item
};

typedef std::vector<ItemPosCount> ItemPosCountVec;

// Trade slots
enum TradeSlots
{
    TRADE_SLOT_COUNT            = 7, // Total trade slots
    TRADE_SLOT_TRADED_COUNT     = 6, // Traded slots count
    TRADE_SLOT_NONTRADED        = 6  // Non-traded slots count
};

// Reasons for transfer abort
enum TransferAbortReason
{
    TRANSFER_ABORT_MAX_PLAYERS                  = 0x01,     // Transfer Aborted: instance is full
    TRANSFER_ABORT_NOT_FOUND                    = 0x02,     // Transfer Aborted: instance not found
    TRANSFER_ABORT_TOO_MANY_INSTANCES           = 0x03,     // You have entered too many instances recently.
    TRANSFER_ABORT_SILENTLY                     = 0x04,     // no message shown; the same effect give values above 5
    TRANSFER_ABORT_ZONE_IN_COMBAT               = 0x05,     // Unable to zone in while an encounter is in progress.
};

// Instance reset warning types
enum InstanceResetWarningType
{
    RAID_INSTANCE_WARNING_HOURS     = 1, // WARNING! %s is scheduled to reset in %d hour(s).
    RAID_INSTANCE_WARNING_MIN       = 2, // WARNING! %s is scheduled to reset in %d minute(s)!
    RAID_INSTANCE_WARNING_MIN_SOON  = 3, // WARNING! %s is scheduled to reset in %d minute(s). Please exit the zone or you will be returned to your bind location!
    RAID_INSTANCE_WELCOME           = 4  // Welcome to %s. This raid instance is scheduled to reset in %s.
};

enum RestType
{
    REST_TYPE_NO                = 0, // No rest
    REST_TYPE_IN_TAVERN         = 1, // Resting in a tavern
    REST_TYPE_IN_CITY           = 2  // Resting in a city
};

// Duel completion types
enum DuelCompleteType
{
    DUEL_INTERRUPTED            = 0, // Duel interrupted
    DUEL_WON                    = 1, // Duel won
    DUEL_FLED                   = 2  // Duel fled
};

// Teleport options
enum TeleportToOptions
{
    TELE_TO_GM_MODE             = 0x01, // GM mode teleport
    TELE_TO_NOT_LEAVE_TRANSPORT = 0x02, // Do not leave transport
    TELE_TO_NOT_LEAVE_COMBAT    = 0x04, // Do not leave combat
    TELE_TO_NOT_UNSUMMON_PET    = 0x08, // Do not unsummon pet
    TELE_TO_SPELL               = 0x10  // Teleport by spell
};

/// Type of environmental damages
enum EnvironmentalDamageType
{
    DAMAGE_EXHAUSTED            = 0, // Exhausted damage
    DAMAGE_DROWNING             = 1, // Drowning damage
    DAMAGE_FALL                 = 2, // Fall damage
    DAMAGE_LAVA                 = 3, // Lava damage
    DAMAGE_SLIME                = 4, // Slime damage
    DAMAGE_FIRE                 = 5, // Fire damage
    DAMAGE_FALL_TO_VOID         = 6  // Custom case for fall without durability loss
};

// Played time indices
enum PlayedTimeIndex
{
    PLAYED_TIME_TOTAL           = 0, // Total played time
    PLAYED_TIME_LEVEL           = 1  // Played time at current level
};

#define MAX_PLAYED_TIME_INDEX   2

// Used at player loading query list preparing, and later result selection
enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM,
    PLAYER_LOGIN_QUERY_LOADGROUP,
    PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,
    PLAYER_LOGIN_QUERY_LOADAURAS,
    PLAYER_LOGIN_QUERY_LOADSPELLS,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADHONORCP,
    PLAYER_LOGIN_QUERY_LOADREPUTATION,
    PLAYER_LOGIN_QUERY_LOADINVENTORY,
    PLAYER_LOGIN_QUERY_LOADITEMLOOT,
    PLAYER_LOGIN_QUERY_LOADACTIONS,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADGUILD,
    PLAYER_LOGIN_QUERY_LOADBGDATA,
    PLAYER_LOGIN_QUERY_LOADSKILLS,
    PLAYER_LOGIN_QUERY_LOADMAILS,
    PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,

    MAX_PLAYER_LOGIN_QUERY
};

// Delayed operations for players
enum PlayerDelayedOperations
{
    DELAYED_SAVE_PLAYER         = 0x01, // Delayed save player
    DELAYED_RESURRECT_PLAYER    = 0x02, // Delayed resurrect player
    DELAYED_SPELL_CAST_DESERTER = 0x04, // Delayed spell cast deserter
    DELAYED_END                         // End of delayed operations
};

// Sources of reputation
enum ReputationSource
{
    REPUTATION_SOURCE_KILL,   // Reputation from kills
    REPUTATION_SOURCE_QUEST,  // Reputation from quests
    REPUTATION_SOURCE_SPELL   // Reputation from spells
};

// Player summoning auto-decline time (in seconds)
#define MAX_PLAYER_SUMMON_DELAY (2*MINUTE)
#define MAX_MONEY_AMOUNT        (0x7FFFFFFF-1) // Maximum money amount

// Structure to hold instance player bind information
struct InstancePlayerBind
{
    DungeonPersistentState* state;
    bool perm;
    /* permanent PlayerInstanceBinds are created in Raid instances for players
       that aren't already permanently bound when they are inside when a boss is killed
       or when they enter an instance that the group leader is permanently bound to. */
    InstancePlayerBind() : state(NULL), perm(false) {}
};

// Enum to represent player rest states
enum PlayerRestState
{
    REST_STATE_RESTED           = 0x01, // Player is rested
    REST_STATE_NORMAL           = 0x02, // Player is in a normal state
    REST_STATE_RAF_LINKED       = 0x04  // Exact use unknown
};

enum PlayerMountResult
{
    MOUNTRESULT_INVALIDMOUNTEE  = 0,    // You can't mount that unit!
    MOUNTRESULT_TOOFARAWAY      = 1,    // That mount is too far away!
    MOUNTRESULT_ALREADYMOUNTED  = 2,    // You're already mounted!
    MOUNTRESULT_NOTMOUNTABLE    = 3,    // That unit can't be mounted!
    MOUNTRESULT_NOTYOURPET      = 4,    // That mount isn't your pet!
    MOUNTRESULT_OTHER           = 5,    // internal
    MOUNTRESULT_LOOTING         = 6,    // You can't mount while looting!
    MOUNTRESULT_RACECANTMOUNT   = 7,    // You can't mount because of your race!
    MOUNTRESULT_SHAPESHIFTED    = 8,    // You can't mount while shapeshifted!
    MOUNTRESULT_FORCEDDISMOUNT  = 9,    // You dismount before continuing.
    MOUNTRESULT_OK              = 10    // no error
};

enum PlayerDismountResult
{
    DISMOUNTRESULT_NOPET        = 0,    // internal
    DISMOUNTRESULT_NOTMOUNTED   = 1,    // You're not mounted!
    DISMOUNTRESULT_NOTYOURPET   = 2,    // internal
    DISMOUNTRESULT_OK           = 3     // no error
};

class PlayerTaxi
{
    public:
        PlayerTaxi();
        ~PlayerTaxi() {}
        // Nodes
        void InitTaxiNodes(uint32 race, uint32 level);
        void LoadTaxiMask(const char* data);

        // Check if a taxi node is known
        bool IsTaximaskNodeKnown(uint32 nodeidx) const
        {
            uint8  field   = uint8((nodeidx - 1) / 32);
            uint32 submask = 1 << ((nodeidx - 1) % 32);
            return (m_taximask[field] & submask) == submask;
        }

        // Set a taxi node as known
        bool SetTaximaskNode(uint32 nodeidx)
        {
            uint8  field   = uint8((nodeidx - 1) / 32);
            uint32 submask = 1 << ((nodeidx - 1) % 32);
            if ((m_taximask[field] & submask) != submask)
            {
                m_taximask[field] |= submask;
                return true;
            }
            else
            {
                return false;
            }
        }

        // Append taxi mask to data
        void AppendTaximaskTo(ByteBuffer& data, bool all);

        // Load taxi destinations from string
        bool LoadTaxiDestinationsFromString(const std::string& values, Team team);

        // Save taxi destinations to string
        std::string SaveTaxiDestinationsToString();

        // Clear taxi destinations
        void ClearTaxiDestinations()
        {
            m_TaxiDestinations.clear();
        }

        // Add a taxi destination
        void AddTaxiDestination(uint32 dest)
        {
            m_TaxiDestinations.push_back(dest);
        }

        // Get the source of the taxi
        uint32 GetTaxiSource() const
        {
            return m_TaxiDestinations.empty() ? 0 : m_TaxiDestinations.front();
        }

        // Get the destination of the taxi
        uint32 GetTaxiDestination() const
        {
            return m_TaxiDestinations.size() < 2 ? 0 : m_TaxiDestinations[1];
        }

        // Get the current taxi path
        uint32 GetCurrentTaxiPath() const;

        // Get the next taxi destination
        uint32 NextTaxiDestination()
        {
            m_TaxiDestinations.pop_front();
            return GetTaxiDestination();
        }

        // Check if there are no taxi destinations
        bool empty() const
        {
            return m_TaxiDestinations.empty();
        }

        // Friend function to output taxi information
        friend std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

    private:
        TaxiMask m_taximask; // Mask of known taxi nodes
        std::deque<uint32> m_TaxiDestinations; // Queue of taxi destinations
};

std::ostringstream& operator<< (std::ostringstream& ss, PlayerTaxi const& taxi);

/// Structure to hold battleground data
struct BGData
{
    BGData() : bgInstanceID(0), bgTypeID(BATTLEGROUND_TYPE_NONE), bgAfkReportedCount(0), bgAfkReportedTimer(0),
        bgTeam(TEAM_NONE), m_needSave(false) {}

    uint32 bgInstanceID; // Battleground instance ID
    ///  when player is teleported to BG - (it is battleground's GUID)
    BattleGroundTypeId bgTypeID; // Battleground type ID

    std::set<uint32> bgAfkReporter; // Set of players who reported AFK
    uint8 bgAfkReportedCount; // Count of AFK reports
    time_t bgAfkReportedTimer; // Timer for AFK reports

    Team bgTeam; // Team of the player in the battleground

    WorldLocation joinPos; // Position where the player joined the battleground

    bool m_needSave; // Indicates if the data needs to be saved
};

// Structure to hold trade status information
struct TradeStatusInfo
{
    TradeStatusInfo() : Status(TRADE_STATUS_BUSY), TraderGuid(), Result(EQUIP_ERR_OK),
        IsTargetResult(false), ItemLimitCategoryId(0), Slot(0) { }

    TradeStatus Status; // Status of the trade
    ObjectGuid TraderGuid; // GUID of the trader
    InventoryResult Result; // Result of the trade
    bool IsTargetResult; // Indicates if the result is for the target
    uint32 ItemLimitCategoryId; // Item limit category ID
    uint8 Slot; // Slot of the item
};

// Class to manage trade data
class TradeData
{
    public: // Constructors
        TradeData(Player* player, Player* trader) :
            m_player(player),  m_trader(trader), m_accepted(false), m_acceptProccess(false),
            m_money(0), m_spell(0) {}

    public: // Access functions

        // Get the trader
        Player* GetTrader() const
        {
            return m_trader;
        }

        // Get the trade data of the trader
        TradeData* GetTraderData() const;

        // Get the item in the specified trade slot
        Item* GetItem(TradeSlots slot) const;

        // Check if the trade has the specified item
        bool HasItem(ObjectGuid item_guid) const;

        // Get the spell applied to the trade
        uint32 GetSpell() const
        {
            return m_spell;
        }

        // Get the item used to cast the spell
        Item* GetSpellCastItem() const;

        // Check if there is a spell cast item
        bool HasSpellCastItem() const
        {
            return !m_spellCastItem.IsEmpty();
        }

        // Get the money placed in the trade
        uint32 GetMoney() const
        {
            return m_money;
        }

        // Check if the trade is accepted
        bool IsAccepted() const
        {
            return m_accepted;
        }

        // Check if the trade is in the accept process
        bool IsInAcceptProcess() const
        {
            return m_acceptProccess;
        }

    public: // Access functions

        // Set the item in the specified trade slot
        void SetItem(TradeSlots slot, Item* item);

        // Set the spell applied to the trade
        void SetSpell(uint32 spell_id, Item* castItem = NULL);

        // Set the money placed in the trade
        void SetMoney(uint32 money);

        // Set the accepted state of the trade
        void SetAccepted(bool state, bool crosssend = false);

        // Set the accept process state of the trade
        void SetInAcceptProcess(bool state)
        {
            m_acceptProccess = state;
        }

    private: // Internal functions

        // Update the trade data
        void Update(bool for_trader = true);

    private: // Fields

        Player* m_player; // Player who owns this TradeData
        Player* m_trader; // Player who trades with m_player

        bool m_accepted; // Indicates if m_player has accepted the trade
        bool m_acceptProccess; // Indicates if the accept process is ongoing

        uint32 m_money; // Money placed in the trade

        uint32 m_spell; // Spell applied to the non-traded slot item
        ObjectGuid m_spellCastItem; // Item used to cast the spell

        ObjectGuid m_items[TRADE_SLOT_COUNT]; // Items traded from m_player's side, including non-traded slot
};

class Player : public Unit
{
        friend class WorldSession;
        friend void Item::AddToUpdateQueueOf(Player* player);
        friend void Item::RemoveFromUpdateQueueOf(Player* player);
    public:
        explicit Player(WorldSession* session);
        ~Player();

        time_t lastTimeLooted; // Time when the player last looted

        void CleanupsBeforeDelete() override; // Cleanup operations before deleting the player

        static UpdateMask updateVisualBits; // Update mask for visual bits
        static void InitVisibleBits(); // Initialize visible bits

        void AddToWorld() override; // Add the player to the world
        void RemoveFromWorld() override; // Remove the player from the world

        bool TeleportTo(uint32 mapid, float x, float y, float z, float orientation, uint32 options = 0, bool allowNoDelay = false);

        // Teleport the player to a specific location using WorldLocation
        bool TeleportTo(WorldLocation const& loc, uint32 options = 0)
        {
            return TeleportTo(loc.mapid, loc.coord_x, loc.coord_y, loc.coord_z, loc.orientation, options);
        }

        bool TeleportToBGEntryPoint(); // Teleport the player to the battleground entry point

        // Set the summon point for the player
        void SetSummonPoint(uint32 mapid, float x, float y, float z)
        {
            m_summon_expire = time(NULL) + MAX_PLAYER_SUMMON_DELAY;
            m_summon_mapid = mapid;
            m_summon_x = x;
            m_summon_y = y;
            m_summon_z = z;
        }
        void SummonIfPossible(bool agree); // Summon the player if possible

        // Create a new player
        bool Create(uint32 guidlow, const std::string& name, uint8 race, uint8 class_, uint8 gender, uint8 skin, uint8 face, uint8 hairStyle, uint8 hairColor, uint8 facialHair, uint8 outfitId);

        void Update(uint32 update_diff, uint32 time) override; // Update the player

        static bool BuildEnumData(QueryResult* result,  WorldPacket* p_data); // Build enumeration data

        void SetInWater(bool apply); // Set the player in water

        bool IsInWater() const override // Check if the player is in water
        {
            return m_isInWater;
        }
        bool IsUnderWater() const override; // Check if the player is underwater
        bool IsFalling() // Check if the player is falling
        {
            return GetPositionZ() < m_lastFallZ;
        }

        void SendInitialPacketsBeforeAddToMap(); // Send initial packets before adding the player to the map
        void SendInitialPacketsAfterAddToMap(); // Send initial packets after adding the player to the map
        void SendInstanceResetWarning(uint32 mapid, uint32 time); // Send instance reset warning

        // Get the NPC if the player can interact with it
        Creature* GetNPCIfCanInteractWith(ObjectGuid guid, uint32 npcflagmask);
        // Get the game object if the player can interact with it
        GameObject* GetGameObjectIfCanInteractWith(ObjectGuid guid, uint32 gameobject_type = MAX_GAMEOBJECT_TYPE) const;

        void ToggleAFK(); // Toggle AFK status
        void ToggleDND(); // Toggle DND status
        bool isAFK() const // Check if the player is AFK
        {
            return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_AFK);
        }
        bool isDND() const // Check if the player is DND
        {
            return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_DND);
        }
        ChatTagFlags GetChatTag() const; // Get the chat tag flags
        std::string autoReplyMsg; // Auto-reply message

        PlayerSocial* GetSocial() // Get the player's social data
        {
            return m_social;
        }

        void SetCreatedDate(uint32 createdDate) // Set the created date of the player
        {
            m_created_date = createdDate;
        }

        uint32 GetCreatedDate() // Get the created date of the player
        {
            return m_created_date;
        }

        PlayerTaxi m_taxi;
        void InitTaxiNodes()
        {
            m_taxi.InitTaxiNodes(getRace(), getLevel());
        }

        // Activate taxi path to specified nodes
        bool ActivateTaxiPathTo(std::vector<uint32> const& nodes, Creature* npc = NULL, uint32 spellid = 0);

        // Activate taxi path to specified taxi path ID
        bool ActivateTaxiPathTo(uint32 taxi_path_id, uint32 spellid = 0);

        // Continue the taxi flight
        void ContinueTaxiFlight();
        void Mount(uint32 mount, uint32 spellId = 0) override;
        void Unmount(bool from_aura = false) override;
        void SendMountResult(PlayerMountResult result);
        void SendDismountResult(PlayerDismountResult result);
        bool isAcceptTickets() const { return GetSession()->GetSecurity() >= SEC_GAMEMASTER && (m_ExtraFlags & PLAYER_EXTRA_GM_ACCEPT_TICKETS); }

        // Set the accept ticket state
        void SetAcceptTicket(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_GM_ACCEPT_TICKETS; } else { m_ExtraFlags &= ~PLAYER_EXTRA_GM_ACCEPT_TICKETS; } }

        // Check if the player accepts whispers
        bool isAcceptWhispers() const { return m_ExtraFlags & PLAYER_EXTRA_ACCEPT_WHISPERS; }

        // Set the accept whispers state
        void SetAcceptWhispers(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_ACCEPT_WHISPERS; } else { m_ExtraFlags &= ~PLAYER_EXTRA_ACCEPT_WHISPERS; } }

        // Check if the player is a game master
        bool isGameMaster() const { return m_ExtraFlags & PLAYER_EXTRA_GM_ON; }

        // Set the game master state
        void SetGameMaster(bool on);

        // Check if the player has GM chat enabled
        bool isGMChat() const { return GetSession()->GetSecurity() >= SEC_MODERATOR && (m_ExtraFlags & PLAYER_EXTRA_GM_CHAT); }

        // Set the GM chat state
        void SetGMChat(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_GM_CHAT; } else { m_ExtraFlags &= ~PLAYER_EXTRA_GM_CHAT; } }

        // Check if the player is a taxi cheater
        bool IsTaxiCheater() const { return m_ExtraFlags & PLAYER_EXTRA_TAXICHEAT; }

        // Set the taxi cheater state
        void SetTaxiCheater(bool on) { if (on) { m_ExtraFlags |= PLAYER_EXTRA_TAXICHEAT; } else { m_ExtraFlags &= ~PLAYER_EXTRA_TAXICHEAT; } }

        // Check if the player is visible as a GM
        bool isGMVisible() const { return !(m_ExtraFlags & PLAYER_EXTRA_GM_INVISIBLE); }

        // Set the GM visibility state
        void SetGMVisible(bool on);

        // Set the PvP death state
        void SetPvPDeath(bool on)
        {
            if (on)
            {
                m_ExtraFlags |= PLAYER_EXTRA_PVP_DEATH;
            }
            else
            {
                m_ExtraFlags &= ~PLAYER_EXTRA_PVP_DEATH;
            }
        }

        // Get the auction access mode
        // 0 = own auction, -1 = enemy auction, 1 = goblin auction
        int GetAuctionAccessMode() const
        {
            return m_ExtraFlags & PLAYER_EXTRA_AUCTION_ENEMY ? -1 : (m_ExtraFlags & PLAYER_EXTRA_AUCTION_NEUTRAL ? 1 : 0);
        }

        // Set the auction access mode
        void SetAuctionAccessMode(int state)
        {
            m_ExtraFlags &= ~(PLAYER_EXTRA_AUCTION_ENEMY | PLAYER_EXTRA_AUCTION_NEUTRAL);

            if (state < 0)
            {
                m_ExtraFlags |= PLAYER_EXTRA_AUCTION_ENEMY;
            }
            else if (state > 0)
            {
                m_ExtraFlags |= PLAYER_EXTRA_AUCTION_NEUTRAL;
            }
        }

        // Give experience points to the player
        void GiveXP(uint32 xp, Unit* victim);

        // Give a level to the player
        void GiveLevel(uint32 level);

        // Initialize stats for the player's level
        void InitStatsForLevel(bool reapplyMods = false);

        // Played Time Stuff
        time_t m_logintime; // Login time
        time_t m_Last_tick; // Last tick time

        uint32 m_Played_time[MAX_PLAYED_TIME_INDEX]; // Played time array

        // Get the total played time
        uint32 GetTotalPlayedTime()
        {
            return m_Played_time[PLAYED_TIME_TOTAL];
        }

        // Get the played time at the current level
        uint32 GetLevelPlayedTime()
        {
            return m_Played_time[PLAYED_TIME_LEVEL];
        }


        // Set the death state of the player
        void SetDeathState(DeathState s) override; // overwrite Unit::SetDeathState

        // Get the rest bonus
        float GetRestBonus() const
        {
            return m_rest_bonus;
        }

        // Set the rest bonus
        void SetRestBonus(float rest_bonus_new);

        /**
        * \brief: compute rest bonus
        * \param: time_t timePassed > time from last check
        * \param: bool offline      > is the player was offline?
        * \param: bool inRestPlace  > if it was offline, is the player was in city/tavern/inn?
        * \returns: float
        **/
        float ComputeRest(time_t timePassed, bool offline = false, bool inRestPlace = false);

        // Get the rest type
        RestType GetRestType() const
        {
            return rest_type;
        }

        // Set the rest type
        void SetRestType(RestType n_r_type, uint32 areaTriggerId = 0);

        // Get the time the player entered the inn
        time_t GetTimeInnEnter() const
        {
            return time_inn_enter;
        }

        // Update the inn enter time
        void UpdateInnerTime(time_t time)
        {
            time_inn_enter = time;
        }

        // Remove the player's pet
        void RemovePet(PetSaveMode mode);

        // Remove the player's mini pet
        void RemoveMiniPet();
        Pet* GetMiniPet() const override;


        // Set the player's mini pet (used only in Pet::Unsummon/Spell::DoSummon)
        void _SetMiniPet(Pet* pet)
        {
            m_miniPetGuid = pet ? pet->GetObjectGuid() : ObjectGuid();
        }

        // Player communication methods
        void Say(const std::string& text, const uint32 language);
        void Yell(const std::string& text, const uint32 language);
        void TextEmote(const std::string& text);

        /**
         * This will log a whisper depending on the setting LogWhispers in mangosd.conf, for a list
         * of available levels please see \ref WhisperLoggingLevels. The logging is done to database
         * in the table characters.character_whispers and includes to/from, text and when the whisper
         * was sent.
         *
         * @param text the text that was sent
         * @param receiver guid of the receiver of the message
         * \see WhisperLoggingLevels
         * \see eConfigUInt32Values::CONFIG_UINT32_LOG_WHISPERS
         */
        void LogWhisper(const std::string& text, ObjectGuid receiver);
        void Whisper(const std::string& text, const uint32 language, ObjectGuid receiver);

        /*********************************************************/
        /***                    STORAGE SYSTEM                 ***/
        /*********************************************************/

        // Set the virtual item slot
        void SetVirtualItemSlot(uint8 i, Item* item);
        void SetSheath(SheathState sheathed) override;      // overwrite Unit version
        bool ViableEquipSlots(ItemPrototype const* proto, uint8 *viable_slots) const;
        uint8 FindEquipSlot(ItemPrototype const* proto, uint32 slot, bool swap) const;

        // Get the count of the specified item
        uint32 GetItemCount(uint32 item, bool inBankAlso = false, Item* skipItem = NULL) const;

        // Get the item by its GUID
        Item* GetItemByGuid(ObjectGuid guid) const;

        // Get the item by its entry ID (only for special cases)
        Item* GetItemByEntry(uint32 item) const;

        // Get the item by its position
        Item* GetItemByPos(uint16 pos) const;

        // Get the item by its bag and slot
        Item* GetItemByPos(uint8 bag, uint8 slot) const;
        Item* GetWeaponForAttack(WeaponAttackType attackType) const
        {
            return GetWeaponForAttack(attackType, false, false);
        }

        // Get the weapon for the specified attack type with additional options
        Item* GetWeaponForAttack(WeaponAttackType attackType, bool nonbroken, bool useable) const;

        // Get the shield (if usable)
        Item* GetShield(bool useable = false) const;

        // Get the attack type by the slot
        static uint32 GetAttackBySlot(uint8 slot);

        // Get the item update queue
        std::vector<Item*>& GetItemUpdateQueue() { return m_itemUpdateQueue; }

        // Check if the position is an inventory position
        static bool IsInventoryPos(uint16 pos) { return IsInventoryPos(pos >> 8, pos & 255); }

        // Check if the position is an inventory position (overloaded)
        static bool IsInventoryPos(uint8 bag, uint8 slot);

        // Check if the position is an equipment position
        static bool IsEquipmentPos(uint16 pos) { return IsEquipmentPos(pos >> 8, pos & 255); }

        // Check if the position is an equipment position (overloaded)
        static bool IsEquipmentPos(uint8 bag, uint8 slot);

        // Check if the position is a bag position
        static bool IsBagPos(uint16 pos);

        // Check if the position is a bank position
        static bool IsBankPos(uint16 pos) { return IsBankPos(pos >> 8, pos & 255); }

        // Check if the position is a bank position (overloaded)
        static bool IsBankPos(uint8 bag, uint8 slot);

        // Check if the position is valid
        bool IsValidPos(uint16 pos, bool explicit_pos) const { return IsValidPos(pos >> 8, pos & 255, explicit_pos); }

        // Check if the position is valid (overloaded)
        bool IsValidPos(uint8 bag, uint8 slot, bool explicit_pos) const;

        // Get the count of bank bag slots
        uint8 GetBankBagSlotCount() const { return GetByteValue(PLAYER_BYTES_2, 2); }

        // Set the count of bank bag slots
        void SetBankBagSlotCount(uint8 count) { SetByteValue(PLAYER_BYTES_2, 2, count); }

        // Check if the player has the specified item count
        bool HasItemCount(uint32 item, uint32 count, bool inBankAlso = false) const;

        // Check if the player has an item that fits the spell requirements
        bool HasItemFitToSpellReqirements(SpellEntry const* spellInfo, Item const* ignoreItem = NULL);

        // Check if the player can cast the spell without reagents
        bool CanNoReagentCast(SpellEntry const* spellInfo) const;
        bool HasItemWithIdEquipped(uint32 item, uint32 count, uint8 except_slot = NULL_SLOT) const;
        InventoryResult CanTakeMoreSimilarItems(Item* pItem) const { return _CanTakeMoreSimilarItems(pItem->GetEntry(), pItem->GetCount(), pItem); }

        // Check if the player can take more similar items (overloaded)
        InventoryResult CanTakeMoreSimilarItems(uint32 entry, uint32 count) const { return _CanTakeMoreSimilarItems(entry, count, NULL); }

        // Check if the player can store a new item
        InventoryResult CanStoreNewItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 item, uint32 count, uint32* no_space_count = NULL) const
        {
            return _CanStoreItem(bag, slot, dest, item, count, NULL, false, no_space_count);
        }

        // Check if the player can store an item
        InventoryResult CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap = false) const
        {
            if (!pItem)
            {
                return EQUIP_ERR_ITEM_NOT_FOUND;
            }
            uint32 count = pItem->GetCount();
            return _CanStoreItem(bag, slot, dest, pItem->GetEntry(), count, pItem, swap, NULL);
        }

        // Check if the player can store multiple items
        InventoryResult CanStoreItems(Item** pItem, int count) const;

        // Check if the player can equip a new item
        InventoryResult CanEquipNewItem(uint8 slot, uint16& dest, uint32 item, bool swap) const;

        // Check if the player can equip an item
        InventoryResult CanEquipItem(uint8 slot, uint16& dest, Item* pItem, bool swap, bool direct_action = true) const;

        // Check if the player can equip a unique item
        InventoryResult CanEquipUniqueItem(Item* pItem, uint8 except_slot = NULL_SLOT) const;

        // Check if the player can equip a unique item (overloaded)
        InventoryResult CanEquipUniqueItem(ItemPrototype const* itemProto, uint8 except_slot = NULL_SLOT) const;

        // Check if the player can unequip items
        InventoryResult CanUnequipItems(uint32 item, uint32 count) const;

        // Check if the player can unequip an item
        InventoryResult CanUnequipItem(uint16 src, bool swap) const;

        // Check if the player can bank an item
        InventoryResult CanBankItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, Item* pItem, bool swap, bool not_loading = true) const;

        // Check if the player can use an item
        InventoryResult CanUseItem(Item* pItem, bool direct_action = true) const;

        // Check if the player has an item with the specified totem category
        bool HasItemTotemCategory(uint32 TotemCategory) const;
        InventoryResult CanUseItem(ItemPrototype const* pItem, bool direct_action = true) const;
        InventoryResult CanUseAmmo(uint32 item) const;

        // Store a new item
        Item* StoreNewItem(ItemPosCountVec const& pos, uint32 item, bool update, int32 randomPropertyId = 0);

        // Store an item
        Item* StoreItem(ItemPosCountVec const& pos, Item* pItem, bool update);

        // Equip a new item
        Item* EquipNewItem(uint16 pos, uint32 item, bool update);

        // Equip an item
        Item* EquipItem(uint16 pos, Item* pItem, bool update);

        // Automatically unequip the offhand item if needed
        void AutoUnequipOffhandIfNeed();

        // Store a new item in the best slots
        bool StoreNewItemInBestSlots(uint32 item_id, uint32 item_count);

        // Store a new item in the inventory slot
        Item* StoreNewItemInInventorySlot(uint32 itemEntry, uint32 amount);

        // Automatically store loot
        void AutoStoreLoot(WorldObject const* lootTarget, uint32 loot_id, LootStore const& store, bool broadcast = false, uint8 bag = NULL_BAG, uint8 slot = NULL_SLOT);

        // Automatically store loot (overloaded)
        void AutoStoreLoot(Loot& loot, bool broadcast = false, uint8 bag = NULL_BAG, uint8 slot = NULL_SLOT);

        // Convert an item to a new item ID
        Item* ConvertItem(Item* item, uint32 newItemId);

        // Internal methods for storing items
        InventoryResult _CanTakeMoreSimilarItems(uint32 entry, uint32 count, Item* pItem, uint32* no_space_count = NULL) const;
        InventoryResult _CanStoreItem(uint8 bag, uint8 slot, ItemPosCountVec& dest, uint32 entry, uint32 count, Item* pItem = NULL, bool swap = false, uint32* no_space_count = NULL) const;

        // Apply equipment cooldown
        void ApplyEquipCooldown(Item* pItem);

        // Set the ammo
        void SetAmmo(uint32 item);

        // Remove the ammo
        void RemoveAmmo();
        std::pair<float, float> GetAmmoDPS() const
        {
            return {m_ammoDPSMin, m_ammoDPSMax};
        }

        // Check if the ammo is compatible
        bool CheckAmmoCompatibility(const ItemPrototype* ammo_proto) const;

        // Quickly equip an item
        void QuickEquipItem(uint16 pos, Item* pItem);

        // Visualize an item
        void VisualizeItem(uint8 slot, Item* pItem);

        // Set the visible item slot
        void SetVisibleItemSlot(uint8 slot, Item* pItem);

        // Bank an item
        Item* BankItem(ItemPosCountVec const& dest, Item* pItem, bool update)
        {
            return StoreItem(dest, pItem, update);
        }

        // Bank an item (overloaded)
        Item* BankItem(uint16 pos, Item* pItem, bool update);

        // Remove an item
        void RemoveItem(uint8 bag, uint8 slot, bool update);

        // Move an item from the inventory
        void MoveItemFromInventory(uint8 bag, uint8 slot, bool update);

        // Move an item to the inventory
        void MoveItemToInventory(ItemPosCountVec const& dest, Item* pItem, bool update, bool in_characterInventoryDB = false);

        // Remove item-dependent auras and casts
        void RemoveItemDependentAurasAndCasts(Item* pItem);

        // Destroy an item
        void DestroyItem(uint8 bag, uint8 slot, bool update);

        // Destroy a specified count of items
        uint32 DestroyItemCount(uint32 item, uint32 count, bool update, bool unequip_check = false, bool delete_from_bank = false, bool delete_from_buyback = false);

        // Destroy a specified count of items (overloaded)
        void DestroyItemCount(Item* item, uint32& count, bool update);
        // Destroy all conjured items
        void DestroyConjuredItems(bool update);

        // Destroy items limited to a specific zone
        void DestroyZoneLimitedItem(bool update, uint32 new_zone);

        // Split an item stack into two stacks
        void SplitItem(uint16 src, uint16 dst, uint32 count);

        // Swap two items
        void SwapItem(uint16 src, uint16 dst);

        // Add an item to the buyback slot
        void AddItemToBuyBackSlot(Item* pItem);

        // Get an item from the buyback slot
        Item* GetItemFromBuyBackSlot(uint32 slot);

        // Remove an item from the buyback slot
        void RemoveItemFromBuyBackSlot(uint32 slot, bool del);

        uint32 GetMaxKeyringSize() const
        {
            return KEYRING_SLOT_END - KEYRING_SLOT_START;
        }

        // Send an equipment error message
        void SendEquipError(InventoryResult msg, Item* pItem, Item* pItem2 = NULL, uint32 itemid = 0) const;

        // Send a buy error message
        void SendBuyError(BuyResult msg, Creature* pCreature, uint32 item, uint32 param);

        // Send a sell error message
        void SendSellError(SellResult msg, Creature* pCreature, ObjectGuid itemGuid, uint32 param);
        void SendOpenContainer();
        void AddWeaponProficiency(uint32 newflag)
        {
            m_WeaponProficiency |= newflag;
        }

        // Add an armor proficiency
        void AddArmorProficiency(uint32 newflag)
        {
            m_ArmorProficiency |= newflag;
        }

        // Get the weapon proficiency
        uint32 GetWeaponProficiency() const
        {
            return m_WeaponProficiency;
        }

        // Get the armor proficiency
        uint32 GetArmorProficiency() const
        {
            return m_ArmorProficiency;
        }

        // Check if a two-handed weapon is used
        bool IsTwoHandUsed() const
        {
            Item* mainItem = GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
            return mainItem && mainItem->GetProto()->InventoryType == INVTYPE_2HWEAPON;
        }

        // Send a new item notification
        void SendNewItem(Item* item, uint32 count, bool received, bool created, bool broadcast = false, bool showInChat = true);

        // Buy an item from a vendor
        bool BuyItemFromVendor(ObjectGuid vendorGuid, uint32 item, uint8 count, uint8 bag, uint8 slot);

        // Get the reputation price discount
        float GetReputationPriceDiscount(Creature const* pCreature) const;

        // Get the trader
        Player* GetTrader() const { return m_trade ? m_trade->GetTrader() : NULL; }

        // Get the trade data
        TradeData* GetTradeData() const { return m_trade; }

        // Cancel the trade
        void TradeCancel(bool sendback);

        // Update enchantment time
        void UpdateEnchantTime(uint32 time);

        // Update item duration
        void UpdateItemDuration(uint32 time, bool realtimeonly = false);

        // Add enchantment durations
        void AddEnchantmentDurations(Item* item);

        // Remove enchantment durations
        void RemoveEnchantmentDurations(Item* item);

        // Remove all enchantments from a slot
        void RemoveAllEnchantments(EnchantmentSlot slot);

        // Add enchantment duration to an item
        void AddEnchantmentDuration(Item* item, EnchantmentSlot slot, uint32 duration);

        // Apply or remove an enchantment
        void ApplyEnchantment(Item* item, EnchantmentSlot slot, bool apply, bool apply_dur = true, bool ignore_condition = false);

        // Apply or remove all enchantments from an item
        void ApplyEnchantment(Item* item, bool apply);

        // Send enchantment durations to the client
        void SendEnchantmentDurations();

        // Add item durations
        void AddItemDurations(Item* item);

        // Remove item durations
        void RemoveItemDurations(Item* item);

        // Send item durations to the client
        void SendItemDurations();

        // Load the player's corpse
        void LoadCorpse();

        // Load the player's pet
        void LoadPet();

        uint32 m_stableSlots; // Number of stable slots

        /*********************************************************/
        /***                    GOSSIP SYSTEM                  ***/
        /*********************************************************/

        // Prepare the gossip menu
        void PrepareGossipMenu(WorldObject* pSource, uint32 menuId = 0);

        // Send the prepared gossip menu
        void SendPreparedGossip(WorldObject* pSource);
        void OnGossipSelect(WorldObject* pSource, uint32 gossipListId);


        // Get the gossip text ID for a menu
        uint32 GetGossipTextId(uint32 menuId, WorldObject* pSource);

        // Get the gossip text ID for a source
        uint32 GetGossipTextId(WorldObject* pSource);

        // Get the default gossip menu for a source
        uint32 GetDefaultGossipMenuForSource(WorldObject* pSource);

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/

        // Return player level when QuestLevel is dynamic (-1)
        uint32 GetQuestLevelForPlayer(Quest const* pQuest) const { return pQuest && (pQuest->GetQuestLevel() > 0) ? (uint32)pQuest->GetQuestLevel() : getLevel(); }

        // Prepare the quest menu
        void PrepareQuestMenu(ObjectGuid guid);

        // Send the prepared quest menu
        void SendPreparedQuest(ObjectGuid guid);

        // Check if a quest is active
        bool IsActiveQuest(uint32 quest_id) const; // can be taken or taken

        // Quest is taken and not yet rewarded
        // if completed_or_not = 0 (or any other value except 1 or 2) - returns true, if quest is taken and doesn't depend if quest is completed or not
        // if completed_or_not = 1 - returns true, if quest is taken but not completed
        // if completed_or_not = 2 - returns true, if quest is taken and already completed
        bool IsCurrentQuest(uint32 quest_id, uint8 completed_or_not = 0) const; // taken and not yet rewarded

        // Get the next quest in a chain
        Quest const* GetNextQuest(ObjectGuid guid, Quest const* pQuest);

        // Check if the player can see the start of a quest
        bool CanSeeStartQuest(Quest const* pQuest) const;

        // Check if the player can take a quest
        bool CanTakeQuest(Quest const* pQuest, bool msg) const;

        // Check if the player can add a quest
        bool CanAddQuest(Quest const* pQuest, bool msg) const;

        // Check if the player can complete a quest
        bool CanCompleteQuest(uint32 quest_id) const;

        // Check if the player can complete a repeatable quest
        bool CanCompleteRepeatableQuest(Quest const* pQuest) const;

        // Check if the player can reward a quest
        bool CanRewardQuest(Quest const* pQuest, bool msg) const;

        // Check if the player can reward a quest with a specific reward
        bool CanRewardQuest(Quest const* pQuest, uint32 reward, bool msg) const;
        // Retrieve a quest template
        // The returned quest can then be used by AddQuest( ) to add to the character_queststatus table
        Quest const* GetQuestTemplate(uint32 quest_id);
        void AddQuest(Quest const* pQuest, Object* questGiver);

        // Complete a quest
        void CompleteQuest(uint32 quest_id, QuestStatus status = QUEST_STATUS_COMPLETE);

        // Mark a quest as incomplete
        void IncompleteQuest(uint32 quest_id);

        // Reward a quest
        void RewardQuest(Quest const* pQuest, uint32 reward, Object* questGiver, bool announce = true);

        // Fail a quest
        void FailQuest(uint32 quest_id);

        // Check if the player satisfies the skill requirements for a quest
        bool SatisfyQuestSkill(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the level requirements for a quest
        bool SatisfyQuestLevel(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the quest log requirements
        bool SatisfyQuestLog(bool msg) const;

        // Check if the player satisfies the previous quest requirements
        bool SatisfyQuestPreviousQuest(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the class requirements for a quest
        bool SatisfyQuestClass(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the race requirements for a quest
        bool SatisfyQuestRace(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the reputation requirements for a quest
        bool SatisfyQuestReputation(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the status requirements for a quest
        bool SatisfyQuestStatus(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the timed requirements for a quest
        bool SatisfyQuestTimed(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the exclusive group requirements for a quest
        bool SatisfyQuestExclusiveGroup(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the next chain requirements for a quest
        bool SatisfyQuestNextChain(Quest const* qInfo, bool msg) const;

        // Check if the player satisfies the previous chain requirements for a quest
        bool SatisfyQuestPrevChain(Quest const* qInfo, bool msg) const;
        bool CanGiveQuestSourceItemIfNeed(Quest const* pQuest, ItemPosCountVec* dest = NULL) const;

        // Give the quest source item if needed
        void GiveQuestSourceItemIfNeed(Quest const* pQuest);

        // Take the quest source item
        bool TakeQuestSourceItem(uint32 quest_id, bool msg);

        // Check if the player has the quest reward status
        bool GetQuestRewardStatus(uint32 quest_id) const;

        // Get the quest status
        QuestStatus GetQuestStatus(uint32 quest_id) const;

        // Set the quest status
        void SetQuestStatus(uint32 quest_id, QuestStatus status);
        // This is used to change the quest's rewarded state
        void SetQuestRewarded(uint32 quest_id, bool rewarded);


        // Find the quest slot for a quest
        uint16 FindQuestSlot(uint32 quest_id) const;

        // Get the quest ID from a quest slot
        uint32 GetQuestSlotQuestId(uint16 slot) const { return GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET); }

        // Set the quest slot
        void SetQuestSlot(uint16 slot, uint32 quest_id, uint32 timer = 0)
        {
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_ID_OFFSET, quest_id);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNT_STATE_OFFSET, 0);
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET, timer);
        }

        // Set the quest slot counter
        void SetQuestSlotCounter(uint16 slot, uint8 counter, uint8 count)
        {
            uint32 val = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNT_STATE_OFFSET);
            val &= ~(0x3F << (counter * 6));
            val |= ((uint32)count << (counter * 6));
            SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNT_STATE_OFFSET, val);
        }
        void SetQuestSlotState(uint16 slot, uint8 state) { SetByteFlag(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNT_STATE_OFFSET, 3, state); }
        void RemoveQuestSlotState(uint16 slot, uint8 state) { RemoveByteFlag(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_COUNT_STATE_OFFSET, 3, state); }
        void SetQuestSlotTimer(uint16 slot, uint32 timer) { SetUInt32Value(PLAYER_QUEST_LOG_1_1 + slot * MAX_QUEST_OFFSET + QUEST_TIME_OFFSET, timer); }

        // Swap two quest slots
        void SwapQuestSlot(uint16 slot1, uint16 slot2)
        {
            for (int i = 0; i < MAX_QUEST_OFFSET; ++i)
            {
                uint32 temp1 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i);
                uint32 temp2 = GetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i);

                SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot1 + i, temp2);
                SetUInt32Value(PLAYER_QUEST_LOG_1_1 + MAX_QUEST_OFFSET * slot2 + i, temp1);
            }
        }

        // Get the current count for a required kill or cast
        uint32 GetReqKillOrCastCurrentCount(uint32 quest_id, int32 entry);

        // Mark an area as explored or an event as happened for a quest
        void AreaExploredOrEventHappens(uint32 questId);

        // Mark a group event as happened for a quest
        void GroupEventHappens(uint32 questId, WorldObject const* pEventObject);

        // Check if an item added satisfies a quest requirement
        void ItemAddedQuestCheck(uint32 entry, uint32 count);

        // Check if an item removed satisfies a quest requirement
        void ItemRemovedQuestCheck(uint32 entry, uint32 count);

        // Mark a monster as killed for a quest
        void KilledMonster(CreatureInfo const* cInfo, ObjectGuid guid);

        // Mark a monster as killed for a quest (with credit)
        void KilledMonsterCredit(uint32 entry, ObjectGuid guid = ObjectGuid());

        // Mark a creature or game object as casted for a quest
        void CastedCreatureOrGO(uint32 entry, ObjectGuid guid, uint32 spell_id, bool original_caster = true);

        // Mark a creature as talked to for a quest
        void TalkedToCreature(uint32 entry, ObjectGuid guid);

        // Handle money change for a quest
        void MoneyChanged(uint32 value);

        // Handle reputation change for a quest
        void ReputationChanged(FactionEntry const* factionEntry);

        // Check if the player has a quest for an item
        bool HasQuestForItem(uint32 itemid) const;

        // Check if the player has a quest for a game object
        bool HasQuestForGO(int32 GOId) const;

        // Update the world objects for quests
        void UpdateForQuestWorldObjects();

        // Check if the player can share a quest
        bool CanShareQuest(uint32 quest_id) const;

        // Send a quest complete event
        void SendQuestCompleteEvent(uint32 quest_id);

        // Send a quest reward notification
        void SendQuestReward(Quest const* pQuest, uint32 XP);

        // Send a quest failed notification
        void SendQuestFailed(uint32 quest_id);
        void SendQuestFailedAtTaker(uint32 quest_id, uint32 reason = INVALIDREASON_DONT_HAVE_REQ);
        void SendQuestTimerFailed(uint32 quest_id);

        // Send a response for the ability to take a quest
        void SendCanTakeQuestResponse(uint32 msg) const;

        // Send a quest confirm accept notification
        void SendQuestConfirmAccept(Quest const* pQuest, Player* pReceiver);
        void SendPushToPartyResponse(Player* pPlayer, uint8 msg);
        void SendQuestUpdateAddItem(Quest const* pQuest, uint32 item_idx, uint32 count);

        // Send a quest update for adding a creature or game object
        void SendQuestUpdateAddCreatureOrGo(Quest const* pQuest, ObjectGuid guid, uint32 creatureOrGO_idx, uint32 count);

        // Get the divider GUID
        ObjectGuid GetDividerGuid() const { return m_dividerGuid; }

        // Set the divider GUID
        void SetDividerGuid(ObjectGuid guid) { m_dividerGuid = guid; }

        // Clear the divider GUID
        void ClearDividerGuid() { m_dividerGuid.Clear(); }

        // Get the in-game time
        uint32 GetInGameTime() { return m_ingametime; }

        // Set the in-game time
        void SetInGameTime(uint32 time) { m_ingametime = time; }

        // Add a timed quest
        void AddTimedQuest(uint32 quest_id) { m_timedquests.insert(quest_id); }

        // Remove a timed quest
        void RemoveTimedQuest(uint32 quest_id) { m_timedquests.erase(quest_id); }

        /*********************************************************/
        /***                   LOAD SYSTEM                     ***/
        /*********************************************************/

        // Load the player from the database
        bool LoadFromDB(ObjectGuid guid, SqlQueryHolder* holder);
#ifdef ENABLE_PLAYERBOTS
        // Minimal load of the player from the database
        bool MinimalLoadFromDB(QueryResult *result, uint32 guid);
#endif

        // Get the zone ID from the database
        static uint32 GetZoneIdFromDB(ObjectGuid guid);

        // Get the level from the database
        static uint32 GetLevelFromDB(ObjectGuid guid);

        // Load the position from the database
        static bool LoadPositionFromDB(ObjectGuid guid, uint32& mapid, float& x, float& y, float& z, float& o, bool& in_flight);

        /*********************************************************/
        /***                   SAVE SYSTEM                     ***/
        /*********************************************************/

        // Save the player to the database
        void SaveToDB();

        // Save the inventory and gold to the database
        void SaveInventoryAndGoldToDB(); // fast save function for item/money cheating preventing

        // Save the gold to the database
        void SaveGoldToDB();

        // Set a uint32 value in an array
        static void SetUInt32ValueInArray(Tokens& data, uint16 index, uint32 value);

        // Save the player's position in the database
        static void SavePositionInDB(ObjectGuid guid, uint32 mapid, float x, float y, float z, float o, uint32 zone);

        // Delete a player from the database
        static void DeleteFromDB(ObjectGuid playerguid, uint32 accountId, bool updateRealmChars = true, bool deleteFinally = false);

        // Delete old characters from the database
        static void DeleteOldCharacters();

        // Delete old characters from the database, keeping characters for a specified number of days
        static void DeleteOldCharacters(uint32 keepDays);

        bool m_mailsUpdated; // Indicates if mails have been updated

        // Send a pet tame failure message
        void SendPetTameFailure(PetTameFailureReason reason);

        // Set the player's bind point
        void SetBindPoint(ObjectGuid guid);

        // Send a talent wipe confirmation message
        void SendTalentWipeConfirm(ObjectGuid guid);
        void RewardRage(uint32 damage, bool attacker);
        void SendPetSkillWipeConfirm();

        // Regenerate all resources
        void RegenerateAll();

        // Regenerate a specific power
        void Regenerate(Powers power);

        // Regenerate health
        void RegenerateHealth();

        // Set the regeneration timer
        void setRegenTimer(uint32 time)
        {
            m_regenTimer = time;
        }

        // Set the weapon change timer
        void setWeaponChangeTimer(uint32 time)
        {
            m_weaponChangeTimer = time;
        }

        // Get the player's money
        uint32 GetMoney() const
        {
            return GetUInt32Value(PLAYER_FIELD_COINAGE);
        }

        // Modify the player's money
        void ModifyMoney(int32 d);

        // Set the player's money
        void SetMoney(uint32 value)
        {
            SetUInt32Value(PLAYER_FIELD_COINAGE, value);
            MoneyChanged(value);
        }

        // Get the player's quest status map
        QuestStatusMap& getQuestStatusMap()
        {
            return mQuestStatus;
        };

        // Get the player's current selection GUID
        ObjectGuid const& GetSelectionGuid() const { return m_curSelectionGuid; }

        // Set the player's current selection GUID
        void SetSelectionGuid(ObjectGuid guid) { m_curSelectionGuid = guid; SetTargetGuid(guid); }

        // Get the player's combo points
        uint8 GetComboPoints() const { return m_comboPoints; }

        // Get the player's combo target GUID
        ObjectGuid const& GetComboTargetGuid() const { return m_comboTargetGuid; }

        // Add combo points to the player
        void AddComboPoints(Unit* target, int8 count);

        // Clear the player's combo points
        void ClearComboPoints();
        void SetComboPoints();


        // Send a mail result message
        void SendMailResult(uint32 mailId, MailResponseType mailAction, MailResponseResult mailError, uint32 equipError = 0, uint32 item_guid = 0, uint32 item_count = 0);

        // Send a new mail notification
        void SendNewMail();

        // Update the next mail delivery time and unread mails
        void UpdateNextMailTimeAndUnreads();

        // Add a new mail delivery time
        void AddNewMailDeliverTime(time_t deliver_time);

        // Remove a mail by ID
        void RemoveMail(uint32 id);

        // Add a mail to the player's mail list
        void AddMail(Mail* mail)
        {
            m_mail.push_front(mail);   // for call from WorldSession::SendMailTo
        }

        // Get the size of the player's mail list
        uint32 GetMailSize()
        {
            return m_mail.size();
        }

        // Get a mail by ID
        Mail* GetMail(uint32 id);

        // Get the beginning iterator of the player's mail list
        PlayerMails::iterator GetMailBegin()
        {
            return m_mail.begin();
        }

        // Get the end iterator of the player's mail list
        PlayerMails::iterator GetMailEnd()
        {
            return m_mail.end();
        }

        /*********************************************************/
        /*** MAILED ITEMS SYSTEM ***/
        /*********************************************************/

        uint8 unReadMails; // Number of unread mails
        time_t m_nextMailDelivereTime; // Time of the next mail delivery

        typedef UNORDERED_MAP<uint32, Item*> ItemMap;

        ItemMap mMitems; // Map of mailed items

        // Get a mailed item by ID
        Item* GetMItem(uint32 id)
        {
            ItemMap::const_iterator itr = mMitems.find(id);
            return itr != mMitems.end() ? itr->second : NULL;
        }

        // Add a mailed item
        void AddMItem(Item* it)
        {
            MANGOS_ASSERT(it);
            // ASSERT deleted, because items can be added before loading
            mMitems[it->GetGUIDLow()] = it;
        }

        // Remove a mailed item by ID
        bool RemoveMItem(uint32 id)
        {
            return mMitems.erase(id) ? true : false;
        }

        // Initialize pet spells
        void PetSpellInitialize();

        // Initialize charm spells
        void CharmSpellInitialize();

        // Initialize possess spells
        void PossessSpellInitialize();

        // Remove the pet action bar
        void RemovePetActionBar();

        // Check if the player has a specific spell
        bool HasSpell(uint32 spell) const override;

        // Check if the player has an active spell
        bool HasActiveSpell(uint32 spell) const; // show in spellbook

        // Get the state of a trainer spell
        TrainerSpellState GetTrainerSpellState(TrainerSpell const* trainer_spell, uint32 reqLevel) const;

        // Check if a spell fits the player's class and race
        bool IsSpellFitByClassAndRace(uint32 spell_id, uint32* pReqlevel = NULL) const;

        // Check if a passive-like spell needs to be cast when learned
        bool IsNeedCastPassiveLikeSpellAtLearn(SpellEntry const* spellInfo) const;

        // Check if the player is immune to a spell effect
        bool IsImmuneToSpellEffect(SpellEntry const* spellInfo, SpellEffectIndex index, bool castOnSelf) const override;

        // Knock back the player from a target
        void KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed);

        // Send proficiency information to the client
        void SendProficiency(ItemClass itemClass, uint32 itemSubclassMask);

        // Send initial spells to the client
        void SendInitialSpells();

        // Add a spell to the player
        bool addSpell(uint32 spell_id, bool active, bool learning, bool dependent, bool disabled);

        // Learn a spell
        void learnSpell(uint32 spell_id, bool dependent);
        void removeSpell(uint32 spell_id, bool disabled = false, bool learn_low_rank = true);
        void resetSpells();

        // Learn the player's default spells
        void learnDefaultSpells();

        // Learn quest-rewarded spells
        void learnQuestRewardedSpells();

        // Learn quest-rewarded spells for a specific quest
        void learnQuestRewardedSpells(Quest const* quest);

        // Learn a high-rank spell
        void learnSpellHighRank(uint32 spellid);

        // Get the player's free talent points
        uint32 GetFreeTalentPoints() const
        {
            return GetUInt32Value(PLAYER_CHARACTER_POINTS1);
        }

        // Set the player's free talent points
        void SetFreeTalentPoints(uint32 points);

        // Update the player's free talent points
        void UpdateFreeTalentPoints(bool resetIfNeed = true);

        // Reset the player's talents
        bool resetTalents(bool no_cost = false);

        // Get the cost to reset the player's talents
        uint32 resetTalentsCost() const;

        // Initialize the player's talents for their level
        void InitTalentForLevel();

        // Learn a talent
        void LearnTalent(uint32 talentId, uint32 talentRank);

        // Calculate the player's talent points
        uint32 CalculateTalentsPoints() const;

        // Get the player's free primary profession points
        uint32 GetFreePrimaryProfessionPoints() const
        {
            return GetUInt32Value(PLAYER_CHARACTER_POINTS2);
        }

        // Set the player's free primary professions
        void SetFreePrimaryProfessions(uint16 profs)
        {
            SetUInt32Value(PLAYER_CHARACTER_POINTS2, profs);
        }

        // Initialize the player's primary professions
        void InitPrimaryProfessions();

        // Get the player's spell map
        PlayerSpellMap const& GetSpellMap() const
        {
            return m_spells;
        }

        // Get the player's spell map (non-const)
        PlayerSpellMap& GetSpellMap()
        {
            return m_spells;
        }

        // Get the player's spell cooldown map
        SpellCooldowns const& GetSpellCooldownMap() const
        {
            return m_spellCooldowns;
        }

        // Add a spell modifier to the player
        void AddSpellMod(SpellModifier* mod, bool apply);

        // Check if the player is affected by a spell modifier
        bool IsAffectedBySpellmod(SpellEntry const* spellInfo, SpellModifier* mod, Spell const* spell = NULL);

        // Apply a spell modifier to a value
        template <class T> T ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, Spell const* spell = NULL);

        // Get a spell modifier for a specific operation and spell ID
        SpellModifier* GetSpellMod(SpellModOp op, uint32 spellId) const;

        // Remove spell modifiers for a specific spell
        void RemoveSpellMods(Spell const* spell);

        // Reset spell modifiers due to a canceled spell
        void ResetSpellModsDueToCanceledSpell(Spell const* spell);

        static uint32 const infinityCooldownDelay = MONTH; // used for set "infinity cooldowns" for spells and check
        static uint32 const infinityCooldownDelayCheck = MONTH / 2;

        // Check if the player has a spell cooldown
        bool HasSpellCooldown(uint32 spell_id) const
        {
            SpellCooldowns::const_iterator itr = m_spellCooldowns.find(spell_id);
            return itr != m_spellCooldowns.end() && itr->second.end > time(NULL);
        }

        // Get the delay for a spell cooldown
        time_t GetSpellCooldownDelay(uint32 spell_id) const
        {
            SpellCooldowns::const_iterator itr = m_spellCooldowns.find(spell_id);
            time_t t = time(NULL);
            return itr != m_spellCooldowns.end() && itr->second.end > t ? itr->second.end - t : 0;
        }

        // Add spell and category cooldowns
        void AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* spell = NULL, bool infinityCooldown = false);

        // Add a spell cooldown
        void AddSpellCooldown(uint32 spell_id, uint32 itemid, time_t end_time);

        // Send a cooldown event to the client
        void SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId = 0, Spell* spell = NULL);

        // Prohibit a spell school for a specific duration
        void ProhibitSpellSchool(SpellSchoolMask idSchoolMask, uint32 unTimeMs) override;

        // Remove a spell cooldown
        void RemoveSpellCooldown(uint32 spell_id, bool update = false);

        // Remove a spell category cooldown
        void RemoveSpellCategoryCooldown(uint32 cat, bool update = false);

        // Send a clear cooldown message to the client
        void SendClearCooldown(uint32 spell_id, Unit* target);

        // Get the global cooldown manager
        GlobalCooldownMgr& GetGlobalCooldownMgr()
        {
            return m_GlobalCooldownMgr;
        }

        void RemoveAllSpellCooldown();

        // Load spell cooldowns from the database
        void _LoadSpellCooldowns(QueryResult* result);

        // Save spell cooldowns to the database
        void _SaveSpellCooldowns();

        // Set resurrect request data
        void setResurrectRequestData(ObjectGuid guid, uint32 mapId, float X, float Y, float Z, uint32 health, uint32 mana)
        {
            m_resurrectGuid = guid;
            m_resurrectMap = mapId;
            m_resurrectX = X;
            m_resurrectY = Y;
            m_resurrectZ = Z;
            m_resurrectHealth = health;
            m_resurrectMana = mana;
        }

        // Clear resurrect request data
        void clearResurrectRequestData() { setResurrectRequestData(ObjectGuid(), 0, 0.0f, 0.0f, 0.0f, 0, 0); }

        // Check if resurrect is requested by a specific GUID
        bool isRessurectRequestedBy(ObjectGuid guid) const { return m_resurrectGuid == guid; }

        // Check if resurrect is requested
        bool isRessurectRequested() const { return !m_resurrectGuid.IsEmpty(); }

        // Resurrect using request data
        void ResurectUsingRequestData();

        // Get the cinematic ID
        uint32 getCinematic()
        {
            return m_cinematic;
        }

        // Set the cinematic ID
        void setCinematic(uint32 cine)
        {
            m_cinematic = cine;
        }

        // Validate action button data
        static bool IsActionButtonDataValid(uint8 button, uint32 action, uint8 type, Player* player);

        // Add an action button
        ActionButton* addActionButton(uint8 button, uint32 action, uint8 type);

        // Remove an action button
        void removeActionButton(uint8 button);

        // Send initial action buttons to the client
        void SendInitialActionButtons() const;

        PvPInfo pvpInfo;
        // Update PvP state
        void UpdatePvP(bool state, bool ovrride = false);

        // Check if the player is in Free-for-All PvP mode
        bool IsFFAPvP() const
        {
            return HasFlag(PLAYER_FLAGS, PLAYER_FLAGS_FFA_PVP);
        }

        // Set Free-for-All PvP mode
        void SetFFAPvP(bool state);

        // Update the player's zone
        void UpdateZone(uint32 newZone, uint32 newArea);

        // Update the player's area
        void UpdateArea(uint32 newArea);

        // Get the cached zone ID
        uint32 GetCachedZoneId() const
        {
            return m_zoneUpdateId;
        }

        // Update zone-dependent auras
        void UpdateZoneDependentAuras();
        void UpdateAreaDependentAuras();                    // subzones

        void UpdatePvPFlag(time_t currTime);

        // Update contested PvP state
        void UpdateContestedPvP(uint32 currTime);

        // Set the contested PvP timer
        void SetContestedPvPTimer(uint32 newTime)
        {
            m_contestedPvPTimer = newTime;
        }

        // Reset contested PvP state
        void ResetContestedPvP()
        {
            clearUnitState(UNIT_STAT_ATTACK_PLAYER);
            RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_CONTESTED_PVP);
            m_contestedPvPTimer = 0;
        }

        // Check if the player is in a duel with another player
            /** todo: -maybe move UpdateDuelFlag+DuelComplete to independent DuelHandler.. **/
        DuelInfo* duel;
        bool IsInDuelWith(Player const* player) const
        {
            return duel && duel->opponent == player && duel->startTime != 0;
        }

        // Update duel flag
        void UpdateDuelFlag(time_t currTime);

        // Check duel distance
        void CheckDuelDistance(time_t currTime);

        // Complete the duel
        void DuelComplete(DuelCompleteType type);

        // Send duel countdown
        void SendDuelCountdown(uint32 counter);

        // Check if the player is visible for another player in the group
        bool IsGroupVisibleFor(Player* p) const;

        // Check if the player is in the same group with another player
        bool IsInSameGroupWith(Player const* p) const;

        // Check if the player is in the same raid with another player
        bool IsInSameRaidWith(Player const* p) const
        {
            return p == this || (GetGroup() != NULL && GetGroup() == p->GetGroup());
        }

        // Uninvite the player from the group
        void UninviteFromGroup();
        static void RemoveFromGroup(Group* group, ObjectGuid guid, uint8 removeMethod = GROUP_LEAVE);
        void RemoveFromGroup() { RemoveFromGroup(GetGroup(), GetObjectGuid()); }

        // Send update to out-of-range group members
        void SendUpdateToOutOfRangeGroupMembers();

        // Set the player's guild ID
        void SetInGuild(uint32 GuildId)
        {
            SetUInt32Value(PLAYER_GUILDID, GuildId);
        }

        // Set the player's guild rank
        void SetRank(uint32 rankId)
        {
            SetUInt32Value(PLAYER_GUILDRANK, rankId);
        }

        // Set the guild ID the player is invited to
        void SetGuildIdInvited(uint32 GuildId)
        {
            m_GuildIdInvited = GuildId;
        }

        // Get the player's guild ID
        uint32 GetGuildId()
        {
            return GetUInt32Value(PLAYER_GUILDID);
        }

        // Get the player's guild ID from the database
        static uint32 GetGuildIdFromDB(ObjectGuid guid);

        // Get the player's guild rank
        uint32 GetRank()
        {
            return GetUInt32Value(PLAYER_GUILDRANK);
        }

        // Get the player's guild rank from the database
        static uint32 GetRankFromDB(ObjectGuid guid);

        // Get the guild ID the player is invited to
        int GetGuildIdInvited()
        {
            return m_GuildIdInvited;
        }
        static void RemovePetitionsAndSigns(ObjectGuid guid);


        // Update the player's skill
        bool UpdateSkill(uint32 skill_id, uint32 step);

        // Update the player's skill proficiency
        bool UpdateSkillPro(uint16 SkillId, int32 Chance, uint32 step);

        // Update the player's crafting skill
        bool UpdateCraftSkill(uint32 spellid);

        // Update the player's gathering skill
        bool UpdateGatherSkill(uint32 SkillId, uint32 SkillValue, uint32 RedLevel, uint32 Multiplicator = 1);

        // Update the player's fishing skill
        bool UpdateFishingSkill();

        // Get the player's base defense skill value
        uint32 GetBaseDefenseSkillValue() const
        {
            return GetPureSkillValue(SKILL_DEFENSE);
        }

        // Get the player's base weapon skill value
        uint32 GetBaseWeaponSkillValue(WeaponAttackType attType) const;

        float GetHealthBonusFromStamina();

        // Get the mana bonus from intellect
        float GetManaBonusFromIntellect();

        // Update the player's stats
        bool UpdateStats(Stats stat) override;

        // Update all of the player's stats
        bool UpdateAllStats() override;

        // Update the player's resistances
        void UpdateResistances(uint32 school) override;

        // Update the player's armor
        void UpdateArmor() override;

        // Update the player's maximum health
        void UpdateMaxHealth() override;

        // Update the player's maximum power
        void UpdateMaxPower(Powers power) override;

        // Update the player's attack power and damage
        void UpdateAttackPowerAndDamage(bool ranged = false) override;
        void UpdateDamagePhysical(WeaponAttackType attType) override;

        // Update the player's spell damage and healing bonus
        void UpdateSpellDamageAndHealingBonus();


        // Calculate the minimum and maximum damage
        void CalculateMinMaxDamage(WeaponAttackType attType, bool normalized, float& min_damage, float& max_damage);

        // Update defense bonuses modifier
        void UpdateDefenseBonusesMod();

        // Get melee critical chance from agility
        float GetMeleeCritFromAgility();

        // Get dodge chance from agility
        float GetDodgeFromAgility();

        // Get spell critical chance from intellect
        float GetSpellCritFromIntellect();

        // Get health regeneration per spirit
        float OCTRegenHPPerSpirit();

        // Get mana regeneration per spirit
        float OCTRegenMPPerSpirit();

        void UpdateBlockPercentage();

        // Update critical percentage
        void UpdateCritPercentage(WeaponAttackType attType);

        // Update all critical percentages
        void UpdateAllCritPercentages();

        // Update parry percentage
        void UpdateParryPercentage();

        // Update dodge percentage
        void UpdateDodgePercentage();


        // Update all spell critical chances
        void UpdateAllSpellCritChances();

        // Update spell critical chance for a specific school
        void UpdateSpellCritChance(uint32 school);
        void UpdateManaRegen();

        // Get the GUID of the loot
        ObjectGuid const& GetLootGuid() const
        {
            return m_lootGuid;
        }

        // Set the GUID of the loot
        void SetLootGuid(ObjectGuid const& guid)
        {
            m_lootGuid = guid;
        }

        // Remove the insignia of the player
        void RemovedInsignia(Player* looterPlr);

        // Get the player's session
        WorldSession* GetSession() const
        {
            return m_session;
        }

        // Set the player's session
        void SetSession(WorldSession* s)
        {
            m_session = s;
        }

        // Build the create update block for the player
        void BuildCreateUpdateBlockForPlayer(UpdateData* data, Player* target) const override;

        // Destroy the player for another player
        void DestroyForPlayer(Player* target) const override;

        // Send log XP gain
        void SendLogXPGain(uint32 GivenXP, Unit* victim, uint32 RestXP);

        // Get the last swing error message
        uint8 LastSwingErrorMsg() const
        {
            return m_swingErrorMsg;
        }

        // Set the swing error message
        void SwingErrorMsg(uint8 val)
        {
            m_swingErrorMsg = val;
        }

        // Notifiers for various attack swing errors
        void SendAttackSwingCantAttack();
        void SendAttackSwingCancelAttack();
        void SendAttackSwingDeadTarget();
        void SendAttackSwingNotStanding();
        void SendAttackSwingNotInRange();
        void SendAttackSwingBadFacingAttack();
        void SendAutoRepeatCancel();
        void SendExplorationExperience(uint32 Area, uint32 Experience);

        void ResetInstances(InstanceResetMethod method);

        // Send reset instance success
        void SendResetInstanceSuccess(uint32 MapId);

        // Send reset instance failed
        void SendResetInstanceFailed(uint32 reason, uint32 MapId);

        // Send reset failed notification
        void SendResetFailedNotify(uint32 mapid);

        // Set the player's position
        bool SetPosition(float x, float y, float z, float orientation, bool teleport = false);

        // Update the player's underwater state
        void UpdateUnderwaterState(Map* m, float x, float y, float z);

        // Send a message to the set of players
        void SendMessageToSet(WorldPacket* data, bool self) const override;

        // Send a message to the set of players within a range
        void SendMessageToSetInRange(WorldPacket* data, float dist, bool self) const override;

        // Send a message to the set of players within a range, with an option for own team only
        void SendMessageToSetInRange(WorldPacket* data, float dist, bool self, bool own_team_only) const;

        // Get the player's corpse
        Corpse* GetCorpse() const;

        // Spawn the player's corpse bones
        void SpawnCorpseBones();

        // Create a corpse for the player
        Corpse* CreateCorpse();

        // Kill the player
        void KillPlayer();

        // Get the resurrection spell ID
        uint32 GetResurrectionSpellId();

        // Resurrect the player
        void ResurrectPlayer(float restore_percent, bool applySickness = false);

        // Build the player repopulation
        void BuildPlayerRepop();

        // Repopulate the player at the graveyard
        void RepopAtGraveyard();

        // Handle durability loss for all items
        void DurabilityLossAll(double percent, bool inventory);

        // Handle durability loss for a specific item
        void DurabilityLoss(Item* item, double percent);

        // Handle durability points loss for all items
        void DurabilityPointsLossAll(int32 points, bool inventory);

        // Handle durability points loss for a specific item
        void DurabilityPointsLoss(Item* item, int32 points);

        // Handle durability point loss for a specific equipment slot
        void DurabilityPointLossForEquipSlot(EquipmentSlots slot);
        uint32 DurabilityRepairAll(bool cost, float discountMod);
        uint32 DurabilityRepair(uint16 pos, bool cost, float discountMod);


        // Update mirror timers
        void UpdateMirrorTimers();

        // Stop all mirror timers
        void StopMirrorTimers()
        {
            StopMirrorTimer(FATIGUE_TIMER);
            StopMirrorTimer(BREATH_TIMER);
            StopMirrorTimer(FIRE_TIMER);
        }

        // Set levitate state
        void SetLevitate(bool enable) override;

        // Set can fly state
        void SetCanFly(bool enable) override;

        // Set feather fall state
        void SetFeatherFall(bool enable) override;

        // Set hover state
        void SetHover(bool enable) override;

        // Set root state
        void SetRoot(bool enable) override;

        // Set water walk state
        void SetWaterWalk(bool enable) override;

        // Handle joining a channel
        void JoinedChannel(Channel* c);

        // Handle leaving a channel
        void LeftChannel(Channel* c);

        // Cleanup channels
        void CleanupChannels();

        // Update local channels based on the new zone
        void UpdateLocalChannels(uint32 newZone);

        // Leave the Looking For Group (LFG) channel
        void LeaveLFGChannel();

        // Update the player's defense
        void UpdateDefense();

        // Update the player's weapon skill
        void UpdateWeaponSkill(WeaponAttackType attType);

        // Update the player's combat skills
        void UpdateCombatSkills(Unit* pVictim, WeaponAttackType attType, bool defence);

        // Set the player's skill
        void SetSkill(uint16 id, uint16 currVal, uint16 maxVal, uint16 step = 0);

        // Get the maximum skill value
        uint16 GetMaxSkillValue(uint32 skill) const;

        // Get the pure maximum skill value
        uint16 GetPureMaxSkillValue(uint32 skill) const;

        // Get the skill value
        uint16 GetSkillValue(uint32 skill) const;

        // Get the base skill value
        uint16 GetBaseSkillValue(uint32 skill) const;

        // Get the pure skill value
        uint16 GetPureSkillValue(uint32 skill) const;

        // Get the permanent bonus value for a skill
        int16 GetSkillPermBonusValue(uint32 skill) const;

        // Get the temporary bonus value for a skill
        int16 GetSkillTempBonusValue(uint32 skill) const;

        // Check if the player has a specific skill
        bool HasSkill(uint32 skill) const;

        // Learn spells rewarded by a skill
        void learnSkillRewardedSpells(uint32 id, uint32 value);

        // Get the teleport destination
        WorldLocation& GetTeleportDest() { return m_teleport_dest; }

        // Check if the player is being teleported
        bool IsBeingTeleported() const { return mSemaphoreTeleport_Near || mSemaphoreTeleport_Far; }

        // Check if the player is being teleported near
        bool IsBeingTeleportedNear() const { return mSemaphoreTeleport_Near; }

        // Check if the player is being teleported far
        bool IsBeingTeleportedFar() const { return mSemaphoreTeleport_Far; }

        // Set the semaphore for near teleportation
        void SetSemaphoreTeleportNear(bool semphsetting) { mSemaphoreTeleport_Near = semphsetting; }

        // Set the semaphore for far teleportation
        void SetSemaphoreTeleportFar(bool semphsetting) { mSemaphoreTeleport_Far = semphsetting; }

        // Process delayed operations
        void ProcessDelayedOperations();

        // Check area exploration and outdoor status
        void CheckAreaExploreAndOutdoor();

        // Get the team for a specific race
        static Team TeamForRace(uint8 race);

        // Get the player's team
        Team GetTeam() const { return m_team; }

        // Get the player's team ID
        PvpTeamIndex GetTeamId() const { return m_team == ALLIANCE ? TEAM_INDEX_ALLIANCE : TEAM_INDEX_HORDE; }

        // Get the faction for a specific race
        static uint32 getFactionForRace(uint8 race);

        // Set the faction for a specific race
        void setFactionForRace(uint8 race);

        // Initialize display IDs
        void InitDisplayIds();

        // Check if the player is at group reward distance
        bool IsAtGroupRewardDistance(WorldObject const* pRewardSource) const;

        // Reward a single player at a kill
        void RewardSinglePlayerAtKill(Unit* pVictim);

        // Reward the player and group at an event
        void RewardPlayerAndGroupAtEvent(uint32 creature_id, WorldObject* pRewardSource);

        // Reward the player and group at a cast
        void RewardPlayerAndGroupAtCast(WorldObject* pRewardSource, uint32 spellid = 0);

        // Check if the player is an honor or XP target
        bool isHonorOrXPTarget(Unit* pVictim) const;

        // Get the player's reputation manager
        ReputationMgr& GetReputationMgr() { return m_reputationMgr; }

        // Get the player's reputation manager (const version)
        ReputationMgr const& GetReputationMgr() const { return m_reputationMgr; }

        // Get the player's reputation rank for a specific faction
        ReputationRank GetReputationRank(uint32 faction_id) const;

        // Reward reputation for killing a unit
        void RewardReputation(Unit* pVictim, float rate);

        // Reward reputation for completing a quest
        void RewardReputation(Quest const* pQuest);

        // Calculate the reputation gain
        int32 CalculateReputationGain(ReputationSource source, int32 rep, int32 faction, uint32 creatureOrQuestLevel = 0, bool noAuraBonus = false);

        // Update skills for the player's level
        void UpdateSkillsForLevel();

        // Update skills to the maximum for the player's level
        void UpdateSkillsToMaxSkillsForLevel();

        // Modify the skill bonus
        void ModifySkillBonus(uint32 skillid, int32 val, bool talent);

        /*********************************************************/
        /***                  HONOR SYSTEM                     ***/
        /*********************************************************/
        bool AddHonorCP(float honor, uint8 type, uint32 victim, uint8 victimType);
        void UpdateHonor();
        void ResetHonor();
        void ClearHonorInfo();
        bool RewardHonor(Unit* pVictim, uint32 groupsize);
        // Assume only Players and Units as kills
        // TYPEID_OBJECT used for CP from BG,quests etc.
        bool isKill(uint8 victimType) { return (victimType == TYPEID_UNIT || victimType == TYPEID_PLAYER); }
        uint32 CalculateTotalKills(Unit* Victim, uint32 fromDate, uint32 toDate) const;
        // Acessors of honor rank
        HonorRankInfo GetHonorRankInfo() const { return m_honor_rank; }
        void SetHonorRankInfo(HonorRankInfo rank) { m_honor_rank = rank; }
        // Acessors of total honor points
        void SetRankPoints(float rankPoints) { m_rank_points = rankPoints; }
        float GetRankPoints(void) const { return m_rank_points; }
        // Acessors of highest rank
        HonorRankInfo GetHonorHighestRankInfo() const { return m_highest_rank; }
        void SetHonorHighestRankInfo(HonorRankInfo hr) { m_highest_rank = hr; }
        // Acessors of rating
        float GetStoredHonor() const { return m_stored_honor; }
        void SetStoredHonor(float rating) { m_stored_honor = rating; }
        // Acessors of lifetime
        uint32 GetHonorStoredKills(bool honorable) const { return honorable ? m_stored_honorableKills : m_stored_dishonorableKills; }
        void SetHonorStoredKills(uint32 kills, bool honorable) { if (honorable) { m_stored_honorableKills = kills; } else { m_stored_dishonorableKills = kills; } }
        // Acessors of last week standing
        int32 GetHonorLastWeekStandingPos() const { return m_standing_pos; }
        void SetHonorLastWeekStandingPos(int32 standingPos) { m_standing_pos = standingPos; }
        void SendPvPCredit(ObjectGuid guid, uint32 rank, uint32 points);

        /*********************************************************/
        /***                  PVP SYSTEM                       ***/
        /*********************************************************/


        // End of PvP System

        // Set the player's drunk value
        void SetDrunkValue(uint16 newDrunkValue, uint32 itemid = 0);

        // Get the player's drunk value
        uint16 GetDrunkValue() const { return m_drunk; }

        // Get the drunken state by value
        static DrunkenState GetDrunkenstateByValue(uint16 value);

        // Get the player's death timer
        uint32 GetDeathTimer() const { return m_deathTimer; }

        // Get the corpse reclaim delay
        uint32 GetCorpseReclaimDelay(bool pvp) const;

        // Update the corpse reclaim delay
        void UpdateCorpseReclaimDelay();

        // Send the corpse reclaim delay
        void SendCorpseReclaimDelay(bool load = false);

        // Get the player's shield block value
        uint32 GetShieldBlockValue() const override;

        // Check if the player can parry
        bool CanParry() const { return m_canParry; }

        // Set the player's ability to parry
        void SetCanParry(bool value);

        // Check if the player can block
        bool CanBlock() const { return m_canBlock; }

        // Set the player's ability to block
        void SetCanBlock(bool value);

        // Check if the player can dual wield
        bool CanDualWield() const { return m_canDualWield; }

        // Set the player's ability to dual wield
        void SetCanDualWield(bool value) { m_canDualWield = value; }

        // in 0.12 and later in Unit
        void InitStatBuffMods()
        {
            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            {
                SetFloatValue(PLAYER_FIELD_POSSTAT0 + i, 0);
            }
            for (int i = STAT_STRENGTH; i < MAX_STATS; ++i)
            {
                SetFloatValue(PLAYER_FIELD_NEGSTAT0 + i, 0);
            }
        }
        void ApplyStatBuffMod(Stats stat, float val, bool apply) { ApplyModSignedFloatValue((val > 0 ? PLAYER_FIELD_POSSTAT0 + stat : PLAYER_FIELD_NEGSTAT0 + stat), val, apply); }
        void ApplyStatPercentBuffMod(Stats stat, float val, bool apply)
        {
            ApplyPercentModFloatValue(PLAYER_FIELD_POSSTAT0 + stat, val, apply);
            ApplyPercentModFloatValue(PLAYER_FIELD_NEGSTAT0 + stat, val, apply);
        }
        float GetPosStat(Stats stat) const { return GetFloatValue(PLAYER_FIELD_POSSTAT0 + stat); }
        float GetNegStat(Stats stat) const { return GetFloatValue(PLAYER_FIELD_NEGSTAT0 + stat); }
        float GetResistanceBuffMods(SpellSchools school, bool positive) const { return GetFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school); }
        void SetResistanceBuffMods(SpellSchools school, bool positive, float val) { SetFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school, val); }
        void ApplyResistanceBuffModsMod(SpellSchools school, bool positive, float val, bool apply) { ApplyModSignedFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school, val, apply); }
        void ApplyResistanceBuffModsPercentMod(SpellSchools school, bool positive, float val, bool apply) { ApplyPercentModFloatValue(positive ? PLAYER_FIELD_RESISTANCEBUFFMODSPOSITIVE + school : PLAYER_FIELD_RESISTANCEBUFFMODSNEGATIVE + school, val, apply); }

        void SetRegularAttackTime();

        // Set the base modifier value
        void SetBaseModValue(BaseModGroup modGroup, BaseModType modType, float value) { m_auraBaseMod[modGroup][modType] = value; }

        // Handle the base modifier value
        void HandleBaseModValue(BaseModGroup modGroup, BaseModType modType, float amount, bool apply);

        // Get the base modifier value
        float GetBaseModValue(BaseModGroup modGroup, BaseModType modType) const;

        // Get the total base modifier value
        float GetTotalBaseModValue(BaseModGroup modGroup) const;

        // Get the total percentage modifier value
        float GetTotalPercentageModValue(BaseModGroup modGroup) const { return m_auraBaseMod[modGroup][FLAT_MOD] + m_auraBaseMod[modGroup][PCT_MOD]; }

        // Apply all stat bonuses
        void _ApplyAllStatBonuses();

        // Remove all stat bonuses
        void _RemoveAllStatBonuses();

        // Apply weapon-dependent aura mods
        void _ApplyWeaponDependentAuraMods(Item* item, WeaponAttackType attackType, bool apply);

        // Apply weapon-dependent aura crit mod
        void _ApplyWeaponDependentAuraCritMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply);

        // Apply weapon-dependent aura damage mod
        void _ApplyWeaponDependentAuraDamageMod(Item* item, WeaponAttackType attackType, Aura* aura, bool apply);

        // Apply item mods
        void _ApplyItemMods(Item* item, uint8 slot, bool apply);

        // Remove all item mods
        void _RemoveAllItemMods();

        // Apply all item mods
        void _ApplyAllItemMods();

        // Apply item bonuses
        void _ApplyItemBonuses(ItemPrototype const* proto, uint8 slot, bool apply);

        // Apply ammo bonuses
        void _ApplyAmmoBonuses();
        void InitDataForForm(bool reapplyMods = false);

        // Apply or remove an equip spell from an item
        void ApplyItemEquipSpell(Item* item, bool apply, bool form_change = false);

        // Apply or remove an equip spell from a spell entry
        void ApplyEquipSpell(SpellEntry const* spellInfo, Item* item, bool apply, bool form_change = false);

        // Update equip spells when the player's form changes
        void UpdateEquipSpellsAtFormChange();

        // Cast a combat spell from an item
        void CastItemCombatSpell(Unit* Target, WeaponAttackType attType);
        void CastItemUseSpell(Item* item, SpellCastTargets const& targets);

        void SendInitWorldStates(uint32 zone);
        void SendUpdateWorldState(uint32 Field, uint32 Value);

        // Send a direct message to the client
        void SendDirectMessage(WorldPacket* data) const;

        // Send aura durations for a target to the client
        void SendAuraDurationsForTarget(Unit* target);

        // Player menu for interactions
        PlayerMenu* PlayerTalkClass;

        // List of item set effects
        std::vector<ItemSetEffect*> ItemSetEff;

        // Send loot information to the client
        void SendLoot(ObjectGuid guid, LootType loot_type);

        // Send loot release information to the client
        void SendLootRelease(ObjectGuid guid);

        // Notify the client that a loot item was removed
        void SendNotifyLootItemRemoved(uint8 lootSlot);

        // Notify the client that loot money was removed
        void SendNotifyLootMoneyRemoved();

        /*********************************************************/
        /***               BATTLEGROUND SYSTEM                 ***/
        /*********************************************************/

        // Check if the player is in a battleground
        bool InBattleGround() const { return m_bgData.bgInstanceID != 0; }

        // Check if the player is in an arena
        bool InArena() const;

        // Get the battleground ID
        uint32 GetBattleGroundId() const { return m_bgData.bgInstanceID; }

        // Get the battleground type ID
        BattleGroundTypeId GetBattleGroundTypeId() const { return m_bgData.bgTypeID; }

        // Get the battleground instance
        BattleGround* GetBattleGround() const;

        // Get the minimum level for a battleground bracket
        static uint32 GetMinLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId);

        // Get the maximum level for a battleground bracket
        static uint32 GetMaxLevelForBattleGroundBracketId(BattleGroundBracketId bracket_id, BattleGroundTypeId bgTypeId);

        // Get the battleground bracket ID from the player's level
        BattleGroundBracketId GetBattleGroundBracketIdFromLevel(BattleGroundTypeId bgTypeId) const;

        // Check if the player is in a battleground queue
        bool InBattleGroundQueue() const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId != BATTLEGROUND_QUEUE_NONE)
                {
                    return true;
                }
            return false;
        }

        // Get the battleground queue type ID
        BattleGroundQueueTypeId GetBattleGroundQueueTypeId(uint32 index) const { return m_bgBattleGroundQueueID[index].bgQueueTypeId; }

        // Get the battleground queue index
        uint32 GetBattleGroundQueueIndex(BattleGroundQueueTypeId bgQueueTypeId) const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
                {
                    return i;
                }
            return PLAYER_MAX_BATTLEGROUND_QUEUES;
        }

        // Check if the player is invited for a battleground queue type
        bool IsInvitedForBattleGroundQueueType(BattleGroundQueueTypeId bgQueueTypeId) const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
                {
                    return m_bgBattleGroundQueueID[i].invitedToInstance != 0;
                }
            return false;
        }

        // Check if the player is in a battleground queue for a specific queue type
        bool InBattleGroundQueueForBattleGroundQueueType(BattleGroundQueueTypeId bgQueueTypeId) const
        {
            return GetBattleGroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES;
        }

        // Set the battleground ID and type
        void SetBattleGroundId(uint32 val, BattleGroundTypeId bgTypeId)
        {
            m_bgData.bgInstanceID = val;
            m_bgData.bgTypeID = bgTypeId;
            m_bgData.m_needSave = true;
        }

        // Add a battleground queue ID
        uint32 AddBattleGroundQueueId(BattleGroundQueueTypeId val)
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
            {
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == BATTLEGROUND_QUEUE_NONE || m_bgBattleGroundQueueID[i].bgQueueTypeId == val)
                {
                    m_bgBattleGroundQueueID[i].bgQueueTypeId = val;
                    m_bgBattleGroundQueueID[i].invitedToInstance = 0;
                    return i;
                }
            }
            return PLAYER_MAX_BATTLEGROUND_QUEUES;
        }

        // Check if the player has a free battleground queue ID
        bool HasFreeBattleGroundQueueId()
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
                {
                    return true;
                }
            return false;
        }

        // Remove a battleground queue ID
        void RemoveBattleGroundQueueId(BattleGroundQueueTypeId val)
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
            {
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == val)
                {
                    m_bgBattleGroundQueueID[i].bgQueueTypeId = BATTLEGROUND_QUEUE_NONE;
                    m_bgBattleGroundQueueID[i].invitedToInstance = 0;
                    return;
                }
            }
        }

        // Set the invite for a battleground queue type
        void SetInviteForBattleGroundQueueType(BattleGroundQueueTypeId bgQueueTypeId, uint32 instanceId)
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].bgQueueTypeId == bgQueueTypeId)
                {
                    m_bgBattleGroundQueueID[i].invitedToInstance = instanceId;
                }
        }

        // Check if the player is invited for a specific battleground instance
        bool IsInvitedForBattleGroundInstance(uint32 instanceId) const
        {
            for (int i = 0; i < PLAYER_MAX_BATTLEGROUND_QUEUES; ++i)
                if (m_bgBattleGroundQueueID[i].invitedToInstance == instanceId)
                {
                    return true;
                }
            return false;
        }

        // Get the battleground entry point
        WorldLocation const& GetBattleGroundEntryPoint() const { return m_bgData.joinPos; }

        // Set the battleground entry point
        void SetBattleGroundEntryPoint(Player* leader = NULL);

        // Set the battleground team
        void SetBGTeam(Team team) { m_bgData.bgTeam = team; m_bgData.m_needSave = true; }

        // Get the battleground team
        Team GetBGTeam() const { return m_bgData.bgTeam ? m_bgData.bgTeam : GetTeam(); }

        // Leave the battleground
        void LeaveBattleground(bool teleportToEntryPoint = true);

        // Check if the player can join a battleground
        bool CanJoinToBattleground() const;


        // Check if the player has access to a battleground by level
        bool GetBGAccessByLevel(BattleGroundTypeId bgTypeId) const;

        // Check if the player can use a battleground object
        bool CanUseBattleGroundObject();

        // Check if the player is totally immune
        bool isTotalImmune();

        // Check if the player is in an active state for capture point capturing
        bool CanUseCapturePoint();

        /*********************************************************/
        /***                    REST SYSTEM                    ***/
        /*********************************************************/

        // Check if the player is rested
        bool isRested() const { return GetRestTime() >= 10 * IN_MILLISECONDS; }

        // Get the XP rest bonus
        uint32 GetXPRestBonus(uint32 xp);

        // Get the rest time
        uint32 GetRestTime() const { return m_restTime; }

        // Set the rest time
        void SetRestTime(uint32 v) { m_restTime = v; }

        /*********************************************************/
        /***              ENVIRONMENTAL SYSTEM                  ***/
        /*********************************************************/

        uint32 EnvironmentalDamage(EnvironmentalDamageType type, uint32 damage);

        /*********************************************************/
        /***               FLOOD FILTER SYSTEM                 ***/
        /*********************************************************/

        void UpdateSpeakTime();

        // Check if the player can speak
        bool CanSpeak() const;

        /*********************************************************/
        /***                 VARIOUS SYSTEMS                   ***/
        /*********************************************************/
        float m_modManaRegen;
        float m_modManaRegenInterrupt;

        float m_rageDecayRate;
        float m_rageDecayMultiplier;

        float m_SpellCritPercentage[MAX_SPELL_SCHOOL];
        bool HasMovementFlag(MovementFlags f) const;        // for script access to m_movementInfo.HasMovementFlag
        void UpdateFallInformationIfNeed(MovementInfo const& minfo, uint16 opcode);

        // Set fall information
        void SetFallInformation(uint32 time, float z)
        {
            m_lastFallTime = time;
            m_lastFallZ = z;
        }

        // Handle fall
        void HandleFall(MovementInfo const& movementInfo);

        // Build a teleport acknowledgment message
        void BuildTeleportAckMsg(WorldPacket& data, float x, float y, float z, float ang) const;

        // Check if the player is moving
        bool isMoving() const { return m_movementInfo.HasMovementFlag(movementFlagsMask); }

        // Check if the player is moving or turning
        bool isMovingOrTurning() const { return m_movementInfo.HasMovementFlag(movementOrTurningFlagsMask); }

        bool CanSwim() const override { return true; }
        bool CanFly() const override { return false; }
        bool IsFlying() const { return false; }
        bool IsFreeFlying() const { return false; }

        bool IsClientControl(Unit* target) const;
        void SetClientControl(Unit* target, uint8 allowMove);

        // Set the mover for the player
        void SetMover(Unit* target) { m_mover = target ? target : this; }

        // Get the mover for the player
        Unit* GetMover() const { return m_mover; }

        // Check if the player is the self mover
        bool IsSelfMover() const { return m_mover == this; }

        // Get the far sight GUID
        ObjectGuid const& GetFarSightGuid() const { return GetGuidValue(PLAYER_FARSIGHT); }

        // Get the transport for the player
        Transport* GetTransport() const { return m_transport; }

        // Set the transport for the player
        void SetTransport(Transport* t) { m_transport = t; }

        // Get the X offset of the player's position on the transport
        float GetTransOffsetX() const { return m_movementInfo.GetTransportPos()->x; }

        // Get the Y offset of the player's position on the transport
        float GetTransOffsetY() const { return m_movementInfo.GetTransportPos()->y; }

        // Get the Z offset of the player's position on the transport
        float GetTransOffsetZ() const { return m_movementInfo.GetTransportPos()->z; }

        // Get the orientation offset of the player's position on the transport
        float GetTransOffsetO() const { return m_movementInfo.GetTransportPos()->o; }

        // Get the transport time
        uint32 GetTransTime() const { return m_movementInfo.GetTransportTime(); }

        // Get the save timer
        uint32 GetSaveTimer() const { return m_nextSave; }

        // Set the save timer
        void SetSaveTimer(uint32 timer) { m_nextSave = timer; }

        // Recall position
        uint32 m_recallMap; // Map ID of the recall position
        float  m_recallX;   // X coordinate of the recall position
        float  m_recallY;   // Y coordinate of the recall position
        float  m_recallZ;   // Z coordinate of the recall position
        float  m_recallO;   // Orientation of the recall position

        // Save the recall position
        void SaveRecallPosition();

        // Set the homebind location
        void SetHomebindToLocation(WorldLocation const& loc, uint32 area_id);

        // Relocate the player to the homebind location
        void RelocateToHomebind() { SetLocationMapId(m_homebindMapId); Relocate(m_homebindX, m_homebindY, m_homebindZ); }

        // Teleport the player to the homebind location
        bool TeleportToHomebind(uint32 options = 0) { return TeleportTo(m_homebindMapId, m_homebindX, m_homebindY, m_homebindZ, GetOrientation(), options); }

        // Get an object by type mask
        Object* GetObjectByTypeMask(ObjectGuid guid, TypeMask typemask);

        // Currently visible objects at the player's client
        GuidSet m_clientGUIDs;

        // Check if an object is visible to the client
        bool HaveAtClient(WorldObject const* u) { return u == this || m_clientGUIDs.find(u->GetObjectGuid()) != m_clientGUIDs.end(); }

        // Check if the player is visible in the grid for another player
        bool IsVisibleInGridForPlayer(Player* pl) const override;

        // Check if the player is visible globally for another player
        bool IsVisibleGloballyFor(Player* pl) const;

        // Update the visibility of a target from a viewpoint
        void UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target);
        void UpdateVisibilityOf(WorldObject const* viewPoint, WorldObject* target, UpdateData& data, std::set<WorldObject*>& visibleNow);


        // Handle detection of stealthed units
        void HandleStealthedUnitsDetection();

        // Get the player's camera
        Camera& GetCamera() { return m_camera; }

        // Forced speed changes
        uint8 m_forced_speed_changes[MAX_MOVE_TYPE];

        // Check if the player has a specific at-login flag
        bool HasAtLoginFlag(AtLoginFlags f) const { return m_atLoginFlags & f; }

        // Set an at-login flag for the player
        void SetAtLoginFlag(AtLoginFlags f) { m_atLoginFlags |= f; }

        // Remove an at-login flag for the player
        void RemoveAtLoginFlag(AtLoginFlags f, bool in_db_also = false);

        // Temporarily removed pet cache
        uint32 GetTemporaryUnsummonedPetNumber() const { return m_temporaryUnsummonedPetNumber; }
        void SetTemporaryUnsummonedPetNumber(uint32 petnumber) { m_temporaryUnsummonedPetNumber = petnumber; }
        void UnsummonPetTemporaryIfAny();
        void ResummonPetTemporaryUnSummonedIfAny();
        bool IsPetNeedBeTemporaryUnsummoned() const { return !IsInWorld() || !IsAlive() || IsMounted() /*+in flight*/; }

        // Send cinematic start to the client
        void SendCinematicStart(uint32 CinematicSequenceId);
#if defined(WOTLK) || defined(CATA) || defined(MISTS)
        // Send movie start to the client
        void SendMovieStart(uint32 MovieId);
#endif

        /*********************************************************/
        /***                 INSTANCE SYSTEM                   ***/
        /*********************************************************/

        typedef UNORDERED_MAP < uint32 /*mapId*/, InstancePlayerBind > BoundInstancesMap;

        // Update the homebind time
        void UpdateHomebindTime(uint32 time);

        uint32 m_HomebindTimer;
        bool m_InstanceValid;
        // permanent binds and solo binds
        BoundInstancesMap m_boundInstances;
        InstancePlayerBind* GetBoundInstance(uint32 mapid);
        BoundInstancesMap& GetBoundInstances() { return m_boundInstances; }
        void UnbindInstance(uint32 mapid, bool unload = false);
        void UnbindInstance(BoundInstancesMap::iterator& itr, bool unload = false);
        InstancePlayerBind* BindToInstance(DungeonPersistentState* save, bool permanent, bool load = false);

        // Send raid information to the client
        void SendRaidInfo();

        // Send saved instances to the client
        void SendSavedInstances();

        // Convert instances to group
        static void ConvertInstancesToGroup(Player* player, Group* group = NULL, ObjectGuid player_guid = ObjectGuid());

        // Get the bound instance save for self or group
        DungeonPersistentState* GetBoundInstanceSaveForSelfOrGroup(uint32 mapid);

        // Get the area trigger lock status
        AreaLockStatus GetAreaTriggerLockStatus(AreaTrigger const* at, uint32& miscRequirement);
        void SendTransferAbortedByLockStatus(MapEntry const* mapEntry, AreaTrigger const* at, AreaLockStatus lockStatus, uint32 miscRequirement = 0);

        /*********************************************************/
        /***                   GROUP SYSTEM                    ***/
        /*********************************************************/

        // Get the group invite
        Group* GetGroupInvite() { return m_groupInvite; }

        // Set the group invite
        void SetGroupInvite(Group* group) { m_groupInvite = group; }

        // Get the group
        Group* GetGroup() { return m_group.getTarget(); }

        // Get the group (const version)
        const Group* GetGroup() const { return (const Group*)m_group.getTarget(); }

        // Get the group reference
        GroupReference& GetGroupRef() { return m_group; }

        // Set the group
        void SetGroup(Group* group, int8 subgroup = -1);

        // Get the subgroup
        uint8 GetSubGroup() const { return m_group.getSubGroup(); }

        // Get the group update flag
        uint32 GetGroupUpdateFlag() const { return m_groupUpdateMask; }

        // Set the group update flag
        void SetGroupUpdateFlag(uint32 flag) { m_groupUpdateMask |= flag; }

        // Get the aura update mask
        const uint64& GetAuraUpdateMask() const { return m_auraUpdateMask; }

        // Set the aura update mask
        void SetAuraUpdateMask(uint8 slot) { m_auraUpdateMask |= (uint64(1) << slot); }

        // Get the next random raid member within a radius
        Player* GetNextRandomRaidMember(float radius);

        // Check if the player can be uninvited from the group
        PartyResult CanUninviteFromGroup() const;

        // Set the battleground raid group
        void SetBattleGroundRaid(Group* group, int8 subgroup = -1);

        // Remove the player from the battleground raid group
        void RemoveFromBattleGroundRaid();

        // Get the original group
        Group* GetOriginalGroup() { return m_originalGroup.getTarget(); }

        // Get the original group reference
        GroupReference& GetOriginalGroupRef() { return m_originalGroup; }

        // Get the original subgroup
        uint8 GetOriginalSubGroup() const { return m_originalGroup.getSubGroup(); }

        // Set the original group
        void SetOriginalGroup(Group* group, int8 subgroup = -1);

        // Get the grid reference
        GridReference<Player>& GetGridRef() { return m_gridRef; }

        // Get the map reference
        MapReference& GetMapRef() { return m_mapRef; }

        // Check if the player is tapped by the player or their group
        bool IsTappedByMeOrMyGroup(Creature* creature);

        // Check if the player is allowed to loot a creature
        bool isAllowedToLoot(Creature* creature);

        bool canSeeSpellClickOn(Creature const* creature) const;

#ifdef ENABLE_PLAYERBOTS
        // Set the player bot AI
        void SetPlayerbotAI(PlayerbotAI* ai) { assert(!m_playerbotAI && !m_playerbotMgr); m_playerbotAI = ai; }

        // Get the player bot AI
        PlayerbotAI* GetPlayerbotAI() { return m_playerbotAI; }

        // Set the player bot manager
        void SetPlayerbotMgr(PlayerbotMgr* mgr) { assert(!m_playerbotAI && !m_playerbotMgr); m_playerbotMgr = mgr; }

        // Get the player bot manager
        PlayerbotMgr* GetPlayerbotMgr() { return m_playerbotMgr; }

        // Set the bot death timer
        void SetBotDeathTimer() { m_deathTimer = 0; }
#endif
        void SaveMail();
    protected:

        uint32 m_contestedPvPTimer; // Timer for contested PvP state

        /*********************************************************/
        /***               BATTLEGROUND SYSTEM                 ***/
        /*********************************************************/

        /*
        This is an array of BG queues (BgTypeIDs) in which the player is queued
        */
        struct BgBattleGroundQueueID_Rec
        {
            BattleGroundQueueTypeId bgQueueTypeId; // Battleground queue type ID
            uint32 invitedToInstance; // Instance ID the player is invited to
        };

        BgBattleGroundQueueID_Rec m_bgBattleGroundQueueID[PLAYER_MAX_BATTLEGROUND_QUEUES]; // Array of battleground queue IDs
        BGData m_bgData; // Battleground data

        /*********************************************************/
        /***                    QUEST SYSTEM                   ***/
        /*********************************************************/

        // We allow only one timed quest active at the same time. Below can then be a simple value instead of a set.
        typedef std::set<uint32> QuestSet;
        QuestSet m_timedquests; // Set of timed quests

        ObjectGuid m_dividerGuid; // Divider GUID
        uint32 m_ingametime; // In-game time

        /*********************************************************/
        /***                   LOAD SYSTEM                     ***/
        /*********************************************************/

        // Load player actions from the database
        void _LoadActions(QueryResult* result);

        // Load player auras from the database
        void _LoadAuras(QueryResult* result, uint32 timediff);

        // Load bound instances from the database
        void _LoadBoundInstances(QueryResult* result);
        void _LoadHonorCP(QueryResult* result);
        void _LoadInventory(QueryResult* result, uint32 timediff);

        // Load item loot from the database
        void _LoadItemLoot(QueryResult* result);

        // Load player mails from the database
        void _LoadMails(QueryResult* result);

        // Load mailed items from the database
        void _LoadMailedItems(QueryResult* result);

        // Load quest status from the database
        void _LoadQuestStatus(QueryResult* result);
        void _LoadGroup(QueryResult* result);

        // Load player skills from the database
        void _LoadSkills(QueryResult* result);

        // Load player spells from the database
        void _LoadSpells(QueryResult* result);

        // Load home bind information from the database
        bool _LoadHomeBind(QueryResult* result);
        void _LoadBGData(QueryResult* result);

        // Load data into a data field
        void _LoadIntoDataField(const char* data, uint32 startOffset, uint32 count);

        /*********************************************************/
        /***                   SAVE SYSTEM                     ***/
        /*********************************************************/

        // Save player actions to the database
        void _SaveActions();

        // Save player auras to the database
        void _SaveAuras();

        // Save player inventory to the database
        void _SaveInventory();
        void _SaveHonorCP();

        void _SaveQuestStatus();
        void _SaveSkills();

        // Save player spells to the database
        void _SaveSpells();

        // Save battleground data to the database
        void _SaveBGData();

        // Save player stats to the database
        void _SaveStats();

        // Set create bits for the update mask
        void _SetCreateBits(UpdateMask* updateMask, Player* target) const override;

        // Set update bits for the update mask
        void _SetUpdateBits(UpdateMask* updateMask, Player* target) const override;

        /*********************************************************/
        /***              ENVIRONMENTAL SYSTEM                 ***/
        /*********************************************************/

        // Handle sobering effect
        void HandleSobering();

        // Send mirror timer to the client
        void SendMirrorTimer(MirrorTimerType Type, uint32 MaxValue, uint32 CurrentValue, int32 Regen);

        // Stop mirror timer
        void StopMirrorTimer(MirrorTimerType Type);

        // Handle drowning effect
        void HandleDrowning(uint32 time_diff);

        // Get the maximum timer value for a mirror timer
        int32 getMaxTimer(MirrorTimerType timer);

        /*********************************************************/
        /***                  HONOR SYSTEM                     ***/
        /*********************************************************/
        HonorCPMap m_honorCP;
        HonorRankInfo m_honor_rank;
        HonorRankInfo m_highest_rank;
        float m_rank_points;
        float m_stored_honor;
        uint32 m_stored_honorableKills;
        uint32 m_stored_dishonorableKills;
        int32 m_standing_pos;

        time_t m_lastHonorUpdateTime; // Last honor update time

        // Output debug stats values
        void outDebugStatsValues() const;

        ObjectGuid m_lootGuid; // Loot GUID

        Team m_team; // Player's team
        uint32 m_nextSave; // Next save time
        time_t m_speakTime; // Last speak time
        uint32 m_speakCount; // Speak count

        uint32 m_atLoginFlags; // At-login flags

        Item* m_items[PLAYER_SLOTS_COUNT]; // Array of player items
        uint32 m_currentBuybackSlot; // Current buyback slot

        std::vector<Item*> m_itemUpdateQueue; // Item update queue
        bool m_itemUpdateQueueBlocked; // Item update queue blocked flag

        uint32 m_ExtraFlags; // Extra flags
        ObjectGuid m_curSelectionGuid; // Current selection GUID

        ObjectGuid m_comboTargetGuid; // Combo target GUID
        int8 m_comboPoints; // Combo points

        QuestStatusMap mQuestStatus; // Quest status map

        SkillStatusMap mSkillStatus; // Skill status map

        uint32 m_GuildIdInvited; // Guild ID invited
        uint32 m_ArenaTeamIdInvited; // Arena team ID invited

        PlayerMails m_mail; // Player mails
        PlayerSpellMap m_spells; // Player spells
        SpellCooldowns m_spellCooldowns; // Spell cooldowns

        GlobalCooldownMgr m_GlobalCooldownMgr; // Global cooldown manager

        float m_auraBaseMod[BASEMOD_END][MOD_END];
        ActionButtonList m_actionButtons; // Action button list

        SpellModList m_spellMods[MAX_SPELLMOD]; // Spell modifiers
        int32 m_SpellModRemoveCount; // Spell modifier remove count
        EnchantDurationList m_enchantDuration; // Enchant duration list
        ItemDurationList m_itemDuration; // Item duration list

        ObjectGuid m_resurrectGuid; // Resurrect GUID
        uint32 m_resurrectMap; // Resurrect map ID
        float m_resurrectX, m_resurrectY, m_resurrectZ; // Resurrect coordinates
        uint32 m_resurrectHealth, m_resurrectMana; // Resurrect health and mana

        WorldSession* m_session; // Player session

        typedef std::list<Channel*> JoinedChannelsList;
        JoinedChannelsList m_channels; // List of joined channels

        uint32 m_cinematic; // Cinematic ID

        TradeData* m_trade; // Trade data

        uint32 m_drunkTimer;
        uint16 m_drunk;
        uint32 m_weaponChangeTimer;


        uint32 m_zoneUpdateId; // Zone update ID
        uint32 m_zoneUpdateTimer; // Zone update timer
        uint32 m_areaUpdateId; // Area update ID
        uint32 m_positionStatusUpdateTimer; // Position status update timer

        uint32 m_deathTimer; // Death timer
        time_t m_deathExpireTime; // Death expire time

        uint32 m_restTime; // Rest time

        uint32 m_WeaponProficiency;
        uint32 m_ArmorProficiency;
        bool m_canParry;
        bool m_canBlock;
        bool m_canDualWield;
        uint8 m_swingErrorMsg;
        float m_ammoDPSMin;
        float m_ammoDPSMax;

        //////////////////// Rest System/////////////////////
        time_t time_inn_enter; // Time entered inn
        uint32 inn_trigger_id; // Inn trigger ID
        float m_rest_bonus; // Rest bonus
        RestType rest_type; // Rest type
        //////////////////// Rest System/////////////////////

        // Transports
        Transport* m_transport; // Player transport

        uint32 m_resetTalentsCost; // Reset talents cost
        time_t m_resetTalentsTime; // Reset talents time
        uint32 m_usedTalentCount; // Used talent count

        // Social
        PlayerSocial* m_social; // Player social data

        // Groups
        GroupReference m_group; // Group reference
        GroupReference m_originalGroup; // Original group reference
        Group* m_groupInvite; // Group invite
        uint32 m_groupUpdateMask; // Group update mask
        uint64 m_auraUpdateMask; // Aura update mask

        ObjectGuid m_miniPetGuid; // Mini pet GUID

        // Player summoning
        time_t m_summon_expire; // Summon expire time
        uint32 m_summon_mapid; // Summon map ID
        float m_summon_x; // Summon X coordinate
        float m_summon_y; // Summon Y coordinate
        float m_summon_z; // Summon Z coordinate

    private:
        uint32 m_created_date = 0;
        // internal common parts for CanStore/StoreItem functions
        InventoryResult _CanStoreItem_InSpecificSlot(uint8 bag, uint8 slot, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool swap, Item* pSrcItem) const;

        // Check if an item can be stored in a bag
        InventoryResult _CanStoreItem_InBag(uint8 bag, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, bool non_specialized, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const;

        // Check if an item can be stored in inventory slots
        InventoryResult _CanStoreItem_InInventorySlots(uint8 slot_begin, uint8 slot_end, ItemPosCountVec& dest, ItemPrototype const* pProto, uint32& count, bool merge, Item* pSrcItem, uint8 skip_bag, uint8 skip_slot) const;

        // Store an item in a specific position
        Item* _StoreItem(uint16 pos, Item* pItem, uint32 count, bool clone, bool update);

        // Update known currencies for the player
        void UpdateKnownCurrencies(uint32 itemId, bool apply);

        // Adjust quest required item count
        void AdjustQuestReqItemCount(Quest const* pQuest, QuestStatusData& questStatusData);

        // Set the ability to delay teleport
        void SetCanDelayTeleport(bool setting) { m_bCanDelayTeleport = setting; }

        // Check if the player has a delayed teleport
        bool IsHasDelayedTeleport() const
        {
            // We should not execute delayed teleports for now dead players but has been alive at teleport
            // because we don't want player's ghost teleported from graveyard
            return m_bHasDelayedTeleport && (IsAlive() || !m_bHasBeenAliveAtDelayedTeleport);
        }

        // Set the delayed teleport flag if possible
        bool SetDelayedTeleportFlagIfCan()
        {
            m_bHasDelayedTeleport = m_bCanDelayTeleport;
            m_bHasBeenAliveAtDelayedTeleport = IsAlive();
            return m_bHasDelayedTeleport;
        }

        // Schedule a delayed operation
        void ScheduleDelayedOperation(uint32 operation)
        {
            if (operation < DELAYED_END)
            {
                m_DelayedOperations |= operation;
            }
        }

        // The unit that is currently moving the player
        Unit* m_mover;

        // The player's camera
        Camera m_camera;

        // Grid reference for the player
        GridReference<Player> m_gridRef;

        // Map reference for the player
        MapReference m_mapRef;

#ifdef ENABLE_PLAYERBOTS
        // Player bot AI
        PlayerbotAI* m_playerbotAI;

        // Player bot manager
        PlayerbotMgr* m_playerbotMgr;
#endif

        // Homebind coordinates
        uint32 m_homebindMapId; // Map ID of the homebind location
        uint16 m_homebindAreaId; // Area ID of the homebind location
        float m_homebindX; // X coordinate of the homebind location
        float m_homebindY; // Y coordinate of the homebind location
        float m_homebindZ; // Z coordinate of the homebind location

        // Last fall time and Z coordinate
        uint32 m_lastFallTime;
        float  m_lastFallZ;

        // Last liquid type the player was in
        LiquidTypeEntry const* m_lastLiquid;

        // Mirror timers for various effects
        int32 m_MirrorTimer[MAX_TIMERS];
        uint8 m_MirrorTimerFlags;
        uint8 m_MirrorTimerFlagsLast;
        bool m_isInWater;

        // Current teleport data
        WorldLocation m_teleport_dest; // Destination of the teleport
        uint32 m_teleport_options; // Options for the teleport
        bool mSemaphoreTeleport_Near; // Semaphore for near teleport
        bool mSemaphoreTeleport_Far; // Semaphore for far teleport

        // Delayed operations
        uint32 m_DelayedOperations;
        bool m_bCanDelayTeleport; // Can delay teleport flag
        bool m_bHasDelayedTeleport; // Has delayed teleport flag
        bool m_bHasBeenAliveAtDelayedTeleport; // Has been alive at delayed teleport flag

        // Detect invisibility timer
        uint32 m_DetectInvTimer;

        // Temporary removed pet cache
        uint32 m_temporaryUnsummonedPetNumber;

        // Reputation manager for the player
        ReputationMgr  m_reputationMgr;
};

void AddItemsSetItem(Player* player, Item* item);
void RemoveItemsSetItem(Player* player, ItemPrototype const* proto);

// "the bodies of template functions must be made available in a header file"
template <class T> T Player::ApplySpellMod(uint32 spellId, SpellModOp op, T& basevalue, Spell const* spell)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        return 0;
    }
    int32 totalpct = 0;
    int32 totalflat = 0;
    for (SpellModList::iterator itr = m_spellMods[op].begin(); itr != m_spellMods[op].end(); ++itr)
    {
        SpellModifier* mod = *itr;

        if (!IsAffectedBySpellmod(spellInfo, mod, spell))
        {
            continue;
        }
        if (mod->type == SPELLMOD_FLAT)
        {
            totalflat += mod->value;
        }
        else if (mod->type == SPELLMOD_PCT)
        {
            // Skip percent mods for null basevalue (most important for spell mods with charges)
            if (basevalue == T(0))
            {
                continue;
            }

            // Special case (skip >10sec spell casts for instant cast setting)
            if (mod->op == SPELLMOD_CASTING_TIME  && basevalue >= T(10 * IN_MILLISECONDS) && mod->value <= -100)
            {
                continue;
            }

            totalpct += mod->value;
        }

        if (mod->charges > 0)
        {
            if (!spell)
            {
                spell = FindCurrentSpellBySpellId(spellId);
            }

            // Avoid double use spellmod charge by same spell
            if (!mod->lastAffected || mod->lastAffected != spell)
            {
                --mod->charges;

                if (mod->charges == 0)
                {
                    mod->charges = -1;
                    ++m_SpellModRemoveCount;
                }

                mod->lastAffected = spell;
            }
        }
    }

    float diff = (float)basevalue * (float)totalpct / 100.0f + (float)totalflat;
    basevalue = T((float)basevalue + diff);
    return T(diff);
}

#endif
