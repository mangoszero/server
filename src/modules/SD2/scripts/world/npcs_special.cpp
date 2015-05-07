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
 * SDName:      Npcs_Special
 * SD%Complete: 0
 * SDComment:   To be used for special NPCs that are located globally.
 * SDCategory:  NPCs
 * EndScriptData
 */

#include "precompiled.h"
#include "escort_ai.h"
#include "ObjectMgr.h"
#include "GameEventMgr.h"

/**
 * ContentData
 * npc_chicken_cluck          100%    support for quest 3861 (Cluck!)
 * npc_garments_of_quests     100%    NPC's related to all Garments of-quests 5621, 5624, 5625, 5648, 5650
 * npc_injured_patient         80%    patients for triage-quests (6622 and 6624)
 * npc_doctor                 100%    Gustaf Vanhowzen and Gregory Victor, quest 6622 and 6624 (Triage)
 * npc_innkeeper               25%    ScriptName not assigned. Innkeepers in general.
 * npc_redemption_target      100%    Used for the paladin quests: 1779, 1781
 * EndContentData
 */

/*########
# npc_chicken_cluck
#########*/

enum
{
    EMOTE_A_HELLO           = -1000204,
    EMOTE_H_HELLO           = -1000205,
    EMOTE_CLUCK_TEXT2       = -1000206,

    QUEST_CLUCK             = 3861,
    FACTION_FRIENDLY        = 35,
    FACTION_CHICKEN         = 31
};

struct npc_chicken_cluck : public CreatureScript
{
    npc_chicken_cluck() : CreatureScript("npc_chicken_cluck") {}

    struct npc_chicken_cluckAI : public ScriptedAI
    {
        npc_chicken_cluckAI(Creature* pCreature) : ScriptedAI(pCreature) {}

        uint32 m_uiResetFlagTimer;

        void Reset() override
        {
            m_uiResetFlagTimer = 120000;

            m_creature->setFaction(FACTION_CHICKEN);
            m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        }

        void ReceiveEmote(Player* pPlayer, uint32 uiEmote) override
        {
            if (uiEmote == TEXTEMOTE_CHICKEN)
            {
                if (!urand(0, 29))
                {
                    if (pPlayer->GetQuestStatus(QUEST_CLUCK) == QUEST_STATUS_NONE)
                    {
                        m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                        m_creature->setFaction(FACTION_FRIENDLY);

                        DoScriptText(EMOTE_A_HELLO, m_creature);

                        /* are there any difference in texts, after 3.x ?
                        if (pPlayer->GetTeam() == HORDE)
                        DoScriptText(EMOTE_H_HELLO, m_creature);
                        else
                        DoScriptText(EMOTE_A_HELLO, m_creature);
                        */
                    }
                }
            }

            if (uiEmote == TEXTEMOTE_CHEER)
            {
                if (pPlayer->GetQuestStatus(QUEST_CLUCK) == QUEST_STATUS_COMPLETE)
                {
                    m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                    m_creature->setFaction(FACTION_FRIENDLY);
                    DoScriptText(EMOTE_CLUCK_TEXT2, m_creature);
                }
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            // Reset flags after a certain time has passed so that the next player has to start the 'event' again
            if (m_creature->HasFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER))
            {
                if (m_uiResetFlagTimer < uiDiff)
                {
                    EnterEvadeMode();
                }
                else
                {
                    m_uiResetFlagTimer -= uiDiff;
                }
            }

            if (m_creature->SelectHostileTarget() && m_creature->getVictim())
            {
                DoMeleeAttackIfReady();
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_chicken_cluckAI(pCreature);
    }

    bool OnQuestAccept(Player* /*pPlayer*/, Creature* pCreature, const Quest* pQuest) override
    {
        if (pQuest->GetQuestId() == QUEST_CLUCK)
        {
            if (npc_chicken_cluckAI* pChickenAI = dynamic_cast<npc_chicken_cluckAI*>(pCreature->AI()))
            {
                pChickenAI->Reset();
            }
        }

        return true;
    }

    bool OnQuestRewarded(Player* /*pPlayer*/, Creature* pCreature, const Quest* pQuest) override
    {
        if (pQuest->GetQuestId() == QUEST_CLUCK)
        {
            if (npc_chicken_cluckAI* pChickenAI = dynamic_cast<npc_chicken_cluckAI*>(pCreature->AI()))
            {
                pChickenAI->Reset();
            }
        }

        return true;
    }
};

/*######
## Triage quest
######*/

enum
{
    SAY_DOC1                    = -1000201,
    SAY_DOC2                    = -1000202,
    SAY_DOC3                    = -1000203,

    QUEST_TRIAGE_H              = 6622,
    QUEST_TRIAGE_A              = 6624,

