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

#ifndef MANGOS_H_IVMAPMANAGER
#define MANGOS_H_IVMAPMANAGER

#include <string>
#include <Platform/Define.h>

//===========================================================

/**
This is the minimum interface to the VMapMamager.
*/

namespace VMAP
{
    /**
     * @brief
     *
     */
    enum VMAPLoadResult
    {
        VMAP_LOAD_RESULT_ERROR,
        VMAP_LOAD_RESULT_OK,
        VMAP_LOAD_RESULT_IGNORED
    };

#define VMAP_INVALID_HEIGHT       -100000.0f            // for check
#define VMAP_INVALID_HEIGHT_VALUE -200000.0f            // real assigned value in unknown height case

    //===========================================================
    /**
     * @brief
     *
     */
    class IVMapManager
    {
        private:
            bool iEnableLineOfSightCalc; /**< TODO */
            bool iEnableHeightCalc; /**< TODO */

        public:
            /**
             * @brief
             *
             */
            IVMapManager() : iEnableLineOfSightCalc(true), iEnableHeightCalc(true) {}

            /**
             * @brief
             *
             */
            virtual ~IVMapManager(void) {}

            /**
             * @brief
             *
             * @param pBasePath
             * @param pMapId
             * @param x
             * @param y
             * @return VMAPLoadResult
             */
            virtual VMAPLoadResult loadMap(const char* pBasePath, unsigned int pMapId, int x, int y) = 0;

            /**
             * @brief
             *
             * @param pBasePath
             * @param pMapId
             * @param x
             * @param y
             * @return bool
             */
            virtual bool existsMap(const char* pBasePath, unsigned int pMapId, int x, int y) = 0;

            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             */
            virtual void unloadMap(unsigned int pMapId, int x, int y) = 0;
            /**
             * @brief
             *
             * @param pMapId
             */
            virtual void unloadMap(unsigned int pMapId) = 0;

            /**
             * @brief
             *
             * @param pMapId
             * @param x1
             * @param y1
             * @param z1
             * @param x2
             * @param y2
             * @param z2
             * @return bool
             */
            virtual bool isInLineOfSight(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2) = 0;
            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             * @param z
             * @param maxSearchDist
             * @return float
             */
            virtual float getHeight(unsigned int pMapId, float x, float y, float z, float maxSearchDist) = 0;
            /**
             * @brief test if we hit an object. return true if we hit one. rx,ry,rz will hold the hit position or the dest position, if no intersection was found
             * return a position, that is pReduceDist closer to the origin
             *
             * @param pMapId
             * @param x1
             * @param y1
             * @param z1
             * @param x2
             * @param y2
             * @param z2
             * @param rx
             * @param ry
             * @param rz
             * @param pModifyDist
             * @return bool
             */
            virtual bool getObjectHitPos(unsigned int pMapId, float x1, float y1, float z1, float x2, float y2, float z2, float& rx, float& ry, float& rz, float pModifyDist) = 0;
            /**
             * @brief send debug commands
             *
             * @param pCommand
             * @return bool
             */
            virtual bool processCommand(char* pCommand) = 0;

            /**
             * @brief Enable/disable LOS calculation
             *
             * It is enabled by default. If it is enabled in mid game the maps have to loaded manually
             *
             * @param pVal
             */
            void setEnableLineOfSightCalc(bool pVal) { iEnableLineOfSightCalc = pVal; }
            /**
             * @brief Enable/disable model height calculation
             *
             * It is enabled by default. If it is enabled in mid game the maps have to loaded manually
             *
             * @param pVal
             */
            void setEnableHeightCalc(bool pVal) { iEnableHeightCalc = pVal; }

            /**
             * @brief
             *
             * @return bool
             */
            bool isLineOfSightCalcEnabled() const { return(iEnableLineOfSightCalc); }
            /**
             * @brief
             *
             * @return bool
             */
            bool isHeightCalcEnabled() const { return(iEnableHeightCalc); }
            /**
             * @brief
             *
             * @return bool
             */
            bool isMapLoadingEnabled() const { return(iEnableLineOfSightCalc || iEnableHeightCalc); }

            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             * @return std::string
             */
            virtual std::string getDirFileName(unsigned int pMapId, int x, int y) const = 0;
            /**
             * @brief Query world model area info.
             *
             * @param pMapId
             * @param x
             * @param y
             * @param z gets adjusted to the ground height for which this are info is valid
             * @param flags
             * @param adtId
             * @param rootId
             * @param groupId
             * @return bool
             */
            virtual bool getAreaInfo(unsigned int pMapId, float x, float y, float& z, uint32& flags, int32& adtId, int32& rootId, int32& groupId) const = 0;
            /**
             * @brief
             *
             * @param pMapId
             * @param x
             * @param y
             * @param z
             * @param ReqLiquidType
             * @param level
             * @param floor
             * @param type
             * @return bool
             */
            virtual bool GetLiquidLevel(uint32 pMapId, float x, float y, float z, uint8 ReqLiquidType, float& level, float& floor, uint32& type) const = 0;
    };
}
#endif
