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
 * SDName:      Stonetalon_Mountains
 * SD%Complete: 100
 * SDComment:   Quest support: 1090, 6523.
 * SDCategory:  Stonetalon Mountains
 * EndScriptData
 */

/**
 * ContentData
 * npc_kaya
 * npc_piznik
 * EndContentData
 */

#include "precompiled.h"
#include "escort_ai.h"

/*######
## npc_kaya
######*/

enum
{
    NPC_GRIMTOTEM_RUFFIAN       = 11910,
    NPC_GRIMTOTEM_BRUTE         = 11912,
    NPC_GRIMTOTEM_SORCERER      = 11913,

    SAY_START                   = -1000357,
    SAY_AMBUSH                  = -1000358,
    SAY_END                     = -1000359,

    QUEST_PROTECT_KAYA          = 6523
};

struct npc_kaya : public CreatureScript
{
    npc_kaya() : CreatureScript("npc_kaya") {}

    struct npc_kayaAI : public npc_escortAI
    {
        npc_kayaAI(Creature* pCreature) : npc_escortAI(pCreature) { }

        void Reset() override { }

        void JustSummoned(Creature* pSummoned) override
        {
            pSummoned->AI()->AttackStart(m_creature);
        }

        void WaypointReached(uint32 uiPointId) override
        {
            switch (uiPointId)
            {
                // Ambush
            case 16:
                // note about event here:
                // apparently NPC say _after_ the ambush is over, and is most likely a bug at you-know-where.
                // we simplify this, and make say when the ambush actually start.
                DoScriptText(SAY_AMBUSH, m_creature);
                m_creature->SummonCreature(NPC_GRIMTOTEM_RUFFIAN, -50.75f, -500.77f, -46.13f, 0.4f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000);
                m_creature->SummonCreature(NPC_GRIMTOTEM_BRUTE, -40.05f, -510.89f, -46.05f, 1.7f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000);
                m_creature->SummonCreature(NPC_GRIMTOTEM_SORCERER, -32.21f, -499.20f, -45.35f, 2.8f, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 30000);
                break;
                // Award quest credit
            case 18:
                DoScriptText(SAY_END, m_creature);

                if (Player* pPlayer = GetPlayerForEscort())
                {
                    pPlayer->GroupEventHappens(QUEST_PROTECT_KAYA, m_creature);
                }
                break;
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_kayaAI(pCreature);
    }

    bool OnQuestAccept(Player* pPlayer, Creature* pCreature, Quest const* pQuest) override
    {
        // Casting Spell and Starting the Escort quest is buggy, so this is a hack. Use the spell when it is possible.

        if (pQuest->GetQuestId() == QUEST_PROTECT_KAYA)
        {
            pCreature->SetFactionTemporary(FACTION_ESCORT_H_PASSIVE, TEMPFACTION_RESTORE_RESPAWN);
            DoScriptText(SAY_START, pCreature);

            if (npc_kayaAI* pEscortAI = dynamic_cast<npc_kayaAI*>(pCreature->AI()))
            {
                pEscortAI->Start(false, pPlayer, pQuest);
            }
            return true;
        }
        return false;
    }
};

/*#####
## npc_piznik
#####*/

enum
{
    QUEST_GERENZOS_ORDERS      = 1090,

    NPC_WINDSHEAR_VERMIN       = 3998,
    NPC_WINDSHEAR_TUNNEL_RAT   = 4001,
    NPC_WINDSHEAR_STONECUTTER  = 4002,
    NPC_WINDSHEAR_GEOMANCER    = 4003,
    NPC_WINDSHEAR_OVERLORD     = 4004,
    FACTION_QUEST              = 495
};

struct SpawnPointstone
{
    float fX;
    float fY;
    float fZ;
    float fO;
};

SpawnPointstone SpawnPointsst[] =  // Set Spawnpoints for Waves
{
    { 938.30f, -257.35f, -2.22f, 6.2f},
    { 942.28f, -254.49f, -2.38f, 6.2f},
    { 938.52f, -253.20f, -2.08f, 6.2f},
    { 939.51f, -253.11f, -2.02f, 6.2f}
};

struct MovePointStone
{
    float fX;
    float fY;
    float fZ;
};

MovePointStone MovePointsst[] =   // // Set Movementpoints for Waves
{
    { 957.69f, -256.17f, -3.61f},
    { 957.12f, -260.06f, -4.68f},
    { 958.76f, -257.87f, -4.28f},
    { 953.72f, -257.25f, -3.38f}
};


struct npc_piznik : public CreatureScript
{
    npc_piznik() : CreatureScript("npc_piznik") {}

    struct npc_piznikAI : public ScriptedAI
    {
        npc_piznikAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_uiPlayerGUID = 0;    //TODO check this - to Reset()?
            m_bEventStarted = false;
            m_uiWaveTimer = 60000;  // One minute until first Wave
            m_uiEventTimer = 420000; // 7 minutes Event time
            m_uiSummonCount = 0;
            m_uiNormFaction = pCreature->getFaction();
            m_uiWaveCount = 0;
            m_uiMoveCount = 0;
        }

        void Reset()
        {
        }

        uint64 m_uiPlayerGUID;
        uint32 m_uiWaveTimer;
        uint32 m_uiEventTimer;
        uint32 m_uiSummonCount;
        uint32 m_uiNormFaction;
        uint8  m_uiWaveCount;
        uint8  m_uiMoveCount;
        bool   m_bEventStarted;

        void StartEvent(uint64 uiPlayerGUID)
        {
            m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PASSIVE);
            m_creature->setFaction(FACTION_QUEST);