    DOCTOR_ALLIANCE             = 12939,
    DOCTOR_HORDE                = 12920,
    ALLIANCE_COORDS             = 7,
    HORDE_COORDS                = 6
};

struct Location
{
    float x, y, z, o;
};

static Location AllianceCoords[] =
{
    { -3757.38f, -4533.05f, 14.16f, 3.62f},                 // Top-far-right bunk as seen from entrance
    { -3754.36f, -4539.13f, 14.16f, 5.13f},                 // Top-far-left bunk
    { -3749.54f, -4540.25f, 14.28f, 3.34f},                 // Far-right bunk
    { -3742.10f, -4536.85f, 14.28f, 3.64f},                 // Right bunk near entrance
    { -3755.89f, -4529.07f, 14.05f, 0.57f},                 // Far-left bunk
    { -3749.51f, -4527.08f, 14.07f, 5.26f},                 // Mid-left bunk
    { -3746.37f, -4525.35f, 14.16f, 5.22f},                 // Left bunk near entrance
};

// alliance run to where
#define A_RUNTOX -3742.96f
#define A_RUNTOY -4531.52f
#define A_RUNTOZ 11.91f

static Location HordeCoords[] =
{
    { -1013.75f, -3492.59f, 62.62f, 4.34f},                 // Left, Behind
    { -1017.72f, -3490.92f, 62.62f, 4.34f},                 // Right, Behind
    { -1015.77f, -3497.15f, 62.82f, 4.34f},                 // Left, Mid
    { -1019.51f, -3495.49f, 62.82f, 4.34f},                 // Right, Mid
    { -1017.25f, -3500.85f, 62.98f, 4.34f},                 // Left, front
    { -1020.95f, -3499.21f, 62.98f, 4.34f}                  // Right, Front
};

// horde run to where
#define H_RUNTOX -1016.44f
#define H_RUNTOY -3508.48f
#define H_RUNTOZ 62.96f

const uint32 AllianceSoldierId[3] =
{
    12938,                                                  // 12938 Injured Alliance Soldier
    12936,                                                  // 12936 Badly injured Alliance Soldier
    12937                                                   // 12937 Critically injured Alliance Soldier
};

const uint32 HordeSoldierId[3] =
{
    12923,                                                  // 12923 Injured Soldier
    12924,                                                  // 12924 Badly injured Soldier
    12925                                                   // 12925 Critically injured Soldier
};

/*######
## npc_doctor (handles both Gustaf Vanhowzen and Gregory Victor)
######*/

struct npc_doctor : public CreatureScript
{
    npc_doctor() : CreatureScript("npc_doctor") {}

    struct npc_doctorAI : public ScriptedAI
    {
        npc_doctorAI(Creature* pCreature) : ScriptedAI(pCreature) { }

        ObjectGuid m_playerGuid;

        uint32 m_uiSummonPatientTimer;
        uint32 m_uiSummonPatientCount;
        uint32 m_uiPatientDiedCount;
        uint32 m_uiPatientSavedCount;

        bool m_bIsEventInProgress;

        GuidList m_lPatientGuids;
        std::vector<Location*> m_vPatientSummonCoordinates;

        void Reset() override
        {
            m_playerGuid.Clear();

            m_uiSummonPatientTimer = 10000;
            m_uiSummonPatientCount = 0;
            m_uiPatientDiedCount = 0;
            m_uiPatientSavedCount = 0;

            m_lPatientGuids.clear();
            if (!m_vPatientSummonCoordinates.empty())
            {
                for (std::vector<Location*>::iterator itr = m_vPatientSummonCoordinates.begin(); itr != m_vPatientSummonCoordinates.end(); ++itr)
                    delete (*itr);
                m_vPatientSummonCoordinates.clear();
            }

            m_bIsEventInProgress = false;

            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void BeginEvent(Player* pPlayer)
        {
            m_playerGuid = pPlayer->GetObjectGuid();

            m_uiSummonPatientTimer = 10000;
            m_uiSummonPatientCount = 0;
            m_uiPatientDiedCount = 0;
            m_uiPatientSavedCount = 0;

            switch (m_creature->GetEntry())
            {
            case DOCTOR_ALLIANCE:
                for (uint8 i = 0; i < ALLIANCE_COORDS; ++i)
                {
                    m_vPatientSummonCoordinates.push_back(new Location(AllianceCoords[i]));
                }
                break;
            case DOCTOR_HORDE:
                for (uint8 i = 0; i < HORDE_COORDS; ++i)
                {
                    m_vPatientSummonCoordinates.push_back(new Location(HordeCoords[i]));
                }
                break;
            }

            m_bIsEventInProgress = true;
            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
        }

        void PatientDied(Location* pPoint)
        {
            Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid);

            if (pPlayer && (pPlayer->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE || pPlayer->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE))
            {
                ++m_uiPatientDiedCount;

                if (m_uiPatientDiedCount > 5 && m_bIsEventInProgress)
                {
                    if (pPlayer->GetQuestStatus(QUEST_TRIAGE_A) == QUEST_STATUS_INCOMPLETE)
                    {
                        pPlayer->FailQuest(QUEST_TRIAGE_A);
                    }
                    else if (pPlayer->GetQuestStatus(QUEST_TRIAGE_H) == QUEST_STATUS_INCOMPLETE)
                    {
                        pPlayer->FailQuest(QUEST_TRIAGE_H);
                    }

                    Reset();
                    return;
                }

                m_vPatientSummonCoordinates.push_back(pPoint);
            }
            else
                // If no player or player abandon quest in progress
            {
                Reset();
            }
        }

