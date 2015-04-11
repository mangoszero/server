# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2015  MaNGOS project <http://getmangos.eu>
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

project(MANGOS_MAP_EXTRACTOR)

set(EXECUTABLE_NAME "map-extractor")

if(WIN32)
    # add resource file to windows build
    set(EXECUTABLE_SRCS ${EXECUTABLE_SRCS} map-extractor.rc)
endif()

include_directories(
    ${CMAKE_SOURCE_DIR}/dep/libmpq
    ${CMAKE_SOURCE_DIR}/dep/loadlib
)

add_executable(${EXECUTABLE_NAME} dbcfile.cpp System.cpp ${EXECUTABLE_SRCS})

target_link_libraries(${EXECUTABLE_NAME} libmpq loadlib)

#Output the compiled exes to build/bin/$(Configuration)/tools directory on windows by default
if(WIN32)
    if ( MSVC )
        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release/tools
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug/tools
        )
    elseif ( MINGW )
        set_target_properties(${EXECUTABLE_NAME} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/tools
            RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/tools
        )
    endif()
endif()

install(TARGETS ${EXECUTABLE_NAME} DESTINATION "${BIN_DIR}/${TOOLS_DIR}")
if(WIN32 AND MSVC)
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/\${BUILD_TYPE}/${EXECUTABLE_NAME}.pdb" DESTINATION "${BIN_DIR}/${TOOLS_DIR}" CONFIGURATIONS Debug)
endif()

#install documentation and generation scripts
install(
    FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries/ExtractResources.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries/make_vmaps.bat"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries/mmap_excluded.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries/MoveMapGen.sh"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries/offmesh.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries/README.txt"
    DESTINATION
        "${BIN_DIR}/${TOOLS_DIR}"
)

if( WIN32 )
    if ( MSVC )
        add_custom_command(TARGET ${EXECUTABLE_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/$(ConfigurationName)/tools/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries" "${CMAKE_BINARY_DIR}/bin/$(ConfigurationName)/tools/"
        )
    elseif ( MINGW )
        add_custom_command(TARGET ${EXECUTABLE_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/bin/tools/"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/../Extractor_Binaries" "${CMAKE_BINARY_DIR}/bin/tools/"
        )
    endif()
endif()