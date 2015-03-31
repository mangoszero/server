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
 * SDName:      Sunken_Temple
 * SD%Complete: 100
 * SDComment:   Quest support: 8733.
 * SDCategory:  Sunken Temple
 * EndScriptData
 */

/**
 * ContentData
 * at_shade_of_eranikus
 * npc_malfurion_stormrage
 * event_antalarion_statue_activation
 * event_avatar_of_hakkar
 * go_eternal_flame
 * effectDummy_summon_hakkar
 * EndContentData
 */

#include "precompiled.h"
#include "sunken_temple.h"

enum
{
    QUEST_THE_CHARGE_OF_DRAGONFLIGHTS = 8555,
    QUEST_ERANIKUS_TYRANT_OF_DREAMS   = 8733
};

struct at_shade_of_eranikus : public AreaTriggerScript
{
    at_shade_of_eranikus() : AreaTriggerScript("at_shade_of_eranikus") {}

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* /*pAt*/) override
    {
        if (ScriptedInstance* pInstance = (ScriptedInstance*)pPlayer->GetInstanceData())
        {
            // Only do stuff, if the player has finished the PreQuest
            if (pPlayer->GetQuestRewardStatus(QUEST_THE_CHARGE_OF_DRAGONFLIGHTS) &&
                !pPlayer->GetQuestRewardStatus(QUEST_ERANIKUS_TYRANT_OF_DREAMS) &&
                pPlayer->GetQuestStatus(QUEST_ERANIKUS_TYRANT_OF_DREAMS) != QUEST_STATUS_COMPLETE)
            {
                if (pInstance->GetData(TYPE_MALFURION) != DONE)
                {
                    pPlayer->SummonCreature(NPC_MALFURION, aSunkenTempleLocation[2].m_fX, aSunkenTempleLocation[2].m_fY, aSunkenTempleLocation[2].m_fZ, aSunkenTempleLocation[2].m_fO, TEMPSUMMON_DEAD_DESPAWN, 0);
                    pInstance->SetData(TYPE_MALFURION, DONE);
                }
            }
        }
        return false;
    }
};

/*######
## npc_malfurion_stormrage
######*/

enum
{
    EMOTE_MALFURION1              = -1109000,
    SAY_MALFURION1                = -1109001,
    SAY_MALFURION2                = -1109002,
    SAY_MALFURION3                = -1109003,
    SAY_MALFURION4                = -1109004,

    MAX_MALFURION_TEMPLE_SPEECHES = 6
};

struct npc_malfurion : public CreatureScript
{
    npc_malfurion() : CreatureScript("npc_malfurion_stormrage") {}

    struct npc_malfurionAI : public ScriptedAI
    {
        npc_malfurionAI(Creature* pCreature) : ScriptedAI(pCreature)
        {
            // Only in Sunken Temple
            if (m_creature->GetMap()->IsDungeon())
            {
                DoScriptText(EMOTE_MALFURION1, m_creature);
                m_uiSpeech = 0;
                m_uiSayTimer = 3000;
            }

            m_creature->RemoveFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
        }

        uint32 m_uiSayTimer;
        uint32 m_uiSpeech;

        void Reset() override {}
        void UpdateAI(const uint32 uiDiff) override
        {
            // We are in Sunken Temple
            if (m_creature->GetMap()->IsDungeon())
            {
                if (m_uiSpeech < MAX_MALFURION_TEMPLE_SPEECHES)
                {
                    if (m_uiSayTimer <= uiDiff)
                    {
                        switch (m_uiSpeech)
                        {
                        case 0:
                            m_creature->HandleEmote(EMOTE_ONESHOT_BOW);
                            m_uiSayTimer = 2000;
                            break;
                        case 1:
                            DoScriptText(SAY_MALFURION1, m_creature);
                            m_creature->HandleEmote(EMOTE_STATE_TALK);
                            m_uiSayTimer = 12000;
                            break;
                        case 2:
                            DoScriptText(SAY_MALFURION2, m_creature);
                            m_uiSayTimer = 12000;
                            break;
                        case 3:
                            DoScriptText(SAY_MALFURION3, m_creature);
                            m_uiSayTimer = 11000;
                            break;
                        case 4:
                            DoScriptText(SAY_MALFURION4, m_creature);
                            m_uiSayTimer = 4000;
                            break;
                        case 5:
                            m_creature->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_QUESTGIVER);
                            m_creature->HandleEmote(EMOTE_STATE_NONE);
                            break;
                        }

                        ++m_uiSpeech;
                    }
                    else
                    {
                        m_uiSayTimer -= uiDiff;
                    }
                }
            }
        }
    };

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new npc_malfurionAI(pCreature);
    }
};

/*######
## event_antalarion_statues
######*/

struct event_antalarion_statue_activation : public MapEventScript
{
    event_antalarion_statue_activation() : MapEventScript("event_antalarion_statue_activation") {}

