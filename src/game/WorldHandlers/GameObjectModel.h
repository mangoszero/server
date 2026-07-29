/**
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * World of Warcraft, and all World of Warcraft or Warcraft art, images,
 * and lore are copyrighted by Blizzard Entertainment, Inc.
 */

#ifndef MANGOSSERVER_GAMEOBJECTMODEL_H
#define MANGOSSERVER_GAMEOBJECTMODEL_H

// One game object's collidable body: a shared model, a placement, and the world box it
// occupies. The geometry itself is held once per display id by GoModelStore -- a keep's
// gate appears hundreds of times over and is stored once.

#include "Platform/Define.h"
#include "terrain/Column.hpp"
#include "terrain/ICollisionModel.hpp"

#include <cfloat>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class GameObject;
class DynamicCollision;

class GameObjectModel
{
    public:
        ~GameObjectModel() = default;

        static GameObjectModel* Create(const GameObject* pGo);

        // A body with no GameObject behind it: the pose is given rather than read.
        // The owner is only ever a source of position, rotation and scale, so nothing
        // else in this class needs one -- which is also what makes it testable without
        // standing up a world.
        static GameObjectModel* CreateStandalone(
            std::shared_ptr<const world::terrain::ICollisionModel> model,
            const Geometry::Transform& xf, uint32 phaseMask);

        const Geometry::Aabb& GetBounds() const { return m_bounds; }
        const Geometry::Vector3& GetPosition() const { return m_xf.pos; }
        const GameObject* GetOwner() const { return m_owner; }
        uint32 GetPhaseMask() const { return m_phaseMask; }
        bool IsCollidable() const { return m_collidable; }

        void SetCollidable(bool enabled) { m_collidable = enabled; }
        void SetPhaseMask(uint32 phaseMask = 0) { m_phaseMask = phaseMask; }

        // Re-derives the placement and the world box from the owner's current pose.
        // Defined in GameObjectModelOwner.cpp -- the only part of this class that knows
        // what a GameObject is.
        void UpdatePose();

        // Sets the placement directly and re-derives the world box.
        void SetPose(const Geometry::Transform& xf);

        // Nearest hit of the world segment a->b as a fraction of it, or a value above 1
        // when this body does not block. The ray is pulled into model space rather than
        // the geometry pushed into world space.
        float SegmentHitFraction(const Geometry::Vector3& a, const Geometry::Vector3& b) const;

        // Appends every surface of this body crossing the window over (x,y).
        void AddSurfaces(float x, float y, float zTop, float zBottom,
                         world::terrain::Column& out) const;

    private:
        friend class DynamicCollision;

        GameObjectModel() = default;
        bool Initialize(const GameObject* pGo, uint32 displayId);
        void DeriveBounds();

        bool m_collidable = false;
        uint32 m_phaseMask = 0;
        std::shared_ptr<const world::terrain::ICollisionModel> m_model;
        const GameObject* m_owner = nullptr;

        Geometry::Transform m_xf;
        Geometry::Aabb m_bounds;

        // Which grid cells this body is filed under, and a stamp so one query visits it
        // once however many cells it spans. Owned by DynamicCollision.
        std::vector<uint32_t> m_cells;
        mutable uint32_t m_epoch = 0;
};

#endif
