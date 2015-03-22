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

#include <stdio.h>
#include <deque>
#include <set>
#include <cstdlib>

#ifdef WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#include "dbcfile.h"
#include "mpq_libmpq.h"

#include "loadlib/adt.h"
#include "loadlib/wdt.h"
#include <fcntl.h>

#ifndef WIN32
#include <unistd.h>
/* This isn't the nicest way to do things..
 * TODO: Fix this with snprintf instead and check that it still works
 */
#define sprintf_s sprintf
#endif

#if defined( __GNUC__ )
#define _open   open
#define _close close
#ifndef O_BINARY
#define O_BINARY 0
#endif
#else
#include <io.h>
#endif

#ifdef O_LARGEFILE
#define OPEN_FLAGS  (O_RDONLY | O_BINARY | O_LARGEFILE)
#else
#define OPEN_FLAGS (O_RDONLY | O_BINARY)
#endif
extern ArchiveSet gOpenArchives;    /**< TODO */

/**
 * @brief
 *
 */
typedef struct
{
    char name[64];                  /**< TODO */
    uint32 id;                      /**< TODO */
} map_id;

map_id* map_ids;                    /**< TODO */
uint16* areas;                      /**< TODO */
uint16* LiqType;                    /**< TODO */
char output_path[128] = ".";        /**< TODO */
char input_path[128] = ".";         /**< TODO */
uint32 maxAreaId = 0;               /**< TODO */

/**
 * @brief Data types which can be extracted
 *
 */
enum Extract
{
    EXTRACT_MAP = 1,
    EXTRACT_DBC = 2
};

int   CONF_extract = EXTRACT_MAP | EXTRACT_DBC; /**< Select data for extract */
bool  CONF_allow_height_limit       = true;     /**< Allows to limit minimum height */
float CONF_use_minHeight            = -500.0f;  /**< Default minimum height */

bool  CONF_allow_float_to_int      = false;      /**< Allows float to int conversion */
float CONF_float_to_int8_limit     = 2.0f;      /**< Max accuracy = val/256 */
float CONF_float_to_int16_limit    = 2048.0f;   /**< Max accuracy = val/65536 */
float CONF_flat_height_delta_limit = 0.005f;    /**< If max - min less this value - surface is flat */
float CONF_flat_liquid_delta_limit = 0.001f;    /**< If max - min less this value - liquid surface is flat */

const char* CONF_mpq_list[] = /**< List MPQ for extract from */
{
    "dbc.MPQ",
    "terrain.MPQ",
    "patch.MPQ",
    "patch-2.MPQ",
};

/**
 * @brief
 *
 * @param Path
 */
void CreateDir(const std::string& Path)
{
#ifdef WIN32
    _mkdir(Path.c_str());
#else
    mkdir(Path.c_str(), 0777);
#endif
}

/**
 * @brief
 *
 * @param FileName
 * @return bool
 */
bool FileExists(const char* FileName)
{
    int fp = _open(FileName, OPEN_FLAGS);
    if (fp != -1)
    {
        _close(fp);
        return true;
    }

    return false;
}

/**
 * @brief
 *
 * @param prg
 */
void Usage(char* prg)
{
    printf("Usage: %s [OPTION]\n\n", prg);
    printf("Extract client database files and generate map files.\n");
    printf("   -h, --help            show the usage\n");
    printf("   -i, --input <path>    search path for game client archives\n");
    printf("   -o, --output <path>   target path for generated files\n");
    printf("   -f, --flat #          store height information as integers reducing map\n");
    printf("                         size, but also accuracy\n");
    printf("   -e, --extract #       extract specified client data. 1 = maps, 2 = DBCs,\n");
    printf("                         3 = both. Defaults to extracting both.\n");
    printf("\n");
    printf("Example:\n");
    printf("- use input path and do not flatten maps:\n");
    printf("  %s -f 0 -i \"c:\\games\\world of warcraft\"\n", prg);
    exit(1);
}

/**
 * @brief
 *
 * @param argc
 * @param argv
 * @return bool
 */
