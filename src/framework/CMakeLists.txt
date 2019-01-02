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

set(LIBRARY_NAME framework)

file(GLOB SRC_GRP_DYNAMIC Dynamic/*.h)
source_group("Dynamic" FILES ${SRC_GRP_DYNAMIC})

file(GLOB SRC_GRP_GAMESYSTEM GameSystem/*.h)
source_group("GameSystem" FILES ${SRC_GRP_GAMESYSTEM})

file(GLOB SRC_GRP_PLATFORM Platform/*.h)
source_group("Platform" FILES ${SRC_GRP_PLATFORM})

file(GLOB SRC_GRP_POLICIES Policies/*.h Policies/*.cpp)
source_group("Policies" FILES ${SRC_GRP_POLICIES})

file(GLOB SRC_GRP_UTILITIES Utilities/*.h Utilities/*.cpp)
source_group("Utilities" FILES ${SRC_GRP_UTILITIES})

file(GLOB SRC_GRP_LINKED_REFERENCE Utilities/LinkedReference/*.h)
source_group("Utilities\\LinkedReference" FILES ${SRC_GRP_LINKED_REFERENCE})

set(LIBRARY_SRCS
    ${SRC_GRP_DYNAMIC}
    ${SRC_GRP_GAMESYSTEM}
    ${SRC_GRP_PLATFORM}
    ${SRC_GRP_POLICIES}
    ${SRC_GRP_UTILITIES}
    ${SRC_GRP_LINKED_REFERENCE}
)

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${ACE_INCLUDE_DIR}
)

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SRCS})

if(NOT ACE_USE_EXTERNAL)
    add_dependencies(${LIBRARY_NAME} ace)
endif()