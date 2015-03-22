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

#ifndef DEF_ULDAMAN_H
#define DEF_ULDAMAN_H

enum
{
    MAX_ENCOUNTER               = 2,

    TYPE_ALTAR_EVENT            = 1,
    TYPE_ARCHAEDAS              = 2,
    DATA_EVENT_STARTER          = 3,

    GO_TEMPLE_DOOR_UPPER        = 124367,
    GO_TEMPLE_DOOR_LOWER        = 141869,
    GO_ANCIENT_VAULT            = 124369,
    GO_ANCIENT_TREASURE         = 141979,

    NPC_ARCHAEDAS               = 2748,
    NPC_CUSTODIAN               = 7309,
    NPC_HALLSHAPER              = 7077,
    NPC_GUARDIAN                = 7076,
    NPC_VAULT_WARDER            = 10120,
    NPC_STONE_KEEPER            = 4857,

    SPELL_STONED                = 10255,
    SPELL_FREEZE_ANIM           = 16245,

    EVENT_ID_ALTAR_KEEPER       = 2228,                     // spell 11568
    EVENT_ID_ALTAR_ARCHAEDAS    = 2268                      // spell 10340
};

class instance_uldaman : public ScriptedInstance
{
    public:
        instance_uldaman(Map* pMap);
        ~instance_uldaman() {}

        void Initialize() override;

        void OnObjectCreate(GameObject* pGo) override;
        void OnCreatureCreate(Creature* pCreature) override;

        void OnCreatureDeath(Creature* pCreature) override;
        void OnCreatureEvade(Creature* pCreature) override;

        void Update(uint32 uiDiff) override;

        void SetData(uint32 uiType, uint32 uiData) override;
        void SetData64(uint32 uiData, uint64 uiGuid) override;
        uint32 GetData(uint32 uiType) const override;
        uint64 GetData64(uint32 uiData) const override;

        void StartEvent(uint32 uiEventId, Player* pPlayer);

        Creature* GetClosestDwarfNotInCombat(Creature* pSearcher);

        const char* Save() const override { return m_strInstData.c_str(); }
        void Load(const char* chrIn) override;

    protected:
        void DoResetKeeperEvent();

        uint32 m_auiEncounter[MAX_ENCOUNTER];
        std::string m_strInstData;

        ObjectGuid m_playerGuid;

        uint32 m_uiKeeperCooldown;
        uint32 m_uiStoneKeepersFallen;

        GuidList m_lWardens;
        GuidList m_lKeepers;
};

#endif
