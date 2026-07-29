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
# terrain -- the tile data model the offline baker writes and the server reads.
#
# It links geometry and nothing else. It must never gain a dependency on the
# database, the world, or StormLib: the baker and the runtime share these types
# precisely so the writer and the reader cannot disagree.
# =============================================================================

add_library(terrain STATIC
    Accelerators.cpp
    Accelerators.hpp
    CollisionModel.cpp
    CollisionModel.hpp
    Column.hpp
    FusedTerrain.cpp
    FusedTerrain.hpp
    Geometry.hpp
    GoModelStore.cpp
    GoModelStore.hpp
    ICollisionModel.hpp
    ILiveGeometry.hpp
    ModelTileSource.hpp
    Terrain.hpp
    TileSerializer.cpp
    TileSerializer.hpp
    WmoModel.cpp
    WmoModel.hpp
)

# Consumers spell it #include "terrain/Terrain.hpp", so the root is src/shared.
target_include_directories(terrain
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(terrain
    PUBLIC
        geometry
        Threads::Threads
)
