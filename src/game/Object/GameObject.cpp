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

#include "GameObject.h"
#include "G3D/Quat.h"
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "Opcodes.h"
#include "WorldPacket.h"
#include "World.h"
#include "Database/DatabaseEnv.h"
#include "LootMgr.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "InstanceData.h"
#include "MapManager.h"
#include "MapPersistentStateMgr.h"
#include "BattleGround/BattleGround.h"
#include "BattleGround/BattleGroundAV.h"
#include "OutdoorPvP/OutdoorPvP.h"
#include "Util.h"
#include "ScriptMgr.h"
#include "vmap/GameObjectModel.h"
#include "CreatureAISelector.h"
#include "SQLStorages.h"
#include "GameObjectAI.h"
#include <memory>

#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */


/**
 * @brief Creates a game object instance with default runtime state.
 */
GameObject::GameObject() : WorldObject(),
    loot(this),
    m_model(NULL),
    m_goInfo(NULL),
    m_AI_locked(false)
{
    m_objectType |= TYPEMASK_GAMEOBJECT;
    m_objectTypeId = TYPEID_GAMEOBJECT;
    m_updateFlag = (UPDATEFLAG_ALL | UPDATEFLAG_HAS_POSITION);

    m_valuesCount = GAMEOBJECT_END;
    m_respawnTime = 0;
    m_respawnDelayTime = 25;
    m_lootState = GO_READY;
    m_spawnedByDefault = true;
    m_useTimes = 0;
    m_spellId = 0;
    m_cooldownTime = 0;

    m_captureTimer = 0;

    m_groupLootTimer = 0;
    m_groupLootId = 0;
    m_lootGroupRecipientId = 0;

    m_isInUse = false;
    m_reStockTimer = 0;
    m_rearmTimer = 0;
    m_despawnTimer = 0;

    m_AI_locked;
}

/**
 * @brief Destroys the game object and its collision model.
 */
GameObject::~GameObject()
{
    delete m_model;
}

/**
 * @brief Adds the game object and its model to the world.
 */
void GameObject::AddToWorld()
{
#ifdef ENABLE_ELUNA
    bool inWorld = IsInWorld();
#endif /* ENABLE_ELUNA */

    ///- Register the gameobject for guid lookup
    if (!IsInWorld())
    {
        GetMap()->GetObjectsStore().insert<GameObject>(GetObjectGuid(), (GameObject*)this);
    }

    if (m_model)
    {
        GetMap()->InsertGameObjectModel(*m_model);
    }

    Object::AddToWorld();

    // After Object::AddToWorld so that for initial state the GO is added to the world (and hence handled correctly)
    UpdateCollisionState();

#ifdef ENABLE_ELUNA
    if (!inWorld)
    {
        if (Eluna* e = GetEluna())
        {
            e->OnAddToWorld(this);
        }
    }
#endif /* ENABLE_ELUNA */

}

/**
 * @brief Removes the game object and its model from the world.
 */
void GameObject::RemoveFromWorld()
{
    ///- Remove the gameobject from the accessor
    if (IsInWorld())
    {
#ifdef ENABLE_ELUNA
        if (Eluna* e = GetEluna())
        {
            e->OnRemoveFromWorld(this);
        }
#endif /* ENABLE_ELUNA */

        // Notify the outdoor pvp script
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetZoneId()))
        {
            outdoorPvP->HandleGameObjectRemove(this);
        }

        // Remove GO from owner
        if (ObjectGuid owner_guid = GetOwnerGuid())
        {
            if (Unit* owner = sObjectAccessor.GetUnit(*this, owner_guid))
            {
                owner->RemoveGameObject(this, false);
            }
            else
            {
                sLog.outError("Delete %s with SpellId %u LinkedGO %u that lost references to owner %s GO list. Crash possible later.",
                    GetGuidStr().c_str(), m_spellId, GetGOInfo()->GetLinkedGameObjectEntry(), owner_guid.GetString().c_str());
            }
        }

        if (m_model && GetMap()->ContainsGameObjectModel(*m_model))
        {
            GetMap()->RemoveGameObjectModel(*m_model);
        }

        GetMap()->GetObjectsStore().erase<GameObject>(GetObjectGuid(), (GameObject*)NULL);
    }

    Object::RemoveFromWorld();
}

/**
 * @brief Performs cleanup before deleting the game object.
 */
void GameObject::CleanupsBeforeDelete()
{
    WorldObject::CleanupsBeforeDelete();
}

/**
 * @brief Creates a game object from template and placement data.
 *
 * @param guidlow The low GUID to assign.
 * @param name_id The gameobject entry id.
 * @param map The target map.
 * @param x The x coordinate.
 * @param y The y coordinate.
 * @param z The z coordinate.
 * @param ang The facing angle.
 * @param r0 Quaternion x component.
 * @param r1 Quaternion y component.
 * @param r2 Quaternion z component.
 * @param r3 Quaternion w component.
 * @param animprogress The initial animation progress.
 * @param go_state The initial gameobject state.
 * @return true if creation succeeded; otherwise, false.
 */
