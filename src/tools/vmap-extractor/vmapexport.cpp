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

#include <cstdio>
#include <iostream>
#include <vector>
#include <list>
#include <algorithm>
#include <errno.h>

#if defined WIN32
#include <Windows.h>
#include <sys/stat.h>
#include <direct.h>
#define mkdir _mkdir
#else
#include <sys/stat.h>
#endif

#undef min
#undef max

//#pragma warning(disable : 4505)
//#pragma comment(lib, "Winmm.lib")

#include <map>

//From Extractor
#include "adtfile.h"
#include "wdtfile.h"
#include "dbcfile.h"
#include "wmo.h"
#include <ml/mpq.h>

#include "vmapexport.h"
#include "Auth/md5.h"

//------------------------------------------------------------------------------
// Defines

#define MPQ_BLOCK_SIZE 0x1000
//-----------------------------------------------------------------------------

extern ArchiveSet gOpenArchives;

typedef struct
{
    char name[64];
    unsigned int id;
} map_id;

map_id* map_ids;
uint16* LiqType = 0;
uint32 map_count;
char output_path[128] = ".";
char input_path[1024] = ".";
bool hasInputPathParam = false;
bool preciseVectorData = true;

// Constants

//static const char * szWorkDirMaps = ".\\Maps";
const char* szWorkDirWmo = "./Buildings";
const char* szRawVMAPMagic = "VMAPz05";

// Local testing functions

bool FileExists(const char* file)
{
    if (FILE* n = std::fopen(file, "rb"))
    {
        fclose(n);
        return true;
    }
    return false;
}

void compute_md5(const char* value, char* result)
{
    md5_byte_t digest[16];
    md5_state_t ctx;
    
    md5_init(&ctx);
    md5_append(&ctx, (const unsigned char*)value, strlen(value));
    md5_finish(&ctx, digest);
    
    for(int i=0;i<16;i++)
        sprintf(result+2*i,"%02x",digest[i]);
    result[32]='\0';
}

std::string GetUniformName(std::string& path)
{
    std::transform(path.begin(),path.end(),path.begin(),::tolower);
    
    string tempPath;
    string file;
    char digest[33];
    
    std::size_t found = path.find_last_of("/\\");
    if (found != string::npos)
    {
      file = path.substr(found+1);
      tempPath = path.substr(0,found);
    }
    else { file = tempPath = path; }

    if (tempPath == "<empty>")  //GameObjectDisplayInfo, OnyxiasLair
       tempPath.assign("world\\wmo\\dungeon\\kl_onyxiaslair");

    if(!tempPath.empty())
        compute_md5(tempPath.c_str(),digest);
    else
        compute_md5("\\",digest);
    
    string result;
    result = result.assign(digest) + "-" + file;

    return result;
}

std::string GetExtension(std::string& path)
{
    string ext;
    size_t foundExt = path.find_last_of(".");
    if (foundExt != std::string::npos) { ext=path.substr(foundExt+1);}
    else {ext.clear();}
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

// copied from contrib/extractor/System.cpp
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
    size_t LiqType_maxid = dbc.getRecord(LiqType_count - 1).getUInt(0);
    LiqType = new uint16[LiqType_maxid + 1];
    memset(LiqType, 0xff, (LiqType_maxid + 1) * sizeof(uint16));

    for (uint32 x = 0; x < LiqType_count; ++x)
    {
        LiqType[dbc.getRecord(x).getUInt(0)] = dbc.getRecord(x).getUInt(3);
    }

    printf("Success! (%u LiqTypes loaded)\n", (unsigned int)LiqType_count);
}

bool ExtractWmo()
{
    bool success = true;
    
    for (ArchiveSet::const_iterator ar_itr = gOpenArchives.begin(); ar_itr != gOpenArchives.end() && success; ++ar_itr)
    {
        vector<string> filelist;

        (*ar_itr)->GetFileListTo(filelist);
        for (vector<string>::iterator fname = filelist.begin(); fname != filelist.end() && success; ++fname)
        {
            if (fname->find(".wmo") != string::npos) { success = ExtractSingleWmo(*fname);}
        }
    }

    if (success)
        { printf("\nExtract wmo complete (No (fatal) errors)\n"); }

    return success;
}