        void PatientSaved(Creature* /*soldier*/, Player* pPlayer, Location* pPoint)
        {
            if (pPlayer && m_playerGuid == pPlayer->GetObjectGuid())
            {
                if (pPlayer->GetQuestStatus(QUEST_TRIAGE_A) == QUEST_STATUS_INCOMPLETE || pPlayer->GetQuestStatus(QUEST_TRIAGE_H) == QUEST_STATUS_INCOMPLETE)
                {
                    ++m_uiPatientSavedCount;

                    if (m_uiPatientSavedCount == 15)
                    {
                        for (GuidList::const_iterator itr = m_lPatientGuids.begin(); itr != m_lPatientGuids.end(); ++itr)
                        {
                            if (Creature* Patient = m_creature->GetMap()->GetCreature(*itr))
                            {
                                Patient->SetDeathState(JUST_DIED);
                            }
                        }

                        if (pPlayer->GetQuestStatus(QUEST_TRIAGE_A) == QUEST_STATUS_INCOMPLETE)
                        {
                            pPlayer->GroupEventHappens(QUEST_TRIAGE_A, m_creature);
                        }
                        else if (pPlayer->GetQuestStatus(QUEST_TRIAGE_H) == QUEST_STATUS_INCOMPLETE)
                        {
                            pPlayer->GroupEventHappens(QUEST_TRIAGE_H, m_creature);
                        }

                        Reset();
                        return;
                    }

                    m_vPatientSummonCoordinates.push_back(pPoint);
                }
            }
        }

        void ReceiveAIEvent(AIEventType eventType, Creature* pSender, Unit* pInvoker, uint32 /*uiMiscValue*/) override
        {
            Location *loc = new Location();
            loc->x = pSender->GetPositionX();
            loc->y = pSender->GetPositionY();
            loc->z = pSender->GetPositionZ();
            loc->o = pSender->GetOrientation();
            if (eventType == AI_EVENT_CUSTOM_A && pInvoker->GetTypeId() == TYPEID_PLAYER)
            {
                PatientSaved(pSender, pInvoker->ToPlayer(), loc);
            }
            else if (eventType == AI_EVENT_CUSTOM_B && pInvoker == pSender)
            {
                PatientDied(loc);
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (m_bIsEventInProgress && m_uiSummonPatientCount >= 20)
            {
                Reset();
                return;
            }

            if (m_bIsEventInProgress && !m_vPatientSummonCoordinates.empty())
            {
                if (m_uiSummonPatientTimer < uiDiff)
                {
                    std::vector<Location*>::iterator itr = m_vPatientSummonCoordinates.begin() + urand(0, m_vPatientSummonCoordinates.size() - 1);
                    uint32 patientEntry = 0;

                    switch (m_creature->GetEntry())
                    {
                    case DOCTOR_ALLIANCE:
                        patientEntry = AllianceSoldierId[urand(0, 2)];
                        break;
                    case DOCTOR_HORDE:
                        patientEntry = HordeSoldierId[urand(0, 2)];
                        break;
                    default:
                        script_error_log("Invalid entry for Triage doctor. Please check your database");
                        return;
                    }

                    if (Creature* Patient = m_creature->SummonCreature(patientEntry, (*itr)->x, (*itr)->y, (*itr)->z, (*itr)->o, TEMPSUMMON_TIMED_OOC_DESPAWN, 5000))
                    {
                        // 2.4.3, this flag appear to be required for client side item->spell to work (TARGET_SINGLE_FRIEND)
                        Patient->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PVP);

                        m_lPatientGuids.push_back(Patient->GetObjectGuid());

                        if (CreatureAI* pPatientAI = Patient->AI())
                        {
                            SendAIEvent(AI_EVENT_CUSTOM_A, m_creature, Patient);
                            //pPatientAI->m_doctorGuid = m_creature->GetObjectGuid();
                            //pPatientAI->m_pCoord = *itr;
                            delete (*itr);
                            m_vPatientSummonCoordinates.erase(itr);
                        }
                    }
                    m_uiSummonPatientTimer = 10000;
                    ++m_uiSummonPatientCount;
                }
                else
                {
                    m_uiSummonPatientTimer -= uiDiff;
                }
            }
        }
    };

