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
 * SDName:      Guards
 * SD%Complete: 100
 * SDComment:   Zone guards
 * SDCategory:  Guards
 * EndScriptData
 */

/**
 * ContentData
 * guard_bluffwatcher
 * guard_contested
 * guard_darnassus
 * guard_dunmorogh
 * guard_durotar
 * guard_elwynnforest
 * guard_ironforge
 * guard_mulgore
 * guard_orgrimmar
 * guard_stormwind
 * guard_teldrassil
 * guard_tirisfal
 * guard_undercity
 * EndContentData
 */

#include "precompiled.h"
#include "guard_ai.h"

struct guard_generic : public CreatureScript
{
    guard_generic() : CreatureScript("guard_generic") {}

    CreatureAI* GetAI(Creature *pCreature) override
    {
        return new guardAI(pCreature);
    }
};

//CreatureAI* GetAI_guard_bluffwatcher(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_contested(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_darnassus(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_dunmorogh(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_durotar(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_elwynnforest(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_ironforge(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_mulgore(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}

struct guard_orgrimmar : public CreatureScript
{
    guard_orgrimmar() : CreatureScript("guard_orgrimmar") {}

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new guardAI_orgrimmar(pCreature);
    }
};

struct guard_stormwind : public CreatureScript
{
    guard_stormwind() : CreatureScript("guard_stormwind") {}

    CreatureAI* GetAI(Creature* pCreature) override
    {
        return new guardAI_stormwind(pCreature);
    }
};

//CreatureAI* GetAI_guard_teldrassil(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_tirisfal(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}
//
//CreatureAI* GetAI_guard_undercity(Creature* pCreature)
//{
//    return new guardAI(pCreature);
//}

void AddSC_guards()
{
    Script* s;
    s = new guard_generic();
    s->RegisterSelf();
    s = new guard_orgrimmar();
    s->RegisterSelf();
    s = new guard_stormwind();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_bluffwatcher";
    //pNewScript->GetAI = &GetAI_guard_bluffwatcher;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_contested";
    //pNewScript->GetAI = &GetAI_guard_contested;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_darnassus";
    //pNewScript->GetAI = &GetAI_guard_darnassus;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_dunmorogh";
    //pNewScript->GetAI = &GetAI_guard_dunmorogh;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_durotar";
    //pNewScript->GetAI = &GetAI_guard_durotar;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_elwynnforest";
    //pNewScript->GetAI = &GetAI_guard_elwynnforest;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_ironforge";
    //pNewScript->GetAI = &GetAI_guard_ironforge;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_mulgore";
    //pNewScript->GetAI = &GetAI_guard_mulgore;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_orgrimmar";
    //pNewScript->GetAI = &GetAI_guard_orgrimmar;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_stormwind";
    //pNewScript->GetAI = &GetAI_guard_stormwind;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_teldrassil";
    //pNewScript->GetAI = &GetAI_guard_teldrassil;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_tirisfal";
    //pNewScript->GetAI = &GetAI_guard_tirisfal;
    //pNewScript->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "guard_undercity";
    //pNewScript->GetAI = &GetAI_guard_undercity;
    //pNewScript->RegisterSelf();
}