            m_uiPlayerGUID = uiPlayerGUID;
            m_bEventStarted = true;
            m_uiWaveTimer = 60000;  // One minute until first Wave
            m_uiEventTimer = 420000; // 7 minutes Event time
            m_uiSummonCount = 0;
            m_uiWaveCount = 0;
            m_uiMoveCount = 0;
        }

        void FinishEvent()
        {
            m_uiPlayerGUID = 0;
            m_bEventStarted = false;
            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
            m_creature->setFaction(m_uiNormFaction);
        }

        void JustDied(Unit* /*pKiller*/)
        {
            if (Player* pPlayer = (m_creature->GetMap()->GetPlayer(m_uiPlayerGUID)))
            {
                pPlayer->SendQuestFailed(QUEST_GERENZOS_ORDERS);
            }

            FinishEvent();
        }

        void JustSummoned(Creature* pSummoned)
        {
            ++m_uiSummonCount;

            // Get waypoint for each creature
            pSummoned->GetMotionMaster()->MovePoint(0, MovePointsst[m_uiMoveCount].fX, MovePointsst[m_uiMoveCount].fY, MovePointsst[m_uiMoveCount].fZ);

            ++m_uiMoveCount;
        }

        void SummonedCreatureJustDied(Creature* /*pKilled*/)
        {
            --m_uiSummonCount;
        }

        void UpdateAI(const uint32 uiDiff)
        {

            if (m_uiEventTimer < uiDiff)  //Event should be completed after 7 minutes even if waves are alive
            {
                if (Player* pPlayer = (m_creature->GetMap()->GetPlayer(m_uiPlayerGUID)))
                {
                    pPlayer->GroupEventHappens(QUEST_GERENZOS_ORDERS, m_creature);
                }

                FinishEvent();
            }


            if (m_bEventStarted && !m_uiSummonCount)
            {

                if (m_uiWaveTimer < uiDiff && m_uiWaveCount <= 2)
                {
                    switch (m_uiWaveCount)
                    {

                    case 0:
                        m_creature->SummonCreature(NPC_WINDSHEAR_VERMIN, SpawnPointsst[2].fX, SpawnPointsst[2].fY, SpawnPointsst[2].fZ, SpawnPointsst[2].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_TUNNEL_RAT, SpawnPointsst[0].fX, SpawnPointsst[0].fY, SpawnPointsst[0].fZ, SpawnPointsst[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_VERMIN, SpawnPointsst[1].fX, SpawnPointsst[1].fY, SpawnPointsst[1].fZ, SpawnPointsst[1].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 45000;
                        m_uiMoveCount = 0;
                        break;
                    case 1:
                        m_creature->SummonCreature(NPC_WINDSHEAR_GEOMANCER, SpawnPointsst[2].fX, SpawnPointsst[2].fY, SpawnPointsst[2].fZ, SpawnPointsst[2].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_STONECUTTER, SpawnPointsst[0].fX, SpawnPointsst[0].fY, SpawnPointsst[0].fZ, SpawnPointsst[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_GEOMANCER, SpawnPointsst[1].fX, SpawnPointsst[1].fY, SpawnPointsst[1].fZ, SpawnPointsst[1].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_STONECUTTER, SpawnPointsst[3].fX, SpawnPointsst[3].fY, SpawnPointsst[3].fZ, SpawnPointsst[3].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiWaveTimer = 45000;
                        m_uiMoveCount = 0;
                        break;
                    case 2:
                        m_creature->SummonCreature(NPC_WINDSHEAR_TUNNEL_RAT, SpawnPointsst[2].fX, SpawnPointsst[2].fY, SpawnPointsst[2].fZ, SpawnPointsst[2].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_OVERLORD, SpawnPointsst[0].fX, SpawnPointsst[0].fY, SpawnPointsst[0].fZ, SpawnPointsst[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_TUNNEL_RAT, SpawnPointsst[1].fX, SpawnPointsst[1].fY, SpawnPointsst[1].fZ, SpawnPointsst[1].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_creature->SummonCreature(NPC_WINDSHEAR_TUNNEL_RAT, SpawnPointsst[3].fX, SpawnPointsst[3].fY, SpawnPointsst[3].fZ, SpawnPointsst[3].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 20000);
                        m_uiMoveCount = 0;
                        break;
                    }

                    ++m_uiWaveCount;
                }
                else
                {
                    m_uiWaveTimer -= uiDiff;
                }
                m_uiEventTimer -= uiDiff;
            }

            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            DoMeleeAttackIfReady();
        }
    };

    bool OnQuestAccept(Player* pPlayer, Creature* pCreature, const Quest* pQuest) override
    {
        if (pQuest->GetQuestId() == QUEST_GERENZOS_ORDERS)
        {
            if (npc_piznikAI* ppiznikAI = dynamic_cast<npc_piznikAI*>(pCreature->AI()))
            {
                ppiznikAI->StartEvent(pPlayer->GetObjectGuid());
            }
        }
        return true;
    }

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_piznikAI(pCreature);
    }
};

/*######
## AddSC
######*/

void AddSC_stonetalon_mountains()
{
    Script* s;
    s = new npc_kaya();
    s->RegisterSelf();
    s = new npc_piznik();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_kaya";
    //pNewScript->GetAI = &GetAI_npc_kaya;
    //pNewScript->pQuestAcceptNPC = &QuestAccept_npc_kaya;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_piznik";
    //pNewScript->GetAI = &GetAI_npc_piznik;
    //pNewScript->pQuestAcceptNPC = &QuestAccept_npc_piznik;
    //pNewScript->RegisterSelf();
}
