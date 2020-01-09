/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2020 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOS_DBCSTRUCTURE_H
#define MANGOS_DBCSTRUCTURE_H

#include "DBCEnums.h"
#include "Path.h"
#include "Platform/Define.h"
#include "SharedDefines.h"

#include <map>
#include <set>
#include <vector>

// Structures using to access raw DBC data and required packing to portability

// GCC have alternative #pragma pack(N) syntax and old gcc version not support pack(push,N), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack(1)
#else
#pragma pack(push,1)
#endif

/**
* \struct AreaTableEntry
* \brief Entry representing area within the game.
*
* AreaTableEntry is an entry indicating the main information about the areas within the game.
* They are used to defined XP rewards of PvP Flags.
*/
struct AreaTableEntry
{
    uint32  ID;                                             // 0        m_ID - ID of the Area within the DBC.
    uint32  mapid;                                          // 1        m_ContinentID - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, ...)
    uint32  zone;                                           // 2        m_ParentAreaID - ID of the parent area.
    uint32  exploreFlag;                                    // 3        m_AreaBit - 
    uint32  flags;                                          // 4        m_flags - 
    // 5        m_SoundProviderPref
    // 6        m_SoundProviderPrefUnderwater
    // 7        m_AmbienceID
    // 8        m_ZoneMusic
    // 9        m_IntroSound
    int32   area_level;                                     // 10       m_ExplorationLevel - Level of Area, used for XP reward calculation.
    char*   area_name[8];                                   // 11-18    m_AreaName_lang - Area Name, position is relying on locale.
    // 19 string flags
    uint32  team;                                           // 20       m_factionGroupMask - Define the faction owning the area (see AreaTeams for values).
    // 21-23    uknown/unused
    uint32  LiquidTypeOverride;                             // 24       m_liquidTypeID - Override for water type (only used for Naxxramass ATM).
};

/**
* \struct AreaTriggerEntry
* \brief Entry representing an area which need to send a specific trigger for quest/resting/..
*/
struct AreaTriggerEntry
{
    uint32    id;                                           // 0 - ID of the Area within the DBC.
    uint32    mapid;                                        // 1 - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, ...)
    float     x;                                            // 2 - X position of the Area Trigger Entry.
    float     y;                                            // 3 - Y position of the Area Trigger Entry.
    float     z;                                            // 4 - Z position of the Area Trigger Entry.
    float     radius;                                       // 5 - Radius around the Area Trigger point.
    float     box_x;                                        // 6 - extent x edge
    float     box_y;                                        // 7 - extent y edge
    float     box_z;                                        // 8 - extent z edge
    float     box_orientation;                              // 9 - extent rotation by about z axis
};

/**
* \struct AuctionHouseEntry
* \brief Entry representing the different type of Auction House existing within the game and their comission.
*/
struct AuctionHouseEntry
{
    uint32    houseId;                                      // 0        m_ID - ID of the Auction House in the DBC.
    uint32    faction;                                      // 1        m_factionID - ID of the Faction (see faction.dbc).
    uint32    depositPercent;                               // 2        m_depositRate - Percentage taken for any deposit.
    uint32    cutPercent;                                   // 3        m_consignmentRate - Percentage taken for any sell.
    // char*     name[8];                                   // 4-11     m_name_lang
    // 12 string flags
};

/**
* \struct BankBagSlotProcesEntry
* \brief Entry representing the bank bag slot price.
*/
struct BankBagSlotPricesEntry
{
    uint32  ID;                                             // 0        m_ID - ID of the Bank Bag Slot in the DBC.
    uint32  price;                                          // 1        m_Cost - Price of the Bank Bag Slot.
};

#define MAX_OUTFIT_ITEMS 12

/**
* \struct CharStartOutfitEntry
* \brief 
*
*/
struct CharStartOutfitEntry
{
    // uint32 Id;                                           // 0        m_ID
    uint32 RaceClassGender;                                 // 1        m_raceID m_classID m_sexID m_outfitID (UNIT_FIELD_BYTES_0 & 0x00FFFFFF) comparable (0 byte = race, 1 byte = class, 2 byte = gender)
    int32 ItemId[MAX_OUTFIT_ITEMS];                         // 2-13     m_ItemID
    // int32 ItemDisplayId[MAX_OUTFIT_ITEMS];               // 14-25    m_DisplayItemID not required at server side
    // int32 ItemInventorySlot[MAX_OUTFIT_ITEMS];           // 26-37    m_InventoryType not required at server side
    // uint32 Unknown1;                                     // 38, unique values (index-like with gaps ordered in other way as ids)
    // uint32 Unknown2;                                     // 39
    // uint32 Unknown3;                                     // 40
};

/**
* \struct ChatChannelsEntry
* \brief Entry representing default chat channels available in game.
*/
struct ChatChannelsEntry
{
    uint32  ChannelID;                                      // 0        m_ID - ID of the Channel in DBC.
    uint32  flags;                                          // 1        m_flags - Flags indicating the type of channel (trading, guid recruitment, ...).
    // 2        m_factionGroup
    char const*   pattern[8];                               // 3-10     m_name_lang - Channel Name (using locales).
    // 11 string flags
    // char*       name[8];                                 // 12-19    m_shortcut_lang
    // 20 string flag
};

/**
* \struct ChrClassesEntry
* \brief Entry representing the classes available in game.
*/
struct ChrClassesEntry
{
    uint32  ClassID;                                        // 0        m_ID - ID of the Char Class in DBC.
    // uint32 unk1;                                         // 1 unknown, all 1
    // uint32 flags;                                        // 2 unknown
    uint32  powerType;                                      // 3        m_DisplayPower - Power Type, (1 = Rage, 3 = Energy, 0 = Mana).
    // 4        m_petNameToken
    char const* name[8];                                    // 5-12     m_name_lang - Class Name (using locales).
    // 13 string flags
    // 14       m_filename
    uint32  spellfamily;                                    // 15       m_spellClassSet - Spell Class ID (3 = Mage, 4 = Warrior, 5 = Warlock, ...)
    // uint32 flags2;                                       // 16       m_flags (0x1 HasRelicSlot)
};

