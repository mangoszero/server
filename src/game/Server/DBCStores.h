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

#ifndef MANGOS_DBCSTORES_H
#define MANGOS_DBCSTORES_H

#include "Common.h"
#include "DataStores/DBCStore.h"
#include "DBCStructure.h"

#include <list>

bool IsAcceptableClientBuild(uint32 build);
std::string AcceptableClientBuildsListStr();

typedef std::list<uint32> SimpleFactionsList;

SimpleFactionsList const* GetFactionTeamList(uint32 faction);
char const* GetPetName(uint32 petfamily, uint32 dbclang);
uint32 GetTalentSpellCost(uint32 spellId);
uint32 GetTalentSpellCost(TalentSpellPos const* pos);
TalentSpellPos const* GetTalentSpellPos(uint32 spellId);

int32 GetAreaFlagByAreaID(uint32 area_id);                  // -1 if not found
uint32 GetAreaFlagByMapId(uint32 mapid);

WMOAreaTableEntry const* GetWMOAreaTableEntryByTripple(int32 rootid, int32 adtid, int32 groupid);

 AreaTableEntry const* GetAreaEntryByAreaID(uint32 area_id);
 AreaTableEntry const* GetAreaEntryByAreaFlagAndMap(uint32 area_flag, uint32 map_id);

uint32 GetVirtualMapForMapAndZone(uint32 mapid, uint32 zoneId);

ChatChannelsEntry const* GetChannelEntryFor(uint32 channel_id);
ChatChannelsEntry const* GetChannelEntryFor(const std::string& name);


bool Zone2MapCoordinates(float& x, float& y, uint32 zone);
bool Map2ZoneCoordinates(float& x, float& y, uint32 zone);

uint32 GetTalentInspectBitPosInTab(uint32 talentId);
uint32 GetTalentTabInspectBitSize(uint32 talentTabId);
uint32 const* /*[3]*/ GetTalentTabPages(uint32 cls);

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
