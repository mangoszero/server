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
 * SDName:      Instance_Naxxramas
 * SD%Complete: 90
 * SDComment:   None
 * SDCategory:  Naxxramas
 * EndScriptData
 */

#include "precompiled.h"
#include "naxxramas.h"

static const DialogueEntry aNaxxDialogue[] =
{
    {NPC_KELTHUZAD,         0,                  10000},
    {SAY_SAPP_DIALOG1,      NPC_KELTHUZAD,      5000},
    {SAY_SAPP_DIALOG2_LICH, NPC_THE_LICHKING,   17000},
    {SAY_SAPP_DIALOG3,      NPC_KELTHUZAD,      6000},
    {SAY_SAPP_DIALOG4_LICH, NPC_THE_LICHKING,   8000},
    {SAY_SAPP_DIALOG5,      NPC_KELTHUZAD,      0},
    {NPC_THANE,             0,                  10000},
    {SAY_KORT_TAUNT1,       NPC_THANE,          5000},
    {SAY_ZELI_TAUNT1,       NPC_ZELIEK,         6000},
    {SAY_BLAU_TAUNT1,       NPC_BLAUMEUX,       6000},
    {SAY_MORG_TAUNT1,       NPC_MOGRAINE,       7000},
    {SAY_BLAU_TAUNT2,       NPC_BLAUMEUX,       6000},
    {SAY_ZELI_TAUNT2,       NPC_ZELIEK,         5000},
    {SAY_KORT_TAUNT2,       NPC_THANE,          7000},
    {SAY_MORG_TAUNT2,       NPC_MOGRAINE,       0},
    {0, 0, 0}
};

struct GothTrigger
{
    bool bIsRightSide;
    bool bIsAnchorHigh;
};

static const float aSapphPositions[4] = { 3521.48f, -5234.87f, 137.626f, 4.53329f };

struct is_naxxramas : public InstanceScript
{
    is_naxxramas() : InstanceScript("instance_naxxramas") {}

    class instance_naxxramas : public ScriptedInstance
    {
    public:
        instance_naxxramas(Map* pMap) : ScriptedInstance(pMap),
            m_fChamberCenterX(0.0f),
            m_fChamberCenterY(0.0f),
            m_fChamberCenterZ(0.0f),
            m_uiSapphSpawnTimer(0),
            m_uiTauntTimer(0),
            m_uiHorseMenKilled(0),
            m_tempCreatureGuid(0),
            m_playerNearGothik(0),
            m_uiChanberCenterAT(0),
            m_dialogueHelper(aNaxxDialogue)
        {
            Initialize();
        }

        ~instance_naxxramas() {}

        void Initialize() override
        {
            memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

            m_dialogueHelper.InitializeDialogueHelper(this, true);
        }

        bool IsEncounterInProgress() const override
        {
            for (uint8 i = 0; i <= TYPE_KELTHUZAD; ++i)
            {
                if (m_auiEncounter[i] == IN_PROGRESS)
                {
                    return true;
                }
            }

            // Some Encounters use SPECIAL while in progress
            if (m_auiEncounter[TYPE_GOTHIK] == SPECIAL)
            {
                return true;
            }

            return false;
        }

        void OnPlayerEnter(Player* pPlayer) override
        {
            // Function only used to summon Sapphiron in case of server reload
            if (GetData(TYPE_SAPPHIRON) != SPECIAL)
            {
                return;
            }

            // Check if already summoned
            if (GetSingleCreatureFromStorage(NPC_SAPPHIRON, true))
            {
                return;
            }

            pPlayer->SummonCreature(NPC_SAPPHIRON, aSapphPositions[0], aSapphPositions[1], aSapphPositions[2], aSapphPositions[3], TEMPSUMMON_DEAD_DESPAWN, 0);
        }

        void OnCreatureCreate(Creature* pCreature) override
        {
            switch (pCreature->GetEntry())
            {
            case NPC_ANUB_REKHAN:
            case NPC_FAERLINA:
            case NPC_THADDIUS:
            case NPC_STALAGG:
            case NPC_FEUGEN:
            case NPC_ZELIEK:
            case NPC_THANE:
            case NPC_BLAUMEUX:
            case NPC_MOGRAINE:
            case NPC_SPIRIT_OF_BLAUMEUX:
            case NPC_SPIRIT_OF_MOGRAINE:
            case NPC_SPIRIT_OF_KORTHAZZ:
            case NPC_SPIRIT_OF_ZELIREK:
            case NPC_GOTHIK:
            case NPC_SAPPHIRON:
            case NPC_KELTHUZAD:
            case NPC_THE_LICHKING:
                m_mNpcEntryGuidStore[pCreature->GetEntry()] = pCreature->GetObjectGuid();
                break;

            case NPC_SUB_BOSS_TRIGGER:
                m_lGothTriggerList.push_back(pCreature->GetObjectGuid());
                break;
            case NPC_TESLA_COIL:
                m_lThadTeslaCoilList.push_back(pCreature->GetObjectGuid());
                break;
            }
        }

