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
 * SDName:      Razorfen_Downs
 * SD%Complete: 100
 * SDComment:   Quest 3525.
 * SDCategory:  Razorfen Downs
 * EndScriptData
 */

/**
 * ContentData
 * npc_belnistrasz
 * EndContentData
 */

#include "precompiled.h"
#include "razorfen_downs.h"
#include "escort_ai.h"

/*###
# npc_belnistrasz
####*/

enum
{
    QUEST_EXTINGUISHING_THE_IDOL    = 3525,

    SAY_BELNISTRASZ_READY           = -1129005,
    SAY_BELNISTRASZ_START_RIT       = -1129006,
    SAY_BELNISTRASZ_AGGRO_1         = -1129007,
    SAY_BELNISTRASZ_AGGRO_2         = -1129008,
    SAY_BELNISTRASZ_3_MIN           = -1129009,
    SAY_BELNISTRASZ_2_MIN           = -1129010,
    SAY_BELNISTRASZ_1_MIN           = -1129011,
    SAY_BELNISTRASZ_FINISH          = -1129012,

    NPC_IDOL_ROOM_SPAWNER           = 8611,

    NPC_WITHERED_BATTLE_BOAR        = 7333,
    NPC_WITHERED_QUILGUARD          = 7329,
    NPC_DEATHS_HEAD_GEOMANCER       = 7335,
    NPC_PLAGUEMAW_THE_ROTTING       = 7356,

    GO_BELNISTRASZ_BRAZIER          = 152097,

    SPELL_ARCANE_INTELLECT          = 13326,                // use this somewhere (he has it as default)
    SPELL_FIREBALL                  = 9053,
    SPELL_FROST_NOVA                = 11831,
    SPELL_IDOL_SHUTDOWN             = 12774,

    // summon spells only exist in 1.x
    // SPELL_SUMMON_1                  = 12694,             // NPC_WITHERED_BATTLE_BOAR
    // SPELL_SUMMON_2                  = 14802,             // NPC_DEATHS_HEAD_GEOMANCER
    // SPELL_SUMMON_3                  = 14801,             // NPC_WITHERED_QUILGUARD
};

static float m_fSpawnerCoord[3][4] =
{
    {2582.79f, 954.392f, 52.4821f, 3.78736f},
    {2569.42f, 956.380f, 52.2732f, 5.42797f},
    {2570.62f, 942.393f, 53.7433f, 0.71558f}
};

struct npc_belnistraszAI : public npc_escortAI
{
    npc_belnistraszAI(Creature* pCreature) : npc_escortAI(pCreature)
    {
        m_uiRitualPhase = 0;
        m_uiRitualTimer = 1000;
        m_bAggro = false;
        Reset();
    }

    uint8 m_uiRitualPhase;
    uint32 m_uiRitualTimer;
    bool m_bAggro;

    uint32 m_uiFireballTimer;
    uint32 m_uiFrostNovaTimer;

    void Reset() override
    {
        m_uiFireballTimer  = 1000;
        m_uiFrostNovaTimer = 6000;
    }

    void AttackedBy(Unit* pAttacker) override
    {
        if (HasEscortState(STATE_ESCORT_PAUSED))
        {
            if (!m_bAggro)
            {
                DoScriptText(urand(0, 1) ? SAY_BELNISTRASZ_AGGRO_1 : SAY_BELNISTRASZ_AGGRO_1, m_creature, pAttacker);
                m_bAggro = true;
            }

            return;
        }

        ScriptedAI::AttackedBy(pAttacker);
    }

    void SpawnerSummon(Creature* pSummoner)
    {
        if (m_uiRitualPhase > 7)
        {
            pSummoner->SummonCreature(NPC_PLAGUEMAW_THE_ROTTING, pSummoner->GetPositionX(), pSummoner->GetPositionY(), pSummoner->GetPositionZ(), pSummoner->GetOrientation(), TEMPSUMMON_TIMED_OOC_DESPAWN, 60000);
            return;
        }

        for (int i = 0; i < 4; ++i)
        {
            uint32 uiEntry = 0;

            // ref TARGET_RANDOM_CIRCUMFERENCE_POINT
            float angle = 2.0f * M_PI_F * rand_norm_f();
            float fX, fZ, fY;
            pSummoner->GetClosePoint(fX, fZ, fY, 0.0f, 2.0f, angle);

            switch (i)
            {
                case 0:
                case 1:
                    uiEntry = NPC_WITHERED_BATTLE_BOAR;
                    break;
                case 2:
                    uiEntry = NPC_WITHERED_QUILGUARD;
                    break;
                case 3:
                    uiEntry = NPC_DEATHS_HEAD_GEOMANCER;
                    break;
            }

            pSummoner->SummonCreature(uiEntry, fX, fZ, fY, 0.0f, TEMPSUMMON_TIMED_OOC_DESPAWN, 60000);
        }
    }

