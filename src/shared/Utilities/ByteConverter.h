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

#ifndef MANGOS_BYTECONVERTER_H
#define MANGOS_BYTECONVERTER_H

/**
 * @file ByteConverter.h
 * @brief Byte order conversion utilities for cross-platform compatibility
 *
 * ByteConverter provides functions to reverse byte order for cross-platform
 * compatibility where systems have different endianness (big-endian vs little-endian).
 * This is essential for network protocol handling in World of Warcraft.
 */

#include "Platform/Define.h"
#include <algorithm>

namespace ByteConverter
{
    /**
     * @brief Reverse byte order for a value of size T
     * @param val Pointer to value to convert
     */
    template<size_t T>
    inline void convert(char* val)
    {
        std::swap(*val, *(val + T - 1));
        convert < T - 2 > (val + 1);
    }

    /**
     * @brief Template specialization for size 0 (no-op)
     */
    template<> inline void convert<0>(char*) {}
    /**
     * @brief Template specialization for size 1 (no-op, single byte)
     */
    template<> inline void convert<1>(char*) {}

    /**
     * @brief Apply byte conversion to a value
     * @param val Pointer to value to convert
     */
    template<typename T>
    inline void apply(T* val)
    {
        convert<sizeof(T)>((char*)(val));
    }
}

#if MANGOS_ENDIAN == MANGOS_BIGENDIAN
/**
 * @brief Convert from host to little-endian byte order (big-endian host)
 * @param val Value to convert
 */
template<typename T> inline void EndianConvert(T& val) { ByteConverter::apply<T>(&val); }
/**
 * @brief Reverse byte order (no-op on big-endian host)
 * @param val Value to convert
 */
template<typename T> inline void EndianConvertReverse(T&) { }
#else
template<typename T> inline void EndianConvert(T&) { }
template<typename T> inline void EndianConvertReverse(T& val) { ByteConverter::apply<T>(&val); }
#endif

/**
 * @brief Deleted template to prevent pointer conversion (will generate link error)
 */
template<typename T> void EndianConvert(T*);
/**
 * @brief Deleted template to prevent pointer conversion (will generate link error)
 */
template<typename T> void EndianConvertReverse(T*);

/**
 * @brief No-op for uint8 (single byte)
 */
inline void EndianConvert(uint8&) { }
/**
 * @brief No-op for int8 (single byte)
 */
inline void EndianConvert(int8&)  { }
/**
 * @brief No-op for uint8 (single byte)
 */
inline void EndianConvertReverse(uint8&) { }
/**
 * @brief No-op for int8 (single byte)
 */
inline void EndianConvertReverse(int8&) { }

#endif
