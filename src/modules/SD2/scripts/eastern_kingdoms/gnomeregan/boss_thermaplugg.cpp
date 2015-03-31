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
 * SDName:      Boss_Mekgineer_Thermaplugg
 * SD%Complete: 90
 * SDComment:   Timer need improvement, especially for bomb-spawning
 * SDCategory:  Gnomeregan
 * EndScriptData
 */

#include "precompiled.h"
#include "gnomeregan.h"

enum
{
    SAY_AGGRO                           = -1090024,
    SAY_PHASE                           = -1090025,
    SAY_BOMB                            = -1090026,
    SAY_SLAY                            = -1090027,

    SPELL_ACTIVATE_BOMB_A               = 11511,            // Target Dest = -530.754 670.571 -313.784
    SPELL_ACTIVATE_BOMB_B               = 11795,            // Target Dest = -530.754 670.571 -313.784
    SPELL_KNOCK_AWAY                    = 10101,
    SPELL_KNOCK_AWAY_AOE                = 11130,
    SPELL_WALKING_BOMB_EFFECT           = 11504,
    //TODO use the spells; effects: send event + activate object; event IDs listed below
    SPELL_ACTIVATE_BOMB_01              = 11518,    //2721
    SPELL_ACTIVATE_BOMB_02              = 11521,    //2723
    SPELL_ACTIVATE_BOMB_03              = 11523,    //2724
    SPELL_ACTIVATE_BOMB_04              = 11524,    //2725
    SPELL_ACTIVATE_BOMB_05              = 11526,    //2726
    SPELL_ACTIVATE_BOMB_07              = 11527,    //2727
    SPELL_ACTIVATE_BOMB_01B             = 11796,    //2814
    SPELL_ACTIVATE_BOMB_02B             = 11797,    //2815
    SPELL_ACTIVATE_BOMB_03B             = 11798,    //2816
    SPELL_ACTIVATE_BOMB_04B             = 11799,    //2817
    SPELL_ACTIVATE_BOMB_05B             = 11800,    //2818
    SPELL_ACTIVATE_BOMB_07B             = 11801,    //2819

    NPC_WALKING_BOMB                    = 7915,
};

static const float fBombSpawnZ  = -316.2625f;

struct boss_thermaplugg : public CreatureScript
{
    boss_thermaplugg() : CreatureScript("boss_thermaplugg") {}

    struct boss_thermapluggAI : public ScriptedAI
    {
        boss_thermapluggAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            m_pInstance = (ScriptedInstance*)pCreature->GetInstanceData();
        }

        ScriptedInstance* m_pInstance;
        bool m_bIsPhaseTwo;

        uint32 m_uiKnockAwayTimer;
        uint32 m_uiActivateBombTimer;

        sBombFace m_asBombFaces[MAX_GNOME_FACES];
        float m_afSpawnPos[3];

        GuidList m_lSummonedBombGUIDs;
        GuidList m_lLandedBombGUIDs;

        void Reset() override
        {
            m_uiKnockAwayTimer = urand(17000, 20000);
            m_uiActivateBombTimer = urand(10000, 15000);
            m_bIsPhaseTwo = false;

            memset(&m_afSpawnPos, 0.0f, sizeof(m_afSpawnPos));
            m_lLandedBombGUIDs.clear();
        }

        void GetAIInformation(ChatHandler& reader) override
        {
            reader.PSendSysMessage("Thermaplugg, currently phase %s", m_bIsPhaseTwo ? "two" : "one");

            if (m_asBombFaces)
            {
                for (uint8 i = 0; i < MAX_GNOME_FACES; ++i)
                {
                    reader.PSendSysMessage("Bomb face %u is %s ", (uint32)i, m_asBombFaces[i].m_bActivated ? "activated" : "not activated");
                }
            }
        }

        void KilledUnit(Unit* /*pVictim*/) override
        {
            DoScriptText(SAY_SLAY, m_creature);
        }