bool GameObject::Create(uint32 guidlow, uint32 name_id, Map* map,float x, float y, float z, float ang,
    float r0, float r1, float r2, float r3, uint32 animprogress, GOState go_state)
{
    if (!map)
    {
        return false;
    }

    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(name_id);
    if (!goinfo)
    {
        sLog.outErrorDb("Gameobject (GUID: %u) not created: Entry %u does not exist in `gameobject_template`", guidlow, name_id);
        return false;
    }

    if (goinfo->type >= MAX_GAMEOBJECT_TYPE)
    {
        sLog.outErrorDb("Gameobject (GUID: %u) not created: Entry %u has invalid type %u in `gameobject_template`. It may crash client if created.", guidlow, name_id, goinfo->type);
        return false;
    }

    Object::_Create(guidlow, goinfo->id, HIGHGUID_GAMEOBJECT);

    // let's make sure we don't send the client invalid quaternion
    if (r0 == 0.0f && r1 == 0.0f && r2 == 0.0f)
    {
        r2 = sin(ang/2);
        r3 = cos(ang/2);
    }

    G3D::Quat q(r0, r1, r2, r3);
    q.unitize();

    float o = GetOrientationFromQuat(q);
    Relocate(x, y, z, o);
    SetMap(map);

    if (!IsPositionValid())
    {
        sLog.outError("Gameobject (GUID: %u Entry: %u ) not created. Suggested coordinates are invalid (X: %f Y: %f)", guidlow, name_id, x, y);
        return false;
    }

    SetQuaternion(q);
    SetGOInfo(goinfo);
    SetObjectScale(m_goInfo->size);
    SetFloatValue(GAMEOBJECT_POS_X, x);
    SetFloatValue(GAMEOBJECT_POS_Y, y);
    SetFloatValue(GAMEOBJECT_POS_Z, z);
    SetUInt32Value(GAMEOBJECT_FACTION, m_goInfo->faction);
    SetUInt32Value(GAMEOBJECT_FLAGS, m_goInfo->flags);
    SetEntry(m_goInfo->id);
    SetDisplayId(m_goInfo->displayId);
    SetGoState(go_state);
    SetGoType(GameobjectTypes(m_goInfo->type));

    SetGoAnimProgress(animprogress);

    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_TRAP:
        case GAMEOBJECT_TYPE_FISHINGNODE:
            m_lootState = GO_NOT_READY;                     // Initialize Traps and Fishingnode delayed in ::Update
            break;
        case GAMEOBJECT_TYPE_CHEST:
            RollIfMineralVein();
            break;
        default:
            break;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnSpawn(this);
    }
#endif /* ENABLE_ELUNA */

    // Notify the battleground or outdoor pvp script
    if (map->IsBattleGround())
    {
        ((BattleGroundMap*)map)->GetBG()->HandleGameObjectCreate(this);
    }
    else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetZoneId()))
    {
        outdoorPvP->HandleGameObjectCreate(this);
    }

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that map.
    // Normally non-players do not teleport to other maps.
    if (InstanceData* iData = map->GetInstanceData())
    {
        iData->OnObjectCreate(this);
    }

    return true;
}


/**
 * @brief Refreshes the game object spawn state on the map.
 */
void GameObject::Refresh()
{
    // not refresh despawned not casted GO (despawned casted GO destroyed in all cases anyway)
    if (m_respawnTime > 0 && m_spawnedByDefault)
    {
        return;
    }

    if (isSpawned())
    {
        GetMap()->Add(this);
    }
}

/**
 * @brief Records a unique player use for this game object.
 *
 * @param player The player using the object.
 */
void GameObject::AddUniqueUse(Player* player)
{
    AddUse();

    if (!m_firstUser)
    {
        m_firstUser = player->GetObjectGuid();
    }

    m_UniqueUsers.insert(player->GetObjectGuid());
}

/**
 * @brief Despawns or schedules removal of the game object.
 */
void GameObject::Delete()
{
    SendObjectDeSpawnAnim(GetObjectGuid());

    SetGoState(GO_STATE_READY);
    SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);

    if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
    {
        sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow());
    }
    else
    {
        AddObjectToRemoveList();
    }
}

/**
 * @brief Saves the loaded game object back to the database.
 */
void GameObject::SaveToDB()
{
    // this should only be used when the gameobject has already been loaded
    // preferably after adding to map, because mapid may not be valid otherwise
    GameObjectData const* data = sObjectMgr.GetGOData(GetGUIDLow());
    if (!data)
    {
        sLog.outError("GameObject::SaveToDB failed, can not get gameobject data!");
        return;
    }

    SaveToDB(GetMapId());
}

/**
 * @brief Saves the game object spawn data to the database for a map.
 *
 * @param mapid The map id to persist.
 */
void GameObject::SaveToDB(uint32 mapid)
{
    const GameObjectInfo* goI = GetGOInfo();

    if (!goI)
    {
        return;
    }

    // update in loaded data (changing data only in this place)
    GameObjectData& data = sObjectMgr.NewGOData(GetGUIDLow());

    // data->guid = guid don't must be update at save
    data.id = GetEntry();
    data.mapid = mapid;
    data.posX = GetFloatValue(GAMEOBJECT_POS_X);
    data.posY = GetFloatValue(GAMEOBJECT_POS_Y);
    data.posZ = GetFloatValue(GAMEOBJECT_POS_Z);
    data.orientation = GetFloatValue(GAMEOBJECT_FACING);
    data.rotation0 = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    data.rotation1 = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    data.rotation2 = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    data.rotation3 = GetFloatValue(GAMEOBJECT_ROTATION + 3);
    data.spawntimesecs = m_spawnedByDefault ? (int32)m_respawnDelayTime : -(int32)m_respawnDelayTime;
    data.animprogress = GetGoAnimProgress();
    data.go_state = GetGoState();

    // updated in DB
    std::ostringstream ss;
    ss << "INSERT INTO `gameobject` VALUES ( "
       << GetGUIDLow() << ", "
       << GetEntry() << ", "
       << mapid << ", "
       << GetFloatValue(GAMEOBJECT_POS_X) << ", "
       << GetFloatValue(GAMEOBJECT_POS_Y) << ", "
       << GetFloatValue(GAMEOBJECT_POS_Z) << ", "
       << GetFloatValue(GAMEOBJECT_FACING) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 1) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 2) << ", "
       << GetFloatValue(GAMEOBJECT_ROTATION + 3) << ", "
       << m_respawnDelayTime << ", "
       << uint32(GetGoAnimProgress()) << ", "
       << uint32(GetGoState()) << ")";

    WorldDatabase.BeginTransaction();
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("%s", ss.str().c_str());
    WorldDatabase.CommitTransaction();
}