/**
* \struct ChrRacesEntry
* \brief Entry rerepsenting 
*/
struct ChrRacesEntry
{
    uint32      RaceID;                                     // 0        m_ID - ID of the Char Race in DBC.
    // 1        m_flags
    uint32      FactionID;                                  // 2        m_factionID - ID of the faction in DBC. (See Faction.dbc)
    // 3        m_ExplorationSoundID
    uint32      model_m;                                    // 4        m_MaleDisplayId - ID of the Male Display.
    uint32      model_f;                                    // 5        m_FemaleDisplayId - ID of the Female Display.
    // 6        m_ClientPrefix
    // 7        unused
    uint32      TeamID;                                     // 8        m_BaseLanguage - ID of the Major Playable Faction (7-Alliance 1-Horde).
    // 9        m_creatureType
    // 10       unused, all 836
    // 11       unused, all 1604
    // 12       m_ResSicknessSpellID
    // 13       m_SplashSoundID
    uint32      startingTaxiMask;                           // 14        Starting Taxi Max for the given Race (already discovered Taxi Nodes).
    // 15       m_clientFileString
    uint32      CinematicSequence;                          // 16       m_cinematicSequenceID - Cinematic Sequence ID.
    char*       name[8];                                    // 17-24    m_name_lang used for DBC language detection/selection 
    // 25 string flags
    // 26-27    m_facialHairCustomization[2]
    // 28       m_hairCustomization
};

/**
* \struct CinematicSequencesEntry
*/
struct CinematicSequencesEntry
{
    uint32      Id;                                         // 0        m_ID - ID in DBC.
    // uint32      unk1;                                    // 1        m_soundID
    // uint32      cinematicCamera;                         // 2        m_camera[8]
};

/**
* \struct CreatureDisplayInfoEntry
* \brief Entry representing the display info.
*/
struct CreatureDisplayInfoEntry
{
    uint32      Displayid;                                  // 0        m_ID - ID in DBC.
    // 1        m_modelID
    // 2        m_soundID
    uint32      ExtendedDisplayInfoID;                      // 3        m_extendedDisplayInfoID - Extended info (see CreatureDisplayInfoExtraEntry).
    float       scale;                                      // 4        m_creatureModelScale - Scale of the Creature.
    // 5        m_creatureModelAlpha
    // 6-8      m_textureVariation[3]
    // 9        m_portraitTextureName
    // 10       m_bloodID
    // 11       m_NPCSoundID
};

/**
* \struct CreatureDisplayInfoExtraEntry
* \brief Entry extending the CreatureDisplayInfoEntry.
*/
struct CreatureDisplayInfoExtraEntry
{
    uint32      DisplayExtraId;                             // 0        m_ID - ID in DBC.
    uint32      Race;                                       // 1        m_DisplayRaceID - Race to which it's applicable.
    // uint32    Gender;                                    // 2        m_DisplaySexID
    // uint32    SkinColor;                                 // 3        m_SkinID
    // uint32    FaceType;                                  // 4        m_FaceID
    // uint32    HairType;                                  // 5        m_HairStyleID
    // uint32    HairStyle;                                 // 6        m_HairColorID
    // uint32    BeardStyle;                                // 7        m_FacialHairID
    // uint32    Equipment[10];                             // 8-17     m_NPCItemDisplay equipped static items EQUIPMENT_SLOT_HEAD..EQUIPMENT_SLOT_HANDS, client show its by self
    // char*                                                // 18       m_BakeName CreatureDisplayExtra-*.blp
};

/**
* \struct CreatureFamilyEntry
* \brief Entry representing the different pet available for players.
*/
struct CreatureFamilyEntry
{
    uint32    ID;                                           // 0 - ID in DBC.
    float     minScale;                                     // 1 - Min Scale of creature within the game.
    uint32    minScaleLevel;                                // 2 0/1 - Minimum level for which the minScale is applicable.
    float     maxScale;                                     // 3 - Max Scale of creature within the game.
    uint32    maxScaleLevel;                                // 4 0/60 - Maximum level for which the maxScale is applicable.
    uint32    skillLine[2];                                 // 5-6 - Skill Lines (See SkillLine.dbc).
    uint32    petFoodMask;                                  // 7 - Food Mask for the given pet.
    char*     Name[8];
};

#define MAX_CREATURE_SPELL_DATA_SLOT 4

/**
* \struct CreatureSpellDataEntry
* \brief Entry representing the different spell available for player's pet.
*/
struct CreatureSpellDataEntry
{
    uint32    ID;                                           // 0        m_ID - ID in DBC.
    uint32    spellId[MAX_CREATURE_SPELL_DATA_SLOT];        // 1-4      m_spells[4] - Spell ID's (see Spell.dbc).
    // uint32    availability[MAX_CREATURE_SPELL_DATA_SLOT];// 4-7      m_availability[4]
};

/**
* \struct CreatureTypeEntry
* \brief Entry representing the different creature type available for player's pet.
*/
struct CreatureTypeEntry
{
    uint32    ID;                                           // 0        m_ID
    // char*   Name[8];                                     // 1-8      m_name_lang
    // 9 string flags
    // uint32    no_expirience;                             // 10       m_flags
};

/**
* \struct DurabilityCostsEntry
* \brief Entry representing the multipliers for item reparation cost.
*/
struct DurabilityCostsEntry
{
    uint32    Itemlvl;                                      // 0        m_ID - ID in DBC.
    uint32    multiplier[29];                               // 1-29     m_weaponSubClassCost m_armorSubClassCost
};

/**
* \struct DurabilityQualityEntry
* \brief Entry representing the quality modifier for item reparation cost.
*/
struct DurabilityQualityEntry
{
    uint32    Id;                                           // 0        m_ID - ID in DBC.
    float     quality_mod;                                  // 1        m_data - Quality modifier values.
};

