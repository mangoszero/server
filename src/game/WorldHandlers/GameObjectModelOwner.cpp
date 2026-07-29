/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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

// The only part of GameObjectModel that knows what a GameObject is.
//
// Everything else about a collidable body -- its geometry, its placement, its world box,
// its raycasts -- needs no world at all, and keeping that half free of GameObject.h is
// what lets it be built and tested without standing a server up.

#include "GameObjectModel.h"

#include "GameObject.h"
#include "Map.h"
#include "Geometry/Quat.h"
#include "terrain/GoModelStore.hpp"

using Geometry::Vector3;

GameObjectModel* GameObjectModel::Create(const GameObject* pGo)
{
    if (!pGo)
    {
        return nullptr;
    }

    GameObjectModel* model = new GameObjectModel();
    if (!model->Initialize(pGo, pGo->GetDisplayId()))
    {
        delete model;
        return nullptr;
    }
    return model;
}

bool GameObjectModel::Initialize(const GameObject* pGo, uint32 displayId)
{
    m_model = world::terrain::GoModelStore::Instance().Get(displayId);
    if (!m_model || m_model->Empty())
    {
        return false;
    }

    m_owner = pGo;
    // Vanilla has no phasing. The field stays, because the collision index is
    // shared with the cores that do phase, and every body sits in the only phase
    // there is so the filter always passes.
    m_phaseMask = Map::PHASE_ANY;
    UpdatePose();
    return true;
}

void GameObjectModel::UpdatePose()
{
    if (!m_owner)
    {
        return;
    }

    Geometry::Quat q;
    m_owner->GetQuaternion(q);

    m_xf.pos = Vector3(m_owner->Where().X(), m_owner->Where().Y(),
                       m_owner->Where().Z());
    m_xf.rot = Geometry::Mat3::fromQuat(q.x, q.y, q.z, q.w);
    m_xf.scale = m_owner->GetObjectScale();
    if (!(m_xf.scale > 0.f))
    {
        m_xf.scale = 1.f;
    }

    DeriveBounds();
}

