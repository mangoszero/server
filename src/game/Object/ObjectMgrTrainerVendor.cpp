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



#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "Log.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "SpellMgr.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Chat.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "GossipDef.h"
#include "Mail.h"
#include "Formulas.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "ItemEnchantmentMgr.h"

/**
 * @brief Loads trainer spell data from a database table.
 *
 * @param tableName The source table name.
 * @param isTemplates true when loading trainer templates instead of direct trainer entries.
 */
void ObjectMgr::LoadTrainers(char const* tableName, bool isTemplates)
{
    CacheTrainerSpellMap& trainerList = isTemplates ? m_mCacheTrainerTemplateSpellMap : m_mCacheTrainerSpellMap;

    // For reload case
    for (CacheTrainerSpellMap::iterator itr = trainerList.begin(); itr != trainerList.end(); ++itr)
    {
        itr->second.Clear();
    }
    trainerList.clear();

    std::set<uint32> skip_trainers;

    QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `spell`,`spellcost`,`reqskill`,`reqskillvalue`,`reqlevel` FROM `%s`", tableName);

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded `%s`, table is empty!", tableName);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    std::set<uint32> talentIds;

    uint32 count = 0;
    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 entry  = fields[0].GetUInt32();
        uint32 spell  = fields[1].GetUInt32();

        SpellEntry const* spellinfo = sSpellStore.LookupEntry(spell);
        if (!spellinfo)
        {
            sLog.outErrorDb("Table `%s` (Entry: %u ) has non existing spell %u, ignore", tableName, entry, spell);
            continue;
        }

        if (spellinfo->Effect[0] != SPELL_EFFECT_LEARN_SPELL)
        {
            sLog.outErrorDb("Table `%s` for trainer (Entry: %u) has non-learning spell %u, ignore", tableName, entry, spell);
            for (uint32 spell2 = 1; spell2 < sSpellStore.GetNumRows(); ++spell2)
            {
                if (SpellEntry const* spellEntry2 = sSpellStore.LookupEntry(spell2))
                {
                    if (spellEntry2->Effect[0] == SPELL_EFFECT_LEARN_SPELL && spellEntry2->EffectTriggerSpell[0] == spell)
                    {
                        sLog.outErrorDb("  ... possible must be used spell %u instead", spell2);
                        break;
                    }
                }
            }
            continue;
        }

        if (!SpellMgr::IsSpellValid(spellinfo))
        {
            sLog.outErrorDb("Table `%s` (Entry: %u) has broken learning spell %u, ignore", tableName, entry, spell);
            continue;
        }

        if (GetTalentSpellCost(spell))
        {
            if (talentIds.find(spell) == talentIds.end())
            {
                sLog.outErrorDb("Table `%s` has talent as learning spell %u, ignore", tableName, spell);
                talentIds.insert(spell);
            }
            continue;
        }

        if (!isTemplates)
        {
            CreatureInfo const* cInfo = GetCreatureTemplate(entry);

            if (!cInfo)
            {
                sLog.outErrorDb("Table `%s` have entry for nonexistent creature template (Entry: %u), ignore", tableName, entry);
                continue;
            }

            if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_TRAINER))
            {
                if (skip_trainers.find(entry) == skip_trainers.end())
                {
                    sLog.outErrorDb("Table `%s` have data for creature (Entry: %u) without trainer flag, ignore", tableName, entry);
                    skip_trainers.insert(entry);
                }
                continue;
            }

            if (TrainerSpellData const* tSpells = cInfo->TrainerTemplateId ? GetNpcTrainerTemplateSpells(cInfo->TrainerTemplateId) : NULL)
            {
                if (tSpells->spellList.find(spell) != tSpells->spellList.end())
                {
                    sLog.outErrorDb("Table `%s` (Entry: %u) has spell %u listed in trainer template %u, ignore", tableName, entry, spell, cInfo->TrainerTemplateId);
                    continue;
                }
            }
        }

        TrainerSpellData& data = trainerList[entry];

        TrainerSpell& trainerSpell = data.spellList[spell];
        trainerSpell.spell         = spell;
        trainerSpell.spellCost     = fields[2].GetUInt32();
        trainerSpell.reqSkill      = fields[3].GetUInt32();
        trainerSpell.reqSkillValue = fields[4].GetUInt32();
        trainerSpell.reqLevel      = fields[5].GetUInt32();

        trainerSpell.isProvidedReqLevel = trainerSpell.reqLevel > 0;

        if (trainerSpell.reqLevel)
        {
            if (trainerSpell.reqLevel == spellinfo->SpellLevel)
            {
                ERROR_DB_STRICT_LOG("Table `%s` (Entry: %u) has redundant reqlevel %u (=spell level) for spell %u", tableName, entry, trainerSpell.reqLevel, spell);
            }
        }
        else
        {
            trainerSpell.reqLevel = spellinfo->SpellLevel;
        }

        if (SpellMgr::IsProfessionSpell(spellinfo->EffectTriggerSpell[0]))
        {
            data.trainerType = 2;
        }

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %d trainer %sspells", count, isTemplates ? "template " : "");
    sLog.outString();
}