/**
 * @brief Loads a game object from static database spawn data.
 *
 * @param guid The database GUID.
 * @param map The destination map.
 * @return true if loading succeeded; otherwise, false.
 */
bool GameObject::LoadFromDB(uint32 guid, Map* map)
{
    GameObjectData const* data = sObjectMgr.GetGOData(guid);

    if (!data)
    {
        sLog.outErrorDb("Gameobject (GUID: %u) not found in table `gameobject`, can't load. ", guid);
        return false;
    }

    uint32 entry = data->id;
    // uint32 map_id = data->mapid;                         // already used before call
    float x = data->posX;
    float y = data->posY;
    float z = data->posZ;
    float ang = data->orientation;

    float rotation0 = data->rotation0;
    float rotation1 = data->rotation1;
    float rotation2 = data->rotation2;
    float rotation3 = data->rotation3;

    uint32 animprogress = data->animprogress;
    GOState go_state = data->go_state;

    if (!Create(guid, entry, map, x, y, z, ang, rotation0, rotation1, rotation2, rotation3, animprogress, go_state))
    {
        return false;
    }

    if (!GetGOInfo()->GetDespawnPossibility() && !GetGOInfo()->IsDespawnAtAction() && data->spawntimesecs >= 0)
    {
        SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);
        m_spawnedByDefault = true;
        m_respawnDelayTime = 0;
        m_respawnTime = 0;
    }
    else
    {
        if (data->spawntimesecs >= 0)
        {
            m_spawnedByDefault = true;
            m_respawnDelayTime = data->spawntimesecs;

            m_respawnTime  = map->GetPersistentState()->GetGORespawnTime(GetGUIDLow());

            // ready to respawn
            if (m_respawnTime && m_respawnTime <= time(NULL))
            {
                m_respawnTime = 0;
                map->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), 0);
            }
        }
        else
        {
            m_spawnedByDefault = false;
            m_respawnDelayTime = -data->spawntimesecs;
            m_respawnTime = 0;
        }
    }

    AIM_Initialize();

    return true;
}

struct GameObjectRespawnDeleteWorker
{
    explicit GameObjectRespawnDeleteWorker(uint32 guid) : i_guid(guid) {}

    void operator()(MapPersistentState* state)
    {
        state->SaveGORespawnTime(i_guid, 0);
    }

    uint32 i_guid;
};

/**
 * @brief Deletes the static database spawn record for this game object.
 */
void GameObject::DeleteFromDB()
{
    if (!HasStaticDBSpawnData())
    {
        DEBUG_LOG("Trying to delete not saved gameobject!");
        return;
    }

    GameObjectRespawnDeleteWorker worker(GetGUIDLow());
    sMapPersistentStateMgr.DoForAllStatesWithMapId(GetMapId(), worker);

    sObjectMgr.DeleteGOData(GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `game_event_gameobject` WHERE `guid` = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM `gameobject_battleground` WHERE `guid` = '%u'", GetGUIDLow());
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/

/**
 * @brief Checks whether the game object starts the specified quest.
 *
 * @param quest_id The quest identifier.
 * @return true if the quest is related to this game object; otherwise, false.
 */
bool GameObject::HasQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds = sObjectMgr.GetGOQuestRelationsMapBounds(GetEntry());
    for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks whether the game object is involved in the specified quest.
 *
 * @param quest_id The quest identifier.
 * @return true if the quest is an involved relation for this game object; otherwise, false.
 */
bool GameObject::HasInvolvedQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(GetEntry());
    for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
        {
            return true;
        }
    }
    return false;
}

/**
 * @brief Checks whether the game object behaves as a transport.
 *
 * @return true if the game object is a transport type; otherwise, false.
 */
bool GameObject::IsTransport() const
{
    // If something is marked as a transport, don't transmit an out of range packet for it.
    GameObjectInfo const* gInfo = GetGOInfo();
    if (!gInfo)
    {
        return false;
    }
    return gInfo->type == GAMEOBJECT_TYPE_TRANSPORT || gInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT;
}

/**
 * @brief Gets the unit that owns this game object.
 *
 * @return The owning unit, or null if none exists.
 */
Unit* GameObject::GetOwner() const
{
    return sObjectAccessor.GetUnit(*this, GetOwnerGuid());
}

/**
 * @brief Saves the current respawn time to persistent state if needed.
 */
void GameObject::SaveRespawnTime()
{
    if (m_respawnTime > time(NULL) && m_spawnedByDefault)
    {
        GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), m_respawnTime);
    }
}

/**
 * @brief Checks whether the game object is visible for a player in the current state.
 *
 * @param u The observing player.
 * @param viewPoint The viewpoint used for distance checks.
 * @param inVisibleList true when evaluating an already-visible object.
 * @return true if the object should be visible; otherwise, false.
 */