    bool OnQuestAccept(Player* pPlayer, Creature* pCreature, const Quest* pQuest) override
    {
        if ((pQuest->GetQuestId() == QUEST_TRIAGE_A) || (pQuest->GetQuestId() == QUEST_TRIAGE_H))
        {
            if (npc_doctorAI* pDocAI = dynamic_cast<npc_doctorAI*>(pCreature->AI()))
            {
                pDocAI->BeginEvent(pPlayer);
            }
        }

        return true;
    }

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_doctorAI(pCreature);
    }
};

/*#####
## npc_injured_patient (handles all the patients, no matter Horde or Alliance)
#####*/

struct npc_injured_patient : public CreatureScript
{
    npc_injured_patient() : CreatureScript("npc_injured_patient") {}

    struct npc_injured_patientAI : public ScriptedAI
    {
        npc_injured_patientAI(Creature* pCreature) : ScriptedAI(pCreature) { }

        ObjectGuid m_doctorGuid;
        Location* m_pCoord;

        void Reset() override
        {
            m_doctorGuid.Clear();
            m_pCoord = NULL;

            // no select
            m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
            // no regen health
            m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);
            // to make them lay with face down
            m_creature->SetStandState(UNIT_STAND_STATE_DEAD);

            switch (m_creature->GetEntry())
            {
                // lower max health
            case 12923:
            case 12938:                                     // Injured Soldier
                m_creature->SetHealth(uint32(m_creature->GetMaxHealth()*.75));
                break;
            case 12924:
            case 12936:                                     // Badly injured Soldier
                m_creature->SetHealth(uint32(m_creature->GetMaxHealth()*.50));
                break;
            case 12925:
            case 12937:                                     // Critically injured Soldier
                m_creature->SetHealth(uint32(m_creature->GetMaxHealth()*.25));
                break;
            }
        }

        void SpellHit(Unit* pCaster, const SpellEntry* pSpell) override
        {
            if (pCaster->GetTypeId() == TYPEID_PLAYER && m_creature->IsAlive() && pSpell->Id == 20804)
            {
                Player* pPlayer = static_cast<Player*>(pCaster);
                if (pPlayer->GetQuestStatus(6624) == QUEST_STATUS_INCOMPLETE || pPlayer->GetQuestStatus(6622) == QUEST_STATUS_INCOMPLETE)
                {
                    if (Creature* pDoctor = m_creature->GetMap()->GetCreature(m_doctorGuid))
                    {
                        if (CreatureAI* pDocAI = pDoctor->AI())
                        {
                            SendAIEvent(AI_EVENT_CUSTOM_A, pPlayer, pDoctor);
                            //pDocAI->PatientSaved(m_creature, pPlayer, m_pCoord);
                        }
                    }
                }

                // quick boost to health to prevent death before next tick can occur
                m_creature->SetHealth(uint32(m_creature->GetMaxHealth()*0.20));

                // make not selectable
                m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                // regen health
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);
                // stand up
                m_creature->SetStandState(UNIT_STAND_STATE_STAND);

                switch (urand(0, 2))
                {
                case 0:
                    DoScriptText(SAY_DOC1, m_creature);
                    break;
                case 1:
                    DoScriptText(SAY_DOC2, m_creature);
                    break;
                case 2:
                    DoScriptText(SAY_DOC3, m_creature);
                    break;
                }

                m_creature->SetWalk(false);

                switch (m_creature->GetEntry())
                {
                case 12923:
                case 12924:
                case 12925:
                    m_creature->GetMotionMaster()->MovePoint(0, H_RUNTOX, H_RUNTOY, H_RUNTOZ);
                    break;
                case 12936:
                case 12937:
                case 12938:
                    m_creature->GetMotionMaster()->MovePoint(0, A_RUNTOX, A_RUNTOY, A_RUNTOZ);
                    break;
                }
            }
        }

        void ReceiveAIEvent(AIEventType eventType, Creature* pSender, Unit* pInvoker, uint32 /*uiMiscValue*/) override
        {
            if (eventType == AI_EVENT_CUSTOM_A && (pSender->GetEntry() == DOCTOR_ALLIANCE || pSender->GetEntry() == DOCTOR_HORDE))
            {
                if (!m_pCoord)
                    m_pCoord = new Location();
                m_pCoord->x = m_creature->GetPositionX();
                m_pCoord->y = m_creature->GetPositionY();
                m_pCoord->z = m_creature->GetPositionZ();
                m_pCoord->o = m_creature->GetOrientation();
                m_doctorGuid = pSender->GetObjectGuid();
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            // lower HP on every world tick makes it a useful counter, not officlone though
            uint32 uiHPLose = uint32(0.03f * uiDiff);
            if (m_creature->IsAlive() && m_creature->GetHealth() > 1 + uiHPLose)
            {
                m_creature->SetHealth(m_creature->GetHealth() - uiHPLose);
            }

            if (m_creature->IsAlive() && m_creature->GetHealth() <= 1 + uiHPLose)
            {
                m_creature->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_IN_COMBAT);
                m_creature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_NOT_SELECTABLE);
                m_creature->SetDeathState(JUST_DIED);
                m_creature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);

