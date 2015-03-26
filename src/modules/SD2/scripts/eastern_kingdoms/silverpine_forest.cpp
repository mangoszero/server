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
 * SDName:      Silverpine_Forest
 * SD%Complete: 100
 * SDComment:   Quest support: 435, 452.
 * SDCategory:  Silverpine Forest
 * EndScriptData
 */

/**
 * ContentData
 * npc_deathstalker_erland
 * npc_deathstalker_faerleia
 * EndContentData
 */

#include "precompiled.h"
#include "escort_ai.h"

/*#####
## npc_deathstalker_erland
#####*/

enum
{
    SAY_START_1         = -1000306,
    SAY_START_2         = -1000307,
    SAY_AGGRO_1         = -1000308,
    SAY_AGGRO_2         = -1000309,
    SAY_AGGRO_3         = -1000310,
    SAY_PROGRESS        = -1000311,
    SAY_END             = -1000312,
    SAY_RANE            = -1000313,
    SAY_RANE_REPLY      = -1000314,
    SAY_CHECK_NEXT      = -1000315,
    SAY_QUINN           = -1000316,
    SAY_QUINN_REPLY     = -1000317,
    SAY_BYE             = -1000318,

    QUEST_ERLAND        = 435,
    NPC_RANE            = 1950,
    NPC_QUINN           = 1951,

    PHASE_RANE          = 0,
    PHASE_QUINN         = 1

};

struct npc_deathstalker_erlandAI : public npc_escortAI
{
    npc_deathstalker_erlandAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        lCreatureList.clear();
        m_uiPhase = 0;
        m_uiPhaseCounter = 0;
        Reset();

    }
    std::list<Creature*> lCreatureList;

    uint32 m_uiPhase;
    uint32 m_uiPhaseCounter;
    uint32 m_uiGlobalTimer;


    void MoveInLineOfSight(Unit* pWho) override
    {
        if (HasEscortState(STATE_ESCORT_ESCORTING) && (pWho->GetEntry() == NPC_QUINN || NPC_RANE))
        {
            lCreatureList.push_back((Creature*)pWho);
        }

        npc_escortAI::MoveInLineOfSight(pWho);
    }

    Creature* GetCreature(uint32 uiCreatureEntry)
    {
        if (!lCreatureList.empty())
        {
            for (std::list<Creature*>::iterator itr = lCreatureList.begin(); itr != lCreatureList.end(); ++itr)
            {
                if ((*itr)->GetEntry() == uiCreatureEntry && (*itr)->IsAlive())
                {
                    return (*itr);
                }
            }
        }

        return NULL;
    }


    void WaypointReached(uint32 i) override
    {
        Player* pPlayer = GetPlayerForEscort();

        if (!pPlayer)
        {
            return;
        }

        switch (i)
        {
            case 0:
                DoScriptText(SAY_START_2, m_creature, pPlayer);
                break;
            case 13:
                switch (urand(0, 1))
                {
                    case 0:
                        DoScriptText(SAY_END, m_creature, pPlayer);
                        break;
                    case 1:
                        DoScriptText(SAY_PROGRESS, m_creature);
                        break;
                }
                pPlayer->GroupEventHappens(QUEST_ERLAND, m_creature);
                m_creature->SetWalk(false);
                break;
            case 16:
                m_creature->SetWalk(true);
                SetEscortPaused(true);
                break;
            case 25:
                SetEscortPaused(true);
                break;
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            if (HasEscortState(STATE_ESCORT_PAUSED))
            {
                if (m_uiGlobalTimer < uiDiff)
                {
                    m_uiGlobalTimer = 5000;

                    switch (m_uiPhase)
                    {
                        case PHASE_RANE:
                        {
                            switch (m_uiPhaseCounter)
                            {
                                case 0:
                                    if (Creature* pRane = GetCreature(NPC_RANE))
                                    {
                                        DoScriptText(SAY_RANE, pRane);
                                    }
                                    break;
                                case 1:
                                    DoScriptText(SAY_RANE_REPLY, m_creature);
                                    break;
                                case 2:
                                    DoScriptText(SAY_CHECK_NEXT, m_creature);
                                    break;
                                case 3:
                                    SetEscortPaused(false);
                                    m_uiPhase = PHASE_QUINN;
                                    break;
                            }
                            break;
                        }
                        case PHASE_QUINN:
                        {
                            switch (m_uiPhaseCounter)
                            {
                                case 4:
                                    DoScriptText(SAY_QUINN, m_creature);
                                    break;
                                case 5:
                                    if (Creature* pQuinn = GetCreature(NPC_QUINN))
                                    {
                                        DoScriptText(SAY_QUINN_REPLY, pQuinn);
                                    }
                                    break;
                                case 6:
                                    DoScriptText(SAY_BYE, m_creature);
                                    break;
                                case 7:
                                    SetEscortPaused(false);
                                    break;
                            }
                            break;
                        }
                    }
                    ++m_uiPhaseCounter;
                }
                else
                {
                    m_uiGlobalTimer -= uiDiff;
                }
            }

            return;
        }

        DoMeleeAttackIfReady();
    }



    void Reset() override {}

    void Aggro(Unit* pWho) override
    {
        switch (urand(0, 2))
        {
            case 0:
                DoScriptText(SAY_AGGRO_1, m_creature, pWho);
                break;
            case 1:
                DoScriptText(SAY_AGGRO_2, m_creature);
                break;
            case 2:
                DoScriptText(SAY_AGGRO_3, m_creature, pWho);
                break;
        }
    }
};