/**
* \struct EmotesEntry
* \brief Entry representing the emotes available.
*/
struct EmotesEntry
{
    uint32  Id;                                             // 0        m_ID - ID in DBC.
    // char*   Name;                                        // 1        m_EmoteSlashCommand
    // uint32  AnimationId;                                 // 2        m_AnimID
    //uint32  Flags;                                          // 3        m_EmoteFlags
    uint32  EmoteType;                                      // 4        m_EmoteSpecProc (determine how emote are shown)
    //uint32  UnitStandState;                                 // 5        m_EmoteSpecProcParam
    // uint32  SoundId;                                     // 6        m_EventSoundID
};

/**
* \struct EmotesTextEntry
* \brief Entry repsenting the text for given emote.
*/
struct EmotesTextEntry
{
    uint32  Id;                                             //          m_ID - ID in DBC.
    //          m_name
    uint32  textid;                                         //          m_emoteID - ID of the text.
    //          m_emoteText
};

/**
* \struct FactionEntry
* \brief Entry representing all the factions available.
*/
struct FactionEntry
{
    uint32      ID;                                         // 0        m_ID - ID in DBC.
    int32       reputationListID;                           // 1        m_reputationIndex - ID of the Reputation List.
    uint32      BaseRepRaceMask[4];                         // 2-5      m_reputationRaceMask - 
    uint32      BaseRepClassMask[4];                        // 6-9      m_reputationClassMask
    int32       BaseRepValue[4];                            // 10-13    m_reputationBase
    uint32      ReputationFlags[4];                         // 14-17    m_reputationFlags
    uint32      team;                                       // 18       m_parentFactionID
    char*       name[8];                                    // 19-26    m_name_lang
    // 27 string flags
    // char*     description[8];                            // 28-35    m_description_lang
    // 36 string flags

    // helpers

    int GetIndexFitTo(uint32 raceMask, uint32 classMask) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if ((BaseRepRaceMask[i] == 0 || (BaseRepRaceMask[i] & raceMask)) &&
                (BaseRepClassMask[i] == 0 || (BaseRepClassMask[i] & classMask)))
                { return i; }
        }

        return -1;
    }
};

/**
* \struct FactionTemplateEntry
* \brief Entry representing the type of faction that exists.
*/
struct FactionTemplateEntry
{
    /// 0
    uint32      ID;
    /// 1
    uint32      faction;
    /// 2 specific flags for that faction
    uint32      factionFlags;
    /// 3 if mask set (see FactionMasks) then faction included in masked team
    uint32      ourMask;
    /// 4 if mask set (see FactionMasks) then faction friendly to masked team
    uint32      friendlyMask;
    /// 5 if mask set (see FactionMasks) then faction hostile to masked team
    uint32      hostileMask;
    /// 6-9
    uint32      enemyFaction[4];
    /// 10-13
    uint32      friendFaction[4];
    //-------------------------------------------------------  end structure

    // helpers
    bool IsFriendlyTo(FactionTemplateEntry const& entry) const
    {
        if (entry.faction)
        {
            for (int i = 0; i < 4; ++i)
                if (enemyFaction[i]  == entry.faction)
                    { return false; }
            for (int i = 0; i < 4; ++i)
                if (friendFaction[i] == entry.faction)
                    { return true; }
        }
        return (friendlyMask & entry.ourMask) || (ourMask & entry.friendlyMask);
    }
    bool IsHostileTo(FactionTemplateEntry const& entry) const
    {
        if (entry.faction)
        {
            for (int i = 0; i < 4; ++i)
                if (enemyFaction[i]  == entry.faction)
                    { return true; }
            for (int i = 0; i < 4; ++i)
                if (friendFaction[i] == entry.faction)
                    { return false; }
        }
        return (hostileMask & entry.ourMask) != 0;
    }
    bool IsHostileToPlayers() const { return (hostileMask & FACTION_MASK_PLAYER) != 0; }
    bool IsNeutralToAll() const
    {
        for (int i = 0; i < 4; ++i)
            if (enemyFaction[i] != 0)
                { return false; }
        return hostileMask == 0 && friendlyMask == 0;
    }
    bool IsContestedGuardFaction() const { return (factionFlags & FACTION_TEMPLATE_FLAG_CONTESTED_GUARD) != 0; }
};

/**
* \struct GameObjectDisplayInfoEntry
* \brief Entry representing the info for the game object to be displayed on the client.
*/
struct GameObjectDisplayInfoEntry
{
    uint32      Displayid;                                  // 0        m_ID - ID in DBC.
    char*       filename;                                   // 1        m_modelName - File name for  the object.
    // uint32   m_Sound[10];                                // 2-11     m_Sound
};

// All Gt* DBC store data for 100 levels, some by 100 per class/race
#define GT_MAX_LEVEL    100

/**
* \struct ItemBagFamilyEntry
* \brief Entry representing the existing bag family.
*/
struct ItemBagFamilyEntry
{
    uint32   ID;                                            // 0        m_ID
    // char*     name[8]                                    // 1-8      m_name_lang
    //                                                      // 9        name flags
};

/**
* \struct ItemClassEntry
* \brief Entry representing the item class type.
*/
struct ItemClassEntry
{
    uint32   ID;                                            // 0        m_ID
    // uint32   unk1;                                       // 1
    // uint32   unk2;                                       // 2        only weapon have 1 in field, other 0
    char*    name[8];                                       // 3-11     m_name_lang
    //                                                      // 12       name flags
};

/**
* \struct ItemRandomPropertiesEntry
* \brief Entry representing the random enchant for Items.
*/
struct ItemRandomPropertiesEntry
{
    uint32    ID;                                           // 0        m_ID
    // char*     internalName                               // 1        m_Name
    uint32    enchant_id[3];                                // 2-4      m_Enchantment
    // 5-6 unused, 0 only values, reserved for additional enchantments
    // char*     nameSuffix[8];                             // 7-14     m_name_lang
    // 15 string flags
};

