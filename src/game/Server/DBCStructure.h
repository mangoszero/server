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
    uint32  ContinentID;                                    // 1        m_ContinentID - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, ...)
    uint32  ParentAreaID;                                   // 2        m_ParentAreaID - ID of the parent area.
    uint32  AreaBit;                                        // 3        m_AreaBit -
    uint32  Flags;                                          // 4        m_flags -
    // uint32    SoundProviderPref;                         // 5        SoundProviderPref (uint) - server-unused
    // uint32    SoundProviderPrefUnderwater;               // 6        SoundProviderPrefUnderwater (uint) - server-unused
    // uint32    AmbienceID;                                // 7        AmbienceID (uint) - server-unused
    // uint32    ZoneMusic;                                 // 8        ZoneMusic (uint) - server-unused
    // uint32    IntroSound;                                // 9        IntroSound (uint) - server-unused
    int32   ExplorationLevel;                               // 10       m_ExplorationLevel - Level of Area, used for XP reward calculation.
    char*   AreaName_lang[8];                               // 11-18    m_AreaName_lang - Area Name, position is relying on locale.
    // uint32    AreaName_lang_flags;                       // 19       AreaName_lang_flags (uint) - server-unused
    uint32  FactionGroupMask;                               // 20       m_factionGroupMask - Define the faction owning the area (see AreaTeams for values).
    // int32     LiquidTypeID_0;                            // 21       LiquidTypeID_0 (int) - server-unused
    // int32     LiquidTypeID_1;                            // 22       LiquidTypeID_1 (int) - server-unused
    // int32     LiquidTypeID_2;                            // 23       LiquidTypeID_2 (int) - server-unused
    uint32  LiquidTypeID_3;                                 // 24       m_liquidTypeID - Override for water type (only used for Naxxramass ATM).
};

/**
 * \struct AreaTriggerEntry
 * \brief Entry representing an area which need to send a specific trigger for quest/resting/..
 */
struct AreaTriggerEntry
{
    uint32    ID;                                           // 0 - ID of the Area within the DBC.
    uint32    mapid;                                        // 1 - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, ...)
    float     x;                                            // 2 - X position of the Area Trigger Entry.
    float     y;                                            // 3 - Y position of the Area Trigger Entry.
    float     z;                                            // 4 - Z position of the Area Trigger Entry.
    float     radius;                                       // 5 - Radius around the Area Trigger point.
    float     box_length;                                   // 6 - extent x edge
    float     box_width;                                    // 7 - extent y edge
    float     box_height;                                   // 8 - extent z edge
    float     box_yaw;                                      // 9 - extent rotation by about z axis
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
    // char*     Name_lang;                                 // 4        Name_lang (string) - server-unused
    // char*     Name_lang_loc2;                             // 5        Name_lang_loc2 (string) - server-unused
    // char*     Name_lang_loc3;                             // 6        Name_lang_loc3 (string) - server-unused
    // char*     Name_lang_loc4;                             // 7        Name_lang_loc4 (string) - server-unused
    // char*     Name_lang_loc5;                             // 8        Name_lang_loc5 (string) - server-unused
    // char*     Name_lang_loc6;                             // 9        Name_lang_loc6 (string) - server-unused
    // char*     Name_lang_loc7;                             // 10       Name_lang_loc7 (string) - server-unused
    // char*     Name_lang_loc8;                             // 11       Name_lang_loc8 (string) - server-unused
    // uint32    Name_lang_flags;                            // 12       Name_lang_flags (uint) - server-unused
};

/**
 * \struct BankBagSlotProcesEntry
 * \brief Entry representing the bank bag slot price.
 */
struct BankBagSlotPricesEntry
{
    uint32  ID;                                             // 0        m_ID - ID of the Bank Bag Slot in the DBC.
    uint32  Price;                                          // 1        Price - Price of the Bank Bag Slot.
};

#define MAX_OUTFIT_ITEMS 12

/**
 * \struct CharStartOutfitEntry
 * \brief
 *
 */
struct CharStartOutfitEntry
{
    // uint32 Id;                                           // 0        m_ID (index, not stored)
    uint8  RaceID;                                          // 1        m_raceID
    uint8  ClassID;                                         // 2        m_classID
    uint8  SexID;                                           // 3        m_sexID
    uint8  OutfitID;                                        // 4        m_outfitID (kept active to 4-align the byte group; server keys on race/class/sex)
    int32  ItemID[MAX_OUTFIT_ITEMS];                        // 5-16     ItemID
    // int32 ItemDisplayId[MAX_OUTFIT_ITEMS];               // 17-28    m_DisplayItemID   - server-unused ('x'); ready to activate
    // int32 ItemInventorySlot[MAX_OUTFIT_ITEMS];           // 29-40    m_InventoryType   - server-unused ('x'); ready to activate
    // NOTE: 41 fields / 152 bytes. The former Unknown1-3 (fields 38-40) did not exist -
    //       they were an artefact of modelling the four byte fields as one uint32.
};  // sizeof == 52 == GetFormatRecordSize(CharStartOutfitEntryfmt)

/**
 * \struct ChatChannelsEntry
 * \brief Entry representing default chat channels available in game.
 */
struct ChatChannelsEntry
{
    uint32  ID;                                             // 0        ID - ID of the Channel in DBC.
    uint32  Flags;                                          // 1        Flags - Flags indicating the type of channel (trading, guid recruitment, ...).
    // int32   FactionGroup;                                // 2        FactionGroup (int) - server-unused
    char const*   Name_lang[8];                             // 3-10     Name_lang - Channel Name (using locales).
    // uint32  Name_lang_flags;                              // 11       Name_lang_flags (uint) - server-unused
    // char*   Shortcut_lang;                                // 12       Shortcut_lang (string) - server-unused
    // char*   Shortcut_lang_loc2;                           // 13       Shortcut_lang_loc2 (string) - server-unused
    // char*   Shortcut_lang_loc3;                           // 14       Shortcut_lang_loc3 (string) - server-unused
    // char*   Shortcut_lang_loc4;                           // 15       Shortcut_lang_loc4 (string) - server-unused
    // char*   Shortcut_lang_loc5;                           // 16       Shortcut_lang_loc5 (string) - server-unused
    // char*   Shortcut_lang_loc6;                           // 17       Shortcut_lang_loc6 (string) - server-unused
    // char*   Shortcut_lang_loc7;                           // 18       Shortcut_lang_loc7 (string) - server-unused
    // char*   Shortcut_lang_loc8;                           // 19       Shortcut_lang_loc8 (string) - server-unused
    // uint32  Shortcut_lang_flags;                          // 20       Shortcut_lang_flags (uint) - server-unused
};

/**
 * \struct ChrClassesEntry
 * \brief Entry representing the classes available in game.
 */
struct ChrClassesEntry
{
    uint32  ID;                                             // 0        ID - ID of the Char Class in DBC.
    // uint32 Unknown1;                                     // 1        Unknown1 (uint) - server-unused
    // uint32 DamageBonusStat;                              // 2        DamageBonusStat (uint) - server-unused
    uint32  DisplayPower;                                   // 3        DisplayPower - Power Type, (1 = Rage, 3 = Energy, 0 = Mana).
    // char*  PetNameToken;                                 // 4        PetNameToken (string) - server-unused
    char const* Name_lang[8];                               // 5-12     Name_lang - Class Name (using locales).
    // uint32 Name_lang_flags;                              // 13       Name_lang_flags (uint) - server-unused
    // char*  Filename;                                     // 14       Filename (string) - server-unused
    uint32  SpellClassSet;                                  // 15       SpellClassSet - Spell Class ID (3 = Mage, 4 = Warrior, 5 = Warlock, ...)
    // uint32 Flags;                                        // 16       Flags (uint) - server-unused
};

