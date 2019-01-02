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

set(LIBRARY_NAME shared)

file(GLOB SRC_GRP_AUTH Auth/*.cpp Auth/*.h)
source_group("Auth" FILES ${SRC_GRP_AUTH})

file(GLOB SRC_GRP_COMMON Common/*.cpp Common/*.h)
source_group("Common" FILES ${SRC_GRP_COMMON})

file(GLOB SRC_GRP_CONFIG Config/*.cpp Config/*.h)
source_group("Config" FILES ${SRC_GRP_CONFIG})

file(GLOB SRC_GRP_DATABASE Database/*.cpp Database/*.h)
source_group("Database" FILES ${SRC_GRP_DATABASE})

file(GLOB SRC_GRP_DATASTORE DataStores/*.cpp DataStores/*.h)
source_group("DataStores" FILES ${SRC_GRP_DATASTORE})

file(GLOB SRC_GRP_LOCKQ LockedQueue/*.cpp LockedQueue/*.h)
source_group("LockedQueue" FILES ${SRC_GRP_LOCKQ})

file(GLOB SRC_GRP_LOG Log/*.cpp Log/*.h)
source_group("Log" FILES ${SRC_GRP_LOG})

file(GLOB SRC_GRP_THREAD Threading/*.cpp Threading/*.h)
source_group("Threading" FILES ${SRC_GRP_THREAD})

file(GLOB SRC_GRP_UTIL Utilities/*.cpp Utilities/*.h)
source_group("Utilities" FILES ${SRC_GRP_UTIL})

set(LIBRARY_SRCS
    ${SRC_GRP_AUTH}
    ${SRC_GRP_COMMON}
    ${SRC_GRP_CONFIG}
    ${SRC_GRP_DATABASE}
    ${SRC_GRP_DATASTORE}
    ${SRC_GRP_LOCKQ}
    ${SRC_GRP_LOG}
    ${SRC_GRP_THREAD}
    ${SRC_GRP_UTIL}
    revision.h
)

# OS specific files
if(WIN32)
    file(GLOB SRC_GRP_WIN Win/*.cpp Win/*.h)
    LIST(APPEND LIBRARY_SRCS ${SRC_GRP_WIN})
else()
    file(GLOB SRC_GRP_LINUX Linux/*.cpp Linux/*.h)
    LIST(APPEND LIBRARY_SRCS ${SRC_GRP_LINUX})
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/dep
    ${CMAKE_SOURCE_DIR}/src/framework
    ${CMAKE_BINARY_DIR}
    ${ACE_INCLUDE_DIR}
    ${OPENSSL_INCLUDE_DIR}
)

if(POSTGRESQL)
    include_directories(${PostgreSQL_INCLUDE_DIRS})
else()
    include_directories(${MYSQL_INCLUDE_DIR})
endif()

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/SystemConfig.h.in" "${CMAKE_CURRENT_BINARY_DIR}/SystemConfig.h")

add_library(${LIBRARY_NAME} STATIC ${LIBRARY_SRCS})

if(POSTGRESQL)
    target_link_libraries(${LIBRARY_NAME} ${PostgreSQL_LIBRARIES})
else()
    target_link_libraries(${LIBRARY_NAME} ${MYSQL_LIBRARY})
endif()

if(NOT ACE_USE_EXTERNAL)
    add_dependencies(${LIBRARY_NAME} ace)
endif()