/**
* \struct ItemSetEntry
* \brief Entry representing the Set of items within the game.
*/
struct ItemSetEntry
{
    // uint32    id                                         // 0        m_ID
    char*     name[8];                                      // 1-8      m_name_lang
    // 9 string flags
    // uint32    itemId[17];                                // 10-26    m_itemID
    uint32    spells[8];                                    // 27-34    m_setSpellID
    uint32    items_to_triggerspell[8];                     // 35-42    m_setThreshold
    uint32    required_skill_id;                            // 43       m_requiredSkill
    uint32    required_skill_value;                         // 44       m_requiredSkillRank
};

/**
* \struct LiquidTypeEntry
* \brief Entry representing the type of liquid within the game.
*/
struct LiquidTypeEntry
{
    uint32 Id;                                              // 0
    uint32 LiquidId;                                        // 1        23: Water; 29: Ocean; 35: Magma; 41: Slime; 47: Naxxramas - Slime.
    uint32 Type;                                            // 2        0: Magma; 2: Slime; 3: Water.
    uint32 SpellId;                                         // 3        Reference to Spell.dbc
};

#define MAX_LOCK_CASE 8

/**
* \struct LockEntry
* \brief Entry representing the different "locks" existing in game (chest, veins, herbs, ...).
*/
struct LockEntry
{
    uint32      ID;                                         // 0        m_ID
    uint32      Type[MAX_LOCK_CASE];                        // 1-5      m_Type
    uint32      Index[MAX_LOCK_CASE];                       // 9-16     m_Index
    uint32      Skill[MAX_LOCK_CASE];                       // 17-24    m_Skill
    // uint32      Action[MAX_LOCK_CASE];                   // 25-32    m_Action
};


/**
* \struct MailTemplateEntry
* \brief Entry representing a mail template for quest result.
*/
struct MailTemplateEntry
{
    uint32      ID;                                         // 0        m_ID
    // char*       subject[8];                              // 1-8      m_subject_lang
    // 9 string flags
};

/**
* \struct MapEntry
* \brief Entry representing maps existing within the game.
*/
struct MapEntry
{
    uint32  MapID;                                          // 0        m_ID
    // char*       internalname;                            // 1        m_Directory
    uint32  map_type;                                       // 2        m_InstanceType
    // uint32 isPvP;                                        // 3        m_PVP 0 or 1 for battlegrounds (not arenas)
    char*   name[8];                                        // 4-11     m_MapName_lang
    // 12 string flags
    // 13-15 unused (something PvPZone related - levels?)
    // 16-18
    uint32  linked_zone;                                    // 19       m_areaTableID
    // char*     hordeIntro[8];                             // 20-27    m_MapDescription0_lang
    // 28 string flags
    // char*     allianceIntro[8];                          // 29-36    m_MapDescription1_lang
    // 37 string flags
    uint32  multimap_id;                                    // 38       m_LoadingScreenID (LoadingScreens.dbc)
    // 39-40 not used
    // float   BattlefieldMapIconScale;                     // 41       m_minimapIconScale

    // Helpers

    bool IsDungeon() const { return map_type == MAP_INSTANCE || map_type == MAP_RAID; }
    bool IsNonRaidDungeon() const { return map_type == MAP_INSTANCE; }
    bool Instanceable() const { return map_type == MAP_INSTANCE || map_type == MAP_RAID || map_type == MAP_BATTLEGROUND; }
    bool IsRaid() const { return map_type == MAP_RAID; }
    bool IsBattleGround() const { return map_type == MAP_BATTLEGROUND; }

    bool IsMountAllowed() const
    {
        return !IsDungeon() ||
               MapID == 309 || MapID == 209 || MapID == 509 || MapID == 269;
    }

    bool IsContinent() const
    {
        return MapID == 0 || MapID == 1;
    }
};

/**
* \struct QuestSortEntry
* \brief Entry representing the type of quest within the game.
*/
struct QuestSortEntry
{
    uint32      id;                                         // 0        m_ID
    // char*       name[8];                                 // 1-8      m_SortName_lang
    // 9 string flags
};

/**
* \struct SkillRaceClassInfoEntry
* \brief Entry representing the available skills for classes (weapons, gear, ..)
*/
struct SkillRaceClassInfoEntry
{
    // uint32    id;                                        // 0        m_ID
    uint32    skillId;                                      // 1        m_skillID
    uint32    raceMask;                                     // 2        m_raceMask
    uint32    classMask;                                    // 3        m_classMask
    uint32    flags;                                        // 4        m_flags
    uint32    reqLevel;                                     // 5        m_minLevel
    // uint32    skillTierId;                               // 6        m_skillTierID
    // uint32    skillCostID;                               // 7        m_skillCostIndex
};

/**
* \struct SkillLineEntry
* \brief Entry representing the type of skill line (fire, frost, racial, ...).
*/
struct SkillLineEntry
{
    uint32    id;                                           // 0        m_ID
    int32     categoryId;                                   // 1        m_categoryID
    // uint32    skillCostID;                               // 2        m_skillCostsID
    char*     name[8];                                      // 3-10     m_displayName_lang
    // 11 string flags
    // char*     description[8];                            // 12-19    m_description_lang
    // 20 string flags
    // uint32    spellIcon;                                    // 21       m_spellIconID
};

/**
* \struct SkillLineAbilityEntry
* \brief Entry representing the skill line abilities, also contains information about learning conditions.
*/
struct SkillLineAbilityEntry
{
    uint32    id;                                           // 0, INDEX
    uint32    skillId;                                      // 1
    uint32    spellId;                                      // 2
    uint32    racemask;                                     // 3
    uint32    classmask;                                    // 4
    // uint32    racemaskNot;                               // 5 always 0 in 2.4.2
    // uint32    classmaskNot;                              // 6 always 0 in 2.4.2
    uint32    req_skill_value;                              // 7 for trade skill.not for training.
    uint32    forward_spellid;                              // 8
    uint32    learnOnGetSkill;                              // 9 can be 1 or 2 for spells learned on get skill
    uint32    max_value;                                    // 10
    uint32    min_value;                                    // 11
    // 12-13, unknown, always 0
    uint32    reqtrainpoints;                               // 14
};

