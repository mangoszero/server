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
    static bool AreaTrigger(Player*, AreaTriggerEntry const*);
    static bool ProcessEvent(uint32, Object*, Object*, bool);
    static bool EffectDummyCreature(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
    static bool EffectDummyGameObject(Unit*, uint32, SpellEffectIndex, GameObject*, ObjectGuid);
    static bool EffectDummyItem(Unit*, uint32, SpellEffectIndex, Item*, ObjectGuid);
    static bool EffectScriptEffectCreature(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
    static bool AuraDummy(Aura const*, bool);
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

struct Script
{
    Script() :
        pGossipHello(NULL), pGossipHelloGO(NULL), pGossipSelect(NULL), pGossipSelectGO(NULL),
        pGossipSelectWithCode(NULL), pGossipSelectGOWithCode(NULL),
        pDialogStatusNPC(NULL), pDialogStatusGO(NULL),
        pQuestAcceptNPC(NULL), pQuestAcceptGO(NULL), pQuestAcceptItem(NULL),
        pQuestRewardedNPC(NULL), pQuestRewardedGO(NULL),
        pGOUse(NULL), pItemUse(NULL), pAreaTrigger(NULL), pProcessEventId(NULL),
        pEffectDummyNPC(NULL), pEffectDummyGO(NULL), pEffectDummyItem(NULL), pEffectScriptEffectNPC(NULL),
        pEffectAuraDummy(NULL), GetAI(NULL), GetInstanceData(NULL)
    {}

    std::string Name;

    bool (*pGossipHello)(Player*, Creature*);
    bool (*pGossipHelloGO)(Player*, GameObject*);
    bool (*pGossipSelect)(Player*, Creature*, uint32, uint32);
    bool (*pGossipSelectGO)(Player*, GameObject*, uint32, uint32);
    bool (*pGossipSelectWithCode)(Player*, Creature*, uint32, uint32, const char*);
    bool (*pGossipSelectGOWithCode)(Player*, GameObject*, uint32, uint32, const char*);
    uint32(*pDialogStatusNPC)(Player*, Creature*);
    uint32(*pDialogStatusGO)(Player*, GameObject*);
    bool (*pQuestAcceptNPC)(Player*, Creature*, Quest const*);
    bool (*pQuestAcceptGO)(Player*, GameObject*, Quest const*);
    bool (*pQuestAcceptItem)(Player*, Item*, Quest const*);
    bool (*pQuestRewardedNPC)(Player*, Creature*, Quest const*);
    bool (*pQuestRewardedGO)(Player*, GameObject*, Quest const*);
    bool (*pGOUse)(Player*, GameObject*);
    bool (*pItemUse)(Player*, Item*, SpellCastTargets const&);
    bool (*pAreaTrigger)(Player*, AreaTriggerEntry const*);
    bool (*pProcessEventId)(uint32, Object*, Object*, bool);
    bool (*pEffectDummyNPC)(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
    bool (*pEffectDummyGO)(Unit*, uint32, SpellEffectIndex, GameObject*, ObjectGuid);
    bool (*pEffectDummyItem)(Unit*, uint32, SpellEffectIndex, Item*, ObjectGuid);
    bool (*pEffectScriptEffectNPC)(Unit*, uint32, SpellEffectIndex, Creature*, ObjectGuid);
    bool (*pEffectAuraDummy)(const Aura*, bool);

    CreatureAI* (*GetAI)(Creature*);
    InstanceData* (*GetInstanceData)(Map*);

    void RegisterSelf(bool bReportError = true);
};

// *********************************************************
// ************* Some functions used globally **************

// Generic scripting text function
void DoScriptText(int32 iTextEntry, WorldObject* pSource, Unit* pTarget = NULL);
void DoOrSimulateScriptTextForMap(int32 iTextEntry, uint32 uiCreatureEntry, Map* pMap, Creature* pCreatureSource = NULL, Unit* pTarget = NULL);

// *********************************************************
// **************** Internal hook mechanics ****************

#if COMPILER == COMPILER_GNU || COMPILER == COMPILER_CLANG
#define FUNC_PTR(name,callconvention,returntype,parameters)    typedef returntype(*name)parameters __attribute__ ((callconvention));
#else
#define FUNC_PTR(name, callconvention, returntype, parameters)    typedef returntype(callconvention *name)parameters;
#endif

#endif