/**
 * \struct ChrRacesEntry
 * \brief Entry rerepsenting
 */
struct ChrRacesEntry
{
    uint32      ID;                                        // 0        m_ID - ID of the Char Race in DBC.
    // uint32   Flags;                                      // 1        Flags (uint) - server-unused
    uint32      FactionID;                                  // 2        m_factionID - ID of the faction in DBC. (See Faction.dbc)
    // uint32   ExplorationSoundID;                         // 3        ExplorationSoundID (uint) - server-unused
    uint32      MaleDisplayID;                              // 4        m_MaleDisplayId - ID of the Male Display.
    uint32      FemaleDisplayID;                            // 5        m_FemaleDisplayId - ID of the Female Display.
    // char*    ClientPrefix;                               // 6        ClientPrefix (string) - server-unused
    // float    MountScale;                                 // 7        MountScale (float) - server-unused
    uint32      BaseLanguage;                               // 8        m_BaseLanguage - ID of the Major Playable Faction (7-Alliance 1-Horde).
    // uint32   CreatureType;                               // 9        CreatureType (uint) - server-unused
    // uint32   LoginEffectSpellID;                         // 10       LoginEffectSpellID (uint) - server-unused
    // uint32   CombatStunSpellID;                          // 11       CombatStunSpellID (uint) - server-unused
    // uint32   ResSicknessSpellID;                         // 12       ResSicknessSpellID (uint) - server-unused
    // uint32   SplashSoundID;                              // 13       SplashSoundID (uint) - server-unused
    uint32      StartingTaxiNodes;                          // 14        Starting Taxi Max for the given Race (already discovered Taxi Nodes).
    // char*    ClientFileString;                           // 15       ClientFileString (string) - server-unused
    uint32      CinematicSequence;                          // 16       m_cinematicSequenceID - Cinematic Sequence ID.
    char*       Name_lang[8];                               // 17-24    m_name_lang used for DBC language detection/selection
    // uint32   Name_lang_flags;                            // 25       Name_lang_flags (uint) - server-unused
    // char*    FacialHairCustomization_0;                  // 26       FacialHairCustomization_0 (string) - server-unused
    // char*    FacialHairCustomization_1;                  // 27       FacialHairCustomization_1 (string) - server-unused
    // char*    HairCustomization;                          // 28       HairCustomization (string) - server-unused
};

/**
 * \struct CinematicSequencesEntry
 */
struct CinematicSequencesEntry
{
    uint32      ID;                                         // 0        ID - ID in DBC.
    // uint32      SoundID;                                 // 1        SoundID (uint) - server-unused
    // uint32      Camera1;                                 // 2        Camera1 (uint) - server-unused
    // uint32      Camera2;                                 // 3        Camera2 (uint) - server-unused
    // uint32      Camera3;                                 // 4        Camera3 (uint) - server-unused
    // uint32      Camera4;                                 // 5        Camera4 (uint) - server-unused
    // uint32      Camera5;                                 // 6        Camera5 (uint) - server-unused
    // uint32      Camera6;                                 // 7        Camera6 (uint) - server-unused
    // uint32      Camera7;                                 // 8        Camera7 (uint) - server-unused
    // uint32      Camera8;                                 // 9        Camera8 (uint) - server-unused
};

/**
 * \struct CreatureDisplayInfoEntry
 * \brief Entry representing the display info.
 */
struct CreatureDisplayInfoEntry
{
    uint32      ID;                                         // 0        ID - ID in DBC.
    // uint32      ModelID;                                 // 1        ModelID (uint) - server-unused
    // uint32      SoundID;                                 // 2        SoundID (uint) - server-unused
    uint32      ExtendedDisplayInfoID;                      // 3        m_extendedDisplayInfoID - Extended info (see CreatureDisplayInfoExtraEntry).
    float       CreatureModelScale;                         // 4        CreatureModelScale - Scale of the Creature.
    // int32       CreatureModelAlpha;                      // 5        CreatureModelAlpha (int) - server-unused
    // char*       TextureVariation_0;                      // 6        TextureVariation_0 (string) - server-unused
    // char*       TextureVariation_1;                      // 7        TextureVariation_1 (string) - server-unused
    // char*       TextureVariation_2;                      // 8        TextureVariation_2 (string) - server-unused
    // int32       SizeClass;                                // 9        SizeClass (int) - server-unused
    // uint32      BloodID;                                  // 10       BloodID (uint) - server-unused
    // uint32      NPCSoundID;                                // 11       NPCSoundID (uint) - server-unused
};

/**
 * \struct CreatureDisplayInfoExtraEntry
 * \brief Entry extending the CreatureDisplayInfoEntry.
 */
struct CreatureDisplayInfoExtraEntry
{
    uint32      DisplayExtraId;                             // 0        m_ID - ID in DBC.
    uint32      Race;                                       // 1        m_DisplayRaceID - Race to which it's applicable.
    // uint32    Gender;                                    // 2        Gender (uint) - server-unused
    // uint32    SkinColor;                                 // 3        SkinColor (uint) - server-unused
    // uint32    FaceType;                                  // 4        FaceType (uint) - server-unused
    // uint32    HairType;                                  // 5        HairType (uint) - server-unused
    // uint32    HairStyle;                                 // 6        HairStyle (uint) - server-unused
    // uint32    BeardStyle;                                // 7        BeardStyle (uint) - server-unused
    // uint32    EquipmentHead;                              // 8        EquipmentHead (uint) - server-unused
    // uint32    EquipmentNeck;                              // 9        EquipmentNeck (uint) - server-unused
    // uint32    EquipmentShoulders;                         // 10       EquipmentShoulders (uint) - server-unused
    // uint32    EquipmentBody;                              // 11       EquipmentBody (uint) - server-unused
    // uint32    EquipmentChest;                             // 12       EquipmentChest (uint) - server-unused
    // uint32    EquipmentWaist;                             // 13       EquipmentWaist (uint) - server-unused
    // uint32    EquipmentLegs;                              // 14       EquipmentLegs (uint) - server-unused
    // uint32    EquipmentFeet;                               // 15       EquipmentFeet (uint) - server-unused
    // uint32    EquipmentWrist;                              // 16       EquipmentWrist (uint) - server-unused
    // uint32    EquipmentHands;                              // 17       EquipmentHands (uint) - server-unused
    // char*     BakeName;                                    // 18       BakeName (string) - server-unused (CreatureDisplayExtra-*.blp)
};

/**
 * \struct CreatureFamilyEntry
 * \brief Entry representing the different pet available for players.
 */
struct CreatureFamilyEntry
{
    uint32    ID;                                           // 0 - ID in DBC.
    float     MinScale;                                     // 1 - Min Scale of creature within the game.
    uint32    MinScaleLevel;                                // 2 0/1 - Minimum level for which the MinScale is applicable.
    float     MaxScale;                                     // 3 - Max Scale of creature within the game.
    uint32    MaxScaleLevel;                                // 4 0/60 - Maximum level for which the MaxScale is applicable.
    uint32    SkillLine[2];                                 // 5-6 - Skill Lines (See SkillLine.dbc).
    uint32    PetFoodMask;                                  // 7 - Food Mask for the given pet.
    char*     Name_lang[8];                                 // 8-15 - m_name_lang.
    // uint32   Name_lang_flags;                            // 16       Name_lang_flags (uint) - server-unused
    // char*    IconFile;                                   // 17       IconFile (string) - server-unused
};

#define MAX_CREATURE_SPELL_DATA_SLOT 4

/**
 * \struct CreatureSpellDataEntry
 * \brief Entry representing the different spell available for player's pet.
 */
