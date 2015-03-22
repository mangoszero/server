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

#ifndef DEF_ZULGURUB_H
#define DEF_ZULGURUB_H

enum
{
    MAX_ENCOUNTER           = 8,
    MAX_PRIESTS             = 5,

    TYPE_JEKLIK             = 0,
    TYPE_VENOXIS            = 1,
    TYPE_MARLI              = 2,
    TYPE_THEKAL             = 3,
    TYPE_ARLOKK             = 4,
    TYPE_OHGAN              = 5,                            // Do not change, used by Acid
    TYPE_LORKHAN            = 6,
    TYPE_ZATH               = 7,

    NPC_LORKHAN             = 11347,
    NPC_ZATH                = 11348,
    NPC_THEKAL              = 14509,
    NPC_JINDO               = 11380,
    NPC_HAKKAR              = 14834,
    NPC_PANTHER_TRIGGER     = 15091,
    NPC_BLOODLORD_MANDOKIR  = 11382,
    NPC_MARLI               = 14510,

    GO_SPIDER_EGG           = 179985,
    GO_GONG_OF_BETHEKK      = 180526,
    GO_FORCEFIELD           = 180497,

    SAY_MINION_DESTROY      = -1309022,
    SAY_HAKKAR_PROTECT      = -1309023,

    HP_LOSS_PER_PRIEST      = 60000,

    AREATRIGGER_ENTER       = 3958,
    AREATRIGGER_ALTAR       = 3960,
};

static const float aMandokirDownstairsPos[3] = { -12196.30f, -1948.37f, 130.31f};

class instance_zulgurub : public ScriptedInstance
{
    public:
        instance_zulgurub(Map* pMap);
        ~instance_zulgurub() {}

        void Initialize() override;
        // IsEncounterInProgress() const override { return false; }  // not active in Zul'Gurub

        void OnCreatureCreate(Creature* pCreature) override;
        void OnObjectCreate(GameObject* pGo) override;

        void SetData(uint32 uiType, uint32 uiData) override;
        uint32 GetData(uint32 uiType) const override;

        const char* Save() const override { return m_strInstData.c_str(); }
        void Load(const char* chrIn) override;

        void DoYellAtTriggerIfCan(uint32 uiTriggerId);

        Creature* SelectRandomPantherTrigger(bool bIsLeft);

    protected:
        void DoLowerHakkarHitPoints();

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        GuidList m_lRightPantherTriggerGUIDList;
        GuidList m_lLeftPantherTriggerGUIDList;
        GuidList m_lSpiderEggGUIDList;

        bool m_bHasIntroYelled;
        bool m_bHasAltarYelled;
};

#endif
