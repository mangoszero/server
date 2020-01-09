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
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

GameObject::GameObject() : WorldObject(),
    loot(this),
    m_model(NULL),
    m_goInfo(NULL)
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
}

GameObject::~GameObject()
{
    delete m_model;
}

void GameObject::AddToWorld()
{
#ifdef ENABLE_ELUNA
    bool inWorld = IsInWorld();
#endif /* ENABLE_ELUNA */

    ///- Register the gameobject for guid lookup
    if (!IsInWorld())
        { GetMap()->GetObjectsStore().insert<GameObject>(GetObjectGuid(), (GameObject*)this); }

    if (m_model)
        { GetMap()->InsertGameObjectModel(*m_model); }

    Object::AddToWorld();

    // After Object::AddToWorld so that for initial state the GO is added to the world (and hence handled correctly)
    UpdateCollisionState();

#ifdef ENABLE_ELUNA
    if (!inWorld)
        sEluna->OnAddToWorld(this);
#endif /* ENABLE_ELUNA */
}

void GameObject::RemoveFromWorld()
{
    ///- Remove the gameobject from the accessor
    if (IsInWorld())
    {
#ifdef ENABLE_ELUNA
        sEluna->OnRemoveFromWorld(this);
#endif /* ENABLE_ELUNA */

        // Notify the outdoor pvp script
        if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetZoneId()))
            { outdoorPvP->HandleGameObjectRemove(this); }

        // Remove GO from owner
        if (ObjectGuid owner_guid = GetOwnerGuid())
        {
            if (Unit* owner = sObjectAccessor.GetUnit(*this, owner_guid))
                { owner->RemoveGameObject(this, false); }
            else
            {
                sLog.outError("Delete %s with SpellId %u LinkedGO %u that lost references to owner %s GO list. Crash possible later.",
                              GetGuidStr().c_str(), m_spellId, GetGOInfo()->GetLinkedGameObjectEntry(), owner_guid.GetString().c_str());
            }
        }

        if (m_model && GetMap()->ContainsGameObjectModel(*m_model))
            { GetMap()->RemoveGameObjectModel(*m_model); }

        GetMap()->GetObjectsStore().erase<GameObject>(GetObjectGuid(), (GameObject*)NULL);
    }

    Object::RemoveFromWorld();
}

void GameObject::CleanupsBeforeDelete()
{
    WorldObject::CleanupsBeforeDelete();
}

bool GameObject::Create(uint32 guidlow, uint32 name_id, Map* map, float x, float y, float z, float ang, float r0, float r1, float r2, float r3, uint32 animprogress, GOState go_state)
{
    if (!map)
      { return false; }

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
    sEluna->OnSpawn(this);
#endif /* ENABLE_ELUNA */

    // Notify the battleground or outdoor pvp script
    if (map->IsBattleGround())
        { ((BattleGroundMap*)map)->GetBG()->HandleGameObjectCreate(this); }
    else if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(GetZoneId()))
        { outdoorPvP->HandleGameObjectCreate(this); }

    // Notify the map's instance data.
    // Only works if you create the object in it, not if it is moves to that map.
    // Normally non-players do not teleport to other maps.
    if (InstanceData* iData = map->GetInstanceData())
        { iData->OnObjectCreate(this); }

    return true;
}