    void JustSummoned(Creature* pSummoned) override
    {
        SpawnerSummon(pSummoned);
    }

    void DoSummonSpawner(int32 iType)
    {
        m_creature->SummonCreature(NPC_IDOL_ROOM_SPAWNER, m_fSpawnerCoord[iType][0], m_fSpawnerCoord[iType][1], m_fSpawnerCoord[iType][2], m_fSpawnerCoord[iType][3], TEMPSUMMON_TIMED_DESPAWN, 10000);
    }

    void WaypointReached(uint32 uiPointId) override
    {
        if (uiPointId == 24)
        {
            DoScriptText(SAY_BELNISTRASZ_START_RIT, m_creature);
            SetEscortPaused(true);
        }
    }

    void UpdateEscortAI(const uint32 uiDiff) override
    {
        if (HasEscortState(STATE_ESCORT_PAUSED))
        {
            if (m_uiRitualTimer < uiDiff)
            {
                switch (m_uiRitualPhase)
                {
                    case 0:
                        DoCastSpellIfCan(m_creature, SPELL_IDOL_SHUTDOWN);
                        m_uiRitualTimer = 1000;
                        break;
                    case 1:
                        DoSummonSpawner(irand(1, 3));
                        m_uiRitualTimer = 39000;
                        break;
                    case 2:
                        DoSummonSpawner(irand(1, 3));
                        m_uiRitualTimer = 20000;
                        break;
                    case 3:
                        DoScriptText(SAY_BELNISTRASZ_3_MIN, m_creature, m_creature);
                        m_uiRitualTimer = 20000;
                        break;
                    case 4:
                        DoSummonSpawner(irand(1, 3));
                        m_uiRitualTimer = 40000;
                        break;
                    case 5:
                        DoSummonSpawner(irand(1, 3));
                        DoScriptText(SAY_BELNISTRASZ_2_MIN, m_creature, m_creature);
                        m_uiRitualTimer = 40000;
                        break;
                    case 6:
                        DoSummonSpawner(irand(1, 3));
                        m_uiRitualTimer = 20000;
                        break;
                    case 7:
                        DoScriptText(SAY_BELNISTRASZ_1_MIN, m_creature, m_creature);
                        m_uiRitualTimer = 40000;
                        break;
                    case 8:
                        DoSummonSpawner(irand(1, 3));
                        m_uiRitualTimer = 20000;
                        break;
                    case 9:
                        DoScriptText(SAY_BELNISTRASZ_FINISH, m_creature, m_creature);
                        m_uiRitualTimer = 3000;
                        break;
                    case 10:
                    {
                        if (Player* pPlayer = GetPlayerForEscort())
                        {
                            pPlayer->GroupEventHappens(QUEST_EXTINGUISHING_THE_IDOL, m_creature);

                            if (GameObject* pGo = GetClosestGameObjectWithEntry(m_creature, GO_BELNISTRASZ_BRAZIER, 10.0f))
                            {
                                if (!pGo->isSpawned())
                                {
                                    pGo->SetRespawnTime(HOUR * IN_MILLISECONDS);
                                    pGo->Refresh();
                                }
                            }
                        }

                        m_creature->RemoveAurasDueToSpell(SPELL_IDOL_SHUTDOWN);
                        SetEscortPaused(false);
                        break;
                    }
                }

                ++m_uiRitualPhase;
            }
            else
            {
                m_uiRitualTimer -= uiDiff;
            }

            return;
        }

        if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
        {
            return;
        }

        if (m_uiFireballTimer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_FIREBALL);
            m_uiFireballTimer  = urand(2000, 3000);
        }
        else
            { m_uiFireballTimer -= uiDiff; }