        void JustDied(Unit* /*pKiller*/) override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_THERMAPLUGG, DONE);
            }

            m_lSummonedBombGUIDs.clear();
        }

        void Aggro(Unit* /*pWho*/) override
        {
            //copy faces GUIDs from instance script; BombFaces are fully controlled by this AI
            uint32 faces[MAX_GNOME_FACES] {GO_GNOME_FACE_1, GO_GNOME_FACE_2, GO_GNOME_FACE_3, GO_GNOME_FACE_4, GO_GNOME_FACE_5, GO_GNOME_FACE_6};
            for (int i = 0; i < MAX_GNOME_FACES; ++i)
            {
                m_asBombFaces[i].m_gnomeFaceGuid = ObjectGuid(m_pInstance->GetData64(faces[i]));
                m_asBombFaces[i].m_bActivated = false;
                m_asBombFaces[i].m_uiBombTimer = 0;
            }

            DoScriptText(SAY_AGGRO, m_creature);

            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_THERMAPLUGG, IN_PROGRESS);
            }

            m_afSpawnPos[0] = m_creature->GetPositionX();
            m_afSpawnPos[1] = m_creature->GetPositionY();
            m_afSpawnPos[2] = m_creature->GetPositionZ();
        }

        void JustReachedHome() override
        {
            if (m_pInstance)
            {
                m_pInstance->SetData(TYPE_THERMAPLUGG, FAIL);
            }

            // Remove remaining bombs
            for (GuidList::const_iterator itr = m_lSummonedBombGUIDs.begin(); itr != m_lSummonedBombGUIDs.end(); ++itr)
            {
                if (Creature* pBomb = m_creature->GetMap()->GetCreature(*itr))
                {
                    pBomb->ForcedDespawn();
                }
            }
            m_lSummonedBombGUIDs.clear();
        }

        void JustSummoned(Creature* pSummoned) override
        {
            if (pSummoned->GetEntry() == NPC_WALKING_BOMB)
            {
                m_lSummonedBombGUIDs.push_back(pSummoned->GetObjectGuid());
                // calculate point for falling down
                float fX, fY;
                fX = 0.2 * m_afSpawnPos[0] + 0.8 * pSummoned->GetPositionX();
                fY = 0.2 * m_afSpawnPos[1] + 0.8 * pSummoned->GetPositionY();
                pSummoned->GetMotionMaster()->MovePoint(1, fX, fY, m_afSpawnPos[2] - 2.0f);
            }
        }

        void SummonedMovementInform(Creature* pSummoned, uint32 uiMotionType, uint32 uiPointId) override
        {
            if (pSummoned->GetEntry() == NPC_WALKING_BOMB && uiMotionType == POINT_MOTION_TYPE && uiPointId == 1)
            {
                m_lLandedBombGUIDs.push_back(pSummoned->GetObjectGuid());
            }
        }

        void SummonedCreatureDespawn(Creature* pSummoned) override
        {
            m_lSummonedBombGUIDs.remove(pSummoned->GetObjectGuid());
        }

        void ReceiveAIEvent(AIEventType eventType, Creature* pSender, Unit* /*pInvoker*/, uint32 data) override
        {
            if (pSender != m_creature)  //handling only events from himself (casts of activate bomb), to be refactored
                return;

            switch (eventType)
            {
            case AI_EVENT_CUSTOM_A:
                DoActivateBombFace(data);
                break;
            case AI_EVENT_CUSTOM_B:
                DoDeactivateBombFace(data);
                break;
            case AI_EVENT_CUSTOM_C:
                for (uint8 i = 0; i < MAX_GNOME_FACES; ++i)
                    DoDeactivateBombFace(i);
                break;
            }
        }

        void UpdateAI(const uint32 uiDiff) override
        {
            if (!m_creature->SelectHostileTarget() || !m_creature->getVictim())
            {
                return;
            }

            // Movement of Summoned mobs
            if (!m_lLandedBombGUIDs.empty())
            {
                for (GuidList::const_iterator itr = m_lLandedBombGUIDs.begin(); itr != m_lLandedBombGUIDs.end(); ++itr)
                {
                    if (Creature* pBomb = m_creature->GetMap()->GetCreature(*itr))
                    {
                        pBomb->GetMotionMaster()->MoveFollow(m_creature, 0.0f, 0.0f);
                    }
                }
                m_lLandedBombGUIDs.clear();
            }

            if (!m_bIsPhaseTwo && m_creature->GetHealthPercent() < 50.0f)
            {
                DoScriptText(SAY_PHASE, m_creature);
                m_bIsPhaseTwo = true;
            }

            if (m_uiKnockAwayTimer < uiDiff)
            {
                if (m_bIsPhaseTwo)
                {
                    if (DoCastSpellIfCan(m_creature, SPELL_KNOCK_AWAY_AOE) == CAST_OK)
                    {
                        m_uiKnockAwayTimer = 12000;
                    }
                }
                else
                {
                    if (DoCastSpellIfCan(m_creature->getVictim(), SPELL_KNOCK_AWAY) == CAST_OK)
                    {
                        m_uiKnockAwayTimer = urand(17000, 20000);
                    }
                }
            }
            else
            {
                m_uiKnockAwayTimer -= uiDiff;
            }

            if (m_uiActivateBombTimer < uiDiff)
            {
                if (DoCastSpellIfCan(m_creature, m_bIsPhaseTwo ? SPELL_ACTIVATE_BOMB_B : SPELL_ACTIVATE_BOMB_A) == CAST_OK)
                {
                    m_uiActivateBombTimer = (m_bIsPhaseTwo ? urand(6, 12) : urand(12, 17)) * IN_MILLISECONDS;
                    if (!urand(0, 5))                           // TODO, chance/ place for this correct?
                    {
                        DoScriptText(SAY_BOMB, m_creature);
                    }
                }
            }
            else
            {
                m_uiActivateBombTimer -= uiDiff;
            }

            // Spawn bombs
            if (m_asBombFaces)
            {
                for (uint8 i = 0; i < MAX_GNOME_FACES; ++i)
                {
                    if (m_asBombFaces[i].m_bActivated)
                    {
                        if (m_asBombFaces[i].m_uiBombTimer < uiDiff)
                        {
                            // Calculate the spawning position as 90% between face and thermaplugg spawn-pos, and hight hardcoded
                            float fX = 0.0f, fY = 0.0f;
                            if (GameObject* pFace = m_creature->GetMap()->GetGameObject(m_asBombFaces[i].m_gnomeFaceGuid))
                            {
                                fX = 0.35 * m_afSpawnPos[0] + 0.65 * pFace->GetPositionX();
                                fY = 0.35 * m_afSpawnPos[1] + 0.65 * pFace->GetPositionY();
                            }
                            m_creature->SummonCreature(NPC_WALKING_BOMB, fX, fY, fBombSpawnZ, 0.0f, TEMPSUMMON_CORPSE_DESPAWN, 0);
                            m_asBombFaces[i].m_uiBombTimer = urand(10000, 25000);   // TODO
                        }
                        else
                        {
                            m_asBombFaces[i].m_uiBombTimer -= uiDiff;
                        }
                    }
                }
            }

            DoMeleeAttackIfReady();
        }
        private:
            void DoActivateBombFace(uint8 uiIndex)
            {
                if (uiIndex >= MAX_GNOME_FACES)
                {
                    return;
                }

                if (!m_asBombFaces[uiIndex].m_bActivated)
                {
                    m_pInstance->DoUseDoorOrButton(m_asBombFaces[uiIndex].m_gnomeFaceGuid);
                    m_asBombFaces[uiIndex].m_bActivated = true;
                    m_asBombFaces[uiIndex].m_uiBombTimer = 3000;
                }
            }

            void DoDeactivateBombFace(uint8 uiIndex)
            {
                if (uiIndex >= MAX_GNOME_FACES)
                {
                    return;
                }

                if (m_asBombFaces[uiIndex].m_bActivated)
                {
                    m_pInstance->DoUseDoorOrButton(m_asBombFaces[uiIndex].m_gnomeFaceGuid);
                    m_asBombFaces[uiIndex].m_bActivated = false;
                    m_asBombFaces[uiIndex].m_uiBombTimer = 0;
                }
            }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new boss_thermapluggAI(pCreature);
    }
};

