# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2023 MaNGOS <https://getmangos.eu>
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

# Build the mangos script library if enabled
if(SCRIPT_LIB_SD3)
    add_subdirectory(SD3)

    add_library(SD3Intf INTERFACE)
    target_include_directories(SD3Intf
        INTERFACE
            ${CMAKE_CURRENT_SOURCE_DIR}/SD3
            ${CMAKE_CURRENT_SOURCE_DIR}/SD3/system
)
endif()

# Build the mangos script library if enabled
if(PLAYERBOTS)
    add_subdirectory(Bots)
endif()

# Build the Eluna library if enabled
if(SCRIPT_LIB_ELUNA)

file(GLOB SRC_GRP_ELUNA Eluna/*.cpp Eluna/*.h)
source_group("Eluna" FILES ${SRC_GRP_ELUNA})

add_library(ElunaIntf INTERFACE)
add_library(Eluna STATIC ${SRC_GRP_ELUNA})

target_include_directories(ElunaIntf
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/Eluna
)

target_link_libraries(ElunaIntf
    INTERFACE
        lualib
)

target_link_libraries(Eluna
    PUBLIC
        game
        lualib
)

install(
    DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Eluna/extensions
    DESTINATION ${BIN_DIR}/lua_scripts
)

endif()