                if (Creature* pDoctor = m_creature->GetMap()->GetCreature(m_doctorGuid))
                {
                    if (CreatureAI* pDocAI = pDoctor->AI())
                    {
                        SendAIEvent(AI_EVENT_CUSTOM_B, m_creature, pDoctor);
                        //pDocAI->PatientDied(m_pCoord);
                    }
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_injured_patientAI(pCreature);
    }
};

/*######
## npc_garments_of_quests
######*/

enum
{
    SPELL_LESSER_HEAL_R2    = 2052,
    SPELL_FORTITUDE_R1      = 1243,

    QUEST_MOON              = 5621,
    QUEST_LIGHT_1           = 5624,
    QUEST_LIGHT_2           = 5625,
    QUEST_SPIRIT            = 5648,
    QUEST_DARKNESS          = 5650,

    ENTRY_SHAYA             = 12429,
    ENTRY_ROBERTS           = 12423,
    ENTRY_DOLF              = 12427,
    ENTRY_KORJA             = 12430,
    ENTRY_DG_KEL            = 12428,

    SAY_COMMON_HEALED       = -1000231,
    SAY_DG_KEL_THANKS       = -1000232,
    SAY_DG_KEL_GOODBYE      = -1000233,
    SAY_ROBERTS_THANKS      = -1000256,
    SAY_ROBERTS_GOODBYE     = -1000257,
    SAY_KORJA_THANKS        = -1000258,
    SAY_KORJA_GOODBYE       = -1000259,
    SAY_DOLF_THANKS         = -1000260,
    SAY_DOLF_GOODBYE        = -1000261,
    SAY_SHAYA_THANKS        = -1000262,
    SAY_SHAYA_GOODBYE       = -1000263,
};

struct npc_garments_of_quests : public CreatureScript
{
    npc_garments_of_quests() : CreatureScript("npc_garments_of_quests") {}

    struct npc_garments_of_questsAI : public npc_escortAI
    {
        npc_garments_of_questsAI(Creature* pCreature) : npc_escortAI(pCreature) { }

        ObjectGuid m_playerGuid;

        bool m_bIsHealed;
        bool m_bCanRun;

        uint32 m_uiRunAwayTimer;

        void Reset() override
        {
            m_playerGuid.Clear();

            m_bIsHealed = false;
            m_bCanRun = false;

            m_uiRunAwayTimer = 5000;

            m_creature->SetStandState(UNIT_STAND_STATE_KNEEL);
            // expect database to have RegenHealth=0
            m_creature->SetHealth(int(m_creature->GetMaxHealth() * 0.7));
        }

