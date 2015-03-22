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
 * SDName:      Instance_Blackrock_Depths
 * SD%Complete: 80
 * SDComment:   None
 * SDCategory:  Blackrock Depths
 * EndScriptData
 */

#include "precompiled.h"
#include "blackrock_depths.h"

instance_blackrock_depths::instance_blackrock_depths(Map* pMap) : ScriptedInstance(pMap),
    m_uiBarAleCount(0),
    m_uiCofferDoorsOpened(0),
    m_uiDwarfRound(0),
    m_uiDwarfFightTimer(0),

    m_fArenaCenterX(0.0f),
    m_fArenaCenterY(0.0f),
    m_fArenaCenterZ(0.0f)
{
    Initialize();
}

void instance_blackrock_depths::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));
}

void instance_blackrock_depths::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_EMPEROR:
        case NPC_PRINCESS:
        case NPC_PHALANX:
        case NPC_HATEREL:
        case NPC_ANGERREL:
        case NPC_VILEREL:
        case NPC_GLOOMREL:
        case NPC_SEETHREL:
        case NPC_DOOMREL:
        case NPC_DOPEREL:
        case NPC_SHILL:
        case NPC_CREST:
        case NPC_JAZ:
        case NPC_TOBIAS:
        case NPC_DUGHAL:
            m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
            break;

        case NPC_WARBRINGER_CONST:
            // Golems not in the Relict Vault?
            if (std::abs(pCreature->GetPositionZ() - aVaultPositions[2]) > 1.0f || !pCreature->IsWithinDist2d(aVaultPositions[0], aVaultPositions[1], 20.0f))
            {
                break;
            }
            // Golems in Relict Vault need to have a stoned aura, set manually to prevent reapply when reached home
            pCreature->CastSpell(pCreature, SPELL_STONED, true);
            // Store the Relict Vault Golems into m_sVaultNpcGuids
        case NPC_WATCHER_DOOMGRIP:
            m_sVaultNpcGuids.insert(pCreature->GetObjectGuid());
            break;
    }
}