void GameObject::Update(uint32 update_diff, uint32 p_time)
{
    if (GetObjectGuid().IsMOTransport())
    {
        //((Transport*)this)->Update(p_time);
        return;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    sEluna->UpdateAI(this, update_diff);
#endif /* ENABLE_ELUNA */

    switch (m_lootState)
    {
        case GO_NOT_READY:
        {
            switch (GetGoType())
            {
                case GAMEOBJECT_TYPE_TRAP:                  // Initialized delayed to be able to use GetOwner()
                {
                    // Arming Time for GAMEOBJECT_TYPE_TRAP (6)
                    Unit* owner = GetOwner();
                    if (owner && owner->IsInCombat())
                        { m_cooldownTime = time(NULL) + GetGOInfo()->trap.startDelay; }
                    m_lootState = GO_READY;
                    break;
                }
                case GAMEOBJECT_TYPE_FISHINGNODE:           // Keep not ready for some delay
                {
                    // fishing code (bobber ready)
                    if (time(NULL) > m_respawnTime - FISHING_BOBBER_READY_TIME)
                    {
                        // splash bobber (bobber ready now)
                        Unit* caster = GetOwner();
                        if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                        {
                            SetGoState(GO_STATE_ACTIVE);
                            // SetUInt32Value(GAMEOBJECT_FLAGS, GO_FLAG_NODESPAWN);

                            SendForcedObjectUpdate();

                            SendGameObjectCustomAnim();
                        }

                        m_lootState = GO_READY;             // can be successfully open with some chance
                    }
                    break;
                }
                default:
                    break;
            }
            break;
        }
        case GO_READY:
        {
            if (m_respawnTime > 0)                          // timer on
            {
                if (m_respawnTime <= time(NULL))            // timer expired
                {
                    m_respawnTime = 0;
                    ClearAllUsesData();

                    switch (GetGoType())
                    {
                        case GAMEOBJECT_TYPE_FISHINGNODE:   // can't fish now
                        {
                            Unit* caster = GetOwner();
                            if (caster && caster->GetTypeId() == TYPEID_PLAYER)
                            {
                                caster->FinishSpell(CURRENT_CHANNELED_SPELL);

                                WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
                                ((Player*)caster)->GetSession()->SendPacket(&data);
                            }
                            // can be deleted
                            m_lootState = GO_JUST_DEACTIVATED;
                            return;
                        }
                        case GAMEOBJECT_TYPE_DOOR:
                        case GAMEOBJECT_TYPE_BUTTON:
                            // we need to open doors if they are closed (add there another condition if this code breaks some usage, but it need to be here for battlegrounds)
                            if (GetGoState() != GO_STATE_READY)
                                { ResetDoorOrButton(); }
                            // flags in AB are type_button and we need to add them here so no break!
                        default:
                            if (!m_spawnedByDefault)        // despawn timer
                            {
                                // can be despawned or destroyed
                                SetLootState(GO_JUST_DEACTIVATED);
                                // Remove Wild-Summoned GO on timer expire
                                if (!HasStaticDBSpawnData())
                                {
                                    if (Unit* owner = GetOwner())
                                        { owner->RemoveGameObject(this, false); }
                                    Delete();
                                }
                                return;
                            }

                            // respawn timer
                            GetMap()->Add(this);
                            break;
                    }
                }
            }

            if (isSpawned())
            {
                // traps can have time and can not have
                GameObjectInfo const* goInfo = GetGOInfo();
                if (goInfo->type == GAMEOBJECT_TYPE_TRAP)   // traps
                {
                    if (m_cooldownTime >= time(NULL))
                        { return; }

                    // FIXME: this is activation radius (in different casting radius that must be selected from spell data)
                    // TODO: move activated state code (cast itself) to GO_ACTIVATED, in this place only check activating and set state
                    float radius = float(goInfo->trap.radius);
                    if (!radius)
                    {
                        if (goInfo->trap.cooldown != 3)     // cast in other case (at some triggering/linked go/etc explicit call)
                            { return; }
                        else
                        {
                            if (m_respawnTime > 0)
                                { break; }

                            // battlegrounds gameobjects has data2 == 0 && data5 == 3
                            radius = float(goInfo->trap.cooldown);
                        }
                    }

                    SpellEntry const* se = sSpellStore.LookupEntry(goInfo->trap.spellId);
                    if(IsAreaOfEffectSpell(se))
                    {
                        MaNGOS::AllSpecificUnitsInGameObjectRangeDo unit_do(this, radius, IsPositiveSpell(se));
                        MaNGOS::UnitWorker<MaNGOS::AllSpecificUnitsInGameObjectRangeDo> worker(unit_do);
                        Cell::VisitAllObjects(this,worker,radius);
                    }
                    else
                    {
                        Unit* targetUnit = NULL;                     // pointer to appropriate target if found any
                        MaNGOS::AnySpecificUnitInGameObjectRangeCheck u_check(this, radius, IsPositiveSpell(se));
                        MaNGOS::UnitSearcher<MaNGOS::AnySpecificUnitInGameObjectRangeCheck> checker(targetUnit, u_check);
                        Cell::VisitAllObjects(this, checker, radius);
                        if (targetUnit)
                          { Use(targetUnit); }
                    }
                }

                if (uint32 max_charges = goInfo->GetCharges())
                {
                    if (m_useTimes >= max_charges)
                    {
                        m_useTimes = 0;
                        SetLootState(GO_JUST_DEACTIVATED);  // can be despawned or destroyed
                    }
                }
            }
            break;
        }
        case GO_ACTIVATED:
        {
            switch (GetGoType())
            {
                case GAMEOBJECT_TYPE_DOOR:
                case GAMEOBJECT_TYPE_BUTTON:
                    if (GetGOInfo()->GetAutoCloseTime() && (m_cooldownTime < time(NULL)))
                        { ResetDoorOrButton(); }
                    break;
                case GAMEOBJECT_TYPE_GOOBER:
                    if (m_cooldownTime < time(NULL))
                    {
                        RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);

                        SetLootState(GO_JUST_DEACTIVATED);
                        m_cooldownTime = 0;
                    }
                    break;
                case GAMEOBJECT_TYPE_CAPTURE_POINT:
                    m_captureTimer += p_time;
                    if (m_captureTimer >= 5000)
                    {
                        TickCapturePoint();
                        m_captureTimer -= 5000;
                    }
                    break;
                default:
                    break;
            }
            break;
        }
        case GO_JUST_DEACTIVATED:
        {
            switch (GetGoType())
            {
                case GAMEOBJECT_TYPE_GOOBER:
                    // if gameobject should cast spell, then this, but some GOs (type = 10) should be destroyed
                    if (uint32 spellId = GetGOInfo()->goober.spellId)
                    {
                        for (GuidSet::const_iterator itr = m_UniqueUsers.begin(); itr != m_UniqueUsers.end(); ++itr)
                        {
                            if (Player* owner = GetMap()->GetPlayer(*itr))
                                { owner->CastSpell(owner, spellId, false, NULL, NULL, GetObjectGuid()); }
                        }

                        ClearAllUsesData();
                    }

                    SetGoState(GO_STATE_READY);

                    // any return here in case battleground traps
                    break;

                case GAMEOBJECT_TYPE_CAPTURE_POINT:
                    // remove capturing players because slider wont be displayed if capture point is being locked
                    for (GuidSet::const_iterator itr = m_UniqueUsers.begin(); itr != m_UniqueUsers.end(); ++itr)
                    {
                        if (Player* owner = GetMap()->GetPlayer(*itr))
                            { owner->SendUpdateWorldState(GetGOInfo()->capturePoint.worldState1, WORLD_STATE_REMOVE); }
                    }

                    m_UniqueUsers.clear();
                    SetLootState(GO_READY);
                    return; // SetLootState and return because go is treated as "burning flag" due to GetGoAnimProgress() being 100 and would be removed on the client
                case GAMEOBJECT_TYPE_CHEST:
                    {
                        uint32 trapEntry = GetGOInfo()->GetLinkedGameObjectEntry();
                        if (trapEntry == 144064) // Special case for Gordunni Cobalt Visual
                        {
                            float range = 0.5f;
                            GameObject* visualGO = NULL;

                            MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*this, 177683, range); //177683 Visual Entry
                            MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(visualGO, go_check);

                            Cell::VisitGridObjects(this, checker, range);

                            if (visualGO)
                                visualGO->SetLootState(GO_JUST_DEACTIVATED);
                        }
                        
                        if (!trapEntry)
                            break;
                        GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
                        if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
                            break;
                        
                        float range = 0.5f;
                        
                        GameObject* trapGO = NULL;

                        MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*this, trapEntry, range);
                        MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(trapGO, go_check);

                        Cell::VisitGridObjects(this, checker, range);
                        
                        // found correct GO
                        if (trapGO)
                            trapGO->SetLootState(GO_JUST_DEACTIVATED);
                    }

                default:
                    break;
            }

            // Remove wild summoned after use
            if (!HasStaticDBSpawnData() && (!GetSpellId() || GetGOInfo()->GetDespawnPossibility()))
            {
                if (Unit* owner = GetOwner())
                    { owner->RemoveGameObject(this, false); }
                Delete();
                return;
            }

            // burning flags in some battlegrounds, if you find better condition, just add it
            if (GetGOInfo()->IsDespawnAtAction() || GetGoAnimProgress() > 0)
            {
                SendObjectDeSpawnAnim(GetObjectGuid());
                // reset flags
                if (GetMap()->Instanceable())
                {
                    // In Instances GO_FLAG_LOCKED, GO_FLAG_INTERACT_COND or GO_FLAG_NO_INTERACT are not changed
                    uint32 currentLockOrInteractFlags = GetUInt32Value(GAMEOBJECT_FLAGS) & (GO_FLAG_LOCKED | GO_FLAG_INTERACT_COND | GO_FLAG_NO_INTERACT);
                    SetUInt32Value(GAMEOBJECT_FLAGS, (GetGOInfo()->flags & ~(GO_FLAG_LOCKED | GO_FLAG_INTERACT_COND | GO_FLAG_NO_INTERACT)) | currentLockOrInteractFlags);
                }
                else
                    { SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags); }
            }

            loot.clear();
            SetLootRecipient(NULL);
            SetLootState(GO_READY);

            if (!m_respawnDelayTime)
                { return; }

            // since pool system can fail to roll unspawned object, this one can remain spawned, so must set respawn nevertheless
            m_respawnTime = m_spawnedByDefault ? time(NULL) + m_respawnDelayTime : 0;

            // if option not set then object will be saved at grid unload
            if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
                { SaveRespawnTime(); }

            // if part of pool, let pool system schedule new spawn instead of just scheduling respawn
            if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
                { sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow()); }

            // can be not in world at pool despawn
            if (IsInWorld())
                { UpdateObjectVisibility(); }

            if (GetGoType() == GAMEOBJECT_TYPE_CHEST)
                { RollIfMineralVein(); }

            break;
        }
    }
}

void GameObject::Refresh()
{
    // not refresh despawned not casted GO (despawned casted GO destroyed in all cases anyway)
    if (m_respawnTime > 0 && m_spawnedByDefault)
        { return; }

    if (isSpawned())
        { GetMap()->Add(this); }
}

void GameObject::AddUniqueUse(Player* player)
{
    AddUse();

    if (!m_firstUser)
        { m_firstUser = player->GetObjectGuid(); }

    m_UniqueUsers.insert(player->GetObjectGuid());
}

void GameObject::Delete()
{
    SendObjectDeSpawnAnim(GetObjectGuid());

    SetGoState(GO_STATE_READY);
    SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);

    if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
        { sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow()); }
    else
        { AddObjectToRemoveList(); }
}

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

void GameObject::SaveToDB(uint32 mapid)
{
    const GameObjectInfo* goI = GetGOInfo();

    if (!goI)
        { return; }

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
    ss << "INSERT INTO gameobject VALUES ( "
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
    WorldDatabase.PExecuteLog("DELETE FROM gameobject WHERE guid = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("%s", ss.str().c_str());
    WorldDatabase.CommitTransaction();
}

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
        { return false; }

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
    WorldDatabase.PExecuteLog("DELETE FROM gameobject WHERE guid = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM game_event_gameobject WHERE guid = '%u'", GetGUIDLow());
    WorldDatabase.PExecuteLog("DELETE FROM gameobject_battleground WHERE guid = '%u'", GetGUIDLow());
}

/*********************************************************/
/***                    QUEST SYSTEM                   ***/
/*********************************************************/
bool GameObject::HasQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds = sObjectMgr.GetGOQuestRelationsMapBounds(GetEntry());
    for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
            { return true; }
    }
    return false;
}

