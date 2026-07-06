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
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "SQLStorages.h"
#include "Chat.h"
#include "Spell.h"
#include "Unit.h"
#include "World.h"

/**
 * @brief Loads spell target destination coordinates from the database.
 */
void SpellMgr::LoadSpellTargetPositions()
{
    mSpellTargetPositions.clear();                          // need for reload case

    uint32 count = 0;

    //                                                0   1           2                  3                  4                  5
    QueryResult* result = WorldDatabase.Query("SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation` FROM `spell_target_position`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell target destination coordinates", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 Spell_ID = fields[0].GetUInt32();

        SpellTargetPosition st;

        st.target_mapId       = fields[1].GetUInt32();
        st.target_X           = fields[2].GetFloat();
        st.target_Y           = fields[3].GetFloat();
        st.target_Z           = fields[4].GetFloat();
        st.target_Orientation = fields[5].GetFloat();

        MapEntry const* mapEntry = sMapStore.LookupEntry(st.target_mapId);
        if (!mapEntry)
        {
            sLog.outErrorDb("Spell (ID:%u) target map (ID: %u) does not exist in `Map.dbc`.", Spell_ID, st.target_mapId);
            continue;
        }

        if (st.target_X == 0 && st.target_Y == 0 && st.target_Z == 0)
        {
            sLog.outErrorDb("Spell (ID:%u) target coordinates not provided.", Spell_ID);
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(Spell_ID);
        if (!spellInfo)
        {
            sLog.outErrorDb("Spell (ID:%u) listed in `spell_target_position` does not exist.", Spell_ID);
            continue;
        }

        bool found = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellInfo->ImplicitTargetA[i] == TARGET_TABLE_X_Y_Z_COORDINATES || spellInfo->ImplicitTargetB[i] == TARGET_TABLE_X_Y_Z_COORDINATES)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            sLog.outErrorDb("Spell (Id: %u) listed in `spell_target_position` does not have target TARGET_TABLE_X_Y_Z_COORDINATES (17).", Spell_ID);
            continue;
        }

        mSpellTargetPositions[Spell_ID] = st;
        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u spell target destination coordinates", count);
    sLog.outString();
}

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

struct DoSpellBonuses
{
    DoSpellBonuses(SpellBonusMap& _spellBonusMap, SpellBonusEntry const& _spellBonus) : spellBonusMap(_spellBonusMap), spellBonus(_spellBonus) {}
    void operator()(uint32 spell_id) { spellBonusMap[spell_id] = spellBonus; }

    SpellBonusMap& spellBonusMap;
    SpellBonusEntry const& spellBonus;
};

/**
 * @brief Loads spell bonus coefficient overrides from the database.
 */
