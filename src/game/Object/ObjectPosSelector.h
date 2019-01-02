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

#ifndef MANGOS_H_OBJECT_POS_SELECTOR
#define MANGOS_H_OBJECT_POS_SELECTOR

#include<Common.h>

#include<map>

class WorldObject;

enum UsedAreaSide { USED_POS_PLUS, USED_POS_MINUS };

inline UsedAreaSide operator ~(UsedAreaSide side)
{
    return side == USED_POS_PLUS ? USED_POS_MINUS : USED_POS_PLUS;
}

inline float SignOf(UsedAreaSide side)
{
    return side == USED_POS_PLUS ? 1.0f : -1.0f;
}

struct ObjectPosSelector
{
    struct OccupiedArea
    {
        OccupiedArea(float _angleOffset, WorldObject const* obj) : angleOffset(_angleOffset), occupyingObj(obj) {}
        float angleOffset;
        WorldObject const* occupyingObj;
    };
    // angle pos -> OccupiedArea
    typedef std::multimap<float, OccupiedArea> UsedAreaList;
    typedef UsedAreaList::value_type UsedArea;

    ObjectPosSelector(float x, float y, float dist, float searchedForSize, WorldObject const* searchPosFor);

    void AddUsedArea(WorldObject const* obj, float angle, float dist);

    bool CheckOriginalAngle() const;

    void InitializeAngle();

    bool NextAngle(float& angle);
    bool NextUsedAngle(float& angle);

    bool CheckAngle(UsedArea const& usedArea, UsedAreaSide side, float angle) const;
    void InitializeAngle(UsedAreaSide side);
    bool NextSideAngle(UsedAreaSide side, float& angle);

    float m_centerX;
    float m_centerY;
    float m_searcherDist;                                   // distance for searching pos
    float m_searchedForReqHAngle;                           // angle size/2 of searcher object (at dist distance)

    UsedAreaList m_UsedAreaLists[2];                        // list left/right side used angles (with angle size)

    UsedAreaList::const_iterator m_nextUsedAreaItr[2];      // next used used areas for check at left/right side, possible angles selected in range m_smallStepAngle..m_nextUsedAreaItr

    float m_stepAngle[2];                                   // current checked angle position at sides (less m_nextUsedArea), positive value

    WorldObject const* m_searchPosFor;                      // For whom a position is searched (can be NULL)
};
#endif
