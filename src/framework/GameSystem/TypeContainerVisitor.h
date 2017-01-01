/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
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

#ifndef MANGOS_TYPECONTAINERVISITOR_H
#define MANGOS_TYPECONTAINERVISITOR_H

/*
 * @class TypeContainerVisitor is implemented as a visitor pattern.  It is
 * a visitor to the TypeMapContainer or ContainerMapList.  The visitor has
 * to overload its types as a visit method is called.
 */

#include "Platform/Define.h"
#include "TypeContainer.h"

// forward declaration
template<class T, class Y> class TypeContainerVisitor;

template<class VISITOR, class TYPE_CONTAINER>
/**
 * @brief visitor helper
 *
 * @param v
 * @param c
 */
void VisitorHelper(VISITOR& v, TYPE_CONTAINER& c)
{
    v.Visit(c);
}

template<class VISITOR>
/**
 * @brief terminate condition container map list
 *
 * @param
 * @param
 */
void VisitorHelper(VISITOR& /*v*/, ContainerMapList<TypeNull>& /*c*/)
{
}

template<class VISITOR, class T>
/**
 * @brief
 *
 * @param v
 * @param c
 */
void VisitorHelper(VISITOR& v, ContainerMapList<T>& c)
{
    v.Visit(c._element);
}

template<class VISITOR, class H, class T>
/**
 * @brief recursion container map list
 *
 * @param v
 * @param ContainerMapList<TypeList<H
 * @param c
 */
void VisitorHelper(VISITOR& v, ContainerMapList<TypeList<H, T> >& c)
{
    VisitorHelper(v, c._elements);
    VisitorHelper(v, c._TailElements);
}

template<class VISITOR, class OBJECT_TYPES>
/**
 * @brief for TypeMapContainer
 *
 * @param v
 * @param c
 */
void VisitorHelper(VISITOR& v, TypeMapContainer<OBJECT_TYPES>& c)
{
    VisitorHelper(v, c.GetElements());
}

template<class VISITOR, class TYPE_CONTAINER>
/**
 * @brief
 *
 */
class TypeContainerVisitor
{
    public:

        /**
         * @brief
         *
         * @param v
         */
        TypeContainerVisitor(VISITOR& v)
            : i_visitor(v)
        {
        }

        /**
         * @brief
         *
         * @param c
         */
        void Visit(TYPE_CONTAINER& c)
        {
            VisitorHelper(i_visitor, c);
        }

        /**
         * @brief
         *
         * @param c
         */
        void Visit(const TYPE_CONTAINER& c) const
        {
            VisitorHelper(i_visitor, c);
        }

    private:

        VISITOR& i_visitor; /**< TODO */
};

#endif
