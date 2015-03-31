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
 * SDName:      Instance_Ruins_of_Ahnqiraj
 * SD%Complete: 80
 * SDComment:   It's not clear if the Rajaxx event should reset if Andorov dies, or party wipes.
 * SDCategory:  Ruins of Ahn'Qiraj
 * EndScriptData
 */

#include "precompiled.h"
#include "ruins_of_ahnqiraj.h"

struct SortingParameters
{
    uint32 m_uiEntry;
    int32 m_uiYellEntry;
    float m_fSearchDist;
};

static const SortingParameters aArmySortingParameters[MAX_ARMY_WAVES] =
{
    { NPC_CAPTAIN_QEEZ, 0, 20.0f },
    { NPC_CAPTAIN_TUUBID, 0, 22.0f },
    { NPC_CAPTAIN_DRENN, SAY_WAVE3, 22.0f },
    { NPC_CAPTAIN_XURREM, SAY_WAVE4, 22.0f },
    { NPC_MAJOR_YEGGETH, SAY_WAVE5, 20.0f },
    { NPC_MAJOR_PAKKON, SAY_WAVE6, 21.0f },
    { NPC_COLONEL_ZERRAN, SAY_WAVE7, 17.0f },
};

// Spawn coords for Andorov and his team
static const SpawnLocation aAndorovSpawnLocs[MAX_HELPERS] =
{
    { NPC_GENERAL_ANDOROV, -8660.4f, 1510.29f, 32.449f, 2.2184f },
    { NPC_KALDOREI_ELITE, -8655.84f, 1509.78f, 32.462f, 2.33341f },
    { NPC_KALDOREI_ELITE, -8657.39f, 1506.28f, 32.418f, 2.33346f },
    { NPC_KALDOREI_ELITE, -8660.96f, 1504.9f, 32.1567f, 2.33306f },
    { NPC_KALDOREI_ELITE, -8664.45f, 1506.44f, 32.0944f, 2.33302f }
};

struct is_ruins_of_ahnqiraj : public InstanceScript
{
    is_ruins_of_ahnqiraj() : InstanceScript("instance_ruins_of_ahnqiraj") {}

    class instance_ruins_of_ahnqiraj : public ScriptedInstance
    {
    public:
        instance_ruins_of_ahnqiraj(Map* pMap) : ScriptedInstance(pMap),
            m_uiArmyDelayTimer(0),
            m_uiCurrentArmyWave(0)
        {
            Initialize();
        }

        ~instance_ruins_of_ahnqiraj() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
        }

