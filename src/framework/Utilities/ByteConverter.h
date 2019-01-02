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

#ifndef MANGOS_BYTECONVERTER_H
#define MANGOS_BYTECONVERTER_H

/**
 * ByteConverter reverse your byte order. This is used for cross platform
 * where they have different endians.
 */

#include "Platform/Define.h"
#include <algorithm>

namespace ByteConverter
{
    template<size_t T>
    /**
     * @brief
     *
     * @param val
     */
    inline void convert(char* val)
    {
        std::swap(*val, *(val + T - 1));
        convert < T - 2 > (val + 1);
    }

    /**
     * @brief
     *
     * @param
     */
    template<> inline void convert<0>(char*) {}
    /**
     * @brief ignore central byte
     *
     * @param
     */
    template<> inline void convert<1>(char*) {}

    template<typename T>
    /**
     * @brief
     *
     * @param val
     */
    inline void apply(T* val)
    {
        convert<sizeof(T)>((char*)(val));
    }
}

#if MANGOS_ENDIAN == MANGOS_BIGENDIAN
/**
 * @brief
 *
 * @param val
 */
template<typename T> inline void EndianConvert(T& val) { ByteConverter::apply<T>(&val); }
/**
 * @brief
 *
 * @param
 */
template<typename T> inline void EndianConvertReverse(T&) { }
#else
template<typename T> inline void EndianConvert(T&) { }
template<typename T> inline void EndianConvertReverse(T& val) { ByteConverter::apply<T>(&val); }
#endif

/**
 * @brief will generate link error
 *
 * @param
 */
template<typename T> void EndianConvert(T*);
/**
 * @brief will generate link error
 *
 * @param
 */
template<typename T> void EndianConvertReverse(T*);

/**
 * @brief
 *
 * @param
 */
inline void EndianConvert(uint8&) { }
/**
 * @brief
 *
 * @param
 */
inline void EndianConvert(int8&)  { }
/**
 * @brief
 *
 * @param
 */
inline void EndianConvertReverse(uint8&) { }
/**
 * @brief
 *
 * @param
 */
inline void EndianConvertReverse(int8&) { }

#endif
