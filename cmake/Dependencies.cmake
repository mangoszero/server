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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# =============================================================================
# Which vendored dependencies this fork builds.
#
# `dep` is a submodule and is never modified here, so the choice cannot live in
# its own CMakeLists. Adding the subdirectories from the outside is how the
# selection is attached without touching the submodule -- the same mechanism as
# cmake/SubmoduleCompat.cmake.
#
# It is also what decides the GATING. The submodule builds StormLib only when
# BUILD_TOOLS is on, which was true when the extractor was an optional extra; it
# is not, because the extractor is what produces the tiles the server reads. A
# core without this file inherits the submodule's answer and fails to compile the
# MPQ reader.
#
# Add a dependency here rather than in dep/CMakeLists.txt. Anything left out is
# simply never configured, so a library the fork has dropped costs no build time
# and cannot be linked back in by accident.
# =============================================================================

set(MANGOS_DEP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/dep")

if(NOT EXISTS "${MANGOS_DEP_DIR}")
    message(FATAL_ERROR
        "The 'dep' submodule is missing. Clone recursively, or run "
        "'git submodule update --init --recursive'.")
endif()

if(NOT TARGET "ZLIB::ZLIB")
    add_subdirectory(${MANGOS_DEP_DIR}/zlib dep/zlib)
endif()

if(NOT TARGET "BZip2::BZip2")
    add_subdirectory(${MANGOS_DEP_DIR}/bzip2 dep/bzip2)
endif()

add_subdirectory(${MANGOS_DEP_DIR}/utf8cpp dep/utf8cpp)

if(BUILD_MANGOSD)
    if(SCRIPT_LIB_ELUNA)
        add_subdirectory(${MANGOS_DEP_DIR}/lualib dep/lualib)
    endif()
    if(SOAP)
        add_subdirectory(${MANGOS_DEP_DIR}/gsoap dep/gsoap)
    endif()
endif()

# Recast and Detour are needed by the server's pathfinder AND by the baker's navmesh
# stage, and the baker always builds, so this is not gated either.
add_subdirectory(${MANGOS_DEP_DIR}/recastnavigation dep/recastnavigation)

# The MPQ reader is not gated: mangos-extractor is what produces the tiles the server
# reads, so it is always built rather than being an optional extra that a fresh clone
# can leave out and then have nothing to run on.
add_subdirectory(${MANGOS_DEP_DIR}/StormLib dep/StormLib)
