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
 * SDName:      Molten_Core
 * SD%Complete: 25
 * SDComment:   None
 * SDCategory:  Molten Core
 * EndScriptData
 */

/**
 * ContentData
 * go_molten_core_rune
 * EndContentData
 */

#include "precompiled.h"
#include "molten_core.h"

struct sRuneEncounters
{
    uint32 m_uiRuneEntry, m_uiType;
};

static const sRuneEncounters m_aMoltenCoreRunes[MAX_MOLTEN_RUNES] =
{
    { GO_RUNE_KRESS, TYPE_MAGMADAR },
    { GO_RUNE_MOHN, TYPE_GEHENNAS },
    { GO_RUNE_BLAZ, TYPE_GARR },
    { GO_RUNE_MAZJ, TYPE_SHAZZRAH },
    { GO_RUNE_ZETH, TYPE_GEDDON },
    { GO_RUNE_THERI, TYPE_GOLEMAGG },
    { GO_RUNE_KORO, TYPE_SULFURON }
};

/*######
## go_molten_core_rune
######*/

struct go_molten_core_rune : public GameObjectScript
{
    go_molten_core_rune() : GameObjectScript("go_molten_core_rune") {}

    bool OnUse(Player* /*pPlayer*/, GameObject* pGo) override
    {
        ScriptedInstance* pInstance = (ScriptedInstance*)pGo->GetInstanceData();

        if (!pInstance)
        {
            return true;
        }

        for (uint8 i = 0; i < MAX_MOLTEN_RUNES; ++i)
        {
            if (pGo->GetEntry() == m_aMoltenCoreRunes[i].m_uiRuneEntry)
            {
                // check if boss is already dead - if not return true
                if (pInstance->GetData(m_aMoltenCoreRunes[i].m_uiType) != DONE)
                {
                    return true;
                }

                pInstance->SetData(m_aMoltenCoreRunes[i].m_uiType, SPECIAL);
                return false;
            }
        }

        return true;
    }
};

void AddSC_molten_core()
{
    Script* s;
    s = new go_molten_core_rune();
    s->RegisterSelf();

    //pNewScript = new Script;
    //pNewScript->Name = "go_molten_core_rune";
    //pNewScript->pGOUse = &GOUse_go_molten_core_rune;
    //pNewScript->RegisterSelf();
}
