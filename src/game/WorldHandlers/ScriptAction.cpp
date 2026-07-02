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

/**
 * @file ScriptMgr.cpp
 * @brief Script system manager implementation
 *
 * This file implements ScriptMgr which manages all game scripts:
 * - Creature AI scripts
 * - GameObject scripts
 * - Item scripts
 * - Area trigger scripts
 * - Spell scripts
 * - Quest scripts
 * - Instance scripts
 *
 * Scripts are loaded from script libraries and provide hooks for
 * customizing game behavior. The script manager routes events to
 * the appropriate script handlers.
 *
 * @see ScriptMgr for the manager class
 * @see ScriptedInstance for instance script base
 */



#include "ScriptMgr.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "Cell.h"
#include "CellImpl.h"
#include "Mail.h"
#include "WaypointManager.h"
#include "WaypointMovementGenerator.h"
#include "Policies/Singleton.h"
#include "ProgressBar.h"
#include "World.h"
#include "SQLStorages.h"
#include "BattleGround/BattleGround.h"
#include "OutdoorPvP/OutdoorPvP.h"
#ifdef CLASSIC
#include "LFGMgr.h"
#endif /* CLASSIC */
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */
#ifdef ENABLE_SD3
#include "system/ScriptDevMgr.h"
#endif /* ENABLE_SD3 */

/// Helper function to get Object source or target for Script-Command
/// returns false iff an error happened
bool ScriptAction::GetScriptCommandObject(const ObjectGuid guid, bool includeItem, Object*& resultObject)
{
    resultObject = NULL;

    if (!guid)
    {
        return true;
    }

    switch (guid.GetHigh())
    {
        case HIGHGUID_UNIT:
#if defined(WOTLK) || defined(CATA) || defined(MISTS)
        case HIGHGUID_VEHICLE:
#endif
            resultObject = m_map->GetCreature(guid);
            break;
        case HIGHGUID_PET:
            resultObject = m_map->GetPet(guid);
            break;
        case HIGHGUID_PLAYER:
            resultObject = m_map->GetPlayer(guid);
            break;
        case HIGHGUID_GAMEOBJECT:
            resultObject = m_map->GetGameObject(guid);
            break;
        case HIGHGUID_CORPSE:
            resultObject = sObjectAccessor.FindCorpse(guid);
            break;
        case HIGHGUID_ITEM: // case HIGHGUID_CONTAINER: ==HIGHGUID_ITEM
        {
            if (includeItem)
            {
                if (Player* player = m_map->GetPlayer(m_ownerGuid))
                {
                    resultObject = player->GetItemByGuid(guid);
                }
                break;
            }
            // else no break, but display error message
        }
        default:
            sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u with unsupported guid %s, skipping", m_type, m_script->id, m_script->command, guid.GetString().c_str());
            return false;
    }

    if (resultObject && !resultObject->IsInWorld())
    {
        resultObject = NULL;
    }

    return true;
}

/// Select source and target for a script command
/// Returns false iff an error happened
bool ScriptAction::GetScriptProcessTargets(WorldObject* pOrigSource, WorldObject* pOrigTarget, WorldObject*& pFinalSource, WorldObject*& pFinalTarget)
{
    WorldObject* pBuddy = NULL;

    if (m_script->buddyEntry)
    {
        if (m_script->data_flags & SCRIPT_FLAG_BUDDY_BY_GUID)
        {
            if (m_script->IsCreatureBuddy())
            {
                CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(m_script->buddyEntry);

                if (cinfo != NULL)
                {
                    pBuddy = m_map->GetCreature(cinfo->GetObjectGuid(m_script->searchRadiusOrGuid));

                    if (pBuddy && !((Creature*)pBuddy)->IsAlive())
                    {
                        sLog.outError(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u by guid %u but buddy is dead, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                        return false;
                    }
                }
                else
                {
                    sLog.outError(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has no buddy %u by guid %u, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                    return false;
                }
            }
            else
            {
                // GameObjectInfo const* ginfo = ObjectMgr::GetGameObjectInfo(m_script->buddyEntry);
                pBuddy = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, m_script->buddyEntry, m_script->searchRadiusOrGuid));
            }
            // TODO Maybe load related grid if not already done? How to handle multi-map case?
            if (!pBuddy)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u by guid %u not loaded in map %u (data-flags %u), skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid, m_map->GetId(), m_script->data_flags);
                return false;
            }
        }
        else                                                // Buddy by entry
        {
            if (!pOrigSource && !pOrigTarget)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without buddy %u, but no source for search available, skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                return false;
            }

            // Prefer non-players as searcher
            WorldObject* pSearcher = pOrigSource ? pOrigSource : pOrigTarget;
            if (pOrigSource && pOrigTarget &&
                pOrigSource->GetTypeId() == TYPEID_PLAYER && pOrigTarget->GetTypeId() != TYPEID_PLAYER)
            {
                pSearcher = pOrigTarget;
            }

            if (m_script->IsCreatureBuddy())
            {
                Creature* pCreatureBuddy = NULL;

                if (m_script->data_flags & SCRIPT_FLAG_BUDDY_IS_DESPAWNED)
                {
                    MaNGOS::AllCreaturesOfEntryInRangeCheck u_check(pSearcher, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                    MaNGOS::CreatureLastSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(pCreatureBuddy, u_check);
                    Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                }
                else
                {
                    MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*pSearcher, m_script->buddyEntry, true, false, m_script->searchRadiusOrGuid, true);
                    MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreatureBuddy, u_check);

                    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_IS_PET)
                    {
                        Cell::VisitWorldObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                    }
                    else                                        // Normal Creature
                    {
                        Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                    }
                }

                pBuddy = pCreatureBuddy;

                // TODO: Remove this extra check output after a while - it might have false effects
                if (!pBuddy && pSearcher->GetEntry() == m_script->buddyEntry)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: WARNING: Process table `db_scripts [type = %d]` id %u, command %u has no OTHER buddy %u found - maybe you need to update the script?", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                    pBuddy = pSearcher;
                }
            }
            else
            {
                GameObject* pGOBuddy = NULL;

                MaNGOS::NearestGameObjectEntryInObjectRangeCheck u_check(*pSearcher, m_script->buddyEntry, m_script->searchRadiusOrGuid);
                MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> searcher(pGOBuddy, u_check);

                Cell::VisitGridObjects(pSearcher, searcher, m_script->searchRadiusOrGuid);
                pBuddy = pGOBuddy;
            }

            if (!pBuddy)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u has buddy %u not found in range %u of searcher %s (data-flags %u), skipping.", m_type, m_script->id, m_script->command, m_script->buddyEntry, m_script->searchRadiusOrGuid, pSearcher->GetGuidStr().c_str(), m_script->data_flags);
                return false;
            }
        }
    }

    if (m_script->data_flags & SCRIPT_FLAG_BUDDY_AS_TARGET)
    {
        pFinalSource = pOrigSource;
        pFinalTarget = pBuddy;
    }
    else
    {
        pFinalSource = pBuddy ? pBuddy : pOrigSource;
        pFinalTarget = pOrigTarget;
    }

    if (m_script->data_flags & SCRIPT_FLAG_REVERSE_DIRECTION)
    {
        std::swap(pFinalSource, pFinalTarget);
    }

    if (m_script->data_flags & SCRIPT_FLAG_SOURCE_TARGETS_SELF)
    {
        pFinalTarget = pFinalSource;
    }

    return true;
}

