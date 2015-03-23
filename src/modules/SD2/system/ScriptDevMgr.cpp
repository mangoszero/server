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

#include "precompiled.h"
#include "Config/Config.h"
#include "Database/DatabaseEnv.h"
#include "DBCStores.h"
#include "ObjectMgr.h"
#include "ProgressBar.h"
#include "system/ScriptLoader.h"
#include "system/system.h"
#include "ScriptDevMgr.h"

typedef std::vector<Script*> SDScriptVec;
int num_sc_scripts;
SDScriptVec m_scripts;

Config SD2Config;

void FillSpellSummary();

void LoadDatabase()
{
    std::string strSD2DBinfo = SD2Config.GetStringDefault("ScriptDev2DatabaseInfo", "");

    if (strSD2DBinfo.empty())
    {
        script_error_log("Missing Scriptdev2 database info from configuration file. Load database aborted.");
        return;
    }

    // Initialize connection to DB
    if (SD2Database.Initialize(strSD2DBinfo.c_str()))
    {
        outstring_log("SD2: ScriptDev2 database initialized.");
        outstring_log("\n");

        pSystemMgr.LoadVersion();
        pSystemMgr.LoadScriptTexts();
        pSystemMgr.LoadScriptTextsCustom();
        pSystemMgr.LoadScriptGossipTexts();
        pSystemMgr.LoadScriptWaypoints();
    }
    else
    {
        script_error_log("Unable to connect to Database. Load database aborted.");
        return;
    }

    SD2Database.HaltDelayThread();
}

struct TSpellSummary
{
    uint8 Targets;                                          // set of enum SelectTarget
    uint8 Effects;                                          // set of enum SelectEffect
} extern* SpellSummary;

//*********************************
//*** Functions used globally ***

/**
    * Function that does script text
    *
    * @param iTextEntry Entry of the text, stored in SD2-database
    * @param pSource Source of the text
    * @param pTarget Can be NULL (depending on CHAT_TYPE of iTextEntry). Possible target for the text
    */
void DoScriptText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget)
{
    if (!pSource)
    {
        script_error_log("DoScriptText entry %i, invalid Source pointer.", iTextEntry);
        return;
    }

    if (iTextEntry >= 0)
    {
        script_error_log("DoScriptText with source entry %u (TypeId=%u, guid=%u) attempts to process text entry %i, but text entry must be negative.",
                            pSource->GetEntry(), pSource->GetTypeId(), pSource->GetGUIDLow(), iTextEntry);

        return;
    }

    DoDisplayText(pSource, iTextEntry, pTarget);
    // TODO - maybe add some call-stack like error output if above function returns false
}

/**
    * Function that either simulates or does script text for a map
    *
    * @param iTextEntry Entry of the text, stored in SD2-database, only type CHAT_TYPE_ZONE_YELL supported
    * @param uiCreatureEntry Id of the creature of whom saying will be simulated
    * @param pMap Given Map on which the map-wide text is displayed
    * @param pCreatureSource Can be NULL. If pointer to Creature is given, then the creature does the map-wide text
    * @param pTarget Can be NULL. Possible target for the text
    */
void DoOrSimulateScriptTextForMap(int32 iTextEntry, uint32 uiCreatureEntry, Map* pMap, Creature* pCreatureSource /*=NULL*/, Unit* pTarget /*=NULL*/)
{
    if (!pMap)
    {
        script_error_log("DoOrSimulateScriptTextForMap entry %i, invalid Map pointer.", iTextEntry);
        return;
    }

    if (iTextEntry >= 0)
    {
        script_error_log("DoOrSimulateScriptTextForMap with source entry %u for map %u attempts to process text entry %i, but text entry must be negative.", uiCreatureEntry, pMap->GetId(), iTextEntry);
        return;
    }

    CreatureInfo const* pInfo = GetCreatureTemplateStore(uiCreatureEntry);
    if (!pInfo)
    {
        script_error_log("DoOrSimulateScriptTextForMap has invalid source entry %u for map %u.", uiCreatureEntry, pMap->GetId());
        return;
    }

    MangosStringLocale const* pData = GetMangosStringData(iTextEntry);
    if (!pData)
    {
        script_error_log("DoOrSimulateScriptTextForMap with source entry %u for map %u could not find text entry %i.", uiCreatureEntry, pMap->GetId(), iTextEntry);
        return;
    }

    debug_log("SD2: DoOrSimulateScriptTextForMap: text entry=%i, Sound=%u, Type=%u, Language=%u, Emote=%u",
                iTextEntry, pData->SoundId, pData->Type, pData->LanguageId, pData->Emote);

    if (pData->Type != CHAT_TYPE_ZONE_YELL)
    {
        script_error_log("DoSimulateScriptTextForMap entry %i has not supported chat type %u.", iTextEntry, pData->Type);
        return;
    }

    if (pData->SoundId)
    {
        pMap->PlayDirectSoundToMap(pData->SoundId);
    }

    if (pCreatureSource)                                // If provided pointer for sayer, use direct version
    {
        pMap->MonsterYellToMap(pCreatureSource->GetObjectGuid(), iTextEntry, pData->LanguageId, pTarget);
    }
    else                                                // Simulate yell
    {
        pMap->MonsterYellToMap(pInfo, iTextEntry, pData->LanguageId, pTarget);
    }
}

