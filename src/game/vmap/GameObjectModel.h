/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

#include "Platform/Define.h"

#include <G3D/Matrix3.h>
#include <G3D/Vector3.h>
#include <G3D/AABox.h>
#include <G3D/Ray.h>
#include <G3D/Quat.h>
#include "DBCStructure.h"
#include "GameObject.h"


namespace VMAP
{
    class WorldModel;
}

/**
 * @brief
 *
 */
class GameObjectModel
{
    private:
        bool          isCollidable;

        std::string   iName;
        G3D::AABox    iBound;
        G3D::AABox    iModelBound;
        G3D::Vector3  iPos;
        G3D::Quat     iQuat;  //Note: this must be a unit quaternion!!!

        float         iScale;
        float         iInvScale;

        VMAP::WorldModel* iModel;
        GameObject const* iOwner;

        GameObjectModel() : isCollidable(false), iModel(NULL), iOwner(NULL) {}
        bool initialize(const GameObject* const pGo, const GameObjectDisplayInfoEntry* info);

    public:
        const G3D::AABox& GetBounds() const { return iBound; }
        const std::string& GetName() const { return iName; }

        ~GameObjectModel();

        const G3D::Vector3& GetPosition() const { return iPos;}
        void UpdateRotation(G3D::Quat const& q);
        const GameObject* GetOwner() const { return iOwner; }

        void SetCollidable(bool enabled) { isCollidable = enabled; }

        bool IntersectRay(const G3D::Ray& Ray, float& MaxDist, bool StopAtFirstHit) const;

        // returns the intersection point given from srcPoint down.
        // If absolute is true, srcPoint is in world space, else srcPoint is assumed in local space.
        // If method succeeds, dstPoint will be filled with local space coordinates of the intersection
        // else will be untouched
        bool GetIntersectPoint(const G3D::Vector3& srcPoint, G3D::Vector3& dstPoint, bool absolute = true) const;

        void GetLocalCoords(const G3D::Vector3& worldCoords, G3D::Vector3& localCoords); //NYI
        void GetWorldCoords(const G3D::Vector3& localCoords, G3D::Vector3& worldCoords); //NYI

        static GameObjectModel* Create(const GameObject* const pGo);
};
#endif