bool GameObject::IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    // Not in world
    if (!IsInWorld() || !u->IsInWorld())
    {
        return false;
    }

    // Transport always visible at this step implementation
    if (IsTransport() && IsInMap(u))
    {
        return true;
    }

    float visibleDistance = GetMap()->GetVisibilityDistance() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f);

    // quick check visibility false cases for non-GM-mode
    if (!u->isGameMaster())
    {
        // despawned and then not visible for non-GM in GM-mode
        if (!isSpawned())
        {
            return false;
        }

        // special invisibility cases
        switch (GetGOInfo()->type)
        {
            case GAMEOBJECT_TYPE_TRAP:
            {
                if (GetGOInfo()->trap.stealthed == 0 && GetGOInfo()->trap.stealthAffected == 0)
                {
                    break;
                }

                Unit* owner = GetOwner();

                if (!owner || u->IsHostileTo(owner))
                {

                    visibleDistance = 10.5f;
                    //2^3=8 and 300 - from spell 2836, EFFECT_INDEX_1 - SPELL_AURA_MOD_INVISIBILITY_DETECTION; TODO check 200 and improve
                    if (u->GetMaxPositiveAuraModifierByMiscValue(SPELL_AURA_MOD_INVISIBILITY_DETECTION, 8) < 200)
                    {
                        if (u->getClass() != CLASS_ROGUE)
                        {
                            return false;       // a wild or enemy trap cannot be seen by non-rogues without proper invis detection
                        }
                        visibleDistance = 0.0f; // minimal detection distance, will be normalized below
                    }

                    if (owner)
                    {
                        // apply to the "owner" and "u" the rules for usual stealth detection; the fragment is taken from Unit::IsVisibleForOrDetect
                        // Visible distance based on stealth value (stealth rank 4 300MOD, 10.5 - 3 = 7.5)
                        visibleDistance -= (owner->getLevel() / 20.0f);  // for rogue stealth (4 spells): modifier = 5*level

                        // Visible distance is modified by
                        //-Level Diff (every level diff = 1.0f in visible distance)
                        visibleDistance += int32(u->GetLevelForTarget(owner)) - int32(owner->GetLevelForTarget(u));
                    }

                    //-Stealth Detection(negative like paranoia)
                    visibleDistance += (int32(u->GetTotalAuraModifier(SPELL_AURA_MOD_STEALTH_DETECT))) / 5.0f;

                    // normalize visible distance
                    if (visibleDistance > MAX_PLAYER_STEALTH_DETECT_RANGE)
                    {
                        visibleDistance = MAX_PLAYER_STEALTH_DETECT_RANGE;
                    }
                    else if (visibleDistance < GetGOInfo()->trap.radius + INTERACTION_DISTANCE)
                    {
                        visibleDistance = GetGOInfo()->trap.radius + INTERACTION_DISTANCE;
                    }
                }

            }

            case GAMEOBJECT_TYPE_SPELL_FOCUS:
            {
                if (GetGOInfo()->spellFocus.serverOnly == 1)
                {
                    return false;
                }
                break;
            }
        }

        // Smuggled Mana Cell required 10 invisibility type detection/state
        if (GetEntry() == 187039 && ((u->m_detectInvisibilityMask | u->m_invisibilityMask) & (1 << 10)) == 0)
        {
            return false;
        }
    }

    // check distance
    return IsWithinDistInMap(viewPoint, visibleDistance, false);
}

/**
 * @brief Forces a respawn for a default-spawned game object.
 */
void GameObject::Respawn()
{
    if (m_spawnedByDefault && m_respawnTime > 0)
    {
        m_respawnTime = time(NULL);
        GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), 0);
    }
}

/**
 * @brief Checks whether this game object should activate for a player's quests.
 *
 * @param pTarget The player using the object.
 * @return true if the object should be quest-active; otherwise, false.
 */
bool GameObject::ActivateToQuest(Player* pTarget) const
{
    // if GO is ReqCreatureOrGoN for quest
    if (pTarget->HasQuestForGO(GetEntry()))
    {
        return true;
    }

    if (!sObjectMgr.IsGameObjectForQuests(GetEntry()))
    {
        return false;
    }

    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_QUESTGIVER:
        {
            // Not fully clear when GO's can activate/deactivate
            // For cases where GO has additional (except quest itself),
            // these conditions are not sufficient/will fail.
            // Never expect flags|4 for these GO's? (NF-note: It doesn't appear it's expected)

            QuestRelationsMapBounds bounds = sObjectMgr.GetGOQuestRelationsMapBounds(GetEntry());

            for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                const Quest* qInfo = sObjectMgr.GetQuestTemplate(itr->second);

                if (pTarget->CanTakeQuest(qInfo, false))
                {
                    return true;
                }
            }

            bounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(GetEntry());

            for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                if ((pTarget->GetQuestStatus(itr->second) == QUEST_STATUS_INCOMPLETE || pTarget->GetQuestStatus(itr->second) == QUEST_STATUS_COMPLETE) &&
                    !pTarget->GetQuestRewardStatus(itr->second))
                {
                    return true;
                }
            }

            break;
        }
        // scan GO chest with loot including quest items
        case GAMEOBJECT_TYPE_CHEST:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->chest.questId) == QUEST_STATUS_INCOMPLETE)
            {
                return true;
            }

            if (LootTemplates_Gameobject.HaveQuestLootForPlayer(GetGOInfo()->GetLootId(), pTarget))
            {
                // look for battlegroundAV for some objects which are only activated after mine gots captured by own team
                if (GetEntry() == BG_AV_OBJECTID_MINE_N || GetEntry() == BG_AV_OBJECTID_MINE_S)
                {
                    if (BattleGround* bg = pTarget->GetBattleGround())
                    {
                        if (bg->GetTypeID() == BATTLEGROUND_AV && !(((BattleGroundAV*)bg)->PlayerCanDoMineQuest(GetEntry(), pTarget->GetTeam())))
                        {
                            return false;
                        }
                    }
                }
                return true;
            }
            break;
        }
        case GAMEOBJECT_TYPE_GENERIC:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->_generic.questID) == QUEST_STATUS_INCOMPLETE)
            {
                return true;
            }
            break;
        }
        case GAMEOBJECT_TYPE_SPELL_FOCUS:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->spellFocus.questID) == QUEST_STATUS_INCOMPLETE)
            {
                return true;
            }
            break;
        }
        case GAMEOBJECT_TYPE_GOOBER:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->goober.questId) == QUEST_STATUS_INCOMPLETE)
            {
                return true;
            }
            break;
        }
        default:
            break;
    }

    return false;
}

/**
 * @brief Summons the linked trap associated with this game object, if any.
 */
