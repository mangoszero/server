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
 * SDName:      instance_razorfen_kraul
 * SD%Complete: 50
 * SDComment:   None
 * SDCategory:  Razorfen Kraul
 * EndScriptData
 */

#include "precompiled.h"
#include "razorfen_kraul.h"

struct is_razorfen_kraul : public InstanceScript
{
    is_razorfen_kraul() : InstanceScript("instance_razorfen_kraul") {}

    class instance_razorfen_kraul : public ScriptedInstance
    {
    public:
        instance_razorfen_kraul(Map* pMap) : ScriptedInstance(pMap),
            m_uiWardKeepersRemaining(0)
        {
            Initialize();
        }

        ~instance_razorfen_kraul() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            switch (pGo->GetEntry())
            {
            case GO_AGATHELOS_WARD:
                m_mGoEntryGuidStore[GO_AGATHELOS_WARD] = pGo->GetObjectGuid();
                if (m_auiEncounter[0] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            }
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_WARD_KEEPER:
                ++m_uiWardKeepersRemaining;
                break;
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
            case TYPE_AGATHELOS:
                --m_uiWardKeepersRemaining;
                if (!m_uiWardKeepersRemaining)
                {
                    m_auiEncounter[0] = uiData;
                    DoUseDoorOrButton(GO_AGATHELOS_WARD);
                }
                break;
            }

            if (uiData == DONE)
            {
                OUT_SAVE_INST_DATA;

                std::ostringstream saveStream;

                saveStream << m_auiEncounter[0];
                m_strInstData = saveStream.str();

                SaveToDB();
                OUT_SAVE_INST_DATA_COMPLETE;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            switch (uiType)
            {
            case TYPE_AGATHELOS:
                return m_auiEncounter[0];
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
            loadStream >> m_auiEncounter[0];

            for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
            {
                if (m_auiEncounter[i] == IN_PROGRESS)
                {
                    m_auiEncounter[i] = NOT_STARTED;
                }
            }

            OUT_LOAD_INST_DATA_COMPLETE;
        }

    private:
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        uint8 m_uiWardKeepersRemaining;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_razorfen_kraul(pMap);
    }
};

void AddSC_instance_razorfen_kraul()
{
    Script* s;
    s = new is_razorfen_kraul();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_razorfen_kraul";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_razorfen_kraul;
    //pNewScript->RegisterSelf();
}
