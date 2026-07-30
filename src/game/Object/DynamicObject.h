/**
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
 * along with this program. If not, see <https://www.gnu.org/licenses/>.
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOSSERVER_DYNAMICOBJECT_H
#define MANGOSSERVER_DYNAMICOBJECT_H

#include "Object.h"
#include "DBCEnums.h"
#include "Unit.h"

enum DynamicObjectType
{
    DYNAMIC_OBJECT_PORTAL           = 0x0,      // unused
    DYNAMIC_OBJECT_AREA_SPELL       = 0x1,
    DYNAMIC_OBJECT_FARSIGHT_FOCUS   = 0x2,
};

struct SpellEntry;

class DynamicObject : public WorldObject
{
    public:
        explicit DynamicObject();

        void AddToWorld() override;
        void RemoveFromWorld() override;

        bool Create(uint32 guidlow, Unit* caster, uint32 spellId, SpellEffectIndex effIndex, float x, float y, float z, int32 duration, float radius, DynamicObjectType type);
        void Update(uint32 update_diff, uint32 p_time) override;
        void Delete();
        uint32 GetSpellId() const { return m_spellId; }
        SpellEffectIndex GetEffIndex() const { return m_effIndex; }
        uint32 GetDuration() const { return m_aliveDuration; }
        ObjectGuid const& GetCasterGuid() const { return GetGuidValue(DYNAMICOBJECT_CASTER); }
        Unit* GetCaster() const;
        float GetRadius() const { return m_radius; }
        DynamicObjectType GetType() const { return (DynamicObjectType)GetByteValue(DYNAMICOBJECT_BYTES, 0); }
        bool IsAffecting(Unit* unit) const { return m_affected.find(unit->GetObjectGuid()) != m_affected.end(); }
        void AddAffected(Unit* unit) { m_affected.insert(unit->GetObjectGuid()); }
        void RemoveAffected(Unit* unit) { m_affected.erase(unit->GetObjectGuid()); }
        void Delay(int32 delaytime);

        bool IsHostileTo(Unit const* unit) const override;
        bool IsFriendlyTo(Unit const* unit) const override;

        float ComputeBoundingRadius() const override      // overwrite WorldObject version
        {
            return 0.0f;                                    // dynamic object not have real interact size
        }

        bool IsControlledByPlayer() const override
        {
            return GetCasterGuid().IsPlayer();
        }

        bool IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const override;

        /**
         * @brief Anchor this area effect to a DECK spot rather than a world point.
         *
         * A persistent area aura cast on a transport belongs to the deck, not to the patch
         * of sea the ship is leaving behind. Its true coordinates are the local offset,
         * because on a deck the world transform is a lie and the offset is the only thing
         * that does not move.
         */
        void BindToTransport(ObjectGuid transportGuid, float lx, float ly, float lz);

        bool OnTransport() const { return bool(m_transportGuid); }

        /**
         * @brief Is `target` inside the effect, measured the honest way?
         *
         * A deck effect and a boarded target are both points in the vessel's own space, so
         * the distance between them is their LOCAL separation -- exact, and independent of
         * wherever the server imagines the hull to be. A target not on this vessel is not
         * in a deck effect at all. Only an ordinary world effect falls back to the world
         * distance.
         */
        bool IsInEffectRange(Unit const* target) const;

        GridReference<DynamicObject>& GetGridRef()
        {
            return m_gridRef;
        }

    protected:
        uint32 m_spellId;
        SpellEffectIndex m_effIndex;
        int32 m_aliveDuration;
        float m_radius;                                     // radius apply persistent effect, 0 = no persistent effect
        bool m_positive;
        GuidSet m_affected;

        /// The vessel this effect rides, or an empty guid for an ordinary world effect.
        ObjectGuid m_transportGuid;
        float m_transOffsetX, m_transOffsetY, m_transOffsetZ;
    private:
        GridReference<DynamicObject> m_gridRef;
};
#endif