bool QuestAccept_npc_deathstalker_erland(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_ERLAND)
    {
        DoScriptText(SAY_START_1, pCreature);

        if (npc_deathstalker_erlandAI* pEscortAI = dynamic_cast<npc_deathstalker_erlandAI*>(pCreature->AI()))
        {
            pEscortAI->Start(false, pPlayer, pQuest);
        }
    }
    return true;
}

CreatureAI* GetAI_npc_deathstalker_erland(Creature* pCreature)
{
    return new npc_deathstalker_erlandAI(pCreature);
}

/*#####
## npc_deathstalker_faerleia
#####*/

enum
{
    QUEST_PYREWOOD_AMBUSH      = 452,

    // cast it after every wave
    SPELL_DRINK_POTION         = 3359,

    SAY_START                  = -1000553,
    SAY_COMPLETED              = -1000554,

    // 1st wave
    NPC_COUNCILMAN_SMITHERS    = 2060,
    // 2nd wave
    NPC_COUNCILMAN_THATHER     = 2061,
    NPC_COUNCILMAN_HENDRICKS   = 2062,
    // 3rd wave
    NPC_COUNCILMAN_WILHELM     = 2063,
    NPC_COUNCILMAN_HARTIN      = 2064,
    NPC_COUNCILMAN_HIGARTH     = 2066,
    // final wave
    NPC_COUNCILMAN_COOPER      = 2065,
    NPC_COUNCILMAN_BRUNSWICK   = 2067,
    NPC_LORD_MAYOR_MORRISON    = 2068
};

struct SpawnPoint
{
    float fX;
    float fY;
    float fZ;
    float fO;
};

SpawnPoint SpawnPoints[] =
{
    { -397.39f, 1509.78f, 18.87f, 4.73f},
    { -396.30f, 1511.68f, 18.87f, 4.76f},
    { -398.26f, 1511.56f, 18.87f, 4.74f}
};

struct MovePoints
{
    float fX;
    float fY;
    float fZ;
};

MovePoints MovePointspy[] =   // Set Movementpoints for Waves
{
    { -396.97f, 1494.43f, 19.77f},
    { -396.21f, 1495.97f, 19.77f},
    { -398.30f, 1495.97f, 19.77f}
};


struct npc_deathstalker_faerleiaAI : public ScriptedAI
{
    npc_deathstalker_faerleiaAI(Creature* pCreature) : ScriptedAI(pCreature) {Reset();}

    void Reset()
    {
    }

    uint64 m_uiPlayerGUID;
    uint32 m_uiWaveTimer;
    uint32 m_uiSummonCount;
    uint32 m_uiRunbackTimer;
    uint8  m_uiWaveCount;
    uint8  m_uiMoveCount;
    bool   m_bEventStarted;
    bool   m_bWaveDied;