        void OnObjectCreate(GameObject* pGo) override
        {
            switch (pGo->GetEntry())
            {
                // Arachnid Quarter
            case GO_ARAC_ANUB_DOOR:
                break;
            case GO_ARAC_ANUB_GATE:
                if (m_auiEncounter[TYPE_ANUB_REKHAN] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_ARAC_FAER_WEB:
                break;
            case GO_ARAC_FAER_DOOR:
                if (m_auiEncounter[TYPE_FAERLINA] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_ARAC_MAEX_INNER_DOOR:
                break;
            case GO_ARAC_MAEX_OUTER_DOOR:
                if (m_auiEncounter[TYPE_FAERLINA] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;

                // Plague Quarter
            case GO_PLAG_NOTH_ENTRY_DOOR:
                break;
            case GO_PLAG_NOTH_EXIT_DOOR:
                if (m_auiEncounter[TYPE_NOTH] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_PLAG_HEIG_ENTRY_DOOR:
                if (m_auiEncounter[TYPE_NOTH] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_PLAG_HEIG_EXIT_HALLWAY:
                if (m_auiEncounter[TYPE_HEIGAN] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_PLAG_LOAT_DOOR:
                break;

                // Military Quarter
            case GO_MILI_GOTH_ENTRY_GATE:
                break;
            case GO_MILI_GOTH_EXIT_GATE:
                if (m_auiEncounter[TYPE_GOTHIK] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_MILI_GOTH_COMBAT_GATE:
                break;
            case GO_MILI_HORSEMEN_DOOR:
                if (m_auiEncounter[TYPE_GOTHIK] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_CHEST_HORSEMEN_NORM:
                break;

                // Construct Quarter
            case GO_CONS_PATH_EXIT_DOOR:
                if (m_auiEncounter[TYPE_PATCHWERK] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_CONS_GLUT_EXIT_DOOR:
                if (m_auiEncounter[TYPE_GLUTH] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_CONS_THAD_DOOR:
                if (m_auiEncounter[TYPE_GLUTH] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_CONS_NOX_TESLA_FEUGEN:
                if (m_auiEncounter[TYPE_THADDIUS] == DONE)
                {
                    pGo->SetGoState(GO_STATE_READY);
                }
                break;
            case GO_CONS_NOX_TESLA_STALAGG:
                if (m_auiEncounter[TYPE_THADDIUS] == DONE)
                {
                    pGo->SetGoState(GO_STATE_READY);
                }
                break;

                // Frostwyrm Lair
            case GO_KELTHUZAD_WATERFALL_DOOR:
                if (m_auiEncounter[TYPE_SAPPHIRON] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_KELTHUZAD_EXIT_DOOR:
            case GO_KELTHUZAD_WINDOW_1:
            case GO_KELTHUZAD_WINDOW_2:
            case GO_KELTHUZAD_WINDOW_3:
            case GO_KELTHUZAD_WINDOW_4:
                break;

                // Eyes
            case GO_ARAC_EYE_RAMP:
            case GO_ARAC_EYE_BOSS:
                if (m_auiEncounter[TYPE_MAEXXNA] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_PLAG_EYE_RAMP:
            case GO_PLAG_EYE_BOSS:
                if (m_auiEncounter[TYPE_LOATHEB] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_MILI_EYE_RAMP:
            case GO_MILI_EYE_BOSS:
                if (m_auiEncounter[TYPE_FOUR_HORSEMEN] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;
            case GO_CONS_EYE_RAMP:
            case GO_CONS_EYE_BOSS:
                if (m_auiEncounter[TYPE_THADDIUS] == DONE)
                {
                    pGo->SetGoState(GO_STATE_ACTIVE);
                }
                break;

                // Portals
            case GO_ARAC_PORTAL:
                if (m_auiEncounter[TYPE_MAEXXNA] == DONE)
                {
                    pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                }
                break;
            case GO_PLAG_PORTAL:
                if (m_auiEncounter[TYPE_LOATHEB] == DONE)
                {
                    pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                }
                break;
            case GO_MILI_PORTAL:
                if (m_auiEncounter[TYPE_FOUR_HORSEMEN] == DONE)
                {
                    pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                }
                break;
            case GO_CONS_PORTAL:
                if (m_auiEncounter[TYPE_THADDIUS] == DONE)
                {
                    pGo->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                }
                break;

            default:
                // Heigan Traps - many different entries which are only required for sorting
                if (pGo->GetGoType() == GAMEOBJECT_TYPE_TRAP)
                {
                    uint32 uiGoEntry = pGo->GetEntry();

                    if ((uiGoEntry >= 181517 && uiGoEntry <= 181524) || uiGoEntry == 181678)
                    {
                        m_alHeiganTrapGuids[0].push_back(pGo->GetObjectGuid());
                    }
                    else if ((uiGoEntry >= 181510 && uiGoEntry <= 181516) || (uiGoEntry >= 181525 && uiGoEntry <= 181531) || uiGoEntry == 181533 || uiGoEntry == 181676)
                    {
                        m_alHeiganTrapGuids[1].push_back(pGo->GetObjectGuid());
                    }
                    else if ((uiGoEntry >= 181534 && uiGoEntry <= 181544) || uiGoEntry == 181532 || uiGoEntry == 181677)
                    {
                        m_alHeiganTrapGuids[2].push_back(pGo->GetObjectGuid());
                    }
                    else if ((uiGoEntry >= 181545 && uiGoEntry <= 181552) || uiGoEntry == 181695)
                    {
                        m_alHeiganTrapGuids[3].push_back(pGo->GetObjectGuid());
                    }
                }

                return;
            }
            m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
        }

        void OnCreatureDeath(Creature* pCreature) override
        {
            if (pCreature->GetEntry() == NPC_MR_BIGGLESWORTH && m_auiEncounter[TYPE_KELTHUZAD] != DONE)
            {
                DoOrSimulateScriptTextForThisInstance(SAY_KELTHUZAD_CAT_DIED, NPC_KELTHUZAD);
            }
        }

        void SetData(uint32 uiType, uint32 uiData) override
        {
            switch (uiType)
            {
            case TYPE_ANUB_REKHAN:
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_ARAC_ANUB_DOOR);
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_ARAC_ANUB_GATE);
                }
                break;
            case TYPE_FAERLINA:
                DoUseDoorOrButton(GO_ARAC_FAER_WEB);
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_ARAC_FAER_DOOR);
                    DoUseDoorOrButton(GO_ARAC_MAEX_OUTER_DOOR);
                }
                m_auiEncounter[uiType] = uiData;
                break;
            case TYPE_MAEXXNA:
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_ARAC_MAEX_INNER_DOOR, uiData);
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_ARAC_EYE_RAMP);
                    DoUseDoorOrButton(GO_ARAC_EYE_BOSS);
                    DoRespawnGameObject(GO_ARAC_PORTAL, 30 * MINUTE);
                    DoToggleGameObjectFlags(GO_ARAC_PORTAL, GO_FLAG_NO_INTERACT, false);
                    m_uiTauntTimer = 5000;
                }
                break;
            case TYPE_NOTH:
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_PLAG_NOTH_ENTRY_DOOR);
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_PLAG_NOTH_EXIT_DOOR);
                    DoUseDoorOrButton(GO_PLAG_HEIG_ENTRY_DOOR);
                }
                break;
            case TYPE_HEIGAN:
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_PLAG_HEIG_ENTRY_DOOR);
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_PLAG_HEIG_EXIT_HALLWAY);
                }
                break;
            case TYPE_LOATHEB:
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_PLAG_LOAT_DOOR);
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_PLAG_EYE_RAMP);
                    DoUseDoorOrButton(GO_PLAG_EYE_BOSS);
                    DoRespawnGameObject(GO_PLAG_PORTAL, 30 * MINUTE);
                    DoToggleGameObjectFlags(GO_PLAG_PORTAL, GO_FLAG_NO_INTERACT, false);
                    m_uiTauntTimer = 5000;
                }
                break;
            case TYPE_RAZUVIOUS:
                m_auiEncounter[uiType] = uiData;
                break;
            case TYPE_GOTHIK:
                switch (uiData)
                {
                case IN_PROGRESS:
                    DoUseDoorOrButton(GO_MILI_GOTH_ENTRY_GATE);
                    DoUseDoorOrButton(GO_MILI_GOTH_COMBAT_GATE);
                    break;
                case SPECIAL:
                    DoUseDoorOrButton(GO_MILI_GOTH_COMBAT_GATE);
                    break;
                case FAIL:
                    if (m_auiEncounter[uiType] == IN_PROGRESS)
                    {
                        DoUseDoorOrButton(GO_MILI_GOTH_COMBAT_GATE);
                    }

                    DoUseDoorOrButton(GO_MILI_GOTH_ENTRY_GATE);
                    break;
                case DONE:
                    DoUseDoorOrButton(GO_MILI_GOTH_ENTRY_GATE);
                    DoUseDoorOrButton(GO_MILI_GOTH_EXIT_GATE);
                    DoUseDoorOrButton(GO_MILI_HORSEMEN_DOOR);

                    m_dialogueHelper.StartNextDialogueText(NPC_THANE);
                    break;
                }
                m_auiEncounter[uiType] = uiData;
                break;
            case TYPE_FOUR_HORSEMEN:
                // Skip if already set
                if (m_auiEncounter[uiType] == uiData)
                {
                    return;
                }
                if (uiData == SPECIAL)
                {
                    ++m_uiHorseMenKilled;

                    if (m_uiHorseMenKilled == 4)
                    {
                        SetData(TYPE_FOUR_HORSEMEN, DONE);
                    }

                    // Don't store special data
                    break;
                }
                if (uiData == FAIL)
                {
                    m_uiHorseMenKilled = 0;
                }
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_MILI_HORSEMEN_DOOR);
                if (uiData == DONE)
                {
                    // Despawn spirits
                    if (Creature* pSpirit = GetSingleCreatureFromStorage(NPC_SPIRIT_OF_BLAUMEUX))
                    {
                        pSpirit->ForcedDespawn();
                    }
                    if (Creature* pSpirit = GetSingleCreatureFromStorage(NPC_SPIRIT_OF_MOGRAINE))
                    {
                        pSpirit->ForcedDespawn();
                    }
                    if (Creature* pSpirit = GetSingleCreatureFromStorage(NPC_SPIRIT_OF_KORTHAZZ))
                    {
                        pSpirit->ForcedDespawn();
                    }
                    if (Creature* pSpirit = GetSingleCreatureFromStorage(NPC_SPIRIT_OF_ZELIREK))
                    {
                        pSpirit->ForcedDespawn();
                    }

                    DoUseDoorOrButton(GO_MILI_EYE_RAMP);
                    DoUseDoorOrButton(GO_MILI_EYE_BOSS);
                    DoRespawnGameObject(GO_MILI_PORTAL, 30 * MINUTE);
                    DoToggleGameObjectFlags(GO_MILI_PORTAL, GO_FLAG_NO_INTERACT, false);
                    DoRespawnGameObject(GO_CHEST_HORSEMEN_NORM, 30 * MINUTE);
                    m_uiTauntTimer = 5000;
                }
                break;
            case TYPE_PATCHWERK:
                m_auiEncounter[uiType] = uiData;
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_CONS_PATH_EXIT_DOOR);
                }
                break;
            case TYPE_GROBBULUS:
                m_auiEncounter[uiType] = uiData;
                break;
            case TYPE_GLUTH:
                m_auiEncounter[uiType] = uiData;
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_CONS_GLUT_EXIT_DOOR);
                    DoUseDoorOrButton(GO_CONS_THAD_DOOR);
                }
                break;
            case TYPE_THADDIUS:
                // Only process real changes here
                if (m_auiEncounter[uiType] == uiData)
                {
                    return;
                }

                m_auiEncounter[uiType] = uiData;
                if (uiData != SPECIAL)
                {
                    DoUseDoorOrButton(GO_CONS_THAD_DOOR, uiData);
                }
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_CONS_EYE_RAMP);
                    DoUseDoorOrButton(GO_CONS_EYE_BOSS);
                    DoRespawnGameObject(GO_CONS_PORTAL, 30 * MINUTE);
                    DoToggleGameObjectFlags(GO_CONS_PORTAL, GO_FLAG_NO_INTERACT, false);
                    m_uiTauntTimer = 5000;
                }
                break;
            case TYPE_SAPPHIRON:
                m_auiEncounter[uiType] = uiData;
                if (uiData == DONE)
                {
                    DoUseDoorOrButton(GO_KELTHUZAD_WATERFALL_DOOR);
                    m_dialogueHelper.StartNextDialogueText(NPC_KELTHUZAD);
                }
                // Start Sapph summoning process
                if (uiData == SPECIAL)
                {
                    m_uiSapphSpawnTimer = 22000;
                }
                break;
            case TYPE_KELTHUZAD:
                m_auiEncounter[uiType] = uiData;
                DoUseDoorOrButton(GO_KELTHUZAD_EXIT_DOOR);
                if (uiData == NOT_STARTED)
                {
                    if (GameObject* pWindow = GetSingleGameObjectFromStorage(GO_KELTHUZAD_WINDOW_1))
                    {
                        pWindow->ResetDoorOrButton();
                    }
                    if (GameObject* pWindow = GetSingleGameObjectFromStorage(GO_KELTHUZAD_WINDOW_2))
                    {
                        pWindow->ResetDoorOrButton();
                    }
                    if (GameObject* pWindow = GetSingleGameObjectFromStorage(GO_KELTHUZAD_WINDOW_3))
                    {
                        pWindow->ResetDoorOrButton();
                    }
                    if (GameObject* pWindow = GetSingleGameObjectFromStorage(GO_KELTHUZAD_WINDOW_4))
                    {
                        pWindow->ResetDoorOrButton();
                    }
                }
                break;
            case TYPE_SIGNAL_1:
                if (AreaTriggerEntry const *at = sAreaTriggerStore.LookupEntry(uiData))
                    SetChamberCenterCoords(at);
                return;
            case TYPE_SIGNAL_2:
                if (Creature *pCreatureTarget = instance->GetCreature(ObjectGuid(m_tempCreatureGuid)))
                {
                    switch (uiData)
                    {
                    case SPELL_A_TO_ANCHOR_1:                           // trigger mobs at high right side
                    case SPELL_B_TO_ANCHOR_1:
                    case SPELL_C_TO_ANCHOR_1:
                        if (Creature* pAnchor2 = GetClosestAnchorForGoth(pCreatureTarget, false))
                        {
                            uint32 uiTriggered = SPELL_A_TO_ANCHOR_2;

                            if (uiData == SPELL_B_TO_ANCHOR_1)
                            {
                                uiTriggered = SPELL_B_TO_ANCHOR_2;
                            }
                            else if (uiData == SPELL_C_TO_ANCHOR_1)
                            {
                                uiTriggered = SPELL_C_TO_ANCHOR_2;
                            }

                            pCreatureTarget->ToCreature()->CastSpell(pAnchor2, uiTriggered, true);
                        }
                        break;
                    case SPELL_A_TO_ANCHOR_2:                           // trigger mobs at high left side
                    case SPELL_B_TO_ANCHOR_2:
                    case SPELL_C_TO_ANCHOR_2:
                    {
                        std::list<Creature*> lTargets;
                        GetGothSummonPointCreatures(lTargets, false);

                        if (!lTargets.empty())
                        {
                            std::list<Creature*>::iterator itr = lTargets.begin();
                            uint32 uiPosition = urand(0, lTargets.size() - 1);
                            advance(itr, uiPosition);

                            if (Creature* pTarget = (*itr))
                            {
                                uint32 uiTriggered = SPELL_A_TO_SKULL;

                                if (uiData == SPELL_B_TO_ANCHOR_2)
                                {
                                    uiTriggered = SPELL_B_TO_SKULL;
                                }
                                else if (uiData == SPELL_C_TO_ANCHOR_2)
                                {
                                    uiTriggered = SPELL_C_TO_SKULL;
                                }

                                pCreatureTarget->CastSpell(pTarget, uiTriggered, true);
                            }
                        }
                        break;
                    }
                    case SPELL_A_TO_SKULL:                              // final destination trigger mob
                    case SPELL_B_TO_SKULL:
                    case SPELL_C_TO_SKULL:
                        if (Creature* pGoth = GetSingleCreatureFromStorage(NPC_GOTHIK))
                        {
                            uint32 uiNpcEntry = NPC_SPECT_TRAINEE;

                            if (uiData == SPELL_B_TO_SKULL)
                            {
                                uiNpcEntry = NPC_SPECT_DEATH_KNIGHT;
                            }
                            else if (uiData == SPELL_C_TO_SKULL)
                            {
                                uiNpcEntry = NPC_SPECT_RIDER;
                            }

                            pGoth->SummonCreature(uiNpcEntry, pCreatureTarget->GetPositionX(), pCreatureTarget->GetPositionY(), pCreatureTarget->GetPositionZ(), pCreatureTarget->GetOrientation(), TEMPSUMMON_DEAD_DESPAWN, 0);

                            if (uiNpcEntry == NPC_SPECT_RIDER)
                            {
                                pGoth->SummonCreature(NPC_SPECT_HORSE, pCreatureTarget->GetPositionX(), pCreatureTarget->GetPositionY(), pCreatureTarget->GetPositionZ(), pCreatureTarget->GetOrientation(), TEMPSUMMON_DEAD_DESPAWN, 0);
                            }
                            break;
                        }
                    }
                }
                return;
            case TYPE_SIGNAL_3:
                 SetGothTriggers();
                return;
            case TYPE_SIGNAL_8:
                DoTriggerHeiganTraps(instance->GetCreature(ObjectGuid(m_tempCreatureGuid)), uint8(uiData));
                return;
            case TYPE_SIGNAL_9:
            case TYPE_SIGNAL_10:
                for (GuidList::const_iterator itr = m_lThadTeslaCoilList.begin(); itr != m_lThadTeslaCoilList.end(); ++itr)
                {
                    if (Creature* pTesla = instance->GetCreature(*itr))
                    {
                        if (CreatureAI* pTeslaAI = pTesla->AI())
                        {
                            pTeslaAI->ReceiveAIEvent(uiType == TYPE_SIGNAL_9 ? AI_EVENT_CUSTOM_A : AI_EVENT_CUSTOM_B, pTesla, pTesla, uiData);
                        }
                    }
                }
                return;
            }

            if (uiData == DONE || (uiData == SPECIAL && uiType == TYPE_SAPPHIRON))
            {
                OUT_SAVE_INST_DATA;

                std::ostringstream saveStream;
                saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2] << " "
                    << m_auiEncounter[3] << " " << m_auiEncounter[4] << " " << m_auiEncounter[5] << " "
                    << m_auiEncounter[6] << " " << m_auiEncounter[7] << " " << m_auiEncounter[8] << " "
                    << m_auiEncounter[9] << " " << m_auiEncounter[10] << " " << m_auiEncounter[11] << " "
                    << m_auiEncounter[12] << " " << m_auiEncounter[13] << " " << m_auiEncounter[14] << " " << m_auiEncounter[15];

                m_strInstData = saveStream.str();

                SaveToDB();
                OUT_SAVE_INST_DATA_COMPLETE;
            }
        }

        uint32 GetData(uint32 uiType) const override
        {
            if (uiType < MAX_ENCOUNTER)
            {
                return m_auiEncounter[uiType];
            }
            else
            {
                switch (uiType)
                {
                case TYPE_SIGNAL_1:
                    return m_uiChanberCenterAT;
                case TYPE_SIGNAL_4:
                    if (Player *pPlayer = instance->GetPlayer(m_playerNearGothik))
                        return uint32(const_cast<instance_naxxramas*>(this)->IsInRightSideGothArea(pPlayer));
                    else
                        return uint32(false);
                    break;
                case TYPE_SIGNAL_7:
                    return uint32(const_cast<instance_naxxramas*>(this)->IsInRightSideGothArea(instance->GetCreature(m_tempCreatureGuid)));
                default:
                    break;
                }
            }

            return 0;
        }

        void SetData64(uint32 type, uint64 data) override
        {
            switch (type)
            {
            case TYPE_SIGNAL_2:
            case TYPE_SIGNAL_6:
            case TYPE_SIGNAL_7:
            case TYPE_SIGNAL_8:
                m_tempCreatureGuid = data;
                break;
            case TYPE_SIGNAL_4:
                m_playerNearGothik = data;
                break;
            default:
                break;
            }
        }

        uint64 GetData64(uint32 type) const override
        {
            switch(type)
            {
            case TYPE_SIGNAL_5: //sequential retrieving of required trigger GUIDs; TODO fix the whole Gothik encounter code design
                if (gtit == m_mGothTriggerMap.end())
                {
                    (const_cast<instance_naxxramas*>(this))->gtit = m_mGothTriggerMap.begin();
                    return 0;
                }
                else
                {
                    while (gtit != m_mGothTriggerMap.end() && (gtit->second.bIsAnchorHigh || !gtit->second.bIsRightSide))
                        ++(const_cast<instance_naxxramas*>(this))->gtit;
                    if (gtit == m_mGothTriggerMap.end())
                        return 0;
                    return gtit->first.GetRawValue();
                }
                break;
            case TYPE_SIGNAL_6: //the same design flaw...
                if (Creature *anchor = (const_cast<instance_naxxramas*>(this))->GetClosestAnchorForGoth(instance->GetCreature(ObjectGuid(m_tempCreatureGuid)), true))
                    return anchor->GetObjectGuid().GetRawValue();
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
            loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >> m_auiEncounter[3]
                >> m_auiEncounter[4] >> m_auiEncounter[5] >> m_auiEncounter[6] >> m_auiEncounter[7]
                >> m_auiEncounter[8] >> m_auiEncounter[9] >> m_auiEncounter[10] >> m_auiEncounter[11]
                >> m_auiEncounter[12] >> m_auiEncounter[13] >> m_auiEncounter[14] >> m_auiEncounter[15];

            for (uint8 i = 0; i < MAX_ENCOUNTER; ++i)
            {
                if (m_auiEncounter[i] == IN_PROGRESS)
                {
                    m_auiEncounter[i] = NOT_STARTED;
                }
            }

            OUT_LOAD_INST_DATA_COMPLETE;
        }

        void Update(uint32 uiDiff) override
        {
            if (m_uiTauntTimer)
            {
                if (m_uiTauntTimer <= uiDiff)
                {
                    DoTaunt();
                    m_uiTauntTimer = 0;
                }
                else
                {
                    m_uiTauntTimer -= uiDiff;
                }
            }

            if (m_uiSapphSpawnTimer)
            {
                if (m_uiSapphSpawnTimer <= uiDiff)
                {
                    if (Player* pPlayer = GetPlayerInMap())
                    {
                        pPlayer->SummonCreature(NPC_SAPPHIRON, aSapphPositions[0], aSapphPositions[1], aSapphPositions[2], aSapphPositions[3], TEMPSUMMON_DEAD_DESPAWN, 0);
                    }

                    m_uiSapphSpawnTimer = 0;
                }
                else
                {
                    m_uiSapphSpawnTimer -= uiDiff;
                }
            }

            m_dialogueHelper.DialogueUpdate(uiDiff);
        }

        // Heigan
        void DoTriggerHeiganTraps(Creature* pHeigan, uint32 uiAreaIndex)
        {
            if (uiAreaIndex >= MAX_HEIGAN_TRAP_AREAS)
            {
                return;
            }

            for (GuidList::const_iterator itr = m_alHeiganTrapGuids[uiAreaIndex].begin(); itr != m_alHeiganTrapGuids[uiAreaIndex].end(); ++itr)
            {
                if (GameObject* pTrap = instance->GetGameObject(*itr))
                {
                    pTrap->Use(pHeigan);
                }
            }
        }

        // goth
        // Right is right side from gothik (eastern)
        bool IsInRightSideGothArea(Unit* pUnit)
        {
            if (GameObject* pCombatGate = GetSingleGameObjectFromStorage(GO_MILI_GOTH_COMBAT_GATE))
            {
                return (pCombatGate->GetPositionY() >= pUnit->GetPositionY());
            }

            script_error_log("left/right side check, Gothik combat area failed.");
            return true;
        }

        // thaddius
        void GetThadTeslaCreatures(GuidList& lList) { lList = m_lThadTeslaCoilList; };

        // kel
        void SetChamberCenterCoords(AreaTriggerEntry const *at)
        {
            m_uiChanberCenterAT = at->id;
        }

        void GetChamberCenterCoords(float& fX, float& fY, float& fZ) { fX = m_fChamberCenterX; fY = m_fChamberCenterY; fZ = m_fChamberCenterZ; }
        void DoTaunt()
        {
            if (m_auiEncounter[TYPE_KELTHUZAD] != DONE)
            {
                uint8 uiWingsCleared = 0;

                if (m_auiEncounter[TYPE_MAEXXNA] == DONE)
                {
                    ++uiWingsCleared;
                }

                if (m_auiEncounter[TYPE_LOATHEB] == DONE)
                {
                    ++uiWingsCleared;
                }

                if (m_auiEncounter[TYPE_FOUR_HORSEMEN] == DONE)
                {
                    ++uiWingsCleared;
                }

                if (m_auiEncounter[TYPE_THADDIUS] == DONE)
                {
                    ++uiWingsCleared;
                }

                switch (uiWingsCleared)
                {
                case 1:
                    DoOrSimulateScriptTextForThisInstance(SAY_KELTHUZAD_TAUNT1, NPC_KELTHUZAD);
                    break;
                case 2:
                    DoOrSimulateScriptTextForThisInstance(SAY_KELTHUZAD_TAUNT2, NPC_KELTHUZAD);
                    break;
                case 3:
                    DoOrSimulateScriptTextForThisInstance(SAY_KELTHUZAD_TAUNT3, NPC_KELTHUZAD);
                    break;
                case 4:
                    DoOrSimulateScriptTextForThisInstance(SAY_KELTHUZAD_TAUNT4, NPC_KELTHUZAD);
                    break;
                }
            }
        }

    private:
        //Gothik
        Creature* GetClosestAnchorForGoth(Creature* pSource, bool bRightSide)
        {
            std::list<Creature* > lList;

            for (UNORDERED_MAP<ObjectGuid, GothTrigger>::iterator itr = m_mGothTriggerMap.begin(); itr != m_mGothTriggerMap.end(); ++itr)
            {
                if (!itr->second.bIsAnchorHigh)
                {
                    continue;
                }

                if (itr->second.bIsRightSide != bRightSide)
                {
                    continue;
                }

                if (Creature* pCreature = instance->GetCreature(itr->first))
                {
                    lList.push_back(pCreature);
                }
            }

            if (!lList.empty())
            {
                lList.sort(ObjectDistanceOrder(pSource));
                return lList.front();
            }

            return NULL;
        }

        void GetGothSummonPointCreatures(std::list<Creature*>& lList, bool bRightSide)
        {
            for (UNORDERED_MAP<ObjectGuid, GothTrigger>::iterator itr = m_mGothTriggerMap.begin(); itr != m_mGothTriggerMap.end(); ++itr)
            {
                if (itr->second.bIsAnchorHigh)
                {
                    continue;
                }

                if (itr->second.bIsRightSide != bRightSide)
                {
                    continue;
                }

                if (Creature* pCreature = instance->GetCreature(itr->first))
                {
                    lList.push_back(pCreature);
                }
            }
        }

        void SetGothTriggers()
        {
            Creature* pGoth = GetSingleCreatureFromStorage(NPC_GOTHIK);

            if (!pGoth)
            {
                return;
            }

            for (GuidList::const_iterator itr = m_lGothTriggerList.begin(); itr != m_lGothTriggerList.end(); ++itr)
            {
                if (Creature* pTrigger = instance->GetCreature(*itr))
                {
                    GothTrigger pGt;
                    pGt.bIsAnchorHigh = (pTrigger->GetPositionZ() >= (pGoth->GetPositionZ() - 5.0f));
                    pGt.bIsRightSide = IsInRightSideGothArea(pTrigger);

                    m_mGothTriggerMap[pTrigger->GetObjectGuid()] = pGt;
                }
            }
            gtit = m_mGothTriggerMap.begin();
        }

        // data
        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidList m_lThadTeslaCoilList;
        GuidList m_lGothTriggerList;
        uint64 m_tempCreatureGuid;
        uint64 m_playerNearGothik;

        UNORDERED_MAP<ObjectGuid, GothTrigger> m_mGothTriggerMap;
        UNORDERED_MAP<ObjectGuid, GothTrigger>::const_iterator gtit;

        GuidList m_alHeiganTrapGuids[MAX_HEIGAN_TRAP_AREAS];

        float m_fChamberCenterX;
        float m_fChamberCenterY;
        float m_fChamberCenterZ;

        uint32 m_uiSapphSpawnTimer;
        uint32 m_uiTauntTimer;
        uint32 m_uiChanberCenterAT;
        uint8 m_uiHorseMenKilled;

        DialogueHelper m_dialogueHelper;
    };

    InstanceData* GetInstanceData(Map* pMap) override
    {
        return new instance_naxxramas(pMap);
    }
};