struct CreatureSpellDataEntry
{
    uint32    ID;                                           // 0        m_ID - ID in DBC.
    uint32    SpellId[MAX_CREATURE_SPELL_DATA_SLOT];        // 1-4      SpellId[4] - Spell ID's (see Spell.dbc).
    // uint32    Availability_1;                            // 5        Availability_1 (uint) - server-unused
    // uint32    Availability_2;                            // 6        Availability_2 (uint) - server-unused
    // uint32    Availability_3;                             // 7        Availability_3 (uint) - server-unused
    // uint32    Availability_4;                             // 8        Availability_4 (uint) - server-unused
};

/**
 * \struct CreatureTypeEntry
 * \brief Entry representing the different creature type available for player's pet.
 */
struct CreatureTypeEntry
{
    uint32    ID;                                           // 0        m_ID
    // char*   Name_lang;                                   // 1        Name_lang (string) - server-unused
    // char*   Name_lang_loc2;                               // 2        Name_lang_loc2 (string) - server-unused
    // char*   Name_lang_loc3;                               // 3        Name_lang_loc3 (string) - server-unused
    // char*   Name_lang_loc4;                               // 4        Name_lang_loc4 (string) - server-unused
    // char*   Name_lang_loc5;                               // 5        Name_lang_loc5 (string) - server-unused
    // char*   Name_lang_loc6;                               // 6        Name_lang_loc6 (string) - server-unused
    // char*   Name_lang_loc7;                               // 7        Name_lang_loc7 (string) - server-unused
    // char*   Name_lang_loc8;                               // 8        Name_lang_loc8 (string) - server-unused
    // uint32  Name_lang_flags;                              // 9        Name_lang_flags (uint) - server-unused
    // uint32  Flags;                                        // 10       Flags (uint) - server-unused
};

/**
 * \struct DurabilityCostsEntry
 * \brief Entry representing the multipliers for item reparation cost.
 */
struct DurabilityCostsEntry
{
    uint32    ID;                                           // 0        ID - ID in DBC.
    uint32    WeaponSubClassCost[29];                       // 1-29     WeaponSubClassCost - WeaponSubClassCost_0-20 (idx 1-21), ArmorSubClassCost_0-7 (idx 22-29).
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
    // char*   EmoteSlashCommand;                           // 1        EmoteSlashCommand (string) - server-unused
    // uint32  AnimID;                                      // 2        AnimID (uint) - server-unused
    // uint32  EmoteFlags;                                  // 3        EmoteFlags (uint) - server-unused
    uint32  EmoteType;                                      // 4        m_EmoteSpecProc (determine how emote are shown)
    // uint32  EmoteSpecProcParam;                          // 5        EmoteSpecProcParam (uint) - server-unused
    // uint32  EventSoundID;                                // 6        EventSoundID (uint) - server-unused
};

/**
 * \struct EmotesTextEntry
 * \brief Entry repsenting the text for given emote.
 */
struct EmotesTextEntry
{
    uint32  ID;                                             // 0        ID - ID in DBC.
    // char*  Name;                                         // 1        Name (string) - server-unused
    uint32  EmoteID;                                        // 2        EmoteID - ID of the text.
    // uint32  EmoteText1;                                  // 3        EmoteText1 (uint) - server-unused
    // uint32  EmoteText2;                                  // 4        EmoteText2 (uint) - server-unused
    // uint32  EmoteText3;                                  // 5        EmoteText3 (uint) - server-unused
    // uint32  EmoteText4;                                  // 6        EmoteText4 (uint) - server-unused
    // uint32  EmoteText5;                                  // 7        EmoteText5 (uint) - server-unused
    // uint32  EmoteText6;                                  // 8        EmoteText6 (uint) - server-unused
    // uint32  EmoteText7;                                  // 9        EmoteText7 (uint) - server-unused
    // uint32  EmoteText8;                                  // 10       EmoteText8 (uint) - server-unused
    // uint32  EmoteText9;                                  // 11       EmoteText9 (uint) - server-unused
    // uint32  EmoteText10;                                 // 12       EmoteText10 (uint) - server-unused
    // uint32  EmoteText11;                                 // 13       EmoteText11 (uint) - server-unused
    // uint32  EmoteText12;                                 // 14       EmoteText12 (uint) - server-unused
    // uint32  EmoteText13;                                 // 15       EmoteText13 (uint) - server-unused
    // uint32  EmoteText14;                                 // 16       EmoteText14 (uint) - server-unused
    // uint32  EmoteText15;                                 // 17       EmoteText15 (uint) - server-unused
    // uint32  EmoteText16;                                 // 18       EmoteText16 (uint) - server-unused
};

/**
 * \struct FactionEntry
 * \brief Entry representing all the factions available.
 */
struct FactionEntry
{
    uint32      ID;                                         // 0        m_ID - ID in DBC.
    int32       ReputationIndex;                            // 1        m_reputationIndex - ID of the Reputation List.
    uint32      ReputationRaceMask[4];                      // 2-5      m_reputationRaceMask -
    uint32      ReputationClassMask[4];                     // 6-9      m_reputationClassMask
    int32       ReputationBase[4];                          // 10-13    m_reputationBase
    uint32      ReputationFlags[4];                         // 14-17    m_reputationFlags
    uint32      ParentFactionID;                            // 18       m_parentFactionID
    char*       Name_lang[8];                               // 19-26    m_name_lang
    // uint32   Name_lang_flags;                            // 27       Name_lang_flags (uint) - server-unused
    // char*    Description_lang[8];                        // 28-35    Description_lang (string) - server-unused
    // uint32   Description_lang_flags;                     // 36       Description_lang_flags (uint) - server-unused

    // helpers

    int GetIndexFitTo(uint32 raceMask, uint32 classMask) const
    {
        for (int i = 0; i < 4; ++i)
        {
            if ((ReputationRaceMask[i] == 0 || (ReputationRaceMask[i] & raceMask)) &&
                (ReputationClassMask[i] == 0 || (ReputationClassMask[i] & classMask)))
            {
                return i;
            }
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
    uint32      Faction;
    /// 2 specific flags for that faction
    uint32      Flags;
    /// 3 if mask set (see FactionMasks) then faction included in masked team
    uint32      FactionGroup;
    /// 4 if mask set (see FactionMasks) then faction friendly to masked team
    uint32      FriendGroup;
    /// 5 if mask set (see FactionMasks) then faction hostile to masked team
    uint32      EnemyGroup;
    /// 6-9
    uint32      Enemies[4];
    /// 10-13
    uint32      Friend[4];
    //-------------------------------------------------------  end structure

    // helpers
    bool IsFriendlyTo(FactionTemplateEntry const& entry) const
    {
        if (entry.Faction)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (Enemies[i]  == entry.Faction)
                {
                    return false;
                }
            }

            for (int i = 0; i < 4; ++i)
            {
                if (Friend[i] == entry.Faction)
                {
                    return true;
                }
            }
        }
        return (FriendGroup & entry.FactionGroup) || (FactionGroup & entry.FriendGroup);
    }

    bool IsHostileTo(FactionTemplateEntry const& entry) const
    {
        if (entry.Faction)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (Enemies[i]  == entry.Faction)
                {
                    return true;
                }
            }

            for (int i = 0; i < 4; ++i)
            {
                if (Friend[i] == entry.Faction)
                {
                    return false;
                }
            }
        }
        return (EnemyGroup & entry.FactionGroup) != 0;
    }

    bool IsHostileToPlayers() const { return (EnemyGroup & FACTION_MASK_PLAYER) != 0; }
    bool IsNeutralToAll() const
    {
        for (int i = 0; i < 4; ++i)
        {
            if (Enemies[i] != 0)
            {
                return false;
            }
        }
        return EnemyGroup == 0 && FriendGroup == 0;
    }

    bool IsContestedGuardFaction() const { return (Flags & FACTION_TEMPLATE_FLAG_CONTESTED_GUARD) != 0; }
};

