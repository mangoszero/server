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

#ifndef MANGOS_H_REFMANAGER
#define MANGOS_H_REFMANAGER

//=====================================================

#include "Utilities/LinkedList.h"
#include "Utilities/LinkedReference/Reference.h"

template <class TO, class FROM>
/**
 * @brief
 *
 */
class RefManager : public LinkedListHead
{
    public:

        /**
         * @brief
         *
         */
        typedef LinkedListHead::Iterator<Reference<TO, FROM> > iterator;
        /**
         * @brief
         *
         */
        RefManager() {}
        /**
         * @brief
         *
         */
        virtual ~RefManager() { clearReferences(); }

        /**
         * @brief
         *
         * @return Reference<TO, FROM>
         */
        Reference<TO, FROM>*       getFirst()       { return ((Reference<TO, FROM>*) LinkedListHead::getFirst()); }
        /**
         * @brief
         *
         * @return const Reference<TO, FROM>
         */
        Reference<TO, FROM> const* getFirst() const { return ((Reference<TO, FROM> const*) LinkedListHead::getFirst()); }
        /**
         * @brief
         *
         * @return Reference<TO, FROM>
         */
        Reference<TO, FROM>*       getLast()       { return ((Reference<TO, FROM>*) LinkedListHead::getLast()); }
        /**
         * @brief
         *
         * @return const Reference<TO, FROM>
         */
        Reference<TO, FROM> const* getLast() const { return ((Reference<TO, FROM> const*) LinkedListHead::getLast()); }

        /**
         * @brief
         *
         * @return iterator
         */
        iterator begin() { return iterator(getFirst()); }
        /**
         * @brief
         *
         * @return iterator
         */
        iterator end() { return iterator(NULL); }
        /**
         * @brief
         *
         * @return iterator
         */
        iterator rbegin() { return iterator(getLast()); }
        /**
         * @brief
         *
         * @return iterator
         */
        iterator rend() { return iterator(NULL); }

        /**
         * @brief
         *
         */
        void clearReferences()
        {
            LinkedListElement* ref;
            while ((ref = getFirst()) != NULL)
            {
                ((Reference<TO, FROM>*) ref)->invalidate();
                ref->delink();                              // the delink might be already done by invalidate(), but doing it here again does not hurt and insures an empty list
            }
        }
};

//=====================================================

#endif
