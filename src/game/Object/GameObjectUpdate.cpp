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
#include "QuestDef.h"
#include "ObjectMgr.h"
#include "PoolManager.h"
#include "SpellMgr.h"
#include "Spell.h"
#include "UpdateMask.h"
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
#include "G3D/Quat.h"
#ifdef ENABLE_ELUNA
#include "LuaEngine.h"
#endif /* ENABLE_ELUNA */

enum
{
    GO_DIRE_MAUL_FIXED_TRAP = 179512,
    NPC_SLIPKIK_GUARD = 14323
};

/**
 * @brief Updates game object state, timers, loot state, and AI.
 *
 * @param update_diff The elapsed AI update time in milliseconds.
 * @param p_time The elapsed world time step in milliseconds.
 */
void GameObject::Update(uint32 update_diff, uint32 p_time)
{
    if (GetObjectGuid().IsMOTransport())
    {
        //((Transport*)this)->Update(p_time);
        return;
    }

    // Used by Eluna
#ifdef ENABLE_ELUNA
    if (Eluna* e = GetEluna())
    {
        e->UpdateAI(this, update_diff);
    }
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
                    {
                        m_cooldownTime = time(NULL) + GetGOInfo()->trap.startDelay;
                    }
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
                case GAMEOBJECT_TYPE_CHEST:
                {
                    if (m_goInfo->chest.chestRestockTime)
                    {
                        if (m_reStockTimer != 0)
                        {
                            if (m_reStockTimer <= time(nullptr))
                            {
                                m_reStockTimer = 0;
                                m_lootState = GO_READY;
                                loot.clear();
                                ForceValuesUpdateAtIndex(GAMEOBJECT_DYN_FLAGS);
                            }
                        }
                        else
                        {
                            m_lootState = GO_READY;
                        }
                        return;
                    }
                    m_lootState = GO_READY;
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
                            {
                                ResetDoorOrButton();
                            }
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
                                    {
                                        owner->RemoveGameObject(this, false);
                                    }
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

                uint32 max_charges = goInfo->GetCharges();

                if (goInfo->type == GAMEOBJECT_TYPE_TRAP)   // traps
                {
                    if (m_cooldownTime >= time(NULL))
                    {
                        return;
                    }

                    // FIXME: this is activation radius (in different casting radius that must be selected from spell data)
                    // TODO: move activated state code (cast itself) to GO_ACTIVATED, in this place only check activating and set state
                    float radius = float(goInfo->trap.radius);
                    if (!radius)
                    {
                        if (goInfo->trap.cooldown != 3)     // cast in other case (at some triggering/linked go/etc explicit call)
                        {
                            return;
                        }
                        else
                        {
                            if (m_respawnTime > 0)
                            {
                                break;
                            }

                            // battlegrounds gameobjects has data2 == 0 && data5 == 3
                            radius = float(goInfo->trap.cooldown);
                        }
                    }

                    SpellEntry const* se = sSpellStore.LookupEntry(goInfo->trap.spellId);
                    if (IsAreaOfEffectSpell(se))
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
                        {
                            bool useTrap = true;
                            // prevent use if GO entry is "Fixed Trap" and target is not SLIKIK
                            if (GetEntry() == GO_DIRE_MAUL_FIXED_TRAP && targetUnit->GetEntry() != NPC_SLIPKIK_GUARD)
                            {
                                useTrap = false;
                            }

                            if (useTrap)
                            {
                                Use(targetUnit);
                            }
                        }
                    }
                }

                // Only despawn object if there are charges to "consume"
                // it means (all GO with charges = 0 in DB should never be despawned)
                // Check : https://www.getmangos.eu/wiki/referenceinfo/dbinfo/mangosdb/mangoszeroworlddb/gameobject_template-r1047
                // for more information about charges field in db depending on object type
                if (max_charges > 0 && m_useTimes >= max_charges)
                {
                    m_useTimes = 0;
                    SetLootState(GO_JUST_DEACTIVATED);  // can be despawned or destroyed
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
                    {
                        ResetDoorOrButton();
                    }
                    break;
                case GAMEOBJECT_TYPE_CHEST:
                    if (true)
                    {
                        if (!loot.empty())
                        {
                            m_despawnTimer = time(nullptr) + 5 * MINUTE; // TODO:: need to add a define?
                        }
                        else if (m_despawnTimer != 0 && m_despawnTimer <= time(nullptr))
                        {
                            m_lootState = GO_JUST_DEACTIVATED;
                        }

                        // TODO : Missing Loot::Update() method found in CMangos
                    }
                    break;
                case GAMEOBJECT_TYPE_TRAP:
                    if (m_rearmTimer == 0)
                    {
                        m_rearmTimer = time(nullptr) + GetRespawnDelay();
                        SetGoState(GO_STATE_ACTIVE_ALTERNATIVE);
                    }

                    if (m_rearmTimer < time(nullptr))
                    {
                        SetGoState(GO_STATE_READY);
                        m_lootState = GO_READY;
                        m_rearmTimer = 0;
                    }
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
                            {
                                owner->CastSpell(owner, spellId, false, NULL, NULL, GetObjectGuid());
                            }
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
                        {
                            owner->SendUpdateWorldState(GetGOInfo()->capturePoint.worldState1, WORLD_STATE_REMOVE);
                        }
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
                        {
                            visualGO->SetLootState(GO_JUST_DEACTIVATED);
                        }
                    }

                    if (!trapEntry)
                    {
                        break;
                    }

                    GameObjectInfo const* trapInfo = sGOStorage.LookupEntry<GameObjectInfo>(trapEntry);
                    if (!trapInfo || trapInfo->type != GAMEOBJECT_TYPE_TRAP)
                    {
                        break;
                    }

                    float range = 0.5f;

                    GameObject* trapGO = NULL;

                    MaNGOS::NearestGameObjectEntryInObjectRangeCheck go_check(*this, trapEntry, range);
                    MaNGOS::GameObjectLastSearcher<MaNGOS::NearestGameObjectEntryInObjectRangeCheck> checker(trapGO, go_check);

                    Cell::VisitGridObjects(this, checker, range);

                    // found correct GO
                    if (trapGO)
                    {
                        trapGO->SetLootState(GO_JUST_DEACTIVATED);
                    }
                }

                default:
                    break;
            }

            // Remove wild summoned after use
            if (!HasStaticDBSpawnData() && (!GetSpellId() || GetGOInfo()->GetDespawnPossibility() || GetGOInfo()->IsDespawnAtAction()))
            {
                if (Unit* owner = GetOwner())
                {
                    owner->RemoveGameObject(this, false);
                }
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
                {
                    SetUInt32Value(GAMEOBJECT_FLAGS, GetGOInfo()->flags);
                }
            }

            loot.clear();
            SetLootRecipient(NULL);
            SetLootState(GO_READY);

            if (!m_respawnDelayTime)
            {
                return;
            }

            // since pool system can fail to roll unspawned object, this one can remain spawned, so must set respawn nevertheless
            m_respawnTime = m_spawnedByDefault ? time(NULL) + m_respawnDelayTime : 0;

            // if option not set then object will be saved at grid unload
            if (sWorld.getConfig(CONFIG_BOOL_SAVE_RESPAWN_TIME_IMMEDIATELY))
            {
                SaveRespawnTime();
            }

            // if part of pool, let pool system schedule new spawn instead of just scheduling respawn
            if (uint16 poolid = sPoolMgr.IsPartOfAPool<GameObject>(GetGUIDLow()))
            {
                sPoolMgr.UpdatePool<GameObject>(*GetMap()->GetPersistentState(), poolid, GetGUIDLow());
            }

            // can be not in world at pool despawn
            if (IsInWorld())
            {
                UpdateObjectVisibility();
            }

            if (GetGoType() == GAMEOBJECT_TYPE_CHEST)
            {
                RollIfMineralVein();
            }

            break;
        }
    }

    if (AI())
    {
        // do not allow the AI to be changed during update
        m_AI_locked = true;
        AI()->UpdateAI(update_diff);   // AI not react good at real update delays (while freeze in non-active part of map)
        m_AI_locked = false;
    }
}