bool GameObject::HasInvolvedQuest(uint32 quest_id) const
{
    QuestRelationsMapBounds bounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(GetEntry());
    for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
    {
        if (itr->second == quest_id)
            { return true; }
    }
    return false;
}

bool GameObject::IsTransport() const
{
    // If something is marked as a transport, don't transmit an out of range packet for it.
    GameObjectInfo const* gInfo = GetGOInfo();
    if (!gInfo) { return false; }
    return gInfo->type == GAMEOBJECT_TYPE_TRANSPORT || gInfo->type == GAMEOBJECT_TYPE_MO_TRANSPORT;
}

Unit* GameObject::GetOwner() const
{
    return sObjectAccessor.GetUnit(*this, GetOwnerGuid());
}

void GameObject::SaveRespawnTime()
{
    if (m_respawnTime > time(NULL) && m_spawnedByDefault)
        { GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), m_respawnTime); }
}

bool GameObject::IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    // Not in world
    if (!IsInWorld() || !u->IsInWorld())
        { return false; }

    // Transport always visible at this step implementation
    if (IsTransport() && IsInMap(u))
        { return true; }

    float visibleDistance = GetMap()->GetVisibilityDistance() + (inVisibleList ? World::GetVisibleObjectGreyDistance() : 0.0f);

    // quick check visibility false cases for non-GM-mode
    if (!u->isGameMaster())
    {
        // despawned and then not visible for non-GM in GM-mode
        if (!isSpawned())
            { return false; }

        // special invisibility cases
        // implementing armed trap stealth, basing on the spell 2836 structure
        if (GetGOInfo()->type == GAMEOBJECT_TYPE_TRAP && GetGOInfo()->trap.stealthed && GetGoState() == GO_STATE_READY)
        {
            Unit *owner = GetOwner();
            if (!owner || u->IsHostileTo(owner))
            {
                visibleDistance = 10.5f;
                //2^3=8 and 300 - from spell 2836, EFFECT_INDEX_1 - SPELL_AURA_MOD_INVISIBILITY_DETECTION; TODO check 200 and improve
                if (u->GetMaxPositiveAuraModifierByMiscValue(SPELL_AURA_MOD_INVISIBILITY_DETECTION, 8) < 200)
                {
                    if (u->getClass() != CLASS_ROGUE)
                        return false;       // a wild or enemy trap cannot be seen by non-rogues without proper invis detection
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
                    visibleDistance = MAX_PLAYER_STEALTH_DETECT_RANGE;
                else if (visibleDistance < GetGOInfo()->trap.radius + INTERACTION_DISTANCE)
                    visibleDistance = GetGOInfo()->trap.radius + INTERACTION_DISTANCE;
            }
        }

        // [-ZERO] Smuggled Mana Cell required 10 invisibility type detection/state
        //if (GetEntry() == 187039 && ((u->m_detectInvisibilityMask | u->m_invisibilityMask) & (1 << 10)) == 0)
        //    { return false; }
    }

    // check distance
    return IsWithinDistInMap(viewPoint, visibleDistance, false);
}

void GameObject::Respawn()
{
    if (m_spawnedByDefault && m_respawnTime > 0)
    {
        m_respawnTime = time(NULL);
        GetMap()->GetPersistentState()->SaveGORespawnTime(GetGUIDLow(), 0);
    }
}

bool GameObject::ActivateToQuest(Player* pTarget) const
{
    // if GO is ReqCreatureOrGoN for quest
    if (pTarget->HasQuestForGO(GetEntry()))
        { return true; }

    if (!sObjectMgr.IsGameObjectForQuests(GetEntry()))
        { return false; }

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
                    { return true; }
            }

            bounds = sObjectMgr.GetGOQuestInvolvedRelationsMapBounds(GetEntry());

            for (QuestRelationsMap::const_iterator itr = bounds.first; itr != bounds.second; ++itr)
            {
                if ((pTarget->GetQuestStatus(itr->second) == QUEST_STATUS_INCOMPLETE || pTarget->GetQuestStatus(itr->second) == QUEST_STATUS_COMPLETE)
                    && !pTarget->GetQuestRewardStatus(itr->second))
                    { return true; }
            }

            break;
        }
        // scan GO chest with loot including quest items
        case GAMEOBJECT_TYPE_CHEST:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->chest.questId) == QUEST_STATUS_INCOMPLETE)
                { return true; }

            if (LootTemplates_Gameobject.HaveQuestLootForPlayer(GetGOInfo()->GetLootId(), pTarget))
            {
                // look for battlegroundAV for some objects which are only activated after mine gots captured by own team
                if (GetEntry() == BG_AV_OBJECTID_MINE_N || GetEntry() == BG_AV_OBJECTID_MINE_S)
                    if (BattleGround* bg = pTarget->GetBattleGround())
                        if (bg->GetTypeID() == BATTLEGROUND_AV && !(((BattleGroundAV*)bg)->PlayerCanDoMineQuest(GetEntry(), pTarget->GetTeam())))
                            { return false; }
                return true;
            }
            break;
        }
        case GAMEOBJECT_TYPE_GENERIC:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->_generic.questID) == QUEST_STATUS_INCOMPLETE)
                { return true; }
            break;
        }
        case GAMEOBJECT_TYPE_SPELL_FOCUS:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->spellFocus.questID) == QUEST_STATUS_INCOMPLETE)
                { return true; }
            break;
        }
        case GAMEOBJECT_TYPE_GOOBER:
        {
            if (pTarget->GetQuestStatus(GetGOInfo()->goober.questId) == QUEST_STATUS_INCOMPLETE)
                { return true; }
            break;
        }
        default:
            break;
    }

    return false;
}

void GameObject::SummonLinkedTrapIfAny()
{
    uint32 linkedEntry = GetGOInfo()->GetLinkedGameObjectEntry();
    if (!linkedEntry)
        { return; }

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

    GetMap()->Add(linkedGO);
}

void GameObject::TriggerLinkedGameObject(Unit* target)
{
    uint32 trapEntry = GetGOInfo()->GetLinkedGameObjectEntry();

    if (!trapEntry)
        { return; }

    GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
    if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
        { return; }

    SpellEntry const* trapSpell = sSpellStore.LookupEntry(trapInfo->trap.spellId);

    // The range to search for linked trap is weird. We set 0.5 as default. Most (all?)
    // traps are probably expected to be pretty much at the same location as the used GO,
    // so it appears that using range from spell is obsolete.
    float range = 0.5f;

    if (trapSpell)                                          // checked at load already
        { range = GetSpellMaxRange(sSpellRangeStore.LookupEntry(trapSpell->rangeIndex)); }

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
        { trapGO->Use(target); }
}

GameObject* GameObject::LookupFishingHoleAround(float range)
{
    GameObject* ok = NULL;

    MaNGOS::NearestGameObjectFishingHoleCheck u_check(*this, range);
    MaNGOS::GameObjectSearcher<MaNGOS::NearestGameObjectFishingHoleCheck> checker(ok, u_check);
    Cell::VisitGridObjects(this, checker, range);

    return ok;
}

bool GameObject::IsCollisionEnabled() const
{
    if (!isSpawned())
        { return false; }

    // TODO: Possible that this function must consider multiple checks
    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:
            return GetGoState() != GO_STATE_ACTIVE && GetGoState() != GO_STATE_ACTIVE_ALTERNATIVE;

        default:
            return true;
    }
}

void GameObject::ResetDoorOrButton()
{
    if (m_lootState == GO_READY || m_lootState == GO_JUST_DEACTIVATED)
        { return; }

    SwitchDoorOrButton(false);
    SetLootState(GO_JUST_DEACTIVATED);
    m_cooldownTime = 0;
}

void GameObject::UseDoorOrButton(uint32 time_to_restore, bool alternative /* = false */)
{
    if (m_lootState != GO_READY)
        { return; }

    if (!time_to_restore)
        { time_to_restore = GetGOInfo()->GetAutoCloseTime(); }

    SwitchDoorOrButton(true, alternative);
    SetLootState(GO_ACTIVATED);

    m_cooldownTime = time(NULL) + time_to_restore;
}

