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

#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"

#include "SQLStorages.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "ProgressBar.h"
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
#include <limits>

INSTANTIATE_SINGLETON_1(ObjectMgr);

bool normalizePlayerName(std::string& name)
{
    if (name.empty())
    {
        return false;
    }

    wchar_t wstr_buf[MAX_INTERNAL_PLAYER_NAME + 1];
    size_t wstr_len = MAX_INTERNAL_PLAYER_NAME;

    if (!Utf8toWStr(name, &wstr_buf[0], wstr_len))
    {
        return false;
    }

    wstr_buf[0] = wcharToUpper(wstr_buf[0]);
    for (size_t i = 1; i < wstr_len; ++i)
    {
        wstr_buf[i] = wcharToLower(wstr_buf[i]);
    }

    if (!WStrToUtf8(wstr_buf, wstr_len, name))
    {
        return false;
    }

    return true;
}

LanguageDesc lang_description[LANGUAGES_COUNT] =
{
    { LANG_ADDON,           0, 0                       },
    { LANG_UNIVERSAL,       0, 0                       },
    { LANG_ORCISH,        669, SKILL_LANG_ORCISH       },
    { LANG_DARNASSIAN,    671, SKILL_LANG_DARNASSIAN   },
    { LANG_TAURAHE,       670, SKILL_LANG_TAURAHE      },
    { LANG_DWARVISH,      672, SKILL_LANG_DWARVEN      },
    { LANG_COMMON,        668, SKILL_LANG_COMMON       },
    { LANG_DEMONIC,       815, SKILL_LANG_DEMON_TONGUE },
    { LANG_TITAN,         816, SKILL_LANG_TITAN        },
    { LANG_THALASSIAN,    813, SKILL_LANG_THALASSIAN   },
    { LANG_DRACONIC,      814, SKILL_LANG_DRACONIC     },
    { LANG_KALIMAG,       817, SKILL_LANG_OLD_TONGUE   },
    { LANG_GNOMISH,      7340, SKILL_LANG_GNOMISH      },
    { LANG_TROLL,        7341, SKILL_LANG_TROLL        },
    { LANG_GUTTERSPEAK, 17737, SKILL_LANG_GUTTERSPEAK  },
};

LanguageDesc const* GetLanguageDescByID(uint32 lang)
{
    for (int i = 0; i < LANGUAGES_COUNT; ++i)
    {
        if (uint32(lang_description[i].lang_id) == lang)
        {
            return &lang_description[i];
        }
    }

    return NULL;
}

template<typename T>
T IdGenerator<T>::Generate()
{
    if (m_nextGuid >= std::numeric_limits<T>::max() - 1)
    {
        sLog.outError("%s guid overflow!! Can't continue, shutting down server. ", m_name);
        World::StopNow(ERROR_EXIT_CODE);
    }
    return m_nextGuid++;
}

template uint32 IdGenerator<uint32>::Generate();
template uint64 IdGenerator<uint64>::Generate();

// create the standing order
bool operator < (const HonorStanding& lhs, const HonorStanding& rhs)
{
    return lhs.honorPoints > rhs.honorPoints;
}

// TODO improve the algorithm based on conditions
bool AreaTrigger::IsLessOrEqualThan(AreaTrigger const* l) const      // Expected to have same map
{
    MANGOS_ASSERT(target_mapId == l->target_mapId);
    if (!condition)
    {
        return true;
    }

    if (!l->condition)
    {
        return false;
    }

    if (condition == l->condition)
    {
        return true;
    }

    // most conditions for AT have level requirement
    // let's order by the least restrictive
    const PlayerCondition* pCond1 = sConditionStorage.LookupEntry<PlayerCondition>(condition);
    const PlayerCondition* pCond2 = sConditionStorage.LookupEntry<PlayerCondition>(l->condition);
    if (pCond1->m_condition == CONDITION_LEVEL && pCond2->m_condition == CONDITION_LEVEL)
    {
        return (pCond1->m_value1 <= pCond2->m_value1);
    }
    if (pCond1->m_condition == CONDITION_LEVEL)
    {
        return false;
    }
    if (pCond2->m_condition == CONDITION_LEVEL)
    {
        return true;
    }
    return false;
}

ObjectMgr::ObjectMgr() :
    m_AuctionIds("Auction ids"),
    m_GuildIds("Guild ids"),
    m_ItemTextIds("Item text ids"),
    m_MailIds("Mail ids"),
    m_PetNumbers("Pet numbers"),
    m_GroupIds("Group ids"),
    m_FirstTemporaryCreatureGuid(1),
    m_FirstTemporaryGameObjectGuid(1),
    DBCLocaleIndex(LOCALE_enUS)
{
}

ObjectMgr::~ObjectMgr()
{
    for (QuestMap::iterator i = mQuestTemplates.begin(); i != mQuestTemplates.end(); ++i)
    {
        delete i->second;
    }

    for (PetLevelInfoMap::iterator i = petInfo.begin(); i != petInfo.end(); ++i)
    {
        delete[] i->second;
    }

    // free only if loaded
    for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
    {
        delete[] playerClassInfo[class_].levelInfo;
    }

    for (int race = 0; race < MAX_RACES; ++race)
    {
        for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
        {
            delete[] playerInfo[race][class_].levelInfo;
        }
    }

    // free objects
    for (GroupMap::iterator itr = mGroupMap.begin(); itr != mGroupMap.end(); ++itr)
    {
        delete itr->second;
    }

    for (CacheVendorItemMap::iterator itr = m_mCacheVendorTemplateItemMap.begin(); itr != m_mCacheVendorTemplateItemMap.end(); ++itr)
    {
        itr->second.Clear();
    }

    for (CacheVendorItemMap::iterator itr = m_mCacheVendorItemMap.begin(); itr != m_mCacheVendorItemMap.end(); ++itr)
    {
        itr->second.Clear();
    }

    for (CacheTrainerSpellMap::iterator itr = m_mCacheTrainerSpellMap.begin(); itr != m_mCacheTrainerSpellMap.end(); ++itr)
    {
        itr->second.Clear();
    }
}

Group* ObjectMgr::GetGroupById(uint32 id) const
{
    GroupMap::const_iterator itr = mGroupMap.find(id);
    if (itr != mGroupMap.end())
    {
        return itr->second;
    }

    return NULL;
}

void ObjectMgr::AddLocaleString(std::string const& s, LocaleConstant locale, StringVector& data)
{
    if (!s.empty())
    {
        if (data.size() <= size_t(locale))
        {
            data.resize(locale + 1);
        }
        data[locale] = s;
    }
}

void ObjectMgr::LoadCreatureLocales()
{
    mCreatureLocaleMap.clear();                             // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`name_loc1`,`subname_loc1`,`name_loc2`,`subname_loc2`,`name_loc3`,`subname_loc3`,`name_loc4`,`subname_loc4`,`name_loc5`,`subname_loc5`,`name_loc6`,`subname_loc6`,`name_loc7`,`subname_loc7`,`name_loc8`,`subname_loc8` FROM `locales_creature`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 creature locale strings. DB table `locales_creature` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetCreatureTemplate(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_creature` has data for not existed creature entry %u, skipped.", entry);
            continue;
        }

        CreatureLocale& data = mCreatureLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }
            str = fields[1 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.SubName.size() <= idx)
                    {
                        data.SubName.resize(idx + 1);
                    }

                    data.SubName[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " creature locale strings", mCreatureLocaleMap.size());
    sLog.outString();
}

void ObjectMgr::LoadGossipMenuItemsLocales()
{
    mGossipMenuItemsLocaleMap.clear();                      // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `menu_id`,`id`,"
                          "`option_text_loc1`,`box_text_loc1`,`option_text_loc2`,`box_text_loc2`,"
                          "`option_text_loc3`,`box_text_loc3`,`option_text_loc4`,`box_text_loc4`,"
                          "`option_text_loc5`,`box_text_loc5`,`option_text_loc6`,`box_text_loc6`,"
                          "`option_text_loc7`,`box_text_loc7`,`option_text_loc8`,`box_text_loc8` "
                          "FROM `locales_gossip_menu_option`");

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 gossip_menu_option locale strings. DB table `locales_gossip_menu_option` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint16 menuId   = fields[0].GetUInt16();
        uint16 id       = fields[1].GetUInt16();

        GossipMenuItemsMapBounds bounds = GetGossipMenuItemsMapBounds(menuId);

        bool found = false;
        if (bounds.first != bounds.second)
        {
            for (GossipMenuItemsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                if (itr->second.id == id)
                {
                    found = true;
                    break;
                }
            }
        }

        if (!found)
        {
            ERROR_DB_STRICT_LOG("Table `locales_gossip_menu_option` has data for nonexistent gossip menu %u item %u, skipped.", menuId, id);
            continue;
        }

        GossipMenuItemsLocale& data = mGossipMenuItemsLocaleMap[MAKE_PAIR32(menuId, id)];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[2 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OptionText.size() <= idx)
                    {
                        data.OptionText.resize(idx + 1);
                    }

                    data.OptionText[idx] = str;
                }
            }
            str = fields[2 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.BoxText.size() <= idx)
                    {
                        data.BoxText.resize(idx + 1);
                    }

                    data.BoxText[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " gossip_menu_option locale strings", mGossipMenuItemsLocaleMap.size());
    sLog.outString();
}

void ObjectMgr::LoadPointOfInterestLocales()
{
    mPointOfInterestLocaleMap.clear();                      // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`icon_name_loc1`,`icon_name_loc2`,`icon_name_loc3`,`icon_name_loc4`,`icon_name_loc5`,`icon_name_loc6`,`icon_name_loc7`,`icon_name_loc8` FROM `locales_points_of_interest`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 points_of_interest locale strings. DB table `locales_points_of_interest` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetPointOfInterest(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_points_of_interest` has data for nonexistent POI entry %u, skipped.", entry);
            continue;
        }

        PointOfInterestLocale& data = mPointOfInterestLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (str.empty())
            {
                continue;
            }

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if (data.IconName.size() <= size_t(idx))
                {
                    data.IconName.resize(idx + 1);
                }

                data.IconName[idx] = str;
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " points_of_interest locale strings", mPointOfInterestLocaleMap.size());
    sLog.outString();
}

struct SQLCreatureLoader : public SQLStorageLoaderBase<SQLCreatureLoader, SQLStorage>
{
    template<class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

void ObjectMgr::LoadCreatureTemplates()
{
    SQLCreatureLoader loader;
    loader.Load(sCreatureStorage);

    // check data correctness
    for (uint32 i = 1; i < sCreatureStorage.GetMaxEntry(); ++i)
    {
        CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i);
        if (!cInfo)
        {
            continue;
        }

        FactionTemplateEntry const* factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->FactionAlliance);
        if (!factionTemplate)
        {
            sLog.outErrorDb("Creature (Entry: %u) has nonexistent faction_A template (%u)", cInfo->Entry, cInfo->FactionAlliance);
        }

        factionTemplate = sFactionTemplateStore.LookupEntry(cInfo->FactionHorde);
        if (!factionTemplate)
        {
            sLog.outErrorDb("Creature (Entry: %u) has nonexistent faction_H template (%u)", cInfo->Entry, cInfo->FactionHorde);
        }

        for (int k = 0; k < MAX_KILL_CREDIT; ++k)
        {
            if (cInfo->KillCredit[k])
            {
                if (!GetCreatureTemplate(cInfo->KillCredit[k]))
                {
                    sLog.outErrorDb("Creature (Entry: %u) has nonexistent creature entry in `KillCredit%d` (%u)", cInfo->Entry, k + 1, cInfo->KillCredit[k]);
                    const_cast<CreatureInfo*>(cInfo)->KillCredit[k] = 0;
                }
            }
        }

        // used later for scale
        CreatureDisplayInfoEntry const* displayScaleEntry = NULL;

        for (int j = 0; j < MAX_CREATURE_MODEL; ++j)
        {
            if (cInfo->ModelId[j])
            {
                CreatureDisplayInfoEntry const* displayEntry = sCreatureDisplayInfoStore.LookupEntry(cInfo->ModelId[j]);
                if (!displayEntry)
                {
                    sLog.outErrorDb("Creature (Entry: %u) has nonexistent modelid_%d (%u), can crash client", cInfo->Entry, j + 1, cInfo->ModelId[j]);
                    const_cast<CreatureInfo*>(cInfo)->ModelId[j] = 0;
                }
                else if (!displayScaleEntry)
                {
                    displayScaleEntry = displayEntry;
                }

                CreatureModelInfo const* minfo = sCreatureModelStorage.LookupEntry<CreatureModelInfo>(cInfo->ModelId[j]);
                if (!minfo)
                {
                    sLog.outErrorDb("Creature (Entry: %u) are using modelid_%d (%u), but creature_model_info are missing for this model.", cInfo->Entry, j + 1, cInfo->ModelId[j]);
                }
            }
        }

        if (!displayScaleEntry)
        {
            sLog.outErrorDb("Creature (Entry: %u) has nonexistent modelid in modelid_1/modelid_2/modelid_3/modelid_4", cInfo->Entry);
        }

        if (!cInfo->MinLevel)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid minlevel, set to 1", cInfo->Entry);
            const_cast<CreatureInfo*>(cInfo)->MinLevel = 1;
        }

        if (cInfo->MinLevel > cInfo->MaxLevel)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid maxlevel, set to minlevel", cInfo->Entry);
            const_cast<CreatureInfo*>(cInfo)->MaxLevel = cInfo->MinLevel;
        }

        if (cInfo->MinLevel > DEFAULT_MAX_CREATURE_LEVEL)
        {
            sLog.outErrorDb("Creature (Entry: %u) `MinLevel` exceeds maximum allowed value of '%u'", cInfo->Entry, uint32(DEFAULT_MAX_CREATURE_LEVEL));
            const_cast<CreatureInfo*>(cInfo)->MinLevel = uint32(DEFAULT_MAX_CREATURE_LEVEL);
        }

        if (cInfo->MaxLevel > DEFAULT_MAX_CREATURE_LEVEL)
        {
            sLog.outErrorDb("Creature (Entry: %u) `MaxLevel` exceeds maximum allowed value of '%u'", cInfo->Entry, uint32(DEFAULT_MAX_CREATURE_LEVEL));
            const_cast<CreatureInfo*>(cInfo)->MaxLevel = uint32(DEFAULT_MAX_CREATURE_LEVEL);
        }

        // use below code for 0-checks for unit_class
        if (!cInfo->UnitClass || (((1 << (cInfo->UnitClass - 1)) & CLASSMASK_ALL_CREATURES) == 0))
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid `UnitClass(%u)` in creature_template", cInfo->Entry, cInfo->UnitClass);
            const_cast<CreatureInfo*>(cInfo)->UnitClass = uint32(CLASS_WARRIOR);
        }

        for (uint32 level = cInfo->MinLevel; level <= cInfo->MaxLevel; ++level)
        {
            if (!GetCreatureClassLvlStats(level, cInfo->UnitClass))
            {
                sLog.outErrorDb("#Creature (Entry: %u), Class(%u), level(%u) has no data in `creature_template_classlevelstats`", cInfo->Entry, cInfo->UnitClass, level);
                break;
            }
        }

        if (cInfo->DamageSchool >= MAX_SPELL_SCHOOL)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid spell school value (%u) in `dmgschool`", cInfo->Entry, cInfo->DamageSchool);
            const_cast<CreatureInfo*>(cInfo)->DamageSchool = SPELL_SCHOOL_NORMAL;
        }

        if (cInfo->MeleeBaseAttackTime == 0)
        {
            const_cast<CreatureInfo*>(cInfo)->MeleeBaseAttackTime  = BASE_ATTACK_TIME;
        }

        if (cInfo->RangedBaseAttackTime == 0)
        {
            const_cast<CreatureInfo*>(cInfo)->RangedBaseAttackTime = BASE_ATTACK_TIME;
        }

        if ((cInfo->NpcFlags & UNIT_NPC_FLAG_TRAINER) && cInfo->TrainerType >= MAX_TRAINER_TYPE)
        {
            sLog.outErrorDb("Creature (Entry: %u) has wrong trainer type %u", cInfo->Entry, cInfo->TrainerType);
        }

        if (cInfo->CreatureType && !sCreatureTypeStore.LookupEntry(cInfo->CreatureType))
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid creature type (%u) in `type`", cInfo->Entry, cInfo->CreatureType);
            const_cast<CreatureInfo*>(cInfo)->CreatureType = CREATURE_TYPE_HUMANOID;
        }

        // must exist or used hidden but used in data horse case
        if (cInfo->Family && !sCreatureFamilyStore.LookupEntry(cInfo->Family) && cInfo->Family != CREATURE_FAMILY_HORSE_CUSTOM)
        {
            sLog.outErrorDb("Creature (Entry: %u) has invalid creature family (%u) in `family`", cInfo->Entry, cInfo->Family);
            const_cast<CreatureInfo*>(cInfo)->Family = 0;
        }

        if (cInfo->InhabitType <= 0 || cInfo->InhabitType > INHABIT_ANYWHERE)
        {
            sLog.outErrorDb("Creature (Entry: %u) has wrong value (%u) in `InhabitType`, creature will not correctly walk/swim/fly", cInfo->Entry, cInfo->InhabitType);
            const_cast<CreatureInfo*>(cInfo)->InhabitType = INHABIT_ANYWHERE;
        }

        if (cInfo->PetSpellDataId)
        {
            CreatureSpellDataEntry const* spellDataId = sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId);
            if (!spellDataId)
            {
                sLog.outErrorDb("Creature (Entry: %u) has non-existing PetSpellDataId (%u)", cInfo->Entry, cInfo->PetSpellDataId);
            }
        }

        if (cInfo->MovementType >= MAX_DB_MOTION_TYPE)
        {
            sLog.outErrorDb("Creature (Entry: %u) has wrong movement generator type (%u), ignore and set to IDLE.", cInfo->Entry, cInfo->MovementType);
            const_cast<CreatureInfo*>(cInfo)->MovementType = IDLE_MOTION_TYPE;
        }

        if (cInfo->EquipmentTemplateId > 0)                         // 0 no equipment
        {
            if (!GetEquipmentInfo(cInfo->EquipmentTemplateId) && !GetEquipmentInfoRaw(cInfo->EquipmentTemplateId))
            {
                sLog.outErrorDb("Table `creature_template` have creature (Entry: %u) with equipment_id %u not found in table `creature_equip_template` or `creature_equip_template_raw`, set to no equipment.", cInfo->Entry, cInfo->EquipmentTemplateId);
                const_cast<CreatureInfo*>(cInfo)->EquipmentTemplateId = 0;
            }
        }

        if (cInfo->VendorTemplateId > 0)
        {
            if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR))
            {
                sLog.outErrorDb("Table `creature_template` have creature (Entry: %u) with vendor_id %u but not have flag UNIT_NPC_FLAG_VENDOR (%u), vendor items will ignored.", cInfo->Entry, cInfo->VendorTemplateId, UNIT_NPC_FLAG_VENDOR);
            }
        }

        /// if not set custom creature scale then load scale from CreatureDisplayInfo.dbc
        if (cInfo->Scale <= 0.0f)
        {
            if (displayScaleEntry)
            {
                const_cast<CreatureInfo*>(cInfo)->Scale = displayScaleEntry->scale;
            }
            else
            {
                const_cast<CreatureInfo*>(cInfo)->Scale = DEFAULT_OBJECT_SCALE;
            }
        }
    }

    sLog.outString(">> Loaded %u creature definitions", sCreatureStorage.GetRecordCount());
    sLog.outString();
}

void ObjectMgr::ConvertCreatureAddonAuras(CreatureDataAddon* addon, char const* table, char const* guidEntryStr)
{
    // Now add the auras, format "spell1 spell2 ..."
    char* p, *s;
    std::vector<int> val;
    s = p = (char*)reinterpret_cast<char const*>(addon->auras);
    if (p)
    {
        while (p[0] != 0)
        {
            ++p;
            if (p[0] == ' ')
            {
                val.push_back(atoi(s));
                s = ++p;
            }
        }
        if (p != s)
        {
            val.push_back(atoi(s));
        }

        // free char* loaded memory
        delete[](char*)reinterpret_cast<char const*>(addon->auras);
    }

    // empty list
    if (val.empty())
    {
        addon->auras = NULL;
        return;
    }

    // replace by new structures array
    const_cast<uint32*&>(addon->auras) = new uint32[val.size() + 1];

    uint32 i = 0;
    for (uint32 j = 0; j < val.size(); ++j)
    {
        uint32& cAura = const_cast<uint32&>(addon->auras[i]);
        cAura = uint32(val[j]);

        SpellEntry const* AdditionalSpellInfo = sSpellStore.LookupEntry(cAura);
        if (!AdditionalSpellInfo)
        {
            sLog.outErrorDb("Creature (%s: %u) has wrong spell %u defined in `auras` field in `%s`.", guidEntryStr, addon->guidOrEntry, cAura, table);
            continue;
        }

        // Must be Aura, but also allow dummy/script effect spells, as they are used sometimes to select a random aura or similar
        if (!IsSpellAppliesAura(AdditionalSpellInfo) && !AdditionalSpellInfo->HasSpellEffect(SPELL_EFFECT_DUMMY) && !AdditionalSpellInfo->HasSpellEffect(SPELL_EFFECT_SCRIPT_EFFECT) && !AdditionalSpellInfo->HasSpellEffect(SPELL_EFFECT_TRIGGER_SPELL))
        {
            sLog.outErrorDb("Creature (%s: %u) has spell %u defined in `auras` field in `%s, but spell doesn't apply an aura`.", guidEntryStr, addon->guidOrEntry, cAura, table);
            continue;
        }

        // TODO: Remove LogFilter check after more research
        if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK) && !IsOnlySelfTargeting(AdditionalSpellInfo))
        {
            sLog.outErrorDb("Creature (%s: %u) has spell %u defined in `auras` field in `%s, but spell is no self-only spell`.", guidEntryStr, addon->guidOrEntry, cAura, table);
            continue;
        }

        if (std::find(&addon->auras[0], &addon->auras[i], cAura) != &addon->auras[i])
        {
            sLog.outErrorDb("Creature (%s: %u) has duplicate spell %u defined in `auras` field in `%s`.", guidEntryStr, addon->guidOrEntry, cAura, table);
            continue;
        }

        ++i;
    }

    // fill terminator element (after last added)
    const_cast<uint32&>(addon->auras[i]) = 0;
}

void ObjectMgr::LoadCreatureAddons(SQLStorage& creatureaddons, char const* entryName, char const* comment)
{
    creatureaddons.Load();

    // check data correctness and convert 'auras'
    for (uint32 i = 1; i < creatureaddons.GetMaxEntry(); ++i)
    {
        CreatureDataAddon const* addon = creatureaddons.LookupEntry<CreatureDataAddon>(i);
        if (!addon)
        {
            continue;
        }

        if (addon->mount)
        {
            if (!sCreatureDisplayInfoStore.LookupEntry(addon->mount))
            {
                sLog.outErrorDb("Creature (%s %u) have invalid displayInfoId for mount (%u) defined in `%s`.", entryName, addon->guidOrEntry, addon->mount, creatureaddons.GetTableName());
                const_cast<CreatureDataAddon*>(addon)->mount = 0;
            }
        }

        if (addon->sheath_state > SHEATH_STATE_RANGED)
        {
            sLog.outErrorDb("Creature (%s %u) has unknown sheath state (%u) defined in `%s`.", entryName, addon->guidOrEntry, addon->sheath_state, creatureaddons.GetTableName());
        }

        if (!sEmotesStore.LookupEntry(addon->emote))
        {
            sLog.outErrorDb("Creature (%s %u) have invalid emote (%u) defined in `%s`.", entryName, addon->guidOrEntry, addon->emote, creatureaddons.GetTableName());
            const_cast<CreatureDataAddon*>(addon)->emote = 0;
        }

        ConvertCreatureAddonAuras(const_cast<CreatureDataAddon*>(addon), creatureaddons.GetTableName(), entryName);
    }

    sLog.outString(">> Loaded %u %s", creatureaddons.GetRecordCount(), comment);
}

void ObjectMgr::LoadCreatureAddons()
{
    LoadCreatureAddons(sCreatureInfoAddonStorage, "Entry", "creature template addons");

    // check entry ids
    for (uint32 i = 1; i < sCreatureInfoAddonStorage.GetMaxEntry(); ++i)
    {
        if (CreatureDataAddon const* addon = sCreatureInfoAddonStorage.LookupEntry<CreatureDataAddon>(i))
        {
            if (!sCreatureStorage.LookupEntry<CreatureInfo>(addon->guidOrEntry))
            {
                sLog.outErrorDb("Creature (Entry: %u) does not exist but has a record in `%s`", addon->guidOrEntry, sCreatureInfoAddonStorage.GetTableName());
            }
        }
    }

    LoadCreatureAddons(sCreatureDataAddonStorage, "GUID", "creature addons");

    // check entry ids
    for (uint32 i = 1; i < sCreatureDataAddonStorage.GetMaxEntry(); ++i)
    {
        if (CreatureDataAddon const* addon = sCreatureDataAddonStorage.LookupEntry<CreatureDataAddon>(i))
        {
            if (mCreatureDataMap.find(addon->guidOrEntry) == mCreatureDataMap.end())
            {
                sLog.outErrorDb("Creature (GUID: %u) does not exist but has a record in `creature_addon`", addon->guidOrEntry);
            }
        }
    }
}

void ObjectMgr::LoadCreatureItemTemplates()
{
    sEquipmentStorageItem.Load(true);

    for (uint32 i = 0; i < sEquipmentStorageItem.GetMaxEntry(); ++i)
    {
        EquipmentInfoItem const* eqInfo = sEquipmentStorageItem.LookupEntry<EquipmentInfoItem>(i);

        if (!eqInfo)
        {
            continue;
        }

        EquipmentInfoItem const* itemProto = GetEquipmentInfoItem(eqInfo->entry);

        switch (itemProto->InventoryType)
        {
            case INVTYPE_2HWEAPON:
            case INVTYPE_HOLDABLE:
            case INVTYPE_QUIVER:
            case INVTYPE_RANGED:
            case INVTYPE_RANGEDRIGHT:
            case INVTYPE_RELIC:
            case INVTYPE_SHIELD:
            case INVTYPE_THROWN:
            case INVTYPE_WEAPON:
            case INVTYPE_WEAPONMAINHAND:
            case INVTYPE_WEAPONOFFHAND:
                break;
            default:
                sLog.outErrorDb("Item (entry=%u) in creature_item_template.entry for entry = %u is not equipable in a hand, forced to 0.", eqInfo->entry, i);
                const_cast<EquipmentInfoItem*>(eqInfo)->entry = 0;
                break;
        }
    }

    sLog.outString(">> Loaded %u creature item template", sEquipmentStorageItem.GetRecordCount());
    sLog.outString();
}

void ObjectMgr::LoadCreatureClassLvlStats()
{
    // initialize data array
    memset(&m_creatureClassLvlStats, 0, sizeof(m_creatureClassLvlStats));

    std::string queryStr = "SELECT `Class`, `Level`, `BaseMana`, `BaseMeleeAttackPower`, `BaseRangedAttackPower`, `BaseArmor`, `BaseHealthExp0`, `BaseDamageExp0` "
                           "FROM `creature_template_classlevelstats` ORDER BY `Class`, `Level`";

    QueryResult* result = WorldDatabase.Query(queryStr.c_str());

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb("DB table `creature_template_classlevelstats` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());
    uint32 storedRow = 0;

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 creatureClass               = fields[0].GetUInt32();
        uint32 creatureLevel               = fields[1].GetUInt32();

        if (creatureLevel == 0 || creatureLevel > DEFAULT_MAX_CREATURE_LEVEL)
        {
            sLog.outErrorDb("Found stats for creature level [%u], incorrect level for this core. Skip!", creatureLevel);
            continue;
        }

        if (((1 << (creatureClass - 1)) & CLASSMASK_ALL_CREATURES) == 0)
        {
            sLog.outErrorDb("Found stats for creature class [%u], incorrect class for this core. Skip!", creatureClass);
            continue;
        }

        CreatureClassLvlStats &cCLS = m_creatureClassLvlStats[creatureLevel][classToIndex[creatureClass]];

        cCLS.BaseMana               = fields[2].GetUInt32();
        cCLS.BaseMeleeAttackPower   = fields[3].GetFloat();
        cCLS.BaseRangedAttackPower  = fields[4].GetFloat();
        cCLS.BaseArmor              = fields[5].GetUInt32();
        cCLS.BaseHealth             = fields[6].GetUInt32();
        cCLS.BaseDamage             = fields[7].GetFloat();

        ++storedRow;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Found %u creature stats definitions.", storedRow);
    sLog.outString();
}

CreatureClassLvlStats const* ObjectMgr::GetCreatureClassLvlStats(uint32 level, uint32 unitClass) const
{
    CreatureClassLvlStats const* cCLS = &m_creatureClassLvlStats[level][classToIndex[unitClass]];

    if (cCLS->BaseHealth != 0 && cCLS->BaseDamage > 0.01f)
    {
        return cCLS;
    }

    return NULL;
}

void ObjectMgr::LoadEquipmentTemplates()
{
    sEquipmentStorage.Load(true);

    for (uint32 i = 0; i < sEquipmentStorage.GetMaxEntry(); ++i)
    {
        EquipmentInfo const* eqInfo = sEquipmentStorage.LookupEntry<EquipmentInfo>(i);

        if (!eqInfo)
        {
            continue;
        }

        for (uint8 j = 0; j < 3; ++j)
        {
            if (!eqInfo->equipentry[j])
            {
                continue;
            }

            EquipmentInfoItem const* itemProto = GetEquipmentInfoItem(eqInfo->equipentry[j]);
            if (!itemProto)
            {
                sLog.outErrorDb("Unknown item (entry=%u) in creature_equip_template.equipentry%u for entry = %u, forced to 0.", eqInfo->equipentry[j], j + 1, i);
                const_cast<EquipmentInfo*>(eqInfo)->equipentry[j] = 0;
                continue;
            }

            switch (itemProto->InventoryType)
            {
                case INVTYPE_WEAPON:
                case INVTYPE_SHIELD:
                case INVTYPE_RANGED:
                case INVTYPE_2HWEAPON:
                case INVTYPE_WEAPONMAINHAND:
                case INVTYPE_WEAPONOFFHAND:
                case INVTYPE_HOLDABLE:
                case INVTYPE_THROWN:
                case INVTYPE_RANGEDRIGHT:
                case INVTYPE_RELIC:
                    break;
                default:
                    sLog.outErrorDb("Item (entry=%u) in creature_equip_template.equipentry%u for entry = %u is not equipable in a hand, forced to 0.", eqInfo->equipentry[j], j + 1, i);
                    const_cast<EquipmentInfo*>(eqInfo)->equipentry[j] = 0;
                    break;
            }
        }
    }

    sLog.outString(">> Loaded %u equipment template", sEquipmentStorage.GetRecordCount());
    sLog.outString();

    sEquipmentStorageRaw.Load(false);
    for (uint32 i = 1; i < sEquipmentStorageRaw.GetMaxEntry(); ++i)
    {
        if (sEquipmentStorageRaw.LookupEntry<EquipmentInfoRaw>(i))
        {
            if (sEquipmentStorage.LookupEntry<EquipmentInfo>(i))
            {
                sLog.outErrorDb("Table 'creature_equip_template_raw` have redundant data for ID %u ('creature_equip_template` already have data)", i);
            }
        }
    }
    sLog.outString(">> Loaded %u equipment template (deprecated format)", sEquipmentStorageRaw.GetRecordCount());
    sLog.outString();
}

// generally for models having another model for the other team (totems)
uint32 ObjectMgr::GetCreatureModelOtherTeamModel(uint32 modelId) const
{
    if (const CreatureModelInfo* modelInfo = GetCreatureModelInfo(modelId))
    {
        return modelInfo->modelid_other_team;
    }

    return 0;
}

CreatureModelInfo const* ObjectMgr::GetCreatureModelRandomGender(uint32 display_id) const
{
    CreatureModelInfo const* minfo = GetCreatureModelInfo(display_id);
    if (!minfo)
    {
        return NULL;
    }

    // If a model for another gender exists, 50% chance to use it
    if (minfo->modelid_other_gender != 0 && urand(0, 1) == 0)
    {
        CreatureModelInfo const* minfo_tmp = GetCreatureModelInfo(minfo->modelid_other_gender);
        if (!minfo_tmp)
        {
            sLog.outErrorDb("Model (Entry: %u) has modelid_other_gender %u not found in table `creature_model_info`. ", minfo->modelid, minfo->modelid_other_gender);
            return minfo;                                   // not fatal, just use the previous one
        }
        else
        {
            return minfo_tmp;
        }
    }
    else
    {
        return minfo;
    }
}

void ObjectMgr::LoadCreatureModelInfo()
{
    sCreatureModelStorage.Load();

    // post processing
    for (uint32 i = 1; i < sCreatureModelStorage.GetMaxEntry(); ++i)
    {
        CreatureModelInfo const* minfo = sCreatureModelStorage.LookupEntry<CreatureModelInfo>(i);
        if (!minfo)
        {
            continue;
        }

        if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid))
        {
            sLog.outErrorDb("Table `creature_model_info` has model for nonexistent model id (%u).", minfo->modelid);
        }

        if (minfo->gender >= MAX_GENDER)
        {
            sLog.outErrorDb("Table `creature_model_info` has invalid gender (%u) for model id (%u).", uint32(minfo->gender), minfo->modelid);
            const_cast<CreatureModelInfo*>(minfo)->gender = GENDER_MALE;
        }

        if (minfo->modelid_other_gender)
        {
            if (minfo->modelid_other_gender == minfo->modelid)
            {
                sLog.outErrorDb("Table `creature_model_info` has redundant modelid_other_gender model (%u) defined for model id %u.", minfo->modelid_other_gender, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_other_gender = 0;
            }
            else if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid_other_gender))
            {
                sLog.outErrorDb("Table `creature_model_info` has nonexistent modelid_other_gender model (%u) defined for model id %u.", minfo->modelid_other_gender, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_other_gender = 0;
            }
        }

        if (minfo->modelid_other_team)
        {
            if (minfo->modelid_other_team == minfo->modelid)
            {
                sLog.outErrorDb("Table `creature_model_info` has redundant modelid_other_team model (%u) defined for model id %u.", minfo->modelid_other_team, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_other_team = 0;
            }
            else if (!sCreatureDisplayInfoStore.LookupEntry(minfo->modelid_other_team))
            {
                sLog.outErrorDb("Table `creature_model_info` has nonexistent modelid_other_team model (%u) defined for model id %u.", minfo->modelid_other_team, minfo->modelid);
                const_cast<CreatureModelInfo*>(minfo)->modelid_other_team = 0;
            }
        }
    }

    // character races expected have model info data in table
    for (uint32 race = 1; race < sChrRacesStore.GetNumRows(); ++race)
    {
        ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
        if (!raceEntry)
        {
            continue;
        }

        if (!((1 << (race - 1)) & RACEMASK_ALL_PLAYABLE))
        {
            continue;
        }

        if (CreatureModelInfo const* minfo = GetCreatureModelInfo(raceEntry->model_f))
        {
            if (minfo->gender != GENDER_FEMALE)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong gender %u for character race %u female model id %u", minfo->gender, race, raceEntry->model_f);
            }

            if (minfo->modelid_other_gender != raceEntry->model_m)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong other gender model id %u for character race %u female model id %u", minfo->modelid_other_gender, race, raceEntry->model_f);
            }

            if (minfo->bounding_radius <= 0.0f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong bounding_radius %f for character race %u female model id %u, use %f instead", minfo->bounding_radius, race, raceEntry->model_f, DEFAULT_WORLD_OBJECT_SIZE);
                const_cast<CreatureModelInfo*>(minfo)->bounding_radius = DEFAULT_WORLD_OBJECT_SIZE;
            }

            if (minfo->combat_reach != 1.5f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong combat_reach %f for character race %u female model id %u, expected always 1.5f", minfo->combat_reach, race, raceEntry->model_f);
                const_cast<CreatureModelInfo*>(minfo)->combat_reach = 1.5f;
            }
        }
        else
        {
            sLog.outErrorDb("Table `creature_model_info` expect have data for character race %u female model id %u", race, raceEntry->model_f);
        }

        if (CreatureModelInfo const* minfo = GetCreatureModelInfo(raceEntry->model_m))
        {
            if (minfo->gender != GENDER_MALE)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong gender %u for character race %u male model id %u", minfo->gender, race, raceEntry->model_m);
            }

            if (minfo->modelid_other_gender != raceEntry->model_f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong other gender model id %u for character race %u male model id %u", minfo->modelid_other_gender, race, raceEntry->model_m);
            }

            if (minfo->bounding_radius <= 0.0f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong bounding_radius %f for character race %u male model id %u, use %f instead", minfo->bounding_radius, race, raceEntry->model_f, DEFAULT_WORLD_OBJECT_SIZE);
                const_cast<CreatureModelInfo*>(minfo)->bounding_radius = DEFAULT_WORLD_OBJECT_SIZE;
            }

            if (minfo->combat_reach != 1.5f)
            {
                sLog.outErrorDb("Table `creature_model_info` have wrong combat_reach %f for character race %u male model id %u, expected always 1.5f", minfo->combat_reach, race, raceEntry->model_m);
                const_cast<CreatureModelInfo*>(minfo)->combat_reach = 1.5f;
            }
        }
        else
        {
            sLog.outErrorDb("Table `creature_model_info` expect have data for character race %u male model id %u", race, raceEntry->model_m);
        }
    }

    sLog.outString(">> Loaded %u creature model based info", sCreatureModelStorage.GetRecordCount());
    sLog.outString();
}

