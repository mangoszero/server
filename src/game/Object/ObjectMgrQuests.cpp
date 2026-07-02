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
#include "LivingWorldAnchorPolicy.h"
#include "MotionGenerators/MotionMaster.h"
#include "Database/DatabaseEnv.h"
#include "Policies/Singleton.h"
#include "SQLStorages.h"
#include "Log.h"
#include "MapManager.h"
#include "ObjectGuid.h"
#include "ScriptMgr.h"
#include "SpellMgr.h"
#include "UpdateMask.h"
#include "World.h"
#include "Group.h"
#include "Transports.h"
#include "ProgressBar.h"
#include "Language.h"
#include "PoolManager.h"
#include "GameEventMgr.h"
#include "Spell.h"
#include "Chat.h"
#include "AccountMgr.h"
#include "MapPersistentStateMgr.h"
#include "SpellAuras.h"
#include "Util.h"
#include "WaypointManager.h"
#include "GossipDef.h"
#include "Mail.h"
#include "InstanceData.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "DisableMgr.h"
#include "AuctionHouseBot/AhBotSystemOwner.h"
#include "Formulas.h"
#include "ItemEnchantmentMgr.h"
#include <limits>
#include <set>

/**
 * @brief Loads quest templates and validates quest-related database references.
 */
void ObjectMgr::LoadQuests()
{
    // For reload case
    for (QuestMap::const_iterator itr = mQuestTemplates.begin(); itr != mQuestTemplates.end(); ++itr)
    {
        delete itr->second;
    }

    mQuestTemplates.clear();

    m_ExclusiveQuestGroups.clear();

    QueryResult* result = WorldDatabase.Query(
        //           0        1         2             3           4             5       6                  7                8                9
            "SELECT `entry`, `Method`, `ZoneOrSort`, `MinLevel`, `QuestLevel`, `Type`, `RequiredClasses`, `RequiredRaces`, `RequiredSkill`, `RequiredSkillValue`,"
        //    10                     11                   12                       13                     14                       15                     16                  17
            "`RepObjectiveFaction`, `RepObjectiveValue`, `RequiredMinRepFaction`, `RequiredMinRepValue`, `RequiredMaxRepFaction`, `RequiredMaxRepValue`, `SuggestedPlayers`, `LimitTime`,"
        //    18            19              20             21             22                23                  24           25              26
            "`QuestFlags`, `SpecialFlags`, `PrevQuestId`, `NextQuestId`, `ExclusiveGroup`, `NextQuestInChain`, `SrcItemId`, `SrcItemCount`, `SrcSpell`,"
        //    27       28         29            30                 31                  32         33                34                35                36
            "`Title`, `Details`, `Objectives`, `OfferRewardText`, `RequestItemsText`, `EndText`, `ObjectiveText1`, `ObjectiveText2`, `ObjectiveText3`, `ObjectiveText4`,"
        //    37            38            39            40            41               42               43               44
            "`ReqItemId1`, `ReqItemId2`, `ReqItemId3`, `ReqItemId4`, `ReqItemCount1`, `ReqItemCount2`, `ReqItemCount3`, `ReqItemCount4`,"
        //    45              46              47              48              49                 50                 51                 52
            "`ReqSourceId1`, `ReqSourceId2`, `ReqSourceId3`, `ReqSourceId4`, `ReqSourceCount1`, `ReqSourceCount2`, `ReqSourceCount3`, `ReqSourceCount4`,"
        //    53                    54                    55                    56                    57                       58                       59                       60
            "`ReqCreatureOrGOId1`, `ReqCreatureOrGOId2`, `ReqCreatureOrGOId3`, `ReqCreatureOrGOId4`, `ReqCreatureOrGOCount1`, `ReqCreatureOrGOCount2`, `ReqCreatureOrGOCount3`, `ReqCreatureOrGOCount4`,"
        //    61               62               63               64
            "`ReqSpellCast1`, `ReqSpellCast2`, `ReqSpellCast3`, `ReqSpellCast4`,"
        //    65                  66                  67                  68                  69                  70
            "`RewChoiceItemId1`, `RewChoiceItemId2`, `RewChoiceItemId3`, `RewChoiceItemId4`, `RewChoiceItemId5`, `RewChoiceItemId6`,"
        //    71                     72                     73                     74                     75                     76
            "`RewChoiceItemCount1`, `RewChoiceItemCount2`, `RewChoiceItemCount3`, `RewChoiceItemCount4`, `RewChoiceItemCount5`, `RewChoiceItemCount6`,"
        //    77            78            79            80            81               82               83               84
            "`RewItemId1`, `RewItemId2`, `RewItemId3`, `RewItemId4`, `RewItemCount1`, `RewItemCount2`, `RewItemCount3`, `RewItemCount4`,"
        //    85                86                87                88                89                90              91              92              93              94
            "`RewRepFaction1`, `RewRepFaction2`, `RewRepFaction3`, `RewRepFaction4`, `RewRepFaction5`, `RewRepValue1`, `RewRepValue2`, `RewRepValue3`, `RewRepValue4`, `RewRepValue5`,"
        //    95               96                  97          98              99                   100                 101           102       103       104
            "`RewOrReqMoney`, `RewMoneyMaxLevel`, `RewSpell`, `RewSpellCast`, `RewMailTemplateId`, `RewMailDelaySecs`, `PointMapId`, `PointX`, `PointY`, `PointOpt`,"
        //    105              106              107              108              109                   110                   111                   112
            "`DetailsEmote1`, `DetailsEmote2`, `DetailsEmote3`, `DetailsEmote4`, `DetailsEmoteDelay1`, `DetailsEmoteDelay2`, `DetailsEmoteDelay3`, `DetailsEmoteDelay4`,"
        //    113                114              115                  116                  117                  118
            "`IncompleteEmote`, `CompleteEmote`, `OfferRewardEmote1`, `OfferRewardEmote2`, `OfferRewardEmote3`, `OfferRewardEmote4`,"
        //    119                       120                       121                       122
            "`OfferRewardEmoteDelay1`, `OfferRewardEmoteDelay2`, `OfferRewardEmoteDelay3`, `OfferRewardEmoteDelay4`,"
        //    123            124
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

    sLog.outString(">> Loaded %zu quests definitions", mQuestTemplates.size());
    sLog.outString();
}

/**
 * @brief Loads localized quest texts and objective strings.
 */
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