void GameObject::SummonLinkedTrapIfAny()
{
    uint32 linkedEntry = GetGOInfo()->GetLinkedGameObjectEntry();
    if (!linkedEntry)
    {
        return;
    }

    GameObject* linkedGO = new GameObject;
    if (!linkedGO->Create(GetMap()->GenerateLocalLowGuid(HIGHGUID_GAMEOBJECT), linkedEntry, GetMap(),
        GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation(), 0.0f, 0.0f, 0.0f, 0.0f, GO_ANIMPROGRESS_DEFAULT, GO_STATE_READY))
    {
        delete linkedGO;
        return;
    }

    linkedGO->SetRespawnTime(GetRespawnDelay());
    linkedGO->SetSpellId(GetSpellId());

    if (GetOwnerGuid())
    {
        linkedGO->SetOwnerGuid(GetOwnerGuid());
        linkedGO->SetUInt32Value(GAMEOBJECT_LEVEL, GetUInt32Value(GAMEOBJECT_LEVEL));
    }

    linkedGO->AIM_Initialize();
    GetMap()->Add(linkedGO);
}

/**
 * @brief Triggers the linked trap game object against a target.
 *
 * @param target The unit activating the trap.
 */
void GameObject::TriggerLinkedGameObject(Unit* target)
{
    uint32 trapEntry = GetGOInfo()->GetLinkedGameObjectEntry();

    if (!trapEntry)
    {
        return;
    }

    GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
    if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
    {
        return;
    }

    SpellEntry const* trapSpell = sSpellStore.LookupEntry(trapInfo->trap.spellId);

    // The range to search for linked trap is weird. We set 0.5 as default. Most (all?)
    // traps are probably expected to be pretty much at the same location as the used GO,
    // so it appears that using range from spell is obsolete.
    float range = 0.5f;

    if (trapSpell)                                          // checked at load already
    {
        range = GetSpellMaxRange(sSpellRangeStore.LookupEntry(trapSpell->rangeIndex));
    }

    // search nearest linked GO
    GameObject* trapGO = NULL;

    {
        // search closest with base of used GO, using max range of trap spell as search radius (why? See above)
        MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*this, trapEntry, range);
        MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(trapGO, go_check);

        Cell::VisitGridObjects(this, checker, range);
    }

    // found correct GO
    if (trapGO)
    {
        trapGO->Use(target);
    }
}

/**
 * @brief Finds a nearby fishing hole around this game object.
 *
 * @param range The search radius.
 * @return The nearest fishing hole, or null if none was found.
 */
GameObject* GameObject::LookupFishingHoleAround(float range)
{
    GameObject* ok = NULL;

    MaNGOS::NearestGameObjectFishingHoleCheck u_check(*this, range);
    MaNGOS::GameObjectSearcher<MaNGOS::NearestGameObjectFishingHoleCheck> checker(ok, u_check);
    Cell::VisitGridObjects(this, checker, range);

    return ok;
}

/**
 * @brief Checks whether collision is currently enabled for the game object.
 *
 * @return true if the model should be collidable; otherwise, false.
 */
bool GameObject::IsCollisionEnabled() const
{
    if (!isSpawned())
    {
        return false;
    }

    // TODO: Possible that this function must consider multiple checks
    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:
            return GetGoState() != GO_STATE_ACTIVE && GetGoState() != GO_STATE_ACTIVE_ALTERNATIVE;

        default:
            return true;
    }
}

/**
 * @brief Resets a door or button back to its default state.
 */
void GameObject::ResetDoorOrButton()
{
    if (m_lootState == GO_READY || m_lootState == GO_JUST_DEACTIVATED)
    {
        return;
    }

    SwitchDoorOrButton(false);
    SetLootState(GO_JUST_DEACTIVATED);
    m_cooldownTime = 0;
}

/**
 * @brief Activates a door or button and schedules restoration.
 *
 * @param time_to_restore The delay before reset.
 * @param alternative true to use the alternative active state.
 */
void GameObject::UseDoorOrButton(uint32 time_to_restore, bool alternative /* = false */)
{
    if (m_lootState != GO_READY)
    {
        return;
    }

    if (!time_to_restore)
    {
        time_to_restore = GetGOInfo()->GetAutoCloseTime();
    }

    SwitchDoorOrButton(true, alternative);
    SetLootState(GO_ACTIVATED);

    m_cooldownTime = time(NULL) + time_to_restore;
}

/**
 * @brief Switches a door or button between active and ready states.
 *
 * @param activate true to activate; false to deactivate.
 * @param alternative true to use the alternative active state.
 */
void GameObject::SwitchDoorOrButton(bool activate, bool alternative /* = false */)
{
    if (activate)
    {
        SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
    }
    else
    {
        RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
    }

    if (GetGoState() == GO_STATE_READY)                     // if closed -> open
    {
        SetGoState(alternative ? GO_STATE_ACTIVE_ALTERNATIVE : GO_STATE_ACTIVE);
    }
    else                                                    // if open -> close
    {
        SetGoState(GO_STATE_READY);
    }
}


// overwrite WorldObject function for proper name localization

/**
 * @brief Gets the localized name for a locale index.
 *
 * @param loc_idx The locale index.
 * @return The localized name, or the default name if unavailable.
 */
const char* GameObject::GetNameForLocaleIdx(int32 loc_idx) const
{
    if (loc_idx >= 0)
    {
        GameObjectLocale const* cl = sObjectMgr.GetGameObjectLocale(GetEntry());
        if (cl)
        {
            if (cl->Name.size() > (size_t)loc_idx && !cl->Name[loc_idx].empty())
            {
                return cl->Name[loc_idx].c_str();
            }
        }
    }

    return GetName();
}

/**
 * @brief Stores the object's rotation quaternion and updates the model.
 *
 * @param q The quaternion to apply.
 */
