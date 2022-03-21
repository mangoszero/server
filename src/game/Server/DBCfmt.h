/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

#ifndef MANGOS_DBCSFRM_H
#define MANGOS_DBCSFRM_H

const char AreaTableEntryfmt[] = "niiiixxxxxissssssssxixxxi";
const char AreaTriggerEntryfmt[] = "niffffffff";
const char AuctionHouseEntryfmt[] = "niiixxxxxxxxx";
const char BankBagSlotPricesEntryfmt[] = "ni";
const char ChrClassesEntryfmt[] = "nxxixssssssssxxix";
const char ChrRacesEntryfmt[] = "nxixiixxixxxxxixissssssssxxxx";
const char CharStartOutfitEntryfmt[] = "diiiiiiiiiiiiixxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char ChatChannelsEntryfmt[] = "iixssssssssxxxxxxxxxx";// ChatChannelsEntryfmt, index not used (more compact store)
const char CinematicSequencesEntryfmt[] = "nxxxxxxxxx";
const char CreatureDisplayInfofmt[] = "nxxifxxxxxxx";
const char CreatureDisplayInfoExtrafmt[] = "nixxxxxxxxxxxxxxxxx";
const char CreatureFamilyfmt[] = "nfifiiiissssssssxx";
const char CreatureSpellDatafmt[] = "niiiixxxx";
const char CreatureTypefmt[] = "nxxxxxxxxxx";
const char DurabilityCostsfmt[] = "niiiiiiiiiiiiiiiiiiiiiiiiiiiii";
const char DurabilityQualityfmt[] = "nf";
const char EmotesEntryfmt[] = "nxxxixx";
const char EmotesTextEntryfmt[] = "nxixxxxxxxxxxxxxxxx";
const char FactionEntryfmt[] = "niiiiiiiiiiiiiiiiiissssssssxxxxxxxxxx";
const char FactionTemplateEntryfmt[] = "niiiiiiiiiiiii";
const char GameObjectDisplayInfofmt[] = "nsxxxxxxxxxx";
const char ItemBagFamilyfmt[] = "nxxxxxxxxx";
const char ItemClassfmt[] = "nxxssssssssx";
const char ItemRandomPropertiesfmt[] = "nxiiixxxxxxxxxxx";
const char ItemSetEntryfmt[] = "dssssssssxxxxxxxxxxxxxxxxxxiiiiiiiiiiiiiiiiii";
const char LiquidTypefmt[] = "niii";
const char LockEntryfmt[] = "niiiiiiiiiiiiiiiiiiiiiiiixxxxxxxx";
const char MailTemplateEntryfmt[] = "nxxxxxxxxx";
const char MapEntryfmt[] = "nxixssssssssxxxxxxxixxxxxxxxxxxxxxxxxxixxx";
const char QuestSortEntryfmt[] = "nxxxxxxxxx";
const char SkillLinefmt[] = "nixssssssssxxxxxxxxxxx";
const char SkillLineAbilityfmt[] = "niiiixxiiiiixxi";
const char SkillRaceClassInfofmt[] = "diiiiixx";
const char SoundEntriesfmt[] = "nxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
const char SpellCastTimefmt[] = "nixx";
const char SpellDurationfmt[] = "niii";
const char SpellEntryfmt[] = "niixiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiifiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffffffiiiiiiiiiiiiiiiiiiiiifffiiiiiiiiiiiifffixiixssssssssxssssssssxxxxxxxxxxxxxxxxxxxiiiiiiiiiixfffxxx";
const char SpellFocusObjectfmt[] = "nxxxxxxxxx";
const char SpellItemEnchantmentfmt[] = "niiiiiixxxiiissssssssxii";
const char SpellRadiusfmt[] = "nfxx";
const char SpellRangefmt[] = "nffxxxxxxxxxxxxxxxxxxx";
const char SpellShapeshiftfmt[] = "nxxxxxxxxxxiix";
const char StableSlotPricesfmt[] = "ni";
const char TalentEntryfmt[] = "niiiiiiiixxxxixxixxxi";
const char TalentTabEntryfmt[] = "nxxxxxxxxxxxiix";
const char TaxiNodesEntryfmt[] = "nifffssssssssxii";
const char TaxiPathEntryfmt[] = "niii";
const char TaxiPathNodeEntryfmt[] = "diiifffii";
const char WMOAreaTableEntryfmt[] = "niiixxxxxiixxxxxxxxx";
const char WorldMapAreaEntryfmt[] = "xinxffff";
const char WorldSafeLocsEntryfmt[] = "nifffxxxxxxxxx";

#endif
