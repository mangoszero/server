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

#ifndef MANGOS_DBCSTORES_H
#define MANGOS_DBCSTORES_H

#include "Common.h"
#include "DataStores/DBCStore.h"
#include "DBCStructure.h"

#include <list>

/**
 * Checks whether the specified client build is supported by the server.
 */
bool IsAcceptableClientBuild(uint32 build);

/**
 * Returns a formatted list of supported client builds.
 */
std::string AcceptableClientBuildsListStr();

typedef std::list<uint32> SimpleFactionsList;

/**
 * Returns the faction team membership list for the specified faction id.
 */
SimpleFactionsList const* GetFactionTeamList(uint32 faction);

/**
 * Returns the localized pet family name for the specified family and locale.
 */
char const* GetPetName(uint32 petfamily, uint32 dbclang);

/**
 * Returns the talent point cost of the specified talent spell.
 */
uint32 GetTalentSpellCost(uint32 spellId);

/**
 * Returns the talent point cost for the specified talent spell position.
 */
uint32 GetTalentSpellCost(TalentSpellPos const* pos);

/**
 * Returns the talent spell position metadata for the specified spell id.
 */
TalentSpellPos const* GetTalentSpellPos(uint32 spellId);

int32 GetAreaFlagByAreaID(uint32 area_id);                  // -1 if not found
/**
 * Returns the default area flag associated with the specified map id.
 */
uint32 GetAreaFlagByMapId(uint32 mapid);

/**
 * Returns the WMO area entry matching the specified root, adt, and group identifiers.
 */
WMOAreaTableEntry const* GetWMOAreaTableEntryByTripple(int32 rootid, int32 adtid, int32 groupid);

/**
 * Returns the area table entry for the specified area id.
 */
AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id);

/**
 * Returns the area table entry for the specified area flag on the given map.
 */
AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(uint32 area_flag, uint32 map_id);

/**
 * Returns the virtual map id that should be used for the specified map and zone.
 */
uint32 GetVirtualMapForMapAndZone(uint32 mapid, uint32 zoneId);

/**
 * Returns the chat channel entry for the specified channel identifier.
 */
ChatChannelsEntry const* GetChannelEntryFor(uint32 channel_id);

/**
 * Returns the chat channel entry matching the specified channel name.
 */
ChatChannelsEntry const* GetChannelEntryFor(const std::string& name);


/**
 * Converts local zone coordinates into map coordinates for the specified zone.
 */
bool Zone2MapCoordinates(float& x, float& y, uint32 zone);

/**
 * Converts map coordinates into local zone coordinates for the specified zone.
 */
bool Map2ZoneCoordinates(float& x, float& y, uint32 zone);

/**
 * Returns the inspection bit position for the specified talent within its tab.
 */
uint32 GetTalentInspectBitPosInTab(uint32 talentId);

/**
 * Returns the inspection bit size for the specified talent tab.
 */
uint32 GetTalentTabInspectBitSize(uint32 talentTabId);
uint32 const* /*[3]*/ GetTalentTabPages(uint32 cls);

/**
 * Checks whether a point lies inside the specified area trigger zone.
 */
bool IsPointInAreaTriggerZone(AreaTriggerEntry const* atEntry, uint32 mapid, float x, float y, float z, float delta = 0.0f);

 uint32 GetCreatureModelRace(uint32 model_id);

extern DBCStorage <AreaTableEntry>               sAreaStore;// recommend access using functions
extern DBCStorage <AreaTriggerEntry>             sAreaTriggerStore;
extern DBCStorage <AuctionHouseEntry>            sAuctionHouseStore;
extern DBCStorage <BankBagSlotPricesEntry>       sBankBagSlotPricesStore;
extern DBCStorage <ChatChannelsEntry>            sChatChannelsStore; //has function for access aswell
extern DBCStorage <CharStartOutfitEntry>         sCharStartOutfitStore;
extern DBCStorage <ChrClassesEntry>              sChrClassesStore;
extern DBCStorage <ChrRacesEntry>                sChrRacesStore;
extern DBCStorage <CinematicSequencesEntry>      sCinematicSequencesStore;
extern DBCStorage <CreatureDisplayInfoEntry>     sCreatureDisplayInfoStore;
extern DBCStorage <CreatureDisplayInfoExtraEntry>sCreatureDisplayInfoExtraStore;
extern DBCStorage <CreatureFamilyEntry>          sCreatureFamilyStore;
extern DBCStorage <CreatureSpellDataEntry>       sCreatureSpellDataStore;
extern DBCStorage <CreatureTypeEntry>            sCreatureTypeStore;
extern DBCStorage <DurabilityCostsEntry>         sDurabilityCostsStore;
extern DBCStorage <DurabilityQualityEntry>       sDurabilityQualityStore;
extern DBCStorage <EmotesEntry>                  sEmotesStore;
extern DBCStorage <EmotesTextEntry>              sEmotesTextStore;
extern DBCStorage <FactionEntry>                 sFactionStore;
extern DBCStorage <FactionTemplateEntry>         sFactionTemplateStore;
extern DBCStorage <GameObjectDisplayInfoEntry>   sGameObjectDisplayInfoStore;

