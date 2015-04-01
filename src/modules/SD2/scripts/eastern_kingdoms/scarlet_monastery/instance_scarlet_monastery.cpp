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
 * SDName:      Instance_Scarlet_Monastery
 * SD%Complete: 50
 * SDComment:   None
 * SDCategory:  Scarlet Monastery
 * EndScriptData
 */

#include "precompiled.h"
#include "scarlet_monastery.h"

struct is_scarlet_monastery : public InstanceScript
{
    is_scarlet_monastery() : InstanceScript("instance_scarlet_monastery") {}

    class instance_scarlet_monastery : public ScriptedInstance
    {
    public:
        instance_scarlet_monastery(Map* pMap) : ScriptedInstance(pMap)
        {
            Initialize();
        }

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_MOGRAINE:
            case NPC_WHITEMANE:
            case NPC_VORREL:
                m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
                break;
            }
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            if (pCreature->GetEntry() == NPC_INTERROGATOR_VISHAS)
            {
                // Any other actions to do with Vorrel? setStandState?
                if (Creature* pVorrel = GetSingleCreatureFromStorage(NPC_VORREL))
                {
                    DoScriptText(SAY_TRIGGER_VORREL, pVorrel);
                }
            }
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            if (pGo->GetEntry() == GO_WHITEMANE_DOOR)
            {
                m_mGoEntryGuidStore[GO_WHITEMANE_DOOR] = pGo->GetObjectGuid();
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            if (uiType == TYPE_MOGRAINE_AND_WHITE_EVENT)
            {
                if (uiData == IN_PROGRESS)
                {
                    DoUseDoorOrButton(GO_WHITEMANE_DOOR);
                }
                if (uiData == FAIL)
                {
                    DoUseDoorOrButton(GO_WHITEMANE_DOOR);
                }

                m_auiEncounter[0] = uiData;
            }
        }

        uint32 GetData(uint32 uiData) const override
        {
            if (uiData == TYPE_MOGRAINE_AND_WHITE_EVENT)
            {
                return m_auiEncounter[0];
            }

            return 0;
        }

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_scarlet_monastery(pMap);
    }
};

void AddSC_instance_scarlet_monastery()
{
    Script* s;
    s = new is_scarlet_monastery();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_scarlet_monastery";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_scarlet_monastery;
    //pNewScript->RegisterSelf();
}
