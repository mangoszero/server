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
 * SDName:      Instance_Molten_Core
 * SD%Complete: 25
 * SDComment:   Majordomos and Ragnaros Event missing
 * SDCategory:  Molten Core
 * EndScriptData
 */

#include "precompiled.h"
#include "molten_core.h"

static sSpawnLocation m_aBosspawnLocs[MAX_MAJORDOMO_ADDS] =
{
    {NPC_FLAMEWAKER_ELITE,  737.945f, -1156.48f, -118.945f, 4.46804f},
    {NPC_FLAMEWAKER_ELITE,  752.520f, -1191.02f, -118.218f, 2.49582f},
    {NPC_FLAMEWAKER_ELITE,  752.953f, -1163.94f, -118.869f, 3.70010f},
    {NPC_FLAMEWAKER_ELITE,  738.814f, -1197.40f, -118.018f, 1.83260f},
    {NPC_FLAMEWAKER_HEALER, 746.939f, -1194.87f, -118.016f, 2.21657f},
    {NPC_FLAMEWAKER_HEALER, 747.132f, -1158.87f, -118.897f, 4.03171f},
    {NPC_FLAMEWAKER_HEALER, 757.116f, -1170.12f, -118.793f, 3.40339f},
    {NPC_FLAMEWAKER_HEALER, 755.910f, -1184.46f, -118.449f, 2.80998f}
};

struct is_molten_core : public InstanceScript
{
    is_molten_core() : InstanceScript("instance_molten_core") {}

    class instance_molten_core : public ScriptedInstance
    {
    public:
        instance_molten_core(Map* pMap) : ScriptedInstance(pMap)
        {
            Initialize();
        }

        ~instance_molten_core() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
            memset(&m_auiRuneState, 0, sizeof(m_auiRuneState));
        }