bool HandleArgs(int argc, char** argv)
{
    char* param = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--input") == 0)
        {
            param = argv[++i];
            if (!param)
            {
                return false;
            }

            strcpy(input_path, param);
        }
        else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0)
        {
            param = argv[++i];
            if (!param)
            {
                return false;
            }

            strcpy(output_path, param);
        }
        else if (strcmp(argv[i], "-f") == 0 || strcmp(argv[i], "--flat") == 0)
        {
            param = argv[++i];
            if (!param)
            {
                return false;
            }

            int convertFloatToInt = atoi(param);
            if (convertFloatToInt != 0)
            {
                CONF_allow_float_to_int = true;
            }
        }
        else if (strcmp(argv[i], "-e") == 0 || strcmp(argv[i], "--extract") == 0)
        {
            param = argv[++i];
            if (!param)
            {
                return false;
            }

            int convertExtract = atoi(param);
            if (convertExtract > 0 && convertExtract < 4)
            {
                CONF_extract = convertExtract;
            }
            else
            {
                Usage(argv[0]);
            }
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            Usage(argv[0]);
        }
    }

    return true;

}

/**
 * @brief
 *
 * @return uint32
 */
uint32 ReadMapDBC()
{
    printf("Reading maps from Map.dbc... ");
    DBCFile dbc("DBFilesClient\\Map.dbc");

    if (!dbc.open())
    {
        printf("Fatal error: Could not read Map.dbc!\n");
        exit(1);
    }

    size_t map_count = dbc.getRecordCount();
    map_ids = new map_id[map_count];
    for (uint32 x = 0; x < map_count; ++x)
    {
        map_ids[x].id = dbc.getRecord(x).getUInt(0);
        strcpy(map_ids[x].name, dbc.getRecord(x).getString(1));
    }
    printf("Success! %lu maps loaded.\n", map_count);
    return map_count;
}

/**
 * @brief
 *
 */
void ReadAreaTableDBC()
{
    printf("Read areas from AreaTable.dbc ...");
    DBCFile dbc("DBFilesClient\\AreaTable.dbc");

    if (!dbc.open())
    {
        printf("Fatal error: Could not read AreaTable.dbc!\n");
        exit(1);
    }

    size_t area_count = dbc.getRecordCount();
    size_t maxid = dbc.getMaxId();
    areas = new uint16[maxid + 1];
    memset(areas, 0xff, (maxid + 1) * sizeof(uint16));

    for (uint32 x = 0; x < area_count; ++x)
        { areas[dbc.getRecord(x).getUInt(0)] = dbc.getRecord(x).getUInt(3); }

    maxAreaId = dbc.getMaxId();

    printf("Success! %lu areas loaded.\n", area_count);
}

/**
 * @brief
 *
 */
void ReadLiquidTypeTableDBC()
{
    printf("Reading liquid types from LiquidType.dbc...");
    DBCFile dbc("DBFilesClient\\LiquidType.dbc");
    if (!dbc.open())
    {
        printf("Fatal error: Could not read LiquidType.dbc!\n");
        exit(1);
    }

    size_t LiqType_count = dbc.getRecordCount();
    size_t LiqType_maxid = dbc.getMaxId();
    LiqType = new uint16[LiqType_maxid + 1];
    memset(LiqType, 0xff, (LiqType_maxid + 1) * sizeof(uint16));

    for (uint32 x = 0; x < LiqType_count; ++x)
        { LiqType[dbc.getRecord(x).getUInt(0)] = dbc.getRecord(x).getUInt(3); }

    printf("Success! %lu liquid types loaded.\n", LiqType_count);
}

//
// Adt file convertor function and data
//

// Map file format data
static char const* MAP_MAGIC         = "MAPS"; /**< TODO */
static char const* MAP_VERSION_MAGIC = "z1.3"; /**< TODO */
static char const* MAP_AREA_MAGIC    = "AREA"; /**< TODO */
static char const* MAP_HEIGHT_MAGIC  = "MHGT"; /**< TODO */
static char const* MAP_LIQUID_MAGIC  = "MLIQ"; /**< TODO */

/**
 * @brief
 *
 */
struct map_fileheader
{
    uint32 mapMagic;        /**< TODO */
    uint32 versionMagic;    /**< TODO */
    uint32 areaMapOffset;   /**< TODO */
    uint32 areaMapSize;     /**< TODO */
    uint32 heightMapOffset; /**< TODO */
    uint32 heightMapSize;   /**< TODO */
    uint32 liquidMapOffset; /**< TODO */
    uint32 liquidMapSize;   /**< TODO */
    uint32 holesOffset;     /**< TODO */
    uint32 holesSize;       /**< TODO */
};

#define MAP_AREA_NO_AREA      0x0001

/**
 * @brief
 *
 */
struct map_areaHeader
{
    uint32 fourcc;          /**< TODO */
    uint16 flags;           /**< TODO */
    uint16 gridArea;        /**< TODO */
};

#define MAP_HEIGHT_NO_HEIGHT  0x0001
#define MAP_HEIGHT_AS_INT16   0x0002
#define MAP_HEIGHT_AS_INT8    0x0004

