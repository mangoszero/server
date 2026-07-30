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


#ifndef MANGOS_TYPECONTAINERVISITOR_H
#define MANGOS_TYPECONTAINERVISITOR_H

/*
 * @class TypeContainerVisitor is implemented as a visitor pattern.  It is
 * a visitor to the TypeMapContainer or ContainerMapList.  The visitor has
 * to overload its types as a visit method is called.
 */

#include <utility>
#include "TypeContainer.h"

template<class VISITOR, class CONTAINER>
class TypeContainerVisitor
{
    public:
        TypeContainerVisitor(VISITOR& v) : i_visitor(v){}
        void Visit(CONTAINER& c)
        {
            c.accept(i_visitor);
        }
        void Visit(const CONTAINER& c) const
        {
            c.accept(i_visitor);
        }
    private:
        VISITOR& i_visitor;
};

#endif
