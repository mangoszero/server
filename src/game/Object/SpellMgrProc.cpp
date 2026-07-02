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



#include "SpellMgr.h"
#include "SpellAuraDefines.h"
#include "Unit.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "SQLStorages.h"
#include "Chat.h"
#include "Spell.h"
#include "World.h"

template <typename EntryType, typename WorkerType, typename StorageType>
    struct SpellRankHelper
{
    SpellRankHelper(SpellMgr& _mgr, StorageType& _storage): mgr(_mgr), worker(_storage), customRank(0) {}
    void RecordRank(EntryType& entry, uint32 spell_id)
    {
        const SpellEntry* spell = sSpellStore.LookupEntry(spell_id);
        if (!spell)
        {
            sLog.outErrorDb("Spell %u listed in `%s` does not exist", spell_id, worker.TableName());
            return;
        }

        uint32 first_id = mgr.GetFirstSpellInChain(spell_id);

        // most spell ranks expected same data
        if (first_id)
        {
            firstRankSpells.insert(first_id);

            if (first_id != spell_id)
            {
                if (!worker.IsValidCustomRank(entry, spell_id, first_id))
                {
                    return;
                }
                // for later check that first rank also added
                else
                {
                    firstRankSpellsWithCustomRanks.insert(first_id);
                    ++customRank;
                }
            }
        }

        worker.AddEntry(entry, spell);
    }
    void FillHigherRanks()
    {
        // check that first rank added for custom ranks
        for (std::set<uint32>::const_iterator itr = firstRankSpellsWithCustomRanks.begin(); itr != firstRankSpellsWithCustomRanks.end(); ++itr)
        {
            if (!worker.HasEntry(*itr))
            {
                sLog.outErrorDb("Spell %u must be listed in `%s` as first rank for listed custom ranks of spell but not found!", *itr, worker.TableName());
            }
        }

        // fill absent non first ranks data base at first rank data
        for (std::set<uint32>::const_iterator itr = firstRankSpells.begin(); itr != firstRankSpells.end(); ++itr)
        {
            if (worker.SetStateToEntry(*itr))
            {
                mgr.doForHighRanks(*itr, worker);
            }
        }
    }
    std::set<uint32> firstRankSpells;
    std::set<uint32> firstRankSpellsWithCustomRanks;

    SpellMgr& mgr;
    WorkerType worker;
    uint32 customRank;
};

struct DoSpellProcEvent
{
    DoSpellProcEvent(SpellProcEventMap& _spe_map) : spe_map(_spe_map), customProc(0), count(0) {}
    void operator()(uint32 spell_id)
    {
        SpellProcEventEntry const& spe = state->second;
        // add ranks only for not filled data (some ranks have ppm data different for ranks for example)
        SpellProcEventMap::const_iterator spellItr = spe_map.find(spell_id);
        if (spellItr == spe_map.end())
        {
            spe_map[spell_id] = spe;
        }
        // if custom rank data added then it must be same except ppm
        else
        {
            SpellProcEventEntry const& r_spe = spellItr->second;
            if (spe.schoolMask != r_spe.schoolMask)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different schoolMask from first rank in chain", spell_id);
            }

            if (spe.spellFamilyName != r_spe.spellFamilyName)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different spellFamilyName from first rank in chain", spell_id);
            }

            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spe.spellFamilyMask[i] != r_spe.spellFamilyMask[i])
                {
                    sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different spellFamilyMask from first rank in chain", spell_id);
                    break;
                }
            }

            if (spe.procFlags != r_spe.procFlags)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different procFlags from first rank in chain", spell_id);
            }

            if (spe.procEx != r_spe.procEx)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different procEx from first rank in chain", spell_id);
            }

            // only ppm allowed has been different from first rank

            if (spe.customChance != r_spe.customChance)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different customChance from first rank in chain", spell_id);
            }

            if (spe.cooldown != r_spe.cooldown)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` as custom rank have different cooldown from first rank in chain", spell_id);
            }
        }
    }

    const char* TableName()
    {
        return "spell_proc_event";
    }

    bool IsValidCustomRank(SpellProcEventEntry const& spe, uint32 entry, uint32 first_id)
    {
        // let have independent data in table for spells with ppm rates (exist rank dependent ppm rate spells)
        if (!spe.ppmRate)
        {
            sLog.outErrorDb("Spell %u listed in `spell_proc_event` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            return false;
        }
        return true;
    }

    void AddEntry(SpellProcEventEntry const& spe, SpellEntry const* spell)
    {
        spe_map[spell->Id] = spe;

        bool isCustom = false;

        if (spe.procFlags == 0)
        {
            if (spell->procFlags == 0)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` probally not triggered spell (no proc flags)", spell->Id);
            }
        }
        else
        {
            if (spell->procFlags == spe.procFlags)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` has exactly same proc flags as in spell.dbc, field value redundant", spell->Id);
            }
            else
            {
                isCustom = true;
            }
        }

        if (spe.customChance == 0)
        {
            /** enable for re-check cases, 0 chance ok for some cases because in some cases it set by another spell/talent spellmod)
             *  if (spell->procChance==0 && !spe.ppmRate)
             *  {
             *      sLog.outErrorDb("Spell %u listed in `spell_proc_event` probally not triggered spell (no chance or ppm)", spell->Id);
             *  }
             */
        }
        else
        {
            if (spell->procChance == spe.customChance)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` has exactly same custom chance as in spell.dbc, field value redundant", spell->Id);
            }
            else
            {
                isCustom = true;
            }
        }

        // totally redundant record
        if (!spe.schoolMask && !spe.procFlags &&
            !spe.procEx && !spe.ppmRate && !spe.customChance && !spe.cooldown)
        {
            bool empty = !spe.spellFamilyName ? true : false;
            for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                if (spe.spellFamilyMask[i])
                {
                    empty = false;
                    break;
                }
            }
            if (empty)
            {
                sLog.outErrorDb("Spell %u listed in `spell_proc_event` doesn't have any useful data", spell->Id);
            }
        }

        if (isCustom)
        {
            ++customProc;
        }
        else
        {
            ++count;
        }
    }

    bool HasEntry(uint32 spellId) { return spe_map.find(spellId) != spe_map.end(); }
    bool SetStateToEntry(uint32 spellId) { return (state = spe_map.find(spellId)) != spe_map.end(); }
    SpellProcEventMap& spe_map;
    SpellProcEventMap::const_iterator state;

    uint32 customProc;
    uint32 count;
};

