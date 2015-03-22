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

#ifndef SC_GUARDAI_H
#define SC_GUARDAI_H

enum
{
    GENERIC_CREATURE_COOLDOWN       = 5000,

    SAY_GUARD_SIL_AGGRO1            = -1000198,
    SAY_GUARD_SIL_AGGRO2            = -1000199,
    SAY_GUARD_SIL_AGGRO3            = -1000200,

    NPC_CENARION_INFANTRY           = 15184
};

enum eShattrathGuard
{
    SPELL_BANISHED_SHATTRATH_A      = 36642,
    SPELL_BANISHED_SHATTRATH_S      = 36671,
    SPELL_BANISH_TELEPORT           = 36643,
    SPELL_EXILE                     = 39533
};

struct guardAI : public ScriptedAI
{
    public:
        explicit guardAI(Creature* pCreature);
        ~guardAI() {}

        uint32 m_uiGlobalCooldown;                          // This variable acts like the global cooldown that players have (1.5 seconds)
        uint32 m_uiBuffTimer;                               // This variable keeps track of buffs

        void Reset() override;

        void Aggro(Unit* pWho) override;

        void JustDied(Unit* /*pKiller*/) override;

        void UpdateAI(const uint32 uiDiff) override;

        // Commonly used for guards in main cities
        void DoReplyToTextEmote(uint32 uiTextEmote);
};

struct guardAI_orgrimmar : public guardAI
{
    guardAI_orgrimmar(Creature* pCreature) : guardAI(pCreature) {}

    void ReceiveEmote(Player* pPlayer, uint32 uiTextEmote) override;
};

struct guardAI_stormwind : public guardAI
{
    guardAI_stormwind(Creature* pCreature) : guardAI(pCreature) {}

    void ReceiveEmote(Player* pPlayer, uint32 uiTextEmote) override;
};

#endif
