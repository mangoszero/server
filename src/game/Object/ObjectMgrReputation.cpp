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
#include "DBCStores.h"
#include "Log.h"
#include "ProgressBar.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "SQLStorages.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
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
 * @brief Loads faction-specific reputation reward rate multipliers.
 */
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

/**
 * @brief Loads creature reputation rewards granted on kill.
 */
void ObjectMgr::LoadReputationOnKill()
{
    uint32 count = 0;

    QueryResult* result = WorldDatabase.Query(
        //           0              1                       2
            "SELECT `creature_id`, `RewOnKillRepFaction1`, `RewOnKillRepFaction2`,"
        //    3               4               5                     6               7               8                     9
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

/**
 * @brief Loads reputation spillover templates for shared faction gains.
 */
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