        void SpellHit(Unit* pCaster, const SpellEntry* pSpell) override
        {
            if (pSpell->Id == SPELL_LESSER_HEAL_R2 || pSpell->Id == SPELL_FORTITUDE_R1)
            {
                // not while in combat
                if (m_creature->IsInCombat())
                {
                    return;
                }

                // nothing to be done now
                if (m_bIsHealed && m_bCanRun)
                {
                    return;
                }

                if (pCaster->GetTypeId() == TYPEID_PLAYER)
                {
                    switch (m_creature->GetEntry())
                    {
                    case ENTRY_SHAYA:
                        if (((Player*)pCaster)->GetQuestStatus(QUEST_MOON) == QUEST_STATUS_INCOMPLETE)
                        {
                            if (m_bIsHealed && !m_bCanRun && pSpell->Id == SPELL_FORTITUDE_R1)
                            {
                                DoScriptText(SAY_SHAYA_THANKS, m_creature, pCaster);
                                m_bCanRun = true;
                            }
                            else if (!m_bIsHealed && pSpell->Id == SPELL_LESSER_HEAL_R2)
                            {
                                m_playerGuid = pCaster->GetObjectGuid();
                                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                                DoScriptText(SAY_COMMON_HEALED, m_creature, pCaster);
                                m_bIsHealed = true;
                            }
                        }
                        break;
                    case ENTRY_ROBERTS:
                        if (((Player*)pCaster)->GetQuestStatus(QUEST_LIGHT_1) == QUEST_STATUS_INCOMPLETE)
                        {
                            if (m_bIsHealed && !m_bCanRun && pSpell->Id == SPELL_FORTITUDE_R1)
                            {
                                DoScriptText(SAY_ROBERTS_THANKS, m_creature, pCaster);
                                m_bCanRun = true;
                            }
                            else if (!m_bIsHealed && pSpell->Id == SPELL_LESSER_HEAL_R2)
                            {
                                m_playerGuid = pCaster->GetObjectGuid();
                                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                                DoScriptText(SAY_COMMON_HEALED, m_creature, pCaster);
                                m_bIsHealed = true;
                            }
                        }
                        break;
                    case ENTRY_DOLF:
                        if (((Player*)pCaster)->GetQuestStatus(QUEST_LIGHT_2) == QUEST_STATUS_INCOMPLETE)
                        {
                            if (m_bIsHealed && !m_bCanRun && pSpell->Id == SPELL_FORTITUDE_R1)
                            {
                                DoScriptText(SAY_DOLF_THANKS, m_creature, pCaster);
                                m_bCanRun = true;
                            }
                            else if (!m_bIsHealed && pSpell->Id == SPELL_LESSER_HEAL_R2)
                            {
                                m_playerGuid = pCaster->GetObjectGuid();
                                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                                DoScriptText(SAY_COMMON_HEALED, m_creature, pCaster);
                                m_bIsHealed = true;
                            }
                        }
                        break;
                    case ENTRY_KORJA:
                        if (((Player*)pCaster)->GetQuestStatus(QUEST_SPIRIT) == QUEST_STATUS_INCOMPLETE)
                        {
                            if (m_bIsHealed && !m_bCanRun && pSpell->Id == SPELL_FORTITUDE_R1)
                            {
                                DoScriptText(SAY_KORJA_THANKS, m_creature, pCaster);
                                m_bCanRun = true;
                            }
                            else if (!m_bIsHealed && pSpell->Id == SPELL_LESSER_HEAL_R2)
                            {
                                m_playerGuid = pCaster->GetObjectGuid();
                                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                                DoScriptText(SAY_COMMON_HEALED, m_creature, pCaster);
                                m_bIsHealed = true;
                            }
                        }
                        break;
                    case ENTRY_DG_KEL:
                        if (((Player*)pCaster)->GetQuestStatus(QUEST_DARKNESS) == QUEST_STATUS_INCOMPLETE)
                        {
                            if (m_bIsHealed && !m_bCanRun && pSpell->Id == SPELL_FORTITUDE_R1)
                            {
                                DoScriptText(SAY_DG_KEL_THANKS, m_creature, pCaster);
                                m_bCanRun = true;
                            }
                            else if (!m_bIsHealed && pSpell->Id == SPELL_LESSER_HEAL_R2)
                            {
                                m_playerGuid = pCaster->GetObjectGuid();
                                m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                                DoScriptText(SAY_COMMON_HEALED, m_creature, pCaster);
                                m_bIsHealed = true;
                            }
                        }
                        break;
                    }

                    // give quest credit, not expect any special quest objectives
                    if (m_bCanRun)
                    {
                        ((Player*)pCaster)->TalkedToCreature(m_creature->GetEntry(), m_creature->GetObjectGuid());
                    }
                }
            }
        }

        void WaypointReached(uint32 /*uiPointId*/) override {}

        void UpdateEscortAI(const uint32 uiDiff) override
        {
            if (m_bCanRun && !m_creature->IsInCombat())
            {
                if (m_uiRunAwayTimer <= uiDiff)
                {
                    if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid))
                    {
                        switch (m_creature->GetEntry())
                        {
                        case ENTRY_SHAYA:
                            DoScriptText(SAY_SHAYA_GOODBYE, m_creature, pPlayer);
                            break;
                        case ENTRY_ROBERTS:
                            DoScriptText(SAY_ROBERTS_GOODBYE, m_creature, pPlayer);
                            break;
                        case ENTRY_DOLF:
                            DoScriptText(SAY_DOLF_GOODBYE, m_creature, pPlayer);
                            break;
                        case ENTRY_KORJA:
                            DoScriptText(SAY_KORJA_GOODBYE, m_creature, pPlayer);
                            break;
                        case ENTRY_DG_KEL:
                            DoScriptText(SAY_DG_KEL_GOODBYE, m_creature, pPlayer);
                            break;
                        }

                        Start(true);
                    }
                    else
                    {
                        EnterEvadeMode();    // something went wrong
                    }

                    m_uiRunAwayTimer = 30000;
                }
                else
                {
                    m_uiRunAwayTimer -= uiDiff;
                }
            }

            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_garments_of_questsAI(pCreature);
    }
};

/*######
## npc_guardian
######*/

#define SPELL_DEATHTOUCH                5

struct npc_guardian : public CreatureScript
{
    npc_guardian() : CreatureScript("npc_guardian") {}

