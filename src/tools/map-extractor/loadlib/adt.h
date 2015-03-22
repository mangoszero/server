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

#ifndef ADT_H
#define ADT_H

#include "loadlib.h"

#define TILESIZE (533.33333f)
#define CHUNKSIZE ((TILESIZE) / 16.0f)
#define UNITSIZE (CHUNKSIZE / 8.0f)

/**
 * @brief
 *
 */
enum LiquidType
{
    LIQUID_TYPE_MAGMA = 0,
    LIQUID_TYPE_OCEAN = 1,
    LIQUID_TYPE_SLIME = 2,
    LIQUID_TYPE_WATER = 3
};

//**************************************************************************************
// ADT file class
//**************************************************************************************
#define ADT_CELLS_PER_GRID    16
#define ADT_CELL_SIZE         8
#define ADT_GRID_SIZE         (ADT_CELLS_PER_GRID*ADT_CELL_SIZE)

/**
 * @brief Adt file height map chunk
 *
 */
class adt_MCVT
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
    public:
        float height_map[(ADT_CELL_SIZE + 1) * (ADT_CELL_SIZE + 1) + ADT_CELL_SIZE* ADT_CELL_SIZE]; /**< TODO */

        /**
         * @brief
         *
         * @return bool
         */
        bool  prepareLoadedData();
};

/**
 * @brief Adt file liquid map chunk (old)
 *
 */
class adt_MCLQ
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
    public:
        float height1; /**< TODO */
        float height2; /**< TODO */
        /**
         * @brief
         *
         */
        struct liquid_data
        {
            uint32 light; /**< TODO */
            float  height; /**< TODO */
        } liquid[ADT_CELL_SIZE + 1][ADT_CELL_SIZE + 1]; /**< TODO */

        // 1<<0 - ochen
        // 1<<1 - lava/slime
        // 1<<2 - water
        // 1<<6 - all water
        // 1<<7 - dark water
        // == 0x0F - not show liquid
        uint8 flags[ADT_CELL_SIZE][ADT_CELL_SIZE]; /**< TODO */
        uint8 data[84]; /**< TODO */
        /**
         * @brief
         *
         * @return bool
         */
        bool  prepareLoadedData();
};

/**
 * @brief Adt file cell chunk
 *
 */
class adt_MCNK
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
    public:
        uint32 flags; /**< TODO */
        uint32 ix; /**< TODO */
        uint32 iy; /**< TODO */
        uint32 nLayers; /**< TODO */
        uint32 nDoodadRefs; /**< TODO */
        uint32 offsMCVT;        /**< height map */
        uint32 offsMCNR;        /**< Normal vectors for each vertex */
        uint32 offsMCLY;        /**< Texture layer definitions */
        uint32 offsMCRF;        /**< A list of indices into the parent file's MDDF chunk */
        uint32 offsMCAL;        /**< Alpha maps for additional texture layers */
        uint32 sizeMCAL; /**< TODO */
        uint32 offsMCSH;        /**< Shadow map for static shadows on the terrain */
        uint32 sizeMCSH; /**< TODO */
        uint32 areaid; /**< TODO */
        uint32 nMapObjRefs; /**< TODO */
        uint16 holes;           /**< locations where models pierce the heightmap */
        uint16 pad; /**< TODO */
        uint16 s[2]; /**< TODO */
        uint32 data1; /**< TODO */
        uint32 data2; /**< TODO */
        uint32 data3; /**< TODO */
        uint32 predTex; /**< TODO */
        uint32 nEffectDoodad; /**< TODO */
        uint32 offsMCSE; /**< TODO */
        uint32 nSndEmitters; /**< TODO */
        uint32 offsMCLQ;         /**< Liqid level (old) */
        uint32 sizeMCLQ;         /**< TODO */
        float  zpos; /**< TODO */
        float  xpos; /**< TODO */
        float  ypos; /**< TODO */
        uint32 offsMCCV;         /**< offsColorValues in WotLK */
        uint32 props; /**< TODO */
        uint32 effectId; /**< TODO */

        /**
         * @brief
         *
         * @return bool
         */
        bool   prepareLoadedData();
        /**
         * @brief
         *
         * @return adt_MCVT
         */
        adt_MCVT* getMCVT()
        {
            if (offsMCVT)
                { return (adt_MCVT*)((uint8*)this + offsMCVT); }
            return 0;
        }
        /**
         * @brief
         *
         * @return adt_MCLQ
         */
        adt_MCLQ* getMCLQ()
        {
            if (offsMCLQ)
                { return (adt_MCLQ*)((uint8*)this + offsMCLQ); }
            return 0;
        }
};

/**
 * @brief Adt file grid chunk
 *
 */
class adt_MCIN
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
    public:
        /**
         * @brief
         *
         */
        struct adt_CELLS
        {
            uint32 offsMCNK; /**< TODO */
            uint32 size; /**< TODO */
            uint32 flags; /**< TODO */
            uint32 asyncId; /**< TODO */
        } cells[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID]; /**< TODO */

        /**
         * @brief
         *
         * @return bool
         */
        bool   prepareLoadedData();
        /**
         * @brief offset from begin file (used this-84)
         *
         * @param x
         * @param y
         * @return adt_MCNK
         */
        adt_MCNK* getMCNK(int x, int y)
        {
            if (cells[x][y].offsMCNK)
                { return (adt_MCNK*)((uint8*)this + cells[x][y].offsMCNK - 84); }
            return 0;
        }
};

