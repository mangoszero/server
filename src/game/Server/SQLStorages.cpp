/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
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

#include "SQLStorages.h"

const char CreatureInfosrcfmt[] = "issiiiiiifiiiiliiiiiffiiffffffiiiiffffiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiis";
const char CreatureInfodstfmt[] = "issiiiiiifiiiiliiiiiffiiffffffiiiiffffiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiis";
const char CreatureDataAddonInfofmt[] = "iiibbiis";
const char CreatureModelfmt[] = "iffbii";
const char CreatureInfoAddonInfofmt[] = "iiibbiis";
const char EquipmentInfofmt[] = "iiii";
const char EquipmentInfoItemfmt[] = "iiiiiii";
const char EquipmentInfoRawfmt[] = "iiiiiiiiii";
const char GameObjectInfosrcfmt[] = "iiisiifiiiiiiiiiiiiiiiiiiiiiiiiii";
const char GameObjectInfodstfmt[] = "iiisiifiiiiiiiiiiiiiiiiiiiiiiiiii";
const char ItemPrototypesrcfmt[] = "iiisiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffiffiffiffiffiiiiiiiiiifiiifiiiiiifiiiiiifiiiiiifiiiiiifiiiisiiiiiiiiiiiiiiiiiiii";
const char ItemPrototypedstfmt[] = "iiisiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiiffiffiffiffiffiiiiiiiiiifiiifiiiiiifiiiiiifiiiiiifiiiiiifiiiisiiiiiiiiiiiiiiiiiiii";
const char PageTextfmt[] = "isi";
const char InstanceTemplatesrcfmt[] = "iiiiiiiff";
const char InstanceTemplatedstfmt[] = "iiiiiiiff";
const char ConditionsSrcFmt[] = "iiii";
const char ConditionsDstFmt[] = "iiii";
const char CreatureTemplateSpellsFmt[] = "iiiii";
const char SpellScriptTargetFmt[] = "iiii";

SQLStorage sCreatureStorage(CreatureInfosrcfmt, CreatureInfodstfmt, "entry", "creature_template");
SQLStorage sCreatureDataAddonStorage(CreatureDataAddonInfofmt, "guid", "creature_addon");
SQLStorage sCreatureModelStorage(CreatureModelfmt, "modelid", "creature_model_info");
SQLStorage sCreatureInfoAddonStorage(CreatureInfoAddonInfofmt, "entry", "creature_template_addon");
SQLStorage sEquipmentStorage(EquipmentInfofmt, "entry", "creature_equip_template");
SQLStorage sEquipmentStorageItem(EquipmentInfoItemfmt, "entry", "creature_item_template");
SQLStorage sEquipmentStorageRaw(EquipmentInfoRawfmt, "entry", "creature_equip_template_raw");
SQLStorage sItemStorage(ItemPrototypesrcfmt, ItemPrototypedstfmt, "entry", "item_template");
SQLStorage sPageTextStore(PageTextfmt, "entry", "page_text");
SQLStorage sInstanceTemplate(InstanceTemplatesrcfmt, InstanceTemplatedstfmt, "map", "instance_template");
SQLStorage sConditionStorage(ConditionsSrcFmt, ConditionsDstFmt, "condition_entry", "conditions");

SQLHashStorage sGOStorage(GameObjectInfosrcfmt, GameObjectInfodstfmt, "entry", "gameobject_template");
SQLHashStorage sCreatureTemplateSpellsStorage(CreatureTemplateSpellsFmt, "entry", "creature_template_spells");

SQLMultiStorage sSpellScriptTargetStorage(SpellScriptTargetFmt, "entry", "spell_script_target");