/**
 * @brief
 *
 */
struct map_heightHeader
{
    uint32 fourcc;          /**< TODO */
    uint32 flags;           /**< TODO */
    float  gridHeight;      /**< TODO */
    float  gridMaxHeight;   /**< TODO */
};

#define MAP_LIQUID_TYPE_NO_WATER    0x00
#define MAP_LIQUID_TYPE_MAGMA       0x01
#define MAP_LIQUID_TYPE_OCEAN       0x02
#define MAP_LIQUID_TYPE_SLIME       0x04
#define MAP_LIQUID_TYPE_WATER       0x08

#define MAP_LIQUID_TYPE_DARK_WATER  0x10
#define MAP_LIQUID_TYPE_WMO_WATER   0x20


#define MAP_LIQUID_NO_TYPE    0x0001
#define MAP_LIQUID_NO_HEIGHT  0x0002

/**
 * @brief
 *
 */
struct map_liquidHeader
{
    uint32 fourcc;          /**< TODO */
    uint16 flags;           /**< TODO */
    uint16 liquidType;      /**< TODO */
    uint8  offsetX;         /**< TODO */
    uint8  offsetY;         /**< TODO */
    uint8  width;           /**< TODO */
    uint8  height;          /**< TODO */
    float  liquidLevel;     /**< TODO */
};

/**
 * @brief
 *
 * @param maxDiff
 * @return float
 */
float selectUInt8StepStore(float maxDiff)
{
    return 255 / maxDiff;
}

/**
 * @brief
 *
 * @param maxDiff
 * @return float
 */
float selectUInt16StepStore(float maxDiff)
{
    return 65535 / maxDiff;
}

uint16 area_flags[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];      /**< Temporary grid data store */

float V8[ADT_GRID_SIZE][ADT_GRID_SIZE];                         /**< TODO */
float V9[ADT_GRID_SIZE + 1][ADT_GRID_SIZE + 1];                 /**< TODO */
uint16 uint16_V8[ADT_GRID_SIZE][ADT_GRID_SIZE];                 /**< TODO */
uint16 uint16_V9[ADT_GRID_SIZE + 1][ADT_GRID_SIZE + 1];         /**< TODO */
uint8  uint8_V8[ADT_GRID_SIZE][ADT_GRID_SIZE];                  /**< TODO */
uint8  uint8_V9[ADT_GRID_SIZE + 1][ADT_GRID_SIZE + 1];          /**< TODO */

uint16 liquid_entry[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];    /**< TODO */
uint8 liquid_flags[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];     /**< TODO */
bool  liquid_show[ADT_GRID_SIZE][ADT_GRID_SIZE];                /**< TODO */
float liquid_height[ADT_GRID_SIZE + 1][ADT_GRID_SIZE + 1];      /**< TODO */

/**
 * @brief
 *
 * @param filename
 * @param filename2
 * @param cell_y
 * @param cell_x
 * @return bool
 */
