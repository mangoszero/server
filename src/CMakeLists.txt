# SPDX-License-Identifier: GPL-3.0-or-later
#
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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

# Geometry is header-only and sits below everything else: it knows nothing
# about WoW, any client version, or the server.
add_subdirectory(shared/Geometry)
add_subdirectory(shared/terrain)

# Build the mangos shared library
add_subdirectory(shared)

add_subdirectory(proto)

# IPC substrate (always built; ah-service and mangosd both link it)
add_subdirectory(ipc)

add_subdirectory(genrev)

# Needs to link against mangos_world.lib
if(BUILD_MANGOSD)
    # Build the mangos game library
    add_subdirectory(game)
endif()

if(BUILD_MANGOSD)
    # Build the mangos world server
    add_subdirectory(mangosd)
endif()

# The modules tree contains BOTH the game-linked scripts (SD3/Eluna, which
# self-gate on SCRIPT_LIB_* AND require the `game` target) and the standalone
# AhWorker (which links only ah_ipc + shared). Add it when EITHER mangosd or the
# worker is being built; the SD3/Eluna add_subdirectory calls themselves stay
# guarded so they never reference `game` when BUILD_MANGOSD is OFF.
if(BUILD_MANGOSD OR BUILD_AH_SERVICE)
    add_subdirectory(modules)
endif()

# Build the mangos realm authentication server
if(BUILD_REALMD)
    add_subdirectory(realmd)

    # realmd is a SUBMODULE and is never edited here. Its own test executables
    # compile Auth/PatchArtifact.cpp, which includes Auth/Md5.h -- a header this
    # core owns, under src/shared. The realmd library reaches it by linking
    # `shared`; the test targets link neither, so the path is attached from the
    # outside, which is the only place a submodule may be adapted from.
    foreach(realmd_test IN ITEMS
            realmd_patch_safety_tests
            realmd_auth_protocol_tests
            realmd_realm_snapshot_tests
            realmd_client_build_policy_tests)
        if(TARGET ${realmd_test})
            target_include_directories(${realmd_test} PRIVATE
                ${CMAKE_SOURCE_DIR}/src/shared)
        endif()
    endforeach()
endif()

# The baker and its client-data parsers, configured unconditionally so the unit
# tests can link the parsers without asking for the tools. BUILD_TOOLS no longer
# gates it: the tiles it produces are what src/shared/terrain reads at runtime,
# so it is not optional the way the old map/vmap extractors were.
add_subdirectory(tools/extractor)

# Scores those tiles against the world database's own ground-only creature spawns.
# A clean parse is not correct geometry; this is what says whether it is.
add_subdirectory(tools/height-check)

if (BUILD_MANGOSD OR BUILD_REALMD)
    if(WIN32)
        get_filename_component(MYSQL_LIB_DIR ${MySQL_LIBRARIES} DIRECTORY)
        get_filename_component(MYSQL_LIB_DLL ${MySQL_LIBRARIES} NAME)
        STRING(REPLACE ".lib" ".dll" MYSQL_LIB_DLL ${MYSQL_LIB_DLL})
        install(FILES ${MYSQL_LIB_DIR}/${MYSQL_LIB_DLL} DESTINATION ${BIN_DIR})
    endif()
endif()

if(WITH_TESTS OR WITH_NET_TESTS)
    add_subdirectory(tests)
endif()
