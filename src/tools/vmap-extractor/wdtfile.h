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

#ifndef WDTFILE_H
#define WDTFILE_H

#include "mpq_libmpq.h"
#include "wmo.h"
#include <string>
#include "stdlib.h"

class ADTFile;

/**
 * @brief
 *
 */
class WDTFile
{
    public:
        /**
         * @brief
         *
         * @param file_name
         * @param file_name1
         */
        WDTFile(char* file_name, char* file_name1);
        /**
         * @brief
         *
         */
        ~WDTFile(void);
        /**
         * @brief
         *
         * @param map_id
         * @param mapID
         * @return bool
         */
        bool init(char* map_id, unsigned int mapID);

        string* gWmoInstansName; /**< TODO */
        int gnWMO, nMaps; /**< TODO */

        /**
         * @brief
         *
         * @param x
         * @param z
         * @return ADTFile
         */
        ADTFile* GetMap(int x, int z);

    private:
        MPQFile WDT; /**< TODO */
        bool maps[64][64]; /**< TODO */
        string filename; /**< TODO */
};

#endif