void ObjectMgr::LoadCreatures()
{
    uint32 count = 0;
    //                                                0                       1   2    3
    QueryResult* result = WorldDatabase.Query("SELECT `creature`.`guid`, `creature`.`id`, `map`, `modelid`,"
                          //   4             5           6           7           8            9              10         11
                          "`equipment_id`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `spawndist`, `currentwaypoint`,"
                          //   12         13       14          15            16
                          "`curhealth`, `curmana`, `DeathState`, `MovementType`, `event`,"
                          //   17                        18
                          "`pool_creature`.`pool_entry`, `pool_creature_template`.`pool_entry` "
                          "FROM `creature` "
                          "LEFT OUTER JOIN `game_event_creature` ON `creature`.`guid` = `game_event_creature`.`guid` "
                          "LEFT OUTER JOIN `pool_creature` ON `creature`.`guid` = `pool_creature`.`guid` "
                          "LEFT OUTER JOIN `pool_creature_template` ON `creature`.`id` = `pool_creature_template`.`id`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 creature. DB table `creature` is empty.");
        sLog.outString();
        return;
    }

    // build single time for check creature data

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 guid         = fields[ 0].GetUInt32();
        uint32 entry        = fields[ 1].GetUInt32();

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_CREATURE_SPAWN, entry, NULL, 0, guid))
        {
            sLog.outDebug("Creature guid %u (entry %u) spawning is disabled.", guid, entry);
            continue;
        }

        CreatureInfo const* cInfo = GetCreatureTemplate(entry);
        if (!cInfo)
        {
            sLog.outErrorDb("Table `creature` has creature (GUID: %u) with non existing creature entry %u, skipped.", guid, entry);
            continue;
        }

        CreatureData& data = mCreatureDataMap[guid];

        data.id                 = entry;
        data.mapid              = fields[ 2].GetUInt32();
        data.modelid_override   = fields[ 3].GetUInt32();
        data.equipmentId        = fields[ 4].GetUInt32();
        data.posX               = fields[ 5].GetFloat();
        data.posY               = fields[ 6].GetFloat();
        data.posZ               = fields[ 7].GetFloat();
        data.orientation        = fields[ 8].GetFloat();
        data.spawntimesecs      = fields[ 9].GetUInt32();
        data.spawndist          = fields[10].GetFloat();
        data.currentwaypoint    = fields[11].GetUInt32();
        data.curhealth          = fields[12].GetUInt32();
        data.curmana            = fields[13].GetUInt32();
        data.is_dead            = fields[14].GetBool();
        data.movementType       = fields[15].GetUInt8();
        int16 gameEvent         = fields[16].GetInt16();
        int16 GuidPoolId        = fields[17].GetInt16();
        int16 EntryPoolId       = fields[18].GetInt16();

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.mapid);
        if (!mapEntry)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u) that spawned at nonexistent map (Id: %u), skipped.", guid, data.mapid);
            continue;
        }

        if (data.modelid_override > 0 && !sCreatureDisplayInfoStore.LookupEntry(data.modelid_override))
        {
            sLog.outErrorDb("Table `creature` GUID %u (entry %u) has model for nonexistent model id (%u), set to 0.", guid, data.id, data.modelid_override);
            data.modelid_override = 0;
        }

        if (data.equipmentId > 0)                           // -1 no equipment, 0 use default
        {
            if (!GetEquipmentInfo(data.equipmentId) && !GetEquipmentInfoRaw(data.equipmentId))
            {
                sLog.outErrorDb("Table `creature` have creature (Entry: %u) with equipment_id %u not found in table `creature_equip_template` or `creature_equip_template_raw`, set to no equipment.", data.id, data.equipmentId);
                data.equipmentId = -1;
            }
        }

        if (cInfo->RegenerateStats & REGEN_FLAG_HEALTH && data.curhealth < cInfo->MinLevelHealth)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `creature_template`.`RegenerateStats` & REGEN_FLAG_HEALTH and low current health (%u), `creature_template`.`MinLevelHealth`=%u.", guid, data.id, data.curhealth, cInfo->MinLevelHealth);
            data.curhealth = cInfo->MinLevelHealth;
        }

        if (cInfo->ExtraFlags & CREATURE_EXTRA_FLAG_INSTANCE_BIND)
        {
            if (!mapEntry || !mapEntry->IsDungeon())
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `creature_template`.`ExtraFlags` including CREATURE_FLAG_EXTRA_INSTANCE_BIND (%u) but creature are not in instance.",
                                guid, data.id, CREATURE_EXTRA_FLAG_INSTANCE_BIND);
        }

        if (cInfo->ExtraFlags & CREATURE_EXTRA_FLAG_AGGRO_ZONE)
        {
            if (!mapEntry || !mapEntry->IsDungeon())
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `creature_template`.`ExtraFlags` including CREATURE_FLAG_EXTRA_AGGRO_ZONE (%u) but creature are not in instance.",
                                guid, data.id, CREATURE_EXTRA_FLAG_AGGRO_ZONE);
        }

        if (data.curmana < cInfo->MinLevelMana)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with low current mana (%u), `creature_template`.`MinLevelMana`=%u.", guid, data.id, data.curmana, cInfo->MinLevelMana);
            data.curmana = cInfo->MinLevelMana;
        }

        if (data.spawndist < 0.0f)
        {
            sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `spawndist`< 0, set to 0.", guid, data.id);
            data.spawndist = 0.0f;
        }
        else if (data.movementType == RANDOM_MOTION_TYPE)
        {
            if (data.spawndist == 0.0f)
            {
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `MovementType`=1 (random movement) but with `spawndist`=0, replace by idle movement type (0).", guid, data.id);
                data.movementType = IDLE_MOTION_TYPE;
            }
        }
        else if (data.movementType == IDLE_MOTION_TYPE)
        {
            if (data.spawndist != 0.0f)
            {
                sLog.outErrorDb("Table `creature` have creature (GUID: %u Entry: %u) with `MovementType`=0 (idle) have `spawndist`<>0, set to 0.", guid, data.id);
                data.spawndist = 0.0f;
            }
        }

        if (gameEvent == 0 && GuidPoolId == 0 && EntryPoolId == 0) // if not this is to be managed by GameEvent System or Pool system
        {
            AddCreatureToGrid(guid, &data);
        }

        if (cInfo->ExtraFlags & CREATURE_EXTRA_FLAG_ACTIVE)
        {
            m_activeCreatures.insert(ActiveCreatureGuidsOnMap::value_type(data.mapid, guid));
        }

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " creatures", mCreatureDataMap.size());
    sLog.outString();
}

void ObjectMgr::AddCreatureToGrid(uint32 guid, CreatureData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids& cell_guids = mMapObjectGuids[data->mapid][cell_id];
    cell_guids.creatures.insert(guid);
}

void ObjectMgr::RemoveCreatureFromGrid(uint32 guid, CreatureData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids& cell_guids = mMapObjectGuids[data->mapid][cell_id];
    cell_guids.creatures.erase(guid);
}

void ObjectMgr::LoadGameObjects()
{
    //                                                           0                1              2               3                      4                      5                      6
    QueryResult* result = WorldDatabase.Query("SELECT `gameobject`.`guid`, `gameobject`.`id`, `gameobject`.`map`, `gameobject`.`position_x`, `gameobject`.`position_y`, `gameobject`.`position_z`, `gameobject`.`orientation`, "
                          //          7                     8                     9                     10                    11                        12                       13
                          "`gameobject`.`rotation0`, `gameobject`.`rotation1`, `gameobject`.`rotation2`, `gameobject`.`rotation3`, `gameobject`.`spawntimesecs`, `gameobject`.`animprogress`, `gameobject`.`state`, "
                          //                      14                      15                                   16
                          "`game_event_gameobject`.`event`, `pool_gameobject`.`pool_entry`, `pool_gameobject_template`.`pool_entry` "
                          "FROM `gameobject` "
                          "LEFT OUTER JOIN `game_event_gameobject` ON `gameobject`.`guid` = `game_event_gameobject`.`guid` "
                          "LEFT OUTER JOIN `pool_gameobject` ON `gameobject`.`guid` = `pool_gameobject`.`guid` "
                          "LEFT OUTER JOIN `pool_gameobject_template` ON `gameobject`.`id` = `pool_gameobject_template`.`id`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 gameobjects. DB table `gameobject` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 local_transports = 0;
    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 guid         = fields[ 0].GetUInt32();
        uint32 entry        = fields[ 1].GetUInt32();

        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_GAMEOBJECT_SPAWN, entry, NULL, 0, guid))
        {
            sLog.outDebug("Gameobject guid %u (entry %u) spawning is disabled.", guid, entry);
            continue;
        }

        GameObjectInfo const* gInfo = GetGameObjectInfo(entry);
        if (!gInfo)
        {
            sLog.outErrorDb("Table `gameobject` has gameobject (GUID: %u) with non existing gameobject entry %u, skipped.", guid, entry);
            continue;
        }

        if (gInfo->displayId && !sGameObjectDisplayInfoStore.LookupEntry(gInfo->displayId))
        {
            sLog.outErrorDb("Gameobject (GUID: %u Entry %u GoType: %u) have invalid displayId (%u), not loaded.", guid, entry, gInfo->type, gInfo->displayId);
            continue;
        }

        GameObjectData& data = mGameObjectDataMap[guid];

        data.id             = entry;
        data.mapid          = fields[ 2].GetUInt32();
        data.posX           = fields[ 3].GetFloat();
        data.posY           = fields[ 4].GetFloat();
        data.posZ           = fields[ 5].GetFloat();
        data.orientation    = fields[ 6].GetFloat();
        data.rotation0      = fields[ 7].GetFloat();
        data.rotation1      = fields[ 8].GetFloat();
        data.rotation2      = fields[ 9].GetFloat();
        data.rotation3      = fields[10].GetFloat();
        data.spawntimesecs  = fields[11].GetInt32();
        data.animprogress   = fields[12].GetUInt32();
        uint32 go_state     = fields[13].GetUInt32();
        int16 gameEvent     = fields[14].GetInt16();
        int16 GuidPoolId    = fields[15].GetInt16();
        int16 EntryPoolId   = fields[16].GetInt16();

        MapEntry const* mapEntry = sMapStore.LookupEntry(data.mapid);
        if (!mapEntry)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) that spawned at nonexistent map (Id: %u), skip", guid, data.id, data.mapid);
            continue;
        }

        if (data.spawntimesecs == 0 && gInfo->IsDespawnAtAction())
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with `spawntimesecs` (0) value, but gameobejct marked as despawnable at action.", guid, data.id);
        }

        if (go_state >= MAX_GO_STATE)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid `state` (%u) value, skip", guid, data.id, go_state);
            continue;
        }
        data.go_state       = GOState(go_state);

        if (data.rotation0 < -1.0f || data.rotation0 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation0 (%f) value, skip", guid, data.id, data.rotation0);
            continue;
        }

        if (data.rotation1 < -1.0f || data.rotation1 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation1 (%f) value, skip", guid, data.id, data.rotation1);
            continue;
        }

        if (data.rotation2 < -1.0f || data.rotation2 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation2 (%f) value, skip", guid, data.id, data.rotation2);
            continue;
        }

        if (data.rotation3 < -1.0f || data.rotation3 > 1.0f)
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid rotation3 (%f) value, skip", guid, data.id, data.rotation3);
            continue;
        }

        if (!MapManager::IsValidMapCoord(data.mapid, data.posX, data.posY, data.posZ, data.orientation))
        {
            sLog.outErrorDb("Table `gameobject` have gameobject (GUID: %u Entry: %u) with invalid coordinates, skip", guid, data.id);
            continue;
        }

        if (gInfo->type != GAMEOBJECT_TYPE_TRANSPORT && gameEvent == 0 && GuidPoolId == 0 && EntryPoolId == 0) // if not this is to be managed by GameEvent System or Pool system
        {
            AddGameobjectToGrid(guid, &data);
        }

        if (gInfo->type == GAMEOBJECT_TYPE_TRANSPORT)
        {
            m_localTransports.insert(LocalTransportGuidsOnMap::value_type(data.mapid, guid));
            ++local_transports;
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded " SIZEFMTD " gameobjects", mGameObjectDataMap.size());
    sLog.outString(">>> Loaded %u local transport objects", local_transports);
}

void ObjectMgr::AddGameobjectToGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids& cell_guids = mMapObjectGuids[data->mapid][cell_id];
    cell_guids.gameobjects.insert(guid);
}

void ObjectMgr::RemoveGameobjectFromGrid(uint32 guid, GameObjectData const* data)
{
    CellPair cell_pair = MaNGOS::ComputeCellPair(data->posX, data->posY);
    uint32 cell_id = (cell_pair.y_coord * TOTAL_NUMBER_OF_CELLS_PER_MAP) + cell_pair.x_coord;

    CellObjectGuids& cell_guids = mMapObjectGuids[data->mapid][cell_id];
    cell_guids.gameobjects.erase(guid);
}

// name must be checked to correctness (if received) before call this function
ObjectGuid ObjectMgr::GetPlayerGuidByName(std::string name) const
{
    ObjectGuid guid;

    CharacterDatabase.escape_string(name);

    // Player name safe to sending to DB (checked at login) and this function using
    QueryResult* result = CharacterDatabase.PQuery("SELECT `guid` FROM `characters` WHERE `name` = '%s'", name.c_str());
    if (result)
    {
        guid = ObjectGuid(HIGHGUID_PLAYER, (*result)[0].GetUInt32());

        delete result;
    }

    return guid;
}

bool ObjectMgr::GetPlayerNameByGUID(ObjectGuid guid, std::string& name) const
{
    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
    {
        name = player->GetName();
        return true;
    }

    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `name` FROM `characters` WHERE `guid` = '%u'", lowguid);

    if (result)
    {
        name = (*result)[0].GetCppString();
        delete result;
        return true;
    }

    return false;
}

Team ObjectMgr::GetPlayerTeamByGUID(ObjectGuid guid) const
{
    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
    {
        return Player::TeamForRace(player->getRace());
    }

    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `race` FROM `characters` WHERE `guid` = '%u'", lowguid);

    if (result)
    {
        uint8 race = (*result)[0].GetUInt8();
        delete result;
        return Player::TeamForRace(race);
    }

    return TEAM_NONE;
}

uint8 ObjectMgr::GetPlayerClassByGUID(ObjectGuid guid) const
{
    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
    {
        return player->getClass();
    }

    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `class` FROM `characters` WHERE `guid` = '%u'", lowguid);

    if (result)
    {
        uint8 pClass = (*result)[0].GetUInt8();
        delete result;
        return pClass;
    }

    return 0;
}

uint32 ObjectMgr::GetPlayerAccountIdByGUID(ObjectGuid guid) const
{
    if (!guid.IsPlayer())
    {
        return 0;
    }

    // prevent DB access for online player
    if (Player* player = GetPlayer(guid))
    {
        return player->GetSession()->GetAccountId();
    }

    uint32 lowguid = guid.GetCounter();

    QueryResult* result = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `guid` = '%u'", lowguid);
    if (result)
    {
        uint32 acc = (*result)[0].GetUInt32();
        delete result;
        return acc;
    }

    return 0;
}

uint32 ObjectMgr::GetPlayerAccountIdByPlayerName(const std::string& name) const
{
    QueryResult* result = CharacterDatabase.PQuery("SELECT `account` FROM `characters` WHERE `name` = '%s'", name.c_str());
    if (result)
    {
        uint32 acc = (*result)[0].GetUInt32();
        delete result;
        return acc;
    }

    return 0;
}

void ObjectMgr::LoadItemLocales()
{
    mItemLocaleMap.clear();                                 // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`name_loc1`,`description_loc1`,`name_loc2`,`description_loc2`,`name_loc3`,`description_loc3`,`name_loc4`,`description_loc4`,`name_loc5`,`description_loc5`,`name_loc6`,`description_loc6`,`name_loc7`,`description_loc7`,`name_loc8`,`description_loc8` FROM `locales_item`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 Item locale strings. DB table `locales_item` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetItemPrototype(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_item` has data for nonexistent item entry %u, skipped.", entry);
            continue;
        }

        ItemLocale& data = mItemLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 2 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }

            str = fields[1 + 2 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Description.size() <= idx)
                    {
                        data.Description.resize(idx + 1);
                    }

                    data.Description[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " Item locale strings", mItemLocaleMap.size());
    sLog.outString();
}

struct SQLItemLoader : public SQLStorageLoaderBase<SQLItemLoader, SQLStorage>
{
    template<class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

void ObjectMgr::LoadItemPrototypes()
{
    SQLItemLoader loader;
    loader.Load(sItemStorage);

    // check data correctness
    for (uint32 i = 1; i < sItemStorage.GetMaxEntry(); ++i)
    {
        ItemPrototype const* proto = sItemStorage.LookupEntry<ItemPrototype >(i);
        if (!proto)
        {
            continue;
        }

        if (proto->Class >= MAX_ITEM_CLASS)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Class value (%u)", i, proto->Class);
            const_cast<ItemPrototype*>(proto)->Class = ITEM_CLASS_MISC;
        }

        if (proto->SubClass >= MaxItemSubclassValues[proto->Class])
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Subclass value (%u) for class %u", i, proto->SubClass, proto->Class);
            const_cast<ItemPrototype*>(proto)->SubClass = 0;// exist for all item classes
        }

        if (proto->Quality >= MAX_ITEM_QUALITY)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Quality value (%u)", i, proto->Quality);
            const_cast<ItemPrototype*>(proto)->Quality = ITEM_QUALITY_NORMAL;
        }

        if (proto->BuyCount <= 0)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong BuyCount value (%u), set to default(1).", i, proto->BuyCount);
            const_cast<ItemPrototype*>(proto)->BuyCount = 1;
        }

        if (proto->InventoryType >= MAX_INVTYPE)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong InventoryType value (%u)", i, proto->InventoryType);
            const_cast<ItemPrototype*>(proto)->InventoryType = INVTYPE_NON_EQUIP;
        }

        if (proto->InventoryType != INVTYPE_NON_EQUIP)
        {
            if (proto->Flags & ITEM_FLAG_LOOTABLE)
            {
                sLog.outErrorDb("Item container (Entry: %u) has not allowed for containers flag ITEM_FLAG_LOOTABLE (%u), flag removed.", i, ITEM_FLAG_LOOTABLE);
                const_cast<ItemPrototype*>(proto)->Flags &= ~ITEM_FLAG_LOOTABLE;
            }
        }
        else if (proto->InventoryType != INVTYPE_BAG)
        {
            if (proto->ContainerSlots > 0)
            {
                sLog.outErrorDb("Non-container item (Entry: %u) has ContainerSlots (%u), set to 0.", i, proto->ContainerSlots);
                const_cast<ItemPrototype*>(proto)->ContainerSlots = 0;
            }
        }

        if (proto->RequiredSkill >= MAX_SKILL_TYPE)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong RequiredSkill value (%u)", i, proto->RequiredSkill);
            const_cast<ItemPrototype*>(proto)->RequiredSkill = 0;
        }

        {
            // can be used in equip slot, as page read use in inventory, or spell casting at use
            bool req = proto->InventoryType != INVTYPE_NON_EQUIP || proto->PageText;
            if (!req)
            {
                for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
                {
                    if (proto->Spells[j].SpellId)
                    {
                        req = true;
                        break;
                    }
                }
            }

            if (req)
            {
                if (!(proto->AllowableClass & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Item (Entry: %u) not have in `AllowableClass` any playable classes (%u) and can't be equipped or use.", i, proto->AllowableClass);
                }

                if (!(proto->AllowableRace & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Item (Entry: %u) not have in `AllowableRace` any playable races (%u) and can't be equipped or use.", i, proto->AllowableRace);
                }
            }
        }

        if (proto->RequiredSpell && !sSpellStore.LookupEntry(proto->RequiredSpell))
        {
            sLog.outErrorDb("Item (Entry: %u) have wrong (nonexistent) spell in RequiredSpell (%u)", i, proto->RequiredSpell);
            const_cast<ItemPrototype*>(proto)->RequiredSpell = 0;
        }

        if (proto->RequiredReputationRank >= MAX_REPUTATION_RANK)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong reputation rank in RequiredReputationRank (%u), item can't be used.", i, proto->RequiredReputationRank);
        }

        if (proto->RequiredReputationFaction)
        {
            if (!sFactionStore.LookupEntry(proto->RequiredReputationFaction))
            {
                sLog.outErrorDb("Item (Entry: %u) has wrong (not existing) faction in RequiredReputationFaction (%u)", i, proto->RequiredReputationFaction);
                const_cast<ItemPrototype*>(proto)->RequiredReputationFaction = 0;
            }

            if (proto->RequiredReputationRank == MIN_REPUTATION_RANK)
            {
                sLog.outErrorDb("Item (Entry: %u) has min. reputation rank in RequiredReputationRank (0) but RequiredReputationFaction > 0, faction setting is useless.", i);
            }
        }

        if (proto->Stackable == 0)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong value in stackable (%u), replace by default 1.", i, proto->Stackable);
            const_cast<ItemPrototype*>(proto)->Stackable = 1;
        }
        else if (proto->Stackable > 255)
        {
            sLog.outErrorDb("Item (Entry: %u) has too large value in stackable (%u), replace by hardcoded upper limit (255).", i, proto->Stackable);
            const_cast<ItemPrototype*>(proto)->Stackable = 255;
        }

        if (proto->ContainerSlots)
        {
            if (proto->ContainerSlots > MAX_BAG_SIZE)
            {
                sLog.outErrorDb("Item (Entry: %u) has too large value in ContainerSlots (%u), replace by hardcoded limit (%u).", i, proto->ContainerSlots, MAX_BAG_SIZE);
                const_cast<ItemPrototype*>(proto)->ContainerSlots = MAX_BAG_SIZE;
            }
        }

        for (int j = 0; j < MAX_ITEM_PROTO_STATS; ++j)
        {
            // for ItemStatValue != 0
            if (proto->ItemStat[j].ItemStatValue && proto->ItemStat[j].ItemStatType >= MAX_ITEM_MOD)
            {
                sLog.outErrorDb("Item (Entry: %u) has wrong stat_type%d (%u)", i, j + 1, proto->ItemStat[j].ItemStatType);
                const_cast<ItemPrototype*>(proto)->ItemStat[j].ItemStatType = 0;
            }
        }

        for (int j = 0; j < MAX_ITEM_PROTO_DAMAGES; ++j)
        {
            if (proto->Damage[j].DamageType >= MAX_SPELL_SCHOOL)
            {
                sLog.outErrorDb("Item (Entry: %u) has wrong dmg_type%d (%u)", i, j + 1, proto->Damage[j].DamageType);
                const_cast<ItemPrototype*>(proto)->Damage[j].DamageType = 0;
            }
        }

        // normal spell list
        {
            for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
            {
                if (DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, proto->Spells[j].SpellId))
                {
                    DEBUG_LOG("Spell %u on item %u (%s) is disabled.", proto->Spells[j].SpellId, proto->ItemId, proto->Name1);
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellId = 0;
                    continue;
                }

                if (proto->Spells[j].SpellTrigger >= MAX_ITEM_SPELLTRIGGER)
                {
                    sLog.outErrorDb("Item (Entry: %u) has wrong item spell trigger value in spelltrigger_%d (%u)", i, j + 1, proto->Spells[j].SpellTrigger);
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellId = 0;
                    const_cast<ItemPrototype*>(proto)->Spells[j].SpellTrigger = ITEM_SPELLTRIGGER_ON_USE;
                }
                // on hit can be sued only at weapon
                else if (proto->Spells[j].SpellTrigger == ITEM_SPELLTRIGGER_CHANCE_ON_HIT)
                {
                    if (proto->Class != ITEM_CLASS_WEAPON)
                    {
                        sLog.outErrorDb("Item (Entry: %u) isn't weapon (Class: %u) but has on hit spelltrigger_%d (%u), it will not triggered.", i, proto->Class, j + 1, proto->Spells[j].SpellTrigger);
                    }
                }

                if (proto->Spells[j].SpellId)
                {
                    SpellEntry const* spellInfo = sSpellStore.LookupEntry(proto->Spells[j].SpellId);
                    if (!spellInfo)
                    {
                        sLog.outErrorDb("Item (Entry: %u) has wrong (not existing) spell in spellid_%d (%u)", i, j + 1, proto->Spells[j].SpellId);
                        const_cast<ItemPrototype*>(proto)->Spells[j].SpellId = 0;
                    }
                }
            }
        }

        if (proto->Bonding >= MAX_BIND_TYPE)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Bonding value (%u)", i, proto->Bonding);
        }

        if (proto->PageText)
        {
            if (!sPageTextStore.LookupEntry<PageText>(proto->PageText))
            {
                sLog.outErrorDb("Item (Entry: %u) has non existing first page (Id:%u)", i, proto->PageText);
            }
        }

        if (proto->LockID && !sLockStore.LookupEntry(proto->LockID))
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong LockID (%u)", i, proto->LockID);
        }

        if (proto->Sheath >= MAX_SHEATHETYPE)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Sheath (%u)", i, proto->Sheath);
            const_cast<ItemPrototype*>(proto)->Sheath = SHEATHETYPE_NONE;
        }

        if (proto->RandomProperty && !sItemRandomPropertiesStore.LookupEntry(GetItemEnchantMod(proto->RandomProperty)))
        {
            sLog.outErrorDb("Item (Entry: %u) has unknown (wrong or not listed in `item_enchantment_template`) RandomProperty (%u)", i, proto->RandomProperty);
            const_cast<ItemPrototype*>(proto)->RandomProperty = 0;
        }

        if (proto->ItemSet && !sItemSetStore.LookupEntry(proto->ItemSet))
        {
            sLog.outErrorDb("Item (Entry: %u) have wrong ItemSet (%u)", i, proto->ItemSet);
            const_cast<ItemPrototype*>(proto)->ItemSet = 0;
        }

        if (proto->Area && !GetAreaEntryByAreaID(proto->Area))
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Area (%u)", i, proto->Area);
        }

        if (proto->Map && !sMapStore.LookupEntry(proto->Map))
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong Map (%u)", i, proto->Map);
        }

        if (proto->BagFamily)
        {
            ItemBagFamilyEntry const* bf = sItemBagFamilyStore.LookupEntry(proto->BagFamily);
            if (!bf)
            {
                sLog.outErrorDb("Item (Entry: %u) has bag family %u not listed in ItemBagFamily.dbc, setted it to 0", i, proto->BagFamily);
                const_cast<ItemPrototype*>(proto)->BagFamily = 0;
            }
        }

        if (proto->DisenchantID)
        {
            if (proto->Quality > ITEM_QUALITY_EPIC || proto->Quality < ITEM_QUALITY_UNCOMMON)
            {
                sLog.outErrorDb("Item (Entry: %u) has wrong quality (%u) for disenchanting, remove disenchanting loot id.", i, proto->Quality);
                const_cast<ItemPrototype*>(proto)->DisenchantID = 0;
            }
            else if (proto->Class != ITEM_CLASS_WEAPON && proto->Class != ITEM_CLASS_ARMOR)
            {
                sLog.outErrorDb("Item (Entry: %u) has wrong item class (%u) for disenchanting, remove disenchanting loot id.", i, proto->Class);
                const_cast<ItemPrototype*>(proto)->DisenchantID = 0;
            }
        }

        if (proto->FoodType >= MAX_PET_DIET)
        {
            sLog.outErrorDb("Item (Entry: %u) has wrong FoodType value (%u)", i, proto->FoodType);
            const_cast<ItemPrototype*>(proto)->FoodType = 0;
        }

        if (proto->ExtraFlags)
        {
            if (proto->ExtraFlags & ~ITEM_EXTRA_ALL)
            {
                sLog.outErrorDb("Item (Entry: %u) has wrong ExtraFlags (%u) with unused bits set", i, proto->ExtraFlags);
            }

            if (proto->ExtraFlags & ITEM_EXTRA_NON_CONSUMABLE)
            {
                bool can_be_need = false;
                for (int j = 0; j < MAX_ITEM_PROTO_SPELLS; ++j)
                {
                    if (proto->Spells[j].SpellCharges < 0)
                    {
                        can_be_need = true;
                        break;
                    }
                }

                if (!can_be_need)
                {
                    sLog.outErrorDb("Item (Entry: %u) has redundant non-consumable flag in ExtraFlags, item not have negative charges", i);
                    const_cast<ItemPrototype*>(proto)->ExtraFlags &= ~ITEM_EXTRA_NON_CONSUMABLE;
                }
            }

            if (proto->ExtraFlags & ITEM_EXTRA_REAL_TIME_DURATION)
            {
                if (proto->Duration == 0)
                {
                    sLog.outErrorDb("Item (Entry: %u) has redundant real-time duration flag in ExtraFlags, item not have duration", i);
                    const_cast<ItemPrototype*>(proto)->ExtraFlags &= ~ITEM_EXTRA_REAL_TIME_DURATION;
                }
            }
        }
    }

    // check some dbc referenced items (avoid duplicate reports)
    std::set<uint32> notFoundOutfit;
    for (uint32 i = 1; i < sCharStartOutfitStore.GetNumRows(); ++i)
    {
        CharStartOutfitEntry const* entry = sCharStartOutfitStore.LookupEntry(i);
        if (!entry)
        {
            continue;
        }

        for (int j = 0; j < MAX_OUTFIT_ITEMS; ++j)
        {
            if (entry->ItemId[j] <= 0)
            {
                continue;
            }

            uint32 item_id = entry->ItemId[j];

            if (!GetItemPrototype(item_id))
            {
                notFoundOutfit.insert(item_id);
            }
        }
    }

    for (std::set<uint32>::const_iterator itr = notFoundOutfit.begin(); itr != notFoundOutfit.end(); ++itr)
    {
        sLog.outErrorDb("Item (Entry: %u) not exist in `item_template` but referenced in `CharStartOutfit.dbc`", *itr);
    }

    sLog.outString(">> Loaded %u item prototypes", sItemStorage.GetRecordCount());
    sLog.outString();
}

void ObjectMgr::LoadItemRequiredTarget()
{
    m_ItemRequiredTarget.clear();                           // needed for reload case

    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`type`,`targetEntry` FROM `item_required_target`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 ItemRequiredTarget. DB table `item_required_target` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 uiItemId      = fields[0].GetUInt32();
        uint32 uiType        = fields[1].GetUInt32();
        uint32 uiTargetEntry = fields[2].GetUInt32();

        ItemPrototype const* pItemProto = sItemStorage.LookupEntry<ItemPrototype>(uiItemId);

        if (!pItemProto)
        {
            sLog.outErrorDb("Table `item_required_target`: Entry %u listed for TargetEntry %u does not exist in `item_template`.", uiItemId, uiTargetEntry);
            continue;
        }

        bool bIsItemSpellValid = false;

        for (int i = 0; i < MAX_ITEM_PROTO_SPELLS; ++i)
        {
            if (SpellEntry const* pSpellInfo = sSpellStore.LookupEntry(pItemProto->Spells[i].SpellId))
            {
                if (pItemProto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE ||
                    pItemProto->Spells[i].SpellTrigger == ITEM_SPELLTRIGGER_ON_NO_DELAY_USE)
                {
                    SQLMultiStorage::SQLMSIteratorBounds<SpellTargetEntry> bounds = sSpellScriptTargetStorage.getBounds<SpellTargetEntry>(pSpellInfo->Id);
                    if (bounds.first != bounds.second)
                    {
                        break;
                    }

                    for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
                    {
                        if (pSpellInfo->EffectImplicitTargetA[j] == TARGET_CHAIN_DAMAGE ||
                            pSpellInfo->EffectImplicitTargetB[j] == TARGET_CHAIN_DAMAGE ||
                            pSpellInfo->EffectImplicitTargetA[j] == TARGET_DUELVSPLAYER ||
                            pSpellInfo->EffectImplicitTargetB[j] == TARGET_DUELVSPLAYER)
                        {
                            bIsItemSpellValid = true;
                            break;
                        }
                    }
                    if (bIsItemSpellValid)
                    {
                        break;
                    }
                }
            }
        }

        if (!bIsItemSpellValid)
        {
            sLog.outErrorDb("Table `item_required_target`: Spell used by item %u does not have implicit target TARGET_CHAIN_DAMAGE(6), TARGET_DUELVSPLAYER(25), already listed in `spell_script_target` or doesn't have item spelltrigger.", uiItemId);
            continue;
        }

        if (!uiType || uiType > MAX_ITEM_REQ_TARGET_TYPE)
        {
            sLog.outErrorDb("Table `item_required_target`: Type %u for TargetEntry %u is incorrect.", uiType, uiTargetEntry);
            continue;
        }

        if (!uiTargetEntry)
        {
            sLog.outErrorDb("Table `item_required_target`: TargetEntry == 0 for Type (%u).", uiType);
            continue;
        }

        if (!sCreatureStorage.LookupEntry<CreatureInfo>(uiTargetEntry))
        {
            sLog.outErrorDb("Table `item_required_target`: creature template entry %u does not exist.", uiTargetEntry);
            continue;
        }

        m_ItemRequiredTarget.insert(ItemRequiredTargetMap::value_type(uiItemId, ItemRequiredTarget(ItemRequiredTargetType(uiType), uiTargetEntry)));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u Item required targets", count);
    sLog.outString();
}