bool ExtractSingleWmo(std::string& fname)
{
    // Copy files from archive

    char szLocalFile[1024];
    string plain_name = GetUniformName(fname);
        
    sprintf(szLocalFile, "%s/%s", szWorkDirWmo, plain_name.c_str());

    if (FileExists(szLocalFile))
        { return true; }

    int p = 0;
    //Select root wmo files
    const char* rchr = strrchr(plain_name.c_str(), '_');
    if (rchr != NULL)
    {
        char cpy[4];
        strncpy((char*)cpy, rchr, 4);
        for (int i = 0; i < 4; ++i)
        {
            int m = cpy[i];
            if (isdigit(m))
                { p++; }
        }
    }

    if (p == 3)
        { return true; }

    bool file_ok = true;
    std::cout << "Extracting " << fname << std::endl;
    WMORoot froot(fname);
    if (!froot.open())
    {
        printf("Couldn't open RootWmo!!!\n");
        return true;
    }

    FILE* output = fopen(szLocalFile, "wb");
    if (!output)
    {
        printf("couldn't open %s for writing!\n", szLocalFile);
        return false;
    }

    froot.ConvertToVMAPRootWmo(output);
    int Wmo_nVertices = 0;
    if (froot.nGroups != 0)
    {
        for (uint32 i = 0; i < froot.nGroups; ++i)
        {
            char temp[1024];
            strcpy(temp, fname.c_str());
            temp[fname.length() - 4] = 0;
            char groupFileName[1024];
            sprintf(groupFileName, "%s_%03d.wmo", temp, i);

            string s(groupFileName);
            
            WMOGroup fgroup(s);
            if (!fgroup.open())
            {
                printf("Could not open all Group file for: %s\n", plain_name.c_str());
                file_ok = false;
                break;
            }

            Wmo_nVertices += fgroup.ConvertToVMAPGroupWmo(output, &froot, preciseVectorData);
        }
    }

    fseek(output, 8, SEEK_SET); // store the correct no of vertices
    fwrite(&Wmo_nVertices, sizeof(int), 1, output);
    fclose(output);

    // Delete the extracted file in the case of an error
    if (!file_ok)
        { remove(szLocalFile); }
    return true;
}

void ParsMapFiles()
{
    char fn[512];
    //char id_filename[64];
    char id[10];
    StringSet failedPaths;
    for (unsigned int i = 0; i < map_count; ++i)
    {
        sprintf(id, "%03u", map_ids[i].id);
        sprintf(fn, "World\\Maps\\%s\\%s.wdt", map_ids[i].name, map_ids[i].name);
        WDTFile WDT(fn, map_ids[i].name);
        if (WDT.init(id, map_ids[i].id))
        {
            printf("Processing Map %u\n[", map_ids[i].id);
            for (int x = 0; x < 64; ++x)
            {
                for (int y = 0; y < 64; ++y)
                {
                    if (ADTFile* ADT = WDT.GetMap(x, y))
                    {
                        //sprintf(id_filename,"%02u %02u %03u",x,y,map_ids[i].id);//!!!!!!!!!
                        ADT->init(map_ids[i].id, x, y, failedPaths);
                        delete ADT;
                    }
                }
                printf("#");
                fflush(stdout);
            }
            printf("]\n");
        }
    }

    if (!failedPaths.empty())
    {
        printf("Warning: Some models could not be extracted, see below\n");
        for (StringSet::const_iterator itr = failedPaths.begin(); itr != failedPaths.end(); ++itr)
            { printf("Could not find file of model %s\n", itr->c_str()); }
        printf("A few not found models can be expected and are not alarming.\n");
    }
}

void getGamePath()
{
#ifdef _WIN32
    strcpy(input_path, "Data\\");
#else
    strcpy(input_path, "Data/");
#endif
}

bool scan_patches(char* scanmatch, std::vector<std::string>& pArchiveNames)
{
    int i;
    char path[512];

    for (i = 1; i <= 99; i++)
    {
        if (i != 1)
        {
            sprintf(path, "%s-%d.MPQ", scanmatch, i);
        }
        else
        {
            sprintf(path, "%s.MPQ", scanmatch);
        }
#ifdef __linux__
        if (FILE* h = fopen64(path, "rb"))
#else
        if (FILE* h = fopen(path, "rb"))
#endif
        {
            fclose(h);
            //matches.push_back(path);
            pArchiveNames.push_back(path);
        }
    }

    return(true);
}

bool fillArchiveNameVector(std::vector<std::string>& pArchiveNames)
{
    printf("\nGame path: %s\n", input_path);

    char path[512];

    // open expansion and common files
    printf("Opening data files from data directory.\n");
    sprintf(path, "%s/Data/terrain.MPQ", input_path);
    pArchiveNames.push_back(path);
    sprintf(path, "%s/Data/model.MPQ", input_path);
    pArchiveNames.push_back(path);
    sprintf(path, "%s/Data/texture.MPQ", input_path);
    pArchiveNames.push_back(path);
    sprintf(path, "%s/Data/wmo.MPQ", input_path);
    pArchiveNames.push_back(path);
    sprintf(path, "%s/Data/misc.MPQ", input_path);
    pArchiveNames.push_back(path);

    // now, scan for the patch levels in the core dir
    printf("Scanning patch levels from data directory.\n");
    sprintf(path, "%s/Data/patch", input_path);
    if (!scan_patches(path, pArchiveNames))
        { return(false); }

    printf("\n");

    return true;
}