#define ADT_LIQUID_HEADER_FULL_LIGHT   0x01
#define ADT_LIQUID_HEADER_NO_HIGHT     0x02

/**
 * @brief
 *
 */
struct adt_liquid_header
{
    uint16 liquidType;             /**< Index from LiquidType.dbc */
    uint16 formatFlags; /**< TODO */
    float  heightLevel1; /**< TODO */
    float  heightLevel2; /**< TODO */
    uint8  xOffset; /**< TODO */
    uint8  yOffset; /**< TODO */
    uint8  width; /**< TODO */
    uint8  height; /**< TODO */
    uint32 offsData2a; /**< TODO */
    uint32 offsData2b; /**< TODO */
};

/**
 * @briefAdt file liquid data chunk (new)
 *
 */
class adt_MH2O
{
    public:
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

        /**
         * @brief
         *
         */
        struct adt_LIQUID
        {
            uint32 offsData1; /**< TODO */
            uint32 used; /**< TODO */
            uint32 offsData2; /**< TODO */
        } liquid[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID]; /**< TODO */

        /**
         * @brief
         *
         * @return bool
         */
        bool   prepareLoadedData();

        /**
         * @brief
         *
         * @param x
         * @param y
         * @return adt_liquid_header
         */
        adt_liquid_header* getLiquidData(int x, int y)
        {
            if (liquid[x][y].used && liquid[x][y].offsData1)
                { return (adt_liquid_header*)((uint8*)this + 8 + liquid[x][y].offsData1); }
            return 0;
        }

        /**
         * @brief
         *
         * @param h
         * @return float
         */
        float* getLiquidHeightMap(adt_liquid_header* h)
        {
            if (h->formatFlags & ADT_LIQUID_HEADER_NO_HIGHT)
                { return 0; }
            if (h->offsData2b)
                { return (float*)((uint8*)this + 8 + h->offsData2b); }
            return 0;
        }

        /**
         * @brief
         *
         * @param h
         * @return uint8
         */
        uint8* getLiquidLightMap(adt_liquid_header* h)
        {
            if (h->formatFlags & ADT_LIQUID_HEADER_FULL_LIGHT)
                { return 0; }
            if (h->offsData2b)
            {
                if (h->formatFlags & ADT_LIQUID_HEADER_NO_HIGHT)
                    { return (uint8*)((uint8*)this + 8 + h->offsData2b); }
                return (uint8*)((uint8*)this + 8 + h->offsData2b + (h->width + 1) * (h->height + 1) * 4);
            }
            return 0;
        }

        /**
         * @brief
         *
         * @param h
         * @return uint32
         */
        uint32* getLiquidFullLightMap(adt_liquid_header* h)
        {
            if (!(h->formatFlags & ADT_LIQUID_HEADER_FULL_LIGHT))
                { return 0; }
            if (h->offsData2b)
            {
                if (h->formatFlags & ADT_LIQUID_HEADER_NO_HIGHT)
                    { return (uint32*)((uint8*)this + 8 + h->offsData2b); }
                return (uint32*)((uint8*)this + 8 + h->offsData2b + (h->width + 1) * (h->height + 1) * 4);
            }
            return 0;
        }

        /**
         * @brief
         *
         * @param h
         * @return uint64
         */
        uint64 getLiquidShowMap(adt_liquid_header* h)
        {
            if (h->offsData2a)
                { return *((uint64*)((uint8*)this + 8 + h->offsData2a)); }
            else
                { return (uint64)0xFFFFFFFFFFFFFFFFLL; }
        }

};

/**
 * @brief Adt file header chunk
 *
 */
class adt_MHDR
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

        uint32 pad; /**< TODO */
        uint32 offsMCIN;           /**< MCIN */
        uint32 offsTex;            /**< MTEX */
        uint32 offsModels;         /**< MMDX */
        uint32 offsModelsIds;      /**< MMID */
        uint32 offsMapObjects;     /**< MWMO */
        uint32 offsMapObjectsIds;  /**< MWID */
        uint32 offsDoodsDef;       /**< MDDF */
        uint32 offsObjectsDef;     /**< MODF */
        uint32 offsMFBO;           /**< MFBO */
        uint32 offsMH2O;           /**< MH2O */
        uint32 data1; /**< TODO */
        uint32 data2; /**< TODO */
        uint32 data3; /**< TODO */
        uint32 data4; /**< TODO */
        uint32 data5; /**< TODO */
    public:
        /**
         * @brief
         *
         * @return bool
         */
        bool prepareLoadedData();
        /**
         * @brief
         *
         * @return adt_MCIN
         */
        adt_MCIN* getMCIN() { return (adt_MCIN*)((uint8*)&pad + offsMCIN);}
        /**
         * @brief
         *
         * @return adt_MH2O
         */
        adt_MH2O* getMH2O() { return offsMH2O ? (adt_MH2O*)((uint8*)&pad + offsMH2O) : 0;}

};

/**
 * @brief
 *
 */
class ADT_file : public FileLoader
{
    public:
        /**
         * @brief
         *
         * @return bool
         */
        bool prepareLoadedData();
        /**
         * @brief
         *
         */
        ADT_file();
        /**
         * @brief
         *
         */
        ~ADT_file();
        /**
         * @brief
         *
         */
        void free();

        adt_MHDR* a_grid; /**< TODO */
};

/**
 * @brief
 *
 * @param holes
 * @param i
 * @param j
 * @return bool
 */
bool isHole(int holes, int i, int j);

#endif