void ObjectMgr::LoadPetLevelInfo()
{
    uint32 count = 0;

    // Loading levels data
    {
        //                                                 0               1      2   3     4    5    6    7     8    9
        QueryResult* result  = WorldDatabase.Query("SELECT `creature_entry`, `level`, `hp`, `mana`, `str`, `agi`, `sta`, `inte`, `spi`, `armor` FROM `pet_levelstats`");


        if (!result)
        {
            BarGoLink bar(1);
            bar.step();
            sLog.outString(">> Loaded %u level pet stats definitions", count);
            sLog.outErrorDb("Error loading `pet_levelstats` table or empty table.");
            sLog.outString();
            return;
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 creature_id = fields[0].GetUInt32();
            if (!sCreatureStorage.LookupEntry<CreatureInfo>(creature_id))
            {
                sLog.outErrorDb("Wrong creature id %u in `pet_levelstats` table, ignoring.", creature_id);
                continue;
            }

            uint32 current_level = fields[1].GetUInt32();
            if (current_level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `pet_levelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_LOG("Unused (> MaxPlayerLevel in mangosd.conf) level %u in `pet_levelstats` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }
            else if (current_level < 1)
            {
                sLog.outErrorDb("Wrong (<1) level %u in `pet_levelstats` table, ignoring.", current_level);
                continue;
            }

            PetLevelInfo*& pInfoMapEntry = petInfo[creature_id];

            if (pInfoMapEntry == NULL)
            {
                pInfoMapEntry =  new PetLevelInfo[sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)];
            }

            // data for level 1 stored in [0] array element, ...
            PetLevelInfo* pLevelInfo = &pInfoMapEntry[current_level - 1];

            pLevelInfo->health = fields[2].GetUInt16();
            pLevelInfo->mana   = fields[3].GetUInt16();
            pLevelInfo->armor  = fields[9].GetUInt16();

            for (int i = 0; i < MAX_STATS; ++i)
            {
                pLevelInfo->stats[i] = fields[i + 4].GetUInt16();
            }

            bar.step();
            ++count;
        }
        while (result->NextRow());

        delete result;
    }

    // Fill gaps and check integrity
    for (PetLevelInfoMap::iterator itr = petInfo.begin(); itr != petInfo.end(); ++itr)
    {
        PetLevelInfo* pInfo = itr->second;

        // fatal error if no level 1 data
        if (!pInfo || pInfo[0].health == 0)
        {
            sLog.outErrorDb("Creature %u does not have pet stats data for Level 1!", itr->first);
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        // fill level gaps
        for (uint32 level = 1; level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
        {
            if (pInfo[level].health == 0)
            {
                sLog.outErrorDb("Creature %u has no data for Level %i pet stats data, using data of Level %i.", itr->first, level + 1, level);
                pInfo[level] = pInfo[level - 1];
            }
        }
    }

    sLog.outString(">> Loaded %u level pet stats definitions", count);
    sLog.outString();
}

PetLevelInfo const* ObjectMgr::GetPetLevelInfo(uint32 creature_id, uint32 level) const
{
    if (level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        level = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    }

    PetLevelInfoMap::const_iterator itr = petInfo.find(creature_id);
    if (itr == petInfo.end())
    {
        return NULL;
    }

    return &itr->second[level - 1];                         // data for level 1 stored in [0] array element, ...
}

void ObjectMgr::LoadPlayerInfo()
{
    // Load playercreate
    {
        //                                                0     1      2    3     4           5           6
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `map`, `zone`, `position_x`, `position_y`, `position_z`, `orientation` FROM `playercreateinfo`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u player create definitions", count);
            sLog.outErrorDb("Error loading `playercreateinfo` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_race  = fields[0].GetUInt32();
            uint32 current_class = fields[1].GetUInt32();
            uint32 mapId         = fields[2].GetUInt32();
            uint32 areaId        = fields[3].GetUInt32();
            float  positionX     = fields[4].GetFloat();
            float  positionY     = fields[5].GetFloat();
            float  positionZ     = fields[6].GetFloat();
            float  orientation   = fields[7].GetFloat();

            ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
            if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong race %u in `playercreateinfo` table, ignoring.", current_race);
                continue;
            }

            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
            if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong class %u in `playercreateinfo` table, ignoring.", current_class);
                continue;
            }

            // accept DB data only for valid position (and non instanceable)
            if (!MapManager::IsValidMapCoord(mapId, positionX, positionY, positionZ, orientation))
            {
                sLog.outErrorDb("Wrong home position for class %u race %u pair in `playercreateinfo` table, ignoring.", current_class, current_race);
                continue;
            }

            if (sMapStore.LookupEntry(mapId)->Instanceable())
            {
                sLog.outErrorDb("Home position in instanceable map for class %u race %u pair in `playercreateinfo` table, ignoring.", current_class, current_race);
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[current_race][current_class];

            pInfo->mapId       = mapId;
            pInfo->areaId      = areaId;
            pInfo->positionX   = positionX;
            pInfo->positionY   = positionY;
            pInfo->positionZ   = positionZ;
            pInfo->orientation = orientation;

            pInfo->displayId_m = rEntry->model_m;
            pInfo->displayId_f = rEntry->model_f;

            bar.step();
            ++count;
        }
        while (result->NextRow());

        delete result;

        sLog.outString();
        sLog.outString(">> Loaded %u player create definitions", count);
    }

    // Load playercreate items
    {
        //                                                0     1      2       3
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `itemid`, `amount` FROM `playercreateinfo_item`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            bar.step();

            sLog.outString();
            sLog.outString(">> Loaded %u custom player create items", count);
        }
        else
        {
            BarGoLink bar(result->GetRowCount());

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt32();
                uint32 current_class = fields[1].GetUInt32();

                ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
                if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong race %u in `playercreateinfo_item` table, ignoring.", current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
                if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong class %u in `playercreateinfo_item` table, ignoring.", current_class);
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];

                uint32 item_id = fields[2].GetUInt32();

                if (!GetItemPrototype(item_id))
                {
                    sLog.outErrorDb("Item id %u (race %u class %u) in `playercreateinfo_item` table but not listed in `item_template`, ignoring.", item_id, current_race, current_class);
                    continue;
                }

                uint32 amount  = fields[3].GetUInt32();

                if (!amount)
                {
                    sLog.outErrorDb("Item id %u (class %u race %u) have amount==0 in `playercreateinfo_item` table, ignoring.", item_id, current_race, current_class);
                    continue;
                }

                pInfo->item.push_back(PlayerCreateInfoItem(item_id, amount));

                bar.step();
                ++count;
            }
            while (result->NextRow());

            delete result;

            sLog.outString();
            sLog.outString(">> Loaded %u custom player create items", count);
        }
    }

    // Load playercreate spells
    {
        //                                                0     1      2
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `Spell` FROM `playercreateinfo_spell`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u player create spells", count);
            sLog.outErrorDb("Error loading `playercreateinfo_spell` table or empty table.");
        }
        else
        {
            BarGoLink bar(result->GetRowCount());

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt32();
                uint32 current_class = fields[1].GetUInt32();

                ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
                if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong race %u in `playercreateinfo_spell` table, ignoring.", current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
                if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong class %u in `playercreateinfo_spell` table, ignoring.", current_class);
                    continue;
                }

                uint32 spell_id = fields[2].GetUInt32();
                if (!sSpellStore.LookupEntry(spell_id))
                {
                    sLog.outErrorDb("Non existing spell %u in `playercreateinfo_spell` table, ignoring.", spell_id);
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];
                pInfo->spell.push_back(spell_id);

                bar.step();
                ++count;
            }
            while (result->NextRow());

            delete result;

            sLog.outString();
            sLog.outString(">> Loaded %u player create spells", count);
        }
    }

    // Load playercreate actions
    {
        //                                                0     1      2       3       4
        QueryResult* result = WorldDatabase.Query("SELECT `race`, `class`, `button`, `action`, `type` FROM `playercreateinfo_action`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u player create actions", count);
            sLog.outErrorDb("Error loading `playercreateinfo_action` table or empty table.");
        }
        else
        {
            BarGoLink bar(result->GetRowCount());

            do
            {
                Field* fields = result->Fetch();

                uint32 current_race = fields[0].GetUInt32();
                uint32 current_class = fields[1].GetUInt32();

                ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
                if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong race %u in `playercreateinfo_action` table, ignoring.", current_race);
                    continue;
                }

                ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
                if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
                {
                    sLog.outErrorDb("Wrong class %u in `playercreateinfo_action` table, ignoring.", current_class);
                    continue;
                }

                uint8 action_button  = fields[2].GetUInt8();
                uint32 action = fields[3].GetUInt32();
                uint8 action_type = fields[4].GetUInt8();

                if (!Player::IsActionButtonDataValid(action_button, action, action_type, NULL))
                {
                    continue;
                }

                PlayerInfo* pInfo = &playerInfo[current_race][current_class];
                pInfo->action.push_back(PlayerCreateInfoAction(action_button, action, action_type));

                bar.step();
                ++count;
            }
            while (result->NextRow());

            delete result;

            sLog.outString();
            sLog.outString(">> Loaded %u player create actions", count);
        }
    }

    // Loading levels data (class only dependent)
    {
        //                                                 0      1      2       3
        QueryResult* result  = WorldDatabase.Query("SELECT `class`, `level`, `basehp`, `basemana` FROM `player_classlevelstats`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u level health/mana definitions", count);
            sLog.outErrorDb("Error loading `player_classlevelstats` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_class = fields[0].GetUInt32();
            if (current_class >= MAX_CLASSES)
            {
                sLog.outErrorDb("Wrong class %u in `player_classlevelstats` table, ignoring.", current_class);
                continue;
            }

            uint32 current_level = fields[1].GetUInt32();
            if (current_level == 0)
            {
                sLog.outErrorDb("Wrong level %u in `player_classlevelstats` table, ignoring.", current_level);
                continue;
            }
            else if (current_level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `player_classlevelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_LOG("Unused (> MaxPlayerLevel in mangosd.conf) level %u in `player_classlevelstats` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }

            PlayerClassInfo* pClassInfo = &playerClassInfo[current_class];

            if (!pClassInfo->levelInfo)
            {
                pClassInfo->levelInfo = new PlayerClassLevelInfo[sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)];
            }

            PlayerClassLevelInfo* pClassLevelInfo = &pClassInfo->levelInfo[current_level - 1];

            pClassLevelInfo->basehealth = fields[2].GetUInt16();
            pClassLevelInfo->basemana   = fields[3].GetUInt16();

            bar.step();
            ++count;
        }
        while (result->NextRow());

        delete result;

        sLog.outString();
        sLog.outString(">> Loaded %u level health/mana definitions", count);
    }

    // Fill gaps and check integrity
    for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
    {
        // skip nonexistent classes
        if (!sChrClassesStore.LookupEntry(class_))
        {
            continue;
        }

        PlayerClassInfo* pClassInfo = &playerClassInfo[class_];

        // fatal error if no level 1 data
        if (!pClassInfo->levelInfo || pClassInfo->levelInfo[0].basehealth == 0)
        {
            sLog.outErrorDb("Class %i Level 1 does not have health/mana data!", class_);
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        // fill level gaps
        for (uint32 level = 1; level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
        {
            if (pClassInfo->levelInfo[level].basehealth == 0)
            {
                sLog.outErrorDb("Class %i Level %i does not have health/mana data. Using stats data of level %i.", class_, level + 1, level);
                pClassInfo->levelInfo[level] = pClassInfo->levelInfo[level - 1];
            }
        }
    }

    // Loading levels data (class/race dependent)
    {
        //                                                 0     1      2      3    4    5    6    7
        QueryResult* result  = WorldDatabase.Query("SELECT `race`, `class`, `level`, `str`, `agi`, `sta`, `inte`, `spi` FROM `player_levelstats`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u level stats definitions", count);
            sLog.outErrorDb("Error loading `player_levelstats` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_race = fields[0].GetUInt32();
            uint32 current_class = fields[1].GetUInt32();

            ChrRacesEntry const* rEntry = sChrRacesStore.LookupEntry(current_race);
            if (!rEntry || !((1 << (current_race - 1)) & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong race %u in `player_levelstats` table, ignoring.", current_race);
                continue;
            }

            ChrClassesEntry const* cEntry = sChrClassesStore.LookupEntry(current_class);
            if (!cEntry || !((1 << (current_class - 1)) & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Wrong class %u in `player_levelstats` table, ignoring.", current_class);
                continue;
            }

            uint32 current_level = fields[2].GetUInt32();
            if (current_level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `player_levelstats` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_LOG("Unused (> MaxPlayerLevel in mangosd.conf) level %u in `player_levelstats` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[current_race][current_class];

            if (!pInfo->levelInfo)
            {
                pInfo->levelInfo = new PlayerLevelInfo[sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL)];
            }

            PlayerLevelInfo* pLevelInfo = &pInfo->levelInfo[current_level - 1];

            for (int i = 0; i < MAX_STATS; ++i)
            {
                pLevelInfo->stats[i] = fields[i + 3].GetUInt8();
            }

            bar.step();
            ++count;
        }
        while (result->NextRow());

        delete result;

        sLog.outString();
        sLog.outString(">> Loaded %u level stats definitions", count);
    }

    // Fill gaps and check integrity
    for (int race = 0; race < MAX_RACES; ++race)
    {
        // skip nonexistent races
        if (!((1 << (race - 1)) & RACEMASK_ALL_PLAYABLE) || !sChrRacesStore.LookupEntry(race))
        {
            continue;
        }

        for (int class_ = 0; class_ < MAX_CLASSES; ++class_)
        {
            // skip nonexistent classes
            if (!((1 << (class_ - 1)) & CLASSMASK_ALL_PLAYABLE) || !sChrClassesStore.LookupEntry(class_))
            {
                continue;
            }

            PlayerInfo* pInfo = &playerInfo[race][class_];

            // skip non loaded combinations
            if (!pInfo->displayId_m || !pInfo->displayId_f)
            {
                continue;
            }

            // fatal error if no level 1 data
            if (!pInfo->levelInfo || pInfo->levelInfo[0].stats[0] == 0)
            {
                sLog.outErrorDb("Race %i Class %i Level 1 does not have stats data!", race, class_);
                Log::WaitBeforeContinueIfNeed();
                exit(1);
            }

            // fill level gaps
            for (uint32 level = 1; level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
            {
                if (pInfo->levelInfo[level].stats[0] == 0)
                {
                    sLog.outErrorDb("Race %i Class %i Level %i does not have stats data. Using stats data of level %i.", race, class_, level + 1, level);
                    pInfo->levelInfo[level] = pInfo->levelInfo[level - 1];
                }
            }
        }
    }

    // Loading xp per level data
    {
        mPlayerXPperLevel.resize(sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL));
        for (uint32 level = 0; level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
        {
            mPlayerXPperLevel[level] = 0;
        }

        //                                                 0    1
        QueryResult* result  = WorldDatabase.Query("SELECT `lvl`, `xp_for_next_level` FROM `player_xp_for_level`");

        uint32 count = 0;

        if (!result)
        {
            BarGoLink bar(1);

            sLog.outString();
            sLog.outString(">> Loaded %u xp for level definitions", count);
            sLog.outErrorDb("Error loading `player_xp_for_level` table or empty table.");
            Log::WaitBeforeContinueIfNeed();
            exit(1);
        }

        BarGoLink bar(result->GetRowCount());

        do
        {
            Field* fields = result->Fetch();

            uint32 current_level = fields[0].GetUInt32();
            uint32 current_xp    = fields[1].GetUInt32();

            if (current_level >= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                if (current_level > STRONG_MAX_LEVEL)       // hardcoded level maximum
                {
                    sLog.outErrorDb("Wrong (> %u) level %u in `player_xp_for_level` table, ignoring.", STRONG_MAX_LEVEL, current_level);
                }
                else
                {
                    DETAIL_LOG("Unused (> MaxPlayerLevel in mangosd.conf) level %u in `player_xp_for_levels` table, ignoring.", current_level);
                    ++count;                                // make result loading percent "expected" correct in case disabled detail mode for example.
                }
                continue;
            }
            // PlayerXPperLevel
            mPlayerXPperLevel[current_level] = current_xp;
            bar.step();
            ++count;
        }
        while (result->NextRow());

        delete result;

        sLog.outString();
        sLog.outString(">> Loaded %u xp for level definitions", count);
    }

    // fill level gaps
    for (uint32 level = 1; level < sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL); ++level)
    {
        if (mPlayerXPperLevel[level] == 0)
        {
            sLog.outErrorDb("Level %i does not have XP for level data. Using data of level [%i] + 100.", level + 1, level);
            mPlayerXPperLevel[level] = mPlayerXPperLevel[level - 1] + 100;
        }
    }
}
void ObjectMgr::LoadStandingList(uint32 dateBegin)
{
    HonorStanding Standing;

    // needed for reload case
    AllyHonorStandingList.clear();
    HordeHonorStandingList.clear();

    uint32 guid, kills, side;

    Field* fields = NULL;
    QueryResult* result2 = NULL;
    // this query create an ordered standing list
    QueryResult* result = CharacterDatabase.PQuery("SELECT `guid`,SUM(`honor`) as honor_sum FROM `character_honor_cp` WHERE `TYPE` = %u AND `date` BETWEEN %u AND %u AND `used`=0 GROUP BY `guid` ORDER BY `honor_sum` DESC", HONORABLE, dateBegin, dateBegin + 7);
    if (result)
    {
        BarGoLink bar(result->GetRowCount());

        do
        {
            fields = result->Fetch();
            guid  = fields[0].GetUInt32();
            side = GetPlayerTeamByGUID(ObjectGuid(HIGHGUID_PLAYER, guid));

            kills = 0;
            // kills count with victim setted ( not zero value )
            result2 = CharacterDatabase.PQuery("SELECT COUNT(*) FROM `character_honor_cp` WHERE `guid` = %u AND `victim`>0 AND `TYPE` = %u AND `date` BETWEEN %u AND %u AND `used`=0", guid, HONORABLE, dateBegin, dateBegin + 7);
            if (result2)
            {
                kills = result2->Fetch()->GetUInt32();
            }

            // you need to reach CONFIG_UINT32_MIN_HONOR_KILLS to be added in standing list
            if (kills < sWorld.getConfig(CONFIG_UINT32_MIN_HONOR_KILLS))
            {
                continue;
            }

            Standing.guid = guid;
            Standing.honorPoints = fields[1].GetUInt32();
            Standing.honorKills = kills;

            if (side == ALLIANCE)
            {
                AllyHonorStandingList.push_back(Standing);
            }
            else if (side == HORDE)
            {
                HordeHonorStandingList.push_back(Standing);
            }

            bar.step();
        }
        while (result->NextRow());

        delete result;
        delete result2;

        // make sure all things are sorted
        AllyHonorStandingList.sort();
        HordeHonorStandingList.sort();
    }
}

void ObjectMgr::LoadStandingList()
{

    uint32 LastWeekBegin = sWorld.GetDateLastMaintenanceDay() - 7;
    LoadStandingList(LastWeekBegin);

    // distribution of RP earning without flushing table
    DistributeRankPoints(ALLIANCE, LastWeekBegin);
    DistributeRankPoints(HORDE, LastWeekBegin);

    sLog.outString();
    sLog.outString(">> Loaded %lu Horde and %lu Ally honor standing definitions", HordeHonorStandingList.size(), AllyHonorStandingList.size());
}

void ObjectMgr::FlushRankPoints(uint32 dateTop)
{
    // FLUSH CP
    QueryResult* result = CharacterDatabase.PQuery("SELECT `date` FROM `character_honor_cp` WHERE `TYPE` = %u AND `date` <= %u AND `used`=0 GROUP BY `date` ORDER BY `date` DESC", HONORABLE, dateTop);
    if (result)
    {
        uint32 date;
        bool flush;
        uint32 WeekBegin = dateTop - 7;
        // search latest non-processed date if the server has been offline for different weeks
        do
        {
            date = result->Fetch()->GetUInt32();
            while (WeekBegin && date < WeekBegin)
            {
                WeekBegin -= 7;
            }
        }
        while (result->NextRow());

        // start to flush from latest non-processed date to up
        while (WeekBegin <= dateTop)
        {
            LoadStandingList(WeekBegin);

            flush = WeekBegin <= dateTop - 7; // flush only with date < lastweek

            DistributeRankPoints(ALLIANCE, WeekBegin, flush);
            DistributeRankPoints(HORDE, WeekBegin, flush);

            WeekBegin += 7;
        }

        delete result;
    }

    // FLUSH KILLS
    static SqlStatementID updHonorable;
    static SqlStatementID updDishonorable;
    // process only HK ( victim_type > 0 )
    result = CharacterDatabase.PQuery("SELECT `guid`,`TYPE`,COUNT(*) AS kills FROM `character_honor_cp` WHERE `date` <= %u AND `victim_type`>0 AND `used`=0 GROUP BY `guid`,`type`", dateTop - 7);
    if (result)
    {
        CharacterDatabase.BeginTransaction();
        uint32 guid, kills;
        uint8 type;
        Field* fields = NULL;
        do
        {
            fields = result->Fetch();
            guid   = fields[0].GetUInt32();
            type   = fields[1].GetUInt8();
            kills  = fields[2].GetUInt32();

            if (type == HONORABLE)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updHonorable, "UPDATE `characters` SET `stored_honorable_kills` = (`stored_honorable_kills` + ?) WHERE `guid` = ?");
                stmt.PExecute(kills, guid);
            }
            else if (type == DISHONORABLE)
            {
                SqlStatement stmt = CharacterDatabase.CreateStatement(updDishonorable, "UPDATE `characters` SET `stored_dishonorable_kills` = (`stored_dishonorable_kills` + ?) WHERE `guid` = ?");
                stmt.PExecute(kills, guid);
            }
        }
        while (result->NextRow());

        // cleaning ALL cp before dateTop
        CharacterDatabase.PExecute("DELETE FROM `character_honor_cp` WHERE `date` <= %u", dateTop - 7);
        CharacterDatabase.CommitTransaction();
        delete result;
    }
    else
    {
        CharacterDatabase.PExecute("DELETE FROM `character_honor_cp` WHERE `date` <= %u AND `used`=1", dateTop - 7);
    }

    sLog.outString();
    sLog.outString(">> Flushed all ranking points");
}

void ObjectMgr::DistributeRankPoints(uint32 team, uint32 dateBegin , bool flush /*false*/)
{
    float RP;
    uint32 HK;

    HonorStandingList list = GetStandingListBySide(team);

    if (list.empty())
    {
        return;
    }

    HonorScores scores = MaNGOS::Honor::GenerateScores(list, team);

    Field* fields = NULL;
    QueryResult* result = NULL;
    for (HonorStandingList::iterator itr = list.begin(); itr != list.end() ; ++itr)
    {
        RP = 0;
        result = CharacterDatabase.PQuery("SELECT `stored_honor_rating`,`stored_honorable_kills` FROM `characters` WHERE `guid` = %u ", itr->guid);
        if (!result)
        {
            continue; // not cleaned table?
        }

        fields = result->Fetch();
        RP = fields[0].GetFloat();
        HK = fields[1].GetUInt32();

        itr->rpEarning = MaNGOS::Honor::CalculateRpEarning(itr->GetInfo()->honorPoints, scores);
        RP             = MaNGOS::Honor::CalculateRpDecay(itr->rpEarning, RP);

        if (flush)
        {
            CharacterDatabase.BeginTransaction();
            CharacterDatabase.PExecute("UPDATE `character_honor_cp` SET `used`=1 WHERE `guid` = %u AND `TYPE` = %u AND `date` BETWEEN %u AND %u", itr->guid, HONORABLE, dateBegin, dateBegin + 7);
            CharacterDatabase.PExecute("UPDATE `characters` SET `stored_honor_rating` = %f , `stored_honorable_kills` = %u WHERE `guid` = %u", finiteAlways(RP + itr->rpEarning), HK + itr->honorKills, itr->guid);
            CharacterDatabase.CommitTransaction();
        }
    }

    delete result;
}

HonorStandingList ObjectMgr::GetStandingListBySide(uint32 side)
{
    switch (side)
    {
        case ALLIANCE: return AllyHonorStandingList;
        case HORDE:    return HordeHonorStandingList;
        default:       return AllyHonorStandingList; // mustn't happen
    }
}

HonorStanding* ObjectMgr::GetHonorStandingByGUID(uint32 guid, uint32 side)
{
    HonorStandingList standingList = sObjectMgr.GetStandingListBySide(side);

    for (HonorStandingList::iterator itr = standingList.begin(); itr != standingList.end() ; ++itr)
        if (itr->guid == guid)
        {
            return itr->GetInfo();
        }

    return 0;
}


HonorStanding* ObjectMgr::GetHonorStandingByPosition(uint32 position, uint32 side)
{
    HonorStandingList standingList = sObjectMgr.GetStandingListBySide(side);
    uint32 pos = 1;

    for (HonorStandingList::iterator itr = standingList.begin(); itr != standingList.end() ; ++itr)
    {
        if (pos == position)
        {
            return itr->GetInfo();
        }
        pos++;
    }

    return 0;
}

uint32 ObjectMgr::GetHonorStandingPositionByGUID(uint32 guid, uint32 side)
{
    HonorStandingList standingList = sObjectMgr.GetStandingListBySide(side);
    uint32 pos = 1;

    for (HonorStandingList::iterator itr = standingList.begin(); itr != standingList.end() ; ++itr)
    {
        if (itr->guid == guid)
        {
            return pos;
        }
        pos++;
    }

    return 0;
}

void ObjectMgr::GetPlayerClassLevelInfo(uint32 class_, uint32 level, PlayerClassLevelInfo* info) const
{
    if (level < 1 || class_ >= MAX_CLASSES)
    {
        return;
    }

    PlayerClassInfo const* pInfo = &playerClassInfo[class_];

    if (level > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        level = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL);
    }

    *info = pInfo->levelInfo[level - 1];
}

void ObjectMgr::GetPlayerLevelInfo(uint32 race, uint32 class_, uint32 level, PlayerLevelInfo* info) const
{
    if (level < 1 || race   >= MAX_RACES || class_ >= MAX_CLASSES)
    {
        return;
    }

    PlayerInfo const* pInfo = &playerInfo[race][class_];
    if (pInfo->displayId_m == 0 || pInfo->displayId_f == 0)
    {
        return;
    }

    if (level <= sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
    {
        *info = pInfo->levelInfo[level - 1];
    }
    else
    {
        BuildPlayerLevelInfo(race, class_, level, info);
    }
}

void ObjectMgr::BuildPlayerLevelInfo(uint8 race, uint8 _class, uint8 level, PlayerLevelInfo* info) const
{
    // base data (last known level)
    *info = playerInfo[race][_class].levelInfo[sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) - 1];

    for (int lvl = sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL) - 1; lvl < level; ++lvl)
    {
        switch (_class)
        {
            case CLASS_WARRIOR:
                info->stats[STAT_STRENGTH]  += (lvl > 23 ? 2 : (lvl > 1  ? 1 : 0));
                info->stats[STAT_STAMINA]   += (lvl > 23 ? 2 : (lvl > 1  ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 36 ? 1 : (lvl > 6 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                break;
            case CLASS_PALADIN:
                info->stats[STAT_STRENGTH]  += (lvl > 3  ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 33 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 1 : (lvl > 7 && !(lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 6 && (lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 7 ? 1 : 0);
                break;
            case CLASS_HUNTER:
                info->stats[STAT_STRENGTH]  += (lvl > 4  ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 4  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 33 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 8 && (lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 1 : (lvl > 9 && !(lvl % 2) ? 1 : 0));
                break;
            case CLASS_ROGUE:
                info->stats[STAT_STRENGTH]  += (lvl > 5  ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 4  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 16 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 8 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 1 : (lvl > 9 && !(lvl % 2) ? 1 : 0));
                break;
            case CLASS_PRIEST:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 5  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 1 : (lvl > 8 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 22 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 3  ? 1 : 0);
                break;
            case CLASS_SHAMAN:
                info->stats[STAT_STRENGTH]  += (lvl > 34 ? 1 : (lvl > 6 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_STAMINA]   += (lvl > 4 ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 7 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_INTELLECT] += (lvl > 5 ? 1 : 0);
                info->stats[STAT_SPIRIT]    += (lvl > 4 ? 1 : 0);
                break;
            case CLASS_MAGE:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 5  ? 1 : 0);
                info->stats[STAT_AGILITY]   += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_INTELLECT] += (lvl > 24 ? 2 : (lvl > 1 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 33 ? 2 : (lvl > 2 ? 1 : 0));
                break;
            case CLASS_WARLOCK:
                info->stats[STAT_STRENGTH]  += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_STAMINA]   += (lvl > 38 ? 2 : (lvl > 3 ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 9 && !(lvl % 2) ? 1 : 0);
                info->stats[STAT_INTELLECT] += (lvl > 33 ? 2 : (lvl > 2 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 2 : (lvl > 3 ? 1 : 0));
                break;
            case CLASS_DRUID:
                info->stats[STAT_STRENGTH]  += (lvl > 38 ? 2 : (lvl > 6 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_STAMINA]   += (lvl > 32 ? 2 : (lvl > 4 ? 1 : 0));
                info->stats[STAT_AGILITY]   += (lvl > 38 ? 2 : (lvl > 8 && (lvl % 2) ? 1 : 0));
                info->stats[STAT_INTELLECT] += (lvl > 38 ? 3 : (lvl > 4 ? 1 : 0));
                info->stats[STAT_SPIRIT]    += (lvl > 38 ? 3 : (lvl > 5 ? 1 : 0));
        }
    }
}

/* ********************************************************************************************* */
/* *                                Static Wrappers                                              */
/* ********************************************************************************************* */
GameObjectInfo const* ObjectMgr::GetGameObjectInfo(uint32 id) { return sGOStorage.LookupEntry<GameObjectInfo>(id); }
Player* ObjectMgr::GetPlayer(const char* name) { return sObjectAccessor.FindPlayerByName(name); }
Player* ObjectMgr::GetPlayer(ObjectGuid guid, bool inWorld /*=true*/) { return sObjectAccessor.FindPlayer(guid, inWorld); }
CreatureInfo const* ObjectMgr::GetCreatureTemplate(uint32 id) { return sCreatureStorage.LookupEntry<CreatureInfo>(id); }
CreatureModelInfo const* ObjectMgr::GetCreatureModelInfo(uint32 modelid) { return sCreatureModelStorage.LookupEntry<CreatureModelInfo>(modelid); }
EquipmentInfo const* ObjectMgr::GetEquipmentInfo(uint32 entry) { return sEquipmentStorage.LookupEntry<EquipmentInfo>(entry); }
EquipmentInfoItem const* ObjectMgr::GetEquipmentInfoItem(uint32 entry) { return sEquipmentStorageItem.LookupEntry<EquipmentInfoItem>(entry); }
EquipmentInfoRaw const* ObjectMgr::GetEquipmentInfoRaw(uint32 entry) { return sEquipmentStorageRaw.LookupEntry<EquipmentInfoRaw>(entry); }
CreatureDataAddon const* ObjectMgr::GetCreatureAddon(uint32 lowguid) { return sCreatureDataAddonStorage.LookupEntry<CreatureDataAddon>(lowguid); }
CreatureDataAddon const* ObjectMgr::GetCreatureTemplateAddon(uint32 entry) { return sCreatureInfoAddonStorage.LookupEntry<CreatureDataAddon>(entry); }
ItemPrototype const* ObjectMgr::GetItemPrototype(uint32 id) { return sItemStorage.LookupEntry<ItemPrototype>(id); }
InstanceTemplate const* ObjectMgr::GetInstanceTemplate(uint32 map) { return sInstanceTemplate.LookupEntry<InstanceTemplate>(map); }

/* ********************************************************************************************* */
/* *                                Loading Functions                                            */
/* ********************************************************************************************* */
void ObjectMgr::LoadGroups()
{
    // -- loading groups --
    uint32 count = 0;
    //                                                    0         1              2           3           4              5      6      7      8      9      10     11     12     13      14          15
    QueryResult* result = CharacterDatabase.Query("SELECT `mainTank`, `mainAssistant`, `lootMethod`, `looterGuid`, `lootThreshold`, `icon1`, `icon2`, `icon3`, `icon4`, `icon5`, `icon6`, `icon7`, `icon8`, `isRaid`, `leaderGuid`, `groupId` FROM `groups`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u group definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();
        Field* fields = result->Fetch();
        ++count;
        Group* group = new Group;
        if (!group->LoadGroupFromDB(fields))
        {
            group->Disband();
            delete group;
            continue;
        }
        AddGroup(group);
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u group definitions", count);
    sLog.outString();

    // -- loading members --
    count = 0;
    //                                       0           1          2         3
    result = CharacterDatabase.Query("SELECT `memberGuid`, `assistant`, `subgroup`, `groupId` FROM `group_member` ORDER BY `groupId`");
    if (!result)
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group = NULL;                                // used as cached pointer for avoid relookup group for each member

        BarGoLink bar2(result->GetRowCount());
        do
        {
            bar2.step();
            Field* fields = result->Fetch();
            ++count;

            uint32 memberGuidlow = fields[0].GetUInt32();
            ObjectGuid memberGuid = ObjectGuid(HIGHGUID_PLAYER, memberGuidlow);
            bool   assistent     = fields[1].GetBool();
            uint8  subgroup      = fields[2].GetUInt8();
            uint32 groupId       = fields[3].GetUInt32();
            if (!group || group->GetId() != groupId)
            {
                group = GetGroupById(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_member table : no group with Id %d for member %s!",
                                    groupId, memberGuid.GetString().c_str());
                    CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid` = '%u'", memberGuidlow);
                    continue;
                }
            }

            if (!group->LoadMemberFromDB(memberGuidlow, subgroup, assistent))
            {
                sLog.outErrorDb("Incorrect entry in group_member table : member %s can not be added to group (Id: %u)!",
                                memberGuid.GetString().c_str(), groupId);
                CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `memberGuid` = '%u'", memberGuidlow);
            }
        }
        while (result->NextRow());
        delete result;
    }

    // clean groups
    // TODO: maybe delete from the DB before loading in this case
    for (GroupMap::iterator itr = mGroupMap.begin(); itr != mGroupMap.end();)
    {
        if (itr->second->GetMembersCount() < 2)
        {
            itr->second->Disband();
            delete itr->second;
            mGroupMap.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    // -- loading instances --
    count = 0;
    result = CharacterDatabase.Query(
                 //      0                          1    2         3          4
                 "SELECT `group_instance`.`leaderGuid`, `map`, `instance`, `permanent`, `resettime`, "
                 // 5
                 "(SELECT COUNT(*) FROM `character_instance` WHERE `guid` = `group_instance`.`leaderGuid` AND `instance` = `group_instance`.`instance` AND `permanent` = 1 LIMIT 1), "
                 // 6
                 " `groups`.`groupId` "
                 "FROM `group_instance` LEFT JOIN `instance` ON `instance` = `id` LEFT JOIN `groups` ON `groups`.`leaderGUID` = `group_instance`.`leaderGUID` ORDER BY `leaderGuid`"
             );

    if (!result)
    {
        BarGoLink bar2(1);
        bar2.step();
    }
    else
    {
        Group* group = NULL;                                // used as cached pointer for avoid relookup group for each member

        BarGoLink bar2(result->GetRowCount());
        do
        {
            bar2.step();
            Field* fields = result->Fetch();
            ++count;

            uint32 leaderGuidLow = fields[0].GetUInt32();
            uint32 mapId = fields[1].GetUInt32();
            uint32 groupId = fields[6].GetUInt32();

            if (!group || group->GetId() != groupId)
            {
                // find group id in map by leader low guid
                group = GetGroupById(groupId);
                if (!group)
                {
                    sLog.outErrorDb("Incorrect entry in group_instance table : no group with leader %d", leaderGuidLow);
                    continue;
                }
            }

            MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
            if (!mapEntry || !mapEntry->IsDungeon())
            {
                sLog.outErrorDb("Incorrect entry in group_instance table : no dungeon map %d", mapId);
                continue;
            }

            DungeonPersistentState* state = (DungeonPersistentState*)sMapPersistentStateMgr.AddPersistentState(mapEntry, fields[2].GetUInt32(), (time_t)fields[4].GetUInt64(), (fields[5].GetUInt32() == 0), true);
            group->BindToInstance(state, fields[3].GetBool(), true);
        }
        while (result->NextRow());
        delete result;
    }

    sLog.outString(">> Loaded %u group-instance binds total", count);
    sLog.outString();

    sLog.outString(">> Loaded %u group members total", count);
    sLog.outString();
}

void ObjectMgr::LoadQuests()
{
    // For reload case
    for (QuestMap::const_iterator itr = mQuestTemplates.begin(); itr != mQuestTemplates.end(); ++itr)
    {
        delete itr->second;
    }

    mQuestTemplates.clear();

    m_ExclusiveQuestGroups.clear();

    //                                                0      1       2           3         4           5     6                7              8              9
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `Method`, `ZoneOrSort`, `MinLevel`, `QuestLevel`, `Type`, `RequiredClasses`, `RequiredRaces`, `RequiredSkill`, `RequiredSkillValue`,"
                          //   10                   11                 12                     13                   14                     15                   16                17
                          "`RepObjectiveFaction`, `RepObjectiveValue`, `RequiredMinRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepFaction`, `RequiredMaxRepValue`, `SuggestedPlayers`, `LimitTime`,"
                          //   18          19            20           21           22              23                24         25            26
                          "`QuestFlags`, `SpecialFlags`, `PrevQuestId`, `NextQuestId`, `ExclusiveGroup`, `NextQuestInChain`, `SrcItemId`, `SrcItemCount`, `SrcSpell`,"
                          //   27     28       29          30               31                32       33              34              35              36
                          "`Title`, `Details`, `Objectives`, `OfferRewardText`, `RequestItemsText`, `EndText`, `ObjectiveText1`, `ObjectiveText2`, `ObjectiveText3`, `ObjectiveText4`,"
                          //   37          38          39          40          41             42             43             44
                          "`ReqItemId1`, `ReqItemId2`, `ReqItemId3`, `ReqItemId4`, `ReqItemCount1`, `ReqItemCount2`, `ReqItemCount3`, `ReqItemCount4`,"
                          //   45            46            47            48            49               50               51               52
                          "`ReqSourceId1`, `ReqSourceId2`, `ReqSourceId3`, `ReqSourceId4`, `ReqSourceCount1`, `ReqSourceCount2`, `ReqSourceCount3`, `ReqSourceCount4`,"
                          //   53                  54                  55                  56                  57                     58                     59                     60
                          "`ReqCreatureOrGOId1`, `ReqCreatureOrGOId2`, `ReqCreatureOrGOId3`, `ReqCreatureOrGOId4`, `ReqCreatureOrGOCount1`, `ReqCreatureOrGOCount2`, `ReqCreatureOrGOCount3`, `ReqCreatureOrGOCount4`,"
                          //   61             62             63             64
                          "`ReqSpellCast1`, `ReqSpellCast2`, `ReqSpellCast3`, `ReqSpellCast4`,"
                          //   65                66                67                68                69                70
                          "`RewChoiceItemId1`, `RewChoiceItemId2`, `RewChoiceItemId3`, `RewChoiceItemId4`, `RewChoiceItemId5`, `RewChoiceItemId6`,"
                          //   71                   72                   73                   74                   75                   76
                          "`RewChoiceItemCount1`, `RewChoiceItemCount2`, `RewChoiceItemCount3`, `RewChoiceItemCount4`, `RewChoiceItemCount5`, `RewChoiceItemCount6`,"
                          //   77          78          79          80          81             82             83             84
                          "`RewItemId1`, `RewItemId2`, `RewItemId3`, `RewItemId4`, `RewItemCount1`, `RewItemCount2`, `RewItemCount3`, `RewItemCount4`,"
                          //   85              86              87              88              89              90            91            92            93            94
                          "`RewRepFaction1`, `RewRepFaction2`, `RewRepFaction3`, `RewRepFaction4`, `RewRepFaction5`, `RewRepValue1`, `RewRepValue2`, `RewRepValue3`, `RewRepValue4`, `RewRepValue5`,"
                          //   95             96                97        98            99                 100               101         102     103     104
                          "`RewOrReqMoney`, `RewMoneyMaxLevel`, `RewSpell`, `RewSpellCast`, `RewMailTemplateId`, `RewMailDelaySecs`, `PointMapId`, `PointX`, `PointY`, `PointOpt`,"
                          //   105            106            107            108            109                 110                 111                 112
                          "`DetailsEmote1`, `DetailsEmote2`, `DetailsEmote3`, `DetailsEmote4`, `DetailsEmoteDelay1`, `DetailsEmoteDelay2`, `DetailsEmoteDelay3`, `DetailsEmoteDelay4`,"
                          //   113              114            115                116                117                118
                          "`IncompleteEmote`, `CompleteEmote`, `OfferRewardEmote1`, `OfferRewardEmote2`, `OfferRewardEmote3`, `OfferRewardEmote4`,"
                          //   119                     120                     121                     122
                          "`OfferRewardEmoteDelay1`, `OfferRewardEmoteDelay2`, `OfferRewardEmoteDelay3`, `OfferRewardEmoteDelay4`,"
                          //   123          124
                          "`StartScript`, `CompleteScript`"
                          " FROM `quest_template`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Loaded 0 quests definitions");
        sLog.outErrorDb("`quest_template` table is empty!");
        sLog.outString();
        return;
    }

    // create multimap previous quest for each existing quest
    // some quests can have many previous maps set by NextQuestId in previous quest
    // for example set of race quests can lead to single not race specific quest
    BarGoLink bar(result->GetRowCount());
    do
    {
        bar.step();
        Field* fields = result->Fetch();

        Quest* newQuest = new Quest(fields);
        mQuestTemplates[newQuest->GetQuestId()] = newQuest;
    }
    while (result->NextRow());

    delete result;

    // Post processing

    std::map<uint32, uint32> usedMailTemplates;

    for (QuestMap::iterator iter = mQuestTemplates.begin(); iter != mQuestTemplates.end(); ++iter)
    {
        // skip post-loading checks for disabled quests
        if (DisableMgr::IsDisabledFor(DISABLE_TYPE_QUEST, iter->first))
        {
            continue;
        }

        Quest* qinfo = iter->second;

        // additional quest integrity checks (GO, creature_template and item_template must be loaded already)

        if (qinfo->GetQuestMethod() >= 3)
        {
            sLog.outErrorDb("Quest %u has `Method` = %u, expected values are 0, 1 or 2.", qinfo->GetQuestId(), qinfo->GetQuestMethod());
        }

        if (qinfo->m_SpecialFlags > QUEST_SPECIAL_FLAG_DB_ALLOWED)
        {
            sLog.outErrorDb("Quest %u has `SpecialFlags` = %u, above max flags not allowed for database.", qinfo->GetQuestId(), qinfo->m_SpecialFlags);
        }

        if (qinfo->HasQuestFlag(QUEST_FLAGS_AUTO_REWARDED))
        {
            // at auto-reward can be rewarded only RewChoiceItemId[0]
            for (int j = 1; j < QUEST_REWARD_CHOICES_COUNT; ++j)
            {
                if (uint32 id = qinfo->RewChoiceItemId[j])
                {
                    sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = %u but item from `RewChoiceItemId%d` can't be rewarded with quest flag QUEST_FLAGS_AUTO_REWARDED.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest ignore this data
                }
            }
        }

        // client quest log visual (area case)
        if (qinfo->ZoneOrSort > 0)
        {
            if (!GetAreaEntryByAreaID(qinfo->ZoneOrSort))
            {
                sLog.outErrorDb("Quest %u has `ZoneOrSort` = %u (zone case) but zone with this id does not exist.",
                                qinfo->GetQuestId(), qinfo->ZoneOrSort);
                // no changes, quest not dependent from this value but can have problems at client
            }
        }
        // client quest log visual (sort case)
        if (qinfo->ZoneOrSort < 0)
        {
            QuestSortEntry const* qSort = sQuestSortStore.LookupEntry(-int32(qinfo->ZoneOrSort));
            if (!qSort)
            {
                sLog.outErrorDb("Quest %u has `ZoneOrSort` = %i (sort case) but quest sort with this id does not exist.",
                                qinfo->GetQuestId(), qinfo->ZoneOrSort);
                // no changes, quest not dependent from this value but can have problems at client (note some may be 0, we must allow this so no check)
            }

            // check for proper RequiredSkill value (skill case)
            if (uint32 skill_id = SkillByQuestSort(-int32(qinfo->ZoneOrSort)))
            {
                if (qinfo->RequiredSkill != skill_id)
                {
                    sLog.outErrorDb("Quest %u has `ZoneOrSort` = %i but `RequiredSkill` does not have a corresponding value (%u).",
                                    qinfo->GetQuestId(), qinfo->ZoneOrSort, skill_id);
                    // override, and force proper value here?
                }
            }
        }

        // RequiredClasses, can be 0/CLASSMASK_ALL_PLAYABLE to allow any class
        if (qinfo->RequiredClasses)
        {
            if (!(qinfo->RequiredClasses & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Quest %u does not contain any playable classes in `RequiredClasses` (%u), value set to 0 (all classes).", qinfo->GetQuestId(), qinfo->RequiredClasses);
                qinfo->RequiredClasses = 0;
            }
        }

        // RequiredRaces, can be 0/RACEMASK_ALL_PLAYABLE to allow any race
        if (qinfo->RequiredRaces)
        {
            if (!(qinfo->RequiredRaces & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Quest %u does not contain any playable races in `RequiredRaces` (%u), value set to 0 (all races).", qinfo->GetQuestId(), qinfo->RequiredRaces);
                qinfo->RequiredRaces = 0;
            }
        }

        // RequiredSkill, can be 0
        if (qinfo->RequiredSkill)
        {
            if (!sSkillLineStore.LookupEntry(qinfo->RequiredSkill))
            {
                sLog.outErrorDb("Quest %u has `RequiredSkill` = %u but this skill does not exist",
                                qinfo->GetQuestId(), qinfo->RequiredSkill);
            }
        }

        if (qinfo->RequiredSkillValue)
        {
            if (qinfo->RequiredSkillValue > sWorld.GetConfigMaxSkillValue())
            {
                sLog.outErrorDb("Quest %u has `RequiredSkillValue` = %u but max possible skill is %u, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->RequiredSkillValue, sWorld.GetConfigMaxSkillValue());
                // no changes, quest can't be done for this requirement
            }
        }
        // else Skill quests can have 0 skill level, this is ok

        if (qinfo->RepObjectiveFaction && !sFactionStore.LookupEntry(qinfo->RepObjectiveFaction))
        {
            sLog.outErrorDb("Quest %u has `RepObjectiveFaction` = %u but faction template %u does not exist, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RepObjectiveFaction, qinfo->RepObjectiveFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepFaction && !sFactionStore.LookupEntry(qinfo->RequiredMinRepFaction))
        {
            sLog.outErrorDb("Quest %u has `RequiredMinRepFaction` = %u but faction template %u does not exist, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMinRepFaction, qinfo->RequiredMinRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMaxRepFaction && !sFactionStore.LookupEntry(qinfo->RequiredMaxRepFaction))
        {
            sLog.outErrorDb("Quest %u has `RequiredMaxRepFaction` = %u but faction template %u does not exist, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMaxRepFaction, qinfo->RequiredMaxRepFaction);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepValue && qinfo->RequiredMinRepValue > ReputationMgr::Reputation_Cap)
        {
            sLog.outErrorDb("Quest %u has `RequiredMinRepValue` = %d but max reputation is %u, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMinRepValue, ReputationMgr::Reputation_Cap);
            // no changes, quest can't be done for this requirement
        }

        if (qinfo->RequiredMinRepValue && qinfo->RequiredMaxRepValue && qinfo->RequiredMaxRepValue <= qinfo->RequiredMinRepValue)
        {
            sLog.outErrorDb("Quest %u has `RequiredMaxRepValue` = %d and `RequiredMinRepValue` = %d, quest can't be done.",
                            qinfo->GetQuestId(), qinfo->RequiredMaxRepValue, qinfo->RequiredMinRepValue);
            // no changes, quest can't be done for this requirement
        }

        if (!qinfo->RepObjectiveFaction && qinfo->RepObjectiveValue > 0)
        {
            sLog.outErrorDb("Quest %u has `RepObjectiveValue` = %d but `RepObjectiveFaction` is 0, value has no effect",
                            qinfo->GetQuestId(), qinfo->RepObjectiveValue);
            // warning
        }

        if (!qinfo->RequiredMinRepFaction && qinfo->RequiredMinRepValue > 0)
        {
            sLog.outErrorDb("Quest %u has `RequiredMinRepValue` = %d but `RequiredMinRepFaction` is 0, value has no effect",
                            qinfo->GetQuestId(), qinfo->RequiredMinRepValue);
            // warning
        }

        if (!qinfo->RequiredMaxRepFaction && qinfo->RequiredMaxRepValue > 0)
        {
            sLog.outErrorDb("Quest %u has `RequiredMaxRepValue` = %d but `RequiredMaxRepFaction` is 0, value has no effect",
                            qinfo->GetQuestId(), qinfo->RequiredMaxRepValue);
            // warning
        }

        if (qinfo->SrcItemId)
        {
            if (!sItemStorage.LookupEntry<ItemPrototype>(qinfo->SrcItemId))
            {
                sLog.outErrorDb("Quest %u has `SrcItemId` = %u but item with entry %u does not exist, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->SrcItemId, qinfo->SrcItemId);
                qinfo->SrcItemId = 0;                       // quest can't be done for this requirement
            }
            else if (qinfo->SrcItemCount == 0)
            {
                sLog.outErrorDb("Quest %u has `SrcItemId` = %u but `SrcItemCount` = 0, set to 1 but need fix in DB.",
                                qinfo->GetQuestId(), qinfo->SrcItemId);
                qinfo->SrcItemCount = 1;                    // update to 1 for allow quest work for backward compatibility with DB
            }
        }
        else if (qinfo->SrcItemCount > 0)
        {
            sLog.outErrorDb("Quest %u has `SrcItemId` = 0 but `SrcItemCount` = %u, useless value.",
                            qinfo->GetQuestId(), qinfo->SrcItemCount);
            qinfo->SrcItemCount = 0;                        // no quest work changes in fact
        }

        if (qinfo->SrcSpell)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->SrcSpell);
            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `SrcSpell` = %u but spell %u doesn't exist, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->SrcSpell, qinfo->SrcSpell);
                qinfo->SrcSpell = 0;                        // quest can't be done for this requirement
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `SrcSpell` = %u but spell %u is broken, quest can't be done.",
                                qinfo->GetQuestId(), qinfo->SrcSpell, qinfo->SrcSpell);
                qinfo->SrcSpell = 0;                        // quest can't be done for this requirement
            }
        }

        for (int j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqItemId[j])
            {
                if (qinfo->ReqItemCount[j] == 0)
                {
                    sLog.outErrorDb("Quest %u has `ReqItemId%d` = %u but `ReqItemCount%d` = 0, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can't be done for this requirement
                }

                qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_DELIVER);

                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    sLog.outErrorDb("Quest %u has `ReqItemId%d` = %u but item with entry %u does not exist, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->ReqItemCount[j] = 0;             // prevent incorrect work of quest
                }
            }
            else if (qinfo->ReqItemCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `ReqItemId%d` = 0 but `ReqItemCount%d` = %u, quest can't be done.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqItemCount[j]);
                qinfo->ReqItemCount[j] = 0;                 // prevent incorrect work of quest
            }
        }

        for (int j = 0; j < QUEST_SOURCE_ITEM_IDS_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqSourceId[j])
            {
                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    sLog.outErrorDb("Quest %u has `ReqSourceId%d` = %u but item with entry %u does not exist, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    // no changes, quest can't be done for this requirement
                }
            }
            else
            {
                if (qinfo->ReqSourceCount[j] > 0)
                {
                    sLog.outErrorDb("Quest %u has `ReqSourceId%d` = 0 but `ReqSourceCount%d` = %u.",
                                    qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqSourceCount[j]);
                    // no changes, quest ignore this data
                }
            }
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            if (uint32 id = qinfo->ReqSpell[j])
            {
                SpellEntry const* spellInfo = sSpellStore.LookupEntry(id);
                if (!spellInfo)
                {
                    sLog.outErrorDb("Quest %u has `ReqSpellCast%d` = %u but spell %u does not exist, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    continue;
                }

                if (!qinfo->ReqCreatureOrGOId[j])
                {
                    bool found = false;
                    for (int k = 0; k < MAX_EFFECT_INDEX; ++k)
                    {
                        if ((spellInfo->Effect[k] == SPELL_EFFECT_QUEST_COMPLETE && uint32(spellInfo->EffectMiscValue[k]) == qinfo->QuestId) ||
                            spellInfo->Effect[k] == SPELL_EFFECT_SEND_EVENT)
                        {
                            found = true;
                            break;
                        }
                    }

                    if (found)
                    {
                        if (!qinfo->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
                        {
                            sLog.outErrorDb("Spell (id: %u) have SPELL_EFFECT_QUEST_COMPLETE or SPELL_EFFECT_SEND_EVENT for quest %u and ReqCreatureOrGOId%d = 0, but quest not have flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. Quest flags or ReqCreatureOrGOId%d must be fixed, quest modified to enable objective.", spellInfo->Id, qinfo->QuestId, j + 1, j + 1);

                            // this will prevent quest completing without objective
                            const_cast<Quest*>(qinfo)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);
                        }
                    }
                    else
                    {
                        sLog.outErrorDb("Quest %u has `ReqSpellCast%d` = %u and ReqCreatureOrGOId%d = 0 but spell %u does not have SPELL_EFFECT_QUEST_COMPLETE or SPELL_EFFECT_SEND_EVENT effect for this quest, quest can't be done.",
                                        qinfo->GetQuestId(), j + 1, id, j + 1, id);
                        // no changes, quest can't be done for this requirement
                    }
                }
            }
        }

        for (int j = 0; j < QUEST_OBJECTIVES_COUNT; ++j)
        {
            int32 id = qinfo->ReqCreatureOrGOId[j];
            if (id < 0 && !sGOStorage.LookupEntry<GameObjectInfo>(-id))
            {
                sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = %i but gameobject %u does not exist, quest can't be done.",
                                qinfo->GetQuestId(), j + 1, id, uint32(-id));
                qinfo->ReqCreatureOrGOId[j] = 0;            // quest can't be done for this requirement
            }

            if (id > 0 && !sCreatureStorage.LookupEntry<CreatureInfo>(id))
            {
                sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = %i but creature with entry %u does not exist, quest can't be done.",
                                qinfo->GetQuestId(), j + 1, id, uint32(id));
                qinfo->ReqCreatureOrGOId[j] = 0;            // quest can't be done for this requirement
            }

            if (id)
            {
                // In fact SpeakTo and Kill are quite same: either you can speak to mob:SpeakTo or you can't:Kill/Cast

                qinfo->SetSpecialFlag(QuestSpecialFlags(QUEST_SPECIAL_FLAG_KILL_OR_CAST | QUEST_SPECIAL_FLAG_SPEAKTO));

                if (!qinfo->ReqCreatureOrGOCount[j])
                {
                    sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = %u but `ReqCreatureOrGOCount%d` = 0, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can be incorrectly done, but we already report this
                }
            }
            else if (qinfo->ReqCreatureOrGOCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `ReqCreatureOrGOId%d` = 0 but `ReqCreatureOrGOCount%d` = %u.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->ReqCreatureOrGOCount[j]);
                // no changes, quest ignore this data
            }
        }

        bool choice_found = false;
        for (int j = QUEST_REWARD_CHOICES_COUNT - 1; j >= 0; --j)
        {
            if (uint32 id = qinfo->RewChoiceItemId[j])
            {
                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = %u but item with entry %u does not exist, quest will not reward this item.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->RewChoiceItemId[j] = 0;          // no changes, quest will not reward this
                }
                else
                {
                    choice_found = true;
                }

                if (!qinfo->RewChoiceItemCount[j])
                {
                    sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = %u but `RewChoiceItemCount%d` = 0, quest can't be done.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes, quest can't be done
                }
            }
            else if (choice_found)                          // 1.12.1 client (but not later) crash if have gap in item reward choices
            {
                sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = 0 but `RewChoiceItemId%d` = %u, client can crash at like data.",
                                qinfo->GetQuestId(), j + 1, j + 2, qinfo->RewChoiceItemId[j + 1]);
                // fill gap by clone later filled choice
                qinfo->RewChoiceItemId[j] = qinfo->RewChoiceItemId[j + 1];
                qinfo->RewChoiceItemCount[j] = qinfo->RewChoiceItemCount[j + 1];
            }
            else if (qinfo->RewChoiceItemCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `RewChoiceItemId%d` = 0 but `RewChoiceItemCount%d` = %u.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewChoiceItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (int j = 0; j < QUEST_REWARDS_COUNT; ++j)
        {
            if (uint32 id = qinfo->RewItemId[j])
            {
                if (!sItemStorage.LookupEntry<ItemPrototype>(id))
                {
                    sLog.outErrorDb("Quest %u has `RewItemId%d` = %u but item with entry %u does not exist, quest will not reward this item.",
                                    qinfo->GetQuestId(), j + 1, id, id);
                    qinfo->RewItemId[j] = 0;                // no changes, quest will not reward this item
                }

                if (!qinfo->RewItemCount[j])
                {
                    sLog.outErrorDb("Quest %u has `RewItemId%d` = %u but `RewItemCount%d` = 0, quest will not reward this item.",
                                    qinfo->GetQuestId(), j + 1, id, j + 1);
                    // no changes
                }
            }
            else if (qinfo->RewItemCount[j] > 0)
            {
                sLog.outErrorDb("Quest %u has `RewItemId%d` = 0 but `RewItemCount%d` = %u.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewItemCount[j]);
                // no changes, quest ignore this data
            }
        }

        for (int j = 0; j < QUEST_REPUTATIONS_COUNT; ++j)
        {
            if (qinfo->RewRepFaction[j])
            {
                if (!qinfo->RewRepValue[j])
                {
                    sLog.outErrorDb("Quest %u has `RewRepFaction%d` = %u but `RewRepValue%d` = 0, quest will not reward this reputation.",
                                    qinfo->GetQuestId(), j + 1, qinfo->RewRepValue[j], j + 1);
                }
                if (!sFactionStore.LookupEntry(qinfo->RewRepFaction[j]))
                {
                    sLog.outErrorDb("Quest %u has `RewRepFaction%d` = %u but raw faction (faction.dbc) %u does not exist, quest will not reward reputation for this faction.",
                                    qinfo->GetQuestId(), j + 1, qinfo->RewRepFaction[j] , qinfo->RewRepFaction[j]);
                    qinfo->RewRepFaction[j] = 0;            // quest will not reward this
                }
            }
            else if (qinfo->RewRepValue[j] != 0)
            {
                sLog.outErrorDb("Quest %u has `RewRepFaction%d` = 0 but `RewRepValue%d` = %i.",
                                qinfo->GetQuestId(), j + 1, j + 1, qinfo->RewRepValue[j]);
                // no changes, quest ignore this data
            }
        }

        if (qinfo->RewSpell)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->RewSpell);

            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u does not exist, spell removed as display reward.",
                                qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell = 0;                        // no spell reward will display for this quest
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u is broken, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell = 0;                        // no spell reward will display for this quest
            }
            else if (GetTalentSpellCost(qinfo->RewSpell))
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u is talent, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpell, qinfo->RewSpell);
                qinfo->RewSpell = 0;                        // no spell reward will display for this quest
            }
        }

        if (qinfo->RewSpellCast)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(qinfo->RewSpellCast);

            if (!spellInfo)
            {
                sLog.outErrorDb("Quest %u has `RewSpellCast` = %u but spell %u does not exist, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpellCast, qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0;                    // no spell will be casted on player
            }
            else if (!SpellMgr::IsSpellValid(spellInfo))
            {
                sLog.outErrorDb("Quest %u has `RewSpellCast` = %u but spell %u is broken, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpellCast, qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0;                    // no spell will be casted on player
            }
            else if (GetTalentSpellCost(qinfo->RewSpellCast))
            {
                sLog.outErrorDb("Quest %u has `RewSpell` = %u but spell %u is talent, quest will not have a spell reward.",
                                qinfo->GetQuestId(), qinfo->RewSpellCast, qinfo->RewSpellCast);
                qinfo->RewSpellCast = 0;                    // no spell will be casted on player
            }
        }

        if (qinfo->RewMailTemplateId)
        {
            if (!sMailTemplateStore.LookupEntry(qinfo->RewMailTemplateId))
            {
                sLog.outErrorDb("Quest %u has `RewMailTemplateId` = %u but mail template  %u does not exist, quest will not have a mail reward.",
                                qinfo->GetQuestId(), qinfo->RewMailTemplateId, qinfo->RewMailTemplateId);
                qinfo->RewMailTemplateId = 0;               // no mail will send to player
                qinfo->RewMailDelaySecs = 0;                // no mail will send to player
            }
            else if (usedMailTemplates.find(qinfo->RewMailTemplateId) != usedMailTemplates.end())
            {
                std::map<uint32, uint32>::const_iterator used_mt_itr = usedMailTemplates.find(qinfo->RewMailTemplateId);
                sLog.outErrorDb("Quest %u has `RewMailTemplateId` = %u but mail template  %u already used for quest %u, quest will not have a mail reward.",
                                qinfo->GetQuestId(), qinfo->RewMailTemplateId, qinfo->RewMailTemplateId, used_mt_itr->second);
                qinfo->RewMailTemplateId = 0;               // no mail will send to player
                qinfo->RewMailDelaySecs = 0;                // no mail will send to player
            }
            else
            {
                usedMailTemplates[qinfo->RewMailTemplateId] = qinfo->GetQuestId();
            }
        }

        if (qinfo->NextQuestInChain)
        {
            QuestMap::iterator qNextItr = mQuestTemplates.find(qinfo->NextQuestInChain);
            if (qNextItr == mQuestTemplates.end())
            {
                sLog.outErrorDb("Quest %u has `NextQuestInChain` = %u but quest %u does not exist, quest chain will not work.",
                                qinfo->GetQuestId(), qinfo->NextQuestInChain , qinfo->NextQuestInChain);
                qinfo->NextQuestInChain = 0;
            }
            else
            {
                qNextItr->second->prevChainQuests.push_back(qinfo->GetQuestId());
            }
        }

        // fill additional data stores
        if (qinfo->PrevQuestId)
        {
            if (mQuestTemplates.find(abs(qinfo->GetPrevQuestId())) == mQuestTemplates.end())
            {
                sLog.outErrorDb("Quest %d has PrevQuestId %i, but no such quest", qinfo->GetQuestId(), qinfo->GetPrevQuestId());
            }
            else
            {
                qinfo->prevQuests.push_back(qinfo->PrevQuestId);
            }
        }

        if (qinfo->NextQuestId)
        {
            QuestMap::iterator qNextItr = mQuestTemplates.find(abs(qinfo->GetNextQuestId()));
            if (qNextItr == mQuestTemplates.end())
            {
                sLog.outErrorDb("Quest %d has NextQuestId %i, but no such quest", qinfo->GetQuestId(), qinfo->GetNextQuestId());
            }
            else
            {
                int32 signedQuestId = qinfo->NextQuestId < 0 ? -int32(qinfo->GetQuestId()) : int32(qinfo->GetQuestId());
                qNextItr->second->prevQuests.push_back(signedQuestId);
            }
        }

        if (qinfo->ExclusiveGroup)
        {
            m_ExclusiveQuestGroups.insert(ExclusiveQuestGroupsMap::value_type(qinfo->ExclusiveGroup, qinfo->GetQuestId()));
        }

        if (qinfo->LimitTime)
        {
            qinfo->SetSpecialFlag(QUEST_SPECIAL_FLAG_TIMED);
        }
    }

    // check QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT for spell with SPELL_EFFECT_QUEST_COMPLETE
    for (uint32 i = 0; i < sSpellStore.GetNumRows(); ++i)
    {
        SpellEntry const* spellInfo = sSpellStore.LookupEntry(i);
        if (!spellInfo)
        {
            continue;
        }

        for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
        {
            if (spellInfo->Effect[j] != SPELL_EFFECT_QUEST_COMPLETE)
            {
                continue;
            }

            uint32 quest_id = spellInfo->EffectMiscValue[j];

            Quest const* quest = GetQuestTemplate(quest_id);

            // some quest referenced in spells not exist (outdated spells)
            if (!quest)
            {
                continue;
            }

            if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
            {
                sLog.outErrorDb("Spell (id: %u) have SPELL_EFFECT_QUEST_COMPLETE for quest %u , but quest does not have SpecialFlags QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT (2) set. Quest SpecialFlags should be corrected to enable this objective.", spellInfo->Id, quest_id);

                // The below forced alteration has been disabled because of spell 33824 / quest 10162.
                // A startup error will still occur with proper data in quest_template, but it will be possible to sucessfully complete the quest with the expected data.

                // this will prevent quest completing without objective
                // const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);
            }
        }
    }

    sLog.outString(">> Loaded " SIZEFMTD " quests definitions", mQuestTemplates.size());
    sLog.outString();
}

void ObjectMgr::LoadQuestLocales()
{
    mQuestLocaleMap.clear();                                // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                          "`Title_loc1`,`Details_loc1`,`Objectives_loc1`,`OfferRewardText_loc1`,`RequestItemsText_loc1`,`EndText_loc1`,`ObjectiveText1_loc1`,`ObjectiveText2_loc1`,`ObjectiveText3_loc1`,`ObjectiveText4_loc1`,"
                          "`Title_loc2`,`Details_loc2`,`Objectives_loc2`,`OfferRewardText_loc2`,`RequestItemsText_loc2`,`EndText_loc2`,`ObjectiveText1_loc2`,`ObjectiveText2_loc2`,`ObjectiveText3_loc2`,`ObjectiveText4_loc2`,"
                          "`Title_loc3`,`Details_loc3`,`Objectives_loc3`,`OfferRewardText_loc3`,`RequestItemsText_loc3`,`EndText_loc3`,`ObjectiveText1_loc3`,`ObjectiveText2_loc3`,`ObjectiveText3_loc3`,`ObjectiveText4_loc3`,"
                          "`Title_loc4`,`Details_loc4`,`Objectives_loc4`,`OfferRewardText_loc4`,`RequestItemsText_loc4`,`EndText_loc4`,`ObjectiveText1_loc4`,`ObjectiveText2_loc4`,`ObjectiveText3_loc4`,`ObjectiveText4_loc4`,"
                          "`Title_loc5`,`Details_loc5`,`Objectives_loc5`,`OfferRewardText_loc5`,`RequestItemsText_loc5`,`EndText_loc5`,`ObjectiveText1_loc5`,`ObjectiveText2_loc5`,`ObjectiveText3_loc5`,`ObjectiveText4_loc5`,"
                          "`Title_loc6`,`Details_loc6`,`Objectives_loc6`,`OfferRewardText_loc6`,`RequestItemsText_loc6`,`EndText_loc6`,`ObjectiveText1_loc6`,`ObjectiveText2_loc6`,`ObjectiveText3_loc6`,`ObjectiveText4_loc6`,"
                          "`Title_loc7`,`Details_loc7`,`Objectives_loc7`,`OfferRewardText_loc7`,`RequestItemsText_loc7`,`EndText_loc7`,`ObjectiveText1_loc7`,`ObjectiveText2_loc7`,`ObjectiveText3_loc7`,`ObjectiveText4_loc7`,"
                          "`Title_loc8`,`Details_loc8`,`Objectives_loc8`,`OfferRewardText_loc8`,`RequestItemsText_loc8`,`EndText_loc8`,`ObjectiveText1_loc8`,`ObjectiveText2_loc8`,`ObjectiveText3_loc8`,`ObjectiveText4_loc8`"
                          " FROM `locales_quest`"
                                             );

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 Quest locale strings. DB table `locales_quest` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetQuestTemplate(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_quest` has data for nonexistent quest entry %u, skipped.", entry);
            continue;
        }

        QuestLocale& data = mQuestLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[1 + 10 * (i - 1)].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Title.size() <= idx)
                    {
                        data.Title.resize(idx + 1);
                    }

                    data.Title[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Details.size() <= idx)
                    {
                        data.Details.resize(idx + 1);
                    }

                    data.Details[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 2].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Objectives.size() <= idx)
                    {
                        data.Objectives.resize(idx + 1);
                    }

                    data.Objectives[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 3].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.OfferRewardText.size() <= idx)
                    {
                        data.OfferRewardText.resize(idx + 1);
                    }

                    data.OfferRewardText[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 4].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.RequestItemsText.size() <= idx)
                    {
                        data.RequestItemsText.resize(idx + 1);
                    }

                    data.RequestItemsText[idx] = str;
                }
            }
            str = fields[1 + 10 * (i - 1) + 5].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.EndText.size() <= idx)
                    {
                        data.EndText.resize(idx + 1);
                    }

                    data.EndText[idx] = str;
                }
            }
            for (int k = 0; k < 4; ++k)
            {
                str = fields[1 + 10 * (i - 1) + 6 + k].GetCppString();
                if (!str.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.ObjectiveText[k].size() <= idx)
                        {
                            data.ObjectiveText[k].resize(idx + 1);
                        }

                        data.ObjectiveText[k][idx] = str;
                    }
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %lu Quest locale strings", (unsigned long)mQuestLocaleMap.size());
}

void ObjectMgr::LoadPetCreateSpells()
{
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `Spell1`, `Spell2`, `Spell3`, `Spell4` FROM `petcreateinfo_spell`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded 0 pet create spells");
        // sLog.outErrorDb("`petcreateinfo_spell` table is empty!");
        return;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());

    mPetCreateSpell.clear();

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 creature_id = fields[0].GetUInt32();

        if (!creature_id)
        {
            sLog.outErrorDb("Creature id %u listed in `petcreateinfo_spell` not exist.", creature_id);
            continue;
        }

        CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(creature_id);
        if (!cInfo)
        {
            sLog.outErrorDb("Creature id %u listed in `petcreateinfo_spell` not exist.", creature_id);
            continue;
        }

        if (CreatureSpellDataEntry const* petSpellEntry = cInfo->PetSpellDataId ? sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId) : NULL)
        {
            sLog.outErrorDb("Creature id %u listed in `petcreateinfo_spell` have set `PetSpellDataId` field and will use its instead, skip.", creature_id);
            continue;
        }

        PetCreateSpellEntry PetCreateSpell;

        bool have_spell = false;
        bool have_spell_db = false;
        for (int i = 0; i < 4; ++i)
        {
            PetCreateSpell.spellid[i] = fields[i + 1].GetUInt32();

            if (!PetCreateSpell.spellid[i])
            {
                continue;
            }

            have_spell_db = true;

            SpellEntry const* i_spell = sSpellStore.LookupEntry(PetCreateSpell.spellid[i]);
            if (!i_spell)
            {
                sLog.outErrorDb("Spell %u listed in `petcreateinfo_spell` does not exist", PetCreateSpell.spellid[i]);
                PetCreateSpell.spellid[i] = 0;
                continue;
            }

            have_spell = true;
        }

        if (!have_spell_db)
        {
            sLog.outErrorDb("Creature %u listed in `petcreateinfo_spell` have only 0 spell data, why it listed?", creature_id);
            continue;
        }

        if (!have_spell)
        {
            continue;
        }

        mPetCreateSpell[creature_id] = PetCreateSpell;
        ++count;
    }
    while (result->NextRow());

    delete result;

    // cache spell->learn spell map for use in next loop
    std::map<uint32, uint32> learnCache;
    for (uint32 spell_id = 1; spell_id < sSpellStore.GetNumRows(); ++spell_id)
    {
        SpellEntry const* spellproto = sSpellStore.LookupEntry(spell_id);
        if (!spellproto)
        {
            continue;
        }

        if (spellproto->Effect[0] != SPELL_EFFECT_LEARN_SPELL && spellproto->Effect[0] != SPELL_EFFECT_LEARN_PET_SPELL)
        {
            continue;
        }

        if (!spellproto->EffectTriggerSpell[0])
        {
            continue;
        }

        learnCache[spellproto->EffectTriggerSpell[0]] = spellproto->Id;
    }

    // fill data from DBC as more correct source if available
    uint32 dcount = 0;
    for (uint32 cr_id = 1; cr_id < sCreatureStorage.GetMaxEntry(); ++cr_id)
    {
        CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(cr_id);
        if (!cInfo)
        {
            continue;
        }

        CreatureSpellDataEntry const* petSpellEntry = cInfo->PetSpellDataId ? sCreatureSpellDataStore.LookupEntry(cInfo->PetSpellDataId) : NULL;
        if (!petSpellEntry)
        {
            continue;
        }

        PetCreateSpellEntry PetCreateSpell;
        for (int i = 0; i < MAX_CREATURE_SPELL_DATA_SLOT; ++i)
        {
            uint32 petspell_id = petSpellEntry->spellId[i];
            if (petspell_id)
            {
                // in dbc stored spell for pet use, but for teaching work we need learn spell ids
                std::map<uint32, uint32>::const_iterator cache_itr = learnCache.find(petspell_id);
                if (cache_itr != learnCache.end())
                {
                    petspell_id = cache_itr->second;
                }
            }

            PetCreateSpell.spellid[i] = petspell_id;
        }

        mPetCreateSpell[cr_id] = PetCreateSpell;
        ++dcount;
    }

    sLog.outString();
    sLog.outString(">> Loaded %u pet create spells from table and %u from DBC", count, dcount);
}

void ObjectMgr::LoadItemTexts()
{
    QueryResult* result = CharacterDatabase.Query("SELECT `id`, `text` FROM `item_text`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString();
        sLog.outString(">> Loaded %u item pages", count);
        return;
    }

    BarGoLink bar(result->GetRowCount());

    Field* fields;
    do
    {
        bar.step();

        fields = result->Fetch();

        mItemTexts[ fields[0].GetUInt32()] = fields[1].GetCppString();

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u item texts", count);
    sLog.outString();
}

void ObjectMgr::LoadPageTexts()
{
    sPageTextStore.Load();
    sLog.outString(">> Loaded %u page texts", sPageTextStore.GetRecordCount());
    sLog.outString();

    for (uint32 i = 1; i < sPageTextStore.GetMaxEntry(); ++i)
    {
        // check data correctness
        PageText const* page = sPageTextStore.LookupEntry<PageText>(i);
        if (!page)
        {
            continue;
        }

        if (page->Next_Page && !sPageTextStore.LookupEntry<PageText>(page->Next_Page))
        {
            sLog.outErrorDb("Page text (Id: %u) has not existing next page (Id:%u)", i, page->Next_Page);
            continue;
        }

        // detect circular reference
        std::set<uint32> checkedPages;
        for (PageText const* pageItr = page; pageItr; pageItr = sPageTextStore.LookupEntry<PageText>(pageItr->Next_Page))
        {
            if (!pageItr->Next_Page)
            {
                break;
            }
            checkedPages.insert(pageItr->Page_ID);
            if (checkedPages.find(pageItr->Next_Page) != checkedPages.end())
            {
                std::ostringstream ss;
                ss << "The text page(s) ";
                for (std::set<uint32>::iterator itr = checkedPages.begin(); itr != checkedPages.end(); ++itr)
                {
                    ss << *itr << " ";
                }
                ss << "create(s) a circular reference, which can cause the server to freeze. Changing Next_Page of page "
                   << pageItr->Page_ID << " to 0";
                sLog.outErrorDb("%s", ss.str().c_str());
                const_cast<PageText*>(pageItr)->Next_Page = 0;
                break;
            }
        }
    }
}

void ObjectMgr::LoadPageTextLocales()
{
    mPageTextLocaleMap.clear();                             // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`text_loc1`,`text_loc2`,`text_loc3`,`text_loc4`,`text_loc5`,`text_loc6`,`text_loc7`,`text_loc8` FROM `locales_page_text`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 PageText locale strings. DB table `locales_page_text` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!sPageTextStore.LookupEntry<PageText>(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_page_text` has data for nonexistent page text entry %u, skipped.", entry);
            continue;
        }

        PageTextLocale& data = mPageTextLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (str.empty())
            {
                continue;
            }

            int idx = GetOrNewIndexForLocale(LocaleConstant(i));
            if (idx >= 0)
            {
                if ((int32)data.Text.size() <= idx)
                {
                    data.Text.resize(idx + 1);
                }

                data.Text[idx] = str;
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " PageText locale strings", mPageTextLocaleMap.size());
    sLog.outString();
}

struct SQLInstanceLoader : public SQLStorageLoaderBase<SQLInstanceLoader, SQLStorage>
{
    template<class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

void ObjectMgr::LoadInstanceTemplate()
{
    SQLInstanceLoader loader;
    loader.Load(sInstanceTemplate);

    for (uint32 i = 0; i < sInstanceTemplate.GetMaxEntry(); ++i)
    {
        InstanceTemplate const* temp = GetInstanceTemplate(i);
        if (!temp)
        {
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(temp->map);
        if (!mapEntry)
        {
            sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad mapid %d for template!", temp->map);
            sInstanceTemplate.EraseEntry(i);
            continue;
        }

        if (!mapEntry->Instanceable())
        {
            sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: non-instanceable mapid %d for template!", temp->map);
            sInstanceTemplate.EraseEntry(i);
            continue;
        }

        if (temp->parent > 0)
        {
            // check existence
            MapEntry const* parentEntry = sMapStore.LookupEntry(temp->parent);
            if (!parentEntry)
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad parent map id %u for instance template %d template!",
                                temp->parent, temp->map);
                const_cast<InstanceTemplate*>(temp)->parent = 0;
                continue;
            }

            if (parentEntry->IsContinent())
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: parent point to continent map id %u for instance template %d template, ignored, need be set only for non-continent parents!",
                                parentEntry->MapID, temp->map);
                const_cast<InstanceTemplate*>(temp)->parent = 0;
                continue;
            }
        }

        // if ghost entrance coordinates provided, can't be not exist for instance without ground entrance
        if (temp->ghostEntranceMap >= 0)
        {
            if (!MapManager::IsValidMapCoord(temp->ghostEntranceMap, temp->ghostEntranceX, temp->ghostEntranceY))
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: ghost entrance coordinates invalid for instance template %d template, ignored, need be set only for non-continent parents!", temp->map);
                sInstanceTemplate.EraseEntry(i);
                continue;
            }

            MapEntry const* ghostEntry = sMapStore.LookupEntry(temp->ghostEntranceMap);
            if (!ghostEntry)
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: bad ghost entrance map id %u for instance template %d template!", ghostEntry->MapID, temp->map);
                sInstanceTemplate.EraseEntry(i);
                continue;
            }

            if (!ghostEntry->IsContinent())
            {
                sLog.outErrorDb("ObjectMgr::LoadInstanceTemplate: ghost entrance not at continent map id %u for instance template %d template, ignored, need be set only for non-continent parents!", ghostEntry->MapID, temp->map);
                sInstanceTemplate.EraseEntry(i);
                continue;
            }
        }

        // the reset_delay must be at least one day
        if (temp->reset_delay)
        {
            const_cast<InstanceTemplate*>(temp)->reset_delay = std::max((uint32)1, (uint32)(temp->reset_delay * sWorld.getConfig(CONFIG_FLOAT_RATE_INSTANCE_RESET_TIME)));
        }
    }

    sLog.outString(">> Loaded %u Instance Template definitions", sInstanceTemplate.GetRecordCount());
    sLog.outString();
}