/**
 * \struct GameObjectDisplayInfoEntry
 * \brief Entry representing the info for the game object to be displayed on the client.
 */
struct GameObjectDisplayInfoEntry
{
    uint32      ID;                                         // 0        ID - ID in DBC.
    char*       ModelName;                                  // 1        ModelName - File name for  the object.
    // uint32      Sound_0;                                  // 2        Sound_0 (uint) - server-unused
    // uint32      Sound_1;                                  // 3        Sound_1 (uint) - server-unused
    // uint32      Sound_2;                                  // 4        Sound_2 (uint) - server-unused
    // uint32      Sound_3;                                  // 5        Sound_3 (uint) - server-unused
    // uint32      Sound_4;                                  // 6        Sound_4 (uint) - server-unused
    // uint32      Sound_5;                                  // 7        Sound_5 (uint) - server-unused
    // uint32      Sound_6;                                  // 8        Sound_6 (uint) - server-unused
    // uint32      Sound_7;                                  // 9        Sound_7 (uint) - server-unused
    // uint32      Sound_8;                                  // 10       Sound_8 (uint) - server-unused
    // uint32      Sound_9;                                  // 11       Sound_9 (uint) - server-unused
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
    // char*     Name_lang;                                 // 1        Name_lang (string) - server-unused
    // char*     Name_lang_loc2;                             // 2        Name_lang_loc2 (string) - server-unused
    // char*     Name_lang_loc3;                             // 3        Name_lang_loc3 (string) - server-unused
    // char*     Name_lang_loc4;                             // 4        Name_lang_loc4 (string) - server-unused
    // char*     Name_lang_loc5;                             // 5        Name_lang_loc5 (string) - server-unused
    // char*     Name_lang_loc6;                             // 6        Name_lang_loc6 (string) - server-unused
    // char*     Name_lang_loc7;                             // 7        Name_lang_loc7 (string) - server-unused
    // char*     Name_lang_loc8;                             // 8        Name_lang_loc8 (string) - server-unused
    // uint32    Name_lang_flags;                            // 9        Name_lang_flags (uint) - server-unused
};

/**
 * \struct ItemClassEntry
 * \brief Entry representing the item class type.
 */
struct ItemClassEntry
{
    uint32   ID;                                            // 0        m_ID
    // uint32   Unk1;                                       // 1        Unk1 (uint) - server-unused
    // uint32   Flags;                                      // 2        Flags (uint) - server-unused (only weapon has 1 in field, others 0)
    char*    ClassName_lang[8];                        // 3-10     ClassName_lang
    // uint32   ClassName_lang_flags;                       // 11       ClassName_lang_flags (uint) - server-unused
};

/**
 * \struct ItemRandomPropertiesEntry
 * \brief Entry representing the random enchant for Items.
 */
struct ItemRandomPropertiesEntry
{
    uint32    ID;                                           // 0        m_ID
    // char*     Name;                                      // 1        Name (string) - server-unused
    uint32    Enchantment[3];                               // 2-4      Enchantment
    // uint32    Enchantment_3;                              // 5        Enchantment_3 (uint) - server-unused
    // uint32    Enchantment_4;                              // 6        Enchantment_4 (uint) - server-unused
    // char*     Name_lang;                                  // 7        Name_lang (string) - server-unused
    // char*     Name_lang_loc2;                              // 8        Name_lang_loc2 (string) - server-unused
    // char*     Name_lang_loc3;                              // 9        Name_lang_loc3 (string) - server-unused
    // char*     Name_lang_loc4;                              // 10       Name_lang_loc4 (string) - server-unused
    // char*     Name_lang_loc5;                              // 11       Name_lang_loc5 (string) - server-unused
    // char*     Name_lang_loc6;                              // 12       Name_lang_loc6 (string) - server-unused
    // char*     Name_lang_loc7;                              // 13       Name_lang_loc7 (string) - server-unused
    // char*     Name_lang_loc8;                              // 14       Name_lang_loc8 (string) - server-unused
    // uint32    Name_lang_flags;                             // 15       Name_lang_flags (uint) - server-unused
};

/**
 * \struct ItemSetEntry
 * \brief Entry representing the Set of items within the game.
 */
struct ItemSetEntry
{
    // uint32    id                                         // 0        m_ID
    char*     Name_lang[8];                                 // 1-8      m_name_lang
    // uint32   Name_lang_flags;                            // 9        Name_lang_flags (uint) - server-unused
    // uint32   ItemID[17];                                 // 10-26    ItemID (uint) - server-unused
    uint32    SetSpellID[8];                                // 27-34    m_setSpellID
    uint32    SetThreshold[8];                              // 35-42    m_setThreshold
    uint32    RequiredSkill;                                // 43       m_requiredSkill
    uint32    RequiredSkillRank;                            // 44       m_requiredSkillRank
};

/**
 * \struct LiquidTypeEntry
 * \brief Entry representing the type of liquid within the game.
 */
struct LiquidTypeEntry
{
    uint32 ID;                                              // 0        ID
    char*  Name;                                            // 1        m_name - liquid name string (Water/Ocean/Magma/Slime/Naxxramas - Slime). Was mis-read as int 'LiquidId'.
    uint32 Type;                                            // 2        0: Magma; 2: Slime; 3: Water. (.dbd name: Flags)
    uint32 SpellID;                                         // 3        SpellID - Reference to Spell.dbc
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
    // uint32      Action1;                                 // 25       Action1 (uint) - server-unused
    // uint32      Action2;                                 // 26       Action2 (uint) - server-unused
    // uint32      Action3;                                 // 27       Action3 (uint) - server-unused
    // uint32      Action4;                                 // 28       Action4 (uint) - server-unused
    // uint32      Action5;                                 // 29       Action5 (uint) - server-unused
    // uint32      Action6;                                 // 30       Action6 (uint) - server-unused
    // uint32      Action7;                                 // 31       Action7 (uint) - server-unused
    // uint32      Action8;                                 // 32       Action8 (uint) - server-unused
};

/**
 * \struct MailTemplateEntry
 * \brief Entry representing a mail template for quest result.
 */
struct MailTemplateEntry
{
    uint32      ID;                                         // 0        m_ID
    // char*       Body_lang;                                // 1        Body_lang (string) - server-unused
    // char*       Body_lang_loc2;                            // 2        Body_lang_loc2 (string) - server-unused
    // char*       Body_lang_loc3;                            // 3        Body_lang_loc3 (string) - server-unused
    // char*       Body_lang_loc4;                            // 4        Body_lang_loc4 (string) - server-unused
    // char*       Body_lang_loc5;                            // 5        Body_lang_loc5 (string) - server-unused
    // char*       Body_lang_loc6;                            // 6        Body_lang_loc6 (string) - server-unused
    // char*       Body_lang_loc7;                            // 7        Body_lang_loc7 (string) - server-unused
    // char*       Body_lang_loc8;                            // 8        Body_lang_loc8 (string) - server-unused
    // uint32      Body_lang_flags;                           // 9        Body_lang_flags (uint) - server-unused
};

/**
 * \struct MapEntry
 * \brief Entry representing maps existing within the game.
 */
