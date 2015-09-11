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

#include "vmapexport.h"
#include <cassert>
#include <algorithm>
#include <cstdio>
#include <ml/mpq.h>
#include "model.h"
#include "wmo.h"
#include "dbcfile.h"

using namespace std;

Vec3D fixCoordSystem(Vec3D v)
{
    return Vec3D(v.x, v.z, -v.y);
}

Vec3D fixCoordSystem2(Vec3D v)
{
    return Vec3D(v.x, v.z, v.y);
}


Model::Model(std::string& filename) : filename(filename), vertices(0), indices(0)
{
}

bool Model::open(StringSet& failedPaths)
{
    MPQFile f(filename.c_str());

    ok = !f.isEof();

    if (!ok)
    {
        f.close();
        failedPaths.insert(filename);
        return false;
    }

    _unload();

    memcpy(&header, f.getBuffer(), sizeof(ModelHeader));
    if (header.nBoundingTriangles > 0)
    {
        boundingVertices = (ModelBoundingVertex*)(f.getBuffer() + header.ofsBoundingVertices);
        vertices = new Vec3D[header.nBoundingVertices];

        for (size_t i = 0; i < header.nBoundingVertices; i++)
        {
            vertices[i] = fixCoordSystem(boundingVertices[i].pos);
        }

        uint16* triangles = (uint16*)(f.getBuffer() + header.ofsBoundingTriangles);

        nIndices = header.nBoundingTriangles; // refers to the number of int16's, not the number of triangles
        indices = new uint16[nIndices];
        memcpy(indices, triangles, nIndices * 2);

        f.close();
    }
    else
    {
        //printf("not included %s\n", filename.c_str());
        f.close();
        return false;
    }
    return true;
}