struct SQLWorldLoader : public SQLStorageLoaderBase<SQLWorldLoader, SQLStorage>
{
    template<class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

void ObjectMgr::LoadConditions()
{
    SQLWorldLoader loader;
    loader.Load(sConditionStorage);

    for (uint32 i = 0; i < sConditionStorage.GetMaxEntry(); ++i)
    {
        const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(i);
        if (!condition)
        {
            continue;
        }

        if (!condition->IsValid())
        {
            sLog.outErrorDb("ObjectMgr::LoadConditions: invalid condition_entry %u, skip", i);
            sConditionStorage.EraseEntry(i);
            continue;
        }
    }

    sLog.outString(">> Loaded %u Condition definitions", sConditionStorage.GetRecordCount());
    sLog.outString();
}

GossipText const* ObjectMgr::GetGossipText(uint32 Text_ID) const
{
    GossipTextMap::const_iterator itr = mGossipText.find(Text_ID);
    if (itr != mGossipText.end())
    {
        return &itr->second;
    }
    return NULL;
}

void ObjectMgr::LoadGossipText()
{
    QueryResult* result = WorldDatabase.Query("SELECT * FROM `npc_text`");

    int count = 0;
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();

        sLog.outString(">> Loaded %u npc texts", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        int cic = 0;

        Field* fields = result->Fetch();

        bar.step();

        uint32 Text_ID    = fields[cic++].GetUInt32();
        if (!Text_ID)
        {
            sLog.outErrorDb("Table `npc_text` has record wit reserved id 0, ignore.");
            continue;
        }

        GossipText& gText = mGossipText[Text_ID];

        for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
        {
            gText.Options[i].Text_0           = fields[cic++].GetCppString();
            gText.Options[i].Text_1           = fields[cic++].GetCppString();

            gText.Options[i].Language         = fields[cic++].GetUInt32();
            gText.Options[i].Probability      = fields[cic++].GetFloat();

            for (int j = 0; j < 3; ++j)
            {
                gText.Options[i].Emotes[j]._Delay  = fields[cic++].GetUInt32();
                gText.Options[i].Emotes[j]._Emote  = fields[cic++].GetUInt32();
            }
        }
    }
    while (result->NextRow());

    sLog.outString(">> Loaded %u npc texts", count);
    sLog.outString();
    delete result;
}

void ObjectMgr::LoadGossipTextLocales()
{
    mNpcTextLocaleMap.clear();                              // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                          "`Text0_0_loc1`,`Text0_1_loc1`,`Text1_0_loc1`,`Text1_1_loc1`,`Text2_0_loc1`,`Text2_1_loc1`,`Text3_0_loc1`,`Text3_1_loc1`,`Text4_0_loc1`,`Text4_1_loc1`,`Text5_0_loc1`,`Text5_1_loc1`,`Text6_0_loc1`,`Text6_1_loc1`,`Text7_0_loc1`,`Text7_1_loc1`,"
                          "`Text0_0_loc2`,`Text0_1_loc2`,`Text1_0_loc2`,`Text1_1_loc2`,`Text2_0_loc2`,`Text2_1_loc2`,`Text3_0_loc2`,`Text3_1_loc1`,`Text4_0_loc2`,`Text4_1_loc2`,`Text5_0_loc2`,`Text5_1_loc2`,`Text6_0_loc2`,`Text6_1_loc2`,`Text7_0_loc2`,`Text7_1_loc2`,"
                          "`Text0_0_loc3`,`Text0_1_loc3`,`Text1_0_loc3`,`Text1_1_loc3`,`Text2_0_loc3`,`Text2_1_loc3`,`Text3_0_loc3`,`Text3_1_loc1`,`Text4_0_loc3`,`Text4_1_loc3`,`Text5_0_loc3`,`Text5_1_loc3`,`Text6_0_loc3`,`Text6_1_loc3`,`Text7_0_loc3`,`Text7_1_loc3`,"
                          "`Text0_0_loc4`,`Text0_1_loc4`,`Text1_0_loc4`,`Text1_1_loc4`,`Text2_0_loc4`,`Text2_1_loc4`,`Text3_0_loc4`,`Text3_1_loc1`,`Text4_0_loc4`,`Text4_1_loc4`,`Text5_0_loc4`,`Text5_1_loc4`,`Text6_0_loc4`,`Text6_1_loc4`,`Text7_0_loc4`,`Text7_1_loc4`,"
                          "`Text0_0_loc5`,`Text0_1_loc5`,`Text1_0_loc5`,`Text1_1_loc5`,`Text2_0_loc5`,`Text2_1_loc5`,`Text3_0_loc5`,`Text3_1_loc1`,`Text4_0_loc5`,`Text4_1_loc5`,`Text5_0_loc5`,`Text5_1_loc5`,`Text6_0_loc5`,`Text6_1_loc5`,`Text7_0_loc5`,`Text7_1_loc5`,"
                          "`Text0_0_loc6`,`Text0_1_loc6`,`Text1_0_loc6`,`Text1_1_loc6`,`Text2_0_loc6`,`Text2_1_loc6`,`Text3_0_loc6`,`Text3_1_loc1`,`Text4_0_loc6`,`Text4_1_loc6`,`Text5_0_loc6`,`Text5_1_loc6`,`Text6_0_loc6`,`Text6_1_loc6`,`Text7_0_loc6`,`Text7_1_loc6`,"
                          "`Text0_0_loc7`,`Text0_1_loc7`,`Text1_0_loc7`,`Text1_1_loc7`,`Text2_0_loc7`,`Text2_1_loc7`,`Text3_0_loc7`,`Text3_1_loc1`,`Text4_0_loc7`,`Text4_1_loc7`,`Text5_0_loc7`,`Text5_1_loc7`,`Text6_0_loc7`,`Text6_1_loc7`,`Text7_0_loc7`,`Text7_1_loc7`, "
                          "`Text0_0_loc8`,`Text0_1_loc8`,`Text1_0_loc8`,`Text1_1_loc8`,`Text2_0_loc8`,`Text2_1_loc8`,`Text3_0_loc8`,`Text3_1_loc1`,`Text4_0_loc8`,`Text4_1_loc8`,`Text5_0_loc8`,`Text5_1_loc8`,`Text6_0_loc8`,`Text6_1_loc8`,`Text7_0_loc8`,`Text7_1_loc8` "
                          " FROM `locales_npc_text`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 Quest locale strings. DB table `locales_npc_text` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetGossipText(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_npc_text` has data for nonexistent gossip text entry %u, skipped.", entry);
            continue;
        }

        NpcTextLocale& data = mNpcTextLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            for (int j = 0; j < 8; ++j)
            {
                std::string str0 = fields[1 + 8 * 2 * (i - 1) + 2 * j].GetCppString();
                if (!str0.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_0[j].size() <= idx)
                        {
                            data.Text_0[j].resize(idx + 1);
                        }

                        data.Text_0[j][idx] = str0;
                    }
                }
                std::string str1 = fields[1 + 8 * 2 * (i - 1) + 2 * j + 1].GetCppString();
                if (!str1.empty())
                {
                    int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                    if (idx >= 0)
                    {
                        if ((int32)data.Text_1[j].size() <= idx)
                        {
                            data.Text_1[j].resize(idx + 1);
                        }

                        data.Text_1[j][idx] = str1;
                    }
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " NpcText locale strings", mNpcTextLocaleMap.size());
    sLog.outString();
}