void GameObject::SetQuaternion(G3D::Quat const& q)
{
    SetFloatValue(GAMEOBJECT_ROTATION + 0, q.x);
    SetFloatValue(GAMEOBJECT_ROTATION + 1, q.y);
    SetFloatValue(GAMEOBJECT_ROTATION + 2, q.z);
    SetFloatValue(GAMEOBJECT_ROTATION + 3, q.w);

    if (m_model)
    {
        m_model->UpdateRotation(q);
    }
}

/**
 * @brief Reads the object's current rotation quaternion.
 *
 * @param q Receives the quaternion components.
 */
void GameObject::GetQuaternion(G3D::Quat& q) const
{
    q.x = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    q.y = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    q.z = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    q.w = GetFloatValue(GAMEOBJECT_ROTATION + 3);
}

/**
 * @brief Converts a quaternion rotation to a facing orientation.
 *
 * @param q The quaternion to evaluate.
 * @return The normalized orientation angle.
 */
float GameObject::GetOrientationFromQuat(G3D::Quat const& q)
{
    double t1 = +2.0f * (q.w * q.z + q.x * q.y);
    double t2 = +1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    return MapManager::NormalizeOrientation(std::atan2(t1, t2));
}

/**
 * @brief Checks whether the game object is hostile to a unit.
 *
 * @param unit The unit to test.
 * @return true if hostile; otherwise, false.
 */
bool GameObject::IsHostileTo(Unit const* unit) const
{
    // always non-hostile to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER && ((Player const*)unit)->isGameMaster())
    {
        return false;
    }

    // test owner instead if have
    if (Unit const* owner = GetOwner())
    {
        return owner->IsHostileTo(unit);
    }

    if (Unit const* targetOwner = unit->GetCharmerOrOwner())
    {
        return IsHostileTo(targetOwner);
    }

    // for not set faction case: be hostile towards player, not hostile towards not-players
    if (!GetGOInfo()->faction)
    {
        return unit->IsControlledByPlayer();
    }

    // faction base cases
    FactionTemplateEntry const* tester_faction = sFactionTemplateStore.LookupEntry(GetGOInfo()->faction);
    FactionTemplateEntry const* target_faction = unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
    {
        return false;
    }

    // GvP forced reaction and reputation case
    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)unit)->GetReputationMgr().GetForcedRankIfAny(tester_faction))
            {
                return *force <= REP_HOSTILE;
            }

            // apply reputation state
            FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->faction);
            if (raw_tester_faction && raw_tester_faction->reputationListID >= 0)
            {
                return ((Player const*)unit)->GetReputationMgr().GetRank(raw_tester_faction) <= REP_HOSTILE;
            }
        }
    }

    // common faction based case (GvC,GvP)
    return tester_faction->IsHostileTo(*target_faction);
}

/**
 * @brief Checks whether the game object is friendly to a unit.
 *
 * @param unit The unit to test.
 * @return true if friendly; otherwise, false.
 */
bool GameObject::IsFriendlyTo(Unit const* unit) const
{
    // always friendly to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER && ((Player const*)unit)->isGameMaster())
    {
        return true;
    }

    // test owner instead if have
    if (Unit const* owner = GetOwner())
    {
        return owner->IsFriendlyTo(unit);
    }

    if (Unit const* targetOwner = unit->GetCharmerOrOwner())
    {
        return IsFriendlyTo(targetOwner);
    }

    // for not set faction case (wild object) use hostile case
    if (!GetGOInfo()->faction)
    {
        return false;
    }

    // faction base cases
    FactionTemplateEntry const* tester_faction = sFactionTemplateStore.LookupEntry(GetGOInfo()->faction);
    FactionTemplateEntry const* target_faction = unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
    {
        return false;
    }

    // GvP forced reaction and reputation case
    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)unit)->GetReputationMgr().GetForcedRankIfAny(tester_faction))
            {
                return *force >= REP_FRIENDLY;
            }

            // apply reputation state
            if (FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->faction))
            {
                if (raw_tester_faction->reputationListID >= 0)
                {
                    return ((Player const*)unit)->GetReputationMgr().GetRank(raw_tester_faction) >= REP_FRIENDLY;
                }
            }
        }
    }

    // common faction based case (GvC,GvP)
    return tester_faction->IsFriendlyTo(*target_faction);
}

/**
 * @brief Rolls alternate mineral vein variants for chest-type nodes.
 */
void GameObject::RollIfMineralVein()
{
    GameObjectInfo const* goinfo = ObjectMgr::GetGameObjectInfo(GetEntry());
    if (goinfo->chest.minSuccessOpens != 0 && goinfo->chest.maxSuccessOpens > goinfo->chest.minSuccessOpens) //in this case it is a mineral vein
    {
        uint32 entrynew = RollMineralVein(GetObjectGuid().GetEntry());

        GameObjectInfo const* goinfonew = ObjectMgr::GetGameObjectInfo(entrynew);
        m_goInfo = goinfonew;

        SetUInt32Value(GAMEOBJECT_DISPLAYID, (goinfonew->displayId));
        Object::_ReCreate(entrynew);
    }
}

/**
 * @brief Rolls a replacement mineral vein entry based on zone and rates.
 *
 * @param entry The base vein entry.
 * @return The selected vein entry.
 */