        if (m_uiFrostNovaTimer < uiDiff)
        {
            DoCastSpellIfCan(m_creature->getVictim(), SPELL_FROST_NOVA);
            m_uiFrostNovaTimer = urand(10000, 15000);
        }
        else
            { m_uiFrostNovaTimer -= uiDiff; }

        DoMeleeAttackIfReady();
    }
};

CreatureAI* GetAI_npc_belnistrasz(Creature* pCreature)
{
    return new npc_belnistraszAI(pCreature);
}

bool QuestAccept_npc_belnistrasz(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    if (pQuest->GetQuestId() == QUEST_EXTINGUISHING_THE_IDOL)
    {
        if (npc_belnistraszAI* pEscortAI = dynamic_cast<npc_belnistraszAI*>(pCreature->AI()))
        {
            pEscortAI->Start(true, pPlayer, pQuest);
            DoScriptText(SAY_BELNISTRASZ_READY, pCreature, pPlayer);
            pCreature->SetFactionTemporary(FACTION_ESCORT_N_NEUTRAL_ACTIVE, TEMPFACTION_RESTORE_RESPAWN);
        }
    }

    return true;
}

/*####
# go_tutenkash_gong
####*/

enum
{
    GO_GONG = 148917,
    NPC_TOMB_FIEND = 7349,
    TOTAL_FIENDS = 8,
    NPC_TOMB_REAVER = 7351,
    TOTAL_REAVERS = 4,
    NPC_TUTENKASH = 7355
};

struct TUTENKASH_CreatureLocation
{
    float fX, fY, fZ, fO;
};

static const TUTENKASH_CreatureLocation aCreatureLocation[] =
{
    { 2540.479f, 906.539f, 46.663f, 5.47f },               // Tomb Fiend/Reaver spawn point
    { 2541.511f, 912.857f, 46.216f, 5.39f },               // Tomb Fiend/Reaver spawn point
    { 2536.703f, 917.214f, 46.094f, 5.57f },               // Tomb Fiend/Reaver spawn point
    { 2530.443f, 913.598f, 46.083f, 5.69f },               // Tomb Fiend/Reaver spawn point
    { 2529.833f, 920.977f, 45.836f, 5.47f },               // Tomb Fiend spawn point
    { 2524.738f, 915.195f, 46.248f, 5.97f },               // Tomb Fiend spawn point
    { 2517.829f, 917.746f, 46.073f, 5.83f },               // Tomb Fiend spawn point
    { 2512.750f, 924.458f, 46.504f, 5.92f }                // Tomb Fiend spawn point
};

static const TUTENKASH_CreatureLocation aTutenkashLocation[] =
{
    { 2493.968f, 790.974f, 39.849f, 5.92f }                // Tuten'kash spawn point
};

// records which round of creatures we are in (Tomb Fiend, Tomb Raider, Boss)
int iWaveNumber = 1;
// use to kick off each wave of creatures and to prevent the event happening more than once whilst in the same instance of the dungeon
bool bWaveInMotion = false;
// keeps track of the number of craetures still alive in the wave
int iTombFiendsAlive = 8;
int iTombReaversAlive = 4;

// used for summoning multiple numbers of creatures
void SummonCreatures(Player* pPlayer, int NPC_ID, int iTotalToSpawn)
{
    // used for generating a different path for each creature
    float fXdifference = 0;
    float fYdifference = 0;

    Creature* pTombCreature = NULL;

    for (int i = 0; i < iTotalToSpawn; i++)
    {
        pTombCreature = pPlayer->SummonCreature(NPC_ID, aCreatureLocation[i].fX, aCreatureLocation[i].fY, aCreatureLocation[i].fZ, aCreatureLocation[i].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 7200000);
        pTombCreature->GetMotionMaster()->MovePoint(0, 2547.565f, 904.983f, 46.776f);
        pTombCreature->GetMotionMaster()->MovePoint(0, 2547.496f, 895.083f, 47.736f);
        pTombCreature->GetMotionMaster()->MovePoint(0, 2543.796f, 884.629f, 47.764f);
        // randomise coordinates
        fXdifference = rand() % 3;
        fYdifference = rand() % 3;
        pTombCreature->GetMotionMaster()->MovePoint(0, 2532.118f + fXdifference, 866.656f + fYdifference, 47.678146f);
        // randomise last coordinates
        fXdifference = rand() % 5;
        fYdifference = rand() % 5;
        pTombCreature->GetMotionMaster()->MovePoint(0, 2522.604f + fXdifference, 858.547f + fYdifference, 47.678673f);
        pTombCreature->GetMotionMaster()->MoveIdle();
    }
}