    void JustRespawned()
    {
        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER); // Reseting flags on respawn in case questgiver died durin event
        Reset();
    }


    void StartEvent(uint64 uiPlayerGUID)
    {
        m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);

        m_uiPlayerGUID   = uiPlayerGUID;
        m_bEventStarted  = true;
        m_bWaveDied      = false;
        m_uiWaveTimer    = 10000;
        m_uiSummonCount  = 0;
        m_uiWaveCount    = 0;
        m_uiRunbackTimer = 0;
        m_uiMoveCount    = 0;
    }

    void FinishEvent()
    {
        m_uiPlayerGUID = 0;
        m_bEventStarted = false;
        m_bWaveDied = false;
    }

    void JustDied(Unit* /*pKiller*/)
    {
        if (Player* pPlayer = (m_creature->GetMap()->GetPlayer(m_uiPlayerGUID)))
        {
            pPlayer->SendQuestFailed(QUEST_PYREWOOD_AMBUSH);
        }

        FinishEvent();
    }

    void JustSummoned(Creature* pSummoned)
    {
        ++m_uiSummonCount;

        // Get waypoint for each creature
        pSummoned->GetMotionMaster()->MovePoint(0, MovePointspy[m_uiMoveCount].fX, MovePointspy[m_uiMoveCount].fY, MovePointspy[m_uiMoveCount].fZ);

        ++m_uiMoveCount;
    }

    void SummonedCreatureJustDied(Creature* /*pKilled*/)
    {
        --m_uiSummonCount;

        if (!m_uiSummonCount)
        {
            m_uiRunbackTimer = 3000;  //without timer creature is in evade state running to start point and do not drink potion
            m_bWaveDied = true;
        }
    }

    void UpdateAI(const uint32 uiDiff)
    {
        if (m_bEventStarted && m_bWaveDied && m_uiRunbackTimer < uiDiff && m_uiWaveCount == 4)
        {
            DoScriptText(SAY_COMPLETED, m_creature);
            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            if (Player* pPlayer = (m_creature->GetMap()->GetPlayer(m_uiPlayerGUID)))
            {
                pPlayer->GroupEventHappens(QUEST_PYREWOOD_AMBUSH, m_creature);
            }

            FinishEvent();
        }


        if (m_bEventStarted && m_bWaveDied && m_uiRunbackTimer < uiDiff && m_uiWaveCount <= 3)
        {
            DoCastSpellIfCan(m_creature, SPELL_DRINK_POTION);
            m_bWaveDied = false;
        }


        if (m_bEventStarted && !m_uiSummonCount)
        {
            if (m_uiWaveTimer < uiDiff)
            {
                switch (m_uiWaveCount)
                {
                    case 0:
                        m_creature->SummonCreature(NPC_COUNCILMAN_SMITHERS,  SpawnPoints[0].fX, SpawnPoints[0].fY, SpawnPoints[0].fZ, SpawnPoints[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 10000;
                        m_uiMoveCount = 0;
                        break;
                    case 1:
                        m_creature->SummonCreature(NPC_COUNCILMAN_THATHER,   SpawnPoints[2].fX, SpawnPoints[2].fY, SpawnPoints[2].fZ, SpawnPoints[2].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_HENDRICKS, SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 10000;
                        m_uiMoveCount = 0;
                        break;
                    case 2:
                        m_creature->SummonCreature(NPC_COUNCILMAN_HARTIN,    SpawnPoints[0].fX, SpawnPoints[0].fY, SpawnPoints[0].fZ, SpawnPoints[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_WILHELM,   SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_HIGARTH,   SpawnPoints[2].fX, SpawnPoints[2].fY, SpawnPoints[2].fZ, SpawnPoints[2].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 8000;
                        m_uiMoveCount = 0;
                        break;
                    case 3:
                        m_creature->SummonCreature(NPC_LORD_MAYOR_MORRISON,  SpawnPoints[0].fX, SpawnPoints[0].fY, SpawnPoints[0].fZ, SpawnPoints[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_COOPER,    SpawnPoints[1].fX, SpawnPoints[1].fY, SpawnPoints[1].fZ, SpawnPoints[1].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_COUNCILMAN_BRUNSWICK, SpawnPoints[2].fX, SpawnPoints[2].fY, SpawnPoints[2].fZ, SpawnPoints[2].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiMoveCount = 0;
                        break;
                    case 4:
                        m_uiRunbackTimer -= uiDiff;
                        return;
                }

                ++m_uiWaveCount;
            }
            else
            {
                m_uiWaveTimer -= uiDiff;
            }

            m_uiRunbackTimer -= uiDiff;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        DoMeleeAttackIfReady();
    }
};

bool QuestAccept_npc_deathstalker_faerleia(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_PYREWOOD_AMBUSH)
    {
        DoScriptText(SAY_START, pCreature, pPlayer);

        if (npc_deathstalker_faerleiaAI* pFaerleiaAI = dynamic_cast<npc_deathstalker_faerleiaAI*>(pCreature->AI()))
        {
            pFaerleiaAI->StartEvent(pPlayer->GetObjectGuid());
        }
    }
    return true;
}

CreatureAI* GetAI_npc_deathstalker_faerleia(Creature* pCreature)
{
    return new npc_deathstalker_faerleiaAI(pCreature);
}

void AddSC_silverpine_forest()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "npc_deathstalker_erland";
    pNewScript->GetAI = &GetAI_npc_deathstalker_erland;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_deathstalker_erland;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_deathstalker_faerleia";
    pNewScript->GetAI = &GetAI_npc_deathstalker_faerleia;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_deathstalker_faerleia;
    pNewScript->RegisterSelf();
}
