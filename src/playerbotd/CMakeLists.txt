#/**
# * MaNGOS is a full featured server for World of Warcraft, supporting
# * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
# *
# * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
# *
# * This program is free software; you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation; either version 2 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this program; if not, write to the Free Software
# * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
# *
# * World of Warcraft, and all World of Warcraft or Warcraft art, images,
# * and lore are copyrighted by Blizzard Entertainment, Inc.
# */

set(SRC_GRP_MAIN
    playerbotd.cpp
)
source_group("Main" FILES ${SRC_GRP_MAIN})

# Reuse thread infrastructure from mangosd
set(MANGOSD_DIR ${CMAKE_SOURCE_DIR}/src/mangosd)
set(SRC_GRP_THREADS
    ${MANGOSD_DIR}/AFThread.cpp
    ${MANGOSD_DIR}/AFThread.h
    ${MANGOSD_DIR}/CliThread.cpp
    ${MANGOSD_DIR}/CliThread.h
    ${MANGOSD_DIR}/RAThread.cpp
    ${MANGOSD_DIR}/RAThread.h
    ${MANGOSD_DIR}/WorldThread.cpp
    ${MANGOSD_DIR}/WorldThread.h
)
source_group("Threads" FILES ${SRC_GRP_THREADS})

if(WIN32)
    set(PRODUCT_NAME "playerbotd.exe")
    set(PRODUCT_ICON ${CMAKE_SOURCE_DIR}/src/mangosd/mangosd.ico)
    set(PRODUCT_DESCRIPTION "${MANGOS_PKG} Playerbot Daemon")
    configure_file(${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.h.in ${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.h)
endif()

add_executable(playerbotd
    ${SRC_GRP_MAIN}
    ${SRC_GRP_THREADS}
    $<$<BOOL:${WIN32}>:
        ${CMAKE_CURRENT_BINARY_DIR}/VersionInfo.h
        ${CMAKE_SOURCE_DIR}/cmake/win/VersionInfo.rc
    >
)

target_include_directories(playerbotd
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_SOURCE_DIR}/src/mangosd
)

target_compile_definitions(playerbotd
    PRIVATE
        $<$<BOOL:${SCRIPT_LIB_ELUNA}>:ENABLE_ELUNA ELUNA_EXPANSION=0 ELUNA_MANGOS>
)

target_link_libraries(playerbotd
    PRIVATE
        Bots
        Threads::Threads
        OpenSSL::Crypto
        OpenSSL::SSL
)

install(
    TARGETS playerbotd
    DESTINATION ${BIN_DIR}
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/playerbotd.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/playerbotd.conf.dist)
install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/playerbotd.conf.dist
    DESTINATION ${CONF_INSTALL_DIR}
)

if(WIN32 AND MSVC)
    install(
        FILES $<TARGET_PDB_FILE:playerbotd>
        DESTINATION ${BIN_DIR}
        OPTIONAL
    )
endif()