/**
* \struct SoundEntriesEntry
* \brief Entry representing sound for client, used for validation.
*/
struct SoundEntriesEntry
{
    uint32    Id;                                           // 0        m_ID
    // uint32    Type;                                      // 1        m_soundType
    // char*     InternalName;                              // 2        m_name
    // char*     FileName[10];                              // 3-12     m_File[10]
    // uint32    Unk13[10];                                 // 13-22    m_Freq[10]
    // char*     Path;                                      // 23       m_DirectoryBase
    // 24       m_volumeFloat
    // 25       m_flags
    // 26       m_minDistance
    // 27       m_distanceCutoff
    // 28       m_EAXDef
};

/**
* \struct ClassFamilyMask
* \brief Used to compare spells and determine if they belong to the same family.
*/
struct ClassFamilyMask
{
    // Flags of the class family.
    uint64 Flags;

    /**
    * Default constructor.
    */
    ClassFamilyMask() : Flags(0) {}

    /**
    * Constructor taking familyFlags as parameter.
    */
    explicit ClassFamilyMask(uint64 familyFlags) : Flags(familyFlags) {}

    /**
    * function indicating whether the class is empty ( = 0) or not.
    * Returns a boolean value.
    */
    bool Empty() const { return Flags == 0; }

    /**
    * function overloading the operator !
    * Returns a boolean value.
    */
    bool operator!() const { return Empty(); }

    operator void const* () const { return Empty() ? NULL : this; } // for allow normal use in if(mask)

    /**
    * function indicating whether a familyFlags belongs to a Spell Family.
    * Does a bitwise comparison between current Flags and familyFlags given in parameter.
    * Returns a boolean value.
    * \param familyFlags The familyFlags to compare.
    */
    bool IsFitToFamilyMask(uint64 familyFlags) const
    {
        return Flags & familyFlags;
    }

    /**
    * function indicating whether a ClassFamilyMask belongs to a Spell Family.
    * Does a bitwise comparison between current Flags and mask's flags.
    * Returns a boolean value.
    * \param mask The ClassFamilyMask to compare.
    */
    bool IsFitToFamilyMask(ClassFamilyMask const& mask) const
    {
        return Flags & mask.Flags;
    }

    /**
    * function overloading the operator & for bitwise comparison.
    */
    uint64 operator& (uint64 mask) const                    // possible will removed at finish convertion code use IsFitToFamilyMask
    {
        return Flags & mask;
    }

    /**
    * function overloading operator |=.
    */
    ClassFamilyMask& operator|= (ClassFamilyMask const& mask)
    {
        Flags |= mask.Flags;
        return *this;
    }
};

#define MAX_SPELL_REAGENTS 8
#define MAX_SPELL_TOTEMS 2

/**
* \struct SpellEntry
* \brief Entry representing each spell of the game.
*
* This structure also contains flags about spell family, attributes, spell effects
* enchantement, cast conditions, proc conditions, mechanic, cast time, damage range, ...
* 
* All we need to know about spells is represented by such entry and used for every effect within the game
* such as elixir, potion, buff, heal, damage, ..
*/
struct SpellEntry
{
        uint32    Id;                                       // 0 normally counted from 0 field (but some tools start counting from 1, check this before tool use for data view!)
        uint32    School;                                   // 1 not schoolMask from 2.x - just school type so everything linked with SpellEntry::SchoolMask must be rewrited
        uint32    Category;                                 // 2
        // uint32 castUI;                                   // 3 not used
        uint32    Dispel;                                   // 4
        uint32    Mechanic;                                 // 5
        uint32    Attributes;                               // 6
        uint32    AttributesEx;                             // 7
        uint32    AttributesEx2;                            // 8
        uint32    AttributesEx3;                            // 9
        uint32    AttributesEx4;                            // 10
        uint32    Stances;                                  // 11
        uint32    StancesNot;                               // 12
        uint32    Targets;                                  // 13
        uint32    TargetCreatureType;                       // 14
        uint32    RequiresSpellFocus;                       // 15
        uint32    CasterAuraState;                          // 16
        uint32    TargetAuraState;                          // 17
        uint32    CastingTimeIndex;                         // 18
        uint32    RecoveryTime;                             // 19
        uint32    CategoryRecoveryTime;                     // 20
        uint32    InterruptFlags;                           // 21
        uint32    AuraInterruptFlags;                       // 22
        uint32    ChannelInterruptFlags;                    // 23
        uint32    procFlags;                                // 24
        uint32    procChance;                               // 25
        uint32    procCharges;                              // 26
        uint32    maxLevel;                                 // 27
        uint32    baseLevel;                                // 28
        uint32    spellLevel;                               // 29
        uint32    DurationIndex;                            // 30
        uint32    powerType;                                // 31
        uint32    manaCost;                                 // 32
        uint32    manaCostPerlevel;                         // 33
        uint32    manaPerSecond;                            // 34
        uint32    manaPerSecondPerLevel;                    // 35
        uint32    rangeIndex;                               // 36
        float     speed;                                    // 37
        uint32    modalNextSpell;                           // 38 not used
        uint32    StackAmount;                              // 39
        uint32    Totem[MAX_SPELL_TOTEMS];                  // 40-41
        int32     Reagent[MAX_SPELL_REAGENTS];              // 42-49
        uint32    ReagentCount[MAX_SPELL_REAGENTS];         // 50-57
        int32     EquippedItemClass;                        // 58 (value)
        int32     EquippedItemSubClassMask;                 // 59 (mask)
        int32     EquippedItemInventoryTypeMask;            // 60 (mask)
        uint32    Effect[MAX_EFFECT_INDEX];                 ///< 61-63 TODO DOCUMENT
        int32     EffectDieSides[MAX_EFFECT_INDEX];         // 64-66
        uint32    EffectBaseDice[MAX_EFFECT_INDEX];         // 67-69
        float     EffectDicePerLevel[MAX_EFFECT_INDEX];     // 70-72
        float     EffectRealPointsPerLevel[MAX_EFFECT_INDEX];   // 73-75
        int32     EffectBasePoints[MAX_EFFECT_INDEX];       // 76-78 (don't must be used in spell/auras explicitly, must be used cached Spell::m_currentBasePoints)
        uint32    EffectMechanic[MAX_EFFECT_INDEX];         // 79-81
        uint32    EffectImplicitTargetA[MAX_EFFECT_INDEX];  // 82-84
        uint32    EffectImplicitTargetB[MAX_EFFECT_INDEX];  // 85-87
        uint32    EffectRadiusIndex[MAX_EFFECT_INDEX];      // 88-90 - spellradius.dbc
        uint32    EffectApplyAuraName[MAX_EFFECT_INDEX];    // 91-93
        uint32    EffectAmplitude[MAX_EFFECT_INDEX];        // 94-96
        float     EffectMultipleValue[MAX_EFFECT_INDEX];    // 97-99
        uint32    EffectChainTarget[MAX_EFFECT_INDEX];      // 100-102
        uint32    EffectItemType[MAX_EFFECT_INDEX];         // 103-105
        int32     EffectMiscValue[MAX_EFFECT_INDEX];        // 106-108
        uint32    EffectTriggerSpell[MAX_EFFECT_INDEX];     // 109-111
        float     EffectPointsPerComboPoint[MAX_EFFECT_INDEX];  // 112-114
        uint32    SpellVisual;                              // 115
        // uint32    SpellVisual2                           // 116 not used
        uint32    SpellIconID;                              // 117
        uint32    activeIconID;                             // 118
        // uint32    spellPriority;                         // 119
        char*     SpellName[8];                             // 120-127
        // uint32    SpellNameFlag;                         // 128
        char*     Rank[8];                                  // 129-136
        // uint32    RankFlags;                             // 137
        // char*     Description[8];                        // 138-145 not used
        // uint32    DescriptionFlags;                      // 146     not used
        // char*     ToolTip[8];                            // 147-154 not used
        // uint32    ToolTipFlags;                          // 155     not used
        uint32    ManaCostPercentage;                       // 156
        uint32    StartRecoveryCategory;                    // 157
        uint32    StartRecoveryTime;                        // 158
        uint32    MaxTargetLevel;                           // 159
        uint32    SpellFamilyName;                          // 160
        ClassFamilyMask SpellFamilyFlags;                   // 161+162
        uint32    MaxAffectedTargets;                       // 163
        uint32    DmgClass;                                 // 164 defenseType
        uint32    PreventionType;                           // 165
        // uint32    StanceBarOrder;                        // 166 not used
        float     DmgMultiplier[MAX_EFFECT_INDEX];          // 167-169
        // uint32    MinFactionId;                          // 170 not used, and 0 in 2.4.2
        // uint32    MinReputation;                         // 171 not used, and 0 in 2.4.2
        // uint32    RequiredAuraVision;                    // 172 not used