bool ConvertADT(char* filename, char* filename2)
{
    ADT_file adt;

    if (!adt.loadFile(filename))
        { return false; }

    adt_MCIN* cells = adt.a_grid->getMCIN();
    if (!cells)
    {
        printf("Can not find cells in '%s'\n", filename);
        return false;
    }

    memset(liquid_show, 0, sizeof(liquid_show));
    memset(liquid_flags, 0, sizeof(liquid_flags));
    memset(liquid_entry, 0, sizeof(liquid_entry));

    // Prepare map header
    map_fileheader map;
    map.mapMagic = *(uint32 const*)MAP_MAGIC;
    map.versionMagic = *(uint32 const*)MAP_VERSION_MAGIC;

    // Get area flags data
    for (int i = 0; i < ADT_CELLS_PER_GRID; i++)
    {
        for (int j = 0; j < ADT_CELLS_PER_GRID; j++)
        {
            adt_MCNK* cell = cells->getMCNK(i, j);
            uint32 areaid = cell->areaid;
            if (areaid && areaid <= maxAreaId)
            {
                if (areas[areaid] != 0xffff)
                {
                    area_flags[i][j] = areas[areaid];
                    continue;
                }
                printf("File: %s\nCan not find area flag for area %u [%d, %d].\n", filename, areaid, cell->ix, cell->iy);
            }
            area_flags[i][j] = 0xffff;
        }
    }
    //============================================
    // Try pack area data
    //============================================
    bool fullAreaData = false;
    uint32 areaflag = area_flags[0][0];
    for (int y = 0; y < ADT_CELLS_PER_GRID; y++)
    {
        for (int x = 0; x < ADT_CELLS_PER_GRID; x++)
        {
            if (area_flags[y][x] != areaflag)
            {
                fullAreaData = true;
                break;
            }
        }
    }

    map.areaMapOffset = sizeof(map);
    map.areaMapSize   = sizeof(map_areaHeader);

    map_areaHeader areaHeader;
    areaHeader.fourcc = *(uint32 const*)MAP_AREA_MAGIC;
    areaHeader.flags = 0;
    if (fullAreaData)
    {
        areaHeader.gridArea = 0;
        map.areaMapSize += sizeof(area_flags);
    }
    else
    {
        areaHeader.flags |= MAP_AREA_NO_AREA;
        areaHeader.gridArea = (uint16)areaflag;
    }

    //
    // Get Height map from grid
    //
    for (int i = 0; i < ADT_CELLS_PER_GRID; i++)
    {
        for (int j = 0; j < ADT_CELLS_PER_GRID; j++)
        {
            adt_MCNK* cell = cells->getMCNK(i, j);
            if (!cell)
                { continue; }
            // Height values for triangles stored in order:
            // 1     2     3     4     5     6     7     8     9
            //    10    11    12    13    14    15    16    17
            // 18    19    20    21    22    23    24    25    26
            //    27    28    29    30    31    32    33    34
            // . . . . . . . .
            // For better get height values merge it to V9 and V8 map
            // V9 height map:
            // 1     2     3     4     5     6     7     8     9
            // 18    19    20    21    22    23    24    25    26
            // . . . . . . . .
            // V8 height map:
            //    10    11    12    13    14    15    16    17
            //    27    28    29    30    31    32    33    34
            // . . . . . . . .

            // Set map height as grid height
            for (int y = 0; y <= ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x <= ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    V9[cy][cx] = cell->ypos;
                }
            }
            for (int y = 0; y < ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x < ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    V8[cy][cx] = cell->ypos;
                }
            }
            // Get custom height
            adt_MCVT* v = cell->getMCVT();
            if (!v)
                { continue; }
            // get V9 height map
            for (int y = 0; y <= ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x <= ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    V9[cy][cx] += v->height_map[y * (ADT_CELL_SIZE * 2 + 1) + x];
                }
            }
            // get V8 height map
            for (int y = 0; y < ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x < ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    V8[cy][cx] += v->height_map[y * (ADT_CELL_SIZE * 2 + 1) + ADT_CELL_SIZE + 1 + x];
                }
            }
        }
    }
    //============================================
    // Try pack height data
    //============================================
    float maxHeight = -20000;
    float minHeight =  20000;
    for (int y = 0; y < ADT_GRID_SIZE; y++)
    {
        for (int x = 0; x < ADT_GRID_SIZE; x++)
        {
            float h = V8[y][x];
            if (maxHeight < h) { maxHeight = h; }
            if (minHeight > h) { minHeight = h; }
        }
    }
    for (int y = 0; y <= ADT_GRID_SIZE; y++)
    {
        for (int x = 0; x <= ADT_GRID_SIZE; x++)
        {
            float h = V9[y][x];
            if (maxHeight < h) { maxHeight = h; }
            if (minHeight > h) { minHeight = h; }
        }
    }

    // Check for allow limit minimum height (not store height in deep ochean - allow save some memory)
    if (CONF_allow_height_limit && minHeight < CONF_use_minHeight)
    {
        for (int y = 0; y < ADT_GRID_SIZE; y++)
            for (int x = 0; x < ADT_GRID_SIZE; x++)
                if (V8[y][x] < CONF_use_minHeight)
                    { V8[y][x] = CONF_use_minHeight; }
        for (int y = 0; y <= ADT_GRID_SIZE; y++)
            for (int x = 0; x <= ADT_GRID_SIZE; x++)
                if (V9[y][x] < CONF_use_minHeight)
                    { V9[y][x] = CONF_use_minHeight; }
        if (minHeight < CONF_use_minHeight)
            { minHeight = CONF_use_minHeight; }
        if (maxHeight < CONF_use_minHeight)
            { maxHeight = CONF_use_minHeight; }
    }

    map.heightMapOffset = map.areaMapOffset + map.areaMapSize;
    map.heightMapSize = sizeof(map_heightHeader);

    map_heightHeader heightHeader;
    heightHeader.fourcc = *(uint32 const*)MAP_HEIGHT_MAGIC;
    heightHeader.flags = 0;
    heightHeader.gridHeight    = minHeight;
    heightHeader.gridMaxHeight = maxHeight;

    if (maxHeight == minHeight)
        { heightHeader.flags |= MAP_HEIGHT_NO_HEIGHT; }

    // Not need store if flat surface
    if (CONF_allow_float_to_int && (maxHeight - minHeight) < CONF_flat_height_delta_limit)
        { heightHeader.flags |= MAP_HEIGHT_NO_HEIGHT; }

    // Try store as packed in uint16 or uint8 values
    if (!(heightHeader.flags & MAP_HEIGHT_NO_HEIGHT))
    {
        float step = 0.0f;
        // Try Store as uint values
        if (CONF_allow_float_to_int)
        {
            float diff = maxHeight - minHeight;
            if (diff < CONF_float_to_int8_limit)      // As uint8 (max accuracy = CONF_float_to_int8_limit/256)
            {
                heightHeader.flags |= MAP_HEIGHT_AS_INT8;
                step = selectUInt8StepStore(diff);
            }
            else if (diff < CONF_float_to_int16_limit) // As uint16 (max accuracy = CONF_float_to_int16_limit/65536)
            {
                heightHeader.flags |= MAP_HEIGHT_AS_INT16;
                step = selectUInt16StepStore(diff);
            }
        }

        // Pack it to int values if need
        if (heightHeader.flags & MAP_HEIGHT_AS_INT8)
        {
            for (int y = 0; y < ADT_GRID_SIZE; y++)
                for (int x = 0; x < ADT_GRID_SIZE; x++)
                    { uint8_V8[y][x] = uint8((V8[y][x] - minHeight) * step + 0.5f); }
            for (int y = 0; y <= ADT_GRID_SIZE; y++)
                for (int x = 0; x <= ADT_GRID_SIZE; x++)
                    { uint8_V9[y][x] = uint8((V9[y][x] - minHeight) * step + 0.5f); }
            map.heightMapSize += sizeof(uint8_V9) + sizeof(uint8_V8);
        }
        else if (heightHeader.flags & MAP_HEIGHT_AS_INT16)
        {
            for (int y = 0; y < ADT_GRID_SIZE; y++)
                for (int x = 0; x < ADT_GRID_SIZE; x++)
                    { uint16_V8[y][x] = uint16((V8[y][x] - minHeight) * step + 0.5f); }
            for (int y = 0; y <= ADT_GRID_SIZE; y++)
                for (int x = 0; x <= ADT_GRID_SIZE; x++)
                    { uint16_V9[y][x] = uint16((V9[y][x] - minHeight) * step + 0.5f); }
            map.heightMapSize += sizeof(uint16_V9) + sizeof(uint16_V8);
        }
        else
            { map.heightMapSize += sizeof(V9) + sizeof(V8); }
    }

    // Get from MCLQ chunk (old)
    for (int i = 0; i < ADT_CELLS_PER_GRID; i++)
    {
        for (int j = 0; j < ADT_CELLS_PER_GRID; j++)
        {
            adt_MCNK* cell = cells->getMCNK(i, j);
            if (!cell)
                { continue; }

            adt_MCLQ* liquid = cell->getMCLQ();
            int count = 0;
            if (!liquid || cell->sizeMCLQ <= 8)
                { continue; }

            for (int y = 0; y < ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x < ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    if (liquid->flags[y][x] != 0x0F)
                    {
                        liquid_show[cy][cx] = true;
                        if (liquid->flags[y][x] & (1 << 7))
                            { liquid_flags[i][j] |= MAP_LIQUID_TYPE_DARK_WATER; }
                        ++count;
                    }
                }
            }

            uint32 c_flag = cell->flags;
            if (c_flag & (1 << 2))
            {
                liquid_entry[i][j] = 1;
                liquid_flags[i][j] |= MAP_LIQUID_TYPE_WATER;            // water
            }
            if (c_flag & (1 << 3))
            {
                liquid_entry[i][j] = 2;
                liquid_flags[i][j] |= MAP_LIQUID_TYPE_OCEAN;            // ocean
            }
            if (c_flag & (1 << 4))
            {
                liquid_entry[i][j] = 3;
                liquid_flags[i][j] |= MAP_LIQUID_TYPE_MAGMA;            // magma/slime
            }

            if (!count && liquid_flags[i][j])
                { fprintf(stderr, "Wrong liquid type detected in MCLQ chunk"); }

            for (int y = 0; y <= ADT_CELL_SIZE; y++)
            {
                int cy = i * ADT_CELL_SIZE + y;
                for (int x = 0; x <= ADT_CELL_SIZE; x++)
                {
                    int cx = j * ADT_CELL_SIZE + x;
                    liquid_height[cy][cx] = liquid->liquid[y][x].height;
                }
            }
        }
    }

    // Get liquid map for grid (in WOTLK used MH2O chunk)
    adt_MH2O* h2o = adt.a_grid->getMH2O();
    if (h2o)
    {
        for (int i = 0; i < ADT_CELLS_PER_GRID; i++)
        {
            for (int j = 0; j < ADT_CELLS_PER_GRID; j++)
            {
                adt_liquid_header* h = h2o->getLiquidData(i, j);
                if (!h)
                    { continue; }

                int count = 0;
                uint64 show = h2o->getLiquidShowMap(h);
                for (int y = 0; y < h->height; y++)
                {
                    int cy = i * ADT_CELL_SIZE + y + h->yOffset;
                    for (int x = 0; x < h->width; x++)
                    {
                        int cx = j * ADT_CELL_SIZE + x + h->xOffset;
                        if (show & 1)
                        {
                            liquid_show[cy][cx] = true;
                            ++count;
                        }
                        show >>= 1;
                    }
                }

                liquid_entry[i][j] = h->liquidType;
                switch (LiqType[h->liquidType])
                {
                    case LIQUID_TYPE_WATER: liquid_flags[i][j] |= MAP_LIQUID_TYPE_WATER; break;
                    case LIQUID_TYPE_OCEAN: liquid_flags[i][j] |= MAP_LIQUID_TYPE_OCEAN; break;
                    case LIQUID_TYPE_MAGMA: liquid_flags[i][j] |= MAP_LIQUID_TYPE_MAGMA; break;
                    case LIQUID_TYPE_SLIME: liquid_flags[i][j] |= MAP_LIQUID_TYPE_SLIME; break;
                    default:
                        printf("\nCan not find liquid type %u for map %s\nchunk %d,%d\n", h->liquidType, filename, i, j);
                        break;
                }
                // Dark water detect
                if (LiqType[h->liquidType] == LIQUID_TYPE_OCEAN)
                {
                    uint8* lm = h2o->getLiquidLightMap(h);
                    if (!lm)
                        { liquid_flags[i][j] |= MAP_LIQUID_TYPE_DARK_WATER; }
                }

                if (!count && liquid_flags[i][j])
                    { printf("Wrong liquid type detected in MH2O chunk"); }

                float* height = h2o->getLiquidHeightMap(h);
                int pos = 0;
                for (int y = 0; y <= h->height; y++)
                {
                    int cy = i * ADT_CELL_SIZE + y + h->yOffset;
                    for (int x = 0; x <= h->width; x++)
                    {
                        int cx = j * ADT_CELL_SIZE + x + h->xOffset;
                        if (height)
                            { liquid_height[cy][cx] = height[pos]; }
                        else
                            { liquid_height[cy][cx] = h->heightLevel1; }
                        pos++;
                    }
                }
            }
        }
    }
    //============================================
    // Pack liquid data
    //============================================
    uint8 type = liquid_flags[0][0];
    bool fullType = false;
    for (int y = 0; y < ADT_CELLS_PER_GRID; y++)
    {
        for (int x = 0; x < ADT_CELLS_PER_GRID; x++)
        {
            if (liquid_flags[y][x] != type)
            {
                fullType = true;
                y = ADT_CELLS_PER_GRID;
                break;
            }
        }
    }

    map_liquidHeader liquidHeader;

    // no water data (if all grid have 0 liquid type)
    if (type == 0 && !fullType)
    {
        // No liquid data
        map.liquidMapOffset = 0;
        map.liquidMapSize   = 0;
    }
    else
    {
        int minX = 255, minY = 255;
        int maxX = 0, maxY = 0;
        maxHeight = -20000;
        minHeight = 20000;
        for (int y = 0; y < ADT_GRID_SIZE; y++)
        {
            for (int x = 0; x < ADT_GRID_SIZE; x++)
            {
                if (liquid_show[y][x])
                {
                    if (minX > x) { minX = x; }
                    if (maxX < x) { maxX = x; }
                    if (minY > y) { minY = y; }
                    if (maxY < y) { maxY = y; }
                    float h = liquid_height[y][x];
                    if (maxHeight < h) { maxHeight = h; }
                    if (minHeight > h) { minHeight = h; }
                }
                else
                    { liquid_height[y][x] = CONF_use_minHeight; }
            }
        }
        map.liquidMapOffset = map.heightMapOffset + map.heightMapSize;
        map.liquidMapSize = sizeof(map_liquidHeader);
        liquidHeader.fourcc = *(uint32 const*)MAP_LIQUID_MAGIC;
        liquidHeader.flags = 0;
        liquidHeader.liquidType = 0;
        liquidHeader.offsetX = minX;
        liquidHeader.offsetY = minY;
        liquidHeader.width   = maxX - minX + 1 + 1;
        liquidHeader.height  = maxY - minY + 1 + 1;
        liquidHeader.liquidLevel = minHeight;

        if (maxHeight == minHeight)
            { liquidHeader.flags |= MAP_LIQUID_NO_HEIGHT; }

        // Not need store if flat surface
        if (CONF_allow_float_to_int && (maxHeight - minHeight) < CONF_flat_liquid_delta_limit)
            { liquidHeader.flags |= MAP_LIQUID_NO_HEIGHT; }

        if (!fullType)
            { liquidHeader.flags |= MAP_LIQUID_NO_TYPE; }

        if (liquidHeader.flags & MAP_LIQUID_NO_TYPE)
            { liquidHeader.liquidType = type; }
        else
            { map.liquidMapSize += sizeof(liquid_entry) + sizeof(liquid_flags); }

        if (!(liquidHeader.flags & MAP_LIQUID_NO_HEIGHT))
            { map.liquidMapSize += sizeof(float) * liquidHeader.width * liquidHeader.height; }
    }

    // map hole info
    uint16 holes[ADT_CELLS_PER_GRID][ADT_CELLS_PER_GRID];

    if (map.liquidMapOffset)
        { map.holesOffset = map.liquidMapOffset + map.liquidMapSize; }
    else
        { map.holesOffset = map.heightMapOffset + map.heightMapSize; }

    map.holesSize = sizeof(holes);
    memset(holes, 0, map.holesSize);

    for (int i = 0; i < ADT_CELLS_PER_GRID; ++i)
    {
        for (int j = 0; j < ADT_CELLS_PER_GRID; ++j)
        {
            adt_MCNK* cell = cells->getMCNK(i, j);
            if (!cell)
                { continue; }
            holes[i][j] = cell->holes;
        }
    }

    // Ok all data prepared - store it
    FILE* output = fopen(filename2, "wb");
    if (!output)
    {
        printf("Can not create the output file '%s'\n", filename2);
        return false;
    }
    fwrite(&map, sizeof(map), 1, output);
    // Store area data
    fwrite(&areaHeader, sizeof(areaHeader), 1, output);
    if (!(areaHeader.flags & MAP_AREA_NO_AREA))
        { fwrite(area_flags, sizeof(area_flags), 1, output); }

    // Store height data
    fwrite(&heightHeader, sizeof(heightHeader), 1, output);
    if (!(heightHeader.flags & MAP_HEIGHT_NO_HEIGHT))
    {
        if (heightHeader.flags & MAP_HEIGHT_AS_INT16)
        {
            fwrite(uint16_V9, sizeof(uint16_V9), 1, output);
            fwrite(uint16_V8, sizeof(uint16_V8), 1, output);
        }
        else if (heightHeader.flags & MAP_HEIGHT_AS_INT8)
        {
            fwrite(uint8_V9, sizeof(uint8_V9), 1, output);
            fwrite(uint8_V8, sizeof(uint8_V8), 1, output);
        }
        else
        {
            fwrite(V9, sizeof(V9), 1, output);
            fwrite(V8, sizeof(V8), 1, output);
        }
    }

    // Store liquid data if need
    if (map.liquidMapOffset)
    {
        fwrite(&liquidHeader, sizeof(liquidHeader), 1, output);
        if (!(liquidHeader.flags & MAP_LIQUID_NO_TYPE))
        {
            fwrite(liquid_entry, sizeof(liquid_entry), 1, output);
            fwrite(liquid_flags, sizeof(liquid_flags), 1, output);
        }
        if (!(liquidHeader.flags & MAP_LIQUID_NO_HEIGHT))
        {
            for (int y = 0; y < liquidHeader.height; y++)
                { fwrite(&liquid_height[y + liquidHeader.offsetY][liquidHeader.offsetX], sizeof(float), liquidHeader.width, output); }
        }
    }

    // store hole data
    fwrite(holes, map.holesSize, 1, output);

    fclose(output);

    return true;
}