        bool IsEncounterInProgress() const override
        {
            for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
            {
                if (m_auiEncounter[i] == IN_PROGRESS)
                {
                    return true;
                }
            }

            return false;
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
                // Bosses
            case NPC_GARR:
            case NPC_SULFURON:
            case NPC_MAJORDOMO:
                m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
                break;
            case NPC_FIRESWORN:
                m_sFireswornGUID.insert(pCreature->GetObjectGuid());
                break;
            case NPC_LAVA_SURGER:
                if (GetData(TYPE_GARR) == DONE)
                    pCreature->ForcedDespawn(500);
                break;
            }
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_FIRESWORN:
                m_sFireswornGUID.erase(pCreature->GetObjectGuid());
                if (Creature* pGarr = GetSingleCreatureFromStorage(NPC_GARR))
                {
                    if (pGarr->IsAlive())
                    {
                        pGarr->CastSpell(pGarr, SPELL_GARR_ENRAGE, true);
                        pGarr->CastSpell(pGarr, SPELL_GARR_ARMOR_DEBUFF, true);

                        if (!m_sFireswornGUID.size())
                            pGarr->AI()->ReceiveAIEvent(AI_EVENT_CUSTOM_B, pGarr, pGarr, 0);
                    }
                }
                break;
            default:
                break;
            }
        }

        void OnCreatureDespawn(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_FIRESWORN:
                OnCreatureDeath(pCreature);
                break;
            default:
                break;
            }
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            switch (pGo->GetEntry())
            {
                // Runes
            case GO_RUNE_KRESS:
            case GO_RUNE_MOHN:
            case GO_RUNE_BLAZ:
            case GO_RUNE_MAZJ:
            case GO_RUNE_ZETH:
            case GO_RUNE_THERI:
            case GO_RUNE_KORO:
                if (sRuneEncounters const *rstr = GetRuneStructForTrapEntry(pGo->GetGOInfo()->button.linkedTrapId))
                {
                    switch (m_auiRuneState[rstr->getRuneType()])
                    {
                    case DONE:
                        pGo->SetGoState(GO_STATE_READY);
                        break;
                    default:
                        pGo->SetGoState(GO_STATE_ACTIVE);
                        break;
                    }
                }
                // no break here!
                // Majordomo event chest
            case GO_CACHE_OF_THE_FIRE_LORD:
                // Ragnaros GOs
            case GO_LAVA_STEAM:
            case GO_LAVA_SPLASH:
                m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
                break;
                // rune traps: just change state
            case GO_RUNE_KRESS_TRAP:
            case GO_RUNE_MOHN_TRAP:
            case GO_RUNE_BLAZ_TRAP:
            case GO_RUNE_MAZJ_TRAP:
            case GO_RUNE_ZETH_TRAP:
            case GO_RUNE_THERI_TRAP:
            case GO_RUNE_KORO_TRAP:
                if (sRuneEncounters const *rstr = GetRuneStructForTrapEntry(pGo->GetEntry()))
                {
                    switch (m_auiRuneState[rstr->getRuneType()])
                    {
                    case DONE:
                        pGo->SetLootState(GO_JUST_DEACTIVATED); //TODO fix GameObject::Use for traps
                        // no break here!
                    case NOT_STARTED:
                        pGo->SetGoState(GO_STATE_ACTIVE);
                    default:
                        break;
                    }
                }
                m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
                break;
            }
        }

        void OnPlayerEnter(Player* pPlayer) override
        {
            // Summon Majordomo if can
            DoSpawnMajordomoIfCan(true);
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            bool save = (uiData == DONE);
            switch (uiType)
            {
            case TYPE_MAJORDOMO:
                if (uiData == DONE)
                {
                    DoRespawnGameObject(GO_CACHE_OF_THE_FIRE_LORD, HOUR);
                }
                // no break here!
            case TYPE_LUCIFRON:
            case TYPE_MAGMADAR:
            case TYPE_GEHENNAS:
            case TYPE_GARR:
            case TYPE_SHAZZRAH:
            case TYPE_GEDDON:
            case TYPE_GOLEMAGG:
            case TYPE_SULFURON:
            case TYPE_RAGNAROS:
                m_auiEncounter[uiType] = uiData;
                break;
            case TYPE_FLAME_DOSED:
                if (sRuneEncounters const *rstr = GetRuneStructForTrapEntry(uiData))
                {
                    m_auiRuneState[rstr->getRuneType()] = DONE;
                    save = true;
                    if (GameObject *trap = GetSingleGameObjectFromStorage(rstr->m_uiTrapEntry))
                    {
                        trap->SetGoState(GO_STATE_ACTIVE);
                        trap->SetLootState(GO_JUST_DEACTIVATED);    //TODO fix GameObject::Use for traps
                    }
                    if (GameObject *rune = GetSingleGameObjectFromStorage(rstr->m_uiRuneEntry))
                        rune->SetGoState(GO_STATE_READY);
                    DoSpawnMajordomoIfCan(false);
                }
                break;
            case TYPE_DO_FREE_GARR_ADDS:
                for (std::set<ObjectGuid>::const_iterator it = m_sFireswornGUID.begin(); it != m_sFireswornGUID.end(); ++it)
                {
                    ObjectGuid guid = *it;
                    int32 bp0 = 0;
                    if (Creature* firesworn = instance->GetCreature(guid))
                        if (firesworn->IsAlive())
                            firesworn->CastCustomSpell(firesworn, SPELL_SEPARATION_ANXIETY, &bp0, NULL, NULL, true);
                }
                return;
            }

            if (uiType < MAX_ENCOUNTER && uiData == DONE)   // a boss just killed
            {
                if (sRuneEncounters const *rstr = GetRuneStructForBoss(uiType))
                {
                    m_auiRuneState[rstr->getRuneType()] = SPECIAL;
                    if (GameObject *trap = GetSingleGameObjectFromStorage(rstr->m_uiTrapEntry))
                        trap->SetGoState(GO_STATE_READY);
                }
            }

            if (save)
            {
                OUT_SAVE_INST_DATA;

                std::ostringstream saveStream;
                saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2] << " "
                    << m_auiEncounter[3] << " " << m_auiEncounter[4] << " " << m_auiEncounter[5] << " "
                    << m_auiEncounter[6] << " " << m_auiEncounter[7] << " " << m_auiEncounter[8] << " "
                    << m_auiEncounter[9] << " " << m_auiRuneState[0] << " " << m_auiRuneState[1] << " "
                    << m_auiRuneState[2] << " " << m_auiRuneState[3] << " " << m_auiRuneState[4] << " "
                    << m_auiRuneState[5] << " " << m_auiRuneState[6];

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
            else if (sRuneEncounters const *rstr = GetRuneStructForTrapEntry(uiType))
                return m_auiRuneState[rstr->getRuneType()];

            return 0;
        }

        uint64 GetData64(uint32 uiType) const override
        {
            switch (uiType)
            {
            case NPC_FIRESWORN:
            {
                Creature* garr = GetSingleCreatureFromStorage(NPC_GARR);
                for (std::set<ObjectGuid>::const_iterator it = m_sFireswornGUID.begin(); it != m_sFireswornGUID.end(); ++it)
                {
                    ObjectGuid guid = *it;
                    if (Creature* firesworn = instance->GetCreature(guid))
                        if (firesworn->IsAlive() && firesworn->IsWithinDistInMap(garr, 20.0f, false))
                            return guid.GetRawValue();
                }
                break;
            }
            default:
                break;
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

            loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >> m_auiEncounter[3]
                >> m_auiEncounter[4] >> m_auiEncounter[5] >> m_auiEncounter[6] >> m_auiEncounter[7]
                >> m_auiEncounter[8] >> m_auiEncounter[9] >> m_auiRuneState[0] >> m_auiRuneState[1]
                >> m_auiRuneState[2] >> m_auiRuneState[3] >> m_auiRuneState[4] >> m_auiRuneState[5]
                >> m_auiRuneState[6];

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
        void DoSpawnMajordomoIfCan(bool bByPlayerEnter)
        {
            // If both Majordomo and Ragnaros events are finished, return
            if (m_auiEncounter[TYPE_MAJORDOMO] == DONE && m_auiEncounter[TYPE_RAGNAROS] == DONE)
            {
                return;
            }

            // If already spawned return
            if (GetSingleCreatureFromStorage(NPC_MAJORDOMO, true))
            {
                return;
            }

            // Check if all rune bosses are done
            for (uint8 i = TYPE_MAGMADAR; i < TYPE_MAJORDOMO; ++i)
            {
                if (m_auiEncounter[i] != SPECIAL)
                {
                    return;
                }
            }

            Player* pPlayer = GetPlayerInMap();
            if (!pPlayer)
            {
                return;
            }

            // Summon Majordomo
            // If Majordomo encounter isn't done, summon at encounter place, else near Ragnaros
            uint8 uiSummonPos = m_auiEncounter[TYPE_MAJORDOMO] == DONE ? 1 : 0;
            if (Creature* pMajordomo = pPlayer->SummonCreature(m_aMajordomoLocations[uiSummonPos].m_uiEntry, m_aMajordomoLocations[uiSummonPos].m_fX, m_aMajordomoLocations[uiSummonPos].m_fY, m_aMajordomoLocations[uiSummonPos].m_fZ, m_aMajordomoLocations[uiSummonPos].m_fO, TEMPSUMMON_MANUAL_DESPAWN, 2 * HOUR * IN_MILLISECONDS))
            {
                if (uiSummonPos)                                    // Majordomo encounter already done, set faction
                {
                    pMajordomo->SetFactionTemporary(FACTION_MAJORDOMO_FRIENDLY, TEMPFACTION_RESTORE_RESPAWN);
                    pMajordomo->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_OOC_NOT_ATTACKABLE);
                    pMajordomo->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
                }
                else                                                // Else yell and summon adds
                {
                    if (!bByPlayerEnter)
                    {
                        DoScriptText(SAY_MAJORDOMO_SPAWN, pMajordomo);
                    }

                    for (uint8 i = 0; i < MAX_MAJORDOMO_ADDS; ++i)
                    {
                        pMajordomo->SummonCreature(m_aBosspawnLocs[i].m_uiEntry, m_aBosspawnLocs[i].m_fX, m_aBosspawnLocs[i].m_fY, m_aBosspawnLocs[i].m_fZ, m_aBosspawnLocs[i].m_fO, TEMPSUMMON_MANUAL_DESPAWN, DAY * IN_MILLISECONDS);
                    }
                }
            }
        }

        sRuneEncounters const *GetRuneStructForBoss(uint32 uiType) const
        {
            for (int i = 0; i < MAX_MOLTEN_RUNES; ++i)
                if (m_aMoltenCoreRunes[i].m_bossType == uiType)
                    return &m_aMoltenCoreRunes[i];

            return NULL;
        }

        sRuneEncounters const *GetRuneStructForTrapEntry(uint32 entry) const
        {
            for (int i = 0; i < MAX_MOLTEN_RUNES; ++i)
            if (m_aMoltenCoreRunes[i].m_uiTrapEntry == entry)
                return &m_aMoltenCoreRunes[i];

            return NULL;
        }

        std::string m_strInstData;
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        uint32 m_auiRuneState[MAX_MOLTEN_RUNES];
        std::set<ObjectGuid> m_sFireswornGUID;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_molten_core(pMap);
    }
};

void AddSC_instance_molten_core()
{
    Script* s;
    s = new is_molten_core();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_molten_core";
    //pNewScript->GetInstanceData = &GetInstance_instance_molten_core;
    //pNewScript->RegisterSelf();
}