        /**
        * function calculating the basic damage/snare/... points for a given Spell Effect.
        * Returns an int32 value representing the basic points.
        * \param eff INDEX of the Spell Effect.
        */
        int32 CalculateSimpleValue(SpellEffectIndex eff) const { return EffectBasePoints[eff] + int32(EffectBaseDice[eff]); }

        /**
        * function indicating whether a spell fits to a spell family.
        * Returns a bool value.
        * \param familyFlags The uint64 value of Spell Family Flags.
        */
        bool IsFitToFamilyMask(uint64 familyFlags) const
        {
            return SpellFamilyFlags.IsFitToFamilyMask(familyFlags);
        }

        /**
        * function indicating whether a spell fits to a spell family based on arguments.
        * Returns a bool value.
        * \param family SpellFamily to which the spell should belong to.
        * \param familyFlags The uint64 value of Spell Family Flags.
        */
        bool IsFitToFamily(SpellFamily family, uint64 familyFlags) const
        {
            return SpellFamily(SpellFamilyName) == family && IsFitToFamilyMask(familyFlags);
        }

        /**
        * function indicating whether a spell fits to a spell class family based on a ClassFamilyMask.
        * Returns a bool value.
        * \param mask ClassFamilyMask representing the class family.
        */
        bool IsFitToFamilyMask(ClassFamilyMask const& mask) const
        {
            return SpellFamilyFlags.IsFitToFamilyMask(mask);
        }

        /**
        * function indicating whether a spell fits to a spell class family based on arguments.
        * Returns a bool value.
        * \param family SpellFamily to which the spell should belong to.
        * \param masl ClassFamilyMask representing the class family.
        */
        bool IsFitToFamily(SpellFamily family, ClassFamilyMask const& mask) const
        {
            return SpellFamily(SpellFamilyName) == family && IsFitToFamilyMask(mask);
        }

        /**
        * function indicating whether a spell has an attribute doing bitwise comparison.
        * Returns a bool value.
        * \param attribute SpellAttributes to compare to actual attribute.
        */
        inline bool HasAttribute(SpellAttributes attribute) const { return Attributes & attribute; }

        /**
        * function indicating whether a spell has an attribute doing bitwise comparison.
        * Returns a bool value.
        * \param attribute SpellAttributesEx to compare to actual attributeEx.
        */
        inline bool HasAttribute(SpellAttributesEx attribute) const { return AttributesEx & attribute; }

        /**
        * function indicating whether a spell has an attribute doing bitwise comparison.
        * Returns a bool value.
        * \param attribute SpellAttributesEx2 to compare to actual attributeEx2.
        */
        inline bool HasAttribute(SpellAttributesEx2 attribute) const { return AttributesEx2 & attribute; }

        /**
        * function indicating whether a spell has an attribute doing bitwise comparison.
        * Returns a bool value.
        * \param attribute SpellAttributesEx3 to compare to actual attributeEx3.
        */
        inline bool HasAttribute(SpellAttributesEx3 attribute) const { return AttributesEx3 & attribute; }

        /**
        * function indicating whether a spell has an attribute doing bitwise comparison.
        * Returns a bool value.
        * \param attribute SpellAttributesEx4 to compare to actual attributeEx4.
        */
        inline bool HasAttribute(SpellAttributesEx4 attribute) const { return AttributesEx4 & attribute; }