/**
 * @brief
 *
 */
void ExtractMapsFromMpq()
{
    char mpq_filename[1024];
    char output_filename[1024];
    char mpq_map_name[1024];

    printf("Extracting maps...\n");

    uint32 map_count = ReadMapDBC();

    ReadAreaTableDBC();
    ReadLiquidTypeTableDBC();

    std::string path = output_path;
    path += "/maps/";
    CreateDir(path);

    printf("Converting map files\n");
    for (uint32 z = 0; z < map_count; ++z)
    {
        printf("Extract %s (%d/%d)                  \n", map_ids[z].name, z + 1, map_count);
        // Loadup map grid data
        sprintf(mpq_map_name, "World\\Maps\\%s\\%s.wdt", map_ids[z].name, map_ids[z].name);
        WDT_file wdt;
        if (!wdt.loadFile(mpq_map_name, false))
        {
            // printf("Error loading %s map WDT data\n", map_ids[z].name);
            continue;
        }

        for (uint32 y = 0; y < WDT_MAP_SIZE; ++y)
        {
            for (uint32 x = 0; x < WDT_MAP_SIZE; ++x)
            {
                if (!wdt.main->adt_list[y][x].exist)
                    { continue; }
                sprintf(mpq_filename, "World\\Maps\\%s\\%s_%u_%u.adt", map_ids[z].name, map_ids[z].name, x, y);
                sprintf(output_filename, "%s/maps/%03u%02u%02u.map", output_path, map_ids[z].id, y, x);
                ConvertADT(mpq_filename, output_filename);// , y, x);
            }
            // draw progress bar
            printf("Processing........................%d%%\r", (100 * (y + 1)) / WDT_MAP_SIZE);
        }
    }
    delete [] areas;
    delete [] map_ids;
}