/**
 * @brief Loads spell proc event condition overrides from the database.
 */
void SpellMgr::LoadSpellProcEvents()
{
    mSpellProcEventMap.clear();                             // need for reload case

    //                                                0      1           2                3                 4                 5                 6          7       8        9             10
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `SchoolMask`, `SpellFamilyName`, `SpellFamilyMask0`, `SpellFamilyMask1`, `SpellFamilyMask2`, `procFlags`, `procEx`, `ppmRate`, `CustomChance`, `Cooldown` FROM `spell_proc_event`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString();
        sLog.outString(">> No spell proc event conditions loaded");
        return;
    }

    SpellRankHelper<SpellProcEventEntry, DoSpellProcEvent, SpellProcEventMap> rankHelper(*this, mSpellProcEventMap);

    BarGoLink bar(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();

        SpellProcEventEntry spe;

        spe.schoolMask      = fields[1].GetUInt32();
        spe.spellFamilyName = fields[2].GetUInt32();

        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            spe.spellFamilyMask[i] = ClassFamilyMask(fields[3 + i].GetUInt64());
        }

        spe.procFlags       = fields[6].GetUInt32();
        spe.procEx          = fields[7].GetUInt32();
        spe.ppmRate         = fields[8].GetFloat();
        spe.customChance    = fields[9].GetFloat();
        spe.cooldown        = fields[10].GetUInt32();

        rankHelper.RecordRank(spe, entry);
    }
    while (result->NextRow());

    rankHelper.FillHigherRanks();

    delete result;

    sLog.outString(">> Loaded %u extra spell proc event conditions +%u custom proc (inc. +%u custom ranks)",  rankHelper.worker.count, rankHelper.worker.customProc, rankHelper.customRank);
    sLog.outString();
}

struct DoSpellProcItemEnchant
{
    DoSpellProcItemEnchant(SpellProcItemEnchantMap& _procMap, float _ppm) : procMap(_procMap), ppm(_ppm) {}
    void operator()(uint32 spell_id) { procMap[spell_id] = ppm; }

    SpellProcItemEnchantMap& procMap;
    float ppm;
};

/**
 * @brief Loads proc-per-minute data for spell item enchant procs.
 */
void SpellMgr::LoadSpellProcItemEnchant()
{
    mSpellProcItemEnchantMap.clear();                       // need for reload case

    uint32 count = 0;

    //                                                0      1
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `ppmRate` FROM `spell_proc_item_enchant`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u proc item enchant definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        float ppmRate = fields[1].GetFloat();

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_proc_item_enchant` does not exist", entry);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            sLog.outErrorDb("Spell %u listed in `spell_proc_item_enchant` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            continue;
        }

        mSpellProcItemEnchantMap[entry] = ppmRate;

        // also add to high ranks
        DoSpellProcItemEnchant worker(mSpellProcItemEnchantMap, ppmRate);
        doForHighRanks(entry, worker);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u proc item enchant definitions", count);
    sLog.outString();
}