//the following is a workaround by a wrong mechanic for missing spells (now found) TODO refactor it
struct spell_boss_thermaplugg : public SpellScript
{
    spell_boss_thermaplugg() : SpellScript("spell_boss_thermaplugg") {}

    bool EffectDummy(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* pCreatureTarget, ObjectGuid /*originalCasterGuid*/) override
    {
        if ((uiSpellId != SPELL_ACTIVATE_BOMB_A && uiSpellId != SPELL_ACTIVATE_BOMB_B) || uiEffIndex != EFFECT_INDEX_0)
        {
            return false;
        }

        // This spell should select a random Bomb-Face and activate it if needed
        if (pCaster->GetEntry() == NPC_MEKGINEER_THERMAPLUGG && pCaster->ToCreature()->AI())
            pCaster->ToCreature()->AI()->SendAIEvent(AI_EVENT_CUSTOM_A, pCaster, pCaster->ToCreature(), urand(0, MAX_GNOME_FACES - 1));

        return true;
    }
};

struct go_gnomeface_button : public GameObjectScript
{
    go_gnomeface_button() : GameObjectScript("go_gnomeface_button") {}

    bool OnUse(Player* pPlayer, GameObject* pGo) override
    {
        InstanceData* pInstance = pPlayer->GetInstanceData();
        if (!pInstance)
        {
            return false;
        }

        //below is a workaround due to missing GO 196608 definition and error in GO_BUTTON_ templates TODO fix it maybe?
        // If a button is used, the related face should be deactivated (if already activated)
        switch (pGo->GetEntry())
        {
        case GO_BUTTON_1:
            pInstance->SetData(TYPE_DO_BOMB_OFF, 0);
            break;
        case GO_BUTTON_2:
            pInstance->SetData(TYPE_DO_BOMB_OFF, 1);
            break;
        case GO_BUTTON_3:
            pInstance->SetData(TYPE_DO_BOMB_OFF, 2);
            break;
        case GO_BUTTON_4:
            pInstance->SetData(TYPE_DO_BOMB_OFF, 3);
            break;
        case GO_BUTTON_5:
            pInstance->SetData(TYPE_DO_BOMB_OFF, 4);
            break;
        case GO_BUTTON_6:
            pInstance->SetData(TYPE_DO_BOMB_OFF, 5);
            break;
        }

        return false;
    }
};

void AddSC_boss_thermaplugg()
{
    Script *s;
    s = new boss_thermaplugg();
    s->RegisterSelf();
    s = new go_gnomeface_button();
    s->RegisterSelf();
    s = new spell_boss_thermaplugg();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "boss_thermaplugg";
    //pNewScript->GetAI = &GetAI_boss_thermaplugg;
    //pNewScript->pEffectDummyNPC = &EffectDummyCreature_spell_boss_thermaplugg;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "go_gnomeface_button";
    //pNewScript->pGOUse = &GOUse_go_gnomeface_button;
    //pNewScript->RegisterSelf();
}