void GameObject::SwitchDoorOrButton(bool activate, bool alternative /* = false */)
{
    if (activate)
        { SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE); }
    else
        { RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE); }

    if (GetGoState() == GO_STATE_READY)                     // if closed -> open
        { SetGoState(alternative ? GO_STATE_ACTIVE_ALTERNATIVE : GO_STATE_ACTIVE); }
    else                                                    // if open -> close
        { SetGoState(GO_STATE_READY); }
}

void GameObject::Use(Unit* user)
{
    // user must be provided
    MANGOS_ASSERT(user || PrintEntryError("GameObject::Use (without user)"));

    // by default spell caster is user
    Unit* spellCaster = user;
    uint32 spellId = 0;
    bool triggered = false;

    // test only for exist cooldown data (cooldown timer used for door/buttons reset that not have use cooldown)
    if (uint32 cooldown = GetGOInfo()->GetCooldown())
    {
        if (m_cooldownTime > sWorld.GetGameTime())
            { return; }

        m_cooldownTime = sWorld.GetGameTime() + cooldown;
    }

    bool scriptReturnValue = user->GetTypeId() == TYPEID_PLAYER && sScriptMgr.OnGameObjectUse((Player*)user, this);
    if (!scriptReturnValue)
        { GetMap()->ScriptsStart(DBS_ON_GOT_USE, GetEntry(), spellCaster, this); }

    switch (GetGoType())
    {
        case GAMEOBJECT_TYPE_DOOR:                          // 0
        {
            // doors never really despawn, only reset to default state/flags
            UseDoorOrButton();

            // activate script
            if (!scriptReturnValue)
                { GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), spellCaster, this); }
            return;
        }
        case GAMEOBJECT_TYPE_BUTTON:                        // 1
        {
            // buttons never really despawn, only reset to default state/flags
            UseDoorOrButton();

            TriggerLinkedGameObject(user);

            // activate script
            if (!scriptReturnValue)
                { GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), spellCaster, this); }

            return;
        }
        case GAMEOBJECT_TYPE_QUESTGIVER:                    // 2
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            if (!sScriptMgr.OnGossipHello(player, this))
            {
                player->PrepareGossipMenu(this, GetGOInfo()->questgiver.gossipID);
                player->SendPreparedGossip(this);
            }

            return;
        }
        case GAMEOBJECT_TYPE_CHEST:                         // 3
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            TriggerLinkedGameObject(user);

            // TODO: possible must be moved to loot release (in different from linked triggering)
            if (GetGOInfo()->chest.eventId)
            {
                DEBUG_LOG("Chest ScriptStart id %u for %s (opened by %s)", GetGOInfo()->chest.eventId, GetGuidStr().c_str(), user->GetGuidStr().c_str());
                StartEvents_Event(GetMap(), GetGOInfo()->chest.eventId, user, this);
            }

            return;
        }
        case GAMEOBJECT_TYPE_GENERIC:                       // 5
        {
            if (scriptReturnValue)
                { return; }

            // No known way to exclude some - only different approach is to select despawnable GOs by Entry
            SetLootState(GO_JUST_DEACTIVATED);
            return;
        }
        case GAMEOBJECT_TYPE_TRAP:                          // 6
        {
            if (scriptReturnValue)
                { return; }

            Unit* owner = GetOwner();
            Unit* caster = owner ? owner : user;

            GameObjectInfo const* goInfo = GetGOInfo();
            float radius = float(goInfo->trap.radius);
            bool IsBattleGroundTrap = !radius && goInfo->trap.cooldown == 3 && m_respawnTime == 0;

            // FIXME: when GO casting will be implemented trap must cast spell to target
            if (goInfo->trap.spellId)
                { caster->CastSpell(user, goInfo->trap.spellId, true, NULL, NULL, GetObjectGuid()); }
            // use template cooldown if provided
            m_cooldownTime = time(NULL) + (goInfo->trap.cooldown ? goInfo->trap.cooldown : uint32(4));

            // count charges
            if (goInfo->trap.charges > 0)
                { AddUse(); }

            if (IsBattleGroundTrap && user->GetTypeId() == TYPEID_PLAYER)
            {
                // BattleGround gameobjects case
                if (BattleGround* bg = ((Player*)user)->GetBattleGround())
                    { bg->HandleTriggerBuff(GetObjectGuid()); }
            }

            // TODO: all traps can be activated, also those without spell.
            // Some may have have animation and/or are expected to despawn.

            // TODO: Improve this when more information is available, currently these traps are known that must send the anim (Onyxia/ Heigan Fissures/ Trap in DireMaul)
            if (GetDisplayId() == 4392 || GetDisplayId() == 4472 || GetDisplayId() == 4491 || GetDisplayId() == 6785 || GetDisplayId() == 3073)
                { SendGameObjectCustomAnim(); }

            // TODO: Despawning of traps? (Also related to code in ::Update)
            return;
        }
        case GAMEOBJECT_TYPE_CHAIR:                         // 7 Sitting: Wooden bench, chairs
        {
            GameObjectInfo const* info = GetGOInfo();
            if (!info)
                { return; }

            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            // a chair may have n slots. we have to calculate their positions and teleport the player to the nearest one

            // check if the db is sane
            if (info->chair.slots > 0)
            {
                float lowestDist = DEFAULT_VISIBILITY_DISTANCE;

                float x_lowest = GetPositionX();
                float y_lowest = GetPositionY();

                // the object orientation + 1/2 pi
                // every slot will be on that straight line
                float orthogonalOrientation = GetOrientation() + M_PI_F * 0.5f;
                // find nearest slot
                for (uint32 i = 0; i < info->chair.slots; ++i)
                {
                    // the distance between this slot and the center of the go - imagine a 1D space
                    float relativeDistance = (info->size * i) - (info->size * (info->chair.slots - 1) / 2.0f);

                    float x_i = GetPositionX() + relativeDistance * cos(orthogonalOrientation);
                    float y_i = GetPositionY() + relativeDistance * sin(orthogonalOrientation);

                    // calculate the distance between the player and this slot
                    float thisDistance = player->GetDistance2d(x_i, y_i);

                    /* debug code. It will spawn a npc on each slot to visualize them.
                    Creature* helper = player->SummonCreature(14496, x_i, y_i, GetPositionZ(), GetOrientation(), TEMPSUMMON_TIMED_OR_DEAD_DESPAWN, 10000);
                    std::ostringstream output;
                    output << i << ": thisDist: " << thisDistance;
                    helper->MonsterSay(output.str().c_str(), LANG_UNIVERSAL);
                    */

                    if (thisDistance <= lowestDist)
                    {
                        lowestDist = thisDistance;
                        x_lowest = x_i;
                        y_lowest = y_i;
                    }
                }
                player->TeleportTo(GetMapId(), x_lowest, y_lowest, GetPositionZ(), GetOrientation(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
            }
            else
            {
                // fallback, will always work
                player->TeleportTo(GetMapId(), GetPositionX(), GetPositionY(), GetPositionZ(), GetOrientation(), TELE_TO_NOT_LEAVE_TRANSPORT | TELE_TO_NOT_LEAVE_COMBAT | TELE_TO_NOT_UNSUMMON_PET);
            }
            player->SetStandState(UNIT_STAND_STATE_SIT_LOW_CHAIR + info->chair.height);
            return;
        }
        case GAMEOBJECT_TYPE_SPELL_FOCUS:                   // 8
        {
            TriggerLinkedGameObject(user);

            // some may be activated in addition? Conditions for this? (ex: entry 181616)
            return;
        }
        case GAMEOBJECT_TYPE_GOOBER:                        // 10
        {
            // Handle OutdoorPvP use cases
            // Note: this may be also handled by DB spell scripts in the future, when the world state manager is implemented
            if (user->GetTypeId() == TYPEID_PLAYER)
            {
                Player* player = (Player*)user;
                if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript(player->GetCachedZoneId()))
                    { outdoorPvP->HandleGameObjectUse(player, this); }
            }

            GameObjectInfo const* info = GetGOInfo();

            TriggerLinkedGameObject(user);

            SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_IN_USE);
            SetLootState(GO_ACTIVATED);

            // this appear to be ok, however others exist in addition to this that should have custom (ex: 190510, 188692, 187389)
            if (info->goober.customAnim)
                { SendGameObjectCustomAnim(); }
            else
                { SetGoState(GO_STATE_ACTIVE); }

            m_cooldownTime = time(NULL) + info->GetAutoCloseTime();

            if (user->GetTypeId() == TYPEID_PLAYER)
            {
                Player* player = (Player*)user;

                if (info->goober.pageId)                    // show page...
                {
                    WorldPacket data(SMSG_GAMEOBJECT_PAGETEXT, 8);
                    data << ObjectGuid(GetObjectGuid());
                    player->GetSession()->SendPacket(&data);
                }
                else if (info->goober.gossipID)             // ...or gossip, if page does not exist
                {
                    if (!sScriptMgr.OnGossipHello(player, this))
                    {
                        player->PrepareGossipMenu(this, info->goober.gossipID);
                        player->SendPreparedGossip(this);
                    }
                }

                if (info->goober.eventId)
                {
                    DEBUG_FILTER_LOG(LOG_FILTER_AI_AND_MOVEGENSS, "Goober ScriptStart id %u for %s (Used by %s).", info->goober.eventId, GetGuidStr().c_str(), player->GetGuidStr().c_str());
                    StartEvents_Event(GetMap(), info->goober.eventId, player, this);
                }

                // possible quest objective for active quests
                if (info->goober.questId && sObjectMgr.GetQuestTemplate(info->goober.questId))
                {
                    // Quest require to be active for GO using
                    if (player->GetQuestStatus(info->goober.questId) != QUEST_STATUS_INCOMPLETE)
                        { break; }
                }

                player->RewardPlayerAndGroupAtCast(this);
            }

            // activate script
            if (!scriptReturnValue)
                { GetMap()->ScriptsStart(DBS_ON_GO_USE, GetGUIDLow(), spellCaster, this); }
            else
                { return; }

            // cast this spell later if provided
            spellId = info->goober.spellId;

            break;
        }
        case GAMEOBJECT_TYPE_CAMERA:                        // 13
        {
            GameObjectInfo const* info = GetGOInfo();
            if (!info)
                { return; }

            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            if (info->camera.cinematicId)
                { player->SendCinematicStart(info->camera.cinematicId); }

            if (info->camera.eventID)
                { StartEvents_Event(GetMap(), info->camera.eventID, player, this); }

            return;
        }
        case GAMEOBJECT_TYPE_FISHINGNODE:                   // 17 fishing bobber
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            if (player->GetObjectGuid() != GetOwnerGuid())
                { return; }

            switch (getLootState())
            {
                case GO_READY:                              // ready for loot
                {
                    // 1) skill must be >= base_zone_skill
                    // 2) if skill == base_zone_skill => 5% chance
                    // 3) chance is linear dependence from (base_zone_skill-skill)

                    uint32 zone, subzone;
                    GetZoneAndAreaId(zone, subzone);

                    int32 zone_skill = sObjectMgr.GetFishingBaseSkillLevel(subzone);
                    if (!zone_skill)
                        { zone_skill = sObjectMgr.GetFishingBaseSkillLevel(zone); }

                    // provide error, no fishable zone or area should be 0
                    if (!zone_skill)
                        { sLog.outErrorDb("Fishable areaId %u are not properly defined in `skill_fishing_base_level`.", subzone); }

                    int32 skill = player->GetSkillValue(SKILL_FISHING);
                    int32 chance = skill - zone_skill + 5;
                    int32 roll = irand(1, 100);

                    DEBUG_LOG("Fishing check (skill: %i zone min skill: %i chance %i roll: %i", skill, zone_skill, chance, roll);

                    // normal chance
                    bool success = skill >= zone_skill && chance >= roll;
                    GameObject* fishingHole = NULL;

                    // overwrite fail in case fishhole if allowed (after 3.3.0)
                    if (!success)
                    {
                        if (!sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_POSSIBLE_FISHINGPOOL))
                        {
                            // TODO: find reasonable value for fishing hole search
                            fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE);
                            if (fishingHole)
                                { success = true; }
                        }
                    }
                    // just search fishhole for success case
                    else
                        // TODO: find reasonable value for fishing hole search
                        { fishingHole = LookupFishingHoleAround(20.0f + CONTACT_DISTANCE); }

                    if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_GAIN_FISHING))
                        { player->UpdateFishingSkill(); }

                    // fish catch or fail and junk allowed (after 3.1.0)
                    if (success || sWorld.getConfig(CONFIG_BOOL_SKILL_FAIL_LOOT_FISHING))
                    {
                        // prevent removing GO at spell cancel
                        player->RemoveGameObject(this, false);
                        SetOwnerGuid(player->GetObjectGuid());

                        if (fishingHole)                    // will set at success only
                        {
                            fishingHole->Use(player);
                            SetLootState(GO_JUST_DEACTIVATED);
                        }
                        else
                            { player->SendLoot(GetObjectGuid(), success ? LOOT_FISHING : LOOT_FISHING_FAIL); }
                    }
                    else
                    {
                        // fish escaped, can be deleted now
                        SetLootState(GO_JUST_DEACTIVATED);

                        WorldPacket data(SMSG_FISH_ESCAPED, 0);
                        player->GetSession()->SendPacket(&data);
                    }
                    break;
                }
                case GO_JUST_DEACTIVATED:                   // nothing to do, will be deleted at next update
                    break;
                default:
                {
                    SetLootState(GO_JUST_DEACTIVATED);

                    WorldPacket data(SMSG_FISH_NOT_HOOKED, 0);
                    player->GetSession()->SendPacket(&data);
                    break;
                }
            }

            player->FinishSpell(CURRENT_CHANNELED_SPELL);
            return;
        }
        case GAMEOBJECT_TYPE_SUMMONING_RITUAL:              // 18
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            Unit* owner = GetOwner();

            GameObjectInfo const* info = GetGOInfo();

            if (owner)
            {
                if (owner->GetTypeId() != TYPEID_PLAYER)
                    { return; }

                // accept only use by player from same group as owner, excluding owner itself (unique use already added in spell effect)
                if (player == (Player*)owner || (info->summoningRitual.castersGrouped && !player->IsInSameRaidWith(((Player*)owner))))
                    { return; }

                // expect owner to already be channeling, so if not...
                if (!owner->GetCurrentSpell(CURRENT_CHANNELED_SPELL))
                    { return; }

                // in case summoning ritual caster is GO creator
                spellCaster = owner;
            }
            else
            {
                if (m_firstUser && player->GetObjectGuid() != m_firstUser && info->summoningRitual.castersGrouped)
                {
                    if (Group* group = player->GetGroup())
                    {
                        if (!group->IsMember(m_firstUser))
                            { return; }
                    }
                    else
                        { return; }
                }

                spellCaster = player;
            }

            AddUniqueUse(player);

            if (info->summoningRitual.animSpell)
            {
                player->CastSpell(player, info->summoningRitual.animSpell, true);

                // for this case, summoningRitual.spellId is always triggered
                triggered = true;
            }

            // full amount unique participants including original summoner, need more
            if (GetUniqueUseCount() < info->summoningRitual.reqParticipants)
                { return; }

            // owner is first user for non-wild GO objects, if it offline value already set to current user
            if (!GetOwnerGuid())
                if (Player* firstUser = GetMap()->GetPlayer(m_firstUser))
                    { spellCaster = firstUser; }

            spellId = info->summoningRitual.spellId;

            // spell have reagent and mana cost but it not expected use its
            // it triggered spell in fact casted at currently channeled GO
            triggered = true;

            // finish owners spell
            if (owner)
                { owner->FinishSpell(CURRENT_CHANNELED_SPELL); }

            // can be deleted now, if
            if (!info->summoningRitual.ritualPersistent)
                { SetLootState(GO_JUST_DEACTIVATED); }
            // reset ritual for this GO
            else
                { ClearAllUsesData(); }

            // go to end function to spell casting
            break;
        }
        case GAMEOBJECT_TYPE_SPELLCASTER:                   // 22
        {
            SetUInt32Value(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);

            GameObjectInfo const* info = GetGOInfo();
            if (!info)
                { return; }

            if (info->spellcaster.partyOnly)
            {
                Unit* caster = GetOwner();
                if (!caster || caster->GetTypeId() != TYPEID_PLAYER)
                    { return; }

                if (user->GetTypeId() != TYPEID_PLAYER || !((Player*)user)->IsInSameRaidWith((Player*)caster))
                    { return; }
            }

            spellId = info->spellcaster.spellId;

            AddUse();
            break;
        }
        case GAMEOBJECT_TYPE_FLAGSTAND:                     // 24
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            if (player->CanUseBattleGroundObject())
            {
                // in battleground check
                BattleGround* bg = player->GetBattleGround();
                if (!bg)
                    { return; }
                // BG flag click
                // AB:
                // 15001
                // 15002
                // 15003
                // 15004
                // 15005
                bg->EventPlayerClickedOnFlag(player, this);
                return;                                     // we don't need to delete flag ... it is despawned!
            }
            break;
        }
        case GAMEOBJECT_TYPE_FISHINGHOLE:                   // 25
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            player->SendLoot(GetObjectGuid(), LOOT_FISHINGHOLE);
            return;
        }
        case GAMEOBJECT_TYPE_FLAGDROP:                      // 26
        {
            if (user->GetTypeId() != TYPEID_PLAYER)
                { return; }

            Player* player = (Player*)user;

            if (player->CanUseBattleGroundObject())
            {
                // in battleground check
                BattleGround* bg = player->GetBattleGround();
                if (!bg)
                    { return; }
                // BG flag dropped
                // WS:
                // 179785 - Silverwing Flag
                // 179786 - Warsong Flag
                GameObjectInfo const* info = GetGOInfo();
                if (info)
                {
                    switch (info->id)
                    {
                        case 179785:                        // Silverwing Flag
                        case 179786:                        // Warsong Flag
                            // check if it's correct bg
                            if (bg->GetTypeID() == BATTLEGROUND_WS)
                                { bg->EventPlayerClickedOnFlag(player, this); }
                            break;
                    }
                }
                // this cause to call return, all flags must be deleted here!!
                spellId = 0;
                Delete();
            }
            break;
        }
        default:
            sLog.outError("GameObject::Use unhandled GameObject type %u (entry %u).", GetGoType(), GetEntry());
            return;
    }

    if (!spellId)
        { return; }

    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);
    if (!spellInfo)
    {
        sLog.outError("WORLD: unknown spell id %u at use action for gameobject (Entry: %u GoType: %u )", spellId, GetEntry(), GetGoType());
        return;
    }

    Spell* spell = new Spell(spellCaster, spellInfo, triggered, GetObjectGuid());

    // spell target is user of GO
    SpellCastTargets targets;
    targets.setUnitTarget(user);

    spell->prepare(&targets);
}