    bool OnReceived(uint32 uiEventId, Object* pSource, Object* pTarget, bool /*bIsStart*/) override
    {
        if (pSource->GetTypeId() == TYPEID_PLAYER && pTarget->GetTypeId() == TYPEID_GAMEOBJECT)
        {
            if (ScriptedInstance* pInstance = (ScriptedInstance*)((Player*)pSource)->GetInstanceData())
            {
                // return if event completed
                if (pInstance->GetData(TYPE_ATALARION) != NOT_STARTED)
                {
                    return true;
                }

                // Send the event id to process
                pInstance->SetData(TYPE_SIGNAL, uiEventId);
                if (pInstance->GetData(TYPE_SIGNAL))
                {
                    // Activate the green light if the correct statue is activated
                    if (GameObject* pLight = GetClosestGameObjectWithEntry((GameObject*)pTarget, GO_ATALAI_LIGHT, INTERACTION_DISTANCE))
                    {
                        pInstance->DoRespawnGameObject(pLight->GetObjectGuid(), 30 * MINUTE);
                    }
                }
                else
                {
                    // If the wrong statue was activated, then trigger trap
                    // We don't know actually which trap goes to which statue so we need to search for each
                    if (GameObject* pTrap = GetClosestGameObjectWithEntry((GameObject*)pTarget, GO_ATALAI_TRAP_1, INTERACTION_DISTANCE))
                    {
                        pTrap->Use((Unit*)pSource);
                    }
                    else if (GameObject* pTrap = GetClosestGameObjectWithEntry((GameObject*)pTarget, GO_ATALAI_TRAP_2, INTERACTION_DISTANCE))
                    {
                        pTrap->Use((Unit*)pSource);
                    }
                    else if (GameObject* pTrap = GetClosestGameObjectWithEntry((GameObject*)pTarget, GO_ATALAI_TRAP_3, INTERACTION_DISTANCE))
                    {
                        pTrap->Use((Unit*)pSource);
                    }
                }

                return true;
            }
        }
        return false;
    }
};

/*######
## event_avatar_of_hakkar
######*/
struct event_avatar_of_hakkar : public MapEventScript
{
    event_avatar_of_hakkar() : MapEventScript("event_avatar_of_hakkar") {}

    bool OnReceived(uint32 /*uiEventId*/, Object* pSource, Object* /*pTarget*/, bool /*bIsStart*/) override
    {
        if (pSource->GetTypeId() == TYPEID_PLAYER)
        {
            if (InstanceData* pInstance = ((Player*)pSource)->GetInstanceData())
            {
                // return if not NOT_STARTED
                if (pInstance->GetData(TYPE_AVATAR) == NOT_STARTED)
                {
                    pInstance->SetData(TYPE_AVATAR, IN_PROGRESS);
                }

                return true;
            }
        }
        return false;
    }
};

/*######
## go_eternal_flame
######*/
struct go_eternal_flame : public GameObjectScript
{
    go_eternal_flame() : GameObjectScript("go_eternal_flame") {}

    bool OnUse(Player* /*pPlayer*/, GameObject* pGo) override
    {
        InstanceData* pInstance = pGo->GetInstanceData();

        if (!pInstance)
        {
            return false;
        }

        if (pInstance->GetData(TYPE_AVATAR) != IN_PROGRESS)
        {
            return false;
        }

        // Set data to special when flame is used
        pInstance->SetData(TYPE_AVATAR, SPECIAL);
        pGo->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);

        return true;
    }
};

/*######
## effectDummy_summon_hakkar
######*/
struct spell_summon_hakkar : public SpellScript
{
    spell_summon_hakkar() : SpellScript("spell_summon_hakkar") {}

    bool EffectDummy(Unit* pCaster, uint32 uiSpellId, SpellEffectIndex uiEffIndex, Object* /*pCreatureTarget*/, ObjectGuid /*originalCasterGuid*/) override
    {
        // Always check spellid and effectindex
        if (uiSpellId == SPELL_SUMMON_AVATAR && uiEffIndex == EFFECT_INDEX_0)
        {
            if (!pCaster || pCaster->GetTypeId() != TYPEID_UNIT)
            {
                return true;
            }

            // Actually summon the avatar of Hakkar and cast some visuals
            if (Creature *pAvatar = pCaster->SummonCreature(NPC_AVATAR_OF_HAKKAR, pCaster->GetPositionX(), pCaster->GetPositionY(), pCaster->GetPositionZ(), pCaster->GetOrientation(), TEMPSUMMON_CORPSE_TIMED_DESPAWN, 1 * DAY*IN_MILLISECONDS))
            {
                pAvatar->CastSpell(pAvatar, SPELL_AVATAR_SUMMONED, true);
            }
            pCaster->ToCreature()->ForcedDespawn(10);

            // Always return true when we are handling this spell and effect
            return true;
        }

        return false;
    }
};

void AddSC_sunken_temple()
{
    Script *s;
    s = new at_shade_of_eranikus();
    s->RegisterSelf();

    s = new npc_malfurion();
    s->RegisterSelf();

    s = new event_antalarion_statue_activation();
    s->RegisterSelf();
    s = new event_avatar_of_hakkar();
    s->RegisterSelf();

    s = new go_eternal_flame();
    s->RegisterSelf();

    s = new spell_summon_hakkar();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "at_shade_of_eranikus";
    //pNewScript->pAreaTrigger = &AreaTrigger_at_shade_of_eranikus;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_malfurion_stormrage";
    //pNewScript->GetAI = &GetAI_npc_malfurion;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "event_antalarion_statue_activation";
    //pNewScript->pProcessEventId = &ProcessEventId_event_antalarion_statue_activation;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "event_avatar_of_hakkar";
    //pNewScript->pProcessEventId = &ProcessEventId_event_avatar_of_hakkar;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "go_eternal_flame";
    //pNewScript->pGOUse = &GOUse_go_eternal_flame;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "npc_shade_of_hakkar";
    //pNewScript->pEffectDummyNPC = &EffectDummyCreature_summon_hakkar;
    //pNewScript->RegisterSelf();
}