void instance_blackrock_depths::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
        case GO_ARENA_1:
        case GO_ARENA_2:
        case GO_ARENA_3:
        case GO_ARENA_4:
        case GO_SHADOW_LOCK:
        case GO_SHADOW_MECHANISM:
        case GO_SHADOW_GIANT_DOOR:
        case GO_SHADOW_DUMMY:
        case GO_BAR_KEG_SHOT:
        case GO_BAR_KEG_TRAP:
        case GO_BAR_DOOR:
        case GO_TOMB_ENTER:
        case GO_TOMB_EXIT:
        case GO_LYCEUM:
        case GO_GOLEM_ROOM_N:
        case GO_GOLEM_ROOM_S:
        case GO_THRONE_ROOM:
        case GO_SPECTRAL_CHALICE:
        case GO_CHEST_SEVEN:
        case GO_ARENA_SPOILS:
        case GO_SECRET_DOOR:
        case GO_JAIL_DOOR_SUPPLY:
        case GO_JAIL_SUPPLY_CRATE:
            break;

        default:
            return;
    }
    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_blackrock_depths::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
        case TYPE_RING_OF_LAW:
            // If finished the arena event after theldren fight
            if (uiData == DONE && m_auiEncounter[0] == SPECIAL)
            {
                DoRespawnGameObject(GO_ARENA_SPOILS, HOUR);
            }
            m_auiEncounter[0] = uiData;
            break;
        case TYPE_VAULT:
            if (uiData == SPECIAL)
            {
                ++m_uiCofferDoorsOpened;

                if (m_uiCofferDoorsOpened == MAX_RELIC_DOORS)
                {
                    SetData(TYPE_VAULT, IN_PROGRESS);

                    Creature* pConstruct = NULL;

                    // Activate vault constructs
                    for (GuidSet::const_iterator itr = m_sVaultNpcGuids.begin(); itr != m_sVaultNpcGuids.end(); ++itr)
                    {
                        pConstruct = instance->GetCreature(*itr);
                        if (pConstruct)
                        {
                            pConstruct->RemoveAurasDueToSpell(SPELL_STONED);
                        }
                    }

                    if (!pConstruct)
                    {
                        return;
                    }

                    // Summon doomgrip
                    pConstruct->SummonCreature(NPC_WATCHER_DOOMGRIP, aVaultPositions[0], aVaultPositions[1], aVaultPositions[2], aVaultPositions[3], TEMPSUMMON_DEAD_DESPAWN, 0);
                }
                // No need to store in this case
                return;
            }
            if (uiData == DONE)
            {
                DoUseDoorOrButton(GO_SECRET_DOOR);
            }
            m_auiEncounter[1] = uiData;
            break;
        case TYPE_BAR:
            if (uiData == SPECIAL)
            {
                ++m_uiBarAleCount;
            }
            else
            {
                m_auiEncounter[2] = uiData;
            }
            break;
        case TYPE_TOMB_OF_SEVEN:
            // Don't set the same data twice
            if (uiData == m_auiEncounter[3])
            {
                break;
            }
            // Combat door
            DoUseDoorOrButton(GO_TOMB_ENTER);
            // Start the event
            if (uiData == IN_PROGRESS)
            {
                DoCallNextDwarf();
            }
            if (uiData == FAIL)
            {
                // Reset dwarfes
                for (uint8 i = 0; i < MAX_DWARFS; ++i)
                {
                    if (Creature* pDwarf = GetSingleCreatureFromStorage(aTombDwarfes[i]))
                    {
                        if (!pDwarf->IsAlive())
                        {
                            pDwarf->Respawn();
                        }
                    }
                }

                m_uiDwarfRound = 0;
                m_uiDwarfFightTimer = 0;
            }
            if (uiData == DONE)
            {
                DoRespawnGameObject(GO_CHEST_SEVEN, HOUR);
                DoUseDoorOrButton(GO_TOMB_EXIT);
            }
            m_auiEncounter[3] = uiData;
            break;
        case TYPE_LYCEUM:
            if (uiData == DONE)
            {
                DoUseDoorOrButton(GO_GOLEM_ROOM_N);
                DoUseDoorOrButton(GO_GOLEM_ROOM_S);
            }
            m_auiEncounter[4] = uiData;
            break;
        case TYPE_IRON_HALL:
            switch (uiData)
            {
                case IN_PROGRESS:
                    DoUseDoorOrButton(GO_GOLEM_ROOM_N);
                    DoUseDoorOrButton(GO_GOLEM_ROOM_S);
                    break;
                case FAIL:
                    DoUseDoorOrButton(GO_GOLEM_ROOM_N);
                    DoUseDoorOrButton(GO_GOLEM_ROOM_S);
                    break;
                case DONE:
                    DoUseDoorOrButton(GO_GOLEM_ROOM_N);
                    DoUseDoorOrButton(GO_GOLEM_ROOM_S);
                    DoUseDoorOrButton(GO_THRONE_ROOM);
                    break;
            }
            m_auiEncounter[5] = uiData;
            break;
        case TYPE_QUEST_JAIL_BREAK:
            m_auiEncounter[6] = uiData;
            return;
    }

    if (uiData == DONE)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2] << " "
                   << m_auiEncounter[3] << " " << m_auiEncounter[4] << " " << m_auiEncounter[5] << " " << m_auiEncounter[6];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_blackrock_depths::GetData(uint32 uiType) const
{
    switch (uiType)
    {
        case TYPE_RING_OF_LAW:
            return m_auiEncounter[0];
        case TYPE_VAULT:
            return m_auiEncounter[1];
        case TYPE_BAR:
            if (m_auiEncounter[2] == IN_PROGRESS && m_uiBarAleCount == 3)
            {
                return SPECIAL;
            }
            else
            {
                return m_auiEncounter[2];
            }
        case TYPE_TOMB_OF_SEVEN:
            return m_auiEncounter[3];
        case TYPE_LYCEUM:
            return m_auiEncounter[4];
        case TYPE_IRON_HALL:
            return m_auiEncounter[5];
        case TYPE_QUEST_JAIL_BREAK:
            return m_auiEncounter[6];
        default:
            return 0;
    }
}

void instance_blackrock_depths::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >> m_auiEncounter[3]
               >> m_auiEncounter[4] >> m_auiEncounter[5] >> m_auiEncounter[6];

    for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
        if (m_auiEncounter[i] == IN_PROGRESS)
        {
            m_auiEncounter[i] = NOT_STARTED;
        }

    OUT_LOAD_INST_DATA_COMPLETE;
}

void instance_blackrock_depths::OnCreatureEnterCombat(Creature* pCreature)
{
    if (pCreature->GetEntry() == NPC_MAGMUS)
    {
        SetData(TYPE_IRON_HALL, IN_PROGRESS);
    }
}

