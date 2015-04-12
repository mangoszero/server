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

#include "precompiled.h"//..\bindings\scripts\include\precompiled.h"
#include "Config/Config.h"
#include "SystemConfig.h"
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

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToCreatureScript()->OnGossipHello(pPlayer, pCreature);
}

bool SD2::GOGossipHello(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToGameObjectScript()->OnGossipHello(pPlayer, pGo);
}

bool SD2::GossipSelect(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction)
{
    debug_log("SD2: Gossip selection, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return false;
    }

    return pTempScript->ToCreatureScript()->OnGossipSelect(pPlayer, pCreature, uiSender, uiAction);
}

bool SD2::GOGossipSelect(Player* pPlayer, GameObject* pGo, uint32 uiSender, uint32 uiAction)
{
    debug_log("SD2: GO Gossip selection, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return false;
    }

    return pTempScript->ToGameObjectScript()->OnGossipSelect(pPlayer, pGo, uiSender, uiAction);
}

bool SD2::GossipSelectWithCode(Player* pPlayer, Creature* pCreature, uint32 uiSender, uint32 uiAction, const char* sCode)
{
    debug_log("SD2: Gossip selection with code, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToCreatureScript()->OnGossipSelectWithCode(pPlayer, pCreature, uiSender, uiAction, sCode);
}

bool SD2::GOGossipSelectWithCode(Player* pPlayer, GameObject* pGo, uint32 uiSender, uint32 uiAction, const char* sCode)
{
    debug_log("SD2: GO Gossip selection with code, sender: %u, action: %u", uiSender, uiAction);

    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToGameObjectScript()->OnGossipSelectWithCode(pPlayer, pGo, uiSender, uiAction, sCode);
}

bool SD2::QuestAccept(Player* pPlayer, Creature* pCreature, const Quest* pQuest)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToCreatureScript()->OnQuestAccept(pPlayer, pCreature, pQuest);
}

bool SD2::QuestRewarded(Player* pPlayer, Creature* pCreature, Quest const* pQuest)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToCreatureScript()->OnQuestRewarded(pPlayer, pCreature, pQuest);
}

uint32 SD2::GetNPCDialogStatus(Player* pPlayer, Creature* pCreature)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return DIALOG_STATUS_UNDEFINED;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToCreatureScript()->OnDialogEnd(pPlayer, pCreature);
}

uint32 SD2::GetGODialogStatus(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return DIALOG_STATUS_UNDEFINED;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToGameObjectScript()->OnDialogEnd(pPlayer, pGo);
}

bool SD2::ItemQuestAccept(Player* pPlayer, Item* pItem, Quest const* pQuest)
{
    Script* pTempScript = m_scripts[pItem->GetScriptId()];

    if (!pTempScript || !pTempScript->ToItemScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToItemScript()->OnQuestAccept(pPlayer, pItem, pQuest);
}

bool SD2::GOUse(Player* pPlayer, GameObject* pGo)
{
    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return false;
    }

    return pTempScript->ToGameObjectScript()->OnUse(pPlayer, pGo);
}

bool SD2::GOQuestAccept(Player* pPlayer, GameObject* pGo, const Quest* pQuest)
{
    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToGameObjectScript()->OnQuestAccept(pPlayer, pGo, pQuest);
}

bool SD2::GOQuestRewarded(Player* pPlayer, GameObject* pGo, Quest const* pQuest)
{
    Script* pTempScript = m_scripts[pGo->GetScriptId()];

    if (!pTempScript || !pTempScript->ToGameObjectScript())
    {
        return false;
    }

    //pPlayer->PlayerTalkClass->ClearMenus();

    return pTempScript->ToGameObjectScript()->OnQuestRewarded(pPlayer, pGo, pQuest);
}

bool SD2::AreaTrigger(Player* pPlayer, AreaTriggerEntry const* atEntry)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_AREATRIGGER, atEntry->id)];

    if (!pTempScript || !pTempScript->ToAreaTriggerScript())
    {
        return false;
    }

    return pTempScript->ToAreaTriggerScript()->OnTrigger(pPlayer, atEntry);
}

