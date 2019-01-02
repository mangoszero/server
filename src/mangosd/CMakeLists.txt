# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2019  MaNGOS project <https://getmangos.eu>
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

set(EXECUTABLE_NAME mangosd)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

#Main Files
file(GLOB SRC_GRP_MAIN *.cpp *.h)
source_group("Main" FILES ${SRC_GRP_MAIN})

set(EXECUTABLE_SRCS
    ${SRC_GRP_MAIN}
)

if(SOAP)
    file(GLOB SRC_GRP_SOAP SOAP/*.cpp SOAP/*.h)
    source_group("SOAP" FILES ${SRC_GRP_SOAP})
    LIST(APPEND EXECUTABLE_SRCS ${SRC_GRP_SOAP})
endif()

if(WIN32)
include(generate_product_version)
    generate_product_version(
        VersionFilesOutputVariable
        NAME "mangosd.exe"
        ICON ${CMAKE_SOURCE_DIR}/src/mangosd/mangosd.ico
        VERSION_MAJOR 0
        VERSION_MINOR 21
        VERSION_PATH ${BUILD_COUNTER}
        VERSION_REVISION ${BUILD_REVISION}
        COMPANY_NAME "getmangos.eu"
        # Do not alter, remove or prevent COMPANY_COPYRIGHT from displaying
        COMPANY_COPYRIGHT
        FILE_DESCRIPTION "Mangos Zero Server Daemon"
    )
    # add resource file to windows build
    set(EXECUTABLE_SRCS ${EXECUTABLE_SRCS} VersionResource.rc)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/src/shared
    ${CMAKE_SOURCE_DIR}/src/shared/Config
    ${CMAKE_SOURCE_DIR}/src/shared/Common
    ${CMAKE_SOURCE_DIR}/src/shared/Threading
    ${CMAKE_SOURCE_DIR}/src/shared/Utilities
    ${CMAKE_SOURCE_DIR}/src/shared/Log
    ${CMAKE_SOURCE_DIR}/src/shared/DataStores
    ${CMAKE_SOURCE_DIR}/src/shared/Win
    ${CMAKE_SOURCE_DIR}/src/shared/Linux
    ${CMAKE_SOURCE_DIR}/src/framework
    ${CMAKE_SOURCE_DIR}/src/game/AuctionHouseBot
    ${CMAKE_SOURCE_DIR}/src/game/BattleGround
    ${CMAKE_SOURCE_DIR}/src/game/Server
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
    ${CMAKE_SOURCE_DIR}/src/game/Maps
    ${CMAKE_SOURCE_DIR}/src/game/Object
    ${CMAKE_SOURCE_DIR}/src/game/Tools
    ${CMAKE_SOURCE_DIR}/src/game/References
    ${CMAKE_SOURCE_DIR}/src/game/MotionGenerators
    ${CMAKE_SOURCE_DIR}/src/game/vmap
    ${CMAKE_BINARY_DIR}
    ${CMAKE_BINARY_DIR}/src/shared
    ${ACE_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

if(PLAYERBOTS)
    include_directories(
        ${CMAKE_SOURCE_DIR}/src/modules/Bots/playerbot
        ${CMAKE_SOURCE_DIR}/src/modules/Bots/ahbot
    )
endif()

if(SCRIPT_LIB_ELUNA)
    include_directories(${CMAKE_SOURCE_DIR}/src/modules/Eluna)
    include_directories(${CMAKE_SOURCE_DIR}/dep/lualib)
endif()

if(SCRIPT_LIB_SD3)
    include_directories(${CMAKE_SOURCE_DIR}/src/modules/SD3)
endif()

if(SOAP)
    include_directories(${CMAKE_SOURCE_DIR}/dep/gsoap)
endif()

add_executable(${EXECUTABLE_NAME} ${EXECUTABLE_SRCS})

target_link_libraries(${EXECUTABLE_NAME}
    game
    shared
    framework
    g3dlite
    ${OPENSSL_LIBRARIES}
)

if(NOT ACE_USE_EXTERNAL)
    add_dependencies(${EXECUTABLE_NAME} ace)
    target_link_libraries(${EXECUTABLE_NAME} ace)
else()
    target_link_libraries(${EXECUTABLE_NAME} ACE)
endif()

if(POSTGRESQL)
    include_directories(${PostgreSQL_INCLUDE_DIR})
    target_link_libraries(${EXECUTABLE_NAME} ${PostgreSQL_LIBRARIES})
else()
    include_directories(${MYSQL_INCLUDE_DIR})
    target_link_libraries(${EXECUTABLE_NAME} ${MYSQL_LIBRARY})
endif()

add_dependencies(${EXECUTABLE_NAME} game)

if(SOAP)
    target_link_libraries(${EXECUTABLE_NAME} gsoap)
endif()

if(WIN32)
    target_link_libraries(${EXECUTABLE_NAME} zlib)
endif()

if(UNIX)
    target_link_libraries(${EXECUTABLE_NAME}
        ${ZLIB_LIBRARIES}
    )
endif()

set(EXECUTABLE_LINK_FLAGS "")

if(UNIX)
    if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
        set(EXECUTABLE_LINK_FLAGS "-Wl,--no-as-needed -pthread -lrt ${EXECUTABLE_LINK_FLAGS}")
    elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
        set(EXECUTABLE_LINK_FLAGS "-Wl,--no-as-needed -ldl -pthread -lrt ${EXECUTABLE_LINK_FLAGS}")
    endif()
endif()

if(APPLE)
    set(EXECUTABLE_LINK_FLAGS "-framework Carbon ${EXECUTABLE_LINK_FLAGS}")
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES LINK_FLAGS "${EXECUTABLE_LINK_FLAGS}")

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${BIN_DIR}")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/mangosd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/mangosd.conf.dist DESTINATION ${CONF_INSTALL_DIR})

if(WIN32 AND MSVC)
    install(FILES $<TARGET_PDB_FILE:${EXECUTABLE_NAME}> DESTINATION ${BIN_DIR} OPTIONAL)
endif()
