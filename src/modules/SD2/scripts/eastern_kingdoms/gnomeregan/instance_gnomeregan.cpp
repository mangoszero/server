/**
 * ScriptDev2 is an extension for mangos providing enhanced features for
 * area triggers, creatures, game objects, instances, items, and spells beyond
 * the default database scripting in mangos.
 *
 * Copyright (C) 2006-2013  ScriptDev2 <http://www.scriptdev2.com/>
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
 * ScriptData
 * SDName:      Instance_Gnomeregan
 * SD%Complete: 90
 * SDComment:   Support for Grubbis and Thermaplugg Encounters
 * SDCategory:  Gnomeregan
 * EndScriptData
 */

#include "precompiled.h"
#include "gnomeregan.h"

static bool sortFromEastToWest(GameObject* pFirst, GameObject* pSecond)
{
    return pFirst && pSecond && pFirst->GetPositionY() < pSecond->GetPositionY();
}

struct is_gnomeregan : public InstanceScript
{
    is_gnomeregan() : InstanceScript("instance_gnomeregan") {}

    class instance_gnomeregan : public ScriptedInstance
    {
    public:
        instance_gnomeregan(Map* pMap) : ScriptedInstance(pMap)
        {
            Initialize();
        }

        ~instance_gnomeregan() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_BLASTMASTER_SHORTFUSE:
            case NPC_MEKGINEER_THERMAPLUGG:
                m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
                break;
            default:
                break;
            }
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            switch (pGo->GetEntry())
            {
            case GO_CAVE_IN_NORTH:
            case GO_CAVE_IN_SOUTH:
            case GO_THE_FINAL_CHAMBER:
            case GO_GNOME_FACE_1:
            case GO_GNOME_FACE_2:
            case GO_GNOME_FACE_3:
            case GO_GNOME_FACE_4:
            case GO_GNOME_FACE_5:
            case GO_GNOME_FACE_6:
                m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
                break;

            case GO_RED_ROCKET:
                m_lRedRocketGUIDs.push_back(pGo->GetObjectGuid());
                return;
            case GO_EXPLOSIVE_CHARGE:
                m_luiExplosiveChargeGUIDs.push_back(pGo->GetObjectGuid());
                return;
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
            case TYPE_GRUBBIS:
                m_auiEncounter[0] = uiData;
                if (uiData == IN_PROGRESS)
                {
                    // Sort the explosive charges if needed
                    if (!m_luiExplosiveChargeGUIDs.empty())
                    {
                        std::list<GameObject*> lExplosiveCharges;
                        for (GuidList::const_iterator itr = m_luiExplosiveChargeGUIDs.begin(); itr != m_luiExplosiveChargeGUIDs.end(); ++itr)
                        {
                            if (GameObject* pCharge = instance->GetGameObject(*itr))
                            {
                                lExplosiveCharges.push_back(pCharge);
                            }
                        }
                        m_luiExplosiveChargeGUIDs.clear();

                        // Sort from east to west
                        lExplosiveCharges.sort(sortFromEastToWest);

                        // Sort to south and north
                        uint8 uiCounterSouth = 0;
                        uint8 uiCounterNorth = 0;
                        GameObject* pCaveInSouth = GetSingleGameObjectFromStorage(GO_CAVE_IN_SOUTH);
                        GameObject* pCaveInNorth = GetSingleGameObjectFromStorage(GO_CAVE_IN_NORTH);
                        if (pCaveInSouth && pCaveInNorth)
                        {
                            for (std::list<GameObject*>::iterator itr = lExplosiveCharges.begin(); itr != lExplosiveCharges.end(); ++itr)
                            {
                                if ((*itr)->GetDistanceOrder(pCaveInSouth, pCaveInNorth) && uiCounterSouth < MAX_EXPLOSIVES_PER_SIDE)
                                {
                                    m_aExplosiveSortedGuids[0][uiCounterSouth] = (*itr)->GetObjectGuid();
                                    ++uiCounterSouth;
                                }
                                else if (uiCounterNorth < MAX_EXPLOSIVES_PER_SIDE)
                                {
                                    m_aExplosiveSortedGuids[1][uiCounterNorth] = (*itr)->GetObjectGuid();
                                    ++uiCounterNorth;
                                }
                            }
                        }
                    }
                }
                if (uiData == FAIL)
                {
                    // Despawn possible spawned explosive charges
                    SetData(TYPE_EXPLOSIVE_CHARGE, DATA_EXPLOSIVE_CHARGE_USE);
                }
                if (uiData == DONE)
                {
                    for (GuidList::const_iterator itr = m_lRedRocketGUIDs.begin(); itr != m_lRedRocketGUIDs.end(); ++itr)
                    {
                        DoRespawnGameObject(*itr, HOUR);
                    }
                }
                break;
            case TYPE_EXPLOSIVE_CHARGE:
                switch (uiData)
                {
                case DATA_EXPLOSIVE_CHARGE_1:
                    DoRespawnGameObject(m_aExplosiveSortedGuids[0][0], HOUR);
                    m_luiSpawnedExplosiveChargeGUIDs.push_back(m_aExplosiveSortedGuids[0][0]);
                    break;
                case DATA_EXPLOSIVE_CHARGE_2:
                    DoRespawnGameObject(m_aExplosiveSortedGuids[0][1], HOUR);
                    m_luiSpawnedExplosiveChargeGUIDs.push_back(m_aExplosiveSortedGuids[0][1]);
                    break;
                case DATA_EXPLOSIVE_CHARGE_3:
                    DoRespawnGameObject(m_aExplosiveSortedGuids[1][0], HOUR);
                    m_luiSpawnedExplosiveChargeGUIDs.push_back(m_aExplosiveSortedGuids[1][0]);
                    break;
                case DATA_EXPLOSIVE_CHARGE_4:
                    DoRespawnGameObject(m_aExplosiveSortedGuids[1][1], HOUR);
                    m_luiSpawnedExplosiveChargeGUIDs.push_back(m_aExplosiveSortedGuids[1][1]);
                    break;
                case DATA_EXPLOSIVE_CHARGE_USE:
                    Creature* pBlastmaster = GetSingleCreatureFromStorage(NPC_BLASTMASTER_SHORTFUSE);
                    if (!pBlastmaster)
                    {
                        break;
                    }
                    for (GuidList::const_iterator itr = m_luiSpawnedExplosiveChargeGUIDs.begin(); itr != m_luiSpawnedExplosiveChargeGUIDs.end(); ++itr)
                    {
                        if (GameObject* pExplosive = instance->GetGameObject(*itr))
                        {
                            pExplosive->Use(pBlastmaster);
                        }
                    }
                    m_luiSpawnedExplosiveChargeGUIDs.clear();
                    break;
                }
                return;
            case TYPE_THERMAPLUGG:
                {
                Creature *thermaplugg = GetSingleCreatureFromStorage(NPC_MEKGINEER_THERMAPLUGG);
                m_auiEncounter[1] = uiData;
                if (uiData == IN_PROGRESS)
                {
                    // Make Door locked
                    if (GameObject* pDoor = GetSingleGameObjectFromStorage(GO_THE_FINAL_CHAMBER))
                    {
                        pDoor->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
                        if (pDoor->getLootState() == GO_ACTIVATED)
                        {
                            pDoor->ResetDoorOrButton();
                        }
                    }

                    // Always directly activates this bomb-face
                    if (thermaplugg && thermaplugg->AI())
                        thermaplugg->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, thermaplugg, thermaplugg, 2);
                }
                else if (uiData == DONE || uiData == FAIL)
                {
                    // Make Door unlocked again
                    if (GameObject* pDoor = GetSingleGameObjectFromStorage(GO_THE_FINAL_CHAMBER))
                    {
                        pDoor->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_LOCKED);
                        if (pDoor->getLootState() == GO_READY)
                        {
                            pDoor->UseDoorOrButton();
                        }
                    }

                    // Deactivate all remaining BombFaces
                    if (thermaplugg && thermaplugg->AI())
                        thermaplugg->AI()->SendAIEvent(AI_EVENT_CUSTOM_C, thermaplugg, thermaplugg);
                }
                break;
                }
            case TYPE_DO_BOMB_OFF:
                if (Creature *thermaplugg = GetSingleCreatureFromStorage(NPC_MEKGINEER_THERMAPLUGG))
                {
                    if (thermaplugg->AI())
                        thermaplugg->AI()->SendAIEvent(AI_EVENT_CUSTOM_B, thermaplugg, thermaplugg, uiData);
                }
                return;
            }

            if (uiData == DONE)
            {
                OUT_SAVE_INST_DATA;

                std::ostringstream saveStream;
                saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1];

                m_strInstData = saveStream.str();

                SaveToDB();
                OUT_SAVE_INST_DATA_COMPLETE;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            switch (uiType)
            {
            case TYPE_GRUBBIS:
                return m_auiEncounter[0];
            case TYPE_THERMAPLUGG:
                return m_auiEncounter[1];
            default:
                return 0;
            }
        }

        uint64 GetData64(uint32 type) const override
        {
            EntryGuidMap::const_iterator find = m_mGoEntryGuidStore.find(type);
            if (find != m_mGoEntryGuidStore.end())
                return find->second.GetRawValue();

            return 0;
        }

        const char* Save() const override { return m_strInstData.c_str(); }
        void Load(const char* chrIn) override
        {
            if (!chrIn)
            {
                OUT_LOAD_INST_DATA_FAIL;
                return;
            }

            OUT_LOAD_INST_DATA(chrIn);

            std::istringstream loadStream(chrIn);
            loadStream >> m_auiEncounter[0] >> m_auiEncounter[1];

            for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
            {
                if (m_auiEncounter[i] == IN_PROGRESS)
                {
                    m_auiEncounter[i] = NOT_STARTED;
                }
            }

            OUT_LOAD_INST_DATA_COMPLETE;
        }

    protected:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        ObjectGuid m_aExplosiveSortedGuids[2][MAX_EXPLOSIVES_PER_SIDE];

        GuidList m_luiExplosiveChargeGUIDs;
        GuidList m_luiSpawnedExplosiveChargeGUIDs;
        GuidList m_lRedRocketGUIDs;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_gnomeregan(pMap);
    }
};

void AddSC_instance_gnomeregan()
{
    Script* s;
    s = new is_gnomeregan();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_gnomeregan";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_gnomeregan;
    //pNewScript->RegisterSelf();
}
