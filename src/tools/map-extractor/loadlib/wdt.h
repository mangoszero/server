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

#ifndef WDT_H
#define WDT_H

#include "loadlib.h"

//**************************************************************************************
// WDT file class and structures
//**************************************************************************************
#define WDT_MAP_SIZE 64

/**
 * @brief
 *
 */
class wdt_MWMO
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
    public:
        uint32 size; /**< TODO */
        /**
         * @brief
         *
         * @return bool
         */
        bool prepareLoadedData();
};

/**
 * @brief
 *
 */
class wdt_MPHD
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
    public:
        uint32 size; /**< TODO */

        uint32 data1; /**< TODO */
        uint32 data2; /**< TODO */
        uint32 data3; /**< TODO */
        uint32 data4; /**< TODO */
        uint32 data5; /**< TODO */
        uint32 data6; /**< TODO */
        uint32 data7; /**< TODO */
        uint32 data8; /**< TODO */
        /**
         * @brief
         *
         * @return bool
         */
        bool   prepareLoadedData();
};

/**
 * @brief
 *
 */
class wdt_MAIN
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
    public:
        uint32 size; /**< TODO */

        /**
         * @brief
         *
         */
        struct adtData
        {
            uint32 exist; /**< TODO */
            uint32 data1; /**< TODO */
        } adt_list[64][64]; /**< TODO */

        /**
         * @brief
         *
         * @return bool
         */
        bool   prepareLoadedData();
};

/**
 * @brief
 *
 */
class WDT_file : public FileLoader
{
    public:
        /**
         * @brief
         *
         * @return bool
         */
        bool   prepareLoadedData();

        /**
         * @brief
         *
         */
        WDT_file();
        /**
         * @brief
         *
         */
        ~WDT_file();
        /**
         * @brief
         *
         */
        void free();

        wdt_MPHD* mphd; /**< TODO */
        wdt_MAIN* main; /**< TODO */
        wdt_MWMO* wmo; /**< TODO */
};

#endif