extern DBCStorage <ItemBagFamilyEntry>           sItemBagFamilyStore;
extern DBCStorage <ItemClassEntry>               sItemClassStore;
extern DBCStorage <ItemRandomPropertiesEntry>    sItemRandomPropertiesStore;
extern DBCStorage <ItemSetEntry>                 sItemSetStore;
extern DBCStorage <LiquidTypeEntry>              sLiquidTypeStore;
extern DBCStorage <LockEntry>                    sLockStore;
extern DBCStorage <MailTemplateEntry>            sMailTemplateStore;
extern DBCStorage <MapEntry>                     sMapStore;
#if !defined(CLASSIC)
extern DBCStorage <MovieEntry>                   sMovieStore;
#endif
extern DBCStorage <QuestSortEntry>               sQuestSortStore;
extern DBCStorage <SkillLineEntry>               sSkillLineStore;
extern DBCStorage <SkillLineAbilityEntry>        sSkillLineAbilityStore;
extern DBCStorage <SkillRaceClassInfoEntry>      sSkillRaceClassInfoStore;
extern DBCStorage <SoundEntriesEntry>            sSoundEntriesStore;
extern DBCStorage <SpellCastTimesEntry>          sSpellCastTimesStore;
extern DBCStorage <SpellDurationEntry>           sSpellDurationStore;
extern DBCStorage <SpellItemEnchantmentEntry>    sSpellItemEnchantmentStore;
extern DBCStorage <SpellFocusObjectEntry>        sSpellFocusObjectStore;
extern SpellCategoryStore                        sSpellCategoryStore;
extern PetFamilySpellsStore                      sPetFamilySpellsStore;
extern DBCStorage <SpellRadiusEntry>             sSpellRadiusStore;
extern DBCStorage <SpellRangeEntry>              sSpellRangeStore;
extern DBCStorage <SpellShapeshiftFormEntry>     sSpellShapeshiftFormStore;
extern DBCStorage <SpellEntry>                   sSpellStore;
extern DBCStorage <StableSlotPricesEntry>        sStableSlotPricesStore;
extern DBCStorage <TalentEntry>                  sTalentStore;
extern DBCStorage <TalentTabEntry>               sTalentTabStore;
extern DBCStorage <TaxiNodesEntry>               sTaxiNodesStore;
extern DBCStorage <TaxiPathEntry>                sTaxiPathStore;
extern TaxiMask                                  sTaxiNodesMask;
extern TaxiPathSetBySource                       sTaxiPathSetBySource;
extern TaxiPathNodesByPath                       sTaxiPathNodesByPath;
extern DBCStorage <WMOAreaTableEntry>            sWMOAreaTableStore;
extern DBCStorage <WorldSafeLocsEntry>           sWorldSafeLocsStore;

/**
 * Loads all required DBC stores from the specified data path.
 */
void LoadDBCStores(const std::string& dataPath);

// script support functions
 DBCStorage <SoundEntriesEntry>          const* GetSoundEntriesStore();
 DBCStorage <SpellEntry>                 const* GetSpellStore();
 DBCStorage <SpellRangeEntry>            const* GetSpellRangeStore();
 DBCStorage <FactionEntry>               const* GetFactionStore();
 DBCStorage <CreatureDisplayInfoEntry>   const* GetCreatureDisplayStore();
 DBCStorage <EmotesEntry>                const* GetEmotesStore();
 DBCStorage <EmotesTextEntry>            const* GetEmotesTextStore();
#endif