// not very fast function but it is called only once a day, or on starting-up
/// @param serverUp true if the server is already running, false when the server is started
void ObjectMgr::ReturnOrDeleteOldMails(bool serverUp)
{
    time_t basetime = time(NULL);
    DEBUG_LOG("Returning mails current time: hour: %d, minute: %d, second: %d ", localtime(&basetime)->tm_hour, localtime(&basetime)->tm_min, localtime(&basetime)->tm_sec);
    // delete all old mails without item and without body immediately, if starting server
    if (!serverUp)
    {
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE `expire_time` < '" UI64FMTD "' AND `has_items` = '0' AND `itemTextId` = 0", (uint64)basetime);
    }
    //                                                     0  1           2      3        4          5         6           7   8       9
    QueryResult* result = CharacterDatabase.PQuery("SELECT `id`,`messageType`,`sender`,`receiver`,`itemTextId`,`has_items`,`expire_time`,`cod`,`checked`,`mailTemplateId` FROM `mail` WHERE `expire_time` < '" UI64FMTD "'", (uint64)basetime);
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Only expired mails (need to be return or delete) or DB table `mail` is empty.");
        sLog.outString();
        return;                                             // any mails need to be returned or deleted
    }

    // std::ostringstream delitems, delmails; // will be here for optimization
    // bool deletemail = false, deleteitem = false;
    // delitems << "DELETE FROM `item_instance` WHERE `guid` IN ( ";
    // delmails << "DELETE FROM `mail` WHERE `id` IN ( "

    BarGoLink bar(result->GetRowCount());
    uint32 count = 0;
    Field* fields;

    do
    {
        bar.step();

        fields = result->Fetch();
        Mail* m = new Mail;
        m->messageID = fields[0].GetUInt32();
        m->messageType = fields[1].GetUInt8();
        m->sender = fields[2].GetUInt32();
        m->receiverGuid = ObjectGuid(HIGHGUID_PLAYER, fields[3].GetUInt32());
        bool has_items = fields[5].GetBool();
        m->expire_time = (time_t)fields[6].GetUInt64();
        m->deliver_time = 0;
        m->COD = fields[7].GetUInt32();
        m->checked = fields[8].GetUInt32();
        m->mailTemplateId = fields[9].GetInt16();

        Player* pl = 0;
        if (serverUp)
        {
            pl = GetPlayer(m->receiverGuid);
        }
        if (pl)
        {
            // this code will run very improbably (the time is between 4 and 5 am, in game is online a player, who has old mail
            // his in mailbox and he has already listed his mails )
            delete m;
            continue;
        }
        // delete or return mail:
        if (has_items)
        {
            QueryResult* resultItems = CharacterDatabase.PQuery("SELECT `item_guid`,`item_template` FROM `mail_items` WHERE `mail_id`='%u'", m->messageID);
            if (resultItems)
            {
                do
                {
                    Field* fields2 = resultItems->Fetch();

                    uint32 item_guid_low = fields2[0].GetUInt32();
                    uint32 item_template = fields2[1].GetUInt32();

                    m->AddItem(item_guid_low, item_template);
                }
                while (resultItems->NextRow());

                delete resultItems;
            }
            // if it is mail from non-player, or if it's already return mail, it shouldn't be returned, but deleted
            if (m->messageType != MAIL_NORMAL || (m->checked & (MAIL_CHECK_MASK_COD_PAYMENT | MAIL_CHECK_MASK_RETURNED)))
            {
                // mail open and then not returned
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    CharacterDatabase.PExecute("DELETE FROM `item_instance` WHERE `guid` = '%u'", itr2->item_guid);
                }
            }
            else
            {
                // mail will be returned:
                CharacterDatabase.PExecute("UPDATE `mail` SET `sender` = '%u', `receiver` = '%u', `expire_time` = '" UI64FMTD "', `deliver_time` = '" UI64FMTD "', `cod` = '0', `checked` = '%u' WHERE `id` = '%u'",
                                           m->receiverGuid.GetCounter(), m->sender, (uint64)(basetime + 30 * DAY), (uint64)basetime, MAIL_CHECK_MASK_RETURNED, m->messageID);
                for (MailItemInfoVec::iterator itr2 = m->items.begin(); itr2 != m->items.end(); ++itr2)
                {
                    // update receiver in mail items for its proper delivery, and in instance_item for avoid lost item at sender delete
                    CharacterDatabase.PExecute("UPDATE `mail_items` SET `receiver` = %u WHERE `item_guid` = '%u'", m->sender, itr2->item_guid);
                    CharacterDatabase.PExecute("UPDATE `item_instance` SET `owner_guid` = %u WHERE `guid` = '%u'", m->sender, itr2->item_guid);
                }
                delete m;
                continue;
            }
        }

        if (m->itemTextId)
        {
            CharacterDatabase.PExecute("DELETE FROM `item_text` WHERE `id` = '%u'", m->itemTextId);
        }

        // deletemail = true;
        // delmails << m->messageID << ", ";
        CharacterDatabase.PExecute("DELETE FROM `mail` WHERE id` = '%u'", m->messageID);
        delete m;
        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u mails", count);
    sLog.outString();
}

void ObjectMgr::LoadQuestAreaTriggers()
{
    mQuestAreaTriggerMap.clear();                           // need for reload case

    QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d", QA_AREATRIGGER);

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u quest trigger points", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 trigger_ID = fields[0].GetUInt32();
        uint32 quest_ID   = fields[1].GetUInt32();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `quest_relations` has area trigger (ID: %u) not listed in `AreaTrigger.dbc`.", trigger_ID);
            continue;
        }

        Quest const* quest = GetQuestTemplate(quest_ID);
        if (!quest)
        {
            sLog.outErrorDb("Table `quest_relations` has record (id: %u) for not existing quest %u", trigger_ID, quest_ID);
            continue;
        }

        if (!quest->HasSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT))
        {
            sLog.outErrorDb("Table `quest_relations` has record (id: %u) for not quest %u, but quest not have flag QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT. Trigger or quest flags must be fixed, quest modified to require objective.", trigger_ID, quest_ID);

            // this will prevent quest completing without objective
            const_cast<Quest*>(quest)->SetSpecialFlag(QUEST_SPECIAL_FLAG_EXPLORATION_OR_EVENT);

            // continue; - quest modified to required objective and trigger can be allowed.
        }

        mQuestAreaTriggerMap[trigger_ID] = quest_ID;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u quest trigger points", count);
    sLog.outString();
}

void ObjectMgr::LoadTavernAreaTriggers()
{
    mTavernAreaTriggerSet.clear();                          // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `id` FROM `areatrigger_tavern`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u tavern triggers", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 Trigger_ID      = fields[0].GetUInt32();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `areatrigger_tavern` has area trigger (ID:%u) not listed in `AreaTrigger.dbc`.", Trigger_ID);
            continue;
        }

        mTavernAreaTriggerSet.insert(Trigger_ID);
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u tavern triggers", count);
    sLog.outString();
}

uint32 ObjectMgr::GetNearestTaxiNode(float x, float y, float z, uint32 mapid, Team team)
{
    bool found = false;
    float dist;
    uint32 id = 0;

    for (uint32 i = 1; i < sTaxiNodesStore.GetNumRows(); ++i)
    {
        TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(i);
        if (!node || node->map_id != mapid || !node->MountCreatureID[team == ALLIANCE ? 1 : 0])
        {
            continue;
        }

        uint8  field   = (uint8)((i - 1) / 32);
        uint32 submask = 1 << ((i - 1) % 32);

        // skip not taxi network nodes
        if ((sTaxiNodesMask[field] & submask) == 0)
        {
            continue;
        }

        float dist2 = (node->x - x) * (node->x - x) + (node->y - y) * (node->y - y) + (node->z - z) * (node->z - z);
        if (found)
        {
            if (dist2 < dist)
            {
                dist = dist2;
                id = i;
            }
        }
        else
        {
            found = true;
            dist = dist2;
            id = i;
        }
    }

    return id;
}

void ObjectMgr::GetTaxiPath(uint32 source, uint32 destination, uint32& path, uint32& cost)
{
    TaxiPathSetBySource::iterator src_i = sTaxiPathSetBySource.find(source);
    if (src_i == sTaxiPathSetBySource.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    TaxiPathSetForSource& pathSet = src_i->second;

    TaxiPathSetForSource::iterator dest_i = pathSet.find(destination);
    if (dest_i == pathSet.end())
    {
        path = 0;
        cost = 0;
        return;
    }

    cost = dest_i->second.price;
    path = dest_i->second.ID;
}

uint32 ObjectMgr::GetTaxiMountDisplayId(uint32 id, Team team, bool allowed_alt_team /* = false */)
{
    uint16 mount_entry = 0;

    // select mount creature id
    TaxiNodesEntry const* node = sTaxiNodesStore.LookupEntry(id);
    if (node)
    {
        if (team == ALLIANCE)
        {
            mount_entry = node->MountCreatureID[1];
            if (!mount_entry && allowed_alt_team)
            {
                mount_entry = node->MountCreatureID[0];
            }
        }
        else if (team == HORDE)
        {
            mount_entry = node->MountCreatureID[0];

            if (!mount_entry && allowed_alt_team)
            {
                mount_entry = node->MountCreatureID[1];
            }
        }
    }

    CreatureInfo const* mount_info = GetCreatureTemplate(mount_entry);
    if (!mount_info)
    {
        return 0;
    }

    uint16 mount_id = Creature::ChooseDisplayId(mount_info);
    if (!mount_id)
    {
        return 0;
    }

    CreatureModelInfo const* minfo = GetCreatureModelRandomGender(mount_id);
    if (minfo)
    {
        mount_id = minfo->modelid;
    }

    return mount_id;
}

void ObjectMgr::LoadGraveyardZones()
{
    mGraveYardMap.clear();                                  // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `id`,`ghost_zone`,`faction` FROM `game_graveyard_zone`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u graveyard-zone links", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        ++count;
        bar.step();

        Field* fields = result->Fetch();

        uint32 safeLocId = fields[0].GetUInt32();
        uint32 zoneId = fields[1].GetUInt32();
        uint32 team   = fields[2].GetUInt32();

        WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(safeLocId);
        if (!entry)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for not existing graveyard (WorldSafeLocs.dbc id) %u, skipped.", safeLocId);
            continue;
        }

        AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(zoneId);
        if (!areaEntry)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for not existing zone id (%u), skipped.", zoneId);
            continue;
        }

        if (areaEntry->zone != 0)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record subzone id (%u) instead of zone, skipped.", zoneId);
            continue;
        }

        if (team != TEAM_BOTH_ALLOWED && team != HORDE && team != ALLIANCE)
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has record for non player faction (%u), skipped.", team);
            continue;
        }

        if (!AddGraveYardLink(safeLocId, zoneId, Team(team), false))
        {
            sLog.outErrorDb("Table `game_graveyard_zone` has a duplicate record for Graveyard (ID: %u) and Zone (ID: %u), skipped.", safeLocId, zoneId);
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u graveyard-zone links", count);
    sLog.outString();
}

WorldSafeLocsEntry const* ObjectMgr::GetClosestGraveYard(float x, float y, float z, uint32 MapId, Team team)
{
    // search for zone associated closest graveyard
    uint32 zoneId = sTerrainMgr.GetZoneId(MapId, x, y, z);

    // Simulate std. algorithm:
    //   found some graveyard associated to (ghost_zone,ghost_map)
    //
    //   if mapId == graveyard.mapId (ghost in plain zone or city or battleground) and search graveyard at same map
    //     then check faction
    //   if mapId != graveyard.mapId (ghost in instance) and search any graveyard associated
    //     then check faction
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    if (bounds.first == bounds.second)
    {
        sLog.outErrorDb("Table `game_graveyard_zone` incomplete: Zone %u Team %u does not have a linked graveyard.", zoneId, uint32(team));
        return NULL;
    }

    // at corpse map
    bool foundNear = false;
    float distNear;
    WorldSafeLocsEntry const* entryNear = NULL;

    // at entrance map for corpse map
    bool foundEntr = false;
    float distEntr;
    WorldSafeLocsEntry const* entryEntr = NULL;

    // some where other
    WorldSafeLocsEntry const* entryFar = NULL;

    InstanceTemplate const* tempEntry = GetInstanceTemplate(MapId);

    for (GraveYardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData const& data = itr->second;

        // Checked on load
        WorldSafeLocsEntry const* entry = sWorldSafeLocsStore.LookupEntry(data.safeLocId);

        // skip enemy faction graveyard
        // team == TEAM_BOTH_ALLOWED case can be at call from .neargrave
        // TEAM_INVALID != team for all teams
        if (data.team != TEAM_BOTH_ALLOWED && data.team != team && team != TEAM_BOTH_ALLOWED)
        {
            continue;
        }

        // find now nearest graveyard at other (continent) map
        if (MapId != entry->map_id)
        {
            // if find graveyard at different map from where entrance placed (or no entrance data), use any first
            if (!tempEntry ||
                tempEntry->ghostEntranceMap < 0 ||
                uint32(tempEntry->ghostEntranceMap) != entry->map_id ||
                (tempEntry->ghostEntranceX == 0.0f && tempEntry->ghostEntranceY == 0.0f))
            {
                // not have any coordinates for check distance anyway
                entryFar = entry;
                continue;
            }

            // at entrance map calculate distance (2D);
            float dist2 = (entry->x - tempEntry->ghostEntranceX) * (entry->x - tempEntry->ghostEntranceX)
                          + (entry->y - tempEntry->ghostEntranceY) * (entry->y - tempEntry->ghostEntranceY);
            if (foundEntr)
            {
                if (dist2 < distEntr)
                {
                    distEntr = dist2;
                    entryEntr = entry;
                }
            }
            else
            {
                foundEntr = true;
                distEntr = dist2;
                entryEntr = entry;
            }
        }
        // find now nearest graveyard at same map
        else
        {
            float dist2 = (entry->x - x) * (entry->x - x) + (entry->y - y) * (entry->y - y) + (entry->z - z) * (entry->z - z);
            if (foundNear)
            {
                if (dist2 < distNear)
                {
                    distNear = dist2;
                    entryNear = entry;
                }
            }
            else
            {
                foundNear = true;
                distNear = dist2;
                entryNear = entry;
            }
        }
    }

    if (entryNear)
    {
        return entryNear;
    }

    if (entryEntr)
    {
        return entryEntr;
    }

    return entryFar;
}

GraveYardData const* ObjectMgr::FindGraveYardData(uint32 id, uint32 zoneId) const
{
    GraveYardMapBounds bounds = mGraveYardMap.equal_range(zoneId);

    for (GraveYardMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second.safeLocId == id)
        {
            return &itr->second;
        }
    }

    return NULL;
}

bool ObjectMgr::AddGraveYardLink(uint32 id, uint32 zoneId, Team team, bool inDB)
{
    if (FindGraveYardData(id, zoneId))                      // This ensures that (safeLoc)Id,  zoneId is unique in mGraveYardMap
    {
        return false;
    }

    // add link to loaded data
    GraveYardData data;
    data.safeLocId = id;
    data.team = team;

    mGraveYardMap.insert(GraveYardMap::value_type(zoneId, data));

    // add link to DB
    if (inDB)
    {
        WorldDatabase.PExecuteLog("INSERT INTO `game_graveyard_zone` (`id`,`ghost_zone`,`faction`) VALUES ('%u', '%u','%u')", id, zoneId, uint32(team));
    }

    return true;
}

void ObjectMgr::SetGraveYardLinkTeam(uint32 id, uint32 zoneId, Team team)
{
    std::pair<GraveYardMap::iterator, GraveYardMap::iterator> bounds = mGraveYardMap.equal_range(zoneId);

    for (GraveYardMap::iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        GraveYardData& data = itr->second;

        // skip not matching safezone id
        if (data.safeLocId != id)
        {
            continue;
        }

        data.team = team;                                   // Validate link
        return;
    }

    if (team == TEAM_INVALID)
    {
        return;
    }

    // Link expected but not exist.
    sLog.outErrorDb("ObjectMgr::SetGraveYardLinkTeam called for safeLoc %u, zoneId %u, but no graveyard link for this found in database.", id, zoneId);
    AddGraveYardLink(id, zoneId, team);                     // Add to prevent further error message and correct mechanismn
}

void ObjectMgr::LoadAreaTriggerTeleports()
{
    mAreaTriggers.clear();                                  // need for reload case

    uint32 count = 0;

    //                                                0   1           2                  3                  4                  5                   6                  7                  8                  9
    QueryResult* result = WorldDatabase.Query("SELECT `id`, `target_map`, `target_position_x`, `target_position_y`, `target_position_z`, `target_orientation`, `condition_id` FROM `areatrigger_teleport`");
    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u area trigger teleport definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();

        bar.step();

        ++count;

        uint32 Trigger_ID = fields[0].GetUInt32();

        AreaTrigger at;

        at.target_mapId         = fields[1].GetUInt32();
        at.target_X             = fields[2].GetFloat();
        at.target_Y             = fields[3].GetFloat();
        at.target_Z             = fields[4].GetFloat();
        at.target_Orientation   = fields[5].GetFloat();
        at.condition            = fields[6].GetUInt16();

        AreaTriggerEntry const* atEntry = sAreaTriggerStore.LookupEntry(Trigger_ID);
        if (!atEntry)
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has area trigger (ID:%u) not listed in `AreaTrigger.dbc`.", Trigger_ID);
            continue;
        }

        MapEntry const* mapEntry = sMapStore.LookupEntry(at.target_mapId);
        if (!mapEntry)
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has nonexistent target map (ID: %u) for Area trigger (ID:%u).", at.target_mapId, Trigger_ID);
            continue;
        }

        if (at.target_X == 0 && at.target_Y == 0 && at.target_Z == 0)
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has area trigger (ID:%u) without target coordinates.", Trigger_ID);
            continue;
        }

        if (at.condition && !sConditionStorage.LookupEntry<PlayerCondition>(at.condition))
        {
            sLog.outErrorDb("Table `areatrigger_teleport` has nonexistent condition (ID:%u) for Area trigger (ID:%u).", at.condition, Trigger_ID);
            continue;
        }

        mAreaTriggers[Trigger_ID] = at;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u area trigger teleport definitions", count);
    sLog.outString();
}

/*
 * Searches for the areatrigger which teleports players out of the given map (only direct to continent)
 */
AreaTrigger const* ObjectMgr::GetGoBackTrigger(uint32 map_id) const
{
    InstanceTemplate const* temp = GetInstanceTemplate(map_id);
    if (!temp)
    {
        return NULL;
    }

    // Try to find one that teleports to the map we want to enter
    std::list<AreaTrigger const*> ghostTrigger;
    AreaTrigger const* compareTrigger = NULL;
    for (AreaTriggerMap::const_iterator itr = mAreaTriggers.begin(); itr != mAreaTriggers.end(); ++itr)
    {
        if (itr->second.target_mapId == uint32(temp->ghostEntranceMap))
        {
            ghostTrigger.push_back(&itr->second);
            // First run, only consider AreaTrigger that teleport in the proper map
            if ((!compareTrigger || itr->second.IsLessOrEqualThan(compareTrigger)) && sAreaTriggerStore.LookupEntry(itr->first)->mapid == map_id)
            {
                if (itr->second.IsMinimal())
                {
                    return &itr->second;
                }

                compareTrigger = &itr->second;
            }
        }
    }
    if (compareTrigger)
    {
        return compareTrigger;
    }

    // Second attempt: take one fitting
    for (std::list<AreaTrigger const*>::const_iterator itr = ghostTrigger.begin(); itr != ghostTrigger.end(); ++itr)
    {
        if (!compareTrigger || (*itr)->IsLessOrEqualThan(compareTrigger))
        {
            if ((*itr)->IsMinimal())
            {
                return *itr;
            }

            compareTrigger = *itr;
        }
    }
    return compareTrigger;
}

/**
 * Searches for the areatrigger which teleports players to the given map
 */
AreaTrigger const* ObjectMgr::GetMapEntranceTrigger(uint32 Map) const
{
    AreaTrigger const* compareTrigger = NULL;
    MapEntry const* mEntry = sMapStore.LookupEntry(Map);

    for (AreaTriggerMap::const_iterator itr = mAreaTriggers.begin(); itr != mAreaTriggers.end(); ++itr)
    {
        if (itr->second.target_mapId == Map)
        {
            if (mEntry->Instanceable())
            {
                // Remark that IsLessOrEqualThan is no total order, and a->IsLeQ(b) != !b->IsLeQ(a)
                if (!compareTrigger || compareTrigger->IsLessOrEqualThan(&itr->second))
                {
                    compareTrigger = &itr->second;
                }
            }
            else
            {
                if (!compareTrigger || itr->second.IsLessOrEqualThan(compareTrigger))
                {
                    if (itr->second.IsMinimal())
                    {
                        return &itr->second;
                    }

                    compareTrigger = &itr->second;
                }
            }
        }
    }
    return compareTrigger;
}

