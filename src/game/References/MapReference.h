/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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

#ifndef MANGOS_H_MAPREFERENCE
#define MANGOS_H_MAPREFERENCE

#include "Utilities/LinkedReference/Reference.h"
#include "Map.h"

/**
 * @brief Map reference class
 *
 * Manages the reference between a Map and a Player.
 */
class MapReference : public Reference<Map, Player>
{
    protected:
        /**
         * @brief Build link to target object
         *
         * Called from link().
         */
        void targetObjectBuildLink() override
        {
            getTarget()->m_mapRefManager.insertFirst(this);
            getTarget()->m_mapRefManager.incSize();
        }

        /**
         * @brief Destroy link to target object
         *
         * Called from unlink().
         */
        void targetObjectDestroyLink() override
        {
            if (isValid())
            {
                getTarget()->m_mapRefManager.decSize();
            }
        }

        /**
         * @brief Destroy link from source object
         *
         * Called from invalidate().
         */
        void sourceObjectDestroyLink() override
        {
            getTarget()->m_mapRefManager.decSize();
        }

    public:
        /**
         * @brief Constructor
         */
        MapReference() : Reference<Map, Player>() {}

        /**
         * @brief Destructor
         */
        ~MapReference() { unlink(); }

        /**
         * @brief Get next reference
         * @return Next map reference
         */
        MapReference* next() { return (MapReference*)Reference<Map, Player>::next(); }

        /**
         * @brief Get next reference (const)
         * @return Next map reference (const)
         */
        MapReference const* next() const { return (MapReference const*)Reference<Map, Player>::next(); }

        /**
         * @brief Get previous reference (no check)
         * @return Previous map reference
         */
        MapReference* nockeck_prev() { return (MapReference*)Reference<Map, Player>::nocheck_prev(); }

        /**
         * @brief Get previous reference (no check, const)
         * @return Previous map reference (const)
         */
        MapReference const* nocheck_prev() const { return (MapReference const*)Reference<Map, Player>::nocheck_prev(); }
};
#endif