struct MapEntry
{
    uint32  MapID;                                          // 0        m_ID
    // char*   Directory;                                   // 1        Directory (string) - server-unused
    uint32  InstanceType;                                   // 2        InstanceType
    // uint32  PVP;                                         // 3        PVP (uint) - server-unused
    char*   MapName_lang[8];                                // 4-11     MapName_lang
    // uint32  MapName_lang_flags;                          // 12       MapName_lang_flags (uint) - server-unused
    // int32   MinLevel;                                    // 13       MinLevel (int) - server-unused
    // int32   MaxLevel;                                    // 14       MaxLevel (int) - server-unused
    // int32   MaxPlayers;                                  // 15       MaxPlayers (int) - server-unused
    // int32   Unk0;                                        // 16       Unk0 (int) - server-unused
    // float   Unk1;                                        // 17       Unk1 (float) - server-unused
    // float   Unk2;                                        // 18       Unk2 (float) - server-unused
    uint32  AreaTableID;                                    // 19       AreaTableID
    // char*   MapDescription0_lang;                        // 20       MapDescription0_lang (string) - server-unused
    // char*   MapDescription0_lang_loc2;                   // 21       MapDescription0_lang_loc2 (string) - server-unused
    // char*   MapDescription0_lang_loc3;                   // 22       MapDescription0_lang_loc3 (string) - server-unused
    // char*   MapDescription0_lang_loc4;                   // 23       MapDescription0_lang_loc4 (string) - server-unused
    // char*   MapDescription0_lang_loc5;                   // 24       MapDescription0_lang_loc5 (string) - server-unused
    // char*   MapDescription0_lang_loc6;                   // 25       MapDescription0_lang_loc6 (string) - server-unused
    // char*   MapDescription0_lang_loc7;                   // 26       MapDescription0_lang_loc7 (string) - server-unused
    // char*   MapDescription0_lang_loc8;                   // 27       MapDescription0_lang_loc8 (string) - server-unused
    // uint32  MapDescription0_lang_flags;                  // 28       MapDescription0_lang_flags (uint) - server-unused
    // char*   MapDescription1_lang;                        // 29       MapDescription1_lang (string) - server-unused
    // char*   MapDescription1_lang_loc2;                   // 30       MapDescription1_lang_loc2 (string) - server-unused
    // char*   MapDescription1_lang_loc3;                   // 31       MapDescription1_lang_loc3 (string) - server-unused
    // char*   MapDescription1_lang_loc4;                   // 32       MapDescription1_lang_loc4 (string) - server-unused
    // char*   MapDescription1_lang_loc5;                   // 33       MapDescription1_lang_loc5 (string) - server-unused
    // char*   MapDescription1_lang_loc6;                   // 34       MapDescription1_lang_loc6 (string) - server-unused
    // char*   MapDescription1_lang_loc7;                   // 35       MapDescription1_lang_loc7 (string) - server-unused
    // char*   MapDescription1_lang_loc8;                   // 36       MapDescription1_lang_loc8 (string) - server-unused
    // uint32  MapDescription1_lang_flags;                  // 37       MapDescription1_lang_flags (uint) - server-unused
    uint32  LoadingScreenID;                                // 38       LoadingScreenID (LoadingScreens.dbc)
    // int32   RaidOffset;                                  // 39       RaidOffset (int) - server-unused
    // int32   Continentname;                                // 40       Continentname (int) - server-unused
    // float   BattlefieldMapIconScale;                     // 41       BattlefieldMapIconScale (float) - server-unused

    // Helpers

    bool IsDungeon() const { return InstanceType == MAP_INSTANCE || InstanceType == MAP_RAID; }
    bool IsNonRaidDungeon() const { return InstanceType == MAP_INSTANCE; }
    bool Instanceable() const { return InstanceType == MAP_INSTANCE || InstanceType == MAP_RAID || InstanceType == MAP_BATTLEGROUND; }
    bool IsRaid() const { return InstanceType == MAP_RAID; }
    bool IsBattleGround() const { return InstanceType == MAP_BATTLEGROUND; }

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

struct MovieEntry
{
    uint32      Id;                                         // 0        m_ID
    // char*       filename;                                // 1        m_filename
    // uint32      unk2;                                    // 2        m_volume
};

/**
 * \struct QuestSortEntry
 * \brief Entry representing the type of quest within the game.
 */
struct QuestSortEntry
{
    uint32      ID;                                         // 0        ID
    // char*       SortName_lang;                           // 1        SortName_lang (string) - server-unused
    // char*       SortName_lang_loc2;                      // 2        SortName_lang_loc2 (string) - server-unused
    // char*       SortName_lang_loc3;                      // 3        SortName_lang_loc3 (string) - server-unused
    // char*       SortName_lang_loc4;                      // 4        SortName_lang_loc4 (string) - server-unused
    // char*       SortName_lang_loc5;                      // 5        SortName_lang_loc5 (string) - server-unused
    // char*       SortName_lang_loc6;                      // 6        SortName_lang_loc6 (string) - server-unused
    // char*       SortName_lang_loc7;                      // 7        SortName_lang_loc7 (string) - server-unused
    // char*       SortName_lang_loc8;                      // 8        SortName_lang_loc8 (string) - server-unused
    // uint32      SortName_lang_flags;                     // 9        SortName_lang_flags (uint) - server-unused
};

/**
 * \struct SkillRaceClassInfoEntry
 * \brief Entry representing the available skills for classes (weapons, gear, ..)
 */
struct SkillRaceClassInfoEntry
{
    // uint32    id;                                        // 0        m_ID
    uint32    SkillID;                                      // 1        m_skillID
    uint32    RaceMask;                                     // 2        m_raceMask
    uint32    ClassMask;                                    // 3        m_classMask
    uint32    Flags;                                        // 4        m_flags
    uint32    MinLevel;                                     // 5        m_minLevel
    // uint32   SkillTierID;                                // 6        SkillTierID (uint) - server-unused
    // uint32   SkillCostIndex;                             // 7        SkillCostIndex (uint) - server-unused
};

/**
 * \struct SkillLineEntry
 * \brief Entry representing the type of skill line (fire, frost, racial, ...).
 */
struct SkillLineEntry
{
    uint32    ID;                                           // 0        ID
    int32     CategoryID;                                   // 1        CategoryID
    // uint32    SkillCostsID;                               // 2        SkillCostsID (uint) - server-unused
    char*     DisplayName_lang[8];                          // 3-10     DisplayName_lang
    // uint32    DisplayName_lang_flags;                     // 11       DisplayName_lang_flags (uint) - server-unused
    // char*     Description_lang;                           // 12       Description_lang (string) - server-unused
    // char*     Description_lang_loc2;                      // 13       Description_lang_loc2 (string) - server-unused
    // char*     Description_lang_loc3;                      // 14       Description_lang_loc3 (string) - server-unused
    // char*     Description_lang_loc4;                      // 15       Description_lang_loc4 (string) - server-unused
    // char*     Description_lang_loc5;                      // 16       Description_lang_loc5 (string) - server-unused
    // char*     Description_lang_loc6;                      // 17       Description_lang_loc6 (string) - server-unused
    // char*     Description_lang_loc7;                      // 18       Description_lang_loc7 (string) - server-unused
    // char*     Description_lang_loc8;                      // 19       Description_lang_loc8 (string) - server-unused
    // uint32    Description_lang_flags;                     // 20       Description_lang_flags (uint) - server-unused
    // uint32    SpellIconID;                                // 21       SpellIconID (uint) - server-unused
};

/**
 * \struct SkillLineAbilityEntry
 * \brief Entry representing the skill line abilities, also contains information about learning conditions.
 */
struct SkillLineAbilityEntry
{
    uint32    ID;                                           // 0, INDEX
    uint32    SkillLine;                                    // 1
    uint32    Spell;                                        // 2
    uint32    RaceMask;                                     // 3
    uint32    ClassMask;                                    // 4
    // uint32    ExcludeRace;                               // 5        ExcludeRace (uint) - always 0 in 2.4.2
    // uint32    ExcludeClass;                              // 6        ExcludeClass (uint) - always 0 in 2.4.2
    uint32    MinSkillLineRank;                             // 7 for trade skill.not for training.
    uint32    SupercededBySpell;                            // 8
    uint32    AcquireMethod;                                // 9 can be 1 or 2 for spells learned on get skill
    uint32    TrivialSkillLineRankHigh;                     // 10
    uint32    TrivialSkillLineRankLow;                      // 11
    // uint32    Abandonable;                               // 12       Abandonable (uint) - always 0
    // uint32    CharacterPoints_0;                         // 13       CharacterPoints_0 (uint) - always 0
    uint32    ReqTrainPoints;                               // 14
};

/**
 * \struct SoundEntriesEntry
 * \brief Entry representing sound for client, used for validation.
 */
struct SoundEntriesEntry
{
    uint32    Id;                                           // 0        m_ID
    // int32     SoundType;                                 // 1        SoundType (int) - server-unused
    // char*     Name;                                      // 2        Name (string) - server-unused
    // char*     File_0;                                    // 3        File_0 (string) - server-unused
    // char*     File_1;                                    // 4        File_1 (string) - server-unused
    // char*     File_2;                                    // 5        File_2 (string) - server-unused
    // char*     File_3;                                    // 6        File_3 (string) - server-unused
    // char*     File_4;                                    // 7        File_4 (string) - server-unused
    // char*     File_5;                                    // 8        File_5 (string) - server-unused
    // char*     File_6;                                    // 9        File_6 (string) - server-unused
    // char*     File_7;                                    // 10       File_7 (string) - server-unused
    // char*     File_8;                                    // 11       File_8 (string) - server-unused
    // char*     File_9;                                    // 12       File_9 (string) - server-unused
    // int32     Freq_0;                                    // 13       Freq_0 (int) - server-unused
    // int32     Freq_1;                                    // 14       Freq_1 (int) - server-unused
    // int32     Freq_2;                                    // 15       Freq_2 (int) - server-unused
    // int32     Freq_3;                                    // 16       Freq_3 (int) - server-unused
    // int32     Freq_4;                                    // 17       Freq_4 (int) - server-unused
    // int32     Freq_5;                                    // 18       Freq_5 (int) - server-unused
    // int32     Freq_6;                                    // 19       Freq_6 (int) - server-unused
    // int32     Freq_7;                                    // 20       Freq_7 (int) - server-unused
    // int32     Freq_8;                                    // 21       Freq_8 (int) - server-unused
    // int32     Freq_9;                                    // 22       Freq_9 (int) - server-unused
    // char*     DirectoryBase;                              // 23       DirectoryBase (string) - server-unused
    // float     VolumeFloat;                                // 24       VolumeFloat (float) - server-unused
    // uint32    Flags;                                      // 25       Flags (uint) - server-unused
    // float     MinDistance;                                // 26       MinDistance (float) - server-unused
    // float     DistanceCutoff;                              // 27       DistanceCutoff (float) - server-unused
    // int32     EAXDef;                                     // 28       EAXDef (int) - server-unused
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