// overwrite WorldObject function for proper name localization
const char* GameObject::GetNameForLocaleIdx(int32 loc_idx) const
{
    if (loc_idx >= 0)
    {
        GameObjectLocale const* cl = sObjectMgr.GetGameObjectLocale(GetEntry());
        if (cl)
        {
            if (cl->Name.size() > (size_t)loc_idx && !cl->Name[loc_idx].empty())
                { return cl->Name[loc_idx].c_str(); }
        }
    }

    return GetName();
}

void GameObject::SetQuaternion(G3D::Quat const& q)
{
    SetFloatValue(GAMEOBJECT_ROTATION + 0, q.x);
    SetFloatValue(GAMEOBJECT_ROTATION + 1, q.y);
    SetFloatValue(GAMEOBJECT_ROTATION + 2, q.z);
    SetFloatValue(GAMEOBJECT_ROTATION + 3, q.w);

    if (m_model)
      { m_model->UpdateRotation(q); }
}

void GameObject::GetQuaternion(G3D::Quat& q) const
{
    q.x = GetFloatValue(GAMEOBJECT_ROTATION + 0);
    q.y = GetFloatValue(GAMEOBJECT_ROTATION + 1);
    q.z = GetFloatValue(GAMEOBJECT_ROTATION + 2);
    q.w = GetFloatValue(GAMEOBJECT_ROTATION + 3);
}