    struct npc_guardianAI : public ScriptedAI
    {
        npc_guardianAI(Creature* pCreature) : ScriptedAI(pCreature) { }

        void Reset() override
        {
        }

        void UpdateAI(const uint32 /*diff*/) override
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            if (m_creature->isAttackReady())
            {
                m_creature->CastSpell(m_creature->getVictim(), SPELL_DEATHTOUCH, true);
                m_creature->resetAttackTimer();
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_guardianAI(pCreature);
    }
};

/*########
# npc_innkeeper
#########*/

// Script applied to all innkeepers by npcflag.
// Are there any known innkeepers that does not hape the options in the below?
// (remember gossipHello is not called unless npcflag|1 is present)

enum
{
    TEXT_ID_WHAT_TO_DO              = 1853,

    SPELL_TRICK_OR_TREAT            = 24751,                // create item or random buff
    SPELL_TRICK_OR_TREATED          = 24755,                // buff player get when tricked or treated
};

#define GOSSIP_ITEM_TRICK_OR_TREAT  "Trick or Treat!"
#define GOSSIP_ITEM_WHAT_TO_DO      "What can I do at an Inn?"

struct npc_innkeeper : public CreatureScript
{
    npc_innkeeper() : CreatureScript("npc_innkeeper") {}

    bool OnGossipHello(Player* pPlayer, Creature* pCreature) override
    {
        //pPlayer->PlayerTalkClass->ClearMenus();
        pPlayer->PrepareGossipMenu(pCreature, pPlayer->GetDefaultGossipMenuForSource(pCreature));

        if (IsHolidayActive(HOLIDAY_HALLOWS_END) && !pPlayer->HasAura(SPELL_TRICK_OR_TREATED, EFFECT_INDEX_0))
        {
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_TRICK_OR_TREAT, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        }

        // Should only apply to innkeeper close to start areas.
        if (AreaTableEntry const* pAreaEntry = GetAreaEntryByAreaID(pCreature->GetAreaId()))
        {
            // Note: this area flag doesn't exist in 1.12.1. The behavior of this gossip require additional research
            //if (pAreaEntry->flags & AREA_FLAG_LOWLEVEL)
            pPlayer->ADD_GOSSIP_ITEM(GOSSIP_ICON_CHAT, GOSSIP_ITEM_WHAT_TO_DO, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        }

        pPlayer->TalkedToCreature(pCreature->GetEntry(), pCreature->GetObjectGuid());
        pPlayer->SendPreparedGossip(pCreature);
        return true;
    }

    bool OnGossipSelect(Player* pPlayer, Creature* pCreature, uint32 /*uiSender*/, uint32 uiAction) override
    {
        pPlayer->PlayerTalkClass->ClearMenus();
        switch (uiAction)
        {
        case GOSSIP_ACTION_INFO_DEF + 1:
            pPlayer->SEND_GOSSIP_MENU(TEXT_ID_WHAT_TO_DO, pCreature->GetObjectGuid());
            break;
        case GOSSIP_ACTION_INFO_DEF + 2:
            pPlayer->CLOSE_GOSSIP_MENU();
            pCreature->CastSpell(pPlayer, SPELL_TRICK_OR_TREAT, true);
            break;
        case GOSSIP_OPTION_VENDOR:
            pPlayer->SEND_VENDORLIST(pCreature->GetObjectGuid());
            break;
        case GOSSIP_OPTION_INNKEEPER:
            pPlayer->CLOSE_GOSSIP_MENU();
            pPlayer->SetBindPoint(pCreature->GetObjectGuid());
            break;
        }

        return true;
    }
};

/*######
## npc_redemption_target
######*/

enum
{
    SAY_HEAL                    = -1000187,

    SPELL_SYMBOL_OF_LIFE        = 8593,
    SPELL_SHIMMERING_VESSEL     = 31225,    //TODO [-Zero]? no such spell
    SPELL_REVIVE_SELF           = 32343,

    NPC_FURBOLG_SHAMAN          = 17542,        // draenei side
    NPC_BLOOD_KNIGHT            = 17768,        // blood elf side
};

struct npc_redemption_target : public CreatureScript
{
    npc_redemption_target() : CreatureScript("npc_redemption_target") {}

    struct npc_redemption_targetAI : public ScriptedAI
    {
        npc_redemption_targetAI(Creature* pCreature) : ScriptedAI(pCreature) { }

        uint32 m_uiEvadeTimer;
        uint32 m_uiHealTimer;

        ObjectGuid m_playerGuid;

        void Reset() override
        {
            m_uiEvadeTimer = 0;
            m_uiHealTimer = 0;

            m_creature->SetFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);
            m_creature->SetStandState(UNIT_STAND_STATE_DEAD);
        }