        // bool IsEncounterInProgress() const override;              // not active in AQ20

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_OSSIRIAN_TRIGGER:
                // Only store static spawned
                if (pCreature->IsTemporarySummon())
                {
                    break;
                }
            case NPC_BURU:
            case NPC_OSSIRIAN:
            case NPC_RAJAXX:
            case NPC_GENERAL_ANDOROV:
            case NPC_COLONEL_ZERRAN:
            case NPC_MAJOR_PAKKON:
            case NPC_MAJOR_YEGGETH:
            case NPC_CAPTAIN_XURREM:
            case NPC_CAPTAIN_DRENN:
            case NPC_CAPTAIN_TUUBID:
            case NPC_CAPTAIN_QEEZ:
                m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
                break;
            case NPC_KALDOREI_ELITE:
                m_lKaldoreiGuidList.push_back(pCreature->GetObjectGuid());
                return;
            }
        }

        void OnPlayerEnter(Player* pPlayer) override
        {
            // Spawn andorov if necessary
            if (m_auiEncounter[TYPE_KURINNAXX] == DONE)
            {
                DoSpawnAndorovIfCan();
            }
        }

        void OnCreatureEnterCombat(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_KURINNAXX:
                SetData(TYPE_KURINNAXX, IN_PROGRESS);
                break;
            case NPC_MOAM:
                SetData(TYPE_MOAM, IN_PROGRESS);
                break;
            case NPC_BURU:
                SetData(TYPE_BURU, IN_PROGRESS);
                break;
            case NPC_AYAMISS:
                SetData(TYPE_AYAMISS, IN_PROGRESS);
                break;
            case NPC_OSSIRIAN:
                SetData(TYPE_OSSIRIAN, IN_PROGRESS);
                break;
            }
        }

        void OnCreatureEvade(Creature* pCreature)
        {
            switch (pCreature->GetEntry())
            {
            case NPC_KURINNAXX:
                SetData(TYPE_KURINNAXX, FAIL);
                break;
            case NPC_MOAM:
                SetData(TYPE_MOAM, FAIL);
                break;
            case NPC_BURU:
                SetData(TYPE_BURU, FAIL);
                break;
            case NPC_AYAMISS:
                SetData(TYPE_AYAMISS, FAIL);
                break;
            case NPC_OSSIRIAN:
                SetData(TYPE_OSSIRIAN, FAIL);
                break;
            case NPC_RAJAXX:
                // Rajaxx yells on evade
                DoScriptText(SAY_DEAGGRO, pCreature);
                // no break;
            case NPC_COLONEL_ZERRAN:
            case NPC_MAJOR_PAKKON:
            case NPC_MAJOR_YEGGETH:
            case NPC_CAPTAIN_XURREM:
            case NPC_CAPTAIN_DRENN:
            case NPC_CAPTAIN_TUUBID:
            case NPC_CAPTAIN_QEEZ:
                SetData(TYPE_RAJAXX, FAIL);
                break;
            }
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_KURINNAXX:
                SetData(TYPE_KURINNAXX, DONE);
                break;
            case NPC_RAJAXX:
                SetData(TYPE_RAJAXX, DONE);
                break;
            case NPC_MOAM:
                SetData(TYPE_MOAM, DONE);
                break;
            case NPC_BURU:
                SetData(TYPE_BURU, DONE);
                break;
            case NPC_AYAMISS:
                SetData(TYPE_AYAMISS, DONE);
                break;
            case NPC_OSSIRIAN:
                SetData(TYPE_OSSIRIAN, DONE);
                break;
            case NPC_COLONEL_ZERRAN:
            case NPC_MAJOR_PAKKON:
            case NPC_MAJOR_YEGGETH:
            case NPC_CAPTAIN_XURREM:
            case NPC_CAPTAIN_DRENN:
            case NPC_CAPTAIN_TUUBID:
            case NPC_CAPTAIN_QEEZ:
            case NPC_QIRAJI_WARRIOR:
            case NPC_SWARMGUARD_NEEDLER:
            {
                                           // If event isn't started by Andorov, return
                                           if (GetData(TYPE_RAJAXX) != IN_PROGRESS)
                                           {
                                               return;
                                           }

                                           // Check if the dead creature belongs to the current wave
                                           if (m_sArmyWavesGuids[m_uiCurrentArmyWave - 1].find(pCreature->GetObjectGuid()) != m_sArmyWavesGuids[m_uiCurrentArmyWave - 1].end())
                                           {
                                               m_sArmyWavesGuids[m_uiCurrentArmyWave - 1].erase(pCreature->GetObjectGuid());

                                               // If all the soldiers from the current wave are dead, then send the next one
                                               if (m_sArmyWavesGuids[m_uiCurrentArmyWave - 1].empty())
                                               {
                                                   DoSendNextArmyWave();
                                               }
                                           }
                                           break;
            }
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
            case TYPE_KURINNAXX:
                if (uiData == DONE)
                {
                    DoSpawnAndorovIfCan();

                    // Yell after kurinnaxx
                    DoOrSimulateScriptTextForThisInstance(SAY_OSSIRIAN_INTRO, NPC_OSSIRIAN);
                }
                m_auiEncounter[uiType] = uiData;
                break;
            case TYPE_RAJAXX:
                m_auiEncounter[uiType] = uiData;
                if (uiData == IN_PROGRESS)
                {
                    DoSortArmyWaves();
                }
                if (uiData == DONE)
                {
                    if (Creature* pAndorov = GetSingleCreatureFromStorage(NPC_GENERAL_ANDOROV))
                    {
                        if (pAndorov->IsAlive())
                        {
                            pAndorov->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                        }
                    }
                }
                break;
            case TYPE_MOAM:
            case TYPE_BURU:
            case TYPE_AYAMISS:
            case TYPE_OSSIRIAN:
                m_auiEncounter[uiType] = uiData;
                break;
            }

            if (uiData == DONE)
            {
                OUT_SAVE_INST_DATA;

                std::ostringstream saveStream;
                saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2]
                    << " " << m_auiEncounter[3] << " " << m_auiEncounter[4] << " " << m_auiEncounter[5];

                m_strInstData = saveStream.str();

                SaveToDB();
                OUT_SAVE_INST_DATA_COMPLETE;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            if (uiType < MAX_ENCOUNTER)
            {
                return m_auiEncounter[uiType];
            }

            return 0;
        }

        void SetData64(uint32 type, uint64 data) override
        {
            if (type == TYPE_SIGNAL)
            {
                Creature *general = instance->GetCreature(ObjectGuid(data));
                for (GuidList::const_iterator itr = m_lKaldoreiGuidList.begin(); itr != m_lKaldoreiGuidList.end(); ++itr)
                {
                    if (Creature* pKaldorei = instance->GetCreature(*itr))
                    {
                        pKaldorei->GetMotionMaster()->MoveFollow(general, pKaldorei->GetDistance(general), pKaldorei->GetAngle(general));
                    }
                }
            }
        }

        void Update(uint32 uiDiff) override
        {
            if (GetData(TYPE_RAJAXX) == IN_PROGRESS)
            {
                if (m_uiArmyDelayTimer)
                {
                    if (m_uiArmyDelayTimer <= uiDiff)
                    {
                        DoSendNextArmyWave();
                        m_uiArmyDelayTimer = 2 * MINUTE * IN_MILLISECONDS;
                    }
                    else
                    {
                        m_uiArmyDelayTimer -= uiDiff;
                    }
                }
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

            loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2]
                >> m_auiEncounter[3] >> m_auiEncounter[4] >> m_auiEncounter[5];

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
        void DoSpawnAndorovIfCan()
        {
            if (GetSingleCreatureFromStorage(NPC_GENERAL_ANDOROV))
            {
                return;
            }

            Player* pPlayer = GetPlayerInMap();
            if (!pPlayer)
            {
                return;
            }

            for (uint8 i = 0; i < MAX_HELPERS; ++i)
            {
                pPlayer->SummonCreature(aAndorovSpawnLocs[i].m_uiEntry, aAndorovSpawnLocs[i].m_fX, aAndorovSpawnLocs[i].m_fY, aAndorovSpawnLocs[i].m_fZ, aAndorovSpawnLocs[i].m_fO, TEMPSUMMON_DEAD_DESPAWN, 0);
            }
        }

        void DoSortArmyWaves()
        {
            std::list<Creature*> lCreatureList;

            // Sort the 7 army waves
            // We need to use gridsearcher for this, because coords search is too complicated here
            for (uint8 i = 0; i < MAX_ARMY_WAVES; ++i)
            {
                // Clear all the army waves
                m_sArmyWavesGuids[i].clear();
                lCreatureList.clear();

                if (Creature* pTemp = GetSingleCreatureFromStorage(aArmySortingParameters[i].m_uiEntry))
                {
                    GetCreatureListWithEntryInGrid(lCreatureList, pTemp, NPC_QIRAJI_WARRIOR, aArmySortingParameters[i].m_fSearchDist);
                    GetCreatureListWithEntryInGrid(lCreatureList, pTemp, NPC_SWARMGUARD_NEEDLER, aArmySortingParameters[i].m_fSearchDist);

                    for (std::list<Creature*>::const_iterator itr = lCreatureList.begin(); itr != lCreatureList.end(); ++itr)
                    {
                        if ((*itr)->IsAlive())
                        {
                            m_sArmyWavesGuids[i].insert((*itr)->GetObjectGuid());
                        }
                    }

                    if (pTemp->IsAlive())
                    {
                        m_sArmyWavesGuids[i].insert(pTemp->GetObjectGuid());
                    }
                }
            }

            // send the first wave
            m_uiCurrentArmyWave = 0;
            DoSendNextArmyWave();
        }

        void DoSendNextArmyWave()
        {
            // The next army wave is sent into battle after 2 min or after the previous wave is finished
            if (GetData(TYPE_RAJAXX) != IN_PROGRESS)
            {
                return;
            }

            // The last wave is General Rajaxx itself
            if (m_uiCurrentArmyWave == MAX_ARMY_WAVES)
            {
                if (Creature* pRajaxx = GetSingleCreatureFromStorage(NPC_RAJAXX))
                {
                    DoScriptText(SAY_INTRO, pRajaxx);
                    pRajaxx->SetInCombatWithZone();
                }
            }
            else
            {
                // Increase the wave id if some are already dead
                while (m_sArmyWavesGuids[m_uiCurrentArmyWave].empty())
                {
                    ++m_uiCurrentArmyWave;
                }

                float fX, fY, fZ;
                for (GuidSet::const_iterator itr = m_sArmyWavesGuids[m_uiCurrentArmyWave].begin(); itr != m_sArmyWavesGuids[m_uiCurrentArmyWave].end(); ++itr)
                {
                    if (Creature* pTemp = instance->GetCreature(*itr))
                    {
                        if (!pTemp->IsAlive())
                        {
                            continue;
                        }

                        pTemp->SetWalk(false);
                        pTemp->GetRandomPoint(aAndorovMoveLocs[4].m_fX, aAndorovMoveLocs[4].m_fY, aAndorovMoveLocs[4].m_fZ, 10.0f, fX, fY, fZ);
                        pTemp->GetMotionMaster()->MovePoint(0, fX, fY, fZ);
                    }
                }

                // Yell on each wave (except the first 2)
                if (aArmySortingParameters[m_uiCurrentArmyWave].m_uiYellEntry)
                {
                    if (Creature* pRajaxx = GetSingleCreatureFromStorage(NPC_RAJAXX))
                    {
                        DoScriptText(aArmySortingParameters[m_uiCurrentArmyWave].m_uiYellEntry, pRajaxx);
                    }
                }
            }

            // on wowwiki it states that there were 3 min between the waves, but this was reduced in later patches
            m_uiArmyDelayTimer = 2 * MINUTE * IN_MILLISECONDS;
            ++m_uiCurrentArmyWave;
        }

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidList m_lKaldoreiGuidList;
        GuidSet m_sArmyWavesGuids[MAX_ARMY_WAVES];

        uint32 m_uiArmyDelayTimer;
        uint8 m_uiCurrentArmyWave;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_ruins_of_ahnqiraj(pMap);
    }
};

void AddSC_instance_ruins_of_ahnqiraj()
{
    Script* s;
    s = new is_ruins_of_ahnqiraj();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_ruins_of_ahnqiraj";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_ruins_of_ahnqiraj;
    //pNewScript->RegisterSelf();
}