float GameObject::GetOrientationFromQuat(G3D::Quat const& q)
{
    double t1 = +2.0f * (q.w * q.z + q.x * q.y);
    double t2 = +1.0f - 2.0f * (q.y * q.y + q.z * q.z);
    return MapManager::NormalizeOrientation(std::atan2(t1, t2));
}

bool GameObject::IsHostileTo(Unit const* unit) const
{
    // always non-hostile to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER && ((Player const*)unit)->isGameMaster())
        { return false; }

    // test owner instead if have
    if (Unit const* owner = GetOwner())
        { return owner->IsHostileTo(unit); }

    if (Unit const* targetOwner = unit->GetCharmerOrOwner())
        { return IsHostileTo(targetOwner); }

    // for not set faction case: be hostile towards player, not hostile towards not-players
    if (!GetGOInfo()->faction)
        { return unit->IsControlledByPlayer(); }

    // faction base cases
    FactionTemplateEntry const* tester_faction = sFactionTemplateStore.LookupEntry(GetGOInfo()->faction);
    FactionTemplateEntry const* target_faction = unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
        { return false; }

    // GvP forced reaction and reputation case
    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)unit)->GetReputationMgr().GetForcedRankIfAny(tester_faction))
                { return *force <= REP_HOSTILE; }

            // apply reputation state
            FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->faction);
            if (raw_tester_faction && raw_tester_faction->reputationListID >= 0)
                { return ((Player const*)unit)->GetReputationMgr().GetRank(raw_tester_faction) <= REP_HOSTILE; }
        }
    }

    // common faction based case (GvC,GvP)
    return tester_faction->IsHostileTo(*target_faction);
}

bool GameObject::IsFriendlyTo(Unit const* unit) const
{
    // always friendly to GM in GM mode
    if (unit->GetTypeId() == TYPEID_PLAYER && ((Player const*)unit)->isGameMaster())
        { return true; }

    // test owner instead if have
    if (Unit const* owner = GetOwner())
        { return owner->IsFriendlyTo(unit); }

    if (Unit const* targetOwner = unit->GetCharmerOrOwner())
        { return IsFriendlyTo(targetOwner); }

    // for not set faction case (wild object) use hostile case
    if (!GetGOInfo()->faction)
        { return false; }

    // faction base cases
    FactionTemplateEntry const* tester_faction = sFactionTemplateStore.LookupEntry(GetGOInfo()->faction);
    FactionTemplateEntry const* target_faction = unit->getFactionTemplateEntry();
    if (!tester_faction || !target_faction)
        { return false; }

    // GvP forced reaction and reputation case
    if (unit->GetTypeId() == TYPEID_PLAYER)
    {
        if (tester_faction->faction)
        {
            // forced reaction
            if (ReputationRank const* force = ((Player*)unit)->GetReputationMgr().GetForcedRankIfAny(tester_faction))
                { return *force >= REP_FRIENDLY; }

            // apply reputation state
            if (FactionEntry const* raw_tester_faction = sFactionStore.LookupEntry(tester_faction->faction))
                if (raw_tester_faction->reputationListID >= 0)
                    { return ((Player const*)unit)->GetReputationMgr().GetRank(raw_tester_faction) >= REP_FRIENDLY; }
        }
    }

    // common faction based case (GvC,GvP)
    return tester_faction->IsFriendlyTo(*target_faction);
}

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