/// Helper to log error information
bool ScriptAction::LogIfNotCreature(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_UNIT)
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-creature, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

/**
 * @brief Logs an error when the provided world object is not a unit.
 *
 * @param pWorldObject The world object to validate.
 * @return true if validation failed; otherwise false.
 */
bool ScriptAction::LogIfNotUnit(WorldObject* pWorldObject)
{
    if (!pWorldObject || !pWorldObject->isType(TYPEMASK_UNIT))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-unit, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

/**
 * @brief Logs an error when the provided world object is not a game object.
 *
 * @param pWorldObject The world object to validate.
 * @return true if validation failed; otherwise false.
 */
bool ScriptAction::LogIfNotGameObject(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_GAMEOBJECT)
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-gameobject, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

/**
 * @brief Logs an error when the provided world object is not a player.
 *
 * @param pWorldObject The world object to validate.
 * @return true if validation failed; otherwise false.
 */
bool ScriptAction::LogIfNotPlayer(WorldObject* pWorldObject)
{
    if (!pWorldObject || pWorldObject->GetTypeId() != TYPEID_PLAYER)
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non-player, skipping.", m_type, m_script->id, m_script->command);
        return true;
    }
    return false;
}

/// Helper to get a player if possible (target preferred)
Player* ScriptAction::GetPlayerTargetOrSourceAndLog(WorldObject* pSource, WorldObject* pTarget)
{
    if ((!pTarget || pTarget->GetTypeId() != TYPEID_PLAYER) && (!pSource || pSource->GetTypeId() != TYPEID_PLAYER))
    {
        sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for non player, skipping.", m_type, m_script->id, m_script->command);
        return NULL;
    }

    return pTarget && pTarget->GetTypeId() == TYPEID_PLAYER ? (Player*)pTarget : (Player*)pSource;
}