struct at_naxxramas : public AreaTriggerScript
{
    at_naxxramas() : AreaTriggerScript("at_naxxramas") {}

    bool OnTrigger(Player* pPlayer, AreaTriggerEntry const* pAt) override
    {
        ScriptedInstance* pInstance = (ScriptedInstance*)pPlayer->GetInstanceData();
        switch (pAt->id)
        {
        case AREATRIGGER_KELTHUZAD:
            if (pPlayer->isGameMaster() || !pPlayer->IsAlive())
            {
                return false;
            }

            if (pInstance)
            {
                pInstance->SetData(TYPE_SIGNAL_1, pAt->id); //it is unclear why do not hardcode AREATRIGGER_KELTHUZAD constant into the single place it used: boss_kelthuzad::JustSummoned

                if (pInstance->GetData(TYPE_KELTHUZAD) == NOT_STARTED)
                {
                    if (Creature* pKelthuzad = pInstance->GetSingleCreatureFromStorage(NPC_KELTHUZAD))
                    {
                        if (pKelthuzad->IsAlive())
                        {
                            pInstance->SetData(TYPE_KELTHUZAD, IN_PROGRESS);
                            pKelthuzad->SetInCombatWithZone();
                        }
                    }
                }
            }
            else
                return false;
            break;
        case AREATRIGGER_THADDIUS_DOOR:
            if (pInstance && pInstance->GetData(TYPE_THADDIUS) == NOT_STARTED)
            {
                if (Creature* pThaddius = pInstance->GetSingleCreatureFromStorage(NPC_THADDIUS))
                {
                    pInstance->SetData(TYPE_THADDIUS, SPECIAL);
                    DoScriptText(SAY_THADDIUS_GREET, pThaddius);
                }
            }
            break;
        case AREATRIGGER_FROSTWYRM_TELE:
            if (pInstance)
            {
                // Area trigger handles teleport in DB. Here we only need to check if all the end wing encounters are done
                if (pInstance->GetData(TYPE_THADDIUS) != DONE || pInstance->GetData(TYPE_LOATHEB) != DONE || pInstance->GetData(TYPE_MAEXXNA) != DONE ||
                    pInstance->GetData(TYPE_FOUR_HORSEMEN) != DONE)
                {
                    return true;
                }
            }
            break;
        }

        return false;
    }
};

void AddSC_instance_naxxramas()
{
    Script* s;
    s = new is_naxxramas();
    s->RegisterSelf();
    s = new at_naxxramas();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "instance_naxxramas";
    //pNewScript->GetInstanceData = &GetInstanceData_instance_naxxramas;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "at_naxxramas";
    //pNewScript->pAreaTrigger = &AreaTrigger_at_naxxramas;
    //pNewScript->RegisterSelf();
}
