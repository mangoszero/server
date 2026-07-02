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
#include "Log.h"
#include "ProgressBar.h"
#include "SQLStorages.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "Policies/Singleton.h"
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
 * @brief Loads quest relation mappings for a specific actor and role.
 *
 * @param map The destination relations map.
 * @param actor The quest actor type.
 * @param role The quest relation role.
 */
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

/**
 * @brief Loads quest-giver relations for gameobjects.
 */
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

/**
 * @brief Loads quest-completion relations for gameobjects.
 */
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

/**
 * @brief Loads quest-giver relations for creatures.
 */
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

/**
 * @brief Loads quest-completion relations for creatures.
 */
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

/**
 * @brief Builds the set of gameobject entries that must activate for quests.
 */
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