uint32 GameObject::RollMineralVein(uint32 entry)      //Maybe incedicite bloodstone and indurium have alternate spawns?
{
    uint32 entrynew = entry;

    if ((GetZoneId() == 46) || (GetZoneId() == 51)) // each node in searing gorge or burning steppes is able to spawn dark iron
    {
        if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_DARKIRON))
        {
            entrynew = 165658;
        }
        return entrynew;
    }

    if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_LOWER)) // beside silver all base ores have the possibility to spawn the lower base ore type so rol for lower version spawn
    {
        switch (entry)
        {
            case 1735: // Iron can spawn Tin
                entry = 1732;
                break;
            case 2040: // Mithril can spawn Iron
                entry = 1735;
                break;
            case 123310: // Ooze covered mithril can spawn ooze covered iron
                entry = 73939;
                break;
            case 324: // small thorium Vein can spawn Mithril
                entry = 2040;
                break;
            case 123848: // ooze covered thorium Vein can spawn ooze covered mithril
                entry = 123310;
                break;
            case 175404: // Rich thorium Vein can spawn small Thorium vein
                entry = 324;
                break;
            case 177388: // ooze covered Rich thorium Vein can spawn ooze covered thorium
                entry = 123848;
                break;
            default: //default case for copper, tin or not listet special veins
                break;
        }
    }

    switch (entry)                      // Now roll for rare spawn
    {
        case 1732: // Tin can spawn Silver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 1733;
            }
            break;
        case 1735: // Iron can spawn Gold
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 1734;
            }
            break;
        case 73939: // Ooze covered iron can spawn ooze covered gold
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 73941;
            }
            break;
        case 2040: // Mithril can spawn Truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 2047;
            }
            break;
        case 123310: // Ooze covered mithril can spawn ooze covered truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 123309;
            }
            break;
        case 324: // small thorium Vein can spawn Truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 2047;
            }
            break;
        case 123848: // ooze covered thorium Vein can spawn ooze covered truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 123309;
            }
            break;
        case 175404: // Rich thorium Vein can spawn truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 2047;
            }
            break;
        case 177388: // ooze covered Rich thorium Vein can spawn ooze covered truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
            {
                entrynew = 123309;
            }
            break;

        default: //default case for copper or not listet special veins
            entrynew = entry;
    }
    return entrynew;
}

/**
 * @brief Sets the loot state and refreshes collision state.
 *
 * @param state The new loot state.
 */
void GameObject::SetLootState(LootState state)
{
    m_lootState = state;
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnLootStateChanged(this, state);
    }
#endif /* ENABLE_ELUNA */
    UpdateCollisionState();
}

/**
 * @brief Sets the gameobject state and refreshes collision state.
 *
 * @param state The new gameobject state.
 */
void GameObject::SetGoState(GOState state)
{
    SetByteValue(GAMEOBJECT_STATE, 0, state);
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->OnGameObjectStateChanged(this, state);
    }
#endif /* ENABLE_ELUNA */
    UpdateCollisionState();
}

/**
 * @brief Sets the display id and refreshes the collision model.
 *
 * @param modelId The display model id.
 */
void GameObject::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(GAMEOBJECT_DISPLAYID, modelId);
    UpdateModel();
}

/**
 * @brief Updates model collision enablement based on current state.
 */
void GameObject::UpdateCollisionState() const
{
    if (!m_model || !IsInWorld())
    {
        return;
    }

    m_model->SetCollidable(IsCollisionEnabled());
}

/**
 * @brief Rebuilds the collision model for the current display.
 */
void GameObject::UpdateModel()
{
    if (m_model && IsInWorld() && GetMap()->ContainsGameObjectModel(*m_model))
    {
        GetMap()->RemoveGameObjectModel(*m_model);
    }
    delete m_model;

    m_model = GameObjectModel::Create(this);
    if (m_model)
    {
        GetMap()->InsertGameObjectModel(*m_model);
    }
}

/**
 * @brief Starts group loot tracking for this game object.
 *
 * @param group The recipient group.
 * @param timer The loot roll timer.
 */
void GameObject::StartGroupLoot(Group* group, uint32 timer)
{
    m_groupLootId = group->GetId();
    m_groupLootTimer = timer;
}

/**
 * @brief Stops active group loot tracking for this game object.
 */
void GameObject::StopGroupLoot()
{
    if (!m_groupLootId)
    {
        return;
    }

    if (Group* group = sObjectMgr.GetGroupById(m_groupLootId))
    {
        group->EndRoll();
    }

    m_groupLootTimer = 0;
    m_groupLootId = 0;
}

/**
 * @brief Gets the original player loot recipient.
 *
 * @return The original loot recipient player, or null if unavailable.
 */
Player* GameObject::GetOriginalLootRecipient() const
{
    return m_lootRecipientGuid ? sObjectAccessor.FindPlayer(m_lootRecipientGuid) : NULL;
}

/**
 * @brief Gets the original group loot recipient.
 *
 * @return The loot recipient group, or null if unavailable.
 */
Group* GameObject::GetGroupLootRecipient() const
{
    // original recipient group if set and not disbanded
    return m_lootGroupRecipientId ? sObjectMgr.GetGroupById(m_lootGroupRecipientId) : NULL;
}

/**
 * @brief Gets the active player loot recipient for this game object.
 *
 * @return The player who should currently receive loot rights, or null if none.
 */
Player* GameObject::GetLootRecipient() const
{
    // original recipient group if set and not disbanded
    Group* group = GetGroupLootRecipient();

    // original recipient player if online
    Player* player = GetOriginalLootRecipient();

    // if group not set or disbanded return original recipient player if any
    if (!group)
    {
        return player;
    }

    // group case

    // return player if it still be in original recipient group
    if (player && player->GetGroup() == group)
    {
        return player;
    }

    // find any in group
    for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
    {
        if (Player* newPlayer = itr->getSource())
        {
            return newPlayer;
        }
    }
    return NULL;
}

/**
 * @brief Assigns loot rights to a unit and its group if applicable.
 *
 * @param pUnit The unit receiving loot rights, or null to clear them.
 */
