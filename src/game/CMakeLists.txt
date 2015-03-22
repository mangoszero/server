# This code is part of MaNGOS. Contributor & Copyright details are in AUTHORS/THANKS.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

set(LIBRARY_NAME game)

add_definitions(-DDT_POLYREF64)

include_directories(
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/AuctionHouseBot"
  "${CMAKE_CURRENT_SOURCE_DIR}/BattleGround"
  "${CMAKE_CURRENT_SOURCE_DIR}/ChatCommands"
  "${CMAKE_CURRENT_SOURCE_DIR}/MotionGenerators"
  "${CMAKE_CURRENT_SOURCE_DIR}/Object"
  "${CMAKE_CURRENT_SOURCE_DIR}/OutdoorPvP"
  "${CMAKE_CURRENT_SOURCE_DIR}/References"
  "${CMAKE_CURRENT_SOURCE_DIR}/Server"
  "${CMAKE_CURRENT_SOURCE_DIR}/Tools"
  "${CMAKE_CURRENT_SOURCE_DIR}/vmap"
  "${CMAKE_CURRENT_SOURCE_DIR}/WorldHandlers"
  "${CMAKE_SOURCE_DIR}/dep/include/g3dlite"
  "${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour/Include"
  "${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour"
  "${CMAKE_SOURCE_DIR}/dep/recastnavigation/"
  "${CMAKE_SOURCE_DIR}/dep/include"
  "${CMAKE_SOURCE_DIR}/src/shared"
  "${CMAKE_SOURCE_DIR}/src/framework"
  "${CMAKE_BINARY_DIR}"
  "${CMAKE_BINARY_DIR}/src/shared"
  "${MYSQL_INCLUDE_DIR}"
  "${ACE_INCLUDE_DIR}"
)

