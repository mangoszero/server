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

#ifndef MANGOS_TYPELIST_H
#define MANGOS_TYPELIST_H

class TypeNull;

template<typename HEAD, typename TAIL>
/**
 * @brief TypeList is the most simple but yet the most powerfull class of all.
 *
 * It holds at compile time the different type of objects in a linked list.
 *
 */
struct TypeList
{
    /**
     * @brief
     *
     */
    typedef HEAD Head;
    /**
     * @brief
     *
     */
    typedef TAIL Tail;
};

// enough for now.. can be expand at any point in time as needed
#define TYPELIST_1(T1)                 TypeList<T1, TypeNull>
#define TYPELIST_2(T1, T2)             TypeList<T1, TYPELIST_1(T2) >
#define TYPELIST_3(T1, T2, T3)         TypeList<T1, TYPELIST_2(T2, T3) >
#define TYPELIST_4(T1, T2, T3, T4)     TypeList<T1, TYPELIST_3(T2, T3, T4) >
#define TYPELIST_5(T1, T2, T3, T4, T5) TypeList<T1, TYPELIST_4(T2, T3, T4, T5) >

#endif