/**
 * @brief Loads trainer templates and validates creature references.
 */
void ObjectMgr::LoadTrainerTemplates()
{
    LoadTrainers("npc_trainer_template", true);

    // post loading check
    std::set<uint32> trainer_ids;
    bool hasErrored = false;

    for (CacheTrainerSpellMap::const_iterator tItr = m_mCacheTrainerTemplateSpellMap.begin(); tItr != m_mCacheTrainerTemplateSpellMap.end(); ++tItr)
    {
        trainer_ids.insert(tItr->first);
    }

    for (uint32 i = 1; i < sCreatureStorage.GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->TrainerTemplateId)
            {
                if (m_mCacheTrainerTemplateSpellMap.find(cInfo->TrainerTemplateId) != m_mCacheTrainerTemplateSpellMap.end())
                {
                    trainer_ids.erase(cInfo->TrainerTemplateId);
                }
                else
                {
                    sLog.outErrorDb("Creature (Entry: %u) has `TrainerTemplateId` = %u for nonexistent trainer template", cInfo->Entry, cInfo->TrainerTemplateId);
                    hasErrored = true;
                }
            }
        }
    }

    for (std::set<uint32>::const_iterator tItr = trainer_ids.begin(); tItr != trainer_ids.end(); ++tItr)
    {
        sLog.outErrorDb("Table `npc_trainer_template` has trainer template %u not used by any trainers ", *tItr);
    }

    if (hasErrored || !trainer_ids.empty())                 // Append extra line in case of reported errors
    {
        sLog.outString();
    }
}

/**
 * @brief Loads vendor item data from a database table.
 *
 * @param tableName The source table name.
 * @param isTemplates true when loading vendor templates instead of direct vendor entries.
 */
void ObjectMgr::LoadVendors(char const* tableName, bool isTemplates)
{
    CacheVendorItemMap& vendorList = isTemplates ? m_mCacheVendorTemplateItemMap : m_mCacheVendorItemMap;

    // For reload case
    for (CacheVendorItemMap::iterator itr = vendorList.begin(); itr != vendorList.end(); ++itr)
    {
        itr->second.Clear();
    }
    vendorList.clear();

    std::set<uint32> skip_vendors;

    QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `item`, `maxcount`, `incrtime`, `condition_id` FROM `%s`", tableName);
    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString(">> Loaded `%s`, table is empty!", tableName);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;
    do
    {
        bar.step();
        Field* fields = result->Fetch();

        uint32 entry        = fields[0].GetUInt32();
        uint32 item_id      = fields[1].GetUInt32();
        uint32 maxcount     = fields[2].GetUInt32();
        uint32 incrtime     = fields[3].GetUInt32();
        uint16 conditionId  = fields[4].GetUInt16();

        if (!IsVendorItemValid(isTemplates, tableName, entry, item_id, maxcount, incrtime, conditionId, NULL, &skip_vendors))
        {
            continue;
        }

        VendorItemData& vList = vendorList[entry];

        vList.AddItem(item_id, maxcount, incrtime, conditionId);
        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u vendor %sitems", count, isTemplates ? "template " : "");
    sLog.outString();
}

/**
 * @brief Loads vendor templates and validates creature references.
 */
void ObjectMgr::LoadVendorTemplates()
{
    LoadVendors("npc_vendor_template", true);

    // post loading check
    std::set<uint32> vendor_ids;

    for (CacheVendorItemMap::const_iterator vItr = m_mCacheVendorTemplateItemMap.begin(); vItr != m_mCacheVendorTemplateItemMap.end(); ++vItr)
    {
        vendor_ids.insert(vItr->first);
    }

    for (uint32 i = 1; i < sCreatureStorage.GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->VendorTemplateId)
            {
                if (m_mCacheVendorTemplateItemMap.find(cInfo->VendorTemplateId) !=  m_mCacheVendorTemplateItemMap.end())
                {
                    vendor_ids.erase(cInfo->VendorTemplateId);
                }
                else
                {
                    sLog.outErrorDb("Creature (Entry: %u) has VendorTemplateId = %u for nonexistent vendor template", cInfo->Entry, cInfo->VendorTemplateId);
                }
            }
        }
    }

    for (std::set<uint32>::const_iterator vItr = vendor_ids.begin(); vItr != vendor_ids.end(); ++vItr)
    {
        sLog.outErrorDb("Table `npc_vendor_template` has vendor template %u not used by any vendors ", *vItr);
    }
}