#AH Bot group
file(GLOB SRC_FILES_AHBOT AuctionHouseBot/*.cpp AuctionHouseBot/*.h)
    set(SRC_GRP_AHBOT
        ${SRC_FILES_AHBOT}
    )

source_group("AhBot"
  FILES
    ${SRC_GRP_AHBOT}
)

#Battlegrounds group
file(GLOB SRC_FILES_BATTLEGROUND BattleGround/*.cpp BattleGround/*.h)
    set(SRC_GRP_BATTLEGROUND
        ${SRC_FILES_BATTLEGROUND}
    )
	
source_group("BattleGround"
  FILES
    ${SRC_GRP_BATTLEGROUND}
)


#Chat Commands group
file(GLOB SRC_FILES_CHAT_COMMANDS ChatCommands/*.cpp ChatCommands/*.h)
    set(SRC_GRP_CHAT_COMMANDS
        ${SRC_FILES_CHAT_COMMANDS}
    )

source_group("Chat Commands"
  FILES
    ${SRC_GRP_CHAT_COMMANDS}
)

#Motion Generator group
file(GLOB SRC_FILES_MOTION_GEN MotionGenerators/*.cpp MotionGenerators/*.h)
    set(SRC_GRP_MOTION_GEN
        ${SRC_FILES_MOTION_GEN}
    )

source_group("Motion generators"
  FILES
    ${SRC_GRP_MOTION_GEN}
)

#Movement group
file(GLOB SRC_FILES_MOVEMENT movement/*.cpp movement/*.h)
    set(SRC_GRP_MOVEMENT
        ${SRC_FILES_MOVEMENT}
    )

source_group("Movement"
  FILES
    ${SRC_GRP_MOVEMENT}
)

#Object group
file(GLOB SRC_FILES_OBJECT Object/*.cpp Object/*.h)
    set(SRC_GRP_OBJECT
        ${SRC_FILES_OBJECT}
    )

source_group("Object"
  FILES
    ${SRC_GRP_OBJECT}
)	

#Outdoors PvP group
file(GLOB SRC_FILES_OUTDOOR_PVP OutdoorPvP/*.cpp OutdoorPvP/*.h)
    set(SRC_GRP_OUTDOOR_PVP
        ${SRC_FILES_OUTDOOR_PVP}
    )
	
source_group("Outdoor PvP"
  FILES
    ${SRC_GRP_OUTDOOR_PVP}
)

#References group
file(GLOB SRC_FILES_REFERENCES References/*.cpp References/*.h)
    set(SRC_GRP_REFERENCES
        ${SRC_FILES_REFERENCES}
    )
	
source_group("References"
  FILES
    ${SRC_GRP_REFERENCES}
)

#Server group
file(GLOB SRC_FILES_SERVER Server/*.cpp Server/*.h)
    set(SRC_GRP_SERVER
        ${SRC_FILES_SERVER}
    )
	
source_group("Server"
  FILES
    ${SRC_GRP_SERVER}
)
	
#Tools group
file(GLOB SRC_FILES_TOOL Tools/*.cpp Tools/*.h)
    set(SRC_GRP_TOOL
        ${SRC_FILES_TOOL}
    )

source_group("Tool"
  FILES
    ${SRC_GRP_TOOL}
)

#VMap group
file(GLOB SRC_FILES_VMAPS vmap/*.cpp vmap/*.h)
    set(SRC_GRP_VMAPS
        ${SRC_FILES_VMAPS}
    )

source_group("vmaps"
  FILES
    ${SRC_GRP_VMAPS}
)
	
#World and Handlers group
file(GLOB SRC_FILES_WORLD_HANDLERS WorldHandlers/*.cpp WorldHandlers/*.h)
    set(SRC_GRP_WORLD_HANDLERS
        ${SRC_FILES_WORLD_HANDLERS}
    )

source_group("World/Handlers"
  FILES
    ${SRC_GRP_WORLD_HANDLERS}
)
	
set(LIBRARY_SRCS
    ${SRC_GRP_AHBOT}
    ${SRC_GRP_BATTLEGROUND}
    ${SRC_GRP_CHAT_COMMANDS}
    ${SRC_GRP_MOTION_GEN}
    ${SRC_GRP_MOVEMENT}
    ${SRC_GRP_OBJECT}
    ${SRC_GRP_OUTDOOR_PVP}
    ${SRC_GRP_REFERENCES}
    ${SRC_GRP_SERVER}
    ${SRC_GRP_TOOL}
    ${SRC_GRP_VMAPS}
    ${SRC_GRP_WORLD_HANDLERS}
    pchdef.cpp
    pchdef.h
)

if(PCH)
  include_directories("${CMAKE_CURRENT_BINARY_DIR}")
endif()

add_library(${LIBRARY_NAME} STATIC
  ${LIBRARY_SRCS}
)

target_link_libraries(${LIBRARY_NAME}
  shared
  detour
)

if(UNIX)
  # Both systems don't have libdl and don't need them
  if (NOT (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD"))
    target_link_libraries(${LIBRARY_NAME}
      dl
    )
  endif()
endif()

if(NOT ACE_USE_EXTERNAL)
    add_dependencies(${LIBRARY_NAME} ace)
endif()

# Generate precompiled header
if(PCH)
    ADD_CXX_PCH(${LIBRARY_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/pchdef.h ${CMAKE_CURRENT_SOURCE_DIR}/pchdef.cpp)
endif()

add_subdirectory(AuctionHouseBot)

if(SCRIPT_LIB_SD2)
    include_directories(
        "${CMAKE_SOURCE_DIR}/src/modules/SD2"
    )
    target_link_libraries(${LIBRARY_NAME}
        mangosscript
    )
    add_dependencies(${LIBRARY_NAME} mangosscript)
endif()

#Eluna specific start

if(SCRIPT_LIB_ELUNA)
  include_directories(
    "${CMAKE_SOURCE_DIR}/dep/lualib"
    "${CMAKE_SOURCE_DIR}/src/modules/Eluna"
  )
  list(APPEND LIBRARY_SRCS ${SRC_GRP_LUAENGINE})

  target_link_libraries(${LIBRARY_NAME} LuaEngine)
  target_link_libraries(${LIBRARY_NAME} lualib)

  add_dependencies(${LIBRARY_NAME} LuaEngine)
  add_dependencies(${LIBRARY_NAME} lualib)
endif()

#Eluna specific end