    operator void const* () const { return Empty() ? NULL : this; } // for allow normal use in if (mask)

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
    uint32    ID;                                       // 0 normally counted from 0 field (but some tools start counting from 1, check this before tool use for data view!)
    uint32    School;                                   // 1 not schoolMask from 2.x - just school type so everything linked with SpellEntry::SchoolMask must be rewrited
    uint32    Category;                                 // 2
    // uint32 CastUI;                                   // 3 not used
    uint32    DispelType;                               // 4
    uint32    Mechanic;                                 // 5
    uint32    Attributes;                               // 6
    uint32    AttributesEx;                             // 7
    uint32    AttributesExB;                            // 8
    uint32    AttributesExC;                            // 9
    uint32    AttributesExD;                            // 10
    uint32    ShapeshiftMask;                           // 11
    uint32    ShapeshiftExclude;                        // 12
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
    uint32    ProcFlags;                                // 24
    uint32    ProcChance;                               // 25
    uint32    ProcCharges;                              // 26
    uint32    MaxLevel;                                 // 27
    uint32    BaseLevel;                                // 28
    uint32    SpellLevel;                               // 29
    uint32    DurationIndex;                            // 30
    uint32    PowerType;                                // 31
    uint32    ManaCost;                                 // 32
    uint32    ManaCostPerLevel;                         // 33
    uint32    ManaPerSecond;                            // 34
    uint32    ManaPerSecondPerLevel;                    // 35
    uint32    RangeIndex;                               // 36
    float     Speed;                                    // 37
    uint32    ModalNextSpell;                           // 38 not used
    uint32    CumulativeAura;                           // 39
    uint32    Totem[MAX_SPELL_TOTEMS];                  // 40-41
    int32     Reagent[MAX_SPELL_REAGENTS];              // 42-49
    uint32    ReagentCount[MAX_SPELL_REAGENTS];         // 50-57
    int32     EquippedItemClass;                        // 58 (value)
    int32     EquippedItemSubclass;                     // 59 (mask)
    int32     EquippedItemInvTypes;                     // 60 (mask)
    uint32    Effect[MAX_EFFECT_INDEX];                 ///< 61-63 TODO DOCUMENT
    int32     EffectDieSides[MAX_EFFECT_INDEX];         // 64-66
    uint32    EffectBaseDice[MAX_EFFECT_INDEX];         // 67-69
    float     EffectDicePerLevel[MAX_EFFECT_INDEX];     // 70-72
    float     EffectRealPointsPerLevel[MAX_EFFECT_INDEX];   // 73-75
    int32     EffectBasePoints[MAX_EFFECT_INDEX];       // 76-78 (don't must be used in spell/auras explicitly, must be used cached Spell::m_currentBasePoints)
    uint32    EffectMechanic[MAX_EFFECT_INDEX];         // 79-81
    uint32    ImplicitTargetA[MAX_EFFECT_INDEX];        // 82-84
    uint32    ImplicitTargetB[MAX_EFFECT_INDEX];        // 85-87
    uint32    EffectRadiusIndex[MAX_EFFECT_INDEX];      // 88-90 - spellradius.dbc
    uint32    EffectAura[MAX_EFFECT_INDEX];             // 91-93
    uint32    EffectAuraPeriod[MAX_EFFECT_INDEX];       // 94-96
    float     EffectAmplitude[MAX_EFFECT_INDEX];        // 97-99
    uint32    EffectChainTargets[MAX_EFFECT_INDEX];     // 100-102
    uint32    EffectItemType[MAX_EFFECT_INDEX];         // 103-105
    int32     EffectMiscValue[MAX_EFFECT_INDEX];        // 106-108
    uint32    EffectTriggerSpell[MAX_EFFECT_INDEX];     // 109-111
    float     EffectPointsPerCombo[MAX_EFFECT_INDEX];   // 112-114
    uint32    SpellVisualID;                            // 115
    // uint32    SpellVisualID_1                        // 116 not used
    uint32    SpellIconID;                              // 117
    uint32    ActiveIconID;                             // 118
    // uint32    SpellPriority;                         // 119
    char*     Name_lang[8];                             // 120-127
    // uint32    Name_lang_flags;                       // 128
    char*     NameSubtext_lang[8];                      // 129-136
    // uint32    NameSubtext_lang_flags;                // 137
    // char*     Description_lang[8];                   // 138-145 not used
    // uint32    Description_lang_flags;                // 146     not used
    // char*     AuraDescription_lang[8];                // 147-154 not used
    // uint32    AuraDescription_lang_flags;            // 155     not used
    uint32    ManaCostPct;                              // 156
    uint32    StartRecoveryCategory;                    // 157
    uint32    StartRecoveryTime;                        // 158
    uint32    MaxTargetLevel;                           // 159
    uint32    SpellClassSet;                            // 160
    ClassFamilyMask SpellClassMask;                     // 161+162
    uint32    MaxTargets;                               // 163
    uint32    DefenseType;                              // 164 defenseType
    uint32    PreventionType;                           // 165
    // uint32    StanceBarOrder;                        // 166 not used
    float     EffectChainAmplitude[MAX_EFFECT_INDEX];   // 167-169
    // uint32    MinFactionID;                          // 170 not used, and 0 in 2.4.2
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
        return SpellClassMask.IsFitToFamilyMask(familyFlags);
    }

