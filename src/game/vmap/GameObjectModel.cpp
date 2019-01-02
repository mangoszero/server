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

#include "VMapFactory.h"
#include "VMapManager2.h"
#include "VMapDefinitions.h"
#include "WorldModel.h"

#include "GameObject.h"
#include "World.h"
#include "GameObjectModel.h"
#include "DBCStores.h"

struct GameobjectModelData
{
    GameobjectModelData(const std::string& name_, const G3D::AABox& box) :
        name(name_), bound(box) {}

    std::string name;
    G3D::AABox bound;
};

typedef UNORDERED_MAP<uint32, GameobjectModelData> ModelList;
ModelList model_list;

void LoadGameObjectModelList()
{
    FILE* model_list_file = fopen((sWorld.GetDataPath() + "vmaps/" + VMAP::GAMEOBJECT_MODELS).c_str(), "rb");
    if (!model_list_file)
        { return; }

    uint32 name_length, displayId;
    char buff[500];
    while (!feof(model_list_file))
    {
        if (fread(&displayId, sizeof(uint32), 1, model_list_file) <= 0)
        {
            sLog.outDebug("File %s seems to be corrupted", VMAP::GAMEOBJECT_MODELS);
            break;
        }
        if (fread(&name_length, sizeof(uint32), 1, model_list_file) <= 0)
        {
            sLog.outDebug("File %s seems to be corrupted", VMAP::GAMEOBJECT_MODELS);
            break;
        }

        if (name_length >= sizeof(buff))
        {
            sLog.outDebug("File %s seems to be corrupted", VMAP::GAMEOBJECT_MODELS);
            break;
        }

        if (fread(&buff, sizeof(char), name_length, model_list_file) <= 0)
        {
            sLog.outDebug("File %s seems to be corrupted", VMAP::GAMEOBJECT_MODELS);
            break;
        }

        G3D::Vector3 v1, v2;
        if (fread(&v1, sizeof(G3D::Vector3), 1, model_list_file) <= 0)
        {
            sLog.outDebug("File %s seems to be corrupted", VMAP::GAMEOBJECT_MODELS);
            break;
        }
        if (fread(&v2, sizeof(G3D::Vector3), 1, model_list_file) <= 0)
        {
            sLog.outDebug("File %s seems to be corrupted", VMAP::GAMEOBJECT_MODELS);
            break;
        }

        model_list.insert(ModelList::value_type(displayId, GameobjectModelData(std::string(buff, name_length), AABox(v1, v2))));
    }
    fclose(model_list_file);
}

GameObjectModel::~GameObjectModel()
{
    if (iModel)
        { ((VMAP::VMapManager2*)VMAP::VMapFactory::createOrGetVMapManager())->releaseModelInstance(iName); }
}

bool GameObjectModel::initialize(const GameObject* const pGo, const GameObjectDisplayInfoEntry* const pDisplayInfo)
{
    ModelList::const_iterator it = model_list.find(pDisplayInfo->Displayid);
    if (it == model_list.end())
        { return false; }

    iModelBound = it->second.bound;
    // ignore models with no bounds
    if (iModelBound == G3D::AABox::zero())
    {
        sLog.outDebug("Model %s has zero bounds, loading skipped", it->second.name.c_str());
        return false;
    }

    iModel = ((VMAP::VMapManager2*)VMAP::VMapFactory::createOrGetVMapManager())->acquireModelInstance(sWorld.GetDataPath() + "vmaps/", it->second.name);

    if (!iModel)
        { return false; }

    iOwner = pGo;
    iName = it->second.name;
    iPos = Vector3(pGo->GetPositionX(), pGo->GetPositionY(), pGo->GetPositionZ());
    iScale = pGo->GetObjectScale();
    iInvScale = 1.f / iScale;

    G3D::Quat q;
    pGo->GetQuaternion(q);
    UpdateRotation(q);

    return true;
}

void GameObjectModel::UpdateRotation(G3D::Quat const& q)
{
    iQuat = q;

    G3D::AABox mdl_box(iModelBound);

    // transform bounding box:
    mdl_box = AABox(mdl_box.low() * iScale, mdl_box.high() * iScale);

    G3D::AABox rotated_bounds;

    for (int i = 0; i < 8; ++i)
        { rotated_bounds.merge((iQuat * G3D::Quat(mdl_box.corner(i)) * iQuat.conj()).imag()); }

    iBound = rotated_bounds + iPos;
}

GameObjectModel* GameObjectModel::Create(const GameObject* const pGo)
{
    const GameObjectDisplayInfoEntry* info = sGameObjectDisplayInfoStore.LookupEntry(pGo->GetDisplayId());
    if (!info)
        { return NULL; }

    GameObjectModel* mdl = new GameObjectModel();
    if (!mdl->initialize(pGo, info))
    {
        delete mdl;
        return NULL;
    }

    return mdl;
}

bool GameObjectModel::IntersectRay(const G3D::Ray& ray, float& MaxDist, bool StopAtFirstHit) const
{
    if (!isCollidable)
        { return false; }

    float time = ray.intersectionTime(iBound);
    if (time == G3D::inf())
        { return false; }

    // child bounds are defined in object space:
    Vector3 p = (iQuat.conj() * G3D::Quat((ray.origin() - iPos) * iInvScale) * iQuat).imag();
    Ray modRay(p, (iQuat.conj() * G3D::Quat(ray.direction()) * iQuat).imag());
    float distance = MaxDist * iInvScale;
    bool hit = iModel->IntersectRay(modRay, distance, StopAtFirstHit);
    if (hit)
    {
        distance *= iScale;
        MaxDist = distance;
    }
    return hit;
}

bool GameObjectModel::GetIntersectPoint(const G3D::Vector3& srcPoint, G3D::Vector3& dstPoint, bool absolute) const
{
    G3D::Vector3 p;
    if (absolute)
        p = (iQuat.conj() * G3D::Quat((srcPoint - iPos) * iInvScale) * iQuat).imag();
    else
        p = srcPoint;

    float dist;
    bool hit = iModel->GetContactPoint(p, G3D::Vector3(0.0f, 0.0f, -1.0f), dist);
    if (hit)
    {
        dstPoint = p;
        dstPoint.z -= dist;
    }
    return hit;
}

void GameObjectModel::GetLocalCoords(const G3D::Vector3& worldCoords, G3D::Vector3& localCoords)
{

}

void GameObjectModel::GetWorldCoords(const G3D::Vector3& localCoords, G3D::Vector3& worldCoords)
{

}
