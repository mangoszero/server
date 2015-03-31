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
 * SDName:      Instance_Blackfathom_Deeps
 * SD%Complete: 50
 * SDComment:   None
 * SDCategory:  Blackfathom Deeps
 * EndScriptData
 */

#include "precompiled.h"
#include "blackfathom_deeps.h"

/**
 * Encounter 0 = Twilight Lord Kelris
 * Encounter 1 = Shrine event
 * Must kill twilight lord for shrine event to be possible
 * Encounter 3 = Baron Aquanis
 */

struct Locations
{
    float m_fX, m_fY, m_fZ, m_fO;
};

static const Locations aSpawnLocations[6] =                 // Should be near the correct positions
{
    { -768.949f, -174.413f, -25.87f, 3.09f },                // Left side
    { -768.888f, -164.238f, -25.87f, 3.09f },
    { -768.951f, -153.911f, -25.88f, 3.09f },
    { -867.782f, -174.352f, -25.87f, 6.27f },                // Right side
    { -867.875f, -164.089f, -25.87f, 6.27f },
    { -867.859f, -153.927f, -25.88f, 6.27f }
};

struct PosCount
{
    uint8 m_uiCount, m_uiSummonPosition;
};

struct SummonInformation
{
    uint8 m_uiWaveIndex;
    uint32 m_uiNpcEntry;
    PosCount m_aCountAndPos[MAX_COUNT_POS];
};

// ASSERT m_uiSummonPosition < 6 (see aSpawnLocations)
static const SummonInformation aWaveSummonInformation[] =
{
    { 0, NPC_AKUMAI_SNAPJAW, { { 1, 0 }, { 1, 1 }, { 1, 5 } } },
    { 1, NPC_AKUMAI_SERVANT, { { 1, 1 }, { 1, 4 }, { 0, 0 } } },
    { 2, NPC_BARBED_CRUSTACEAN, { { 1, 0 }, { 1, 2 }, { 0, 0 } } },
    { 2, NPC_BARBED_CRUSTACEAN, { { 1, 3 }, { 1, 4 }, { 0, 0 } } },
    { 3, NPC_MURKSHALLOW_SOFTSHELL, { { 2, 0 }, { 1, 1 }, { 2, 2 } } },
    { 3, NPC_MURKSHALLOW_SOFTSHELL, { { 1, 3 }, { 2, 4 }, { 2, 5 } } }
};

struct is_blackfathom_deeps : public InstanceScript
{
    is_blackfathom_deeps() : InstanceScript("instance_blackfathom_deeps") {}

    class instance_blackfathom_deeps : public ScriptedInstance
    {
    public:
        instance_blackfathom_deeps(Map* pMap) : ScriptedInstance(pMap),
            m_uiWaveCounter(0)
        {
            Initialize();
        }

        ~instance_blackfathom_deeps() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
            memset(&m_uiSpawnMobsTimer, 0, sizeof(m_uiSpawnMobsTimer));
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            if (pCreature->GetEntry() == NPC_KELRIS)
            {
                m_mNpcEntryGuidStore[NPC_KELRIS] = pCreature->GetObjectGuid();
            }
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            switch (pGo->GetEntry())
            {
            case GO_PORTAL_DOOR:
                if (m_auiEncounter[1] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }

                m_mGoEntryGuidStore[GO_PORTAL_DOOR] = pGo->GetObjectGuid();
                break;
            case GO_SHRINE_1:
            case GO_SHRINE_2:
            case GO_SHRINE_3:
            case GO_SHRINE_4:
                if (m_auiEncounter[1] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            }
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            // Only use this function if shrine event is in progress
            if (m_auiEncounter[1] != IN_PROGRESS)
            {
                return;
            }

            switch (pCreature->GetEntry())
            {
            case NPC_AKUMAI_SERVANT:
                m_lWaveMobsGuids[1].remove(pCreature->GetGUIDLow());
                break;
            case NPC_AKUMAI_SNAPJAW:
                m_lWaveMobsGuids[0].remove(pCreature->GetGUIDLow());
                break;
            case NPC_BARBED_CRUSTACEAN:
                m_lWaveMobsGuids[2].remove(pCreature->GetGUIDLow());
                break;
            case NPC_MURKSHALLOW_SOFTSHELL:
                m_lWaveMobsGuids[3].remove(pCreature->GetGUIDLow());
                break;
            }

            if (IsWaveEventFinished())
            {
                SetData(TYPE_SHRINE, DONE);
            }
        }