/**
 * @brief
 *
 * @param mpq_name
 * @param filename
 * @return bool
 */
bool ExtractFile(char const* mpq_name, std::string const& filename)
{
    FILE* output = fopen(filename.c_str(), "wb");
    if (!output)
    {
        printf("Can not create the output file '%s'\n", filename.c_str());
        return false;
    }
    MPQFile m(mpq_name);
    if (!m.isEof())
        { fwrite(m.getPointer(), 1, m.getSize(), output); }

    fclose(output);
    return true;
}

/**
 * @brief
 *
 */
void ExtractDBCFiles()
{
    printf("Extracting client database files...\n");

    std::set<std::string> dbcfiles;

    // get DBC file list
    for (ArchiveSet::iterator i = gOpenArchives.begin(); i != gOpenArchives.end(); ++i)
    {
        vector<string> files;
        (*i)->GetFileListTo(files);
        for (vector<string>::iterator iter = files.begin(); iter != files.end(); ++iter)
            if (iter->rfind(".dbc") == iter->length() - strlen(".dbc"))
                { dbcfiles.insert(*iter); }
    }

    std::string path = output_path;
    path += "/dbc/";
    CreateDir(path);

    // extract DBCs
    int count = 0;
    for (set<string>::iterator iter = dbcfiles.begin(); iter != dbcfiles.end(); ++iter)
    {
        string filename = path;
        filename += (iter->c_str() + strlen("DBFilesClient\\"));

        if (ExtractFile(iter->c_str(), filename))
            { ++count; }
    }
    printf("Extracted %u client database files\n\n", count);
}

