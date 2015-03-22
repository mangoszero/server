/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
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

#ifndef LOAD_LIB_H
#define LOAD_LIB_H

#ifdef WIN32
typedef __int64            int64;
typedef __int32            int32;
typedef __int16            int16;
typedef __int8             int8;
typedef unsigned __int64   uint64;
typedef unsigned __int32   uint32;
typedef unsigned __int16   uint16;
typedef unsigned __int8    uint8;
#else
#include <stdint.h>
#ifndef uint64_t
#ifdef __linux__
#include <linux/types.h>
#endif
#endif

/**
 * @brief
 *
 */
typedef int64_t            int64;
/**
 * @brief
 *
 */
typedef int32_t            int32;
/**
 * @brief
 *
 */
typedef int16_t            int16;
/**
 * @brief
 *
 */
typedef int8_t             int8;
/**
 * @brief
 *
 */
typedef uint64_t           uint64;
/**
 * @brief
 *
 */
typedef uint32_t           uint32;
/**
 * @brief
 *
 */
typedef uint16_t           uint16;
/**
 * @brief
 *
 */
typedef uint8_t            uint8;
#endif

#define FILE_FORMAT_VERSION    18

/**
 * @brief File version chunk
 *
 */
struct file_MVER
{
    /**
     * @brief
     *
     */
    union
    {
        uint32 fcc; /**< TODO */
        char   fcc_txt[4]; /**< TODO */
    };
    uint32 size; /**< TODO */
    uint32 ver; /**< TODO */
};

/**
 * @brief
 *
 */
class FileLoader
{
        uint8*  data; /**< TODO */
        uint32  data_size; /**< TODO */
    public:
        /**
         * @brief
         *
         * @return bool
         */
        virtual bool prepareLoadedData();
        /**
         * @brief
         *
         * @return uint8
         */
        uint8* GetData()     {return data;}
        /**
         * @brief
         *
         * @return uint32
         */
        uint32 GetDataSize() {return data_size;}

        file_MVER* version; /**< TODO */
        /**
         * @brief
         *
         */
        FileLoader();
        /**
         * @brief
         *
         */
        ~FileLoader();
        /**
         * @brief
         *
         * @param filename
         * @param log
         * @return bool
         */
        bool loadFile(char* filename, bool log = true);
        /**
         * @brief
         *
         */
        virtual void free();
};
#endif