/// Handle one Script Step
// Return true if and only if further parts of this script shall be skipped
bool ScriptAction::HandleScriptStep()
{
    WorldObject* pSource;
    WorldObject* pTarget;
    Object* pSourceOrItem;                                  // Stores a provided pSource (if exists as WorldObject) or source-item

    {
        // Add scope for source & target variables so that they are not used below
        Object* source = NULL;
        Object* target = NULL;
        if (!GetScriptCommandObject(m_sourceGuid, true, source))
        {
            return false;
        }
        if (!GetScriptCommandObject(m_targetGuid, false, target))
        {
            return false;
        }

        // Give some debug log output for easier use
        DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u for source %s (%sin world), target %s (%sin world)", m_type, m_script->id, m_script->command, m_sourceGuid.GetString().c_str(), source ? "" : "not ", m_targetGuid.GetString().c_str(), target ? "" : "not ");

        // Get expected source and target (if defined with buddy)
        pSource = source && source->isType(TYPEMASK_WORLDOBJECT) ? (WorldObject*)source : NULL;
        pTarget = target && target->isType(TYPEMASK_WORLDOBJECT) ? (WorldObject*)target : NULL;
        if (!GetScriptProcessTargets(pSource, pTarget, pSource, pTarget))
        {
            return false;
        }

        pSourceOrItem = pSource ? pSource : (source && source->isType(TYPEMASK_ITEM) ? source : NULL);
    }

    switch (m_script->command)
    {
        case SCRIPT_COMMAND_TALK:                           // 0
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u found no worldobject as source, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            Unit* unitTarget = pTarget && pTarget->isType(TYPEMASK_UNIT) ? static_cast<Unit*>(pTarget) : NULL;
            int32 textId = m_script->textId[0];

            // May have text for random
            if (m_script->textId[1])
            {
                int i = 2;
                for (; i < MAX_TEXT_ID; ++i)
                {
                    if (!m_script->textId[i])
                    {
                        break;
                    }
                }

                // Use one random
                textId = m_script->textId[urand(0, i - 1)];
            }

            if (!DoDisplayText(pSource, textId, unitTarget))
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, could not display text %i properly", m_type, m_script->id, textId);
            }
            break;
        }
        case SCRIPT_COMMAND_EMOTE:                          // 1
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            std::vector<uint32> emotes;
            emotes.push_back(m_script->emote.emoteId);
            for (int i = 0; i < MAX_TEXT_ID; ++i)
            {
                if (!m_script->textId[i])
                {
                    break;
                }
                emotes.push_back(uint32(m_script->textId[i]));
            }

            ((Unit*)pSource)->HandleEmote(emotes[urand(0, emotes.size() - 1)]);
            break;
        }
        case SCRIPT_COMMAND_FIELD_SET:                      // 2
            if (!pSourceOrItem)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for NULL object.", m_type, m_script->id, m_script->command);
                break;
            }
            if (m_script->setField.fieldId <= OBJECT_FIELD_ENTRY || m_script->setField.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u call for wrong field %u (max count: %u) in %s.",
                    m_type, m_script->id, m_script->command, m_script->setField.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->SetUInt32Value(m_script->setField.fieldId, m_script->setField.fieldValue);
            break;
        case SCRIPT_COMMAND_MOVE_TO:                        // 3
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            // Just turn around
            if ((m_script->x == 0.0f && m_script->y == 0.0f && m_script->z == 0.0f) ||
                // Check point-to-point distance, hence revert effect of bounding radius
                ((Unit*)pSource)->IsWithinDist3d(m_script->x, m_script->y, m_script->z, 0.01f - ((Unit*)pSource)->GetObjectBoundingRadius()))
            {
                ((Unit*)pSource)->SetFacingTo(m_script->o);
                break;
            }

            // For command additional teleport the unit
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Unit*)pSource)->NearTeleportTo(m_script->x, m_script->y, m_script->z, m_script->o != 0.0f ? m_script->o : ((Unit*)pSource)->GetOrientation());
                break;
            }

            // Normal Movement
            if (m_script->moveTo.travelSpeed)
            {
                ((Unit*)pSource)->MonsterMoveWithSpeed(m_script->x, m_script->y, m_script->z, m_script->moveTo.travelSpeed * 0.01f);
            }
            else
            {
                ((Unit*)pSource)->GetMotionMaster()->Clear();
                ((Unit*)pSource)->GetMotionMaster()->MovePoint(0, m_script->x, m_script->y, m_script->z);
            }
            break;
        }
        case SCRIPT_COMMAND_FLAG_SET:                       // 4
            if (!pSourceOrItem)
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_SET (script id %u) call for NULL object.", m_script->id);
                break;
            }
            if (m_script->setFlag.fieldId <= OBJECT_FIELD_ENTRY || m_script->setFlag.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_SET (script id %u) call for wrong field %u (max count: %u) in %s.",
                    m_script->id, m_script->setFlag.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->SetFlag(m_script->setFlag.fieldId, m_script->setFlag.fieldValue);
            break;
        case SCRIPT_COMMAND_FLAG_REMOVE:                    // 5
            if (!pSourceOrItem)
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for NULL object.", m_script->id);
                break;
            }
            if (m_script->removeFlag.fieldId <= OBJECT_FIELD_ENTRY || m_script->removeFlag.fieldId >= pSourceOrItem->GetValuesCount())
            {
                sLog.outErrorDb("SCRIPT_COMMAND_FLAG_REMOVE (script id %u) call for wrong field %u (max count: %u) in %s.",
                    m_script->id, m_script->removeFlag.fieldId, pSourceOrItem->GetValuesCount(), pSourceOrItem->GetGuidStr().c_str());
                break;
            }
            pSourceOrItem->RemoveFlag(m_script->removeFlag.fieldId, m_script->removeFlag.fieldValue);
            break;
        case SCRIPT_COMMAND_TELEPORT_TO:                    // 6
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            pPlayer->TeleportTo(m_script->teleportTo.mapId, m_script->x, m_script->y, m_script->z, m_script->o);
            break;
        }
        case SCRIPT_COMMAND_QUEST_EXPLORED:                 // 7
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            WorldObject* pWorldObject = NULL;
            if (pSource && pSource->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
            {
                pWorldObject = pSource;
            }
            else if (pTarget && pTarget->isType(TYPEMASK_CREATURE_OR_GAMEOBJECT))
            {
                pWorldObject = pTarget;
            }

            // if we have a distance, we must have a worldobject
            if (m_script->questExplored.distance != 0 && !pWorldObject)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without source worldobject, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            bool failQuest = false;
            // Creature must be alive for giving credit
            if (pWorldObject && pWorldObject->GetTypeId() == TYPEID_UNIT && !((Creature*)pWorldObject)->IsAlive())
            {
                failQuest = true;
            }
            else if (m_script->questExplored.distance != 0 && !pWorldObject->IsWithinDistInMap(pPlayer, float(m_script->questExplored.distance)))
            {
                failQuest = true;
            }

            // quest id and flags checked at script loading
            if (!failQuest)
            {
                pPlayer->AreaExploredOrEventHappens(m_script->questExplored.questId);
            }
            else
            {
                pPlayer->FailQuest(m_script->questExplored.questId);
            }

            break;
        }
        case SCRIPT_COMMAND_KILL_CREDIT:                    // 8
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            uint32 creatureEntry = m_script->killCredit.creatureEntry;
            WorldObject* pRewardSource = pSource && pSource->GetTypeId() == TYPEID_UNIT ? pSource : (pTarget && pTarget->GetTypeId() == TYPEID_UNIT ? pTarget : NULL);

            // dynamic effect, take entry of reward Source
            if (!creatureEntry)
            {
                if (pRewardSource)
                {
                    creatureEntry =  pRewardSource->GetEntry();
                }
                else
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called for dynamic killcredit without creature partner, skipping.", m_type, m_script->id, m_script->command);
                    break;
                }
            }

            if (m_script->killCredit.isGroupCredit)
            {
                WorldObject* pSearcher = pRewardSource ? pRewardSource : (pSource ? pSource : pTarget);
                if (pSearcher != pRewardSource)
                {
                    sLog.outDebug(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, SCRIPT_COMMAND_KILL_CREDIT called for groupCredit without creature as searcher, script might need adjustment.", m_type, m_script->id);
                }
                pPlayer->RewardPlayerAndGroupAtEvent(creatureEntry, pSearcher);
            }
            else
            {
                pPlayer->KilledMonsterCredit(creatureEntry, pRewardSource ? pRewardSource->GetObjectGuid() : ObjectGuid());
            }

            break;
        }
        case SCRIPT_COMMAND_RESPAWN_GO:                     // 9
        {
            GameObject* pGo;
            if (m_script->respawnGo.goGuid)
            {
                GameObjectData const* goData = sObjectMgr.GetGOData(m_script->respawnGo.goGuid);
                if (!goData)
                {
                    break;                                   // checked at load
                }

                // TODO - This was a change, was before current map of source
                pGo = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->respawnGo.goGuid));
            }
            else
            {
                if (LogIfNotGameObject(pSource))
                {
                    break;
                }

                pGo = (GameObject*)pSource;
            }

            if (!pGo)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, m_script->respawnGo.goGuid, m_script->buddyEntry);
                break;
            }

            if (pGo->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE ||
                pGo->GetGoType() == GAMEOBJECT_TYPE_DOOR)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u can not be used with gameobject of type %u (guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, uint32(pGo->GetGoType()), m_script->respawnGo.goGuid, m_script->buddyEntry);
                break;
            }

            if (pGo->isSpawned())
            {
                break;                                       // gameobject already spawned
            }

            uint32 time_to_despawn = m_script->respawnGo.despawnDelay < 5 ? 5 : m_script->respawnGo.despawnDelay;

            pGo->SetLootState(GO_READY);
            pGo->SetRespawnTime(time_to_despawn);           // despawn object in ? seconds
            pGo->Refresh();
            break;
        }
        case SCRIPT_COMMAND_TEMP_SUMMON_CREATURE:           // 10
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u found no worldobject as source, skipping.", m_type, m_script->id, m_script->command);
                break;
            }

            float x = m_script->x;
            float y = m_script->y;
            float z = m_script->z;
            float o = m_script->o;

            Creature* pCreature = pSource->SummonCreature(m_script->summonCreature.creatureEntry, x, y, z, o, m_script->summonCreature.despawnDelay ? TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN : TEMPSPAWN_DEAD_DESPAWN, m_script->summonCreature.despawnDelay, (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) ? true : false, m_script->textId[0] != 0);
            if (!pCreature)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for creature (entry: %u).", m_type, m_script->id, m_script->command, m_script->summonCreature.creatureEntry);
                break;
            }

            break;
        }
        case SCRIPT_COMMAND_OPEN_DOOR:                      // 11
        case SCRIPT_COMMAND_CLOSE_DOOR:                     // 12
        {
            GameObject* pDoor;
            uint32 time_to_reset = m_script->changeDoor.resetDelay < 15 ? 15 : m_script->changeDoor.resetDelay;

            if (m_script->changeDoor.goGuid)
            {
                GameObjectData const* goData = sObjectMgr.GetGOData(m_script->changeDoor.goGuid);
                if (!goData)                                // checked at load
                {
                    break;
                }

                // TODO - Was a change, before random map
                pDoor = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->changeDoor.goGuid));
            }
            else
            {
                if (LogIfNotGameObject(pSource))
                {
                    break;
                }

                pDoor = (GameObject*)pSource;
            }

            if (!pDoor)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(guid: %u, buddyEntry: %u).", m_type, m_script->id, m_script->command, m_script->changeDoor.goGuid, m_script->buddyEntry);
                break;
            }

            if (pDoor->GetGoType() != GAMEOBJECT_TYPE_DOOR)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for non-door(GoType: %u).", m_type, m_script->id, m_script->command, pDoor->GetGoType());
                break;
            }

            if ((m_script->command == SCRIPT_COMMAND_OPEN_DOOR && pDoor->GetGoState() != GO_STATE_READY) ||
                (m_script->command == SCRIPT_COMMAND_CLOSE_DOOR && pDoor->GetGoState() == GO_STATE_READY))
            {
                break;                                       // to be opened door already open, or to be closed door already closed
            }

            pDoor->UseDoorOrButton(time_to_reset);

            if (pTarget && pTarget->isType(TYPEMASK_GAMEOBJECT) && ((GameObject*)pTarget)->GetGoType() == GAMEOBJECT_TYPE_BUTTON)
            {
                ((GameObject*)pTarget)->UseDoorOrButton(time_to_reset);
            }

            break;
        }
        case SCRIPT_COMMAND_ACTIVATE_OBJECT:                // 13
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }
            if (LogIfNotGameObject(pTarget))
            {
                break;
            }

            ((GameObject*)pTarget)->Use((Unit*)pSource);
            break;
        }
        case SCRIPT_COMMAND_REMOVE_AURA:                    // 14
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }

            ((Unit*)pSource)->RemoveAurasDueToSpell(m_script->removeAura.spellId);
            break;
        }
        case SCRIPT_COMMAND_CAST_SPELL:                     // 15
        {
            if (LogIfNotUnit(pTarget))                      // TODO - Change when support for casting without victim will be supported
            {
                break;
            }

            // Select Spell
            uint32 spell = m_script->castSpell.spellId;
            uint32 filledCount = 0;
            while (filledCount < MAX_TEXT_ID && m_script->textId[filledCount])  // Count which dataint fields are filled
            {
                ++filledCount;
            }

            if (filledCount > 0)
            {
                if (uint32 randomField = urand(0, filledCount))               // Random selection resulted in one of the dataint fields
                {
                    spell = m_script->textId[randomField - 1];
                }
            }
            // TODO: when GO cast implemented, code below must be updated accordingly to also allow GO spell cast
            if (pSource && pSource->GetTypeId() == TYPEID_GAMEOBJECT)
            {
                ((Unit*)pTarget)->CastSpell(((Unit*)pTarget), spell, true, NULL, NULL, pSource->GetObjectGuid());
                {
                    break;
                }
            }

            if (LogIfNotUnit(pSource))
            {
                break;
            }
            ((Unit*)pSource)->CastSpell(((Unit*)pTarget), spell, (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) != 0);

            break;
        }
        case SCRIPT_COMMAND_PLAY_SOUND:                     // 16
        {
            if (!pSource)
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u could not find proper source", m_type, m_script->id, m_script->command);
                break;
            }

            // bitmask: 0/1=target-player, 0/2=with distance dependent, 0/4=map wide, 0/8=zone wide
            Player* pSoundTarget = NULL;
            if (m_script->playSound.flags & 1)
            {
                pSoundTarget = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
                if (!pSoundTarget)
                {
                    break;
                }
            }

            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                pSource->PlayMusic(m_script->playSound.soundId, pSoundTarget);
            }
            else
            {
                if (m_script->playSound.flags & 2)
                {
                    pSource->PlayDistanceSound(m_script->playSound.soundId, pSoundTarget);
                }
                else if (m_script->playSound.flags & (4 | 8))
                {
                    m_map->PlayDirectSoundToMap(m_script->playSound.soundId, (m_script->playSound.flags & 8) ? pSource->GetZoneId() : 0);
                }
                else
                {
                    pSource->PlayDirectSound(m_script->playSound.soundId, pSoundTarget);
                }
            }
            break;
        }
        case SCRIPT_COMMAND_CREATE_ITEM:                    // 17
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            if (Item* pItem = pPlayer->StoreNewItemInInventorySlot(m_script->createItem.itemEntry, m_script->createItem.amount))
            {
                pPlayer->SendNewItem(pItem, m_script->createItem.amount, true, false);
            }

            break;
        }
        case SCRIPT_COMMAND_DESPAWN_SELF:                   // 18
        {
            // TODO - Remove this check after a while
            if (pTarget && pTarget->GetTypeId() != TYPEID_UNIT && pSource && pSource->GetTypeId() == TYPEID_UNIT)
            {
                sLog.outErrorDb("DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u target must be creature, but (only) source is, use data_flags to fix", m_type, m_script->id, m_script->command);
                pTarget = pSource;
            }

            if (LogIfNotCreature(pTarget))
            {
                break;
            }

            ((Creature*)pTarget)->ForcedDespawn(m_script->despawn.despawnDelay);

            break;
        }
        case SCRIPT_COMMAND_PLAY_MOVIE:                     // 19
        {
#if defined(WOTLK) || defined (CATA) || defined (MISTS)
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            pPlayer->SendMovieStart(m_script->playMovie.movieId);
#endif
            break;                                      // must be skipped at loading
        }
        case SCRIPT_COMMAND_MOVEMENT:                       // 20
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            // Consider add additional checks for cases where creature should not change movementType
            // (pet? in combat? already using same MMgen as script try to apply?)

            switch (m_script->movement.movementType)
            {
                case IDLE_MOTION_TYPE:
                    ((Creature*)pSource)->GetMotionMaster()->MoveIdle();
                    break;
                case RANDOM_MOTION_TYPE:
                    if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                    {
                        ((Creature*)pSource)->GetMotionMaster()->MoveRandomAroundPoint(pSource->GetPositionX(), pSource->GetPositionY(), pSource->GetPositionZ(), float(m_script->movement.wanderDistance));
                    }
                    else
                    {
                        float respX, respY, respZ, respO, wander_distance;
                        ((Creature*)pSource)->GetRespawnCoord(respX, respY, respZ, &respO, &wander_distance);
                        wander_distance = m_script->movement.wanderDistance ? m_script->movement.wanderDistance : wander_distance;
                        ((Creature*)pSource)->GetMotionMaster()->MoveRandomAroundPoint(respX, respY, respZ, wander_distance);
                    }
                    break;
                case WAYPOINT_MOTION_TYPE:
                    ((Creature*)pSource)->GetMotionMaster()->MoveWaypoint();
                    break;
            }

            break;
        }
        case SCRIPT_COMMAND_SET_ACTIVEOBJECT:               // 21
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Creature*)pSource)->SetActiveObjectState(m_script->activeObject.activate);
            break;
        }
        case SCRIPT_COMMAND_SET_FACTION:                    // 22
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            if (m_script->faction.factionId)
            {
                ((Creature*)pSource)->SetFactionTemporary(m_script->faction.factionId, m_script->faction.flags);
            }
            else
            {
                ((Creature*)pSource)->ClearTemporaryFaction();
            }

            break;
        }
        case SCRIPT_COMMAND_MORPH_TO_ENTRY_OR_MODEL:        // 23
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            if (!m_script->morph.creatureOrModelEntry)
            {
                ((Creature*)pSource)->DeMorph();
            }
            else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Creature*)pSource)->SetDisplayId(m_script->morph.creatureOrModelEntry);
            }
            else
            {
                CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_script->morph.creatureOrModelEntry);
                uint32 display_id = Creature::ChooseDisplayId(ci);

                ((Creature*)pSource)->SetDisplayId(display_id);
            }

            break;
        }
        case SCRIPT_COMMAND_MOUNT_TO_ENTRY_OR_MODEL:        // 24
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            if (!m_script->mount.creatureOrModelEntry)
            {
                ((Creature*)pSource)->Unmount();
            }
            else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                ((Creature*)pSource)->Mount(m_script->mount.creatureOrModelEntry);
            }
            else
            {
                CreatureInfo const* ci = ObjectMgr::GetCreatureTemplate(m_script->mount.creatureOrModelEntry);
                uint32 display_id = Creature::ChooseDisplayId(ci);

                ((Creature*)pSource)->Mount(display_id);
            }

            break;
        }
        case SCRIPT_COMMAND_SET_RUN:                        // 25
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Creature*)pSource)->SetWalk(!m_script->run.run, true);

            break;
        }
        case SCRIPT_COMMAND_ATTACK_START:                   // 26
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }
            if (LogIfNotUnit(pTarget))
            {
                break;
            }

            Creature* pAttacker = static_cast<Creature*>(pSource);
            Unit* unitTarget = static_cast<Unit*>(pTarget);

            if (pAttacker->IsFriendlyTo(unitTarget))
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u attacker is friendly to target, can not attack (Attacker: %s, Target: %s)", m_type, m_script->id, m_script->command, pAttacker->GetGuidStr().c_str(), unitTarget->GetGuidStr().c_str());
                break;
            }

            pAttacker->AI()->AttackStart(unitTarget);

            break;
        }
        case SCRIPT_COMMAND_GO_LOCK_STATE:                  // 27
        {
            if (LogIfNotGameObject(pSource))
            {
                break;
            }

            GameObject* pGo = static_cast<GameObject*>(pSource);

            /** flag lockState
             * go_lock          0x01
             * go_unlock        0x02
             * go_nonInteract   0x04
             * go_Interact      0x08
             */

            // Lock or Unlock
            if (m_script->goLockState.lockState & 0x01)
            {
                pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
            }
            else if (m_script->goLockState.lockState & 0x02)
            {
                pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
            }
            // Set Non Interactable or Set Interactable
            if (m_script->goLockState.lockState & 0x04)
            {
                pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            }
            else if (m_script->goLockState.lockState & 0x08)
            {
                pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
            }

            break;
        }
        case SCRIPT_COMMAND_STAND_STATE:                    // 28
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            // Must be safe cast to Unit* here
            ((Unit*)pSource)->SetStandState(m_script->standState.stand_state);
            break;
        }
        case SCRIPT_COMMAND_MODIFY_NPC_FLAGS:               // 29
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            // Add Flags
            if (m_script->npcFlag.change_flag & 0x01)
            {
                pSource->SetFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
            }
            // Remove Flags
            else if (m_script->npcFlag.change_flag & 0x02)
            {
                pSource->RemoveFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
            }
            // Toggle Flags
            else
            {
                if (pSource->HasFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag))
                {
                    pSource->RemoveFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
                }
                else
                {
                    pSource->SetFlag(UNIT_NPC_FLAGS, m_script->npcFlag.flag);
                }
            }

            break;
        }
        case SCRIPT_COMMAND_SEND_TAXI_PATH:                 // 30
        {
            // only Player
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            pPlayer->ActivateTaxiPathTo(m_script->sendTaxiPath.taxiPathId);
            break;
        }
        case SCRIPT_COMMAND_TERMINATE_SCRIPT:               // 31
        {
            bool result = false;
            if (m_script->terminateScript.npcEntry)
            {
                WorldObject* pSearcher = pSource ? pSource : pTarget;
                if (!pSearcher)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u called without source or target for npc search %u in range %u, skipping.",
                        m_type, m_script->id, m_script->command, m_script->terminateScript.npcEntry, m_script->terminateScript.searchDist);
                    break;
                }

                if (pSearcher->GetTypeId() == TYPEID_PLAYER && pTarget && pTarget->GetTypeId() != TYPEID_PLAYER)
                {
                    pSearcher = pTarget;
                }

                Creature* pCreatureBuddy = NULL;
                MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck u_check(*pSearcher, m_script->terminateScript.npcEntry, true, false, m_script->terminateScript.searchDist, true);
                MaNGOS::CreatureLastSearcher<MaNGOS::NearestCreatureEntryWithLiveStateInObjectRangeCheck> searcher(pCreatureBuddy, u_check);
                Cell::VisitGridObjects(pSearcher, searcher, m_script->terminateScript.searchDist);

                if (!(m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL) && !pCreatureBuddy)
                {
                    DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, terminate further steps of this script! (as searched other npc %u was not found alive)", m_type, m_script->id, m_script->terminateScript.npcEntry);
                    result = true;
                }
                else if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL && pCreatureBuddy)
                {
                    DEBUG_FILTER_LOG(LOG_FILTER_DB_SCRIPTS, "DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, terminate further steps of this script! (as searched other npc %u was found alive)", m_type, m_script->id, m_script->terminateScript.npcEntry);
                    result = true;
                }
            }
            else
            {
                result = true;
            }

            if (result)                                    // Terminate further steps of this script
            {
                if (m_script->textId[0] && !LogIfNotCreature(pSource))
                {
                    Creature* cSource = static_cast<Creature*>(pSource);
                    if (cSource->GetMotionMaster()->GetCurrentMovementGeneratorType() == WAYPOINT_MOTION_TYPE)
                    {
                        (static_cast<WaypointMovementGenerator<Creature>* >(cSource->GetMotionMaster()->top()))->AddToWaypointPauseTime(m_script->textId[0]);
                    }
                }

                return true;
            }

            break;
        }
        case SCRIPT_COMMAND_PAUSE_WAYPOINTS:                // 32
        {
            if (LogIfNotCreature(pSource))
            {
                return false;
            }
            if (m_script->pauseWaypoint.doPause)
            {
                ((Creature*)pSource)->addUnitState(UNIT_STAT_WAYPOINT_PAUSED);
            }
            else
            {
                ((Creature*)pSource)->clearUnitState(UNIT_STAT_WAYPOINT_PAUSED);
            }
            break;
        }
        case SCRIPT_COMMAND_JOIN_LFG:                       // 33
        {
            //Not supported
            break;
        }
        case SCRIPT_COMMAND_TERMINATE_COND:                 // 34
        {
            Player* player = NULL;
            WorldObject* second = pSource;
            // First case: target is player
            if (pTarget && pTarget->GetTypeId() == TYPEID_PLAYER)
            {
                player = static_cast<Player*>(pTarget);
            }
            // Second case: source is player
            else if (pSource && pSource->GetTypeId() == TYPEID_PLAYER)
            {
                player = static_cast<Player*>(pSource);
                second = pTarget;
            }

            bool terminateResult;
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                terminateResult = !sObjectMgr.IsPlayerMeetToCondition(m_script->terminateCond.conditionId, player, m_map, second, CONDITION_FROM_DBSCRIPTS);
            }
            else
            {
                terminateResult = sObjectMgr.IsPlayerMeetToCondition(m_script->terminateCond.conditionId, player, m_map, second, CONDITION_FROM_DBSCRIPTS);
            }

            if (terminateResult && m_script->terminateCond.failQuest && player)
            {
                if (Group* group = player->GetGroup())
                {
                    for (GroupReference* groupRef = group->GetFirstMember(); groupRef != NULL; groupRef = groupRef->next())
                    {
                        Player* member = groupRef->getSource();
                        if (member->GetQuestStatus(m_script->terminateCond.failQuest) == QUEST_STATUS_INCOMPLETE)
                        {
                            member->FailQuest(m_script->terminateCond.failQuest);
                        }
                    }
                }
                else
                {
                    if (player->GetQuestStatus(m_script->terminateCond.failQuest) == QUEST_STATUS_INCOMPLETE)
                    {
                        player->FailQuest(m_script->terminateCond.failQuest);
                    }
                }
            }
            return terminateResult;
        }
        case SCRIPT_COMMAND_SEND_AI_EVENT_AROUND:           // 35
        {
            if (LogIfNotCreature(pSource))
            {
                return false;
            }
            if (LogIfNotUnit(pTarget))
            {
                break;
            }

#if defined(CLASSIC) || defined(TBC) || defined(WOTLK)
            ((Creature*)pSource)->AI()->SendAIEventAround(AIEventType(m_script->sendAIEvent.eventType), (Unit*)pTarget, 0, float(m_script->sendAIEvent.radius));
#else
            // if radius is provided send AI event around
            if (m_script->sendAIEvent.radius)
            {
                ((Creature*)pSource)->AI()->SendAIEventAround(AIEventType(m_script->sendAIEvent.eventType), (Unit*)pTarget, 0, float(m_script->sendAIEvent.radius));
            }
            // else if no radius and target is creature send AI event to target
            else if (pTarget->GetTypeId() == TYPEID_UNIT)
            {
                ((Creature*)pSource)->AI()->SendAIEvent(AIEventType(m_script->sendAIEvent.eventType), NULL, (Creature*)pTarget);
            }
#endif
            break;
        }
        case SCRIPT_COMMAND_TURN_TO:                        // 36
        {
            if (LogIfNotUnit(pSource))
            {
                break;
            }
            //note for self: this command has different impl. and usage in other core(s)
            ((Unit*)pSource)->SetFacingTo(pSource->GetAngle(pTarget));
            break;
        }
        case SCRIPT_COMMAND_MOVE_DYNAMIC:                   // 37
        {
            if (LogIfNotCreature(pSource))
            {
                return false;
            }
            if (LogIfNotUnit(pTarget))
            {
                return false;
            }

            float x, y, z;
            if (m_script->moveDynamic.maxDist == 0)         // Move to pTarget
            {
                if (pTarget == pSource)
                {
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, _MOVE_DYNAMIC called with maxDist == 0, but resultingSource == resultingTarget (== %s)", m_type, m_script->id, pSource->GetGuidStr().c_str());
                    break;
                }
                pTarget->GetContactPoint(pSource, x, y, z);
            }
            else                                            // Calculate position
            {
                float orientation;
                if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
                {
                    orientation = pSource->GetOrientation() + m_script->o + 2 * M_PI_F;
                }
                else
                {
                    orientation = m_script->o;
                }

                pSource->GetRandomPoint(pTarget->GetPositionX(), pTarget->GetPositionY(), pTarget->GetPositionZ(), m_script->moveDynamic.maxDist, x, y, z,
                    m_script->moveDynamic.minDist, (orientation == 0.0f ? NULL : &orientation));
                z = std::max(z, pTarget->GetPositionZ());
                pSource->UpdateAllowedPositionZ(x, y, z);
            }
            ((Creature*)pSource)->GetMotionMaster()->MovePoint(1, x, y, z);
            break;
        }
        case SCRIPT_COMMAND_SEND_MAIL:                      // 38
        {
            if (LogIfNotPlayer(pTarget))
            {
                return false;
            }
            if (!m_script->sendMail.altSender && LogIfNotCreature(pSource))
            {
                return false;
            }

            MailSender sender;
            if (m_script->sendMail.altSender)
            {
                sender = MailSender(MAIL_CREATURE, m_script->sendMail.altSender);
            }
            else
            {
                sender = MailSender(pSource);
            }
            uint32 deliverDelay = m_script->textId[0] > 0 ? (uint32)m_script->textId[0] : 0;

            MailDraft(m_script->sendMail.mailTemplateId).SendMailTo(static_cast<Player*>(pTarget), sender, MAIL_CHECK_MASK_HAS_BODY, deliverDelay);
            break;
        }
        case SCRIPT_COMMAND_CHANGE_ENTRY:                   // 39
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            ((Creature*)pSource)->UpdateEntry(m_script->changeEntry.creatureEntry);
            break;
        }
        case SCRIPT_COMMAND_DESPAWN_GO:                     // 40
        {

            uint32 goEntry;
            GameObject* pGo;
            if (!m_script->despawnGo.goGuid)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has no gameobject defined in SCRIPT_COMMAND_DESPAWN_GO for script id %u", m_type, m_script->id);
                break;
            }

            GameObjectData const* goData = sObjectMgr.GetGOData(m_script->despawnGo.goGuid);
            if (!goData)
            {
                sLog.outErrorDb("Table `db_scripts [type = %d]` has invalid gameobject (GUID: %u) in SCRIPT_COMMAND_RESPAWN_GO for script id %u", m_type, m_script->despawnGo.goGuid, m_script->id);
                break;
            }

            pGo = m_map->GetGameObject(ObjectGuid(HIGHGUID_GAMEOBJECT, goData->id, m_script->despawnGo.goGuid));

            pGo->SetRespawnTime(m_script->despawnGo.respawnTime);
            pGo->SetLootState(GO_JUST_DEACTIVATED);

            break;
        }
        case SCRIPT_COMMAND_RESPAWN:                        // 41
        {
            if (LogIfNotCreature(pTarget))
            {
                break;
            }
            ((Creature*)pTarget)->Respawn();
            break;
        }
        case SCRIPT_COMMAND_SET_EQUIPMENT_SLOTS:            // 42
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            Creature* pCSource = static_cast<Creature*>(pSource);

            // reset default
            if (m_script->setEquipment.resetDefault)
            {
                pCSource->LoadEquipment(pCSource->GetCreatureInfo()->EquipmentTemplateId, true);
                break;
            }

            // main hand
            if (m_script->textId[0] >= 0)
            {
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_0, m_script->textId[0]);
            }

            // off hand
            if (m_script->textId[1] >= 0)
            {
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_1, m_script->textId[1]);
            }

            // ranged
            if (m_script->textId[2] >= 0)
            {
                pCSource->SetVirtualItem(VIRTUAL_ITEM_SLOT_2, m_script->textId[2]);
            }
            break;
        }
        case SCRIPT_COMMAND_RESET_GO:                       // 43
        {
            if (LogIfNotGameObject(pTarget))
            {
                break;
            }

            GameObject* pGoTarget = static_cast<GameObject*>(pTarget);

            switch (pGoTarget->GetGoType())
            {
                case GAMEOBJECT_TYPE_DOOR:
                case GAMEOBJECT_TYPE_BUTTON:
                    pGoTarget->ResetDoorOrButton();
                    break;
                default:
                    sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u failed for gameobject(buddyEntry: %u). Gameobject is not a door or button", m_type, m_script->id, m_script->command, m_script->buddyEntry);
                    break;
            }
            break;
        }
        case SCRIPT_COMMAND_UPDATE_TEMPLATE:                // 44
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            Creature* pCre = static_cast<Creature*>(pSource);

            if (pCre->GetEntry() != m_script->updateTemplate.entry)
            {
                pCre->UpdateEntry(m_script->updateTemplate.entry, m_script->updateTemplate.faction ? HORDE : ALLIANCE);
            }
            else
            {
                sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u: source creature already has the specified creature entry %u", m_type, m_script->id, m_script->command, m_script->updateTemplate.entry);
            }
            break;
        }
        case SCRIPT_COMMAND_XP_USER:                        // 53
        {
            Player* pPlayer = GetPlayerTargetOrSourceAndLog(pSource, pTarget);
            if (!pPlayer)
            {
                break;
            }

            if (m_script->xpDisabled.flags)
            {
                pPlayer->SetFlag(PLAYER_FLAGS, PLAYER_FLAGS_XP_USER_DISABLED);
            }
            else
            {
                pPlayer->RemoveFlag(PLAYER_FLAGS, PLAYER_FLAGS_XP_USER_DISABLED);
            }
            break;
        }

        case SCRIPT_COMMAND_SET_FLY:                        // 59
        {
            if (LogIfNotCreature(pSource))
            {
                break;
            }

            // enable / disable the fly anim flag
            if (m_script->data_flags & SCRIPT_FLAG_COMMAND_ADDITIONAL)
            {
                if (m_script->fly.enable)
                {
#if defined(CLASSIC) || defined(TBC) || defined(WOTLK)
                    pSource->SetByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND);
#else
                    pSource->SetByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_FLY_ANIM);
#endif
                }
                else
                {
#if defined(CLASSIC) || defined(TBC) || defined(WOTLK)
                    pSource->RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_ALWAYS_STAND);
#else
                    pSource->RemoveByteFlag(UNIT_FIELD_BYTES_1, 3, UNIT_BYTE1_FLAG_FLY_ANIM);
#endif
                }
            }

            ((Creature*)pSource)->SetLevitate((m_script->fly.enable == 0) ? false : true);
            break;
        }
        default:
            sLog.outErrorDb(" DB-SCRIPTS: Process table `db_scripts [type = %d]` id %u, command %u unknown command used.", m_type, m_script->id, m_script->command);
            break;
    }

    return false;
}