/**
 * @brief
 *
 */
void LoadCommonMPQFiles()
{
    char filename[512];
    int count = sizeof(CONF_mpq_list) / sizeof(char*);
    for (int i = 0; i < count; ++i)
    {
        sprintf_s(filename, "%s/Data/%s", input_path, CONF_mpq_list[i]);
        if (FileExists(filename))
            { new MPQArchive(filename); }
    }
}

/**
 * @brief
 *
 */
inline void CloseMPQFiles()
{
    for (ArchiveSet::iterator j = gOpenArchives.begin(); j != gOpenArchives.end(); ++j) { (*j)->close(); }
    gOpenArchives.clear();
}

/**
 * @brief
 *
 * @param argc
 * @param argv
 * @return int
 */
int main(int argc, char** argv)
{
    printf("mangos-zero DBC & map (version %s) extractor\n\n", MAP_VERSION_MAGIC);

    if (!HandleArgs(argc, argv))
    {
        return 1;
    }

    // Open MPQs
    LoadCommonMPQFiles();

    // Extract dbc
    if (CONF_extract & EXTRACT_DBC)
        { ExtractDBCFiles(); }

    // Extract maps
    if (CONF_extract & EXTRACT_MAP)
        { ExtractMapsFromMpq(); }

    // Close MPQs
    CloseMPQFiles();

    return 0;
}