void SpellMgr::LoadSpellBonuses()
{
    mSpellBonusMap.clear();                             // need for reload case
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `direct_bonus`, `one_hand_direct_bonus`, `two_hand_direct_bonus`, \
        `direct_bonus_done`, `one_hand_direct_bonus_done`, `two_hand_direct_bonus_done`, \
        `direct_bonus_taken`, `one_hand_direct_bonus_taken`, `two_hand_direct_bonus_taken`, \
        `dot_bonus`, `ap_bonus`, `ap_dot_bonus` FROM `spell_bonus_data`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell bonus data", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());
    do
    {
        Field* fields = result->Fetch();
        bar.step();
        uint32 entry = fields[0].GetUInt32();

        SpellEntry const* spell = sSpellStore.LookupEntry(entry);
        if (!spell)
        {
            sLog.outErrorDb("Spell %u listed in `spell_bonus_data` does not exist", entry);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            sLog.outErrorDb("Spell %u listed in `spell_bonus_data` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            continue;
        }

        SpellBonusEntry sbe;

        sbe.direct_damage = fields[1].GetFloat();
        sbe.one_hand_direct_damage = fields[2].GetFloat();
        sbe.two_hand_direct_damage = fields[3].GetFloat();
        sbe.direct_damage_done = fields[4].GetFloat();
        sbe.one_hand_direct_damage_done = fields[5].GetFloat();
        sbe.two_hand_direct_damage_done = fields[6].GetFloat();
        sbe.direct_damage_taken = fields[7].GetFloat();
        sbe.one_hand_direct_damage_taken = fields[8].GetFloat();
        sbe.two_hand_direct_damage_taken = fields[9].GetFloat();
        sbe.dot_damage    = fields[10].GetFloat();
        sbe.ap_bonus      = fields[11].GetFloat();
        sbe.ap_dot_bonus   = fields[12].GetFloat();

        bool need_dot = false;
        bool need_direct = false;
        uint32 x = 0;                                       // count all, including empty, meaning: not all existing effect is DoTs/HoTs
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (!spell->Effect[i])
            {
                ++x;
                continue;
            }

            // DoTs/HoTs
            switch (spell->EffectAura[i])
            {
                case SPELL_AURA_PERIODIC_DAMAGE:
                case SPELL_AURA_PERIODIC_DAMAGE_PERCENT:
                case SPELL_AURA_PERIODIC_LEECH:
                case SPELL_AURA_PERIODIC_HEAL:
                case SPELL_AURA_OBS_MOD_HEALTH:
                case SPELL_AURA_PERIODIC_MANA_LEECH:
                case SPELL_AURA_OBS_MOD_MANA:
                case SPELL_AURA_POWER_BURN_MANA:
                    need_dot = true;
                    ++x;
                    break;
                default:
                    break;
            }
        }

        // TODO: maybe add explicit list possible direct damage spell effects...
        if (x < MAX_EFFECT_INDEX)
        {
            need_direct = true;
        }

        // Check if direct_bonus is needed in `spell_bonus_data`
        float direct_calc = 0.0f;
        float direct_diff = 1000.0f;                        // for have big diff if no DB field value
        if (sbe.direct_damage)
        {
            direct_calc = CalculateDefaultCoefficient(spell, SPELL_DIRECT_DAMAGE);
            direct_diff = std::abs(sbe.direct_damage - direct_calc);
        }

        // Check if direct_bonus_done is needed in `spell_bonus_data`
        float direct_done_calc = 0.0f;
        float direct_done_diff = 1000.0f;
        if (sbe.direct_damage_done)
        {
            direct_done_calc = CalculateDefaultCoefficient(spell, SPELL_DIRECT_DAMAGE);
            direct_done_diff = std::abs(sbe.direct_damage_done - direct_done_calc);
        }

        // Check if direct_bonus_taken is needed in `spell_bonus_data`
        float direct_taken_calc = 0.0f;
        float direct_taken_diff = 1000.0f;
        if (sbe.direct_damage_taken)
        {
            direct_taken_calc = CalculateDefaultCoefficient(spell, SPELL_DIRECT_DAMAGE);
            direct_taken_diff = std::abs(sbe.direct_damage_taken - direct_taken_calc);
        }

        // Check if dot_bonus is needed in `spell_bonus_data`
        float dot_calc = 0.0f;
        float dot_diff = 1000.0f;                           // for have big diff if no DB field value
        if (sbe.dot_damage)
        {
            dot_calc = CalculateDefaultCoefficient(spell, DOT);
            dot_diff = std::abs(sbe.dot_damage - dot_calc);
        }

        // direct bonus
        if (direct_diff < 0.02f && !need_dot && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not needed (data from table: %f, calculated %f, difference of %f) and `dot_bonus` also not used",
                entry, sbe.direct_damage, direct_calc, direct_diff);
        }
        else if (direct_diff < 0.02f && dot_diff < 0.02f && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not needed (data from table: %f, calculated %f, difference of %f) and ",
                entry, sbe.direct_damage, direct_calc, direct_diff);
            sLog.outErrorDb("                                  ... `dot_bonus` not needed (data from table: %f, calculated %f, difference of %f)",
                sbe.dot_damage, dot_calc, dot_diff);
        }
        else if (!need_direct && dot_diff < 0.02f && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `dot_bonus` not needed (data from table: %f, calculated %f, difference of %f) and direct also not used",
                entry, sbe.dot_damage, dot_calc, dot_diff);
        }
        else if (!need_direct && sbe.direct_damage)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not used (spell not have non-periodic affects)", entry);
        }
        else if (!need_dot && sbe.dot_damage)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `dot_bonus` not used (spell not have periodic affects)", entry);
        }

        // direct bonus done
        if (direct_done_diff < 0.02f && !need_dot && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not needed (data from table: %f, calculated %f, difference of %f) and `dot_bonus` also not used",
                entry, sbe.direct_damage_done, direct_done_calc, direct_done_diff);
        }
        else if (direct_done_diff < 0.02f && dot_diff < 0.02f && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not needed (data from table: %f, calculated %f, difference of %f) and ",
                entry, sbe.direct_damage_done, direct_done_calc, direct_done_diff);
            sLog.outErrorDb("                                  ... `dot_bonus` not needed (data from table: %f, calculated %f, difference of %f)",
                sbe.dot_damage, dot_calc, dot_diff);
        }
        else if (!need_direct && sbe.direct_damage_done)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not used (spell not have non-periodic affects)", entry);
        }

        // direct bonus taken
        if (direct_taken_diff < 0.02f && !need_dot && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not needed (data from table: %f, calculated %f, difference of %f) and `dot_bonus` also not used",
                entry, sbe.direct_damage_taken, direct_taken_calc, direct_taken_diff);
        }
        else if (direct_taken_diff < 0.02f && dot_diff < 0.02f && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not needed (data from table: %f, calculated %f, difference of %f) and ",
                entry, sbe.direct_damage_taken, direct_taken_calc, direct_taken_diff);
            sLog.outErrorDb("                                  ... `dot_bonus` not needed (data from table: %f, calculated %f, difference of %f)",
                sbe.dot_damage, dot_calc, dot_diff);
        }
        else if (!need_direct && sbe.direct_damage_taken)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `direct_bonus` not used (spell not have non-periodic affects)", entry);
        }

        if (!need_direct && sbe.ap_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `ap_bonus` not used (spell not have non-periodic affects)", entry);
        }
        else if (!need_dot && sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u `ap_dot_bonus` not used (spell not have periodic affects)", entry);
        }

        mSpellBonusMap[entry] = sbe;

        // also add to high ranks
        DoSpellBonuses worker(mSpellBonusMap, sbe);
        doForHighRanks(entry, worker);

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u extra spell bonus data",  count);
}

