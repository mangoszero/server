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

#ifndef MANGOS_H_GRIDREFERENCE
#define MANGOS_H_GRIDREFERENCE

#include "Utilities/LinkedReference/Reference.h"

template<class OBJECT> class GridRefManager;

template<class OBJECT>
/**
 * @brief
 *
 */
class GridReference : public Reference<GridRefManager<OBJECT>, OBJECT>
{
    protected:

        /**
         * @brief
         *
         */
        void targetObjectBuildLink() override
        {
            // called from link()
            this->getTarget()->insertFirst(this);
            this->getTarget()->incSize();
        }

        /**
         * @brief
         *
         */
        void targetObjectDestroyLink() override
        {
            // called from unlink()
            if (this->isValid())
                { this->getTarget()->decSize(); }
        }

        /**
         * @brief
         *
         */
        void sourceObjectDestroyLink() override
        {
            // called from invalidate()
            this->getTarget()->decSize();
        }

    public:
        /**
         * @brief
         *
         */
        GridReference()
            : Reference<GridRefManager<OBJECT>, OBJECT>()
        {
        }

        /**
         * @brief
         *
         */
        ~GridReference()
        {
            this->unlink();
        }

        /**
         * @brief
         *
         * @return GridReference
         */
        GridReference* next()
        {
            return (GridReference*)Reference<GridRefManager<OBJECT>, OBJECT>::next();
        }
};

#endif