void ObjectMgr::PackGroupIds()
{
    // this routine renumbers groups in such a way so they start from 1 and go up

    // obtain set of all groups
    std::set<uint32> groupIds;

    // all valid ids are in the instance table
    // any associations to ids not in this table are assumed to be
    // cleaned already in CleanupInstances
    QueryResult* result = CharacterDatabase.Query("SELECT `groupId` FROM `groups`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            uint32 id = fields[0].GetUInt32();

            if (id == 0)
            {
                CharacterDatabase.BeginTransaction();
                CharacterDatabase.PExecute("DELETE FROM `groups` WHERE `groupId` = '%u'", id);
                CharacterDatabase.PExecute("DELETE FROM `group_member` WHERE `groupId` = '%u'", id);
                CharacterDatabase.CommitTransaction();
                continue;
            }

            groupIds.insert(id);
        }
        while (result->NextRow());
        delete result;
    }

    BarGoLink bar(groupIds.size() + 1);
    bar.step();

    uint32 groupId = 1;
    // we do assume std::set is sorted properly on integer value
    for (std::set<uint32>::iterator i = groupIds.begin(); i != groupIds.end(); ++i)
    {
        if (*i != groupId)
        {
            // remap group id
            CharacterDatabase.BeginTransaction();
            CharacterDatabase.PExecute("UPDATE `groups` SET `groupId` = '%u' WHERE `groupId` = '%u'", groupId, *i);
            CharacterDatabase.PExecute("UPDATE `group_member` SET `groupId` = '%u' WHERE `groupId` = '%u'", groupId, *i);
            CharacterDatabase.CommitTransaction();
        }

        ++groupId;
        bar.step();
    }

    m_GroupIds.Set(groupId);

    sLog.outString(">> Group Ids remapped, next group id is %u", groupId);
    sLog.outString();
}

void ObjectMgr::SetHighestGuids()
{
    QueryResult* result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `characters`");
    if (result)
    {
        m_CharGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = WorldDatabase.Query("SELECT MAX(`guid`) FROM `creature`");
    if (result)
    {
        m_FirstTemporaryCreatureGuid = (*result)[0].GetUInt32() + 1;
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `item_instance`");
    if (result)
    {
        m_ItemGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    // Cleanup other tables from nonexistent guids (>=m_hiItemGuid)
    CharacterDatabase.BeginTransaction();
    CharacterDatabase.PExecute("DELETE FROM `character_inventory` WHERE `item` >= '%u'", m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.PExecute("DELETE FROM `mail_items` WHERE `item_guid` >= '%u'", m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.PExecute("DELETE FROM `auction` WHERE `itemguid` >= '%u'", m_ItemGuids.GetNextAfterMaxUsed());
    CharacterDatabase.CommitTransaction();

    result = WorldDatabase.Query("SELECT MAX(`guid`) FROM `gameobject`");
    if (result)
    {
        m_FirstTemporaryGameObjectGuid = (*result)[0].GetUInt32() + 1;
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `auction`");
    if (result)
    {
        m_AuctionIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `mail`");
    if (result)
    {
        m_MailIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `item_text`");
    if (result)
    {
        m_ItemTextIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`guid`) FROM `corpse`");
    if (result)
    {
        m_CorpseGuids.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`guildid`) FROM `guild`");
    if (result)
    {
        m_GuildIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    result = CharacterDatabase.Query("SELECT MAX(`groupId`) FROM `groups`");
    if (result)
    {
        m_GroupIds.Set((*result)[0].GetUInt32() + 1);
        delete result;
    }

    // setup reserved ranges for static guids spawn
    m_StaticCreatureGuids.Set(m_FirstTemporaryCreatureGuid);
    m_FirstTemporaryCreatureGuid += sWorld.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_CREATURE);

    m_StaticGameObjectGuids.Set(m_FirstTemporaryGameObjectGuid);
    m_FirstTemporaryGameObjectGuid += sWorld.getConfig(CONFIG_UINT32_GUID_RESERVE_SIZE_GAMEOBJECT);
}

uint32 ObjectMgr::CreateItemText(std::string text)
{
    uint32 newItemTextId = GenerateItemTextID();
    // insert new itempage to container
    mItemTexts[ newItemTextId ] = text;
    // save new itempage
    CharacterDatabase.escape_string(text);
    // any Delete query needed, itemTextId is maximum of all ids
    std::ostringstream query;
    query << "INSERT INTO `item_text` (`id`,`text`) VALUES ( '" << newItemTextId << "', '" << text << "')";
    CharacterDatabase.Execute(query.str().c_str());         // needs to be run this way, because mail body may be more than 1024 characters
    return newItemTextId;
}

void ObjectMgr::LoadGameObjectLocales()
{
    mGameObjectLocaleMap.clear();                           // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `entry`,"
                          "`name_loc1`,`name_loc2`,`name_loc3`,`name_loc4`,`name_loc5`,`name_loc6`,`name_loc7`,`name_loc8` FROM `locales_gameobject`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 gameobject locale strings. DB table `locales_gameobject` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 entry = fields[0].GetUInt32();

        if (!GetGameObjectInfo(entry))
        {
            ERROR_DB_STRICT_LOG("Table `locales_gameobject` has data for nonexistent gameobject entry %u, skipped.", entry);
            continue;
        }

        GameObjectLocale& data = mGameObjectLocaleMap[entry];

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    if ((int32)data.Name.size() <= idx)
                    {
                        data.Name.resize(idx + 1);
                    }

                    data.Name[idx] = str;
                }
            }
        }
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded " SIZEFMTD " gameobject locale strings", mGameObjectLocaleMap.size());
    sLog.outString();
}

struct SQLGameObjectLoader : public SQLStorageLoaderBase<SQLGameObjectLoader, SQLHashStorage>
{
    template<class D>
    void convert_from_str(uint32 /*field_pos*/, char const* src, D& dst)
    {
        dst = D(sScriptMgr.GetScriptId(src));
    }
};

inline void CheckGOLockId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (sLockStore.LookupEntry(dataN))
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but lock (Id: %u) not found.",
                    goInfo->id, goInfo->type, N, dataN, dataN);
}

inline void CheckGOLinkedTrapId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(dataN))
    {
        if (trapInfo->type != GAMEOBJECT_TYPE_TRAP)
            sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but GO (Entry %u) have not GAMEOBJECT_TYPE_TRAP (%u) type.",
                            goInfo->id, goInfo->type, N, dataN, dataN, GAMEOBJECT_TYPE_TRAP);
    }
    else
        // too many error reports about nonexistent trap templates
        ERROR_DB_STRICT_LOG("Gameobject (Entry: %u GoType: %u) have data%d=%u but trap GO (Entry %u) not exist in `gameobject_template`.",
                            goInfo->id, goInfo->type, N, dataN, dataN);
}

inline void CheckGOSpellId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    if (sSpellStore.LookupEntry(dataN))
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but Spell (Entry %u) not exist.",
                    goInfo->id, goInfo->type, N, dataN, dataN);
}

inline void CheckAndFixGOChairHeightId(GameObjectInfo const* goInfo, uint32 const& dataN, uint32 N)
{
    if (dataN <= (UNIT_STAND_STATE_SIT_HIGH_CHAIR - UNIT_STAND_STATE_SIT_LOW_CHAIR))
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but correct chair height in range 0..%i.",
                    goInfo->id, goInfo->type, N, dataN, UNIT_STAND_STATE_SIT_HIGH_CHAIR - UNIT_STAND_STATE_SIT_LOW_CHAIR);

    // prevent client and server unexpected work
    const_cast<uint32&>(dataN) = 0;
}

inline void CheckGONoDamageImmuneId(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean (0/1) noDamageImmune field value.",
                    goInfo->id, goInfo->type, N, dataN);
}

inline void CheckGOConsumable(GameObjectInfo const* goInfo, uint32 dataN, uint32 N)
{
    // 0/1 correct values
    if (dataN <= 1)
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data%d=%u but expected boolean (0/1) consumable field value.",
                    goInfo->id, goInfo->type, N, dataN);
}

inline void CheckAndFixGOCaptureMinTime(GameObjectInfo const* goInfo, uint32 const& dataN, uint32 N)
{
    if (dataN > 0)
    {
        return;
    }

    sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) has data%d=%u but minTime field value must be > 0.",
                    goInfo->id, goInfo->type, N, dataN);

    // prevent division through 0 exception
    const_cast<uint32&>(dataN) = 1;
}

void ObjectMgr::LoadGameobjectInfo()
{
    SQLGameObjectLoader loader;
    loader.Load(sGOStorage);

    // some checks
    for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
    {
        GameObjectInfo const* goInfo = itr.getValue();

        if (goInfo->size <= 0.0f)                           // prevent use too small scales
        {
            ERROR_DB_STRICT_LOG("Gameobject (Entry: %u GoType: %u) have too small size=%f",
                                goInfo->id, goInfo->type, goInfo->size);
            const_cast<GameObjectInfo*>(goInfo)->size =  DEFAULT_OBJECT_SCALE;
        }

        // some GO types have unused go template, check goInfo->displayId at GO spawn data loading or ignore

        switch (goInfo->type)
        {
            case GAMEOBJECT_TYPE_DOOR:                      // 0
            {
                if (goInfo->door.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->door.lockId, 1);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->door.noDamageImmune, 3);
                break;
            }
            case GAMEOBJECT_TYPE_BUTTON:                    // 1
            {
                if (goInfo->button.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->button.lockId, 1);
                }
                if (goInfo->button.linkedTrapId)            // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->button.linkedTrapId, 3);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->button.noDamageImmune, 4);
                break;
            }
            case GAMEOBJECT_TYPE_QUESTGIVER:                // 2
            {
                if (goInfo->questgiver.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->questgiver.lockId, 0);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->questgiver.noDamageImmune, 5);
                break;
            }
            case GAMEOBJECT_TYPE_CHEST:                     // 3
            {
                if (goInfo->chest.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->chest.lockId, 0);
                }

                CheckGOConsumable(goInfo, goInfo->chest.consumable, 3);

                if (goInfo->chest.linkedTrapId)             // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->chest.linkedTrapId, 7);
                }
                break;
            }
            case GAMEOBJECT_TYPE_TRAP:                      // 6
            {
                if (goInfo->trap.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->trap.lockId, 0);
                }
                /* disable check for while, too many nonexistent spells
                if (goInfo->trap.spellId)                   // spell
                {
                    CheckGOSpellId(goInfo,goInfo->trap.spellId,3);
                }
                */
                break;
            }
            case GAMEOBJECT_TYPE_CHAIR:                     // 7
                CheckAndFixGOChairHeightId(goInfo, goInfo->chair.height, 1);
                break;
            case GAMEOBJECT_TYPE_SPELL_FOCUS:               // 8
            {
                if (goInfo->spellFocus.focusId)
                {
                    if (!sSpellFocusObjectStore.LookupEntry(goInfo->spellFocus.focusId))
                        sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data0=%u but SpellFocus (Id: %u) not exist.",
                                        goInfo->id, goInfo->type, goInfo->spellFocus.focusId, goInfo->spellFocus.focusId);
                }

                if (goInfo->spellFocus.linkedTrapId)        // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->spellFocus.linkedTrapId, 2);
                }
                break;
            }
            case GAMEOBJECT_TYPE_GOOBER:                    // 10
            {
                if (goInfo->goober.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->goober.lockId, 0);
                }

                CheckGOConsumable(goInfo, goInfo->goober.consumable, 3);

                if (goInfo->goober.pageId)                  // pageId
                {
                    if (!sPageTextStore.LookupEntry<PageText>(goInfo->goober.pageId))
                        sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data7=%u but PageText (Entry %u) not exist.",
                                        goInfo->id, goInfo->type, goInfo->goober.pageId, goInfo->goober.pageId);
                }
                /* disable check for while, too many nonexistent spells
                if (goInfo->goober.spellId)                 // spell
                {
                    CheckGOSpellId(goInfo,goInfo->goober.spellId,10);
                }
                */
                CheckGONoDamageImmuneId(goInfo, goInfo->goober.noDamageImmune, 11);
                if (goInfo->goober.linkedTrapId)            // linked trap
                {
                    CheckGOLinkedTrapId(goInfo, goInfo->goober.linkedTrapId, 12);
                }
                break;
            }
            case GAMEOBJECT_TYPE_AREADAMAGE:                // 12
            {
                if (goInfo->areadamage.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->areadamage.lockId, 0);
                }
                break;
            }
            case GAMEOBJECT_TYPE_CAMERA:                    // 13
            {
                if (goInfo->camera.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->camera.lockId, 0);
                }
                break;
            }
            case GAMEOBJECT_TYPE_MO_TRANSPORT:              // 15
            {
                if (goInfo->moTransport.taxiPathId)
                {
                    if (goInfo->moTransport.taxiPathId >= sTaxiPathNodesByPath.size() || sTaxiPathNodesByPath[goInfo->moTransport.taxiPathId].empty())
                        sLog.outErrorDb("Gameobject (Entry: %u GoType: %u) have data0=%u but TaxiPath (Id: %u) not exist.",
                                        goInfo->id, goInfo->type, goInfo->moTransport.taxiPathId, goInfo->moTransport.taxiPathId);
                }
                break;
            }
            case GAMEOBJECT_TYPE_SUMMONING_RITUAL:          // 18
            {
                /* disable check for while, too many nonexistent spells
                // always must have spell
                CheckGOSpellId(goInfo,goInfo->summoningRitual.spellId,1);
                */
                break;
            }
            case GAMEOBJECT_TYPE_SPELLCASTER:               // 22
            {
                // always must have spell
                CheckGOSpellId(goInfo, goInfo->spellcaster.spellId, 0);
                break;
            }
            case GAMEOBJECT_TYPE_FLAGSTAND:                 // 24
            {
                if (goInfo->flagstand.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->flagstand.lockId, 0);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->flagstand.noDamageImmune, 5);
                break;
            }
            case GAMEOBJECT_TYPE_FISHINGHOLE:               // 25
            {
                if (goInfo->fishinghole.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->fishinghole.lockId, 4);
                }
                break;
            }
            case GAMEOBJECT_TYPE_FLAGDROP:                  // 26
            {
                if (goInfo->flagdrop.lockId)
                {
                    CheckGOLockId(goInfo, goInfo->flagdrop.lockId, 0);
                }
                CheckGONoDamageImmuneId(goInfo, goInfo->flagdrop.noDamageImmune, 3);
                break;
            }
            case GAMEOBJECT_TYPE_CAPTURE_POINT:             // 29
            {
                CheckAndFixGOCaptureMinTime(goInfo, goInfo->capturePoint.minTime, 16);
                break;
            }
        }
    }

    sLog.outString(">> Loaded %u game object templates", sGOStorage.GetRecordCount());
    sLog.outString();
}

void ObjectMgr::LoadExplorationBaseXP()
{
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `level`,`basexp` FROM `exploration_basexp`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u BaseXP definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        uint32 level  = fields[0].GetUInt32();
        uint32 basexp = fields[1].GetUInt32();
        mBaseXPTable[level] = basexp;
        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u BaseXP definitions", count);
    sLog.outString();
}

uint32 ObjectMgr::GetBaseXP(uint32 level) const
{
    BaseXPMap::const_iterator itr = mBaseXPTable.find(level);
    return itr != mBaseXPTable.end() ? itr->second : 0;
}

uint32 ObjectMgr::GetXPForLevel(uint32 level) const
{
    if (level < mPlayerXPperLevel.size())
    {
        return mPlayerXPperLevel[level];
    }
    return 0;
}

void ObjectMgr::LoadPetNames()
{
    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `word`,`entry`,`half` FROM `pet_name_generation`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u pet name parts", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        std::string word = fields[0].GetString();
        uint32 entry     = fields[1].GetUInt32();
        bool   half      = fields[2].GetBool();
        if (half)
        {
            PetHalfName1[entry].push_back(word);
        }
        else
        {
            PetHalfName0[entry].push_back(word);
        }
        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u pet name parts", count);
    sLog.outString();
}

void ObjectMgr::LoadPetNumber()
{
    QueryResult* result = CharacterDatabase.Query("SELECT MAX(`id`) FROM `character_pet`");
    if (result)
    {
        Field* fields = result->Fetch();
        m_PetNumbers.Set(fields[0].GetUInt32() + 1);
        delete result;
    }

    BarGoLink bar(1);
    bar.step();

    sLog.outString(">> Loaded the max pet number: %d", m_PetNumbers.GetNextAfterMaxUsed() - 1);
    sLog.outString();
}

std::string ObjectMgr::GeneratePetName(uint32 entry)
{
    std::vector<std::string>& list0 = PetHalfName0[entry];
    std::vector<std::string>& list1 = PetHalfName1[entry];

    if (list0.empty() || list1.empty())
    {
        CreatureInfo const* cinfo = GetCreatureTemplate(entry);
        char const* petname = GetPetName(cinfo->Family, sWorld.GetDefaultDbcLocale());
        if (!petname)
        {
            petname = cinfo->Name;
        }
        return std::string(petname);
    }

    return *(list0.begin() + urand(0, list0.size() - 1)) + *(list1.begin() + urand(0, list1.size() - 1));
}

void ObjectMgr::LoadCorpses()
{
    uint32 count = 0;
    //                                                    0            1       2                  3                  4                  5                   6
    QueryResult* result = CharacterDatabase.Query("SELECT `corpse`.`guid`, `player`, `corpse`.`position_x`, `corpse`.`position_y`, `corpse`.`position_z`, `corpse`.`orientation`, `corpse`.`map`, "
                          //   7     8            9         10      11    12     13           14            15              16       17
                          "`time`, `corpse_type`, `instance`, `gender`, `race`, `class`, `playerBytes`, `playerBytes2`, `equipmentCache`, `guildId`, `playerFlags` FROM `corpse` "
                          "JOIN `characters` ON `player` = `characters`.`guid` "
                          "LEFT JOIN `guild_member` ON `player`=`guild_member`.`guid` WHERE `corpse_type` <> 0");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u corpses", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 guid = fields[0].GetUInt32();

        Corpse* corpse = new Corpse;
        if (!corpse->LoadFromDB(guid, fields))
        {
            delete corpse;
            continue;
        }

        sObjectAccessor.AddCorpse(corpse);

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u corpses", count);
    sLog.outString();
}

void ObjectMgr::LoadReputationRewardRate()
{
    m_RepRewardRateMap.clear();                             // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `faction`, `quest_rate`, `creature_rate`, `spell_rate` FROM `reputation_reward_rate`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded `reputation_reward_rate`, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 factionId        = fields[0].GetUInt32();

        RepRewardRate repRate;

        repRate.quest_rate      = fields[1].GetFloat();
        repRate.creature_rate   = fields[2].GetFloat();
        repRate.spell_rate      = fields[3].GetFloat();

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);
        if (!factionEntry)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_reward_rate`", factionId);
            continue;
        }

        if (repRate.quest_rate < 0.0f)
        {
            sLog.outErrorDb("Table reputation_reward_rate has quest_rate with invalid rate %f, skipping data for faction %u", repRate.quest_rate, factionId);
            continue;
        }

        if (repRate.creature_rate < 0.0f)
        {
            sLog.outErrorDb("Table reputation_reward_rate has creature_rate with invalid rate %f, skipping data for faction %u", repRate.creature_rate, factionId);
            continue;
        }

        if (repRate.spell_rate < 0.0f)
        {
            sLog.outErrorDb("Table reputation_reward_rate has spell_rate with invalid rate %f, skipping data for faction %u", repRate.spell_rate, factionId);
            continue;
        }

        m_RepRewardRateMap[factionId] = repRate;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u reputation_reward_rate", count);
    sLog.outString();
}

void ObjectMgr::LoadReputationOnKill()
{
    uint32 count = 0;

    //                                                0            1                     2
    QueryResult* result = WorldDatabase.Query("SELECT `creature_id`, `RewOnKillRepFaction1`, `RewOnKillRepFaction2`,"
                          //   3             4             5                   6             7             8                   9
                          "`IsTeamAward1`, `MaxStanding1`, `RewOnKillRepValue1`, `IsTeamAward2`, `MaxStanding2`, `RewOnKillRepValue2`, `TeamDependent` "
                          "FROM `creature_onkill_reputation`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 creature award reputation definitions. DB table `creature_onkill_reputation` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 creature_id = fields[0].GetUInt32();

        ReputationOnKillEntry repOnKill;
        repOnKill.repfaction1          = fields[1].GetUInt32();
        repOnKill.repfaction2          = fields[2].GetUInt32();
        repOnKill.is_teamaward1        = fields[3].GetBool();
        repOnKill.reputation_max_cap1  = fields[4].GetUInt32();
        repOnKill.repvalue1            = fields[5].GetInt32();
        repOnKill.is_teamaward2        = fields[6].GetBool();
        repOnKill.reputation_max_cap2  = fields[7].GetUInt32();
        repOnKill.repvalue2            = fields[8].GetInt32();
        repOnKill.team_dependent       = fields[9].GetUInt8();

        if (!GetCreatureTemplate(creature_id))
        {
            sLog.outErrorDb("Table `creature_onkill_reputation` have data for nonexistent creature entry (%u), skipped", creature_id);
            continue;
        }

        if (repOnKill.repfaction1)
        {
            FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(repOnKill.repfaction1);
            if (!factionEntry1)
            {
                sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `creature_onkill_reputation`", repOnKill.repfaction1);
                continue;
            }
        }

        if (repOnKill.repfaction2)
        {
            FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(repOnKill.repfaction2);
            if (!factionEntry2)
            {
                sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `creature_onkill_reputation`", repOnKill.repfaction2);
                continue;
            }
        }

        mRepOnKill[creature_id] = repOnKill;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u creature award reputation definitions", count);
    sLog.outString();
}

void ObjectMgr::LoadReputationSpilloverTemplate()
{
    m_RepSpilloverTemplateMap.clear();                      // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `faction`, `faction1`, `rate_1`, `rank_1`, `faction2`, `rate_2`, `rank_2`, `faction3`, `rate_3`, `rank_3`, `faction4`, `rate_4`, `rank_4` FROM `reputation_spillover_template`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded `reputation_spillover_template`, table is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 factionId                = fields[0].GetUInt32();

        RepSpilloverTemplate repTemplate;

        repTemplate.faction[0]          = fields[1].GetUInt32();
        repTemplate.faction_rate[0]     = fields[2].GetFloat();
        repTemplate.faction_rank[0]     = fields[3].GetUInt32();
        repTemplate.faction[1]          = fields[4].GetUInt32();
        repTemplate.faction_rate[1]     = fields[5].GetFloat();
        repTemplate.faction_rank[1]     = fields[6].GetUInt32();
        repTemplate.faction[2]          = fields[7].GetUInt32();
        repTemplate.faction_rate[2]     = fields[8].GetFloat();
        repTemplate.faction_rank[2]     = fields[9].GetUInt32();
        repTemplate.faction[3]          = fields[10].GetUInt32();
        repTemplate.faction_rate[3]     = fields[11].GetFloat();
        repTemplate.faction_rank[3]     = fields[12].GetUInt32();

        FactionEntry const* factionEntry = sFactionStore.LookupEntry(factionId);

        if (!factionEntry)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", factionId);
            continue;
        }

        if (factionEntry->team == 0)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u in `reputation_spillover_template` does not belong to any team, skipping", factionId);
            continue;
        }

        for (uint32 i = 0; i < MAX_SPILLOVER_FACTIONS; ++i)
        {
            if (repTemplate.faction[i])
            {
                FactionEntry const* factionSpillover = sFactionStore.LookupEntry(repTemplate.faction[i]);

                if (!factionSpillover)
                {
                    sLog.outErrorDb("Spillover faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template` for faction %u, skipping", repTemplate.faction[i], factionId);
                    continue;
                }

                if (factionSpillover->reputationListID < 0)
                {
                    sLog.outErrorDb("Spillover faction (faction.dbc) %u for faction %u in `reputation_spillover_template` can not be listed for client, and then useless, skipping", repTemplate.faction[i], factionId);
                    continue;
                }

                if (repTemplate.faction_rank[i] >= MAX_REPUTATION_RANK)
                {
                    sLog.outErrorDb("Rank %u used in `reputation_spillover_template` for spillover faction %u is not valid, skipping", repTemplate.faction_rank[i], repTemplate.faction[i]);
                    continue;
                }
            }
        }

        FactionEntry const* factionEntry0 = sFactionStore.LookupEntry(repTemplate.faction[0]);
        if (repTemplate.faction[0] && !factionEntry0)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[0]);
            continue;
        }
        FactionEntry const* factionEntry1 = sFactionStore.LookupEntry(repTemplate.faction[1]);
        if (repTemplate.faction[1] && !factionEntry1)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[1]);
            continue;
        }
        FactionEntry const* factionEntry2 = sFactionStore.LookupEntry(repTemplate.faction[2]);
        if (repTemplate.faction[2] && !factionEntry2)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[2]);
            continue;
        }
        FactionEntry const* factionEntry3 = sFactionStore.LookupEntry(repTemplate.faction[3]);
        if (repTemplate.faction[3] && !factionEntry3)
        {
            sLog.outErrorDb("Faction (faction.dbc) %u does not exist but is used in `reputation_spillover_template`", repTemplate.faction[3]);
            continue;
        }

        m_RepSpilloverTemplateMap[factionId] = repTemplate;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u reputation_spillover_template", count);
    sLog.outString();
}

void ObjectMgr::LoadPointsOfInterest()
{
    mPointsOfInterest.clear();                              // need for reload case

    uint32 count = 0;

    //                                                0      1  2  3      4     5
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `x`, `y`, `icon`, `flags`, `data`, `icon_name` FROM `points_of_interest`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded 0 Points of Interest definitions. DB table `points_of_interest` is empty.");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 point_id = fields[0].GetUInt32();

        PointOfInterest POI;
        POI.x                    = fields[1].GetFloat();
        POI.y                    = fields[2].GetFloat();
        POI.icon                 = fields[3].GetUInt32();
        POI.flags                = fields[4].GetUInt32();
        POI.data                 = fields[5].GetUInt32();
        POI.icon_name            = fields[6].GetCppString();

        if (!MaNGOS::IsValidMapCoord(POI.x, POI.y))
        {
            sLog.outErrorDb("Table `points_of_interest` (Entry: %u) have invalid coordinates (X: %f Y: %f), ignored.", point_id, POI.x, POI.y);
            continue;
        }

        mPointsOfInterest[point_id] = POI;

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u Points of Interest definitions", count);
    sLog.outString();
}

void ObjectMgr::DeleteCreatureData(uint32 guid)
{
    // remove mapid*cellid -> guid_set map
    CreatureData const* data = GetCreatureData(guid);
    if (data)
    {
        RemoveCreatureFromGrid(guid, data);
    }

    mCreatureDataMap.erase(guid);
}

void ObjectMgr::DeleteGOData(uint32 guid)
{
    // remove mapid*cellid -> guid_set map
    GameObjectData const* data = GetGOData(guid);
    if (data)
    {
        RemoveGameobjectFromGrid(guid, data);
    }

    mGameObjectDataMap.erase(guid);
}

void ObjectMgr::AddCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid, uint32 instance)
{
    // corpses are always added to spawn mode 0 and they are spawned by their instance id
    CellObjectGuids& cell_guids = mMapObjectGuids[mapid][cellid];
    cell_guids.corpses[player_guid] = instance;
}

void ObjectMgr::DeleteCorpseCellData(uint32 mapid, uint32 cellid, uint32 player_guid)
{
    // corpses are always added to spawn mode 0 and they are spawned by their instance id
    CellObjectGuids& cell_guids = mMapObjectGuids[mapid][cellid];
    cell_guids.corpses.erase(player_guid);
}

void ObjectMgr::LoadQuestRelationsHelper(QuestRelationsMap& map, QuestActor actor, QuestRole role)
{
    map.clear();                                            // need for reload case

    uint32 count = 0;

    QueryResult* result = WorldDatabase.PQuery("SELECT `entry`, `quest` FROM `quest_relations` WHERE `actor` = %d AND `role` = %d", actor, role);

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        sLog.outErrorDb(">> Loaded 0 quest relations. DB table `quest_relations` is empty.");
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        uint32 id    = fields[0].GetUInt32();
        uint32 quest = fields[1].GetUInt32();

        if (mQuestTemplates.find(quest) == mQuestTemplates.end())
        {
            sLog.outErrorDb("Table `quest_relations`: Quest %u listed for entry %u does not exist.", quest, id);
            continue;
        }

        map.insert(QuestRelationsMap::value_type(id, quest));

        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString();
    sLog.outString(">> Loaded %u %s quest %s from `quest_relations`", count, (actor == 1) ? "gameobject" : "creature", (role == 1) ? "takers" : "givers");
}

void ObjectMgr::LoadGameobjectQuestRelations()
{
    LoadQuestRelationsHelper(m_GOQuestRelations, QA_GAMEOBJECT, QR_START);

    for (QuestRelationsMap::iterator itr = m_GOQuestRelations.begin(); itr != m_GOQuestRelations.end(); ++itr)
    {
        GameObjectInfo const* goInfo = GetGameObjectInfo(itr->first);
        if (!goInfo)
        {
            sLog.outErrorDb("Table `quest_relations` have data for nonexistent gameobject entry (%u) and existing quest %u", itr->first, itr->second);
        }
        else if (goInfo->type != GAMEOBJECT_TYPE_QUESTGIVER)
        {
            sLog.outErrorDb("Table `quest_relations` have data gameobject entry (%u) for quest %u, but GO is not GAMEOBJECT_TYPE_QUESTGIVER", itr->first, itr->second);
        }
    }
}

void ObjectMgr::LoadGameobjectInvolvedRelations()
{
    LoadQuestRelationsHelper(m_GOQuestInvolvedRelations, QA_GAMEOBJECT, QR_END);

    for (QuestRelationsMap::iterator itr = m_GOQuestInvolvedRelations.begin(); itr != m_GOQuestInvolvedRelations.end(); ++itr)
    {
        GameObjectInfo const* goInfo = GetGameObjectInfo(itr->first);
        if (!goInfo)
        {
            sLog.outErrorDb("Table `quest_relations` have data for nonexistent gameobject entry (%u) and existing quest %u", itr->first, itr->second);
        }
        else if (goInfo->type != GAMEOBJECT_TYPE_QUESTGIVER)
        {
            sLog.outErrorDb("Table `quest_relations` have data gameobject entry (%u) for quest %u, but GO is not GAMEOBJECT_TYPE_QUESTGIVER", itr->first, itr->second);
        }
    }
}

void ObjectMgr::LoadCreatureQuestRelations()
{
    LoadQuestRelationsHelper(m_CreatureQuestRelations, QA_CREATURE, QR_START);

    for (QuestRelationsMap::iterator itr = m_CreatureQuestRelations.begin(); itr != m_CreatureQuestRelations.end(); ++itr)
    {
        CreatureInfo const* cInfo = GetCreatureTemplate(itr->first);
        if (!cInfo)
        {
            sLog.outErrorDb("Table `quest_relations` have data for nonexistent creature entry (%u) and existing quest %u", itr->first, itr->second);
        }
        else if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_QUESTGIVER))
        {
            sLog.outErrorDb("Table `quest_relations` has creature entry (%u) for quest %u, but npcflag does not include UNIT_NPC_FLAG_QUESTGIVER", itr->first, itr->second);
        }
    }
}

void ObjectMgr::LoadCreatureInvolvedRelations()
{
    LoadQuestRelationsHelper(m_CreatureQuestInvolvedRelations, QA_CREATURE, QR_END);

    for (QuestRelationsMap::iterator itr = m_CreatureQuestInvolvedRelations.begin(); itr != m_CreatureQuestInvolvedRelations.end(); ++itr)
    {
        CreatureInfo const* cInfo = GetCreatureTemplate(itr->first);
        if (!cInfo)
        {
            sLog.outErrorDb("Table `quest_relations` have data for nonexistent creature entry (%u) and existing quest %u", itr->first, itr->second);
        }
        else if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_QUESTGIVER))
        {
            sLog.outErrorDb("Table `quest_relations` has creature entry (%u) for quest %u, but npcflag does not include UNIT_NPC_FLAG_QUESTGIVER", itr->first, itr->second);
        }
    }
}

void ObjectMgr::LoadReservedPlayersNames()
{
    m_ReservedNames.clear();                                // need for reload case

    QueryResult* result = WorldDatabase.Query("SELECT `name` FROM `reserved_name`");

    uint32 count = 0;

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u reserved player names", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    Field* fields;
    do
    {
        bar.step();
        fields = result->Fetch();
        std::string name = fields[0].GetCppString();

        std::wstring wstr;
        if (!Utf8toWStr(name, wstr))
        {
            sLog.outError("Table `reserved_name` have invalid name: %s", name.c_str());
            continue;
        }

        wstrToLower(wstr);

        m_ReservedNames.insert(wstr);
        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u reserved player names", count);
    sLog.outString();
}

bool ObjectMgr::IsReservedName(const std::string& name) const
{
    std::wstring wstr;
    if (!Utf8toWStr(name, wstr))
    {
        return false;
    }

    wstrToLower(wstr);

    return m_ReservedNames.find(wstr) != m_ReservedNames.end();
}

enum LanguageType
{
    LT_BASIC_LATIN    = 0x0000,
    LT_EXTENDEN_LATIN = 0x0001,
    LT_CYRILLIC       = 0x0002,
    LT_EAST_ASIA      = 0x0004,
    LT_ANY            = 0xFFFF
};

static LanguageType GetRealmLanguageType(bool create)
{
    switch (sWorld.getConfig(CONFIG_UINT32_REALM_ZONE))
    {
        case REALM_ZONE_UNKNOWN:                            // any language
        case REALM_ZONE_DEVELOPMENT:
        case REALM_ZONE_TEST_SERVER:
        case REALM_ZONE_QA_SERVER:
            return LT_ANY;
        case REALM_ZONE_UNITED_STATES:                      // extended-Latin
        case REALM_ZONE_OCEANIC:
        case REALM_ZONE_LATIN_AMERICA:
        case REALM_ZONE_ENGLISH:
        case REALM_ZONE_GERMAN:
        case REALM_ZONE_FRENCH:
        case REALM_ZONE_SPANISH:
            return LT_EXTENDEN_LATIN;
        case REALM_ZONE_KOREA:                              // East-Asian
        case REALM_ZONE_TAIWAN:
        case REALM_ZONE_CHINA:
            return LT_EAST_ASIA;
        case REALM_ZONE_RUSSIAN:                            // Cyrillic
            return LT_CYRILLIC;
        default:
            return create ? LT_BASIC_LATIN : LT_ANY;        // basic-Latin at create, any at login
    }
}

bool isValidString(const std::wstring &wstr, uint32 strictMask, bool numericOrSpace, bool create = false)
{
    if (strictMask == 0)                                    // any language, ignore realm
    {
        if (isExtendedLatinString(wstr, numericOrSpace))
        {
            return true;
        }
        if (isCyrillicString(wstr, numericOrSpace))
        {
            return true;
        }
        if (isEastAsianString(wstr, numericOrSpace))
        {
            return true;
        }
        return false;
    }

    if (strictMask & 0x2)                                   // realm zone specific
    {
        LanguageType lt = GetRealmLanguageType(create);
        if (lt & LT_EXTENDEN_LATIN)
            if (isExtendedLatinString(wstr, numericOrSpace))
            {
                return true;
            }
        if (lt & LT_CYRILLIC)
            if (isCyrillicString(wstr, numericOrSpace))
            {
                return true;
            }
        if (lt & LT_EAST_ASIA)
            if (isEastAsianString(wstr, numericOrSpace))
            {
                return true;
            }
    }

    if (strictMask & 0x1)                                   // basic Latin
    {
        if (isBasicLatinString(wstr, numericOrSpace))
        {
            return true;
        }
    }

    return false;
}

uint8 ObjectMgr::CheckPlayerName(const std::string& name, bool create)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return CHAR_NAME_INVALID_CHARACTER;
    }

    if (wname.size() > MAX_PLAYER_NAME)
    {
        return CHAR_NAME_TOO_LONG;
    }

    uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_PLAYER_NAME);
    if (wname.size() < minName)
    {
        return CHAR_NAME_TOO_SHORT;
    }

    uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_PLAYER_NAMES);
    if (!isValidString(wname, strictMask, false, create))
    {
        return CHAR_NAME_MIXED_LANGUAGES;
    }

    return CHAR_NAME_SUCCESS;
}