        inline bool HasSpellEffect(SpellEffects effect) const
        {
            for (uint8 i = EFFECT_INDEX_0; i <= EFFECT_INDEX_2; ++i)
                if (Effect[i] == effect)
                    return true;
            return false;
        }
    private:
        // prevent creating custom entries (copy data from original in fact)
        SpellEntry(SpellEntry const&);                      // DON'T must have implementation
};

/**
* \struct SpellCastTimesEntry
* \brief Entry representing the spell cast time for a given spell.
*/
struct SpellCastTimesEntry
{
    uint32    ID;                                           // 0        m_ID
    int32     CastTime;                                     // 1        m_base
    // float     CastTimePerLevel;                          // 2        m_perLevel
    // int32     MinCastTime;                               // 3        m_minimum
};

/**
* \struct SpellRadiusEntry
* \brief Entry representing the radius of action of some spells.
*/
struct SpellRadiusEntry
{
    uint32    ID;                                           //          m_ID
    float     Radius;                                       //          m_radius
    //          m_radiusPerLevel
    // float     RadiusMax;                                 //          m_radiusMax
};

/**
* \struct SpellRangeEntry
* \brief Entry representing the spell range of spells between which the spellcast is possible.
*/
struct SpellRangeEntry
{
    uint32    ID;                                           // 0        m_ID
    float     minRange;                                     // 1        m_rangeMin
    float     maxRange;                                     // 2        m_rangeMax
    // uint32  Flags;                                       // 3        m_flags
    // char*  Name[8];                                      // 4-11     m_displayName_lang
    // uint32 NameFlags;                                    // 12 string flags
    // char*  ShortName[8];                                 // 13-20    m_displayNameShort_lang
    // uint32 NameFlags;                                    // 21 string flags
};

/**
* \struct SpellShapeshiftFormEntry
* \brief Entry representing the valid shape shift within the game (stealth, bear, ...).
*/
struct SpellShapeshiftFormEntry
{
    uint32 ID;                                              // 0        m_ID
    // uint32 buttonPosition;                               // 1        m_bonusActionBar
    // char*  Name[8];                                      // 2-9      m_name_lang
    // uint32 NameFlags;                                    // 10 string flags
    uint32 flags1;                                          // 11       m_flags
    int32  creatureType;                                    // 12       m_creatureType <=0 humanoid, other normal creature types
    // uint32 unk1;                                         // 13       m_attackIconID
};

/**
* \struct SpellDurationEntry
* \brief Entry representing the spell duration.
*/
struct SpellDurationEntry
{
    uint32    ID;                                           //          m_ID
    int32     Duration[3];                                  //          m_duration, m_durationPerLevel, m_maxDuration
};

/**
* \struct SpellFocusObjectEntry
* \brief 
*/
struct SpellFocusObjectEntry
{
    uint32    ID;                                           // 0        m_ID
    // char*     Name[8];                                   // 1-8      m_name_lang
    // 9 string flags
};

/**
* \struct SpellItemEnchantmentEntry
* \brief Entry representing the link between a Spell Trigger Enchantement and its enchant.
*/
struct SpellItemEnchantmentEntry
{
    uint32      ID;                                         // 0        m_ID
    uint32      type[3];                                    // 1-3      m_effect[3]
    uint32      amount[3];                                  // 4-6      m_effectPointsMin[3]
    // uint32      amount2[3]                               // 7-9      m_effectPointsMax[3]
    uint32      spellid[3];                                 // 10-12    m_effectArg[3]
    char*       description[8];                             // 13-20    m_name_lang[8]
    // 21 string flags
    uint32      aura_id;                                    // 22       m_itemVisual
    uint32      slot;                                       // 23       m_flags
};

/**
* \struct StableSlotPricesEntry
* \brief Entry representing the price for a stable slot.
*/
struct StableSlotPricesEntry
{
    uint32 Slot;                                            //          m_ID
    uint32 Price;                                           //          m_cost
};

#define MAX_TALENT_RANK 5

/**
* \struct TalentEntry
* \brief Entry representing the talent tree and the links between each of them (conditions, ..)
*/
struct TalentEntry
{
    uint32    TalentID;                                     // 0        m_ID
    uint32    TalentTab;                                    // 1        m_tabID (TalentTab.dbc)
    uint32    Row;                                          // 2        m_tierID
    uint32    Col;                                          // 3        m_columnIndex
    uint32    RankID[MAX_TALENT_RANK];                      // 4-8      m_spellRank
    // 9-12 part of prev field
    uint32    DependsOn;                                    // 13       m_prereqTalent (Talent.dbc)
    // 14-15 part of prev field
    uint32    DependsOnRank;                                // 16       m_prereqRank
    // 17-18 part of prev field
    // uint32  needAddInSpellBook;                          // 19       m_flags also need disable higest ranks on reset talent tree
    uint32    DependsOnSpell;                               // 20       m_requiredSpellID req.spell
};

/**
* \struct TalentTabEntry
* \brief Entry representing the available talents tab for each classes.
*/
struct TalentTabEntry
{
    uint32  TalentTabID;                                    // 0        m_ID
    // char* name[8];                                       // 1-8      m_name_lang
    // uint32  nameFlags;                                   // 9 string flags
    // unit32  spellicon;                                   // 10       m_spellIconID
    // 11       m_raceMask
    uint32  ClassMask;                                      // 12       m_classMask
    uint32  tabpage;                                        // 13       m_orderIndex
    // char* internalname;                                  // 14       m_backgroundFile
};

/**
* \struct TaxiNodesEntry
* \brief Entry representing a taxi node point coming from DBC.
*
* Each Taxi Node is used to be stored as a location for a taxi node NPC inside the game.
* The Taxi Node ID is used within a bitwise comparison with Character.taximask to determine whether the
* nearby Node is known by the player.
*
*/
struct TaxiNodesEntry
{
    uint32    ID;                                           // 0        ID - ID of the Taxi Node in DBC.
    uint32    map_id;                                       // 1        m_ContinentID - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, 30 = Alterac Valley)
    float     x;                                            // 2        m_x - X position of the Taxi Node.
    float     y;                                            // 3        m_y - Y position of the Taxi Node.
    float     z;                                            // 4        m_z - Z position of the Taxi Node.
    char*     name[8];                                      // 5-12     m_Name_lang - Name of the Taxi Node (relies on locale).
    // 13 string flags
    uint32    MountCreatureID[2];                           // 14-15    m_MountCreatureID[2] - Creature ID (indicates as well the Taxi Node type : horde[14]-alliance[15])
};


