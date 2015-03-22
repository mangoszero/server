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

#ifndef SC_PET_H
#define SC_PET_H

// Using CreatureAI for now. Might change later and use PetAI (need to export for dll first)
class ScriptedPetAI : public CreatureAI
{
    public:
        explicit ScriptedPetAI(Creature* pCreature);
        ~ScriptedPetAI() {}

        void MoveInLineOfSight(Unit* /*pWho*/) override;

        void AttackStart(Unit* /*pWho*/) override;

        void AttackedBy(Unit* /*pAttacker*/) override;

        bool IsVisible(Unit* /*pWho*/) const override;

        void KilledUnit(Unit* /*pVictim*/) override {}

        void OwnerKilledUnit(Unit* /*pVictim*/) override {}

        void UpdateAI(const uint32 uiDiff) override;

        virtual void Reset() {}

        virtual void UpdatePetAI(const uint32 uiDiff);      // while in combat

        virtual void UpdatePetOOCAI(const uint32 /*uiDiff*/) {} // when not in combat

    protected:
        void ResetPetCombat();
};

#endif