void Usage(char* prg)
{
    printf("Usage: %s [OPTION]\n\n", prg);
    printf("Extract client database fiels and generate map files.\n");
    printf("   -h, --help            show the usage\n");
    printf("   -d, --data <path>     search path for game client archives\n");
    printf("   -s, --small           extract smaller vmaps by optimizing data. Reduces\n");
    printf("                         size by ~ 500MB\n");
    printf("\n");
    printf("Example:\n");
    printf("- use data path and create larger vmaps:\n");
    printf("  %s -l -d \"c:\\games\\world of warcraft\"\n", prg);
}

bool processArgv(int argc, char** argv)
{
    bool result = true;
    bool hasInputPathParam = false;
    bool preciseVectorData = true;
    char* param = NULL;

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 )
        {
            result = false;
            break;
        }
        else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--small") == 0 )
        {
            result = true;
            preciseVectorData = false;
        }
        else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--data") == 0 )
        {
            param = argv[++i];
            if (!param)
            {
                result = false;
                break;
            }

            result = true;
            hasInputPathParam = true;
            strcpy(input_path, param);
            if (input_path[strlen(input_path) - 1] != '\\' || input_path[strlen(input_path) - 1] != '/')
            {
                strcat(input_path, "/");
            }
        }
        else
        {
            result = false;
            break;
        }
    }

    if (!result)
    {
        Usage(argv[0]);
    }
    return result;
}


//xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
// Main
//
// The program must be run with two command line arguments
//
// Arg1 - The source MPQ name (for testing reading and file find)
// Arg2 - Listfile name
//

int main(int argc, char** argv)
{
    printf("mangos-zero vmap (version %s) extractor\n\n", szRawVMAPMagic);

    bool success = true;

    // Use command line arguments, when some
    if (!processArgv(argc, argv))
        { return 1; }

    // some simple check if working dir is dirty
    else
    {
        std::string sdir = std::string(szWorkDirWmo) + "/dir";
        std::string sdir_bin = std::string(szWorkDirWmo) + "/dir_bin";
        struct stat status;
        if (!stat(sdir.c_str(), &status) || !stat(sdir_bin.c_str(), &status))
        {
            printf("Your output directory seems to be polluted, please use an empty directory!\n");
            printf("<press return to exit>");
            char garbage[2];
            scanf("%c", garbage);
            return 1;
        }
    }

    printf("Beginning work ....\n");
    //xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    // Create the working directory
    if (mkdir(szWorkDirWmo
#if defined(__linux__) || defined(__APPLE__) || defined(__FreeBSD__)
              , 0711
#endif
             ))
        { success = (errno == EEXIST); }

    // prepare archive name list
    std::vector<std::string> archiveNames;
    fillArchiveNameVector(archiveNames);
    for (size_t i = 0; i < archiveNames.size(); ++i)
    {
        MPQArchive* archive = new MPQArchive(archiveNames[i].c_str());
        if (!gOpenArchives.size() || gOpenArchives.front() != archive)
            { delete archive; }
    }

    if (gOpenArchives.empty())
    {
        printf("FATAL ERROR: None MPQ archive found by path '%s'. Use -d option with proper path.\n", input_path);
        return 1;
    }
    ReadLiquidTypeTableDBC();

    // extract data
    if (success)
        { success = ExtractWmo(); }

    //xxxxxxxxxxxxxxxxxxxxxxxxxxxxxx
    //map.dbc
    if (success)
    {
        DBCFile* dbc = new DBCFile("DBFilesClient\\Map.dbc");
        if (!dbc->open())
        {
            delete dbc;
            printf("FATAL ERROR: Map.dbc not found in data file.\n");
            return 1;
        }
        map_count = dbc->getRecordCount();
        map_ids = new map_id[map_count];
        for (unsigned int x = 0; x < map_count; ++x)
        {
            map_ids[x].id = dbc->getRecord(x).getUInt(0);
            strcpy(map_ids[x].name, dbc->getRecord(x).getString(1));
            printf("Map - %s\n", map_ids[x].name);
        }


        delete dbc;
        ParsMapFiles();
        delete [] map_ids;
        //nError = ERROR_SUCCESS;
        // Extract models, listed in DameObjectDisplayInfo.dbc
        ExtractGameobjectModels();
    }

    printf("\n");
    if (!success)
    {
        printf("ERROR: Work NOT complete.\n   Precise vector data=%d.\nPress any key.\n", preciseVectorData);
        getchar();
    }

    printf("Work complete. No errors.\n");
    delete [] LiqType;
    return 0;
}
