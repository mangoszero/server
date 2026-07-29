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

#ifndef MANGOS_H_DYNAMICCOLLISION
#define MANGOS_H_DYNAMICCOLLISION

// Game-object collision for one Map. Replaces DynamicMapTree, a BIH that was rebuilt
// periodically through balance() and update().
//
// That design treated every game object as dynamic and paid tree-rebuild cost for
// objects that never move, which is backwards for WoW: of the collidable game objects,
// effectively all are pose-frozen after spawn -- doors, chests, bridges, mailboxes. A
// door "opening" flips a collidable FLAG, it does not move. Only a handful per map
// genuinely re-pose.
//
// So the partition is on mobility, not on space. Frozen bodies are filed into the same
// 64x64 tile grid the terrain uses, under every tile their world box overlaps, so a
// hundred-yard bridge is found from any tile it touches -- a position-keyed grid would
// file it under one cell. In steady state those buckets never change and a query is a
// flat sweep over a contiguous vector: no tree, no traversal stack, nothing to rebuild.
//
// Both worlds answer a segment the same way -- a fraction of src->dest, greater than one
// when clear -- so Map can ask both over the SAME segment and keep the smaller. That
// order matters: bounding this sweep by the static hit, as Map used to, hides every body
// standing in the last modifyDist of the ray, because the static hit handed over had
// already been pulled back.
//
// Threading: one instance per Map, touched only by that map's update thread.

#include "terrain/Geometry.hpp"
#include "terrain/ILiveGeometry.hpp"

#include <cfloat>
#include <cstdint>
#include <unordered_map>
#include <vector>

class GameObjectModel;

class DynamicCollision : public world::terrain::ILiveGeometry
{
    public:
        DynamicCollision() = default;

        void Insert(GameObjectModel& model);
        void Remove(GameObjectModel& model);
        bool Contains(const GameObjectModel& model) const;

        // Re-files a body whose world box changed. The POSE is the caller's to set
        // first: a spatial index has no business reading a game object.
        void Refresh(GameObjectModel& model);

        int Size() const { return static_cast<int>(m_all.size()); }

        bool IsInLineOfSight(float x1, float y1, float z1, float x2, float y2, float z2,
                             uint32_t phasemask) const;

        // Nearest collidable hit along the segment as a fraction of it; > 1 when nothing
        // blocks. Same primitive and same units as FusedTerrain::NearestHitFraction, so
        // Map can compare the two directly and resolve only the winner into a point.
        float NearestHitFraction(float x1, float y1, float z1, float x2, float y2,
                                 float z2, uint32_t phasemask) const;

        // Every collidable surface crossing the window over (x,y), appended to the
        // terrain engine's column. `filter` is the phase mask: this is the ILiveGeometry
        // side of the seam, so the engine hands it back without having looked at it.
        void AddSurfaces(float x, float y, float zTop, float zBottom, uint32_t filter,
                         world::terrain::Column& out) const override;

    private:
        void FileBody(GameObjectModel& model);
        void UnfileBody(GameObjectModel& model);

        // Visits each body whose bucket overlaps the XY box at most once.
        template <typename F>
        void ForEachCandidate(float minx, float miny, float maxx, float maxy, F&& f) const;

        static uint32_t CellKey(int tx, int ty)
        {
            return uint32_t(tx) * 64u + uint32_t(ty);
        }

        std::unordered_map<uint32_t, std::vector<GameObjectModel*>> m_buckets;
        std::vector<GameObjectModel*> m_all;

        mutable uint32_t m_epoch = 0;
};

#endif
