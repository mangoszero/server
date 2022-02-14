# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2022 MaNGOS <https://getmangos.eu>
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

cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

cmake_policy(SET CMP0028 NEW)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0042 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0063 NEW)

set(CMAKE_CXX_STANDARD          11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)

set_property(GLOBAL
    PROPERTY USE_FOLDERS ON
)

#==================================================================================
# Define available cmake options below
option(BUILD_MANGOSD        "Build the main server"                         ON)
option(BUILD_REALMD         "Build the login server"                        ON)
option(BUILD_TOOLS          "Build the map/vmap/mmap extractors"            ON)
option(USE_STORMLIB         "Use StormLib for reading MPQs"                 ON)
option(SCRIPT_LIB_ELUNA     "Compile with support for Eluna scripts"        ON)
option(SCRIPT_LIB_SD3       "Compile with support for ScriptDev3 scripts"   ON)
option(PLAYERBOTS           "Enable Player Bots"                            OFF)
option(SOAP                 "Enable remote access via SOAP"                 OFF)
option(PCH                  "Enable precompiled headers"                    ON)
option(DEBUG                "Enable debug build (only on non IDEs)"         OFF)
#==================================================================================
message("")
message(
  "This script builds the MaNGOS server.
  Options that can be used in order to configure the process:
   General:
    CMAKE_INSTALL_PREFIX    Path where the server should be installed to
    BUILD_MANGOSD           Build the main server
    BUILD_REALMD            Build the login server
    BUILD_TOOLS             Build the map/vmap/mmap extractors
    USE_STORMLIB            Use StormLib for reading MPQs
    SOAP                    Enable remote access via SOAP
    PCH                     Enable use of precompiled headers
    DEBUG                   Debug build, only for systems without IDE (Linux, *BSD)
   Scripting engines:
    SCRIPT_LIB_ELUNA        Compile with support for Eluna scripts
    SCRIPT_LIB_SD3          Compile with support for ScriptDev3 scripts
   Modules:
    PLAYERBOTS              Enable Player Bots

  To set an option simply type -D<OPTION>=<VALUE> after 'cmake <srcs>'.
  Also, you can specify the generator with -G. See 'cmake --help' for more details
  For example: cmake .. -DCMAKE_INSTALL_PREFIX=/opt/mangos -DSCRIPT_LIB_SD3=0"
)
message("")
#==================================================================================#

project(MaNGOS VERSION 0.22.0)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_BINARY_DIR}/install CACHE PATH "Default install directory" FORCE)
endif()

if(DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
else()
  set(CMAKE_BUILD_TYPE Release)
endif()

if(UNIX)
    set(BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
    if (NOT CONF_INSTALL_DIR)
        set(CONF_INSTALL_DIR ../etc)
    endif()
else()
    set(BIN_DIR ${CMAKE_INSTALL_PREFIX})
    if (NOT CONF_INSTALL_DIR)
        set(CONF_INSTALL_DIR .)
    endif()
endif()

find_package(Threads REQUIRED)
find_package(MySQL   REQUIRED)
find_package(DL      REQUIRED)

find_package(ZLIB  QUIET)
find_package(BZip2 QUIET)

find_package(OpenSSL REQUIRED)

include(${CMAKE_SOURCE_DIR}/cmake/EnsureVersion.cmake)

include(${CMAKE_SOURCE_DIR}/cmake/MangosParams.cmake)
include(${CMAKE_SOURCE_DIR}/cmake/SetDefinitions.cmake)

if(PCH)
    include(${CMAKE_SOURCE_DIR}/cmake/PCHSupport.cmake)
endif()

add_subdirectory(dep)
add_subdirectory(src)

include(${CMAKE_SOURCE_DIR}/cmake/StatusInfo.cmake)
