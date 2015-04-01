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
 * SDName:      Instance_Uldaman
 * SD%Complete: 60
 * SDComment:   None
 * SDCategory:  Uldaman
 * EndScriptData
 */

#include "precompiled.h"
#include "uldaman.h"

struct is_uldaman : public InstanceScript
{
    is_uldaman() : InstanceScript("instance_uldaman") {}

    class instance_uldaman : public ScriptedInstance
    {
    public:
        instance_uldaman(Map* pMap) : ScriptedInstance(pMap),
            m_uiKeeperCooldown(0),
            m_uiStoneKeepersFallen(0)
        {
            Initialize();
        }

        ~instance_uldaman() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            switch (pGo->GetEntry())
            {
            case GO_TEMPLE_DOOR_UPPER:
            case GO_TEMPLE_DOOR_LOWER:
                if (GetData(TYPE_ALTAR_EVENT) == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_ANCIENT_VAULT:
                if (GetData(TYPE_ARCHAEDAS) == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_ANCIENT_TREASURE:
                break;
            default:
                return;
            }
            m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_HALLSHAPER:
            case NPC_CUSTODIAN:
                m_lWardens.push_back(pCreature->GetObjectGuid());
                break;
            case NPC_STONE_KEEPER:
                m_lKeepers.push_back(pCreature->GetObjectGuid());
                break;
            case NPC_ARCHAEDAS:
                m_mNpcEntryGuidStore[NPC_ARCHAEDAS] = pCreature->GetObjectGuid();
                break;
            default:
                break;
            }
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            if (pCreature->GetEntry() == NPC_STONE_KEEPER)
            {
                ++m_uiStoneKeepersFallen;

                if (m_lKeepers.size() == m_uiStoneKeepersFallen)
                {
                    SetData(TYPE_ALTAR_EVENT, DONE);
                }
                else
                {
                    m_uiKeeperCooldown = 5000;
                }
            }
        }

        void OnCreatureEvade(Creature* pCreature) override
        {
            // Reset Altar event
            if (pCreature->GetEntry() == NPC_STONE_KEEPER)
            {
                SetData(TYPE_ALTAR_EVENT, NOT_STARTED);
            }
        }

        void Update(uint32 uiDiff) override
        {
            if (GetData(TYPE_ALTAR_EVENT) != IN_PROGRESS)
            {
                return;
            }

            if (!m_uiKeeperCooldown)
            {
                return;
            }

            if (m_uiKeeperCooldown <= uiDiff)
            {
                for (GuidList::const_iterator itr = m_lKeepers.begin(); itr != m_lKeepers.end(); ++itr)
                {
                    // Get Keeper which is alive and out of combat
                    Creature* pKeeper = instance->GetCreature(*itr);
                    if (!pKeeper || !pKeeper->IsAlive() || pKeeper->getVictim())
                    {
                        continue;
                    }

                    // Get starter player for attack
                    Player* pPlayer = pKeeper->GetMap()->GetPlayer(m_playerGuid);
                    if (!pPlayer || !pPlayer->IsAlive())
                    {
                        // If he's not available, then get a random player, within a reasonamble distance in map
                        pPlayer = GetPlayerInMap(true, false);
                        if (!pPlayer || !pPlayer->IsWithinDistInMap(pKeeper, 50.0f))
                        {
                            SetData(TYPE_ALTAR_EVENT, NOT_STARTED);
                            return;
                        }
                    }

                    // Attack the player
                    pKeeper->RemoveAurasDueToSpell(SPELL_STONED);
                    pKeeper->AI()->AttackStart(pPlayer);
                    break;
                }

                m_uiKeeperCooldown = 0;
            }
            else
            {
                m_uiKeeperCooldown -= uiDiff;
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
            case TYPE_ALTAR_EVENT:
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_TEMPLE_DOOR_UPPER);
                    DoUseDoorOrButton(GO_TEMPLE_DOOR_LOWER);
                }
                else if (uiData == IN_PROGRESS)
                {
                    // Also do a reset before starting the event - this will respawn dead Keepers
                    DoResetKeeperEvent();
                    m_uiKeeperCooldown = 5000;
                }
                else if (uiData == NOT_STARTED)
                {
                    DoResetKeeperEvent();
                    m_uiStoneKeepersFallen = 0;
                }
                m_auiEncounter[0] = uiData;
                break;

