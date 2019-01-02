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

#ifndef MANGOS_H_TILEASSEMBLER
#define MANGOS_H_TILEASSEMBLER

#include <map>
#include <set>

#include "ModelInstance.h"
#include "WorldModel.h"
#include <G3D/Vector3.h>
#include <G3D/Matrix3.h>

namespace VMAP
{
    /**
     * @brief This Class is used to convert raw vector data into balanced BSP-Trees.
     *
     */
    class ModelPosition
    {
        private:
            G3D::Matrix3 iRotation; /**< TODO */
        public:
            G3D::Vector3 iPos; /**< TODO */
            G3D::Vector3 iDir; /**< TODO */
            float iScale; /**< TODO */
            /**
             * @brief
             *
             */
            void init()
            {
                iRotation = G3D::Matrix3::fromEulerAnglesZYX(G3D::pi() * iDir.y / 180.f, G3D::pi() * iDir.x / 180.f, G3D::pi() * iDir.z / 180.f);
            }
            /**
             * @brief
             *
             * @param pIn
             * @return G3D::Vector3
             */
            G3D::Vector3 transform(const G3D::Vector3& pIn) const;
            /**
             * @brief
             *
             * @param pBasePos
             */
            void moveToBasePos(const G3D::Vector3& pBasePos) { iPos -= pBasePos; }
    };

    /**
     * @brief
     *
     */
    typedef std::map<uint32, ModelSpawn> UniqueEntryMap;
    /**
     * @brief
     *
     */
    typedef std::multimap<uint32, uint32> TileMap;

    /**
     * @brief
     *
     */
    struct MapSpawns
    {
        UniqueEntryMap UniqueEntries; /**< TODO */
        TileMap TileEntries; /**< TODO */
    };

    /**
     * @brief
     *
     */
    typedef std::map<uint32, MapSpawns*> MapData;
    //===============================================

    /**
     * @brief
     *
     */
    struct GroupModel_Raw
    {
        uint32 mogpflags; /**< TODO */
        uint32 GroupWMOID; /**< TODO */

        G3D::AABox bounds; /**< TODO */
        uint32 liquidflags; /**< TODO */
        std::vector<MeshTriangle> triangles; /**< TODO */
        std::vector<G3D::Vector3> vertexArray; /**< TODO */
        class WmoLiquid* liquid; /**< TODO */

        /**
         * @brief
         *
         */
        GroupModel_Raw() : liquid(0) {}
        /**
         * @brief
         *
         */
        ~GroupModel_Raw();

        /**
         * @brief
         *
         * @param f
         * @return bool
         */
        bool Read(FILE* f);
    };

    /**
     * @brief
     *
     */
    struct WorldModel_Raw
    {
        uint32 RootWMOID; /**< TODO */
        std::vector<GroupModel_Raw> groupsArray; /**< TODO */

        /**
         * @brief
         *
         * @param path
         * @return bool
         */
        bool Read(const char* path, const char *RAW_VMAP_MAGIC);
    };

    /**
     * @brief
     *
     */
    class TileAssembler
    {
        private:
            std::string iDestDir; /**< TODO */
            std::string iSrcDir; /**< TODO */
            /**
             * @brief
             *
             * @param pName
             * @return bool
             */
            bool (*iFilterMethod)(char* pName);
            G3D::Table<std::string, unsigned int > iUniqueNameIds; /**< TODO */
            unsigned int iCurrentUniqueNameId; /**< TODO */
            MapData mapData; /**< TODO */
            std::set<std::string> spawnedModelFiles; /**< TODO */

        public:
            /**
             * @brief
             *
             * @param pSrcDirName
             * @param pDestDirName
             */
            TileAssembler(const std::string& pSrcDirName, const std::string& pDestDirName);
            /**
             * @brief
             *
             */
            virtual ~TileAssembler();

            /**
             * @brief
             *
             * @return bool
             */
            bool convertWorld2(const char *RAW_VMAP_MAGIC);
            /**
             * @brief
             *
             * @return bool
             */
            bool readMapSpawns();
            /**
             * @brief
             *
             * @param spawn
             * @return bool
             */
            bool calculateTransformedBound(ModelSpawn& spawn, const char *RAW_VMAP_MAGIC);

            /**
             * @brief
             *
             */
            void exportGameobjectModels(const char *RAW_VMAP_MAGIC);
            /**
             * @brief
             *
             * @param pModelFilename
             * @return bool
             */
            bool convertRawFile(const std::string& pModelFilename, const char *RAW_VMAP_MAGIC);
            /**
             * @brief
             *
             * @param )
             */
            void setModelNameFilterMethod(bool (*pFilterMethod)(char* pName)) { iFilterMethod = pFilterMethod; }
    };
}
#endif