void instance_blackrock_depths::OnCreatureEvade(Creature* pCreature)
{
    if (GetData(TYPE_RING_OF_LAW) == IN_PROGRESS || GetData(TYPE_RING_OF_LAW) == SPECIAL)
    {
        for (uint8 i = 0; i < countof(aArenaNPCs); ++i)
        {
            if (pCreature->GetEntry() == aArenaNPCs[i])
            {
                SetData(TYPE_RING_OF_LAW, FAIL);
                return;
            }
        }
    }

    switch (pCreature->GetEntry())
    {
            // Handle Tomb of the Seven reset in case of wipe
        case NPC_HATEREL:
        case NPC_ANGERREL:
        case NPC_VILEREL:
        case NPC_GLOOMREL:
        case NPC_SEETHREL:
        case NPC_DOPEREL:
        case NPC_DOOMREL:
            SetData(TYPE_TOMB_OF_SEVEN, FAIL);
            break;
        case NPC_MAGMUS:
            SetData(TYPE_IRON_HALL, FAIL);
            break;
    }
}

void instance_blackrock_depths::OnCreatureDeath(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        case NPC_WARBRINGER_CONST:
        case NPC_WATCHER_DOOMGRIP:
            if (GetData(TYPE_VAULT) == IN_PROGRESS)
            {
                m_sVaultNpcGuids.erase(pCreature->GetObjectGuid());

                // If all event npcs dead then set event to done
                if (m_sVaultNpcGuids.empty())
                {
                    SetData(TYPE_VAULT, DONE);
                }
            }
            break;
        case NPC_OGRABISI:
        case NPC_SHILL:
        case NPC_CREST:
        case NPC_JAZ:
            if (GetData(TYPE_QUEST_JAIL_BREAK) == IN_PROGRESS)
            {
                SetData(TYPE_QUEST_JAIL_BREAK, SPECIAL);
            }
            break;
            // Handle Tomb of the Seven dwarf death event
        case NPC_HATEREL:
        case NPC_ANGERREL:
        case NPC_VILEREL:
        case NPC_GLOOMREL:
        case NPC_SEETHREL:
        case NPC_DOPEREL:
            // Only handle the event when event is in progress
            if (GetData(TYPE_TOMB_OF_SEVEN) != IN_PROGRESS)
            {
                return;
            }
            // Call the next dwarf only if it's the last one which joined the fight
            if (pCreature->GetEntry() == aTombDwarfes[m_uiDwarfRound - 1])
            {
                DoCallNextDwarf();
            }
            break;
        case NPC_DOOMREL:
            SetData(TYPE_TOMB_OF_SEVEN, DONE);
            break;
        case NPC_MAGMUS:
            SetData(TYPE_IRON_HALL, DONE);
            break;
    }
}

void instance_blackrock_depths::DoCallNextDwarf()
{
    if (Creature* pDwarf = GetSingleCreatureFromStorage(aTombDwarfes[m_uiDwarfRound]))
    {
        if (Player* pPlayer = GetPlayerInMap())
        {
            pDwarf->SetFactionTemporary(FACTION_DWARF_HOSTILE, TEMPFACTION_RESTORE_RESPAWN | TEMPFACTION_RESTORE_REACH_HOME);
            pDwarf->AI()->AttackStart(pPlayer);
        }
    }
    m_uiDwarfFightTimer = 30000;
    ++m_uiDwarfRound;
}

void instance_blackrock_depths::Update(uint32 uiDiff)
{
    if (m_uiDwarfFightTimer)
    {
        if (m_uiDwarfFightTimer <= uiDiff)
        {
            if (m_uiDwarfRound < MAX_DWARFS)
            {
                DoCallNextDwarf();
                m_uiDwarfFightTimer = 30000;
            }
            else
            {
                m_uiDwarfFightTimer = 0;
            }
        }
        else
        {
            m_uiDwarfFightTimer -= uiDiff;
        }
    }
}

InstanceData* GetInstanceData_instance_blackrock_depths(Map* pMap)
{
    return new instance_blackrock_depths(pMap);
}

void AddSC_instance_blackrock_depths()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_blackrock_depths";
    pNewScript->GetInstanceData = &GetInstanceData_instance_blackrock_depths;
    pNewScript->RegisterSelf();
}
