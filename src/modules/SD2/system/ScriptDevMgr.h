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

#ifndef SC_SCRIPTMGR_H
#define SC_SCRIPTMGR_H

#include "Common.h"
#include "DBCStructure.h"
#include "ScriptMgr.h"

class Player;
class Creature;
class CreatureAI;
class InstanceData;
class Quest;
class Item;
class GameObject;
class SpellCastTargets;
class Map;
class Unit;
class WorldObject;
class Aura;
class Object;
class ObjectGuid;

// *********************************************************
// **************** Functions used by core *****************

class SD2
{
public:
    static void FreeScriptLibrary();
    static void InitScriptLibrary();
    static char const* GetScriptLibraryVersion();

    static CreatureAI* GetCreatureAI(Creature* pCreature);
    static InstanceData* CreateInstanceData(Map* pMap);

    static bool GossipHello(Player*, Creature*);
    static bool GOGossipHello(Player*, GameObject*);
    static bool GossipSelect(Player*, Creature*, uint32, uint32);
    static bool GOGossipSelect(Player*, GameObject*, uint32, uint32);
    static bool GossipSelectWithCode(Player*, Creature*, uint32, uint32, const char*);
    static bool GOGossipSelectWithCode(Player*, GameObject*, uint32, uint32, const char*);
    static bool QuestAccept(Player*, Creature*, Quest const*);
    static bool GOQuestAccept(Player*, GameObject*, Quest const*);
    static bool ItemQuestAccept(Player*, Item*, Quest const*);
    static bool QuestRewarded(Player*, Creature*, Quest const*);
    static bool GOQuestRewarded(Player*, GameObject*, Quest const*);
    static uint32 GetNPCDialogStatus(Player*, Creature*);
    static uint32 GetGODialogStatus(Player*, GameObject*);
    static bool GOUse(Player*, GameObject*);
    static bool ItemUse(Player*, Item*, SpellCastTargets const&);
    static bool ItemEquip(Player*, Item*, bool);    //new TODO
    static bool ItemDelete(Player*, Item*);         //new TODO
    static bool AreaTrigger(Player*, AreaTriggerEntry const*);
    static bool ProcessEvent(uint32, Object*, Object*, bool);
    static bool EffectDummyCreature(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
    static bool EffectDummyGameObject(Unit*, uint32, SpellEffectIndex, GameObject*, ObjectGuid);
    static bool EffectDummyItem(Unit*, uint32, SpellEffectIndex, Item*, ObjectGuid);
    static bool EffectScriptEffectCreature(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
    static bool AuraDummy(Aura const *, bool);
    //static bool AuraDummyTick(Aura const*);         [-ZERO:] no dummy ticks. TODO
};

// *********************************************************
// ************** Some defines used globally ***************

// Basic defines
#define VISIBLE_RANGE       (166.0f)                        // MAX visible range (size of grid)
#define DEFAULT_TEXT        "<ScriptDev2 Text Entry Missing!>"

/* Escort Factions
 * TODO: find better namings and definitions.
 * N=Neutral, A=Alliance, H=Horde.
 * NEUTRAL or FRIEND = Hostility to player surroundings (not a good definition)
 * ACTIVE or PASSIVE = Hostility to environment surroundings.
 */
enum EscortFaction
{
    FACTION_ESCORT_A_NEUTRAL_PASSIVE    = 10,
    FACTION_ESCORT_H_NEUTRAL_PASSIVE    = 33,
    FACTION_ESCORT_N_NEUTRAL_PASSIVE    = 113,

    FACTION_ESCORT_A_NEUTRAL_ACTIVE     = 231,
    FACTION_ESCORT_H_NEUTRAL_ACTIVE     = 232,
    FACTION_ESCORT_N_NEUTRAL_ACTIVE     = 250,

    FACTION_ESCORT_N_FRIEND_PASSIVE     = 290,
    FACTION_ESCORT_N_FRIEND_ACTIVE      = 495,

    FACTION_ESCORT_A_PASSIVE            = 774,
    FACTION_ESCORT_H_PASSIVE            = 775
};

// *********************************************************
// ************* Some structures used globally *************
struct CreatureScript;
struct GameObjectScript;
struct ItemScript;
struct AreaTriggerScript;
struct MapEventScript;
struct ZoneScript;
struct OutdoorPvPScript;
struct BattleGroundScript;
struct InstanceScript;
struct SpellScript;
struct AuraScript;
struct ConditionScript;
struct AchievementScript;

struct Script
{
    std::string Name;
    ScriptedObjectType Type;

    void RegisterSelf(bool bReportError = true);
    virtual bool IsValid() { return true; }

    Script() : Name(""), Type(SCRIPTED_MAX_TYPE) {}
    Script(ScriptedObjectType type, const char* name) : Name(name), Type(type) {}

    CreatureScript* ToCreatureScript() { return Type == SCRIPTED_UNIT && IsValid() ? (CreatureScript*)this : NULL; }
    GameObjectScript* ToGameObjectScript() { return Type == SCRIPTED_GAMEOBJECT && IsValid() ? (GameObjectScript*)this : NULL; }
    ItemScript* ToItemScript() { return Type == SCRIPTED_ITEM && IsValid() ? (ItemScript*)this : NULL; }
    AreaTriggerScript* ToAreaTriggerScript() { return Type == SCRIPTED_AREATRIGGER && IsValid() ? (AreaTriggerScript*)this : NULL; }
    MapEventScript* ToMapEventScript() { return Type == SCRIPTED_MAPEVENT && IsValid() ? (MapEventScript*)this : NULL; }
    ZoneScript* ToZoneScript() { return Type == SCRIPTED_MAP && IsValid() ? (ZoneScript*)this : NULL; }
    OutdoorPvPScript* ToOutdoorPvPScript() { return Type == SCRIPTED_PVP_ZONE && IsValid() ? (OutdoorPvPScript*)this : NULL; }
    BattleGroundScript* ToBattleGroundScript() { return Type == SCRIPTED_BATTLEGROUND && IsValid() ? (BattleGroundScript*)this : NULL; }
    InstanceScript* ToInstanceScript() { return Type == SCRIPTED_INSTANCE && IsValid() ? (InstanceScript*)this : NULL; }
    SpellScript* ToSpellScript() { return Type == SCRIPTED_SPELL && IsValid() ? (SpellScript*)this : NULL; }
    AuraScript* ToAuraScript() { return Type == SCRIPTED_AURASPELL && IsValid() ? (AuraScript*)this : NULL; }
    ConditionScript* ToConditionScript() { return Type == SCRIPTED_CONDITION && IsValid() ? (ConditionScript*)this : NULL; }
    AchievementScript* ToAchievementScript() { return Type == SCRIPTED_ACHIEVEMENT && IsValid() ? (AchievementScript*)this : NULL; }
};

struct CreatureScript : public Script
{
    CreatureScript(const char* name) : Script(SCRIPTED_UNIT, name) {}

    virtual bool OnGossipHello(Player*, Creature*) { return false; }
    virtual bool OnGossipSelect(Player*, Creature*, uint32, uint32) { return false; }
    virtual bool OnGossipSelectWithCode(Player*, Creature*, uint32, uint32, const char*) { return false; }
    virtual uint32 OnDialogEnd(Player*, Creature*) { return 0; }
    virtual bool OnQuestAccept(Player*, Creature*, Quest const*) { return false; }
    virtual bool OnQuestRewarded(Player*, Creature*, Quest const*) { return false; }

    virtual CreatureAI* GetAI(Creature*) { return NULL; }
};

struct GameObjectScript : public Script
{
    GameObjectScript(const char* name) : Script(SCRIPTED_GAMEOBJECT, name) {}

    virtual bool OnGossipHello(Player*, GameObject*) { return false; }
    virtual bool OnGossipSelect(Player*, GameObject*, uint32, uint32) { return false; }
    virtual bool OnGossipSelectWithCode(Player*, GameObject*, uint32, uint32, const char*) { return false; }
    virtual uint32 OnDialogEnd(Player*, GameObject*) { return 0; }
    virtual bool OnQuestAccept(Player*, GameObject*, Quest const*) { return false; }
    virtual bool OnQuestRewarded(Player*, GameObject*, Quest const*) { return false; }
    virtual bool OnUse(Player*, GameObject*) { return false; }
};

struct ItemScript : public Script
{
    ItemScript(const char* name) : Script(SCRIPTED_ITEM, name) {}

    virtual bool OnQuestAccept(Player*, Item*, Quest const*) { return false; }
    virtual bool OnUse(Player*, Item*, SpellCastTargets const&) { return false; }
    virtual bool OnEquip(Player*, Item*, bool on) { return false; }
    virtual bool OnDelete(Player*, Item*) { return false; }
};

struct AreaTriggerScript : public Script
{
    AreaTriggerScript(const char* name) : Script(SCRIPTED_AREATRIGGER, name) {}

    virtual bool OnTrigger(Player*, AreaTriggerEntry const*) { return false; }
};

struct MapEventScript : public Script
{
    MapEventScript(const char *name) : Script(SCRIPTED_MAPEVENT, name) {}

    virtual bool OnReceived(uint32, Object*, Object*, bool) { return false; }
};

struct ZoneScript : public Script
{
    ZoneScript(const char* name) : Script(SCRIPTED_MAP, name) {}
    ZoneScript(const char* name, ScriptedObjectType type) : Script(type, name) {}

    virtual bool OnMapEvent(uint32, Object*, Object*, bool) { return false; }

    virtual InstanceData* GetInstanceData(Map*) { return NULL; }
};

struct OutdoorPvPScript : public ZoneScript
{
    OutdoorPvPScript(const char* name) : ZoneScript(name, SCRIPTED_PVP_ZONE) {}
};

struct BattleGroundScript : public ZoneScript
{
    BattleGroundScript(const char* name) : ZoneScript(name, SCRIPTED_BATTLEGROUND) {}
    //bool IsValid() override { return map && map->IsBattleGround(); }
};

struct InstanceScript : public ZoneScript
{
    InstanceScript(const char* name) : ZoneScript(name, SCRIPTED_INSTANCE) {}
    //bool IsValid() override { return map && map->IsDungeon(); }
};

struct SpellScript : public Script
{
    SpellScript(const char* name) : Script(SCRIPTED_SPELL, name) {}
    //bool IsValid() override { return bool(sSpellStore.LookupEntry(spellID)); }

    virtual bool EffectDummy(Unit*, uint32, SpellEffectIndex, Object*, ObjectGuid) { return false; }
    virtual bool EffectScriptEffect(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid) { return false; }
};

struct AuraScript : public Script
{
    AuraScript(const char* name) : Script(SCRIPTED_AURASPELL, name) {}
    //bool IsValid() override { return bool(sSpellStore.LookupEntry(spellID)); }

    virtual bool OnDummyApply(const Aura*, bool) { return false; }
    //virtual bool OnDummyTick(const Aura*) { return false; }   [-ZERO:] no AURA_RERIODIC_DUMMY
};

struct ConditionScript : public Script
{
    ConditionScript(const char* name) : Script(SCRIPTED_CONDITION, name) {}
};

struct AchievementScript : public Script
{
    AchievementScript(const char* name) : Script(SCRIPTED_ACHIEVEMENT, name) {}
};

//struct Script
//{
//    Script() :
//        pGossipHello(NULL), pGossipHelloGO(NULL), pGossipSelect(NULL), pGossipSelectGO(NULL),
//        pGossipSelectWithCode(NULL), pGossipSelectGOWithCode(NULL),
//        pDialogStatusNPC(NULL), pDialogStatusGO(NULL),
//        pQuestAcceptNPC(NULL), pQuestAcceptGO(NULL), pQuestAcceptItem(NULL),
//        pQuestRewardedNPC(NULL), pQuestRewardedGO(NULL),
//        pGOUse(NULL), pItemUse(NULL), pAreaTrigger(NULL), pProcessEventId(NULL),
//        pEffectDummyNPC(NULL), pEffectDummyGO(NULL), pEffectDummyItem(NULL), pEffectScriptEffectNPC(NULL),
//        pEffectAuraDummy(NULL), GetAI(NULL), GetInstanceData(NULL)
//    {}
//
//    std::string Name;
//
//    bool (*pGossipHello)(Player*, Creature*);
//    bool (*pGossipHelloGO)(Player*, GameObject*);
//    bool (*pGossipSelect)(Player*, Creature*, uint32, uint32);
//    bool (*pGossipSelectGO)(Player*, GameObject*, uint32, uint32);
//    bool (*pGossipSelectWithCode)(Player*, Creature*, uint32, uint32, const char*);
//    bool (*pGossipSelectGOWithCode)(Player*, GameObject*, uint32, uint32, const char*);
//    uint32(*pDialogStatusNPC)(Player*, Creature*);
//    uint32(*pDialogStatusGO)(Player*, GameObject*);
//    bool (*pQuestAcceptNPC)(Player*, Creature*, Quest const*);
//    bool (*pQuestAcceptGO)(Player*, GameObject*, Quest const*);
//    bool (*pQuestAcceptItem)(Player*, Item*, Quest const*);
//    bool (*pQuestRewardedNPC)(Player*, Creature*, Quest const*);
//    bool (*pQuestRewardedGO)(Player*, GameObject*, Quest const*);
//    bool (*pGOUse)(Player*, GameObject*);
//    bool (*pItemUse)(Player*, Item*, SpellCastTargets const&);
//    bool (*pAreaTrigger)(Player*, AreaTriggerEntry const*);
//    bool (*pProcessEventId)(uint32, Object*, Object*, bool);
//    bool (*pEffectDummyNPC)(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
//    bool (*pEffectDummyGO)(Unit*, uint32, SpellEffectIndex, GameObject*, ObjectGuid);
//    bool (*pEffectDummyItem)(Unit*, uint32, SpellEffectIndex, Item*, ObjectGuid);
//    bool (*pEffectScriptEffectNPC)(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
//    bool (*pEffectAuraDummy)(const Aura*, bool);
//
//    CreatureAI* (*GetAI)(Creature*);
//    InstanceData* (*GetInstanceData)(Map*);
//
//    void RegisterSelf(bool bReportError = true);
//};

// *********************************************************
// ************* Some functions used globally **************

// Generic scripting text function
void DoScriptText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget = NULL);
void DoOrSimulateScriptTextForMap(int32 iTextEntry, uint32 uiCreatureEntry, Map* pMap, Creature* pCreatureSource = NULL, Unit* pTarget = NULL);

#endif
