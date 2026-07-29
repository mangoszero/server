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

#include "DynamicCollision.h"

#include "GameObjectModel.h"
#include "terrain/Terrain.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>

using Geometry::Vector3;

namespace
{
    constexpr int GRID_COUNT = world::terrain::FusedTerrainGridCount;

    int ClampTile(int v)
    {
        return std::min(GRID_COUNT - 1, std::max(0, v));
    }
}

void DynamicCollision::FileBody(GameObjectModel& model)
{
    model.m_cells.clear();
    const Geometry::Aabb& b = model.GetBounds();
    if (!b.valid())
    {
        return;
    }

    // Filed under EVERY tile the body's box overlaps. Keying on its position instead
    // would file a hundred-yard bridge under one cell and lose it from every other.
    const int txLo = ClampTile(world::terrain::TileIndex(b.hi.x));
    const int txHi = ClampTile(world::terrain::TileIndex(b.lo.x));
    const int tyLo = ClampTile(world::terrain::TileIndex(b.hi.y));
    const int tyHi = ClampTile(world::terrain::TileIndex(b.lo.y));

    for (int tx = txLo; tx <= txHi; ++tx)
    {
        for (int ty = tyLo; ty <= tyHi; ++ty)
        {
            const uint32_t key = CellKey(tx, ty);
            m_buckets[key].push_back(&model);
            model.m_cells.push_back(key);
        }
    }
}

void DynamicCollision::UnfileBody(GameObjectModel& model)
{
    for (uint32_t key : model.m_cells)
    {
        auto bucket = m_buckets.find(key);
        if (bucket == m_buckets.end())
        {
            continue;
        }
        auto& v = bucket->second;
        v.erase(std::remove(v.begin(), v.end(), &model), v.end());
        if (v.empty())
        {
            m_buckets.erase(bucket);
        }
    }
    model.m_cells.clear();
}

void DynamicCollision::Insert(GameObjectModel& model)
{
    if (Contains(model))
    {
        return;
    }
    m_all.push_back(&model);
    FileBody(model);
}

void DynamicCollision::Remove(GameObjectModel& model)
{
    UnfileBody(model);
    m_all.erase(std::remove(m_all.begin(), m_all.end(), &model), m_all.end());
}

bool DynamicCollision::Contains(const GameObjectModel& model) const
{
    return std::find(m_all.begin(), m_all.end(), &model) != m_all.end();
}

void DynamicCollision::Refresh(GameObjectModel& model)
{
    if (!Contains(model))
    {
        return;
    }
    UnfileBody(model);
    FileBody(model);
}

template <typename F>
void DynamicCollision::ForEachCandidate(float minx, float miny, float maxx, float maxy,
                                        F&& f) const
{
    const int txLo = ClampTile(world::terrain::TileIndex(maxx));
    const int txHi = ClampTile(world::terrain::TileIndex(minx));
    const int tyLo = ClampTile(world::terrain::TileIndex(maxy));
    const int tyHi = ClampTile(world::terrain::TileIndex(miny));

    ++m_epoch;
    for (int tx = txLo; tx <= txHi; ++tx)
    {
        for (int ty = tyLo; ty <= tyHi; ++ty)
        {
            auto bucket = m_buckets.find(CellKey(tx, ty));
            if (bucket == m_buckets.end())
            {
                continue;
            }
            for (GameObjectModel* model : bucket->second)
            {
                // A body spanning several cells appears in several buckets; the stamp
                // is what keeps one query from raycasting it more than once.
                if (model->m_epoch == m_epoch)
                {
                    continue;
                }
                model->m_epoch = m_epoch;
                f(*model);
            }
        }
    }
}

float DynamicCollision::NearestHitFraction(float x1, float y1, float z1, float x2,
                                           float y2, float z2, uint32_t phasemask) const
{
    const Vector3 a{x1, y1, z1};
    const Vector3 b{x2, y2, z2};
    const Vector3 seg = b - a;
    if (Geometry::dot(seg, seg) < 1e-6f)
    {
        return 2.0f;
    }

    auto inv = [](float d) { return std::fabs(d) > 1e-9f ? 1.0f / d : 1e30f; };
    const Vector3 invDir{inv(seg.x), inv(seg.y), inv(seg.z)};

    float best = 2.0f;
    ForEachCandidate(std::min(x1, x2), std::min(y1, y2), std::max(x1, x2),
                     std::max(y1, y2), [&](const GameObjectModel& model)
    {
        if (!model.IsCollidable() || !(model.GetPhaseMask() & phasemask) ||
            !model.GetBounds().intersectsRay(a, invDir, 1.0f))
        {
            return;
        }
        const float t = model.SegmentHitFraction(a, b);
        if (t < best)
        {
            best = t;
        }
    });
    return best;
}

bool DynamicCollision::IsInLineOfSight(float x1, float y1, float z1, float x2, float y2,
                                       float z2, uint32_t phasemask) const
{
    return NearestHitFraction(x1, y1, z1, x2, y2, z2, phasemask) > 1.0f;
}

void DynamicCollision::AddSurfaces(float x, float y, float zTop, float zBottom,
                                   uint32_t filter, world::terrain::Column& out) const
{
    ForEachCandidate(x, y, x, y, [&](const GameObjectModel& model)
    {
        if (!model.IsCollidable() || !(model.GetPhaseMask() & filter))
        {
            return;
        }
        model.AddSurfaces(x, y, zTop, zBottom, out);
    });
}