uint32 GameObject::RollMineralVein(uint32 entry)      //Maybe incedicite bloodstone and indurium have alternate spawns?
{
    uint32 entrynew = entry;

    if ((GetZoneId() == 46) || (GetZoneId() == 51)) // each node in searing gorge or burning steppes is able to spawn dark iron
        {
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_DARKIRON))
            entrynew = 165658;
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
                entrynew = 1733;
                break;
        case 1735: // Iron can spawn Gold
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 1734;
                break;
        case 73939: // Ooze covered iron can spawn ooze covered gold
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 73941;
                break;
        case 2040: // Mithril can spawn Truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 2047; 
                break;
        case 123310: // Ooze covered mithril can spawn ooze covered truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 123309;
                break;
        case 324: // small thorium Vein can spawn Truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 2047;
                break;
        case 123848: // ooze covered thorium Vein can spawn ooze covered truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 123309;
                break;
        case 175404: // Rich thorium Vein can spawn truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 2047;
                break;
        case 177388: // ooze covered Rich thorium Vein can spawn ooze covered truesilver
            if (urand (0, 100) < sWorld.getConfig(CONFIG_UINT32_RATE_MINING_RARE))
                entrynew = 123309;
                break;

        default: //default case for copper or not listet special veins
            entrynew = entry;
    }   
        return entrynew;
}

void GameObject::SetLootState(LootState state)
{
    m_lootState = state;
#ifdef ENABLE_ELUNA
    sEluna->OnLootStateChanged(this, state);
#endif /* ENABLE_ELUNA */
    UpdateCollisionState();
}

void GameObject::SetGoState(GOState state)
{
    SetByteValue(GAMEOBJECT_STATE, 0, state);
#ifdef ENABLE_ELUNA
    sEluna->OnGameObjectStateChanged(this, state);
#endif /* ENABLE_ELUNA */
    UpdateCollisionState();
}

void GameObject::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(GAMEOBJECT_DISPLAYID, modelId);
    UpdateModel();
}

void GameObject::UpdateCollisionState() const
{
    if (!m_model || !IsInWorld())
        { return; }

    m_model->SetCollidable(IsCollisionEnabled());
}

void GameObject::UpdateModel()
{
    if (m_model && IsInWorld() && GetMap()->ContainsGameObjectModel(*m_model))
        { GetMap()->RemoveGameObjectModel(*m_model); }
    delete m_model;

    m_model = GameObjectModel::Create(this);
    if (m_model)
        { GetMap()->InsertGameObjectModel(*m_model); }
}

void GameObject::StartGroupLoot(Group* group, uint32 timer)
{
    m_groupLootId = group->GetId();
    m_groupLootTimer = timer;
}

void GameObject::StopGroupLoot()
{
    if (!m_groupLootId)
        { return; }

    if (Group* group = sObjectMgr.GetGroupById(m_groupLootId))
        { group->EndRoll(); }

    m_groupLootTimer = 0;
    m_groupLootId = 0;
}

Player* GameObject::GetOriginalLootRecipient() const
{
    return m_lootRecipientGuid ? sObjectAccessor.FindPlayer(m_lootRecipientGuid) : NULL;
}

Group* GameObject::GetGroupLootRecipient() const
{
    // original recipient group if set and not disbanded
    return m_lootGroupRecipientId ? sObjectMgr.GetGroupById(m_lootGroupRecipientId) : NULL;
}

Player* GameObject::GetLootRecipient() const
{
    // original recipient group if set and not disbanded
    Group* group = GetGroupLootRecipient();

    // original recipient player if online
    Player* player = GetOriginalLootRecipient();

    // if group not set or disbanded return original recipient player if any
    if (!group)
        { return player; }

    // group case

    // return player if it still be in original recipient group
    if (player && player->GetGroup() == group)
        { return player; }

    // find any in group
    for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
        if (Player* newPlayer = itr->getSource())
            { return newPlayer; }

    return NULL;
}

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
        { return; }

    // set player for non group case or if group will disbanded
    m_lootRecipientGuid = player->GetObjectGuid();

    // set group for group existed case including if player will leave group at loot time
    if (Group* group = player->GetGroup())
        { m_lootGroupRecipientId = group->GetId(); }
}

float GameObject::GetObjectBoundingRadius() const
{
    // 1.12.1 GameObjectDisplayInfo.dbc not have any info related to size
    return DEFAULT_WORLD_OBJECT_SIZE;
}

bool GameObject::IsInSkillupList(Player* player) const
{
    return m_SkillupSet.find(player->GetObjectGuid()) != m_SkillupSet.end();
}

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
            { pGameobject->AddObjectToRemoveList(); }
    }

    ObjectGuid i_guid;
};

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
        if (map->IsLoaded(i_data->posX, i_data->posY))
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
                    { map->Add(pGameobject); }
            }
        }
    }

    uint32 i_guid;
    GameObjectData const* i_data;
};

void GameObject::SpawnInMaps(uint32 db_guid, GameObjectData const* data)
{
    SpawnGameObjectInMapsWorker worker(db_guid, data);
    sMapMgr.DoForAllMapsWithMapId(data->mapid, worker);
}

bool GameObject::HasStaticDBSpawnData() const
{
    return sObjectMgr.GetGOData(GetGUIDLow()) != NULL;
}

void GameObject::SetCapturePointSlider(float value, bool isLocked)
{
    GameObjectInfo const* info = GetGOInfo();

    m_captureSlider = value;

    // only activate non-locked capture point
    if (!isLocked)
        { SetLootState(GO_ACTIVATED); }

    // set the state of the capture point based on the slider value
    if ((int)m_captureSlider == CAPTURE_SLIDER_ALLIANCE)
        { m_captureState = CAPTURE_STATE_WIN_ALLIANCE; }
    else if ((int)m_captureSlider == CAPTURE_SLIDER_HORDE)
        { m_captureState = CAPTURE_STATE_WIN_HORDE; }
    else if (m_captureSlider > CAPTURE_SLIDER_MIDDLE + info->capturePoint.neutralPercent * 0.5f)
        { m_captureState = CAPTURE_STATE_PROGRESS_ALLIANCE; }
    else if (m_captureSlider < CAPTURE_SLIDER_MIDDLE - info->capturePoint.neutralPercent * 0.5f)
        { m_captureState = CAPTURE_STATE_PROGRESS_HORDE; }
    else
        { m_captureState = CAPTURE_STATE_NEUTRAL; }
}

