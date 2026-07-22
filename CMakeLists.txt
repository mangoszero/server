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

cmake_minimum_required(VERSION 3.18 FATAL_ERROR)

# =============================================================================
# Project setup
# =============================================================================
project(MaNGOS VERSION 0.22.0 LANGUAGES C CXX)

# Use modern CMake policies
cmake_policy(SET CMP0028 NEW)
cmake_policy(SET CMP0048 NEW)
cmake_policy(SET CMP0054 NEW)
cmake_policy(SET CMP0063 NEW)
# CMP0042 = MACOSX_RPATH (macOS only) — optional
if(APPLE)
  cmake_policy(SET CMP0042 NEW)
endif()

# =============================================================================
# C++ / C standard configuration
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# IDE folder organization
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Always export compile_commands.json for tooling
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
option(WITHOUT_GIT          "Disable Git revision detection"                OFF)
option(BUILD_AH_SERVICE     "Build the out-of-process auction-house service" ON)
option(BUILD_TESTING        "Build focused regression tests"                OFF)
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
    WITHOUT_GIT             Disable Git revision detection
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

# =============================================================================
# Build type logic
# =============================================================================
if(NOT CMAKE_CONFIGURATION_TYPES)
    if(DEBUG)
        set(CMAKE_BUILD_TYPE Debug CACHE STRING "" FORCE)
    else()
        set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
    endif()
endif()

# =============================================================================
# Install paths
# =============================================================================
include(GNUInstallDirs)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}_install CACHE PATH "Default install directory" FORCE)
endif()

if(UNIX)
    set(BIN_DIR "${CMAKE_INSTALL_FULL_BINDIR}")
    set(CONF_INSTALL_DIR "${CMAKE_INSTALL_FULL_SYSCONFDIR}")
else()
    set(BIN_DIR "${CMAKE_INSTALL_PREFIX}")
    set(CONF_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}")
endif()

# =============================================================================
# Runtime library search path (RPATH)
# =============================================================================
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path/../lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN/../lib")
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()

# =============================================================================
# Dependencies
# =============================================================================

# =============================================================================
# Include project CMake modules safely (relative to current dir)
# =============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

if(NOT WITHOUT_GIT)
  find_package(Git QUIET)
endif()

find_package(Threads REQUIRED)
find_package(OpenSSL 3.0 REQUIRED)
# OpenSSL 4.x support is intentionally deferred until the OpenSSL 4.2 LTS migration.
if(OPENSSL_VERSION VERSION_GREATER_EQUAL "4.0.0")
  message(FATAL_ERROR
    "Unsupported OpenSSL ${OPENSSL_VERSION}: MaNGOS currently requires >=3.0.0 and <4.0.0")
endif()
find_package(MySQL REQUIRED)

# =============================================================================
# OpenSSL policy
#
# OpenSSL 3.x is the baseline: there are no version conditionals left in src/.
# Every OpenSSL decision lives here, in two interface targets:
#
#   mangos_openssl        - what to link against and where the headers live.
#                           Linked PUBLIC by anything exposing OpenSSL types in
#                           its own headers (`shared`), so consumers inherit it.
#
#   mangos_openssl_strict - the API level we hold ourselves to. Together these
#                           two definitions delete the *declarations* of
#                           everything OpenSSL deprecated up to 3.0 -- the
#                           low-level MD5_*, SHA1_*, HMAC_CTX and SSLeay_*
#                           families. Reaching for the old API is then a
#                           compile error rather than a warning nobody reads.
#
#                           Link it PRIVATE, always: it must not leak into the
#                           vendored code under dep/, which still uses the old API.
# =============================================================================
add_library(mangos_openssl INTERFACE)
target_link_libraries(mangos_openssl
    INTERFACE
        OpenSSL::SSL
        OpenSSL::Crypto
)

add_library(mangos_openssl_strict INTERFACE)
target_compile_definitions(mangos_openssl_strict
    INTERFACE
        OPENSSL_API_COMPAT=0x30000000L
        OPENSSL_NO_DEPRECATED
)

find_package(ZLIB QUIET)
find_package(BZip2 QUIET)

# dep/lualib (the Lua build used by Eluna) links against readline by its bare
# library name and is not itself touched here; find_package(Readline) below
# declares a GLOBAL IMPORTED target literally named `readline` (see
# cmake/FindReadline.cmake) before that subdirectory is added, so its
# existing bare-name references resolve to a properly-detected library
# instead of an unqualified linker search. Readline has no Windows build and
# is only needed when Eluna is being built.
if(NOT WIN32 AND SCRIPT_LIB_ELUNA)
  find_package(Readline REQUIRED)
endif()

include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/GenRevision.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/EnsureVersion.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/MangosParams.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/SetDefinitions.cmake)

if(PCH)
    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/PCHSupport.cmake)
endif()

# =============================================================================
# Subdirectories
# =============================================================================
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/dep/CMakeLists.txt")
  add_subdirectory(dep)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/CMakeLists.txt")
  add_subdirectory(src)
else()
  message(WARNING "Source directory 'src' not found. Nothing to build.")
endif()

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

# =============================================================================
# Final info and summary
# =============================================================================
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/StatusInfo.cmake)
