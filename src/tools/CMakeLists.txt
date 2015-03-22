#
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
#

#Used by vmap-assembler and mmap-generator
include_directories(
    "${CMAKE_SOURCE_DIR}/src/shared"
    "${CMAKE_SOURCE_DIR}/src/game/vmap/"
    "${CMAKE_SOURCE_DIR}/dep/include/g3dlite/"
    "${CMAKE_SOURCE_DIR}/src/framework/"
    "${ACE_INCLUDE_DIR}"
)

add_definitions(-DMMAP_GENERATOR -DNO_CORE_FUNCS)

add_library(vmap
    ${CMAKE_SOURCE_DIR}/src/game/vmap/BIH.cpp
    ${CMAKE_SOURCE_DIR}/src/game/vmap/VMapManager2.cpp
    ${CMAKE_SOURCE_DIR}/src/game/vmap/MapTree.cpp
    ${CMAKE_SOURCE_DIR}/src/game/vmap/TileAssembler.cpp
    ${CMAKE_SOURCE_DIR}/src/game/vmap/WorldModel.cpp
    ${CMAKE_SOURCE_DIR}/src/game/vmap/ModelInstance.cpp
)

target_link_libraries(vmap g3dlite zlib)

# Used for install targets in subdirs
set(TOOLS_DIR "tools")

add_subdirectory(Movemap-Generator)
add_subdirectory(map-extractor)
add_subdirectory(vmap-assembler)
add_subdirectory(vmap-extractor)