void GameObject::TickCapturePoint()
{
    // TODO: On retail: Ticks every 5.2 seconds. slider value increase when new player enters on tick

    GameObjectInfo const* info = GetGOInfo();
    float radius = info->capturePoint.radius;

    // search for players in radius
    std::list<Player*> capturingPlayers;
    MaNGOS::AnyPlayerInCapturePointRange u_check(this, radius);
    MaNGOS::PlayerListSearcher<MaNGOS::AnyPlayerInCapturePointRange> checker(capturingPlayers, u_check);
    Cell::VisitWorldObjects(this, checker, radius);

    GuidSet tempUsers(m_UniqueUsers);
    uint32 neutralPercent = info->capturePoint.neutralPercent;
    int oldValue = m_captureSlider;
    int rangePlayers = 0;

    for (std::list<Player*>::iterator itr = capturingPlayers.begin(); itr != capturingPlayers.end(); ++itr)
    {
        if ((*itr)->GetTeam() == ALLIANCE)
            { ++rangePlayers; }
        else
            { --rangePlayers; }

        ObjectGuid guid = (*itr)->GetObjectGuid();
        if (!tempUsers.erase(guid))
        {
            // new player entered capture point zone
            m_UniqueUsers.insert(guid);

            // send capture point enter packets
            (*itr)->SendUpdateWorldState(info->capturePoint.worldState3, neutralPercent);
            (*itr)->SendUpdateWorldState(info->capturePoint.worldState2, oldValue);
            (*itr)->SendUpdateWorldState(info->capturePoint.worldState1, WORLD_STATE_ADD);
            (*itr)->SendUpdateWorldState(info->capturePoint.worldState2, oldValue); // also redundantly sent on retail to prevent displaying the initial capture direction on client capture slider incorrectly
        }
    }

    for (GuidSet::iterator itr = tempUsers.begin(); itr != tempUsers.end(); ++itr)
    {
        // send capture point leave packet
        if (Player* owner = GetMap()->GetPlayer(*itr))
            { owner->SendUpdateWorldState(info->capturePoint.worldState1, WORLD_STATE_REMOVE); }

        // player left capture point zone
        m_UniqueUsers.erase(*itr);
    }

    // return if there are not enough players capturing the point (works because minSuperiority is always 1)
    if (rangePlayers == 0)
    {
        // set to inactive if all players left capture point zone
        if (m_UniqueUsers.empty())
            { SetActiveObjectState(false); }
        return;
    }

    // prevents unloading gameobject before all players left capture point zone (to prevent m_UniqueUsers not being cleared if grid is set to idle)
    SetActiveObjectState(true);

    // cap speed
    int maxSuperiority = info->capturePoint.maxSuperiority;
    if (rangePlayers > maxSuperiority)
        { rangePlayers = maxSuperiority; }
    else if (rangePlayers < -maxSuperiority)
        { rangePlayers = -maxSuperiority; }

    // time to capture from 0% to 100% is maxTime for minSuperiority amount of players and minTime for maxSuperiority amount of players (linear function: y = dy/dx*x+d)
    float deltaSlider = info->capturePoint.minTime;

    if (int deltaSuperiority = maxSuperiority - info->capturePoint.minSuperiority)
        { deltaSlider += (float)(maxSuperiority - abs(rangePlayers)) / deltaSuperiority * (info->capturePoint.maxTime - info->capturePoint.minTime); }

    // calculate changed slider value for a duration of 5 seconds (5 * 100%)
    deltaSlider = 500.0f / deltaSlider;

    Team progressFaction;
    if (rangePlayers > 0)
    {
        progressFaction = ALLIANCE;
        m_captureSlider += deltaSlider;
        if (m_captureSlider > CAPTURE_SLIDER_ALLIANCE)
            { m_captureSlider = CAPTURE_SLIDER_ALLIANCE; }
    }
    else
    {
        progressFaction = HORDE;
        m_captureSlider -= deltaSlider;
        if (m_captureSlider < CAPTURE_SLIDER_HORDE)
            { m_captureSlider = CAPTURE_SLIDER_HORDE; }
    }

    // return if slider did not move a whole percent
    if ((int)m_captureSlider == oldValue)
        { return; }

    // on retail this is also sent to newly added players even though they already received a slider value
    for (std::list<Player*>::iterator itr = capturingPlayers.begin(); itr != capturingPlayers.end(); ++itr)
        { (*itr)->SendUpdateWorldState(info->capturePoint.worldState2, (uint32)m_captureSlider); }

    // send capture point events
    uint32 eventId = 0;

    /* WIN EVENTS */
    // alliance wins tower with max points
    if (m_captureState != CAPTURE_STATE_WIN_ALLIANCE && (int)m_captureSlider == CAPTURE_SLIDER_ALLIANCE)
    {
        eventId = info->capturePoint.winEventID1;
        m_captureState = CAPTURE_STATE_WIN_ALLIANCE;
    }
    // horde wins tower with max points
    else if (m_captureState != CAPTURE_STATE_WIN_HORDE && (int)m_captureSlider == CAPTURE_SLIDER_HORDE)
    {
        eventId = info->capturePoint.winEventID2;
        m_captureState = CAPTURE_STATE_WIN_HORDE;
    }

    /* PROGRESS EVENTS */
    // alliance takes the tower from neutral, contested or horde (if there is no neutral area) to alliance
    else if (m_captureState != CAPTURE_STATE_PROGRESS_ALLIANCE && m_captureSlider > CAPTURE_SLIDER_MIDDLE + neutralPercent * 0.5f && progressFaction == ALLIANCE)
    {
        eventId = info->capturePoint.progressEventID1;

        // handle objective complete
        if (m_captureState == CAPTURE_STATE_NEUTRAL)
            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript((*capturingPlayers.begin())->GetCachedZoneId()))
                { outdoorPvP->HandleObjectiveComplete(eventId, capturingPlayers, progressFaction); }

        // set capture state to alliance
        m_captureState = CAPTURE_STATE_PROGRESS_ALLIANCE;
    }
    // horde takes the tower from neutral, contested or alliance (if there is no neutral area) to horde
    else if (m_captureState != CAPTURE_STATE_PROGRESS_HORDE && m_captureSlider < CAPTURE_SLIDER_MIDDLE - neutralPercent * 0.5f && progressFaction == HORDE)
    {
        eventId = info->capturePoint.progressEventID2;

        // handle objective complete
        if (m_captureState == CAPTURE_STATE_NEUTRAL)
            if (OutdoorPvP* outdoorPvP = sOutdoorPvPMgr.GetScript((*capturingPlayers.begin())->GetCachedZoneId()))
                { outdoorPvP->HandleObjectiveComplete(eventId, capturingPlayers, progressFaction); }

        // set capture state to horde
        m_captureState = CAPTURE_STATE_PROGRESS_HORDE;
    }

    /* NEUTRAL EVENTS */
    // alliance takes the tower from horde to neutral
    else if (m_captureState != CAPTURE_STATE_NEUTRAL && m_captureSlider >= CAPTURE_SLIDER_MIDDLE - neutralPercent * 0.5f && m_captureSlider <= CAPTURE_SLIDER_MIDDLE + neutralPercent * 0.5f && progressFaction == ALLIANCE)
    {
        eventId = info->capturePoint.neutralEventID1;
        m_captureState = CAPTURE_STATE_NEUTRAL;
    }
    // horde takes the tower from alliance to neutral
    else if (m_captureState != CAPTURE_STATE_NEUTRAL && m_captureSlider >= CAPTURE_SLIDER_MIDDLE - neutralPercent * 0.5f && m_captureSlider <= CAPTURE_SLIDER_MIDDLE + neutralPercent * 0.5f && progressFaction == HORDE)
    {
        eventId = info->capturePoint.neutralEventID2;
        m_captureState = CAPTURE_STATE_NEUTRAL;
    }

    /* CONTESTED EVENTS */
    // alliance attacks tower which is in control or progress by horde (except if alliance also gains control in that case)
    else if ((m_captureState == CAPTURE_STATE_WIN_HORDE || m_captureState == CAPTURE_STATE_PROGRESS_HORDE) && progressFaction == ALLIANCE)
    {
        eventId = info->capturePoint.contestedEventID1;
        m_captureState = CAPTURE_STATE_CONTEST_HORDE;
    }
    // horde attacks tower which is in control or progress by alliance (except if horde also gains control in that case)
    else if ((m_captureState == CAPTURE_STATE_WIN_ALLIANCE || m_captureState == CAPTURE_STATE_PROGRESS_ALLIANCE) && progressFaction == HORDE)
    {
        eventId = info->capturePoint.contestedEventID2;
        m_captureState = CAPTURE_STATE_CONTEST_ALLIANCE;
    }

    if (eventId)
        { StartEvents_Event(GetMap(), eventId, this, this, true, *capturingPlayers.begin()); }
}

uint32 GameObject::GetScriptId()
{
    return sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, -int32(GetGUIDLow())) ? sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, -int32(GetGUIDLow())) : sScriptMgr.GetBoundScriptId(SCRIPTED_GAMEOBJECT, GetEntry());
}

float GameObject::GetInteractionDistance() const
{
    float maxdist = INTERACTION_DISTANCE;
    switch (GetGoType())
    {
        // TODO: find out how the client calculates the maximal usage distance to spellless working
        // gameobjects like mailboxes - 10.0 is a just an abitrary choosen number
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

void GameObject::SendGameObjectCustomAnim(uint32 animId /*= 0*/)
{
    WorldPacket data(SMSG_GAMEOBJECT_CUSTOM_ANIM, 8 + 4);
    data << GetObjectGuid();
    data << uint32(animId);
    SendMessageToSet(&data, true);
}

void GameObject::SendGameObjectReset()
{
    WorldPacket data(SMSG_GAMEOBJECT_RESET_STATE, 8);
    data << GetObjectGuid();
    SendMessageToSet(&data, true);
}