            case TYPE_ARCHAEDAS:
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_ANCIENT_VAULT);
                    DoRespawnGameObject(GO_ANCIENT_TREASURE, HOUR);
                }
                m_auiEncounter[1] = uiData;
                break;
            case TYPE_SIGNAL:
                StartEvent(uiData);
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

        void SetData64(uint32 uiData, uint64 uiGuid) override
        {
            switch (uiData)
            {
                // ToDo: check if this one is used in ACID. Otherwise it can be dropped
            case DATA_EVENT_STARTER:
                m_playerGuid = ObjectGuid(uiGuid);
                break;
            case TYPE_SIGNAL:
                m_dwarfSearcher = ObjectGuid(uiGuid);
                break;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            switch (uiType)
            {
            case TYPE_ALTAR_EVENT:
                return m_auiEncounter[0];
            case TYPE_ARCHAEDAS:
                return m_auiEncounter[1];
            }
            return 0;
        }

        uint64 GetData64(uint32 uiData) const override
        {
            switch (uiData)
            {
            case DATA_EVENT_STARTER:
                return m_playerGuid.GetRawValue();
            case TYPE_SIGNAL:
                return GetClosestDwarfNotInCombat() ? GetClosestDwarfNotInCombat()->GetObjectGuid().GetRawValue() : 0;
            }
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

    private:
        void DoResetKeeperEvent()
        {
            if (m_lKeepers.empty())
            {
                script_error_log("Instance Uldaman: ERROR creature %u couldn't be found or something really bad happened.", NPC_STONE_KEEPER);
                return;
            }

            // Force reset all keepers to the original state
            for (GuidList::const_iterator itr = m_lKeepers.begin(); itr != m_lKeepers.end(); ++itr)
            {
                if (Creature* pKeeper = instance->GetCreature(*itr))
                {
                    if (!pKeeper->IsAlive())
                    {
                        pKeeper->Respawn();
                    }
                }
            }
        }

        void StartEvent(uint32 uiEventId)   //TODO check SetData to be really reenterable!
        {
            //m_playerGuid = pPlayer->GetObjectGuid();

            if (uiEventId == EVENT_ID_ALTAR_KEEPER)
            {
                if (GetData(TYPE_ALTAR_EVENT) == NOT_STARTED)
                {
                    SetData(TYPE_ALTAR_EVENT, IN_PROGRESS);
                }
            }
            else if (uiEventId == EVENT_ID_ALTAR_ARCHAEDAS)
            {
                if (GetData(TYPE_ARCHAEDAS) == NOT_STARTED || GetData(TYPE_ARCHAEDAS) == FAIL)
                {
                    SetData(TYPE_ARCHAEDAS, SPECIAL);
                }
            }
        }

        Creature* GetClosestDwarfNotInCombat() const
        {
            Creature *pSearcher = instance->GetCreature(m_dwarfSearcher);
            std::list<Creature*> lTemp;

            for (GuidList::const_iterator itr = m_lWardens.begin(); itr != m_lWardens.end(); ++itr)
            {
                Creature* pTemp = instance->GetCreature(*itr);

                if (pTemp && pTemp->IsAlive() && !pTemp->getVictim())
                {
                    lTemp.push_back(pTemp);
                }
            }

            if (lTemp.empty())
            {
                return NULL;
            }

            lTemp.sort(ObjectDistanceOrder(pSearcher));
            return lTemp.front();
        }

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        ObjectGuid m_playerGuid;
        ObjectGuid m_dwarfSearcher;

        uint32 m_uiKeeperCooldown;
        uint32 m_uiStoneKeepersFallen;

        GuidList m_lWardens;
        GuidList m_lKeepers;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_uldaman(pMap);
    }
};

struct event_spell_altar_boss_aggro : public MapEventScript
{
    event_spell_altar_boss_aggro() : MapEventScript("event_spell_altar_boss_aggro") {}

    bool OnReceived(uint32 uiEventId, Object* pSource, Object* /*pTarget*/, bool bIsStart) override
    {
        if (bIsStart && pSource->GetTypeId() == TYPEID_PLAYER)
        {
            if (InstanceData* pInstance = ((Player*)pSource)->GetInstanceData())
            {
                pInstance->SetData64(DATA_EVENT_STARTER, pSource->GetObjectGuid().GetRawValue());
                pInstance->SetData(TYPE_SIGNAL, uiEventId);
                //pInstance->StartEvent(uiEventId, (Player*)pSource);
                return true;
            }
        }
        return false;
    }
};

void AddSC_instance_uldaman()
{
    Script* s;
    s = new is_uldaman();
    s->RegisterSelf();
    s = new event_spell_altar_boss_aggro();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_uldaman";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_uldaman;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "event_spell_altar_boss_aggro";
    //pNewScript->pProcessEventId = &ProcessEventId_event_spell_altar_boss_aggro;
    //pNewScript->RegisterSelf();
}
