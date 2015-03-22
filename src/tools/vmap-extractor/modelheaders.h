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

#ifndef MODELHEADERS_H
#define MODELHEADERS_H

/* typedef unsigned char uint8;
typedef char int8;
typedef unsigned short uint16;
typedef short int16;
typedef unsigned int uint32;
typedef int int32; */

#pragma pack(push,1)

/**
 * @brief
 *
 */
struct ModelHeader
{
    char id[4]; /**< TODO */
    uint8 version[4]; /**< TODO */
    uint32 nameLength; /**< TODO */
    uint32 nameOfs; /**< TODO */
    uint32 type; /**< TODO */
    uint32 nGlobalSequences; /**< TODO */
    uint32 ofsGlobalSequences; /**< TODO */
    uint32 nAnimations; /**< TODO */
    uint32 ofsAnimations; /**< TODO */
    uint32 nAnimationLookup; /**< TODO */
    uint32 ofsAnimationLookup; /**< TODO */
    uint32 nD; /**< TODO */
    uint32 ofsD; /**< TODO */
    uint32 nBones; /**< TODO */
    uint32 ofsBones; /**< TODO */
    uint32 nKeyBoneLookup; /**< TODO */
    uint32 ofsKeyBoneLookup; /**< TODO */
    uint32 nVertices; /**< TODO */
    uint32 ofsVertices; /**< TODO */
    uint32 nViews; /**< TODO */
    uint32 ofsViews; /**< TODO */
    uint32 nColors; /**< TODO */
    uint32 ofsColors; /**< TODO */
    uint32 nTextures; /**< TODO */
    uint32 ofsTextures; /**< TODO */
    uint32 nTransparency; /**< TODO */
    uint32 ofsTransparency; /**< TODO */
    uint32 nI; /**< TODO */
    uint32 ofsI; /**< TODO */
    uint32 nTextureanimations; /**< TODO */
    uint32 ofsTextureanimations; /**< TODO */
    uint32 nTexReplace; /**< TODO */
    uint32 ofsTexReplace; /**< TODO */
    uint32 nRenderFlags; /**< TODO */
    uint32 ofsRenderFlags; /**< TODO */
    uint32 nBoneLookupTable; /**< TODO */
    uint32 ofsBoneLookupTable; /**< TODO */
    uint32 nTexLookup; /**< TODO */
    uint32 ofsTexLookup; /**< TODO */
    uint32 nTexUnits; /**< TODO */
    uint32 ofsTexUnits; /**< TODO */
    uint32 nTransLookup; /**< TODO */
    uint32 ofsTransLookup; /**< TODO */
    uint32 nTexAnimLookup; /**< TODO */
    uint32 ofsTexAnimLookup; /**< TODO */
    float floats[14]; /**< TODO */
    uint32 nBoundingTriangles; /**< TODO */
    uint32 ofsBoundingTriangles; /**< TODO */
    uint32 nBoundingVertices; /**< TODO */
    uint32 ofsBoundingVertices; /**< TODO */
    uint32 nBoundingNormals; /**< TODO */
    uint32 ofsBoundingNormals; /**< TODO */
    uint32 nAttachments; /**< TODO */
    uint32 ofsAttachments; /**< TODO */
    uint32 nAttachLookup; /**< TODO */
    uint32 ofsAttachLookup; /**< TODO */
    uint32 nAttachments_2; /**< TODO */
    uint32 ofsAttachments_2; /**< TODO */
    uint32 nLights; /**< TODO */
    uint32 ofsLights; /**< TODO */
    uint32 nCameras; /**< TODO */
    uint32 ofsCameras; /**< TODO */
    uint32 nCameraLookup; /**< TODO */
    uint32 ofsCameraLookup; /**< TODO */
    uint32 nRibbonEmitters; /**< TODO */
    uint32 ofsRibbonEmitters; /**< TODO */
    uint32 nParticleEmitters; /**< TODO */
    uint32 ofsParticleEmitters; /**< TODO */

};

struct ModelBoundingVertex
{
    Vec3D pos; /**< TODO */
};

#pragma pack(pop)
#endif