/**
 * @brief Checks whether a proc event definition can be triggered by a proc context.
 *
 * @param spellProcEvent The proc event definition to evaluate.
 * @param EventProcFlag The event flag being tested.
 * @param procSpell The spell that caused the proc, if any.
 * @param procFlags The proc flags of the current event.
 * @param procExtra Additional proc result flags.
 * @return true if the proc event can trigger; otherwise, false.
 */
bool SpellMgr::IsSpellProcEventCanTriggeredBy(SpellProcEventEntry const* spellProcEvent, uint32 EventProcFlag, SpellEntry const* procSpell, uint32 procFlags, uint32 procExtra)
{
    // No extra req need
    uint32 procEvent_procEx = PROC_EX_NONE;

    // check prockFlags for condition
    if ((procFlags & EventProcFlag) == 0)
    {
        return false;
    }

    // Always trigger for this
    if (EventProcFlag & (PROC_FLAG_KILLED | PROC_FLAG_KILL | PROC_FLAG_ON_TRAP_ACTIVATION))
    {
        return true;
    }

    if (spellProcEvent)     // Exist event data
    {
        // Store extra req
        procEvent_procEx = spellProcEvent->procEx;

        // For melee triggers
        if (procSpell == NULL)
        {
            // Check (if set) for school (melee attack have Normal school)
            if (spellProcEvent->schoolMask && (spellProcEvent->schoolMask & SPELL_SCHOOL_MASK_NORMAL) == 0)
            {
                return false;
            }
        }
        else // For spells need check school/spell family/family mask
        {
            // Check (if set) for school
            if (spellProcEvent->schoolMask && (spellProcEvent->schoolMask & GetSchoolMask(procSpell->School)) == 0)
            {
                return false;
            }

            // Check (if set) for spellFamilyName
            if (spellProcEvent->spellFamilyName && (spellProcEvent->spellFamilyName != procSpell->SpellClassSet))
            {
                return false;
            }
        }
    }

    // Check for extra req (if none) and hit/crit
    if (procEvent_procEx == PROC_EX_NONE)
    {
        // Don't allow proc from periodic heal if no extra requirement is defined
        if (EventProcFlag & (PROC_FLAG_ON_DO_PERIODIC | PROC_FLAG_ON_TAKE_PERIODIC) && (procExtra & PROC_EX_PERIODIC_POSITIVE))
        {
            return false;
        }

        // No extra req, so can trigger for (damage/healing present) and hit/crit
        if (procExtra & (PROC_EX_NORMAL_HIT | PROC_EX_CRITICAL_HIT))
        {
            return true;
        }
    }
    else // all spells hits here only if resist/reflect/immune/evade
    {
        // Exist req for PROC_EX_EX_TRIGGER_ALWAYS
        if (procEvent_procEx & PROC_EX_EX_TRIGGER_ALWAYS)
        {
            return true;
        }
        // Check Extra Requirement like (hit/crit/miss/resist/parry/dodge/block/immune/reflect/absorb and other)
        if (procEvent_procEx & procExtra)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Loads elixir classification data from the database.
 */
void SpellMgr::LoadSpellElixirs()
{
    mSpellElixirs.clear();                                  // need for reload case

    uint32 count = 0;

    //                                                0      1
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `mask` FROM `spell_elixir`");
    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString(">> Loaded %u spell elixir definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();
        uint8 mask = fields[1].GetUInt8();

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_elixir` does not exist", entry);
            continue;
        }

        mSpellElixirs[entry] = mask;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u spell elixir definitions", count);
    sLog.outString();
}

struct DoSpellThreat
{
    DoSpellThreat(SpellThreatMap& _threatMap) : threatMap(_threatMap), count(0) {}
    void operator()(uint32 spell_id)
    {
        SpellThreatEntry const& ste = state->second;
        // add ranks only for not filled data (spells adding flat threat are usually different for ranks)
        SpellThreatMap::const_iterator spellItr = threatMap.find(spell_id);
        if (spellItr == threatMap.end())
        {
            threatMap[spell_id] = ste;
        }

        // just assert that entry is not redundant
        else
        {
            SpellThreatEntry const& r_ste = spellItr->second;
            if (ste.threat == r_ste.threat && ste.multiplier == r_ste.multiplier && ste.ap_bonus == r_ste.ap_bonus)
            {
                sLog.outErrorDb("Spell %u listed in `spell_threat` as custom rank has same data as Rank 1, so redundant", spell_id);
            }
        }
    }

    const char* TableName()
    {
        return "spell_threat";
    }

    bool IsValidCustomRank(SpellThreatEntry const& ste, uint32 entry, uint32 first_id)
    {
        if (!ste.threat)
        {
            sLog.outErrorDb("Spell %u listed in `spell_threat` is not first rank (%u) in chain and has no threat", entry, first_id);
            // prevent loading unexpected data
            return false;
        }
        return true;
    }
    void AddEntry(SpellThreatEntry const& ste, SpellEntry const* spell)
    {
        threatMap[spell->ID] = ste;

        // flat threat bonus and attack power bonus currently only work properly when all
        // effects have same targets, otherwise, we'd need to seperate it by effect index
        if (ste.threat || ste.ap_bonus != 0.f)
        {
            const uint32* targetA = spell->ImplicitTargetA;
            if ((targetA[EFFECT_INDEX_1] && targetA[EFFECT_INDEX_1] != targetA[EFFECT_INDEX_0]) ||
                (targetA[EFFECT_INDEX_2] && targetA[EFFECT_INDEX_2] != targetA[EFFECT_INDEX_0]))
            {
                sLog.outErrorDb("Spell %u listed in `spell_threat` has effects with different targets, threat may be assigned incorrectly", spell->ID);
            }
        }
        ++count;
    }
    bool HasEntry(uint32 spellId) { return threatMap.count(spellId) > 0; }
    bool SetStateToEntry(uint32 spellId) { return (state = threatMap.find(spellId)) != threatMap.end(); }

    SpellThreatMap& threatMap;
    SpellThreatMap::const_iterator state;
    uint32 count;
};

/**
 * @brief Loads custom spell threat definitions from the database.
 */
void SpellMgr::LoadSpellThreats()
{
    mSpellThreatMap.clear();                                // need for reload case

    //                                                0      1       2           3
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `Threat`, `multiplier`, `ap_bonus` FROM `spell_threat`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> No spell threat entries loaded.");
        sLog.outString();
        return;
    }

    SpellRankHelper<SpellThreatEntry, DoSpellThreat, SpellThreatMap> rankHelper(*this, mSpellThreatMap);

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        uint32 entry = fields[0].GetUInt32();

        SpellThreatEntry ste;
        ste.threat = fields[1].GetUInt16();
        ste.multiplier = fields[2].GetFloat();
        ste.ap_bonus = fields[3].GetFloat();

        rankHelper.RecordRank(ste, entry);
    }
    while (result->NextRow());

    rankHelper.FillHigherRanks();

    delete result;

    sLog.outString(">> Loaded %u spell threat entries", rankHelper.worker.count);
    sLog.outString();
}

/**
 * @brief Loads and validates scripted spell targets.
 */
void SpellMgr::LoadSpellScriptTarget()
{
    sSpellScriptTargetStorage.Load();

    // Check content
    for (SQLMultiStorage::SQLSIterator<SpellTargetEntry> itr = sSpellScriptTargetStorage.getDataBegin<SpellTargetEntry>(); itr < sSpellScriptTargetStorage.getDataEnd<SpellTargetEntry>(); ++itr)
    {
        SpellEntry const* spellProto = sSpellStore.LookupEntry(itr->spellId);
        if (!spellProto)
        {
            sLog.outErrorDb("Table `spell_script_target`: spellId %u listed for TargetEntry %u does not exist.", itr->spellId, itr->targetEntry);
            sSpellScriptTargetStorage.EraseEntry(itr->spellId);
            continue;
        }

        bool targetfound = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            if (spellProto->ImplicitTargetA[i] == TARGET_SCRIPT ||
                spellProto->ImplicitTargetB[i] == TARGET_SCRIPT ||
                spellProto->ImplicitTargetA[i] == TARGET_SCRIPT_COORDINATES ||
                spellProto->ImplicitTargetB[i] == TARGET_SCRIPT_COORDINATES ||
                spellProto->ImplicitTargetA[i] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                spellProto->ImplicitTargetB[i] == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                spellProto->ImplicitTargetA[i] == TARGET_AREAEFFECT_INSTANT ||
                spellProto->ImplicitTargetB[i] == TARGET_AREAEFFECT_INSTANT ||
                spellProto->ImplicitTargetA[i] == TARGET_AREAEFFECT_CUSTOM ||
                spellProto->ImplicitTargetB[i] == TARGET_AREAEFFECT_CUSTOM ||
                spellProto->ImplicitTargetA[i] == TARGET_AREAEFFECT_GO_AROUND_SOURCE ||
                spellProto->ImplicitTargetB[i] == TARGET_AREAEFFECT_GO_AROUND_SOURCE ||
                spellProto->ImplicitTargetA[i] == TARGET_AREAEFFECT_GO_AROUND_DEST ||
                spellProto->ImplicitTargetB[i] == TARGET_AREAEFFECT_GO_AROUND_DEST ||
                spellProto->ImplicitTargetA[i] == TARGET_NARROW_FRONTAL_CONE ||
                spellProto->ImplicitTargetB[i] == TARGET_NARROW_FRONTAL_CONE)
            {
                targetfound = true;
                break;
            }
        }
        if (!targetfound)
        {
            sLog.outErrorDb("Table `spell_script_target`: spellId %u listed for TargetEntry %u does not have any implicit target TARGET_SCRIPT(38) or TARGET_SCRIPT_COORDINATES (46) or TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT (40).", itr->spellId, itr->targetEntry);
            sSpellScriptTargetStorage.EraseEntry(itr->spellId);
            continue;
        }

        if (itr->type >= MAX_SPELL_TARGET_TYPE)
        {
            sLog.outErrorDb("Table `spell_script_target`: target type %u for TargetEntry %u is incorrect.", itr->type, itr->targetEntry);
            sSpellScriptTargetStorage.EraseEntry(itr->spellId);
            continue;
        }

        // Checks by target type
        switch (itr->type)
        {
            case SPELL_TARGET_TYPE_GAMEOBJECT:
            {
                if (!itr->targetEntry)
                {
                    break;
                }

                if (!sGOStorage.LookupEntry<GameObjectInfo>(itr->targetEntry))
                {
                    sLog.outErrorDb("Table `spell_script_target`: gameobject template entry %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                    continue;
                }
                break;
            }
            default:
                if (!itr->targetEntry)
                {
                    sLog.outErrorDb("Table `spell_script_target`: target entry == 0 for not GO target type (%u).", itr->type);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                    continue;
                }
                if (const CreatureInfo* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(itr->targetEntry))
                {
                    if (itr->spellId == 30427 && !cInfo->SkinningLootId)
                    {
                        sLog.outErrorDb("Table `spell_script_target` has creature %u as a target of spellid 30427, but this creature has no SkinningLootId. Gas extraction will not work!", cInfo->Entry);
                        sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                        continue;
                    }
                }
                else
                {
                    sLog.outErrorDb("Table `spell_script_target`: creature template entry %u does not exist.", itr->targetEntry);
                    sSpellScriptTargetStorage.EraseEntry(itr->spellId);
                    continue;
                }
                break;
        }
    }

    // Check all spells
    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(i);
            if (!spellInfo)
            {
                continue;
            }

            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                if (spellInfo->Effect[j] && (spellInfo->ImplicitTargetA[j] == TARGET_SCRIPT ||
                    (spellInfo->ImplicitTargetA[j] != TARGET_SELF && spellInfo->ImplicitTargetB[j] == TARGET_SCRIPT)))
                {
                    SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(i);
                    if (bounds.first == bounds.second)
                    {
                        sLog.outErrorDb("Spell (ID: %u) has effect EffectImplicitTargetA/EffectImplicitTargetB = %u (TARGET_SCRIPT), but does not have record in `spell_script_target`", spellInfo->ID, TARGET_SCRIPT);
                        break;                              // effects of spell
                    }
                }
            }
        }
    }

    sLog.outString(">> Loaded %u spell_script_target definitions", sSpellScriptTargetStorage.GetRecordCount());
    sLog.outString();
}