/**
* \struct TaxiPathEntry
* \brief Entry representing a taxi path between two taxi nodes.
*
* Each Taxi Path is used within the game to determine the price between 2 taxi nodes.
*/
struct TaxiPathEntry
{
    uint32    ID;                                            // 0        ID - ID of the Taxi Path in DBC.
    uint32    from;                                          // 1        m_from - ID of the Starting Taxi Node of the travel.
    uint32    to;                                            // 2        m_to - ID of the Ending Taxi Node of the travel.
    uint32    price;                                         // 3        m_price - Basic Price of the travel (Unit : Copper).
};

/**
* \struct TaxiPathNodeEntry
* \brief Entry representing a Taxi Path Node - It is not loaded from the DBC but generated from it.
*/
struct TaxiPathNodeEntry
{
    // 0        m_ID - ID in the DBC.
    uint32    path;                                         // 1        m_PathID - ID of the path in the DBC.
    uint32    index;                                        // 2        m_NodeIndex - Index of the Node in the path.
    uint32    mapid;                                        // 3        m_ContinentID - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, 30 = Alterac Valley)
    float     x;                                            // 4        m_LocX - X position of the Node.
    float     y;                                            // 5        m_LocY - Y position of the Node.
    float     z;                                            // 6        m_LocZ - Z position of the Node.
    uint32    actionFlag;                                   // 7        m_flags - Unknown usage.
    uint32    delay;                                        // 8        m_delay - Unknown usage.
};

/**
* \struct WMOAreaTableEntry
* \brief Entry representing the links between area, area's name, area's location, ...
*/
struct WMOAreaTableEntry
{
    uint32 Id;                                              // 0        m_ID index
    int32 rootId;                                           // 1        m_WMOID used in root WMO
    int32 adtId;                                            // 2        m_NameSetID used in adt file
    int32 groupId;                                          // 3        m_WMOGroupID used in group WMO
    // uint32 field4;                                       // 4        m_SoundProviderPref
    // uint32 field5;                                       // 5        m_SoundProviderPrefUnderwater
    // uint32 field6;                                       // 6        m_AmbienceID
    // uint32 field7;                                       // 7        m_ZoneMusic
    // uint32 field8;                                       // 8        m_IntroSound
    uint32 Flags;                                           // 9        m_flags (used for indoor/outdoor determination)
    uint32 areaId;                                          // 10       m_AreaTableID (AreaTable.dbc)
    // char *Name[8];                                       //          m_AreaName_lang
    // uint32 nameFlags;
};

/**
* \struct WorldMapAreaEntry
* \brief Entry representing the location of World Map Area.
*/
struct WorldMapAreaEntry
{
    // uint32  ID;                                          // 0        m_ID
    uint32  map_id;                                         // 1        m_mapID
    uint32  area_id;                                        // 2        m_areaID index (continent 0 areas ignored)
    // char* internal_name                                  // 3        m_areaName
    float   y1;                                             // 4        m_locLeft
    float   y2;                                             // 5        m_locRight
    float   x1;                                             // 6        m_locTop
    float   x2;                                             // 7        m_locBottom
};

/**
* \struct WorldSafeLocsEntry
* \brief Entry representing safe location within the world.
*/
struct WorldSafeLocsEntry
{
    uint32    ID;                                           // 0        m_ID
    uint32    map_id;                                       // 1        m_continent
    float     x;                                            // 2        m_locX
    float     y;                                            // 3        m_locY
    float     z;                                            // 4        m_locZ
    // char*   name[8]                                      // 5-12     m_AreaName_lang
    // 13 string flags
};

// GCC have alternative #pragma pack() syntax and old gcc version not support pack(pop), also any gcc version not support it at some platform
#if defined( __GNUC__ )
#pragma pack()
#else
#pragma pack(pop)
#endif

typedef std::set<uint32> SpellCategorySet;
typedef std::map<uint32, SpellCategorySet > SpellCategoryStore;
typedef std::set<uint32> PetFamilySpellsSet;
typedef std::map<uint32, PetFamilySpellsSet > PetFamilySpellsStore;

// Structures not used for casting to loaded DBC data and not required then packing
struct TalentSpellPos
{
    TalentSpellPos() : talent_id(0), rank(0) {}
    TalentSpellPos(uint16 _talent_id, uint8 _rank) : talent_id(_talent_id), rank(_rank) {}

    uint16 talent_id;
    uint8  rank;
};

typedef std::map<uint32, TalentSpellPos> TalentSpellPosMap;

struct TaxiPathBySourceAndDestination
{
    TaxiPathBySourceAndDestination() : ID(0), price(0) {}
    TaxiPathBySourceAndDestination(uint32 _id, uint32 _price) : ID(_id), price(_price) {}

    uint32    ID;
    uint32    price;
};
typedef std::map<uint32, TaxiPathBySourceAndDestination> TaxiPathSetForSource;
typedef std::map<uint32, TaxiPathSetForSource> TaxiPathSetBySource;

struct TaxiPathNodePtr
{
    TaxiPathNodePtr() : i_ptr(NULL) {}
    TaxiPathNodePtr(TaxiPathNodeEntry const* ptr) : i_ptr(ptr) {}

    TaxiPathNodeEntry const* i_ptr;

    operator TaxiPathNodeEntry const& () const { return *i_ptr; }
};

typedef Path<TaxiPathNodePtr, TaxiPathNodeEntry const> TaxiPathNodeList;
typedef std::vector<TaxiPathNodeList> TaxiPathNodesByPath;

#define TaxiMaskSize 8
typedef uint32 TaxiMask[TaxiMaskSize];
#endif