//the analogous method OnMapEvent exists also in the ZoneScript class and there it should have a higher priority. TODO
bool SD2::ProcessEvent(uint32 uiEventId, Object* pSource, Object* pTarget, bool bIsStart)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_MAPEVENT, uiEventId)];

    if (!pTempScript || !pTempScript->ToMapEventScript())
    {
        return false;
    }

    // bIsStart may be false, when event is from taxi node events (arrival=false, departure=true)
    return pTempScript->ToMapEventScript()->OnReceived(uiEventId, pSource, pTarget, bIsStart);
}

CreatureAI* SD2::GetCreatureAI(Creature* pCreature)
{
    Script* pTempScript = m_scripts[pCreature->GetScriptId()];

    if (!pTempScript || !pTempScript->ToCreatureScript())
    {
        return NULL;
    }

    CreatureAI* ai = pTempScript->ToCreatureScript()->GetAI(pCreature);
    if (ai)
        ai->Reset();

    return ai;
}

bool SD2::ItemUse(Player* pPlayer, Item* pItem, SpellCastTargets const& targets)
{
    Script* pTempScript = m_scripts[pItem->GetScriptId()];

    if (!pTempScript || !pTempScript->ToItemScript())
    {
        return false;
    }

    return pTempScript->ToItemScript()->OnUse(pPlayer, pItem, targets);
}

bool SD2::ItemEquip(Player* pPlayer, Item* pItem, bool on)
{
    Script* pTempScript = m_scripts[pItem->GetScriptId()];

    if (!pTempScript || !pTempScript->ToItemScript())
    {
        return false;
    }

    return pTempScript->ToItemScript()->OnEquip(pPlayer, pItem, on);
}

bool SD2::ItemDelete(Player* pPlayer, Item* pItem)
{
    Script* pTempScript = m_scripts[pItem->GetScriptId()];

    if (!pTempScript || !pTempScript->ToItemScript())
    {
        return false;
    }

    return pTempScript->ToItemScript()->OnDelete(pPlayer, pItem);
}

// NOTE! test if sScriptMgr is allowed here, or else add script id on upper level to the parameters
bool SD2::EffectDummyCreature(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Creature* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_SPELL, spellId | effIndex << 24)];

    if (!pTempScript || !pTempScript->ToSpellScript())
    {
        return false;
    }

    return pTempScript->ToSpellScript()->EffectDummy(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::EffectDummyGameObject(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, GameObject* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_SPELL, spellId | effIndex << 24)];

    if (!pTempScript || !pTempScript->ToSpellScript())
    {
        return false;
    }

    return pTempScript->ToSpellScript()->EffectDummy(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::EffectDummyItem(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Item* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_SPELL, spellId | effIndex << 24)];

    if (!pTempScript || !pTempScript->ToSpellScript())
    {
        return false;
    }

    return pTempScript->ToSpellScript()->EffectDummy(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::EffectScriptEffectCreature(Unit* pCaster, uint32 spellId, SpellEffectIndex effIndex, Creature* pTarget, ObjectGuid originalCasterGuid)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_SPELL, spellId | effIndex << 24)];

    if (!pTempScript || !pTempScript->ToSpellScript())
    {
        return false;
    }

    return pTempScript->ToSpellScript()->EffectScriptEffect(pCaster, spellId, effIndex, pTarget, originalCasterGuid);
}

bool SD2::AuraDummy(Aura const* pAura, bool bApply)
{
    Script* pTempScript = m_scripts[sScriptMgr.GetBoundScriptId(SCRIPTED_AURASPELL, pAura->GetId() | pAura->GetEffIndex() << 24)];

    if (!pTempScript || !pTempScript->ToAuraScript())
    {
        return false;
    }

    return pTempScript->ToAuraScript()->OnDummyApply(pAura, bApply);
}
// END Note!

InstanceData* SD2::CreateInstanceData(Map* pMap)
{
    Script* pTempScript = m_scripts[pMap->GetScriptId()];

    if (!pTempScript || !pTempScript->ToInstanceScript())
    {
        return NULL;
    }

    return pTempScript->ToInstanceScript()->GetInstanceData(pMap);
}

//#ifdef WIN32
//#  include <windows.h>
//BOOL APIENTRY DllMain(HANDLE /*hModule*/, DWORD /*ul_reason_for_call*/, LPVOID /*lpReserved*/)
//{
//    return true;
//}
//#endif
