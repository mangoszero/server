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

project(MaNGOS)
set(MANGOS_VERSION 0.21)

# CMake policies
cmake_minimum_required(VERSION 2.8)
# Until CMake 3.0 is the standard
# And a solution to set_directory_properties is found.
if(POLICY CMP0043)
    cmake_policy(SET CMP0043 OLD)
endif()

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${LIBS_DIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Set the correct macro directory path
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Define available cmake options below
if(WIN32)
    set(CONF_DIR "" CACHE STRING "Path to the configs, can be absolute or relative.")
else()
    set(CONF_DIR "etc/" CACHE STRING "Path to the configs, can be absolute or relative.")
endif()
option(DEBUG                "Debug mode (strict compile, all warnings)"     OFF)
option(ACE_USE_EXTERNAL     "Use external ACE"                              OFF)
option(POSTGRESQL           "Use PostgreSQL instead of MySQL"               OFF)
option(BUILD_TOOLS          "Build the map/vmap/mmap extractors"            ON)
option(SCRIPT_LIB_ELUNA     "Compile with support for Eluna scripts"        ON)
option(SCRIPT_LIB_SD2       "Compile with support for ScriptDev2 scripts"   ON)
option(PLAYERBOTS           "Enable Player Bots"                            OFF)
option(SOAP                 "Enable remote access via SOAP"                 OFF)
# Hidden option to enable/disable PCH. DEV ONLY!
set(PCH ON)

# Print CLI helper message
message("")
message(
  "This script builds the MaNGOS server.
  Options that can be used in order to configure the process:
   General:
    CMAKE_INSTALL_PREFIX    Path where the server should be installed to
    CONF_DIR                Path to the configs, can be absolute or relative.
    DEBUG                   Debug mode (strict compile, all warnings)
    ACE_USE_EXTERNAL        Use external ACE
    BUILD_TOOLS             Build the map/vmap/mmap extractors
    SOAP                    Enable remote access via SOAP
   Scripting engines:
    SCRIPT_LIB_ELUNA        Compile with support for Eluna scripts
    SCRIPT_LIB_SD2          Compile with support for ScriptDev2 scripts
   Modules:
    PLAYERBOTS              Enable Player Bots
  To set an option simply type -D<OPTION>=<VALUE> after 'cmake <srcs>'.
  Also, you can specify the generator with -G. see 'cmake --help' for more details
  For example: cmake .. -DDEBUG=1 -DCMAKE_INSTALL_PREFIX=/opt/mangos"
  
  Note: On UNIX systems, CONF_DIR is relative to the bin folder.
)
message("")

# Search for and set up the needed packages
set(OPENSSL_EXPECTED_VERSION 1.0.0)

find_package(Platform REQUIRED)
find_package(Git)
find_package(PCHSupport)
find_package(OpenSSL REQUIRED)

# For Unix systems set the rpath so that libraries are found
set(CMAKE_INSTALL_RPATH "${LIBS_DIR}")
set(CMAKE_INSTALL_NAME_DIR "${LIBS_DIR}")
# Run out of build tree
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

if(ACE_USE_EXTERNAL)
    find_package(ACE)
    if(NOT ACE_FOUND)
        message(FATAL_ERROR
            "This project requires ACE installed when ACE_USE_EXTERNAL is set. Please download the ACE Micro Release Kit from http://download.dre.vanderbilt.edu/ and install it. If this script didn't find ACE and it was correctly installed please set ACE_ROOT to the correct path."
        )
    endif()
    if(EXISTS ${ACE_INCLUDE_DIR}/ace/Stack_Trace.h)
        add_definitions(-DHAVE_ACE_STACK_TRACE_H)
    endif()
else()
    include(cmake/ImportACE.cmake)
endif()

if(POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
else()
    find_package(MySQL REQUIRED)
endif()

if(UNIX)
    find_package(ZLIB)
    find_package(BZip2)
endif()

#Include platform/compiler-specific definitions
include(${CMAKE_SOURCE_DIR}/cmake/SetDefinitions.cmake)

# Generate revision-extractor
set(GENREV_SRC src/tools/genrevision/genrevision.cpp)
add_executable(genrev ${GENREV_SRC})

# Find core revision
if(GIT_EXECUTABLE)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        OUTPUT_VARIABLE GIT_REVISION
        RESULT_VARIABLE GIT_RESULT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(GIT_RESULT)
        set(GIT_REVISION "Git repository not found")
    endif()
else()
    set(GIT_REVISION "Git not found")
endif()

message(STATUS "MaNGOS-Core revision  : ${GIT_REVISION}")
message(STATUS "Install server to     : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Install configs to    : ${CONF_INSTALL_DIR}")
if("${CONF_DIR}" STREQUAL "")
    message(STATUS "Search configs from   : binary directory (default)")
else()
    message(STATUS "Search configs from   : ${CONF_DIR}")
endif()

if(SOAP)
    message(STATUS "Support for SOAP      : Yes")
    add_definitions(-DENABLE_SOAP)
else()
    message(STATUS "Support for SOAP      : No (default)")
endif()

if(SCRIPT_LIB_ELUNA)
    message(STATUS "Script engine Eluna   : Yes (default)")
    add_definitions(-DENABLE_ELUNA)
else()
    message(STATUS "Script engine Eluna   : No")
endif()

if(SCRIPT_LIB_SD2)
    message(STATUS "Script engine SD2     : Yes (default)")
    add_definitions(-DENABLE_SD2)
else()
    message(STATUS "Script engine SD2     : No")
endif()

if(PLAYERBOTS)
    message(STATUS "Enable Player Bots    : Yes (default)")
    add_definitions(-DENABLE_PLAYERBOTS)
else()
    message(STATUS "Enable Player Bots    : No")
endif()

if(BUILD_TOOLS)
    message(STATUS "Build tools           : Yes (default)")
else()
    message(STATUS "Build tools           : No")
endif()

if(DEBUG)
    set(CMAKE_BUILD_TYPE Debug)
    message(STATUS "Build in debug-mode   : Yes")
else()
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "Build in debug-mode   : No  (default)")
endif()

# Add dependency path
add_subdirectory(dep)

# Add source path
add_subdirectory(src)
