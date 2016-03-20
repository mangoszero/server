/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2016  MaNGOS project <https://getmangos.eu>
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
        bool collision_enabled; /**< TODO */
        G3D::AABox iBound; /**< TODO */
        G3D::Matrix3 iInvRot; /**< TODO */
        G3D::Vector3 iPos; /**< TODO */
        //G3D::Vector3 iRot;
        float iInvScale; /**< TODO */
        float iScale; /**< TODO */
        VMAP::WorldModel* iModel; /**< TODO */

        /**
         * @brief
         *
         */
        GameObjectModel() : collision_enabled(false), iModel(NULL) {}
        /**
         * @brief
         *
         * @param pGo
         * @param info
         * @return bool
         */
        bool initialize(const GameObject* const pGo, const GameObjectDisplayInfoEntry* info);

    public:
        std::string name; /**< TODO */

        /**
         * @brief
         *
         * @return const G3D::AABox
         */
        const G3D::AABox& getBounds() const { return iBound; }

        /**
         * @brief
         *
         */
        ~GameObjectModel();

        /**
         * @brief
         *
         * @return const G3D::Vector3
         */
        const G3D::Vector3& getPosition() const { return iPos;}

        /** Enables\disables collision. */
        /**
         * @brief
         *
         */
        void disable() { collision_enabled = false;}
        /**
         * @brief
         *
         * @param enabled
         */
        void enable(bool enabled) { collision_enabled = enabled;}
        /**
         * @brief
         *
         * @param Ray
         * @param MaxDist
         * @param StopAtFirstHit
         * @return bool
         */
        bool intersectRay(const G3D::Ray& Ray, float& MaxDist, bool StopAtFirstHit) const;

        /**
         * @brief
         *
         * @param pGo
         * @return GameObjectModel
         */
        static GameObjectModel* construct(const GameObject* const pGo);
};
#endif