bool ObjectMgr::IsValidCharterName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return false;
    }

    if (wname.size() > MAX_CHARTER_NAME)
    {
        return false;
    }

    uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_CHARTER_NAME);
    if (wname.size() < minName)
    {
        return false;
    }

    uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_CHARTER_NAMES);

    return isValidString(wname, strictMask, true);
}

PetNameInvalidReason ObjectMgr::CheckPetName(const std::string& name)
{
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return PET_NAME_INVALID;
    }

    if (wname.size() > MAX_PET_NAME)
    {
        return PET_NAME_TOO_LONG;
    }

    uint32 minName = sWorld.getConfig(CONFIG_UINT32_MIN_PET_NAME);
    if (wname.size() < minName)
    {
        return PET_NAME_TOO_SHORT;
    }

    uint32 strictMask = sWorld.getConfig(CONFIG_UINT32_STRICT_PET_NAMES);
    if (!isValidString(wname, strictMask, false))
    {
        return PET_NAME_MIXED_LANGUAGES;
    }

    return PET_NAME_SUCCESS;
}

int ObjectMgr::GetIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
    {
        return -1;
    }

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
        if (m_LocalForIndex[i] == loc)
        {
            return i;
        }

    return -1;
}

LocaleConstant ObjectMgr::GetLocaleForIndex(int i)
{
    if (i < 0 || i >= (int32)m_LocalForIndex.size())
    {
        return LOCALE_enUS;
    }

    return m_LocalForIndex[i];
}

int ObjectMgr::GetOrNewIndexForLocale(LocaleConstant loc)
{
    if (loc == LOCALE_enUS)
    {
        return -1;
    }

    for (size_t i = 0; i < m_LocalForIndex.size(); ++i)
        if (m_LocalForIndex[i] == loc)
        {
            return i;
        }

    m_LocalForIndex.push_back(loc);
    return m_LocalForIndex.size() - 1;
}

void ObjectMgr::LoadGameObjectForQuests()
{
    mGameObjectForQuestSet.clear();                         // need for reload case

    if (!sGOStorage.GetMaxEntry())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded 0 GameObjects for quests");
        sLog.outString();
        return;
    }

    BarGoLink bar(sGOStorage.GetRecordCount());
    uint32 count = 0;

    // collect GO entries for GO that must activated
    for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
    {
        bar.step();
        switch (itr->type)
        {
            case GAMEOBJECT_TYPE_QUESTGIVER:
            {
                if (m_GOQuestRelations.find(itr->id) != m_GOQuestRelations.end() ||
                    m_GOQuestInvolvedRelations.find(itr->id) != m_GOQuestInvolvedRelations.end())
                {
                    mGameObjectForQuestSet.insert(itr->id);
                    ++count;
                }

                break;
            }
            case GAMEOBJECT_TYPE_CHEST:
            {
                // scan GO chest with loot including quest items
                uint32 loot_id = itr->GetLootId();

                // always activate to quest, GO may not have loot, OR find if GO has loot for quest.
                if (itr->chest.questId || LootTemplates_Gameobject.HaveQuestLootFor(loot_id))
                {
                    mGameObjectForQuestSet.insert(itr->id);
                    ++count;
                }
                break;
            }
            case GAMEOBJECT_TYPE_GENERIC:
            {
                if (itr->_generic.questID)                  // quest related objects, has visual effects
                {
                    mGameObjectForQuestSet.insert(itr->id);
                    ++count;
                }
                break;
            }
            case GAMEOBJECT_TYPE_SPELL_FOCUS:
            {
                if (itr->spellFocus.questID)                // quest related objects, has visual effect
                {
                    mGameObjectForQuestSet.insert(itr->id);
                    ++count;
                }
                break;
            }
            case GAMEOBJECT_TYPE_GOOBER:
            {
                if (itr->goober.questId)                    // quests objects
                {
                    mGameObjectForQuestSet.insert(itr->id);
                    ++count;
                }
                break;
            }
            default:
                break;
        }
    }

    sLog.outString(">> Loaded %u GameObjects for quests", count);
    sLog.outString();
}

inline void _DoStringError(int32 entry, char const* text, ...)
{
    MANGOS_ASSERT(text);

    char buf[256];
    va_list ap;
    va_start(ap, text);
    vsnprintf(buf, 256, text, ap);
    va_end(ap);

    if (entry <= MAX_CREATURE_AI_TEXT_STRING_ID)            // script library error
    {
        sLog.outErrorScriptLib("%s", buf);
    }
    else if (entry <= MIN_CREATURE_AI_TEXT_STRING_ID)       // eventAI error
    {
        sLog.outErrorEventAI("%s", buf);
    }
    else if (entry < MIN_DB_SCRIPT_STRING_ID)               // mangos string error
    {
        sLog.outError("%s", buf);
    }
    else // if (entry > MIN_DB_SCRIPT_STRING_ID)            // DB script text error
    {
        sLog.outErrorDb("DB-SCRIPTS: %s", buf);
    }
}

bool ObjectMgr::LoadMangosStrings(DatabaseType& db, char const* table, int32 min_value, int32 max_value, bool extra_content)
{
    int32 start_value = min_value;
    int32 end_value   = max_value;
    // some string can have negative indexes range
    if (start_value < 0)
    {
        if (end_value >= start_value)
        {
            sLog.outErrorDb("Table '%s' attempt loaded with invalid range (%d - %d), strings not loaded.", table, min_value, max_value);
            return false;
        }

        // real range (max+1,min+1) exaple: (-10,-1000) -> -999...-10+1
        std::swap(start_value, end_value);
        ++start_value;
        ++end_value;
    }
    else
    {
        if (start_value >= end_value)
        {
            sLog.outErrorDb("Table '%s' attempt loaded with invalid range (%d - %d), strings not loaded.", table, min_value, max_value);
            return false;
        }
    }

    // cleanup affected map part for reloading case
    for (MangosStringLocaleMap::iterator itr = mMangosStringLocaleMap.begin(); itr != mMangosStringLocaleMap.end();)
    {
        if (itr->first >= start_value && itr->first < end_value)
        {
            mMangosStringLocaleMap.erase(itr++);
        }
        else
        {
            ++itr;
        }
    }

    sLog.outString("Loading texts from %s%s", table, extra_content ? ", with additional data" : "");

    QueryResult* result = db.PQuery("SELECT `entry`,`content_default`,`content_loc1`,`content_loc2`,`content_loc3`,`content_loc4`,`content_loc5`,`content_loc6`,`content_loc7`,`content_loc8` %s FROM %s",
                                    extra_content ? ",sound,type,language,emote" : "", table);

    if (!result)
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString();
        if (min_value == MIN_MANGOS_STRING_ID)              // error only in case internal strings
        {
            sLog.outErrorDb(">> Loaded 0 mangos strings. DB table `%s` is empty. Can not continue.", table);
        }
        else
        {
            sLog.outString(">> Loaded 0 string templates. DB table `%s` is empty.", table);
        }
        return false;
    }

    uint32 count = 0;

    BarGoLink bar(result->GetRowCount());

    do
    {
        Field* fields = result->Fetch();
        bar.step();

        int32 entry = fields[0].GetInt32();

        if (entry == 0)
        {
            _DoStringError(start_value, "Table `%s` contain reserved entry 0, ignored.", table);
            continue;
        }
        else if (entry < start_value || entry >= end_value)
        {
            _DoStringError(start_value, "Table `%s` contain entry %i out of allowed range (%d - %d), ignored.", table, entry, min_value, max_value);
            continue;
        }

        MangosStringLocale& data = mMangosStringLocaleMap[entry];

        if (!data.Content.empty())
        {
            _DoStringError(entry, "Table `%s` contain data for already loaded entry  %i (from another table?), ignored.", table, entry);
            continue;
        }

        data.Content.resize(1);
        ++count;

        // 0 -> default, idx in to idx+1
        data.Content[0] = fields[1].GetCppString();

        for (int i = 1; i < MAX_LOCALE; ++i)
        {
            std::string str = fields[i + 1].GetCppString();
            if (!str.empty())
            {
                int idx = GetOrNewIndexForLocale(LocaleConstant(i));
                if (idx >= 0)
                {
                    // 0 -> default, idx in to idx+1
                    if ((int32)data.Content.size() <= idx + 1)
                    {
                        data.Content.resize(idx + 2);
                    }

                    data.Content[idx + 1] = str;
                }
            }
        }

        // Load additional string content if necessary
        if (extra_content)
        {
            data.SoundId     = fields[10].GetUInt32();
            data.Type        = fields[11].GetUInt32();
            data.LanguageId  = Language(fields[12].GetUInt32());
            data.Emote       = fields[13].GetUInt32();

            if (data.SoundId && !sSoundEntriesStore.LookupEntry(data.SoundId))
            {
                _DoStringError(entry, "Entry %i in table `%s` has soundId %u but sound does not exist.", entry, table, data.SoundId);
                data.SoundId = 0;
            }

            if (!GetLanguageDescByID(data.LanguageId))
            {
                _DoStringError(entry, "Entry %i in table `%s` using Language %u but Language does not exist.", entry, table, uint32(data.LanguageId));
                data.LanguageId = LANG_UNIVERSAL;
            }

            if (data.Type > CHAT_TYPE_ZONE_YELL)
            {
                _DoStringError(entry, "Entry %i in table `%s` has Type %u but this Chat Type does not exist.", entry, table, data.Type);
                data.Type = CHAT_TYPE_SAY;
            }

            if (data.Emote && !sEmotesStore.LookupEntry(data.Emote))
            {
                _DoStringError(entry, "Entry %i in table `%s` has Emote %u but emote does not exist.", entry, table, data.Emote);
                data.Emote = EMOTE_ONESHOT_NONE;
            }
        }
    }
    while (result->NextRow());

    delete result;

    if (min_value == MIN_MANGOS_STRING_ID)
    {
        sLog.outString(">> Loaded %u MaNGOS strings from table %s", count, table);
    }
    else
    {
        sLog.outString(">> Loaded %u %s templates from %s", count, extra_content ? "text" : "string", table);
    }
    sLog.outString();

    m_loadedStringCount[min_value] = count;

    return true;
}

const char* ObjectMgr::GetMangosString(int32 entry, int locale_idx) const
{
    // locale_idx==-1 -> default, locale_idx >= 0 in to idx+1
    // Content[0] always exist if exist MangosStringLocale
    if (MangosStringLocale const* msl = GetMangosStringLocale(entry))
    {
        if ((int32)msl->Content.size() > locale_idx + 1 && !msl->Content[locale_idx + 1].empty())
        {
            return msl->Content[locale_idx + 1].c_str();
        }
        else
        {
            return msl->Content[0].c_str();
        }
    }

    _DoStringError(entry, "Entry %i not found but requested", entry);

    return "<error>";
}

void ObjectMgr::LoadFishingBaseSkillLevel()
{
    mFishingBaseForArea.clear();                            // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `entry`,`skill` FROM `skill_fishing_base_level`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded `skill_fishing_base_level`, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();
        uint32 entry  = fields[0].GetUInt32();
        int32 skill   = fields[1].GetInt32();

        AreaTableEntry const* fArea = GetAreaEntryByAreaID(entry);
        if (!fArea)
        {
            sLog.outErrorDb("AreaId %u defined in `skill_fishing_base_level` does not exist", entry);
            continue;
        }

        mFishingBaseForArea[entry] = skill;
        ++count;
    }
    while (result->NextRow());

    delete result;

    sLog.outString(">> Loaded %u areas for fishing base skill level", count);
    sLog.outString();
}

// Check if a player meets condition conditionId
bool ObjectMgr::IsPlayerMeetToCondition(uint16 conditionId, Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType, ConditionEntry* entry) const
{
    if (const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(conditionId))
    {
        return condition->Meets(pPlayer, map, source, conditionSourceType, entry);
    }

    return false;
}

// Attention: make sure to keep this list in sync with ConditionSource to avoid array
//            out of bounds access! It is accessed with ConditionSource as index!
char const* conditionSourceToStr[] =
{
    "loot system",
    "referencing loot",
    "gossip menu",
    "gossip menu option",
    "event AI",
    "hardcoded",
    "vendor's item check",
    "spell_area check",
    "npc_spellclick_spells check", // Unused. For 3.x and later.
    "DBScript engine",
    "area trigger check"
};

// Checks if player meets the condition
bool PlayerCondition::Meets(Player const* player, Map const* map, WorldObject const* source, ConditionSource conditionSourceType, ConditionEntry* entry) const
{
    DEBUG_LOG("Condition-System: Check condition %u, type %i - called from %s with params plr: %s, map %i, src %s",
              m_entry, m_condition, conditionSourceToStr[conditionSourceType], player ? player->GetGuidStr().c_str() : "<NULL>", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "<NULL>");

    if (entry)
    {
        entry->type = m_condition;
        entry->param1 = m_value1;
        entry->param2 = m_value2;
    }

    if (!CheckParamRequirements(player, map, source, conditionSourceType))
    {
        return false;
    }

    switch (m_condition)
    {
        case CONDITION_NOT:
            // Checked on load
            return !sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(player, map, source, conditionSourceType, entry);
        case CONDITION_OR:
            // Checked on load
            return sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(player, map, source, conditionSourceType, entry) || sConditionStorage.LookupEntry<PlayerCondition>(m_value2)->Meets(player, map, source, conditionSourceType, entry);
        case CONDITION_AND:
            // Checked on load
            return sConditionStorage.LookupEntry<PlayerCondition>(m_value1)->Meets(player, map, source, conditionSourceType, entry) && sConditionStorage.LookupEntry<PlayerCondition>(m_value2)->Meets(player, map, source, conditionSourceType, entry);
        case CONDITION_NONE:
            return true;                                    // empty condition, always met
        case CONDITION_AURA:
            return player->HasAura(m_value1, SpellEffectIndex(m_value2));
        case CONDITION_ITEM:
            return player->HasItemCount(m_value1, m_value2);
        case CONDITION_ITEM_EQUIPPED:
            return player->HasItemWithIdEquipped(m_value1, 1);
        case CONDITION_AREAID:
        {
            uint32 zone, area;
            WorldObject const* searcher = source ? source : player;
            searcher->GetZoneAndAreaId(zone, area);
            return (zone == m_value1 || area == m_value1) == (m_value2 == 0);
        }
        case CONDITION_REPUTATION_RANK_MIN:
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(m_value1);
            return faction && player->GetReputationMgr().GetRank(faction) >= ReputationRank(m_value2);
        }
        case CONDITION_TEAM:
        {
            if (conditionSourceType == CONDITION_FROM_REFERING_LOOT && sWorld.getConfig(CONFIG_BOOL_ALLOW_TWO_SIDE_INTERACTION_AUCTION))
            {
                return true;
            }
            else
            {
                return uint32(player->GetTeam()) == m_value1;
            }
        }
        case CONDITION_SKILL:
            return player->HasSkill(m_value1) && player->GetBaseSkillValue(m_value1) >= m_value2;
        case CONDITION_QUESTREWARDED:
            return player->GetQuestRewardStatus(m_value1);
        case CONDITION_QUESTTAKEN:
            return player->IsCurrentQuest(m_value1, m_value2);
        case CONDITION_AD_COMMISSION_AURA:
        {
            Unit::SpellAuraHolderMap const& auras = player->GetSpellAuraHolderMap();
            for (Unit::SpellAuraHolderMap::const_iterator itr = auras.begin(); itr != auras.end(); ++itr)
                if ((itr->second->GetSpellProto()->HasAttribute(SPELL_ATTR_CASTABLE_WHILE_MOUNTED) || itr->second->GetSpellProto()->HasAttribute(SPELL_ATTR_UNK4)) && itr->second->GetSpellProto()->SpellVisual == 3580)
                {
                    return true;
                }
            return false;
        }
        case CONDITION_NO_AURA:
            return !player->HasAura(m_value1, SpellEffectIndex(m_value2));
        case CONDITION_ACTIVE_GAME_EVENT:
            return sGameEventMgr.IsActiveEvent(m_value1);
        case CONDITION_AREA_FLAG:
        {
            WorldObject const* searcher = source ? source : player;
            if (AreaTableEntry const* pAreaEntry = GetAreaEntryByAreaID(searcher->GetAreaId()))
            {
                if ((!m_value1 || (pAreaEntry->flags & m_value1)) && (!m_value2 || !(pAreaEntry->flags & m_value2)))
                {
                    return true;
                }
            }
            return false;
        }
        case CONDITION_RACE_CLASS:
            if ((!m_value1 || (player->getRaceMask() & m_value1)) && (!m_value2 || (player->getClassMask() & m_value2)))
            {
                return true;
            }
            return false;
        case CONDITION_LEVEL:
        {
            switch (m_value2)
            {
                case 0: return player->getLevel() == m_value1;
                case 1: return player->getLevel() >= m_value1;
                case 2: return player->getLevel() <= m_value1;
            }
            return false;
        }
        case CONDITION_NOITEM:
            return !player->HasItemCount(m_value1, m_value2);
        case CONDITION_SPELL:
        {
            switch (m_value2)
            {
                case 0: return player->HasSpell(m_value1);
                case 1: return !player->HasSpell(m_value1);
            }
            return false;
        }
        case CONDITION_INSTANCE_SCRIPT:
        {
            if (!map)
            {
                map = player ? player->GetMap() : source->GetMap();
            }

            if (InstanceData* data = map->GetInstanceData())
            {
                return data->CheckConditionCriteriaMeet(player, m_value1, source, conditionSourceType);
            }
            return false;
        }
        case CONDITION_QUESTAVAILABLE:
        {
            return player->CanTakeQuest(sObjectMgr.GetQuestTemplate(m_value1), false);
        }
        case CONDITION_RESERVED_1:
        case CONDITION_RESERVED_2:
        case CONDITION_RESERVED_3:
        case CONDITION_RESERVED_4:
            return false;
        case CONDITION_QUEST_NONE:
        {
            if (!player->IsCurrentQuest(m_value1) && !player->GetQuestRewardStatus(m_value1))
            {
                return true;
            }
            return false;
        }
        case CONDITION_ITEM_WITH_BANK:
            return player->HasItemCount(m_value1, m_value2, true);
        case CONDITION_NOITEM_WITH_BANK:
            return !player->HasItemCount(m_value1, m_value2, true);
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
            return !sGameEventMgr.IsActiveEvent(m_value1);
        case CONDITION_ACTIVE_HOLIDAY:
            return sGameEventMgr.IsActiveHoliday(HolidayIds(m_value1));
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            return !sGameEventMgr.IsActiveHoliday(HolidayIds(m_value1));
        case CONDITION_LEARNABLE_ABILITY:
        {
            // Already know the spell
            if (player->HasSpell(m_value1))
            {
                return false;
            }

            // If item defined, check if player has the item already.
            if (m_value2)
            {
                // Hard coded item count. This should be ok, since the intention with this condition is to have
                // a all-in-one check regarding items that learn some ability (primary/secondary tradeskills).
                // Commonly, items like this is unique and/or are not expected to be obtained more than once.
                if (player->HasItemCount(m_value2, 1, true))
                {
                    return false;
                }
            }

            bool isSkillOk = false;

            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(m_value1);

            for (SkillLineAbilityMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                const SkillLineAbilityEntry* skillInfo = itr->second;

                if (!skillInfo)
                {
                    continue;
                }

                // doesn't have skill
                if (!player->HasSkill(skillInfo->skillId))
                {
                    return false;
                }

                // doesn't match class
                if (skillInfo->classmask && (skillInfo->classmask & player->getClassMask()) == 0)
                {
                    return false;
                }

                // doesn't match race
                if (skillInfo->racemask && (skillInfo->racemask & player->getRaceMask()) == 0)
                {
                    return false;
                }

                // skill level too low
                if (skillInfo->min_value > player->GetSkillValue(skillInfo->skillId))
                {
                    return false;
                }

                isSkillOk = true;
                break;
            }

            if (isSkillOk)
            {
                return true;
            }

            return false;
        }
        case CONDITION_SKILL_BELOW:
        {
            if (m_value2 == 1)
            {
                return !player->HasSkill(m_value1);
            }
            else
            {
                return player->HasSkill(m_value1) && player->GetBaseSkillValue(m_value1) < m_value2;
            }
        }
        case CONDITION_REPUTATION_RANK_MAX:
        {
            FactionEntry const* faction = sFactionStore.LookupEntry(m_value1);
            return faction && player->GetReputationMgr().GetRank(faction) <= ReputationRank(m_value2);
        }
        case CONDITION_SOURCE_AURA:
        {
            if (!source->isType(TYPEMASK_UNIT))
            {
                sLog.outErrorDb("CONDITION_SOURCE_AURA (entry %u) is used for non unit source (source %s) by %s", m_entry, source->GetGuidStr().c_str(), player->GetGuidStr().c_str());
                return false;
            }
            return ((Unit*)source)->HasAura(m_value1, SpellEffectIndex(m_value2));
        }
        case CONDITION_LAST_WAYPOINT:
        {
            if (source->GetTypeId() != TYPEID_UNIT)
            {
                sLog.outErrorDb("CONDITION_LAST_WAYPOINT (entry %u) is used for non creature source (source %s) by %s", m_entry, source->GetGuidStr().c_str(), player->GetGuidStr().c_str());
                return false;
            }
            uint32 lastReachedWp = ((Creature*)source)->GetMotionMaster()->getLastReachedWaypoint();
            switch (m_value2)
            {
                case 0: return m_value1 == lastReachedWp;
                case 1: return m_value1 <= lastReachedWp;
                case 2: return m_value1 > lastReachedWp;
            }
            return false;
        }
        case CONDITION_GENDER:
            return player->getGender() == m_value1;
        case CONDITION_DEAD_OR_AWAY:
            switch (m_value1)
            {
                case 0:                                     // Player dead or out of range
                    return !player || !player->IsAlive() || (m_value2 && source && !source->IsWithinDistInMap(player, m_value2));
                case 1:                                     // All players in Group dead or out of range
                    if (!player)
                    {
                        return true;
                    }
                    if (Group const* grp = player->GetGroup())
                    {
                        for (GroupReference const* itr = grp->GetFirstMember(); itr != NULL; itr = itr->next())
                        {
                            Player const* pl = itr->getSource();
                            if (pl && pl->IsAlive() && !pl->isGameMaster() && (!m_value2 || !source || source->IsWithinDistInMap(pl, m_value2)))
                            {
                                return false;
                            }
                        }
                        return true;
                    }
                    else
                    {
                        return !player->IsAlive() || (m_value2 && source && !source->IsWithinDistInMap(player, m_value2));
                    }
                case 2:                                     // All players in instance dead or out of range
                    for (Map::PlayerList::const_iterator itr = map->GetPlayers().begin(); itr != map->GetPlayers().end(); ++itr)
                    {
                        Player const* plr = itr->getSource();
                        if (plr && plr->IsAlive() && !plr->isGameMaster() && (!m_value2 || !source || source->IsWithinDistInMap(plr, m_value2)))
                        {
                            return false;
                        }
                    }
                    return true;
                case 3:                                     // Creature source is dead
                    return !source || source->GetTypeId() != TYPEID_UNIT || !((Unit*)source)->IsAlive();
            }
        case CONDITION_CREATURE_IN_RANGE:
        {
            Creature* creature = NULL;

            MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck creature_check(*player, m_value1, true, false, m_value2, true);
            MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(creature, creature_check);
            Cell::VisitGridObjects(player, searcher, m_value2);

            return creature;
        }
        case CONDITION_GAMEOBJECT_IN_RANGE:
        {
            GameObject* pGo = NULL;

            if (source)
            {
                MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*source, m_value1, m_value2);
                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(pGo, go_check);

                Cell::VisitGridObjects(source, searcher, m_value2);
            }
            return pGo;
        }
        case CONDITION_PVP_RANK:
        {
            switch (m_value2)
            {
              case 0: return player->GetHonorRankInfo().rank == m_value1;
              case 1: return player->GetHonorRankInfo().rank >= m_value1;
              case 2: return player->GetHonorRankInfo().rank <= m_value1;
            }
        }
        default:
            return false;
    }
}

// Which params must be provided to a Condition
bool PlayerCondition::CheckParamRequirements(Player const* pPlayer, Map const* map, WorldObject const* source, ConditionSource conditionSourceType) const
{
    switch (m_condition)
    {
        case CONDITION_NOT:
        case CONDITION_AND:
        case CONDITION_OR:
        case CONDITION_NONE:
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            break;
        case CONDITION_AREAID:
        case CONDITION_AREA_FLAG:
            if (!pPlayer && !source)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        case CONDITION_INSTANCE_SCRIPT:
            if (!pPlayer && !source && !map)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        case CONDITION_SOURCE_AURA:
        case CONDITION_LAST_WAYPOINT:
            if (!source)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        case CONDITION_DEAD_OR_AWAY:
            switch (m_value1)
            {
                case 0:                                     // Player dead or out of range
                case 1:                                     // All players in Group dead or out of range
                case 2:                                     // All players in instance dead or out of range
                    if (m_value2 && !source)
                    {
                        sLog.outErrorDb("CONDITION_DEAD_OR_AWAY %u - called from %s without source, but source expected for range check", m_entry, conditionSourceToStr[conditionSourceType]);
                        return false;
                    }
                    if (m_value1 != 2)
                    {
                        return true;
                    }
                    // Case 2 (Instance map only)
                    if (!map && (pPlayer || source))
                    {
                        map = source ? source->GetMap() : pPlayer->GetMap();
                    }
                    if (!map || !map->Instanceable())
                    {
                        sLog.outErrorDb("CONDITION_DEAD_OR_AWAY %u (Player in instance case) - called from %s without map param or from non-instanceable map %i", m_entry,  conditionSourceToStr[conditionSourceType], map ? map->GetId() : -1);
                        return false;
                    }
                case 3:                                     // Creature source is dead
                    return true;
            }
            break;
        case CONDITION_PVP_RANK:
            if (!pPlayer)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with %s, map %i, src %s",
                                m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
        default:
            if (!pPlayer)
            {
                sLog.outErrorDb("CONDITION %u type %u used with bad parameters, called from %s, used with plr: %s, map %i, src %s",
                                m_entry, m_condition, conditionSourceToStr[conditionSourceType], pPlayer ? pPlayer->GetGuidStr().c_str() : "NULL", map ? map->GetId() : -1, source ? source->GetGuidStr().c_str() : "NULL");
                return false;
            }
            break;
    }
    return true;
}

