# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.

# =============================================================================
# The object hierarchy owns no geometry: an object HAS a Geometry::Placement. The
# linker cannot hold that seam -- a member function costs nothing to add back --
# so it is held here. Free functions that compose a placement with what the
# component must not know (phase, world membership, terrain, line of sight) are
# deliberately allowed; so is the write-through that keeps the extent current.
# =============================================================================

set(HIERARCHY_HEADERS
    Object/Object.h
    Object/Unit.h
    Object/Player.h
    Object/Creature.h
    Object/GameObject.h
    Object/DynamicObject.h
    Object/Corpse.h
    Object/Vehicle.h
    Object/Pet.h
    Object/Totem.h
    Object/TemporarySummon.h)

set(FORBIDDEN_MEMBERS
    GetPositionX GetPositionY GetPositionZ GetOrientation
    GetObjectBoundingRadius GetDistance GetDistance2d GetDistanceZ
    GetDistanceOrder IsWithinDist IsWithinDist2d IsWithinDist3d
    IsWithinDistInMap _IsWithinDist IsInRange IsInRange2d IsInRange3d
    GetAngle HasInArc IsInFront IsInBack IsInFrontInMap IsInBackInMap
    IsInMap IsWithinLOS IsWithinLOSInMap IsPositionValid
    Relocate SetOrientation GetNearPoint GetNearPoint2D GetClosePoint
    GetContactPoint GetRandomPoint UpdateGroundPositionZ
    UpdateAllowedPositionZ GetRespawnCoord SetRespawnCoord ResetRespawnCoord
    GetCombatStartPosition SetCombatStartPosition GetCombatReach
    GetCombatDistance CanReachWithMeleeAttack IsNearWaypoint
    NormalizeRotatedPosition CalculateLocalPositionOf RotateLocalPosition
    GetLocalPositionX GetLocalPositionY GetLocalPositionZ GetLocalOrientation
    GetOrientationFromQuat)

set(VIOLATIONS "")

foreach(HEADER IN LISTS HIERARCHY_HEADERS)
  set(PATH "${SOURCE_ROOT}/src/game/${HEADER}")
  if(NOT EXISTS "${PATH}")
    continue()
  endif()
  file(READ "${PATH}" TEXT)
  foreach(NAME IN LISTS FORBIDDEN_MEMBERS)
    # Indented return type + name + parameter list: a member. Free functions open
    # at column zero and do not match.
    if(TEXT MATCHES "\n[ \t]+[A-Za-z_][A-Za-z_0-9:<>,&\\* \t]*[ \t\\*&]${NAME}[ \t]*\\(")
      list(APPEND VIOLATIONS "${HEADER}: ${NAME}")
    endif()
  endforeach()
endforeach()

file(READ "${SOURCE_ROOT}/src/game/Object/Object.h" OBJECT_H)
foreach(REQUIRED_TEXT
    "Geometry::Placement m_placement"
    "Geometry::Placement const& Where() const")
  string(FIND "${OBJECT_H}" "${REQUIRED_TEXT}" POSITION)
  if(POSITION EQUAL -1)
    list(APPEND VIOLATIONS "Object.h no longer holds the component: ${REQUIRED_TEXT}")
  endif()
endforeach()

if(VIOLATIONS)
  string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
  message(FATAL_ERROR
    "Spatial geometry is back on the object hierarchy:\n  ${REPORT}\n"
    "Ask the component instead: obj->Where().DistanceTo(other->Where()).")
endif()

message(STATUS "Spatial boundary intact: the hierarchy owns no geometry")
