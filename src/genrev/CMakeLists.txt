# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
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
# revision_data.h -- the git revision baked into the binaries.
#
# The header is produced at configure time by cmake/GenRevision.cmake, which the
# top-level CMakeLists.txt includes. This target exists only to refresh it when
# the repository moves underneath an already-configured build tree.
#
# What it replaces:
#
#   add_custom_target(revision_data.h ALL COMMAND cmake -P GenRevision.cmake)
#
# That ran on *every* build -- forking a second CMake process and half a dozen
# git processes each time -- to recompute a value that changes only when someone
# commits. Two further consequences, both silent:
#
#   - In `cmake -P` script mode there is no cache, so the guards inside
#     GenRevision.cmake (which compare against CACHE INTERNAL variables) were
#     never satisfied and the whole body ran every time.
#   - In script mode CMAKE_CURRENT_BINARY_DIR is the working directory, which
#     that target pointed at the SOURCE tree, so every build wrote a generated
#     header into the checkout. The .gitignore entry for it was the symptom
#     being managed instead of the cause being fixed.
#
# DEPENDS on .git/HEAD and .git/index is what makes this cheap: HEAD moves on
# checkout and commit, index moves on staging, so the regeneration fires exactly
# when the revision can have changed and never otherwise.
# =============================================================================

if(WITHOUT_GIT OR NOT IS_DIRECTORY "${CMAKE_SOURCE_DIR}/.git")
    return()
endif()

set(REVISION_HEADER "${CMAKE_BINARY_DIR}/src/shared/revision_data.h")

set(GIT_STATE_FILES)
foreach(state HEAD index)
    if(EXISTS "${CMAKE_SOURCE_DIR}/.git/${state}")
        list(APPEND GIT_STATE_FILES "${CMAKE_SOURCE_DIR}/.git/${state}")
    endif()
endforeach()

if(NOT GIT_STATE_FILES)
    return()
endif()

add_custom_command(
    OUTPUT "${REVISION_HEADER}"
    COMMAND "${CMAKE_COMMAND}"
            -DBUILDDIR=${CMAKE_BINARY_DIR}
            -DCMAKE_SOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DSCRIPT_LIB_ELUNA=${SCRIPT_LIB_ELUNA}
            -DSCRIPT_LIB_SD3=${SCRIPT_LIB_SD3}
            -DGIT_EXECUTABLE=${GIT_EXECUTABLE}
            -P "${CMAKE_SOURCE_DIR}/cmake/GenRevision.cmake"
    DEPENDS ${GIT_STATE_FILES}
            "${CMAKE_SOURCE_DIR}/src/shared/revision_data.h.in"
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    COMMENT "Refreshing revision_data.h (repository HEAD moved)"
    VERBATIM
)

add_custom_target(revision_data ALL DEPENDS "${REVISION_HEADER}")