        void Update(uint32 uiDiff) override
        {
            // Only use this function if shrine event is in progress
            if (m_auiEncounter[1] != IN_PROGRESS)
            {
                return;
            }

            for (uint8 i = 0; i < MAX_FIRES; ++i)
            {
                if (m_uiSpawnMobsTimer[i])
                {
                    if (m_uiSpawnMobsTimer[i] <= uiDiff)
                    {
                        DoSpawnMobs(i);
                        m_uiSpawnMobsTimer[i] = 0;
                    }
                    else
                    {
                        m_uiSpawnMobsTimer[i] -= uiDiff;
                    }
                }
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
            case TYPE_KELRIS:                                   // EventAI must set instance data (1,3) at his death
                if (m_auiEncounter[0] != DONE && uiData == DONE)
                {
                    m_auiEncounter[0] = uiData;
                }
                break;
            case TYPE_SHRINE:
                m_auiEncounter[1] = uiData;
                if (uiData == IN_PROGRESS)
                {
                    m_uiSpawnMobsTimer[m_uiWaveCounter] = 3000;
                    ++m_uiWaveCounter;
                }
                else if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_PORTAL_DOOR);
                }
                break;
            case TYPE_STONE:
                if (m_auiEncounter[2] != DONE && uiData == DONE)
                {
                    m_auiEncounter[2] = uiData;
                }
                break;

            }

            if (uiData == DONE)
            {
                OUT_SAVE_INST_DATA;

                std::ostringstream saveStream;

                saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2];

                m_strInstData = saveStream.str();
                SaveToDB();
                OUT_SAVE_INST_DATA_COMPLETE;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            switch (uiType)
            {
            case TYPE_KELRIS:
                return m_auiEncounter[0];
            case TYPE_SHRINE:
                return m_auiEncounter[1];
            case TYPE_STONE:
                return m_auiEncounter[2];
            default:
                return 0;
            }
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
            loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2];

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
        void DoSpawnMobs(uint8 uiWaveIndex)
        {
            Creature* pKelris = GetSingleCreatureFromStorage(NPC_KELRIS);
            if (!pKelris)
            {
                return;
            }

            float fX_resp, fY_resp, fZ_resp;

            pKelris->GetRespawnCoord(fX_resp, fY_resp, fZ_resp);

            for (uint8 i = 0; i < countof(aWaveSummonInformation); ++i)
            {
                if (aWaveSummonInformation[i].m_uiWaveIndex != uiWaveIndex)
                {
                    continue;
                }

                // Summon mobs at positions
                for (uint8 j = 0; j < MAX_COUNT_POS; ++j)
                {
                    for (uint8 k = 0; k < aWaveSummonInformation[i].m_aCountAndPos[j].m_uiCount; ++k)
                    {
                        uint8 uiPos = aWaveSummonInformation[i].m_aCountAndPos[j].m_uiSummonPosition;
                        float fPosX = aSpawnLocations[uiPos].m_fX;
                        float fPosY = aSpawnLocations[uiPos].m_fY;
                        float fPosZ = aSpawnLocations[uiPos].m_fZ;
                        float fPosO = aSpawnLocations[uiPos].m_fO;

                        // Adapt fPosY slightly in case of higher summon-counts
                        if (aWaveSummonInformation[i].m_aCountAndPos[j].m_uiCount > 1)
                        {
                            fPosY = fPosY - INTERACTION_DISTANCE / 2 + k * INTERACTION_DISTANCE / aWaveSummonInformation[i].m_aCountAndPos[j].m_uiCount;
                        }

                        if (Creature* pSummoned = pKelris->SummonCreature(aWaveSummonInformation[i].m_uiNpcEntry, fPosX, fPosY, fPosZ, fPosO, TEMPSUMMON_DEAD_DESPAWN, 0))
                        {
                            pSummoned->GetMotionMaster()->MovePoint(0, fX_resp, fY_resp, fZ_resp);
                            m_lWaveMobsGuids[uiWaveIndex].push_back(pSummoned->GetGUIDLow());
                        }
                    }
                }
            }
        }

        bool IsWaveEventFinished()
        {
            // If not all fires are lighted return
            if (m_uiWaveCounter < MAX_FIRES)
            {
                return false;
            }

            // Check if all mobs are dead
            for (uint8 i = 0; i < MAX_FIRES; ++i)
            {
                if (!m_lWaveMobsGuids[i].empty())
                {
                    return false;
                }
            }

            return true;
        }

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint32 m_uiSpawnMobsTimer[MAX_FIRES];
        uint8 m_uiWaveCounter;

        std::list<uint32> m_lWaveMobsGuids[MAX_FIRES];
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_blackfathom_deeps(pMap);
    }
};

void AddSC_instance_blackfathom_deeps()
{
    Script* s;
    s = new is_blackfathom_deeps();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_blackfathom_deeps";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_blackfathom_deeps;
    //pNewScript->RegisterSelf();
}