        void DoReviveSelf(ObjectGuid m_guid)
        {
            // Wait until he resets again
            if (m_uiEvadeTimer)
            {
                return;
            }

            DoCastSpellIfCan(m_creature, SPELL_REVIVE_SELF);
            m_creature->SetDeathState(JUST_ALIVED);
            m_playerGuid = m_guid;
            m_uiHealTimer = 2000;
        }

        void ReceiveAIEvent(AIEventType eventType, Creature* pSender, Unit* pInvoker, uint32 /*uiMiscValue*/) override
        {
            if (eventType == AI_EVENT_CUSTOM_A && pSender == m_creature)
            {
                DoReviveSelf(pInvoker->GetObjectGuid());
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (m_uiHealTimer)
            {
                if (m_uiHealTimer <= uiDiff)
                {
                    if (Player* pPlayer = m_creature->GetMap()->GetPlayer(m_playerGuid))
                    {
                        DoScriptText(SAY_HEAL, m_creature, pPlayer);

                        // Quests 9600 and 9685 requires kill credit
                        if (m_creature->GetEntry() == NPC_FURBOLG_SHAMAN || m_creature->GetEntry() == NPC_BLOOD_KNIGHT)
                        {
                            pPlayer->KilledMonsterCredit(m_creature->GetEntry(), m_creature->GetObjectGuid());
                        }
                    }

                    m_creature->RemoveFlag(UNIT_DYNAMIC_FLAGS, UNIT_DYNFLAG_DEAD);
                    m_creature->SetStandState(UNIT_STAND_STATE_STAND);
                    m_uiHealTimer = 0;
                    m_uiEvadeTimer = 2 * MINUTE * IN_MILLISECONDS;
                }
                else
                {
                    m_uiHealTimer -= uiDiff;
                }
            }

            if (m_uiEvadeTimer)
            {
                if (m_uiEvadeTimer <= uiDiff)
                {
                    EnterEvadeMode();
                    m_uiEvadeTimer = 0;
                }
                else
                {
                    m_uiEvadeTimer -= uiDiff;
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_redemption_targetAI(pCreature);
    }
};

struct spell_npc_redemption_target : public SpellScript
{
    spell_npc_redemption_target() : SpellScript("spell_npc_redemption_target") {}

    bool EffectDummy(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pCreatureTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        // always check spellid and effectindex
        if ((uiSpellId == SPELL_SYMBOL_OF_LIFE || uiSpellId == SPELL_SHIMMERING_VESSEL) && uiEffIndex == EFFECT_INDEX_0)
        {
            if (CreatureAI* pTargetAI = pCreatureTarget->ToCreature()->AI())
            {
                pTargetAI->SendAIEvent(AI_EVENT_CUSTOM_A, pCaster, pCreatureTarget->ToCreature());//>DoReviveSelf(pCaster->GetObjectGuid());
            }

            // always return true when we are handling this spell and effect
            return true;
        }

        return false;
    }
};

void AddSC_npcs_special()
{
    Script* s;
    s = new npc_chicken_cluck();
    s->RegisterSelf();
    s = new npc_doctor();
    s->RegisterSelf();
    s = new npc_injured_patient();
    s->RegisterSelf();
    s = new npc_garments_of_quests();
    s->RegisterSelf();
    s = new npc_guardian();
    s->RegisterSelf();
    s = new npc_innkeeper();
    s->RegisterSelf(false);
    s = new npc_redemption_target();
    s->RegisterSelf();
    s = new spell_npc_redemption_target();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_chicken_cluck";
    //pNewScript->GetAI = &GetAI_npc_chicken_cluck;
    //pNewScript->pQuestAcceptNPC =   &QuestAccept_npc_chicken_cluck;
    //pNewScript->pQuestRewardedNPC = &QuestRewarded_npc_chicken_cluck;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_injured_patient";
    //pNewScript->GetAI = &GetAI_npc_injured_patient;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_doctor";
    //pNewScript->GetAI = &GetAI_npc_doctor;
    //pNewScript->pQuestAcceptNPC = &QuestAccept_npc_doctor;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_garments_of_quests";
    //pNewScript->GetAI = &GetAI_npc_garments_of_quests;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_guardian";
    //pNewScript->GetAI = &GetAI_npc_guardian;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_innkeeper";
    //pNewScript->pGossipHello = &GossipHello_npc_innkeeper;
    //pNewScript->pGossipSelect = &GossipSelect_npc_innkeeper;
    //pNewScript->RegisterSelf(false);                        // script and error report disabled, but script can be used for custom needs, adding ScriptName

    //pNewScript = new Script;
    //pNewScript->Name = "npc_redemption_target";
    //pNewScript->GetAI = &GetAI_npc_redemption_target;
    //pNewScript->pEffectDummyNPC = &EffectDummyCreature_npc_redemption_target;
    //pNewScript->RegisterSelf();
}