void GameObject::SetLootRecipient(Unit* pUnit)
{
    // set the player whose group should receive the right
    // to loot the gameobject after its used
    // should be set to NULL after the loot disappears

    if (!pUnit)
    {
        m_lootRecipientGuid.Clear();
        m_lootGroupRecipientId = 0;
        return;
    }

    Player* player = pUnit->GetCharmerOrOwnerPlayerOrPlayerItself();
    if (!player)                                            // normal creature, no player involved
    {
        return;
    }

    // set player for non group case or if group will disbanded
    m_lootRecipientGuid = player->GetObjectGuid();

    // set group for group existed case including if player will leave group at loot time
    if (Group* group = player->GetGroup())
    {
        m_lootGroupRecipientId = group->GetId();
    }
}

/**
 * @brief Gets the object bounding radius used for visibility and interaction.
 *
 * @return The default game object radius.
 */
float GameObject::GetObjectBoundingRadius() const
{
    // 1.12.1 GameObjectDisplayInfo.dbc not have any info related to size
    return DEFAULT_WORLD_OBJECT_SIZE;
}

/**
 * @brief Checks whether a player has already received skillup credit from this object.
 *
 * @param player The player to test.
 * @return true if the player is in the skillup list; otherwise, false.
 */
bool GameObject::IsInSkillupList(Player* player) const
{
    return m_SkillupSet.find(player->GetObjectGuid()) != m_SkillupSet.end();
}

/**
 * @brief Adds a player to the skillup tracking list.
 *
 * @param player The player to add.
 */
void GameObject::AddToSkillupList(Player* player)
{
    m_SkillupSet.insert(player->GetObjectGuid());
}

struct AddGameObjectToRemoveListInMapsWorker
{
    AddGameObjectToRemoveListInMapsWorker(ObjectGuid guid) : i_guid(guid) {}

    void operator()(Map* map)
    {
        if (GameObject* pGameobject = map->GetGameObject(i_guid))
        {
            pGameobject->AddObjectToRemoveList();
        }
    }

    ObjectGuid i_guid;
};

/**
 * @brief Adds matching spawned instances to remove lists across loaded maps.
 *
 * @param db_guid The database GUID.
 * @param data The static spawn data.
 */
void GameObject::AddToRemoveListInMaps(uint32 db_guid, GameObjectData const* data)
{
    AddGameObjectToRemoveListInMapsWorker worker(ObjectGuid(HIGHGUID_GAMEOBJECT, data->id, db_guid));
    sMapMgr.DoForAllMapsWithMapId(data->mapid, worker);
}

struct SpawnGameObjectInMapsWorker
{
    SpawnGameObjectInMapsWorker(uint32 guid, GameObjectData const* data)
        : i_guid(guid), i_data(data) {}

    void operator()(Map* map)
    {
        // Spawn if necessary (loaded grids only)
        if (map->IsCellLoaded(i_data->posX, i_data->posY))
        {
            GameObject* pGameobject = new GameObject;
            // DEBUG_LOG("Spawning gameobject %u", *itr);
            if (!pGameobject->LoadFromDB(i_guid, map))
            {
                delete pGameobject;
            }
            else
            {
                if (pGameobject->isSpawnedByDefault())
                {
                    map->Add(pGameobject);
                }
            }
        }
    }

    uint32 i_guid;
    GameObjectData const* i_data;
};

/**
 * @brief Spawns this database game object across eligible loaded maps.
 *
 * @param db_guid The database GUID.
 * @param data The static spawn data.
 */
void GameObject::SpawnInMaps(uint32 db_guid, GameObjectData const* data)
{
    SpawnGameObjectInMapsWorker worker(db_guid, data);
    sMapMgr.DoForAllMapsWithMapId(data->mapid, worker);
}

/**
 * @brief Checks whether this object has static database spawn data.
 *
 * @return true if the object has a saved DB spawn; otherwise, false.
 */
bool GameObject::HasStaticDBSpawnData() const
{
    return sObjectMgr.GetGOData(GetGUIDLow()) != NULL;
}



/**
 * @brief Gets the bound script id for this game object.
 *
 * @return The script identifier.
 */
uint32 GameObject::GetScriptId()
{
    return sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, -int32(GetGUIDLow())) ? sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, -int32(GetGUIDLow())) : sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, GetEntry());
}

/**
 * @brief Gets the interaction distance for this game object type.
 *
 * @return The maximum interaction distance.
 */
float GameObject::GetInteractionDistance() const
{
    float maxdist = INTERACTION_DISTANCE;
    switch (GetGoType())
    {
        // TODO: find out how the client calculates the maximal usage distance to spellless working
        // gameobjects like mailboxes - 10.0 is a just an abitrary chosen number
        case GAMEOBJECT_TYPE_MAILBOX:
            maxdist = 10.0f;
            break;
        case GAMEOBJECT_TYPE_FISHINGHOLE:
        case GAMEOBJECT_TYPE_FISHINGNODE:
            maxdist = 20.0f + CONTACT_DISTANCE;     // max spell range
            break;
        default:
            break;
    }
    return maxdist;
}

/**
 * @brief Sends a custom animation packet for this game object.
 *
 * @param animId The animation identifier.
 */
void GameObject::SendGameObjectCustomAnim(uint32 animId /*= 0*/)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8 + 4);
    data << GetObjectGuid();
    data << uint32(animId);
    SendMessageToSet(&data, true);
}

/**
 * @brief Sends a reset-state packet for this game object.
 */
void GameObject::SendGameObjectReset()
{
    WorldPacket data(SMSG_GAMEOBJECT_RESET_STATE, 8);
    data << GetObjectGuid();
    SendMessageToSet(&data, true);
}

/**
 * @brief Initializes the scripted AI instance for the game object.
 *
 * @return true if initialization succeeded; otherwise, false.
 */
bool  GameObject::AIM_Initialize()
{

    // make sure nothing can change the AI during AI update
    if (m_AI_locked)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "AIM_Initialize: failed to init, locked.");
        return false;
    }

    m_AI.reset(sScriptMgr.GetGameObjectAI(this));

    return true;
}
