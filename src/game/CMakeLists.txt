# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2017  MaNGOS project <https://getmangos.eu>
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
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/AuctionHouseBot
    ${CMAKE_CURRENT_SOURCE_DIR}/BattleGround
    ${CMAKE_CURRENT_SOURCE_DIR}/ChatCommands
    ${CMAKE_CURRENT_SOURCE_DIR}/Maps
    ${CMAKE_CURRENT_SOURCE_DIR}/MotionGenerators
    ${CMAKE_CURRENT_SOURCE_DIR}/Object
    ${CMAKE_CURRENT_SOURCE_DIR}/OutdoorPvP
    ${CMAKE_CURRENT_SOURCE_DIR}/References
    ${CMAKE_CURRENT_SOURCE_DIR}/Server
    ${CMAKE_CURRENT_SOURCE_DIR}/Tools
    ${CMAKE_CURRENT_SOURCE_DIR}/vmap
    ${CMAKE_CURRENT_SOURCE_DIR}/Warden
    ${CMAKE_CURRENT_SOURCE_DIR}/Warden/Modules
    ${CMAKE_CURRENT_SOURCE_DIR}/WorldHandlers
    ${CMAKE_SOURCE_DIR}/dep/g3dlite
    ${CMAKE_SOURCE_DIR}/dep/zlib
    ${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour/Include
    ${CMAKE_SOURCE_DIR}/dep/recastnavigation/Detour
    ${CMAKE_SOURCE_DIR}/dep/recastnavigation
    ${CMAKE_SOURCE_DIR}/dep/include
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/shared/Auth
    ${CMAKE_SOURCE_DIR}/src/shared/Config
    ${CMAKE_SOURCE_DIR}/src/shared/Common
    ${CMAKE_SOURCE_DIR}/src/shared/Database
    ${CMAKE_SOURCE_DIR}/src/shared/DataStores
    ${CMAKE_SOURCE_DIR}/src/shared/Utilities
    ${CMAKE_SOURCE_DIR}/src/shared/Log
    ${CMAKE_SOURCE_DIR}/src/shared/Threading
    ${CMAKE_SOURCE_DIR}/src/framework
    ${CMAKE_BINARY_DIR}/src/shared
    ${MYSQL_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
    ${ACE_INCLUDE_DIR}
)

#AH Bot group
file(GLOB SRC_GRP_AHBOT AuctionHouseBot/*.cpp AuctionHouseBot/*.h)
source_group("AhBot" FILES ${SRC_GRP_AHBOT})

#Battlegrounds group
file(GLOB SRC_GRP_BATTLEGROUND BattleGround/*.cpp BattleGround/*.h)
source_group("BattleGround" FILES ${SRC_GRP_BATTLEGROUND})

#Chat Commands group
file(GLOB SRC_GRP_CHAT_COMMANDS ChatCommands/*.cpp ChatCommands/*.h)
source_group("Chat Commands" FILES ${SRC_GRP_CHAT_COMMANDS})

#Map Threading group
file(GLOB SRC_GRP_MAP_THREAD Maps/*.cpp Maps/*.h)
source_group("Map Threading" FILES ${SRC_GRP_MAP_THREAD})

#Motion Generator group
file(GLOB SRC_GRP_MOTION_GEN MotionGenerators/*.cpp MotionGenerators/*.h)
source_group("Motion generators" FILES ${SRC_GRP_MOTION_GEN})

#Movement group
file(GLOB SRC_GRP_MOVEMENT movement/*.cpp movement/*.h)
source_group("Movement" FILES ${SRC_GRP_MOVEMENT})

#Object group
file(GLOB SRC_GRP_OBJECT Object/*.cpp Object/*.h)
source_group("Object" FILES ${SRC_GRP_OBJECT})

#Outdoors PvP group
file(GLOB SRC_GRP_OUTDOOR_PVP OutdoorPvP/*.cpp OutdoorPvP/*.h)
source_group("Outdoor PvP" FILES ${SRC_GRP_OUTDOOR_PVP})

#References group
file(GLOB SRC_GRP_REFERENCES References/*.cpp References/*.h)
source_group("References" FILES ${SRC_GRP_REFERENCES})

#Server group
file(GLOB SRC_GRP_SERVER Server/*.cpp Server/*.h)
source_group("Server" FILES ${SRC_GRP_SERVER})

#Tools group
file(GLOB SRC_GRP_TOOL Tools/*.cpp Tools/*.h)
source_group("Tool" FILES ${SRC_GRP_TOOL})

#VMap group
file(GLOB SRC_GRP_VMAPS vmap/*.cpp vmap/*.h)
source_group("vmaps" FILES ${SRC_GRP_VMAPS})

#Warden group
file(GLOB SRC_GRP_WARDEN Warden/*.cpp Warden/*.h)
source_group("Warden" FILES ${SRC_GRP_WARDEN})

#Warden Modules group
file(GLOB SRC_GRP_WARDEN_MODULES Warden/Modules/*.cpp Warden/Modules/*.h)
source_group("Warden\\Modules" FILES ${SRC_GRP_WARDEN_MODULES})

#World and Handlers group
file(GLOB SRC_GRP_WORLD_HANDLERS WorldHandlers/*.cpp WorldHandlers/*.h)
source_group("World/Handlers" FILES ${SRC_GRP_WORLD_HANDLERS})

set(LIBRARY_SRCS
    ${SRC_GRP_AHBOT}
    ${SRC_GRP_BATTLEGROUND}
    ${SRC_GRP_CHAT_COMMANDS}
    ${SRC_GRP_MAP_THREAD}
    ${SRC_GRP_MOTION_GEN}
    ${SRC_GRP_MOVEMENT}
    ${SRC_GRP_OBJECT}
    ${SRC_GRP_OUTDOOR_PVP}
    ${SRC_GRP_REFERENCES}
    ${SRC_GRP_SERVER}
    ${SRC_GRP_TOOL}
    ${SRC_GRP_VMAPS}
    ${SRC_GRP_WARDEN}
    ${SRC_GRP_WARDEN_MODULES}
    ${SRC_GRP_WORLD_HANDLERS}
    pchdef.cpp
    pchdef.h
)

if(PCH)
    include_directories(${CMAKE_CURRENT_BINARY_DIR})
endif()

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SRCS})

target_link_libraries(${LIBRARY_NAME} shared detour)

if(UNIX)
    # Both systems don't have libdl and don't need them
    if (NOT (CMAKE_SYSTEM_NAME STREQUAL "FreeBSD" OR CMAKE_SYSTEM_NAME STREQUAL "NetBSD"))
        target_link_libraries(${LIBRARY_NAME} dl)
    endif()
endif()

if(NOT ACE_USE_EXTERNAL)
    add_dependencies(${LIBRARY_NAME} ace)
endif()

# Generate precompiled header
if(PCH)
    ADD_CXX_PCH(${LIBRARY_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/pchdef.h ${CMAKE_CURRENT_SOURCE_DIR}/pchdef.cpp)
endif()

if(PLAYERBOTS)
    include_directories(
        ${CMAKE_SOURCE_DIR}/src/modules/Bots/playerbot
        ${CMAKE_SOURCE_DIR}/src/modules/Bots/ahbot
    )
    target_link_libraries(${LIBRARY_NAME} Bots)
    add_dependencies(${LIBRARY_NAME} Bots)
endif()

if(SCRIPT_LIB_SD3)
    include_directories(${CMAKE_SOURCE_DIR}/src/modules/SD3)
    target_link_libraries(${LIBRARY_NAME} mangosscript)
    add_dependencies(${LIBRARY_NAME} mangosscript)
endif()

if(SCRIPT_LIB_ELUNA)
    include_directories(
        ${CMAKE_SOURCE_DIR}/dep/lualib
        ${CMAKE_SOURCE_DIR}/src/modules/Eluna
    )
    list(APPEND LIBRARY_SRCS ${SRC_GRP_LUAENGINE})
    target_link_libraries(${LIBRARY_NAME} LuaEngine)
    target_link_libraries(${LIBRARY_NAME} lualib)
    add_dependencies(${LIBRARY_NAME} LuaEngine)
    add_dependencies(${LIBRARY_NAME} lualib)
endif()

#AH Bot Config
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/AuctionHouseBot/ahbot.conf.dist.in" "${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist" DESTINATION "${CONF_INSTALL_DIR}")

if(WIN32 AND MSVC)
    add_custom_command(TARGET ${LIBRARY_NAME}
        POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/AuctionHouseBot/ahbot.conf.dist" "${CONF_COPY_DIR}"
    )
endif()