bool Model::ConvertToVMAPModel(std::string& outfilename)
{
    int N[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    FILE* output = fopen(outfilename.c_str(), "wb");
    if (!output)
    {
        printf("Can't create the output file '%s'\n", outfilename.c_str());
        return false;
    }
    
    std::fwrite(szRawVMAPMagic, 8, 1, output);
    uint32 nVertices = 0;
    nVertices = header.nBoundingVertices;

    std::fwrite(&nVertices, sizeof(int), 1, output);
    uint32 nofgroups = 1;
    std::fwrite(&nofgroups, sizeof(uint32), 1, output);
    std::fwrite(N, 4 * 3, 1, output); // rootwmoid, flags, groupid
    std::fwrite(N, sizeof(float), 3 * 2, output); //bbox, only needed for WMO currently
    std::fwrite(N, 4, 1, output); // liquidflags
    std::fwrite("GRP ", 4, 1, output);
    uint32 branches = 1;
    int wsize;
    wsize = sizeof(branches) + sizeof(uint32) * branches;
    std::fwrite(&wsize, sizeof(int), 1, output);
    std::fwrite(&branches, sizeof(branches), 1, output);
    uint32 nIndexes = (uint32) nIndices;
    std::fwrite(&nIndexes, sizeof(uint32), 1, output);
    std::fwrite("INDX", 4, 1, output);
    wsize = sizeof(uint32) + sizeof(unsigned short) * nIndexes;
    std::fwrite(&wsize, sizeof(int), 1, output);
    std::fwrite(&nIndexes, sizeof(uint32), 1, output);
    if (nIndexes > 0)
    {
        for (uint32 i = 0; i < nIndices; ++i)
        {
            // index[0] -> x, index[1] -> y, index[2] -> z, index[3] -> x ...
            if ((i % 3) - 1 == 0)
            {
                uint16 tmp = indices[i];
                indices[i] = indices[i+1];
                indices[i+1] = tmp;
            }
        }
        std::fwrite(indices, sizeof(unsigned short), nIndexes, output);
    }
    std::fwrite("VERT", 4, 1, output);
    wsize = sizeof(int) + sizeof(float) * 3 * nVertices;
    std::fwrite(&wsize, sizeof(int), 1, output);
    std::fwrite(&nVertices, sizeof(int), 1, output);
    if (nVertices > 0)
    {
        for (uint32 vpos = 0; vpos < nVertices; ++vpos)
        {
            float tmp = vertices[vpos].y;
            vertices[vpos].y = -vertices[vpos].z;
            vertices[vpos].z = tmp;
        }
        std::fwrite(vertices, sizeof(float) * 3, nVertices, output);
    }

    fclose(output);

    return true;
}



ModelInstance::ModelInstance(MPQFile& f, string& ModelInstName, uint32 mapID, uint32 tileX, uint32 tileY, FILE* pDirfile)
{
    float ff[3];
    f.read(&id, 4);
    f.read(ff, 12);
    pos = fixCoords(Vec3D(ff[0], ff[1], ff[2]));
    f.read(ff, 12);
    rot = Vec3D(ff[0], ff[1], ff[2]);
    //uint16 fFlags;      // dummy var
    //f.read(&scale, 2);
    //f.read(&fFlags, 2); // unknown but flag 1 is used for biodome in Outland, currently this value is not used    
    f.read(&scale,4);  // The above three lines introduced a regression bug in Mangos Zero, is Fine for other cores.

    // scale factor - divide by 1024. blizzard devs must be on crack, why not just use a float?
    sc = scale / 1024.0f;

    char tempname[512];
    sprintf(tempname, "%s/%s", szWorkDirWmo, ModelInstName.c_str());
    FILE* input;
    input = fopen(tempname, "r+b");

    if (!input)
    {
        //printf("ModelInstance::ModelInstance couldn't open %s\n", tempname);
        return;
    }

    fseek(input, 8, SEEK_SET); // get the correct no of vertices
    int nVertices;
    size_t file_read = fread(&nVertices, sizeof(int), 1, input);
    fclose(input);

    if (nVertices == 0 || file_read <= 0)
        { return; }

    uint16 adtId = 0;// not used for models
    uint32 flags = MOD_M2;
    if (tileX == 65 && tileY == 65) { flags |= MOD_WORLDSPAWN; }
    //write mapID, tileX, tileY, Flags, ID, Pos, Rot, Scale, name
    std::fwrite(&mapID, sizeof(uint32), 1, pDirfile);
    std::fwrite(&tileX, sizeof(uint32), 1, pDirfile);
    std::fwrite(&tileY, sizeof(uint32), 1, pDirfile);
    std::fwrite(&flags, sizeof(uint32), 1, pDirfile);
    std::fwrite(&adtId, sizeof(uint16), 1, pDirfile);
    std::fwrite(&id, sizeof(uint32), 1, pDirfile);
    std::fwrite(&pos, sizeof(float), 3, pDirfile);
    std::fwrite(&rot, sizeof(float), 3, pDirfile);
    std::fwrite(&sc, sizeof(float), 1, pDirfile);
    uint32 nlen = ModelInstName.length();
    std::fwrite(&nlen, sizeof(uint32), 1, pDirfile);
    std::fwrite(ModelInstName.c_str(), sizeof(char), nlen, pDirfile);

}

bool ExtractSingleModel(std::string& origPath, std::string& fixedName, StringSet& failedPaths)
{
    string ext = GetExtension(origPath);

    // < 3.1.0 ADT MMDX section store filename.mdx filenames for corresponded .m2 file
    if (ext == "mdx")
    {
        // replace .mdx -> .m2
        origPath.erase(origPath.length() - 2, 2);
        origPath.append("2");
    }
    // >= 3.1.0 ADT MMDX section store filename.m2 filenames for corresponded .m2 file
    // nothing do

    fixedName = GetUniformName(origPath);
    std::string output(szWorkDirWmo);                       // Stores output filename
    output += "/";
    output += fixedName;

    if (FileExists(output.c_str()))
        { return true; }

    Model mdl(origPath);                                    // Possible changed fname
    if (!mdl.open(failedPaths))
        { return false; }

    return mdl.ConvertToVMAPModel(output);
}

void ExtractGameobjectModels()
{
    printf("\n");
    printf("Extracting GameObject models...\n");
    DBCFile dbc("DBFilesClient\\GameObjectDisplayInfo.dbc");
    if (!dbc.open())
    {
        printf("Fatal error: Invalid GameObjectDisplayInfo.dbc file format!\n");
        exit(1);
    }

    std::string basepath = szWorkDirWmo;
    basepath += "/";
    std::string path;
    StringSet failedPaths;

    FILE* model_list = fopen((basepath + "temp_gameobject_models").c_str(), "wb");

    for (DBCFile::Iterator it = dbc.begin(); it != dbc.end(); ++it)
    {
        path = it->getString(1);

        if (path.length() < 4)
            { continue; }

        string name;

        string ch_ext = GetExtension(path);
        if (ch_ext.empty())
            { continue; }

        bool result = false;
        if (ch_ext == "wmo")
        {
            result = ExtractSingleWmo(path);
        }
        else if (ch_ext == "mdl")
        {
            // TODO: extract .mdl files, if needed
            continue;
        }
        else
        {
            result = ExtractSingleModel(path, name, failedPaths);
        }

        if (result)
        {
            uint32 displayId = it->getUInt(0);
            uint32 path_length = name.length();
            std::fwrite(&displayId, sizeof(uint32), 1, model_list);
            std::fwrite(&path_length, sizeof(uint32), 1, model_list);
            std::fwrite(name.c_str(), sizeof(char), path_length, model_list);
        }
    }

    fclose(model_list);

    if (!failedPaths.empty())
    {
        printf("Warning: Some models could not be extracted, see below\n");
        for (StringSet::const_iterator itr = failedPaths.begin(); itr != failedPaths.end(); ++itr)
            { printf("Could not find file of model %s\n", itr->c_str()); }
        printf("A few of these warnings are expected to happen, so be not alarmed!\n");
    }

    printf("Done!\n");
}