//*********************************
//*** Functions used internally ***

void Script::RegisterSelf(bool bReportError)
{
    if (uint32 id = GetScriptId(Name.c_str()))
    {
        m_scripts[id] = this;
        ++num_sc_scripts;
    }
    else
    {
        if (bReportError)
        {
            script_error_log("Script registering but ScriptName %s is not assigned in database. Script will not be used.", Name.c_str());
        }

        delete this;
    }
}

//************************************
//*** Functions to be used by core ***

void SD2::FreeScriptLibrary()
{
    // Free Spell Summary
    delete []SpellSummary;

    // Free resources before library unload
    for (SDScriptVec::const_iterator itr = m_scripts.begin(); itr != m_scripts.end(); ++itr)
    {
        delete *itr;
    }

    m_scripts.clear();

    num_sc_scripts = 0;

    setScriptLibraryErrorFile(NULL, NULL);
}

void SD2::InitScriptLibrary()
{
    // ScriptDev2 startup
    outstring_log("  ___         _      _   ___          ___ ");
    outstring_log(" / __| __ _ _(_)_ __| |_|   \\ _____ _|_  )");
    outstring_log(" \\__ \\/ _| '_| | '_ \\  _| |) / -_) V // / ");
    outstring_log(" |___/\\__|_| |_| .__/\\__|___/\\___|\\_//___|");
    outstring_log("               |_|                        ");
    outstring_log("                    http://scriptdev2.com/\n");

    // Get configuration file
    bool configFailure = false;
    if (!SD2Config.SetSource(MANGOSD_CONFIG_LOCATION))
    {
        configFailure = true;
    }
    else
    {
        outstring_log("SD2: Using configuration file %s", MANGOSD_CONFIG_LOCATION);
    }

    // Set SD2 Error Log File
    std::string sd2LogFile = SD2Config.GetStringDefault("SD2ErrorLogFile", "scriptdev2-errors.log");
    setScriptLibraryErrorFile(sd2LogFile.c_str(), "SD2");

    if (configFailure)
    {
        script_error_log("Unable to open configuration file. Database will be unaccessible. Configuration values will use default.");
    }

    // Check config file version
    if (SD2Config.GetIntDefault("ConfVersion", 0) != MANGOSD_CONFIG_VERSION)
    {
        script_error_log("Configuration file version doesn't match expected version. Some config variables may be wrong or missing.");
    }

    outstring_log("\n");

    // Load database (must be called after SD2Config.SetSource).
    LoadDatabase();

    outstring_log("SD2: Loading C++ scripts");
    BarGoLink bar(1);
    bar.step();

    // Resize script ids to needed ammount of assigned ScriptNames (from core)
    m_scripts.resize(GetScriptIdsCount(), NULL);

    FillSpellSummary();

    AddScripts();

    // Check existance scripts for all registered by core script names
    for (uint32 i = 1; i < GetScriptIdsCount(); ++i)
    {
        if (!m_scripts[i])
        {
            script_error_log("No script found for ScriptName '%s'.", GetScriptName(i));
        }
    }

    outstring_log(">> Loaded %i C++ Scripts.", num_sc_scripts);
}

char const* SD2::GetScriptLibraryVersion()
{
    return strSD2Version.c_str();
}

bool SD2::GossipHello(Player* pPlayer, Creature* pCreature)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->pGossipHello)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pGossipHello(pPlayer, pCreature);
}

bool SD2::GOGossipHello(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pGossipHelloGO)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pGossipHelloGO(pPlayer, pGo);
}

bool SD2::GossipSelect(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    debug_log("SD2: Gossip selection, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->pGossipSelect)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pGossipSelect(pPlayer, pCreature, uiSender, uiAction);
}

bool SD2::GOGossipSelect(Player* pPlayer, GameObject* pGo, uint32 uiSender, uint32 uiAction)
{
    debug_log("SD2: GO Gossip selection, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pGossipSelectGO)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pGossipSelectGO(pPlayer, pGo, uiSender, uiAction);
}

bool SD2::GossipSelectWithCode(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char* sCode)
{
    debug_log("SD2: Gossip selection with code, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->pGossipSelectWithCode)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pGossipSelectWithCode(pPlayer, pCreature, uiSender, uiAction, sCode);
}

bool SD2::GOGossipSelectWithCode(Player* pPlayer, GameObject* pGo, uint32 uiSender, uint32 uiAction, const char* sCode)
{
    debug_log("SD2: GO Gossip selection with code, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pGossipSelectGOWithCode)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pGossipSelectGOWithCode(pPlayer, pGo, uiSender, uiAction, sCode);
}

bool SD2::QuestAccept(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->pQuestAcceptNPC)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pQuestAcceptNPC(pPlayer, pCreature, pQuest);
}

bool SD2::QuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->pQuestRewardedNPC)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pQuestRewardedNPC(pPlayer, pCreature, pQuest);
}

uint32 SD2::GetNPCDialogStatus(Player* pPlayer, Creature* pCreature)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->pDialogStatusNPC)
    {
        return DIALOG_STATUS_UNDEFINED;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pDialogStatusNPC(pPlayer, pCreature);
}