bool GOUse_go_tutenkash_gong(Player* pPlayer, GameObject* /*pGo*/)
{
    // gong will only spawn next wave if current wave has been wiped out
    if (!bWaveInMotion)
    {
        switch (iWaveNumber)
        {
        case 1:
            // spawn Tomb Fiends
            bWaveInMotion = true;
            SummonCreatures(pPlayer, NPC_TOMB_FIEND, TOTAL_FIENDS);
            break;
        case 2:
            // spawn Tomb Reavers
            bWaveInMotion = true;
            SummonCreatures(pPlayer, NPC_TOMB_REAVER, TOTAL_REAVERS);
            break;
        default:
            // spawn boss (Tuten'kash)
            bWaveInMotion = true; // last wave,so will never be set back to false, therefore this event cannot happen again
            Creature* pTutenkash = pPlayer->SummonCreature(NPC_TUTENKASH, aTutenkashLocation[0].fX, aTutenkashLocation[0].fY, aTutenkashLocation[0].fZ, aTutenkashLocation[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 7200000);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2488.502686f, 801.684021f, 42.731823f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2485.428955f, 815.734619f, 43.195621f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2486.951904f, 826.718079f, 43.586765f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2496.677002f, 838.880005f, 45.809792f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2501.559814f, 847.080750f, 47.408485f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2506.661377f, 855.430359f, 47.678036f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2514.890869f, 861.339966f, 47.678036f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2526.009033f, 865.386108f, 47.678036f);
            pTutenkash->GetMotionMaster()->MovePoint(0, 2539.416504f, 874.278931f, 47.711197f);
            pTutenkash->GetMotionMaster()->MoveIdle();
            break;
        }
    }

    return true;
}


// handles AI related script for the Tomb Fiends and Tomb Reavers
// - at present that is solely for the recording of the death of the creatures
struct npc_tomb_creature : public ScriptedAI
{

    npc_tomb_creature(Creature* pCreature) : ScriptedAI(pCreature)
    {
        Reset();
    }

    void Reset() override
    {
        // leaving this her for future use, just-in-case
    }

    void JustDied(Unit* /*pKiller*/) override
    {
        switch (m_creature->GetEntry())
        {
        case NPC_TOMB_FIEND:
            iTombFiendsAlive--;
            if (!iTombFiendsAlive)
            {
                bWaveInMotion = false;
                iWaveNumber = 2; // Reaver time
            }
            break;
        case NPC_TOMB_REAVER:
            iTombReaversAlive--;
            if (!iTombReaversAlive)
            {
                bWaveInMotion = false;
                iWaveNumber = 3; // boss time!!!
            }
            break;
        }
        
    }

    void UpdateAI(const uint32 /*uiDiff*/) override
    {
        // leaving this here for future use, just-in-case
    }

};


// This will count down the deaths of the mobs in each wave (Tomb Fiends and Tomb Reavers)
CreatureAI* GetAI_npc_tomb_creature(Creature* pCreature)
{
    return new npc_tomb_creature(pCreature);
}

void AddSC_razorfen_downs()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "go_tutenkash_gong";
    pNewScript->pGOUse = &GOUse_go_tutenkash_gong;
    pNewScript->RegisterSelf();
    
    pNewScript = new Script;
    pNewScript->Name = "npc_tomb_creature"; // repressents both Tomb Fiends and Tomb Reavers
    pNewScript->GetAI = &GetAI_npc_tomb_creature;
    pNewScript->RegisterSelf();
    
    pNewScript = new Script;
    pNewScript->Name = "npc_belnistrasz";
    pNewScript->GetAI = &GetAI_npc_belnistrasz;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_belnistrasz;
    pNewScript->RegisterSelf();
}
