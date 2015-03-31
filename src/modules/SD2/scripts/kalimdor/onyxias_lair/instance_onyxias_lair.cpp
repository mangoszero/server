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
 * SDName:      Instance_Onyxias_Lair
 * SD%Complete: 50
 * SDComment:   None
 * SDCategory:  Onyxia's Lair
 * EndScriptData
 */

#include "precompiled.h"
#include "onyxias_lair.h"

struct is_onyxias_lair : public InstanceScript
{
    is_onyxias_lair() : InstanceScript("instance_onyxias_lair") {}

    class instance_onyxias_lair : public ScriptedInstance
    {
    public:
        instance_onyxias_lair(Map* pMap) : ScriptedInstance(pMap)
        {
            Initialize();
        }

        ~instance_onyxias_lair() {}

        void Initialize() override
        {
            m_uiEncounter = NOT_STARTED;
            m_tPhaseTwoStart = time(NULL);
        }

        bool IsEncounterInProgress() const override
        {
            return m_uiEncounter == IN_PROGRESS || m_uiEncounter >= DATA_LIFTOFF;
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_ONYXIA_TRIGGER:
                m_mNpcEntryGuidStore[NPC_ONYXIA_TRIGGER] = pCreature->GetObjectGuid();
                break;
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            if (uiType != TYPE_ONYXIA)
            {
                return;
            }

            m_uiEncounter = uiData;

            if (uiData == DATA_LIFTOFF)
            {
                m_tPhaseTwoStart = time(NULL);
            }

            // Currently no reason to save anything
        }

    protected:
        uint32 m_uiEncounter;

        time_t m_tPhaseTwoStart;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_onyxias_lair(pMap);
    }
};

void AddSC_instance_onyxias_lair()
{
    Script* s;
    s = new is_onyxias_lair();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_onyxias_lair";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_onyxias_lair;
    //pNewScript->RegisterSelf();
}