// Verification of condition values validity
bool PlayerCondition::IsValid(uint16 entry, ConditionType condition, uint32 value1, uint32 value2)
{
    switch (condition)
    {
        case CONDITION_NOT:
        {
            if (value1 >= entry)
            {
                sLog.outErrorDb("CONDITION_NOT (entry %u, type %d) has invalid value1 %u, must be lower than entry, skipped", entry, condition, value1);
                return false;
            }
            const PlayerCondition* condition1 = sConditionStorage.LookupEntry<PlayerCondition>(value1);
            if (!condition1)
            {
                sLog.outErrorDb("CONDITION_NOT (entry %u, type %d) has value1 %u without proper condition, skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_OR:
        case CONDITION_AND:
        {
            if (value1 >= entry)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has invalid value1 %u, must be lower than entry, skipped", entry, condition, value1);
                return false;
            }
            if (value2 >= entry)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has invalid value2 %u, must be lower than entry, skipped", entry, condition, value2);
                return false;
            }
            const PlayerCondition* condition1 = sConditionStorage.LookupEntry<PlayerCondition>(value1);
            if (!condition1)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has value1 %u without proper condition, skipped", entry, condition, value1);
                return false;
            }
            const PlayerCondition* condition2 = sConditionStorage.LookupEntry<PlayerCondition>(value2);
            if (!condition2)
            {
                sLog.outErrorDb("CONDITION _AND or _OR (entry %u, type %d) has value2 %u without proper condition, skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_AURA:
        case CONDITION_SOURCE_AURA:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }
            if (value2 >= MAX_EFFECT_INDEX)
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing effect index (%u) (must be 0..%u), skipped", entry, condition, value2, MAX_EFFECT_INDEX - 1);
                return false;
            }
            break;
        }
        case CONDITION_ITEM:
        case CONDITION_NOITEM:
        case CONDITION_ITEM_WITH_BANK:
        case CONDITION_NOITEM_WITH_BANK:
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
            if (!proto)
            {
                sLog.outErrorDb("Item condition (entry %u, type %u) requires to have non existing item (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 < 1)
            {
                sLog.outErrorDb("Item condition (entry %u, type %u) useless with count < 1, skipped", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_ITEM_EQUIPPED:
        {
            ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value1);
            if (!proto)
            {
                sLog.outErrorDb("ItemEquipped condition (entry %u, type %u) requires to have non existing item (%u) equipped, skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_AREAID:
        {
            AreaTableEntry const* areaEntry = GetAreaEntryByAreaID(value1);
            if (!areaEntry)
            {
                sLog.outErrorDb("Zone condition (entry %u, type %u) requires to be in non existing area (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Zone condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_REPUTATION_RANK_MIN:
        case CONDITION_REPUTATION_RANK_MAX:
        {
            FactionEntry const* factionEntry = sFactionStore.LookupEntry(value1);
            if (!factionEntry)
            {
                sLog.outErrorDb("Reputation condition (entry %u, type %u) requires to have reputation non existing faction (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 >= MAX_REPUTATION_RANK)
            {
                sLog.outErrorDb("Reputation condition (entry %u, type %u) has invalid rank requirement (value2 = %u) - must be between %u and %u, skipped", entry, condition, value2, MIN_REPUTATION_RANK, MAX_REPUTATION_RANK - 1);
                return false;
            }
            break;
        }
        case CONDITION_TEAM:
        {
            if (value1 != ALLIANCE && value1 != HORDE)
            {
                sLog.outErrorDb("Team condition (entry %u, type %u) specifies unknown team (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_SKILL:
        case CONDITION_SKILL_BELOW:
        {
            SkillLineEntry const* pSkill = sSkillLineStore.LookupEntry(value1);
            if (!pSkill)
            {
                sLog.outErrorDb("Skill condition (entry %u, type %u) specifies non-existing skill (%u), skipped", entry, condition, value1);
                return false;
            }
            if (value2 < 1 || value2 > sWorld.GetConfigMaxSkillValue())
            {
                sLog.outErrorDb("Skill condition (entry %u, type %u) specifies invalid skill value (%u), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_QUESTREWARDED:
        case CONDITION_QUESTTAKEN:
        case CONDITION_QUESTAVAILABLE:
        case CONDITION_QUEST_NONE:
        {
            Quest const* Quest = sObjectMgr.GetQuestTemplate(value1);
            if (!Quest)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) specifies non-existing quest (%u), skipped", entry, condition, value1);
                return false;
            }

            if (value2 && condition != CONDITION_QUESTTAKEN)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }
            break;
        }
        case CONDITION_AD_COMMISSION_AURA:
        {
            if (value1)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value1 (%u)!", entry, condition, value1);
            }
            if (value2)
            {
                sLog.outErrorDb("Quest condition (entry %u, type %u) has useless data in value2 (%u)!", entry, condition, value2);
            }
            break;
        }
        case CONDITION_NO_AURA:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }
            if (value2 > MAX_EFFECT_INDEX)
            {
                sLog.outErrorDb("Aura condition (entry %u, type %u) requires to have non existing effect index (%u) (must be 0..%u), skipped", entry, condition, value2, MAX_EFFECT_INDEX - 1);
                return false;
            }
            break;
        }
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        {
            if (!sGameEventMgr.IsValidEvent(value1))
            {
                sLog.outErrorDb("(Not)Active event condition (entry %u, type %u) requires existing event id (%u), skipped", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_AREA_FLAG:
        {
            if (!value1 && !value2)
            {
                sLog.outErrorDb("Area flag condition (entry %u, type %u) has both values like 0, skipped", entry, condition);
                return false;
            }
            break;
        }
        case CONDITION_RACE_CLASS:
        {
            if (!value1 && !value2)
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has both values like 0, skipped", entry, condition);
                return false;
            }

            if (value1 && !(value1 & RACEMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has invalid player class %u, skipped", entry, condition, value1);
                return false;
            }

            if (value2 && !(value2 & CLASSMASK_ALL_PLAYABLE))
            {
                sLog.outErrorDb("Race_class condition (entry %u, type %u) has invalid race mask %u, skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_LEVEL:
        {
            if (!value1 || value1 > sWorld.getConfig(CONFIG_UINT32_MAX_PLAYER_LEVEL))
            {
                sLog.outErrorDb("Level condition (entry %u, type %u)has invalid level %u, skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 2)
            {
                sLog.outErrorDb("Level condition (entry %u, type %u) has invalid argument %u (must be 0..2), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_SPELL:
        {
            if (!sSpellStore.LookupEntry(value1))
            {
                sLog.outErrorDb("Spell condition (entry %u, type %u) requires to have non existing spell (Id: %d), skipped", entry, condition, value1);
                return false;
            }

            if (value2 > 1)
            {
                sLog.outErrorDb("Spell condition (entry %u, type %u) has invalid argument %u (must be 0..1), skipped", entry, condition, value2);
                return false;
            }

            break;
        }
        case CONDITION_INSTANCE_SCRIPT:
            break;
        case CONDITION_RESERVED_1:
        case CONDITION_RESERVED_2:
        case CONDITION_RESERVED_3:
        case CONDITION_RESERVED_4:
        {
            sLog.outErrorDb("Condition (%u) reserved for later versions, skipped", condition);
            return false;
        }
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
            // no way check holidays in pre-3.x
            break;
        case CONDITION_LEARNABLE_ABILITY:
        {
            SkillLineAbilityMapBounds bounds = sSpellMgr.GetSkillLineAbilityMapBounds(value1);

            if (bounds.first == bounds.second)
            {
                sLog.outErrorDb("Learnable ability condition (entry %u, type %u) has spell id %u defined, but this spell is not listed in SkillLineAbility and can not be used, skipping.", entry, condition, value1);
                return false;
            }

            if (value2)
            {
                ItemPrototype const* proto = ObjectMgr::GetItemPrototype(value2);
                if (!proto)
                {
                    sLog.outErrorDb("Learnable ability condition (entry %u, type %u) has item entry %u defined but item does not exist, skipping.", entry, condition, value2);
                    return false;
                }
            }

            break;
        }
        case CONDITION_LAST_WAYPOINT:
        {
            if (value2 > 2)
            {
                sLog.outErrorDb("Last Waypoint condition (entry %u, type %u) has an invalid value in value2. (Has %u, supported 0, 1, or 2), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_GENDER:
        {
            if (value1 >= MAX_GENDER)
            {
                sLog.outErrorDb("Gender condition (entry %u, type %u) has an invalid value in value1. (Has %u, must be smaller than %u), skipping.", entry, condition, value1, MAX_GENDER);
                return false;
            }
            break;
        }
        case CONDITION_DEAD_OR_AWAY:
        {
            if (value1 >= 4)
            {
                sLog.outErrorDb("Dead condition (entry %u, type %u) has an invalid value in value1. (Has %u, must be smaller than 4), skipping.", entry, condition, value1);
                return false;
            }
            break;
        }
        case CONDITION_CREATURE_IN_RANGE:
        {
            if (!sCreatureStorage.LookupEntry<CreatureInfo> (value1))
            {
                sLog.outErrorDb("Creature in range condition (entry %u, type %u) has an invalid value in value1. (Creature %u does not exist in the database), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 <= 0)
            {
                sLog.outErrorDb("Creature in range condition (entry %u, type %u) has an invalid value in value2. (Range %u must be greater than 0), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_GAMEOBJECT_IN_RANGE:
        {
            if (!sGOStorage.LookupEntry<GameObjectInfo>(value1))
            {
                sLog.outErrorDb("Game object in range condition (entry %u, type %u) has an invalid value in value1 (gameobject). (Game object %u does not exist in the database), skipping.", entry, condition, value1);
                return false;
            }
            if (value2 <= 0)
            {
                sLog.outErrorDb("Game object in range condition (entry %u, type %u) has an invalid value in value2 (range). (Range %u must be greater than 0), skipping.", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_PVP_RANK:
        {
            if (value2 > 2)
            {
                sLog.outErrorDb("PVP rank condition (entry %u, type %u) has invalid argument %u (must be 0..2), skipped", entry, condition, value2);
                return false;
            }
            break;
        }
        case CONDITION_NONE:
            break;
        default:
            sLog.outErrorDb("Condition entry %u has bad type of %d, skipped ", entry, condition);
            return false;
    }
    return true;
}

// Check if a condition can be used without providing a player param
bool PlayerCondition::CanBeUsedWithoutPlayer(uint16 entry)
{
    PlayerCondition const* condition = sConditionStorage.LookupEntry<PlayerCondition>(entry);
    if (!condition)
    {
        return false;
    }

    switch (condition->m_condition)
    {
        case CONDITION_NOT:
            return CanBeUsedWithoutPlayer(condition->m_value1);
        case CONDITION_AND:
        case CONDITION_OR:
            return CanBeUsedWithoutPlayer(condition->m_value1) && CanBeUsedWithoutPlayer(condition->m_value2);
        case CONDITION_NONE:
        case CONDITION_ACTIVE_GAME_EVENT:
        case CONDITION_NOT_ACTIVE_GAME_EVENT:
        case CONDITION_ACTIVE_HOLIDAY:
        case CONDITION_NOT_ACTIVE_HOLIDAY:
        case CONDITION_AREAID:
        case CONDITION_AREA_FLAG:
        case CONDITION_INSTANCE_SCRIPT:
        case CONDITION_SOURCE_AURA:
        case CONDITION_LAST_WAYPOINT:
            return true;
        default:
            return false;
    }
}

SkillRangeType GetSkillRangeType(SkillLineEntry const* pSkill, bool racial)
{
    switch (pSkill->categoryId)
    {
        case SKILL_CATEGORY_LANGUAGES: return SKILL_RANGE_LANGUAGE;
        case SKILL_CATEGORY_WEAPON:
            if (pSkill->id != SKILL_FIST_WEAPONS)
            {
                return SKILL_RANGE_LEVEL;
            }
            else
            {
                return SKILL_RANGE_MONO;
            }
        case SKILL_CATEGORY_ARMOR:
        case SKILL_CATEGORY_CLASS:
            if (pSkill->id != SKILL_POISONS && pSkill->id != SKILL_LOCKPICKING)
            {
                return SKILL_RANGE_MONO;
            }
            else
            {
                return SKILL_RANGE_LEVEL;
            }
        case SKILL_CATEGORY_SECONDARY:
        case SKILL_CATEGORY_PROFESSION:
            // not set skills for professions and racial abilities
            if (IsProfessionSkill(pSkill->id))
            {
                return SKILL_RANGE_RANK;
            }
            else if (racial)
            {
                return SKILL_RANGE_NONE;
            }
            else
            {
                return SKILL_RANGE_MONO;
            }
        default:
        case SKILL_CATEGORY_ATTRIBUTES:                     // not found in dbc
        case SKILL_CATEGORY_GENERIC:                        // only GENERIC(DND)
            return SKILL_RANGE_NONE;
    }
}

void ObjectMgr::LoadGameTele()
{
    m_GameTeleMap.clear();                                  // for reload case

    uint32 count = 0;
    QueryResult* result = WorldDatabase.Query("SELECT `id`, `position_x`, `position_y`, `position_z`, `orientation`, `map`, `name` FROM `game_tele`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded `game_tele`, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        uint32 id         = fields[0].GetUInt32();

        GameTele gt;

        gt.position_x     = fields[1].GetFloat();
        gt.position_y     = fields[2].GetFloat();
        gt.position_z     = fields[3].GetFloat();
        gt.orientation    = fields[4].GetFloat();
        gt.mapId          = fields[5].GetUInt32();
        gt.name           = fields[6].GetCppString();

        if (!MapManager::IsValidMapCoord(gt.mapId, gt.position_x, gt.position_y, gt.position_z, gt.orientation))
        {
            sLog.outErrorDb("Wrong position for id %u (name: %s) in `game_tele` table, ignoring.", id, gt.name.c_str());
            continue;
        }

        if (!Utf8toWStr(gt.name, gt.wnameLow))
        {
            sLog.outErrorDb("Wrong UTF8 name for id %u in `game_tele` table, ignoring.", id);
            continue;
        }

        wstrToLower(gt.wnameLow);

        m_GameTeleMap[id] = gt;

        ++count;
    }
    while (result->NextRow());
    delete result;

    sLog.outString(">> Loaded %u GameTeleports", count);
    sLog.outString();
}

GameTele const* ObjectMgr::GetGameTele(const std::string& name) const
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return NULL;
    }

    // converting string that we try to find to lower case
    wstrToLower(wname);

    // Alternative first GameTele what contains wnameLow as substring in case no GameTele location found
    const GameTele* alt = NULL;
    for (GameTeleMap::const_iterator itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
        if (itr->second.wnameLow == wname)
        {
            return &itr->second;
        }
        else if (alt == NULL && itr->second.wnameLow.find(wname) != std::wstring::npos)
        {
            alt = &itr->second;
        }

    return alt;
}

bool ObjectMgr::AddGameTele(GameTele& tele)
{
    // find max id
    uint32 new_id = 0;
    for (GameTeleMap::const_iterator itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
        if (itr->first > new_id)
        {
            new_id = itr->first;
        }

    // use next
    ++new_id;

    if (!Utf8toWStr(tele.name, tele.wnameLow))
    {
        return false;
    }

    wstrToLower(tele.wnameLow);

    m_GameTeleMap[new_id] = tele;
    std::string safeName(tele.name);
    WorldDatabase.escape_string(safeName);

    return WorldDatabase.PExecuteLog("INSERT INTO `game_tele` "
                                     "(`id`,`position_x`,`position_y`,`position_z`,`orientation`,`map`,`name`) "
                                     "VALUES (%u,%f,%f,%f,%f,%u,'%s')",
                                     new_id, tele.position_x, tele.position_y, tele.position_z,
                                     tele.orientation, tele.mapId, safeName.c_str());
}

bool ObjectMgr::DeleteGameTele(const std::string& name)
{
    // explicit name case
    std::wstring wname;
    if (!Utf8toWStr(name, wname))
    {
        return false;
    }

    // converting string that we try to find to lower case
    wstrToLower(wname);

    for (GameTeleMap::iterator itr = m_GameTeleMap.begin(); itr != m_GameTeleMap.end(); ++itr)
    {
        if (itr->second.wnameLow == wname)
        {
            WorldDatabase.PExecuteLog("DELETE FROM `game_tele` WHERE `name` = '%s'", itr->second.name.c_str());
            m_GameTeleMap.erase(itr);
            return true;
        }
    }

    return false;
}

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
            if (trainerSpell.reqLevel == spellinfo->spellLevel)
            {
                ERROR_DB_STRICT_LOG("Table `%s` (Entry: %u) has redundant reqlevel %u (=spell level) for spell %u", tableName, entry, trainerSpell.reqLevel, spell);
            }
        }
        else
        {
            trainerSpell.reqLevel = spellinfo->spellLevel;
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
                    sLog.outErrorDb("Creature (Entry: %u) has TrainerTemplateId = %u for nonexistent trainer template", cInfo->Entry, cInfo->TrainerTemplateId);
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
                    sLog.outErrorDb("Creature (Entry: %u) has VendorTemplateId = %u for nonexistent vendor template", cInfo->Entry, cInfo->VendorTemplateId);
            }
        }
    }

    for (std::set<uint32>::const_iterator vItr = vendor_ids.begin(); vItr != vendor_ids.end(); ++vItr)
    {
        sLog.outErrorDb("Table `npc_vendor_template` has vendor template %u not used by any vendors ", *vItr);
    }
}

void ObjectMgr::LoadGossipMenu(std::set<uint32>& gossipScriptSet)
{
    m_mGossipMenusMap.clear();
    //                                                0      1        2
    QueryResult* result = WorldDatabase.Query("SELECT `entry`, `text_id`, `script_id`, "
                          //   3
                          "`condition_id` FROM `gossip_menu`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded gossip_menu, table is empty!");
        sLog.outString();
        return;
    }

    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        GossipMenus gMenu;

        gMenu.entry             = fields[0].GetUInt32();
        gMenu.text_id           = fields[1].GetUInt32();
        gMenu.script_id         = fields[2].GetUInt32();

        gMenu.conditionId       = fields[3].GetUInt16();

        if (!GetGossipText(gMenu.text_id))
        {
            sLog.outErrorDb("Table gossip_menu entry %u are using non-existing text_id %u", gMenu.entry, gMenu.text_id);
            continue;
        }

        // Check script-id
        if (gMenu.script_id)
        {
            ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(DBS_ON_GOSSIP);
            if (!scm)
            {
                continue;
            }

            if (scm->find(gMenu.script_id) == scm->end())
            {
                sLog.outErrorDb("Table gossip_menu for menu %u, text-id %u have script_id %u that does not exist in `db_scripts [type = %d]`, ignoring", gMenu.entry, gMenu.text_id, gMenu.script_id, DBS_ON_GOSSIP);
                continue;
            }

            // Remove used script id
            gossipScriptSet.erase(gMenu.script_id);
        }

        if (gMenu.conditionId)
        {
            const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(gMenu.conditionId);
            if (!condition)
            {
                sLog.outErrorDb("Table gossip_menu for menu %u, text-id %u has condition_id %u that does not exist in `conditions`, ignoring", gMenu.entry, gMenu.text_id, gMenu.conditionId);
                gMenu.conditionId = 0;
            }
        }

        m_mGossipMenusMap.insert(GossipMenusMap::value_type(gMenu.entry, gMenu));

        ++count;
    }
    while (result->NextRow());

    delete result;

    // post loading tests
    for (uint32 i = 1; i < sCreatureStorage.GetMaxEntry(); ++i)
    {
        if (CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i))
        {
            if (cInfo->GossipMenuId)
            {
                if (m_mGossipMenusMap.find(cInfo->GossipMenuId) == m_mGossipMenusMap.end())
                {
                    sLog.outErrorDb("Creature (Entry: %u) has GossipMenuId = %u for nonexistent menu", cInfo->Entry, cInfo->GossipMenuId);
                }
            }
        }
    }

    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
        {
            if (uint32 menuid = itr->GetGossipMenuId())
            {
                if (m_mGossipMenusMap.find(menuid) == m_mGossipMenusMap.end())
                {
                    sLog.outErrorDb("Gameobject (Entry: %u) has gossip_menu_id = %u for nonexistent menu", itr->id, menuid);
                }
            }
        }
    }

    sLog.outString(">> Loaded %u gossip_menu entries", count);
    sLog.outString();
}

void ObjectMgr::LoadGossipMenuItems(std::set<uint32>& gossipScriptSet)
{
    m_mGossipMenuItemsMap.clear();

    QueryResult* result = WorldDatabase.Query(
                              "SELECT `menu_id`, `id`, `option_icon`, `option_text`, `option_id`, `npc_option_npcflag`, "
                              "`action_menu_id`, `action_poi_id`, `action_script_id`, `box_coded`, `box_money`, `box_text`, "
                              "`condition_id` "
                              "FROM `gossip_menu_option` ORDER BY `menu_id`, `id`");

    if (!result)
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outErrorDb(">> Loaded gossip_menu_option, table is empty!");
        sLog.outString();
        return;
    }

    // prepare data for unused menu ids
    std::set<uint32> menu_ids;                              // for later integrity check
    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))   // check unused menu ids only in strict mode
    {
        for (GossipMenusMap::const_iterator itr = m_mGossipMenusMap.begin(); itr != m_mGossipMenusMap.end(); ++itr)
            if (itr->first)
            {
                menu_ids.insert(itr->first);
            }

        for (SQLStorageBase::SQLSIterator<GameObjectInfo> itr = sGOStorage.getDataBegin<GameObjectInfo>(); itr < sGOStorage.getDataEnd<GameObjectInfo>(); ++itr)
            if (uint32 menuid = itr->GetGossipMenuId())
            {
                menu_ids.erase(menuid);
            }
    }

    // loading
    BarGoLink bar(result->GetRowCount());

    uint32 count = 0;

    // prepare menuid -> CreatureInfo map for fast access
    typedef  std::multimap<uint32, const CreatureInfo*> Menu2CInfoMap;
    Menu2CInfoMap menu2CInfoMap;
    for (uint32 i = 1;  i < sCreatureStorage.GetMaxEntry(); ++i)
        if (CreatureInfo const* cInfo = sCreatureStorage.LookupEntry<CreatureInfo>(i))
            if (cInfo->GossipMenuId)
            {
                menu2CInfoMap.insert(Menu2CInfoMap::value_type(cInfo->GossipMenuId, cInfo));

                // unused check data preparing part
                if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
                {
                    menu_ids.erase(cInfo->GossipMenuId);
                }
            }

    do
    {
        bar.step();

        Field* fields = result->Fetch();

        GossipMenuItems gMenuItem;

        gMenuItem.menu_id               = fields[0].GetUInt32();
        gMenuItem.id                    = fields[1].GetUInt32();
        gMenuItem.option_icon           = fields[2].GetUInt8();
        gMenuItem.option_text           = fields[3].GetCppString();
        gMenuItem.option_id             = fields[4].GetUInt32();
        gMenuItem.npc_option_npcflag    = fields[5].GetUInt32();
        gMenuItem.action_menu_id        = fields[6].GetInt32();
        gMenuItem.action_poi_id         = fields[7].GetUInt32();
        gMenuItem.action_script_id      = fields[8].GetUInt32();
        gMenuItem.box_coded             = fields[9].GetUInt8() != 0;
        gMenuItem.box_text              = fields[11].GetCppString();

        gMenuItem.conditionId           = fields[12].GetUInt16();

        if (gMenuItem.menu_id)                              // == 0 id is special and not have menu_id data
        {
            if (m_mGossipMenusMap.find(gMenuItem.menu_id) == m_mGossipMenusMap.end())
            {
                sLog.outErrorDb("Gossip menu option (MenuId: %u) for nonexistent menu", gMenuItem.menu_id);
                continue;
            }
        }

        if (gMenuItem.action_menu_id > 0)
        {
            if (m_mGossipMenusMap.find(gMenuItem.action_menu_id) == m_mGossipMenusMap.end())
            {
                sLog.outErrorDb("Gossip menu option (MenuId: %u Id: %u) have action_menu_id = %u for nonexistent menu", gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_menu_id);
            }
            else if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
            {
                menu_ids.erase(gMenuItem.action_menu_id);
            }
        }

        if (gMenuItem.option_icon >= GOSSIP_ICON_MAX)
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has unknown icon id %u. Replacing with GOSSIP_ICON_CHAT", gMenuItem.menu_id, gMenuItem.id, gMenuItem.option_icon);
            gMenuItem.option_icon = GOSSIP_ICON_CHAT;
        }

        if (gMenuItem.option_id == GOSSIP_OPTION_NONE)
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u use option id GOSSIP_OPTION_NONE. Option will never be used", gMenuItem.menu_id, gMenuItem.id);
        }

        if (gMenuItem.option_id >= GOSSIP_OPTION_MAX)
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has unknown option id %u. Option will not be used", gMenuItem.menu_id, gMenuItem.id, gMenuItem.option_id);
        }

        if (gMenuItem.menu_id && gMenuItem.npc_option_npcflag)
        {
            bool found_menu_uses = false;
            bool found_flags_uses = false;

            std::pair<Menu2CInfoMap::const_iterator, Menu2CInfoMap::const_iterator> tm_bounds = menu2CInfoMap.equal_range(gMenuItem.menu_id);
            for (Menu2CInfoMap::const_iterator it2 = tm_bounds.first; !found_flags_uses && it2 != tm_bounds.second; ++it2)
            {
                CreatureInfo const* cInfo = it2->second;

                found_menu_uses = true;

                // some from creatures with gossip menu can use gossip option base at npc_flags
                if (gMenuItem.npc_option_npcflag & cInfo->NpcFlags)
                {
                    found_flags_uses = true;
                }
            }

            if (found_menu_uses && !found_flags_uses)
            {
                sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has `npc_option_npcflag` = %u but creatures using this menu does not have corresponding `NpcFlags`. Option will not accessible in game.", gMenuItem.menu_id, gMenuItem.id, gMenuItem.npc_option_npcflag);
            }
        }

        if (gMenuItem.action_poi_id && !GetPointOfInterest(gMenuItem.action_poi_id))
        {
            sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u use non-existing action_poi_id %u, ignoring", gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_poi_id);
            gMenuItem.action_poi_id = 0;
        }

        if (gMenuItem.action_script_id)
        {
            ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(DBS_ON_GOSSIP);
            if (!scm)
            {
                continue;
            }

            if (scm->find(gMenuItem.action_script_id) == scm->end())
            {
                sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u have action_script_id %u that does not exist in `db_scripts [type = %d]`, ignoring", gMenuItem.menu_id, gMenuItem.id, gMenuItem.action_script_id, DBS_ON_GOSSIP);
                continue;
            }

            // Remove used script id
            gossipScriptSet.erase(gMenuItem.action_script_id);
        }

        if (gMenuItem.conditionId)
        {
            const PlayerCondition* condition = sConditionStorage.LookupEntry<PlayerCondition>(gMenuItem.conditionId);
            if (!condition)
            {
                sLog.outErrorDb("Table gossip_menu_option for menu %u, id %u has condition_id %u that does not exist in `conditions`, ignoring", gMenuItem.menu_id, gMenuItem.id, gMenuItem.conditionId);
                gMenuItem.conditionId = 0;
            }
        }

        m_mGossipMenuItemsMap.insert(GossipMenuItemsMap::value_type(gMenuItem.menu_id, gMenuItem));

        ++count;
    }
    while (result->NextRow());

    delete result;

    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (std::set<uint32>::const_iterator itr = menu_ids.begin(); itr != menu_ids.end(); ++itr)
        {
            sLog.outErrorDb("Table `gossip_menu` contain unused (in creature or GO or menu options) menu id %u.", *itr);
        }
    }

    sLog.outString(">> Loaded %u gossip_menu_option entries", count);
    sLog.outString();
}

void ObjectMgr::LoadGossipMenus()
{
    ScriptChainMap const* scm = sScriptMgr.GetScriptChainMap(DBS_ON_GOSSIP);
    if (!scm)
    {
        return;
    }

    // Check which script-ids in db_scripts type DBS_ON_GOSSIP are not used
    std::set<uint32> gossipScriptSet;
    for (ScriptChainMap::const_iterator itr = scm->begin(); itr != scm->end(); ++itr)
    {
        gossipScriptSet.insert(itr->first);
    }

    // Load gossip_menu and gossip_menu_option data
    sLog.outString("(Re)Loading Gossip menus...");
    LoadGossipMenu(gossipScriptSet);
    sLog.outString("(Re)Loading Gossip menu options...");
    LoadGossipMenuItems(gossipScriptSet);

    for (std::set<uint32>::const_iterator itr = gossipScriptSet.begin(); itr != gossipScriptSet.end(); ++itr)
    {
        sLog.outErrorDb("Table `db_scripts [type = %d]` contains unused script, id %u.", DBS_ON_GOSSIP, *itr);
    }
}

void ObjectMgr::AddVendorItem(uint32 entry, uint32 item, uint32 maxcount, uint32 incrtime)
{
    VendorItemData& vList = m_mCacheVendorItemMap[entry];
    vList.AddItem(item, maxcount, incrtime, 0);

    WorldDatabase.PExecuteLog("INSERT INTO `npc_vendor` (`entry`,`item`,`maxcount`,`incrtime`) VALUES('%u','%u','%u','%u')", entry, item, maxcount, incrtime);
}

bool ObjectMgr::RemoveVendorItem(uint32 entry, uint32 item)
{
    CacheVendorItemMap::iterator  iter = m_mCacheVendorItemMap.find(entry);
    if (iter == m_mCacheVendorItemMap.end())
    {
        return false;
    }

    if (!iter->second.RemoveItem(item))
    {
        return false;
    }

    WorldDatabase.PExecuteLog("DELETE FROM `npc_vendor` WHERE `entry`='%u' AND `item`='%u'", entry, item);
    return true;
}

bool ObjectMgr::IsVendorItemValid(bool isTemplate, char const* tableName, uint32 vendor_entry, uint32 item_id, uint32 maxcount, uint32 incrtime, uint16 conditionId, Player* pl, std::set<uint32>* skip_vendors) const
{
    char const* idStr = isTemplate ? "vendor template" : "vendor";
    CreatureInfo const* cInfo = NULL;

    if (!isTemplate)
    {
        cInfo = GetCreatureTemplate(vendor_entry);
        if (!cInfo)
        {
            if (pl)
            {
                ChatHandler(pl).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
            }
            else
            {
                sLog.outErrorDb("Table `%s` has data for nonexistent creature (Entry: %u), ignoring", tableName, vendor_entry);
            }
            return false;
        }

        if (!(cInfo->NpcFlags & UNIT_NPC_FLAG_VENDOR))
        {
            if (!skip_vendors || skip_vendors->count(vendor_entry) == 0)
            {
                if (pl)
                {
                    ChatHandler(pl).SendSysMessage(LANG_COMMAND_VENDORSELECTION);
                }
                else
                {
                    sLog.outErrorDb("Table `%s` has data for creature (Entry: %u) without vendor flag, ignoring", tableName, vendor_entry);
                }

                if (skip_vendors)
                {
                    skip_vendors->insert(vendor_entry);
                }
            }
            return false;
        }
    }

    if (!GetItemPrototype(item_id))
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage(LANG_ITEM_NOT_FOUND, item_id);
        }
        else
            sLog.outErrorDb("Table `%s` for %s %u contain nonexistent item (%u), ignoring",
                            tableName, idStr, vendor_entry, item_id);
        return false;
    }

    if (maxcount > 0 && incrtime == 0)
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage("MaxCount!=0 (%u) but IncrTime==0", maxcount);
        }
        else
            sLog.outErrorDb("Table `%s` has `maxcount` (%u) for item %u of %s %u but `incrtime`=0, ignoring",
                            tableName, maxcount, item_id, idStr, vendor_entry);
        return false;
    }
    else if (maxcount == 0 && incrtime > 0)
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage("MaxCount==0 but IncrTime<>=0");
        }
        else
            sLog.outErrorDb("Table `%s` has `maxcount`=0 for item %u of %s %u but `incrtime`<>0, ignoring",
                            tableName, item_id, idStr, vendor_entry);
        return false;
    }

    if (conditionId && !sConditionStorage.LookupEntry<PlayerCondition>(conditionId))
    {
        sLog.outErrorDb("Table `%s` has `condition_id`=%u for item %u of %s %u but this condition is not valid, ignoring", tableName, conditionId, item_id, idStr, vendor_entry);
        return false;
    }

    VendorItemData const* vItems = isTemplate ? GetNpcVendorTemplateItemList(vendor_entry) : GetNpcVendorItemList(vendor_entry);
    VendorItemData const* tItems = isTemplate ? NULL : GetNpcVendorTemplateItemList(vendor_entry);

    if (!vItems && !tItems)
    {
        return true;                                        // later checks for non-empty lists
    }

    if (vItems && vItems->FindItem(item_id))
    {
        if (pl)
        {
            ChatHandler(pl).PSendSysMessage(LANG_ITEM_ALREADY_IN_LIST, item_id);
        }
        else
        {
            sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u, ignoring",
                            tableName, item_id, idStr, vendor_entry);
        }
        return false;
    }

    if (!isTemplate)
    {
        if (tItems && tItems->GetItem(item_id))
        {
            if (pl)
            {
                ChatHandler(pl).PSendSysMessage(LANG_ITEM_ALREADY_IN_LIST, item_id);
            }
            else
            {
                if (!cInfo->VendorTemplateId)
                    sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u, ignoring",
                                    tableName, item_id, idStr, vendor_entry);
                else
                    sLog.outErrorDb("Table `%s` has duplicate items %u for %s %u (or possible in vendor template %u), ignoring",
                                    tableName, item_id, idStr, vendor_entry, cInfo->VendorTemplateId);
            }
            return false;
        }
    }

    uint32 countItems = vItems ? vItems->GetItemCount() : 0;
    countItems += tItems ? tItems->GetItemCount() : 0;

    if (countItems >= MAX_VENDOR_ITEMS)
    {
        if (pl)
        {
            ChatHandler(pl).SendSysMessage(LANG_COMMAND_ADDVENDORITEMITEMS);
        }
        else
            sLog.outErrorDb("Table `%s` has too many items (%u >= %i) for %s %u, ignoring",
                            tableName, countItems, MAX_VENDOR_ITEMS, idStr, vendor_entry);
        return false;
    }

    return true;
}

void ObjectMgr::AddGroup(Group* group)
{
    mGroupMap[group->GetId()] = group ;
}

void ObjectMgr::RemoveGroup(Group* group)
{
    mGroupMap.erase(group->GetId());
}

void ObjectMgr::GetCreatureLocaleStrings(uint32 entry, int32 loc_idx, char const** namePtr, char const** subnamePtr) const
{
    if (loc_idx >= 0)
    {
        if (CreatureLocale const *il = GetCreatureLocale(entry))
        {
            if (namePtr && il->Name.size() > size_t(loc_idx) && !il->Name[loc_idx].empty())
            {
                *namePtr = il->Name[loc_idx].c_str();
            }

            if (subnamePtr && il->SubName.size() > size_t(loc_idx) && !il->SubName[loc_idx].empty())
            {
                *subnamePtr = il->SubName[loc_idx].c_str();
            }
        }
    }
}

void ObjectMgr::GetItemLocaleStrings(uint32 entry, int32 loc_idx, std::string* namePtr, std::string* descriptionPtr) const
{
    if (loc_idx >= 0)
    {
        if (ItemLocale const *il = GetItemLocale(entry))
        {
            if (namePtr && il->Name.size() > size_t(loc_idx) && !il->Name[loc_idx].empty())
            {
                *namePtr = il->Name[loc_idx];
            }

            if (descriptionPtr && il->Description.size() > size_t(loc_idx) && !il->Description[loc_idx].empty())
            {
                *descriptionPtr = il->Description[loc_idx];
            }
        }
    }
}

void ObjectMgr::GetQuestLocaleStrings(uint32 entry, int32 loc_idx, std::string* titlePtr) const
{
    if (loc_idx >= 0)
    {
        if (QuestLocale const *il = GetQuestLocale(entry))
        {
            if (titlePtr && il->Title.size() > size_t(loc_idx) && !il->Title[loc_idx].empty())
            {
                *titlePtr = il->Title[loc_idx];
            }
        }
    }
}

void ObjectMgr::GetNpcTextLocaleStringsAll(uint32 entry, int32 loc_idx, ObjectMgr::NpcTextArray* text0_Ptr, ObjectMgr::NpcTextArray* text1_Ptr) const
{
    if (loc_idx >= 0)
    {
        if (NpcTextLocale const *nl = GetNpcTextLocale(entry))
        {
            if (text0_Ptr)
                for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
                    if (nl->Text_0[i].size() > (size_t)loc_idx && !nl->Text_0[i][loc_idx].empty())
                    {
                        (*text0_Ptr)[i] = nl->Text_0[i][loc_idx];
                    }

            if (text1_Ptr)
                for (int i = 0; i < MAX_GOSSIP_TEXT_OPTIONS; ++i)
                    if (nl->Text_1[i].size() > (size_t)loc_idx && !nl->Text_1[i][loc_idx].empty())
                    {
                        (*text1_Ptr)[i] = nl->Text_1[i][loc_idx];
                    }
        }
    }
}

void ObjectMgr::GetNpcTextLocaleStrings0(uint32 entry, int32 loc_idx, std::string* text0_0_Ptr, std::string* text1_0_Ptr) const
{
    if (loc_idx >= 0)
    {
        if (NpcTextLocale const *nl = GetNpcTextLocale(entry))
        {
            if (text0_0_Ptr)
                if (nl->Text_0[0].size() > (size_t)loc_idx && !nl->Text_0[0][loc_idx].empty())
                {
                    *text0_0_Ptr = nl->Text_0[0][loc_idx];
                }

            if (text1_0_Ptr)
                if (nl->Text_1[0].size() > (size_t)loc_idx && !nl->Text_1[0][loc_idx].empty())
                {
                    *text1_0_Ptr = nl->Text_1[0][loc_idx];
                }
        }
    }
}

// Functions for scripting access
bool LoadMangosStrings(DatabaseType& db, char const* table, int32 start_value, int32 end_value, bool extra_content)
{
    // MAX_DB_SCRIPT_STRING_ID is max allowed negative value for scripts (scrpts can use only more deep negative values
    // start/end reversed for negative values
    if (start_value > MAX_DB_SCRIPT_STRING_ID || end_value >= start_value)
    {
        sLog.outErrorDb("Table '%s' attempt loaded with reserved by mangos range (%d - %d), strings not loaded.", table, start_value, end_value + 1);
        return false;
    }

    return sObjectMgr.LoadMangosStrings(db, table, start_value, end_value, extra_content);
}

void ObjectMgr::LoadCreatureTemplateSpells()
{
    sCreatureTemplateSpellsStorage.Load();

    for (SQLStorageBase::SQLSIterator<CreatureTemplateSpells> itr = sCreatureTemplateSpellsStorage.getDataBegin<CreatureTemplateSpells>(); itr < sCreatureTemplateSpellsStorage.getDataEnd<CreatureTemplateSpells>(); ++itr)
    {
        if (!sCreatureStorage.LookupEntry<CreatureInfo>(itr->entry))
        {
            sLog.outErrorDb("LoadCreatureTemplateSpells: Spells found for creature entry %u, but creature does not exist, skipping", itr->entry);
            sCreatureTemplateSpellsStorage.EraseEntry(itr->entry);
        }
        for (uint8 i = 0; i < CREATURE_MAX_SPELLS; ++i)
        {
            if (itr->spells[i] && !sSpellStore.LookupEntry(itr->spells[i]))
            {
                sLog.outErrorDb("LoadCreatureTemplateSpells: Spells found for creature entry %u, assigned spell %u does not exist, set to 0", itr->entry, itr->spells[i]);
                const_cast<CreatureTemplateSpells*>(*itr)->spells[i] = 0;
            }
        }
    }

    sLog.outString(">> Loaded %u creature_template_spells definitions", sCreatureTemplateSpellsStorage.GetRecordCount());
    sLog.outString();
}

CreatureInfo const* GetCreatureTemplateStore(uint32 entry)
{
    return sCreatureStorage.LookupEntry<CreatureInfo>(entry);
}

Quest const* GetQuestTemplateStore(uint32 entry)
{
    return sObjectMgr.GetQuestTemplate(entry);
}

MangosStringLocale const* GetMangosStringData(int32 entry)
{
    return sObjectMgr.GetMangosStringLocale(entry);
}

bool FindCreatureData::operator()(CreatureDataPair const& dataPair)
{
    // skip wrong entry ids
    if (i_id && dataPair.second.id != i_id)
    {
        return false;
    }

    if (!i_anyData)
    {
        i_anyData = &dataPair;
    }

    // without player we can't find more stricted cases, so use fouded
    if (!i_player)
    {
        return true;
    }

    // skip diff. map cases
    if (dataPair.second.mapid != i_player->GetMapId())
    {
        return false;
    }

    float new_dist = i_player->GetDistance2d(dataPair.second.posX, dataPair.second.posY);

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    // skip not spawned (in any state),
    uint16 pool_id = sPoolMgr.IsPartOfAPool<Creature>(dataPair.first);
    if (pool_id && !i_player->GetMap()->GetPersistentState()->IsSpawnedPoolObject<Creature>(dataPair.first))
    {
        return false;
    }

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

CreatureDataPair const* FindCreatureData::GetResult() const
{
    if (i_spawnedData)
    {
        return i_spawnedData;
    }

    if (i_mapData)
    {
        return i_mapData;
    }

    return i_anyData;
}

bool FindGOData::operator()(GameObjectDataPair const& dataPair)
{
    // skip wrong entry ids
    if (i_id && dataPair.second.id != i_id)
    {
        return false;
    }

    if (!i_anyData)
    {
        i_anyData = &dataPair;
    }

    // without player we can't find more stricted cases, so use fouded
    if (!i_player)
    {
        return true;
    }

    // skip diff. map cases
    if (dataPair.second.mapid != i_player->GetMapId())
    {
        return false;
    }

    float new_dist = i_player->GetDistance2d(dataPair.second.posX, dataPair.second.posY);

    if (!i_mapData || new_dist < i_mapDist)
    {
        i_mapData = &dataPair;
        i_mapDist = new_dist;
    }

    // skip not spawned (in any state)
    uint16 pool_id = sPoolMgr.IsPartOfAPool<GameObject>(dataPair.first);
    if (pool_id && !i_player->GetMap()->GetPersistentState()->IsSpawnedPoolObject<GameObject>(dataPair.first))
    {
        return false;
    }

    if (!i_spawnedData || new_dist < i_spawnedDist)
    {
        i_spawnedData = &dataPair;
        i_spawnedDist = new_dist;
    }

    return false;
}

GameObjectDataPair const* FindGOData::GetResult() const
{
    if (i_mapData)
    {
        return i_mapData;
    }

    if (i_spawnedData)
    {
        return i_spawnedData;
    }

    return i_anyData;
}

bool DoDisplayText(WorldObject* source, int32 entry, Unit const* target /*=NULL*/)
{
    MangosStringLocale const* data = sObjectMgr.GetMangosStringLocale(entry);

    if (!data)
    {
        _DoStringError(entry, "DoScriptText with source %s could not find text entry %i.", source->GetGuidStr().c_str(), entry);
        return false;
    }

    if (data->SoundId)
    {
        if (data->Type == CHAT_TYPE_ZONE_YELL)
        {
            source->GetMap()->PlayDirectSoundToMap(data->SoundId, source->GetZoneId());
        }
        else if (data->Type == CHAT_TYPE_WHISPER || data->Type == CHAT_TYPE_BOSS_WHISPER)
        {
            // An error will be displayed for the text
            if (target && target->GetTypeId() == TYPEID_PLAYER)
            {
                source->PlayDirectSound(data->SoundId, (Player const*)target);
            }
        }
        else
        {
            source->PlayDirectSound(data->SoundId);
        }
    }

    if (data->Emote)
    {
        if (source->GetTypeId() == TYPEID_UNIT || source->GetTypeId() == TYPEID_PLAYER)
        {
            ((Unit*)source)->HandleEmote(data->Emote);
        }
        else
        {
            _DoStringError(entry, "DoDisplayText entry %i tried to process emote for invalid source %s", entry, source->GetGuidStr().c_str());
            return false;
        }
    }

    if ((data->Type == CHAT_TYPE_WHISPER || data->Type == CHAT_TYPE_BOSS_WHISPER) && (!target || target->GetTypeId() != TYPEID_PLAYER))
    {
        _DoStringError(entry, "DoDisplayText entry %i can not whisper without target unit (TYPEID_PLAYER).", entry);
        return false;
    }

    source->MonsterText(data, target);
    return true;
}