uint32 SD2::GetGODialogStatus(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pDialogStatusGO)
    {
        return DIALOG_STATUS_UNDEFINED;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pDialogStatusGO(pPlayer, pGo);
}

bool SD2::ItemQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{
    Script* pTempScript = m_scripts[pItem->GetProto()->ScriptId];

    if (!pTempScript || !pTempScript->pQuestAcceptItem)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pQuestAcceptItem(pPlayer, pItem, pQuest);
}

bool SD2::GOUse(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pGOUse)
    {
        return false;
    }

    return pTempScript->pGOUse(pPlayer, pGo);
}

bool SD2::GOQuestAccept(Player* pPlayer, GameObject* pGo, const Quest* pQuest)
{
    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pQuestAcceptGO)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pQuestAcceptGO(pPlayer, pGo, pQuest);
}

bool SD2::GOQuestRewarded(Player* pPlayer, GameObject* pGo, Quest const* pQuest)
{
    Script* pTempScript = m_scripts[pGo->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pQuestRewardedGO)
    {
        return false;
    }

    pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->pQuestRewardedGO(pPlayer, pGo, pQuest);
}

bool SD2::AreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{
    Script* pTempScript = m_scripts[GetAreaTriggerScriptId(atEntry->id)];

    if (!pTempScript || !pTempScript->pAreaTrigger)
    {
        return false;
    }

    return pTempScript->pAreaTrigger(pPlayer, atEntry);
}

bool SD2::ProcessEvent(uint32 uiEventId, Object* pSource, Object* pTarget, bool bIsStart)
{
    Script* pTempScript = m_scripts[GetEventIdScriptId(uiEventId)];

    if (!pTempScript || !pTempScript->pProcessEventId)
    {
        return false;
    }

    // bIsStart may be false, when event is from taxi node events (arrival=false, departure=true)
    return pTempScript->pProcessEventId(uiEventId, pSource, pTarget, bIsStart);
}

CreatureAI* SD2::GetCreatureAI(Creature* pCreature)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->GetAI)
    {
        return NULL;
    }

    return pTempScript->GetAI(pCreature);
}

bool SD2::ItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{
    Script* pTempScript = m_scripts[pItem->GetProto()->ScriptId];

    if (!pTempScript || !pTempScript->pItemUse)
    {
        return false;
    }

    return pTempScript->pItemUse(pPlayer, pItem, targets);
}

bool SD2::EffectDummyCreature(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Creature* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[pTarget->GetScriptId()];

    if (!pTempScript || !pTempScript->pEffectDummyNPC)
    {
        return false;
    }

    return pTempScript->pEffectDummyNPC(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::EffectDummyGameObject(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[pTarget->GetGOInfo()->ScriptId];

    if (!pTempScript || !pTempScript->pEffectDummyGO)
    {
        return false;
    }

    return pTempScript->pEffectDummyGO(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::EffectDummyItem(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[pTarget->GetProto()->ScriptId];

    if (!pTempScript || !pTempScript->pEffectDummyItem)
    {
        return false;
    }

    return pTempScript->pEffectDummyItem(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::EffectScriptEffectCreature(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Creature* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[pTarget->GetScriptId()];

    if (!pTempScript || !pTempScript->pEffectScriptEffectNPC)
    {
        return false;
    }

    return pTempScript->pEffectScriptEffectNPC(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::AuraDummy(Aura const* pAura, bool bApply)
{
    Script* pTempScript = m_scripts[((Creature*)pAura->GetTarget())->GetScriptId()];

    if (!pTempScript || !pTempScript->pEffectAuraDummy)
    {
        return false;
    }

    return pTempScript->pEffectAuraDummy(pAura, bApply);
}

InstanceData* SD2::CreateInstanceData(Map* pMap)
{
    Script* pTempScript = m_scripts[pMap->GetScriptId()];

    if (!pTempScript || !pTempScript->GetInstanceData)
    {
        return NULL;
    }

    return pTempScript->GetInstanceData(pMap);
}

#ifdef WIN32
#  include <windows.h>
BOOL APIENTRY DllMain(HANDLE /*hModule*/, DWORD /*ul_reason_for_call*/, LPVOID /*lpReserved*/)
{
    return true;
}
#endif