    /**
     * function indicating whether a spell fits to a spell family based on arguments.
     * Returns a bool value.
     * \param family SpellFamily to which the spell should belong to.
     * \param familyFlags The uint64 value of Spell Family Flags.
     */
    bool IsFitToFamily(SpellFamily family, uint64 familyFlags) const
    {
        return SpellFamily(SpellClassSet) == family && IsFitToFamilyMask(familyFlags);
    }

    /**
     * function indicating whether a spell fits to a spell class family based on a ClassFamilyMask.
     * Returns a bool value.
     * \param mask ClassFamilyMask representing the class family.
     */
    bool IsFitToFamilyMask(ClassFamilyMask const& mask) const
    {
        return SpellClassMask.IsFitToFamilyMask(mask);
    }

    /**
     * function indicating whether a spell fits to a spell class family based on arguments.
     * Returns a bool value.
     * \param family SpellFamily to which the spell should belong to.
     * \param masl ClassFamilyMask representing the class family.
     */
    bool IsFitToFamily(SpellFamily family, ClassFamilyMask const& mask) const
    {
        return SpellFamily(SpellClassSet) == family && IsFitToFamilyMask(mask);
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
    inline bool HasAttribute(SpellAttributesEx2 attribute) const { return AttributesExB & attribute; }

    /**
     * function indicating whether a spell has an attribute doing bitwise comparison.
     * Returns a bool value.
     * \param attribute SpellAttributesEx3 to compare to actual attributeEx3.
     */
    inline bool HasAttribute(SpellAttributesEx3 attribute) const { return AttributesExC & attribute; }

    /**
     * function indicating whether a spell has an attribute doing bitwise comparison.
     * Returns a bool value.
     * \param attribute SpellAttributesEx4 to compare to actual attributeEx4.
     */
    inline bool HasAttribute(SpellAttributesEx4 attribute) const { return AttributesExD & attribute; }

    inline bool HasSpellEffect(SpellEffects effect) const
    {
        for (uint8 i = EFFECT_INDEX_0; i <= EFFECT_INDEX_2; ++i)
        {
            if (Effect[i] == effect)
            {
                return true;
            }
        }
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
    int32     Base;                                         // 1        Base
    // int32     PerLevel;                                  // 2        PerLevel (int) - server-unused
    // int32     Minimum;                                   // 3        Minimum (int) - server-unused
};

/**
 * \struct SpellRadiusEntry
 * \brief Entry representing the radius of action of some spells.
 */
struct SpellRadiusEntry
{
    uint32    ID;                                           //          m_ID
    float     Radius;                                       //          m_radius
    // float     RadiusPerLevel;                            // 2        RadiusPerLevel (float) - server-unused
    // float     RadiusMax;                                 // 3        RadiusMax (float) - server-unused
};

/**
 * \struct SpellRangeEntry
 * \brief Entry representing the spell range of spells between which the spellcast is possible.
 */
struct SpellRangeEntry
{
    uint32    ID;                                           // 0        m_ID
    float     RangeMin;                                     // 1        RangeMin
    float     RangeMax;                                     // 2        RangeMax
    // uint32  Flags;                                       // 3        Flags (uint) - server-unused
    // char*  DisplayName_lang;                              // 4        DisplayName_lang (string) - server-unused
    // char*  DisplayName_lang_loc2;                         // 5        DisplayName_lang_loc2 (string) - server-unused
    // char*  DisplayName_lang_loc3;                         // 6        DisplayName_lang_loc3 (string) - server-unused
    // char*  DisplayName_lang_loc4;                         // 7        DisplayName_lang_loc4 (string) - server-unused
    // char*  DisplayName_lang_loc5;                         // 8        DisplayName_lang_loc5 (string) - server-unused
    // char*  DisplayName_lang_loc6;                         // 9        DisplayName_lang_loc6 (string) - server-unused
    // char*  DisplayName_lang_loc7;                         // 10       DisplayName_lang_loc7 (string) - server-unused
    // char*  DisplayName_lang_loc8;                         // 11       DisplayName_lang_loc8 (string) - server-unused
    // uint32 DisplayName_lang_flags;                        // 12       DisplayName_lang_flags (uint) - server-unused
    // char*  DisplayNameShort_lang;                         // 13       DisplayNameShort_lang (string) - server-unused
    // char*  DisplayNameShort_lang_loc2;                    // 14       DisplayNameShort_lang_loc2 (string) - server-unused
    // char*  DisplayNameShort_lang_loc3;                    // 15       DisplayNameShort_lang_loc3 (string) - server-unused
    // char*  DisplayNameShort_lang_loc4;                    // 16       DisplayNameShort_lang_loc4 (string) - server-unused
    // char*  DisplayNameShort_lang_loc5;                    // 17       DisplayNameShort_lang_loc5 (string) - server-unused
    // char*  DisplayNameShort_lang_loc6;                    // 18       DisplayNameShort_lang_loc6 (string) - server-unused
    // char*  DisplayNameShort_lang_loc7;                    // 19       DisplayNameShort_lang_loc7 (string) - server-unused
    // char*  DisplayNameShort_lang_loc8;                    // 20       DisplayNameShort_lang_loc8 (string) - server-unused
    // uint32 DisplayNameShort_lang_flags;                   // 21       DisplayNameShort_lang_flags (uint) - server-unused
};

/**
 * \struct SpellShapeshiftFormEntry
 * \brief Entry representing the valid shape shift within the game (stealth, bear, ...).
 */
struct SpellShapeshiftFormEntry
{
    uint32 ID;                                              // 0        m_ID
    // uint32 BonusActionBar;                               // 1        BonusActionBar (uint) - server-unused
    // char*  Name_lang;                                    // 2        Name_lang (string) - server-unused
    // char*  Name_lang_loc2;                                // 3        Name_lang_loc2 (string) - server-unused
    // char*  Name_lang_loc3;                                // 4        Name_lang_loc3 (string) - server-unused
    // char*  Name_lang_loc4;                                // 5        Name_lang_loc4 (string) - server-unused
    // char*  Name_lang_loc5;                                // 6        Name_lang_loc5 (string) - server-unused
    // char*  Name_lang_loc6;                                // 7        Name_lang_loc6 (string) - server-unused
    // char*  Name_lang_loc7;                                // 8        Name_lang_loc7 (string) - server-unused
    // char*  Name_lang_loc8;                                // 9        Name_lang_loc8 (string) - server-unused
    // uint32 Name_lang_flags;                               // 10       Name_lang_flags (uint) - server-unused
    uint32 Flags;                                           // 11       Flags
    int32  CreatureType;                                    // 12       CreatureType <=0 humanoid, other normal creature types
    // uint32 AttackIconID;                                 // 13       AttackIconID (uint) - server-unused
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
    // char*     Name_lang;                                 // 1        Name_lang (string) - server-unused
    // char*     Name_lang_loc2;                             // 2        Name_lang_loc2 (string) - server-unused
    // char*     Name_lang_loc3;                             // 3        Name_lang_loc3 (string) - server-unused
    // char*     Name_lang_loc4;                             // 4        Name_lang_loc4 (string) - server-unused
    // char*     Name_lang_loc5;                             // 5        Name_lang_loc5 (string) - server-unused
    // char*     Name_lang_loc6;                             // 6        Name_lang_loc6 (string) - server-unused
    // char*     Name_lang_loc7;                             // 7        Name_lang_loc7 (string) - server-unused
    // char*     Name_lang_loc8;                             // 8        Name_lang_loc8 (string) - server-unused
    // uint32    Name_lang_flags;                            // 9        Name_lang_flags (uint) - server-unused
};

/**
 * \struct SpellItemEnchantmentEntry
 * \brief Entry representing the link between a Spell Trigger Enchantement and its enchant.
 */
struct SpellItemEnchantmentEntry
{
    uint32      ID;                                         // 0        m_ID
    uint32      Effect[3];                                  // 1-3      m_effect[3]
    uint32      EffectPointsMin[3];                         // 4-6      m_effectPointsMin[3]
    // uint32   EffectPointsMax[3];                         // 7-9      EffectPointsMax (uint) - server-unused
    uint32      EffectArg[3];                               // 10-12    m_effectArg[3]
    char*       Name_lang[8];                               // 13-20    m_name_lang[8]
    // uint32   Name_lang_flags;                            // 21       Name_lang_flags (uint) - server-unused
    uint32      ItemVisual;                                 // 22       m_itemVisual
    uint32      Flags;                                      // 23       m_flags
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
    // uint32    RankID_5;                                  // 9        RankID_5 (uint) - server-unused
    // uint32    RankID_6;                                  // 10       RankID_6 (uint) - server-unused
    // uint32    RankID_7;                                  // 11       RankID_7 (uint) - server-unused
    // uint32    RankID_8;                                  // 12       RankID_8 (uint) - server-unused
    uint32    DependsOn;                                    // 13       m_prereqTalent (Talent.dbc)
    // uint32    DependsOn_1;                                // 14       DependsOn_1 (uint) - server-unused
    // uint32    DependsOn_2;                                // 15       DependsOn_2 (uint) - server-unused
    uint32    DependsOnRank;                                // 16       m_prereqRank
    // uint32    DependsOnRank_1;                            // 17       DependsOnRank_1 (uint) - server-unused
    // uint32    DependsOnRank_2;                            // 18       DependsOnRank_2 (uint) - server-unused
    // uint32    Flags;                                     // 19       Flags (uint) - server-unused (also needed to disable highest ranks on reset talent tree)
    uint32    RequiredSpellID;                              // 20       RequiredSpellID req.spell
};

/**
 * \struct TalentTabEntry
 * \brief Entry representing the available talents tab for each classes.
 */
struct TalentTabEntry
{
    uint32  ID;                                             // 0        ID
    // char*   Name_lang;                                   // 1        Name_lang (string) - server-unused
    // char*   Name_lang_loc2;                               // 2        Name_lang_loc2 (string) - server-unused
    // char*   Name_lang_loc3;                               // 3        Name_lang_loc3 (string) - server-unused
    // char*   Name_lang_loc4;                               // 4        Name_lang_loc4 (string) - server-unused
    // char*   Name_lang_loc5;                               // 5        Name_lang_loc5 (string) - server-unused
    // char*   Name_lang_loc6;                               // 6        Name_lang_loc6 (string) - server-unused
    // char*   Name_lang_loc7;                               // 7        Name_lang_loc7 (string) - server-unused
    // char*   Name_lang_loc8;                               // 8        Name_lang_loc8 (string) - server-unused
    // uint32  Name_lang_flags;                              // 9        Name_lang_flags (uint) - server-unused
    // uint32  SpellIconID;                                  // 10       SpellIconID (uint) - server-unused
    // uint32  RaceMask;                                     // 11       RaceMask (uint) - server-unused
    uint32  ClassMask;                                      // 12       m_classMask
    uint32  OrderIndex;                                     // 13       OrderIndex
    // char*   BackgroundFile;                               // 14       BackgroundFile (string) - server-unused
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
    char*     Name_lang[8];                                 // 5-12     Name_lang - Name of the Taxi Node (relies on locale).
    // uint32    Name_lang_flags;                           // 13       Name_lang_flags (uint) - server-unused
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
    uint32    PathID;                                       // 1        m_PathID - ID of the path in the DBC.
    uint32    NodeIndex;                                    // 2        m_NodeIndex - Index of the Node in the path.
    uint32    ContinentID;                                  // 3        m_ContinentID - ID of the Continent in DBC (0 = Azeroth, 1 = Kalimdor, 30 = Alterac Valley)
    float     LocX;                                         // 4        m_LocX - X position of the Node.
    float     LocY;                                         // 5        m_LocY - Y position of the Node.
    float     LocZ;                                         // 6        m_LocZ - Z position of the Node.
    uint32    Flags;                                        // 7        m_flags - Unknown usage.
    uint32    Delay;                                        // 8        m_delay - Unknown usage.
};

/**
 * \struct WMOAreaTableEntry
 * \brief Entry representing the links between area, area's name, area's location, ...
 */
struct WMOAreaTableEntry
{
    uint32 ID;                                              // 0        m_ID index
    int32 WMOID;                                            // 1        m_WMOID used in root WMO
    int32 NameSetID;                                        // 2        m_NameSetID used in adt file
    int32 WMOGroupID;                                       // 3        m_WMOGroupID used in group WMO
    // int32 SoundProviderPref;                             // 4        SoundProviderPref (int) - server-unused
    // int32 SoundProviderPrefUnderwater;                   // 5        SoundProviderPrefUnderwater (int) - server-unused
    // int32 AmbienceID;                                    // 6        AmbienceID (int) - server-unused
    // int32 ZoneMusic;                                     // 7        ZoneMusic (int) - server-unused
    // int32 IntroSound;                                    // 8        IntroSound (int) - server-unused
    uint32 Flags;                                           // 9        m_flags (used for indoor/outdoor determination)
    uint32 AreaTableID;                                     // 10       m_AreaTableID (AreaTable.dbc)
    // char* AreaName_lang[8];                              // 11-18    AreaName_lang (string) - server-unused
    // uint32 AreaName_lang_flags;                          // 19       AreaName_lang_flags (uint) - server-unused
};

/**
 * \struct WorldMapAreaEntry
 * \brief Entry representing the location of World Map Area.
 */
struct WorldMapAreaEntry
{
    // uint32  ID;                                          // 0        ID (uint) - server-unused
    uint32  MapID;                                          // 1        m_mapID
    uint32  AreaID;                                         // 2        m_areaID index (continent 0 areas ignored)
    // char*   AreaName;                                    // 3        AreaName (string) - server-unused
    float   LocLeft;                                        // 4        m_locLeft
    float   LocRight;                                       // 5        m_locRight
    float   LocTop;                                         // 6        m_locTop
    float   LocBottom;                                      // 7        m_locBottom
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
    // char*   AreaName_lang;                                // 5        AreaName_lang (string) - server-unused
    // char*   AreaName_lang_loc2;                            // 6        AreaName_lang_loc2 (string) - server-unused
    // char*   AreaName_lang_loc3;                            // 7        AreaName_lang_loc3 (string) - server-unused
    // char*   AreaName_lang_loc4;                            // 8        AreaName_lang_loc4 (string) - server-unused
    // char*   AreaName_lang_loc5;                            // 9        AreaName_lang_loc5 (string) - server-unused
    // char*   AreaName_lang_loc6;                            // 10       AreaName_lang_loc6 (string) - server-unused
    // char*   AreaName_lang_loc7;                            // 11       AreaName_lang_loc7 (string) - server-unused
    // char*   AreaName_lang_loc8;                            // 12       AreaName_lang_loc8 (string) - server-unused
    // uint32  AreaName_lang_flags;                           // 13       AreaName_lang_flags (uint) - server-unused
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
