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
 * SDName:      Instance Razorfen_Downs
 * SD%Complete: 10
 * SDComment:   TODO encounter trace and other nice things
 * SDCategory:  Razorfen Downs instancescript
 * EndScriptData
 */

#include "precompiled.h"
#include "razorfen_downs.h"

struct is_razorfen_downs : public InstanceScript
{
    is_razorfen_downs() : InstanceScript("instance_razorfen_downs") {}

    class instance_razorfen_downs : public ScriptedInstance
    {
    public:
        instance_razorfen_downs(Map *pMap) : ScriptedInstance(pMap), iWaveNumber(1), iTombFiendsAlive(0), iTombReaversAlive(0), bWaveInMotion(false)
        {
            Initialize();
        }

        ~instance_razorfen_downs() {}

        void Initialize() override
        {
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_TOMB_FIEND:
                ++iTombFiendsAlive;
                break;
            case NPC_TOMB_REAVER:
                ++iTombReaversAlive;
                break;
            }
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_TOMB_FIEND:
                if (!--iTombFiendsAlive)
                {
                    bWaveInMotion = false;
                    iWaveNumber = 2; // Reaver time
                }
                break;
            case NPC_TOMB_REAVER:
                if (!--iTombReaversAlive)
                {
                    bWaveInMotion = false;
                    iWaveNumber = 3; // boss time!!!
                }
                break;
            }
        }

        void SetData64(uint32 type, uint64 data) override
        {
            switch (type)
            {
            case TYPE_GONG_USED:
                if (bWaveInMotion)
                    return;
                if (Player *pPlayer = instance->GetPlayer(ObjectGuid(data)))
                {
                    bWaveInMotion = true;
                    switch (iWaveNumber)
                    {
                    case 1:
                        // spawn Tomb Fiends
                        SummonCreatures(pPlayer, NPC_TOMB_FIEND, TOTAL_FIENDS);
                        break;
                    case 2:
                        // spawn Tomb Reavers
                        SummonCreatures(pPlayer, NPC_TOMB_REAVER, TOTAL_REAVERS);
                        break;
                    default:
                        // spawn boss (Tuten'kash). Last wave,so will never be set back to false, therefore this event cannot happen again
                        //TODO anything with this "motion"...
                        if (Creature* pTutenkash = pPlayer->SummonCreature(NPC_TUTENKASH, aTutenkashLocation[0].fX, aTutenkashLocation[0].fY, aTutenkashLocation[0].fZ, aTutenkashLocation[0].fO, TEMPSUMMON_CORPSE_TIMED_DESPAWN, 7200000))
                        {
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
                        }
                        break;
                    }
                }
                break;
            }
        }

    private:
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
                //TODO "motion"...
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

        // records which round of creatures we are in (Tomb Fiend, Tomb Raider, Boss)
        int iWaveNumber;
        // use to kick off each wave of creatures and to prevent the event happening more than once whilst in the same instance of the dungeon
        bool bWaveInMotion;
        // keeps track of the number of craetures still alive in the wave
        int iTombFiendsAlive;
        int iTombReaversAlive;

    };

    InstanceData* GetInstanceData(Map *pMap) override
    {
        return new instance_razorfen_downs(pMap);
    }
};

void AddSC_instance_razorfen_downs()
{
    Script* s;
    s = new is_razorfen_downs();
    s->RegisterSelf();
}
