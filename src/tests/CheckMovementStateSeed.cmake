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
# THE CREATE BLOCK IS WRITTEN FROM THE MOVEMENT STATE, NOT FROM THE PLACEMENT.
# Every site where the SERVER places a unit must seed m_movementInfo alongside,
# or the client is told the unit is at the map origin -- a `Position` that
# zero-initialises, so the lie is exactly (0, 0, 0). A player logging in falls
# out of the world; a creature that never walks a path is drawn at the origin
# forever, because only its first SMSG_MONSTER_MOVE would have corrected it.
#
# This cannot be a unit test: it needs Player, Creature and Map, and the test
# binary deliberately links no `game`. So it is checked as source, over text
# normalised to single spaces -- reindenting or rewrapping a call is invisible
# here, renaming or deleting one is not.
#
# Both setter names pass, because the four cores have not converged on one:
# 02 stores a Geometry::Placement and calls Report(), 00/01/03 keep the raw
# Position and call ChangePosition(). See PORTING.md.
# =============================================================================

set(SEED_CALL "m_movementInfo\\.(ChangePosition|Report)\\(")

# file | anchor: the placement the server performs, which the seed must follow.
# The window is generous enough for the comment that explains each one.
set(SEEDED_SITES
    "Object/Creature.cpp|cr->Place\\(\\)\\.MoveTo\\("
    "Object/Player.cpp|Place\\(\\)\\.MoveTo\\(info->positionX"
    "Object/Player.cpp|PlayerRelocation\\(this,"
    "Object/PlayerLoad.cpp|Place\\(\\)\\.MoveTo\\(fields\\[12\\]"
    "Object/PlayerLoad.cpp|SetFallInformation\\(0, Where\\(\\)\\.Z\\(\\)\\)"
    # Unit::InterruptMoving is not a site here: it re-places through SetPosition or
    # Map::CreatureRelocation, both of which are.
    "WorldHandlers/Map.cpp|player->Place\\(\\)\\.MoveTo\\(x, y, z, orientation\\)"
    "WorldHandlers/Map.cpp|creature->Place\\(\\)\\.MoveTo\\(x, y, z, ang\\)"
    "WorldHandlers/Map.cpp|c->Place\\(\\)\\.MoveTo\\(resp_x"
    "WorldHandlers/MovementHandler.cpp|Place\\(\\)\\.MoveTo\\(loc\\.coord_x")

set(WINDOW 800)
set(VIOLATIONS "")

foreach(SITE IN LISTS SEEDED_SITES)
  string(REPLACE "|" ";" PARTS "${SITE}")
  list(GET PARTS 0 RELATIVE)
  list(GET PARTS 1 ANCHOR)

  set(PATH "${SOURCE_ROOT}/src/game/${RELATIVE}")
  if(NOT EXISTS "${PATH}")
    list(APPEND VIOLATIONS "${RELATIVE}: file is gone")
    continue()
  endif()

  file(READ "${PATH}" TEXT)
  string(REGEX REPLACE "[ \t\r\n]+" " " NORM "${TEXT}")

  string(REGEX MATCH "${ANCHOR}" HIT "${NORM}")
  if(NOT HIT)
    list(APPEND VIOLATIONS "${RELATIVE}: placement site not found (${ANCHOR})")
    continue()
  endif()

  string(FIND "${NORM}" "${HIT}" POSITION)
  string(LENGTH "${NORM}" TOTAL)
  math(EXPR REMAINING "${TOTAL} - ${POSITION}")
  if(REMAINING GREATER ${WINDOW})
    set(REMAINING ${WINDOW})
  endif()
  string(SUBSTRING "${NORM}" ${POSITION} ${REMAINING} TAIL)

  if(NOT TAIL MATCHES "${SEED_CALL}")
    list(APPEND VIOLATIONS "${RELATIVE}: ${HIT} places without seeding the movement state")
  endif()
endforeach()

# The premise. If the create block stops reading the movement state, the seeds
# above are no longer what keeps a unit off the map origin -- and this check is
# the thing to rewrite, not the code it is guarding.
file(READ "${SOURCE_ROOT}/src/game/Object/ObjectUpdate.cpp" OBJECT_UPDATE)
if(NOT OBJECT_UPDATE MATCHES "WriteMovementInfo")
  list(APPEND VIOLATIONS
      "ObjectUpdate.cpp: the create block no longer writes the movement state")
endif()

if(VIOLATIONS)
  string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
  message(FATAL_ERROR
    "The server places a unit without telling the movement state:\n  ${REPORT}\n"
    "The create block is written from m_movementInfo, so an unseeded placement "
    "sends (0, 0, 0). Seed it beside the Place() call.")
endif()

message(STATUS "Movement state seeded at every server-side placement")
