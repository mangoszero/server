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

# CMake policies
cmake_minimum_required(VERSION 2.8)
# Until CMake 3.0 is the standard
# And a solution to set_directory_properties is found.
if(POLICY CMP0043)
    cmake_policy(SET CMP0043 OLD)
endif()

project(MaNGOS)
set(MANGOS_VERSION 0.20)

set(CMAKE_MODULE_PATH
  "${CMAKE_MODULE_PATH}"
  "${CMAKE_SOURCE_DIR}/cmake"
)

# define all options here
if(WIN32)
  set(CONF_DIR "" CACHE STRING "Config path. Can be absolute or relative")
else()
  set(CONF_DIR "etc/" CACHE STRING "Config path. Can be absolute or relative")
endif()
option(DEBUG                "Debug mode"                             OFF)
option(USE_STD_MALLOC       "Use standard malloc instead of TBB"     ON)
option(ACE_USE_EXTERNAL     "Use external ACE"                       OFF)
option(POSTGRESQL           "Use PostgreSQL"                         OFF)
option(BUILD_TOOLS          "Build tools (map/vmap/mmap extractors)" ON)
option(SCRIPT_LIB_ELUNA     "Use Eluna as the scripting engine"      ON)
option(SCRIPT_LIB_SD2       "Use ScriptDev2 as the scripting engine" ON)
option(SOAP                 "Enable access via SOAP or not"          OFF)
option(PCH                  "Use precompiled headers"                ON)
# TODO: options that should be checked/created:
#option(CLI                  "With CLI"                              ON)
#option(RA                   "With Remote Access"                    OFF)
#option(SQL                  "Copy SQL files"                        OFF)

# Output description of this script
message("")
message(
  "This script builds the MaNGOS server.
  Options that can be used in order to configure the process:
   General:
    CMAKE_INSTALL_PREFIX    Path where the server should be installed to
    CONF_DIR                Path to the configs, can be absolute or relative.
    PCH                     Use precompiled headers
    DEBUG                   Debug mode
    USE_STD_MALLOC          Use standard malloc instead of TBB
    ACE_USE_EXTERNAL        Use external ACE
    BUILD_TOOLS             Build map/vmap/mmap extractors
    SOAP                    Enable remote access via SOAP
   Scripting engines:
    SCRIPT_LIB_ELUNA        Compile with support for Eluna scripts
    SCRIPT_LIB_SD2          Compile with support for ScriptDev2 scripts

  To set an option simply type -D<OPTION>=<VALUE> after 'cmake <srcs>'.
  Also, you can specify the generator with -G. see 'cmake --help' for more details
  For example: cmake .. -DDEBUG=1 -DCMAKE_INSTALL_PREFIX=/opt/mangos"
  
  Note: On UNIX systems, CONF_DIR is relative to the bin folder.
)
message("")

# Force out-of-source build
if(${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
  message(FATAL_ERROR
    "This project requires an out of source build. Remove the file 'CMakeCache.txt' found in this directory before continuing, create a separate build directory and run 'cmake [options] <srcs>' from there."
  )
endif()

# TODO: allow other compilers under windows in the future
if(WIN32 AND NOT MSVC)
  message(FATAL_ERROR
    "Under Windows other compiler than Microsoft Visual Studio are not supported."
  )
endif()

# TODO: remove this in the future! it has only been added to make the switch easier for end users
if(PREFIX)
  message(FATAL_ERROR "The parameter PREFIX has been removed. Please re-run CMake and use CMAKE_INSTALL_PREFIX instead to define your installation location!")
endif()

# TODO: use MSVC_CXX_ARCHITECTURE_ID instead to identify platform on windows (not required on other systems)
# find platform: required to build 3rd party libraries w/o CMake files
find_package(Platform REQUIRED)
# Find out what system we use to include the needed libs
if(WIN32)
  if(PLATFORM MATCHES X86) # 32-bit
    set(DEP_ARCH win32)
  else() # 64-bit
    set(DEP_ARCH x64)
  endif()
endif()

# Used by map-extractor for now
if(UNIX)
  find_package(BZip2 REQUIRED)
endif()

# find Git: used to get the revision number
find_package(Git)

# check if the platform supports precomiled headers
find_package(PCHSupport)

# TODO: remove this (refactor ACE & TBB CMake code)
# VS100 uses MSBuild.exe instead of devenv.com, so force it to use devenv.com
if(WIN32 AND MSVC_VERSION MATCHES 1600)
  find_package(VisualStudio2010)
endif()

# Override configuration-types - we don't use anything else than debug and release
if(CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_CONFIGURATION_TYPES Release Debug)
  set(CMAKE_CONFIGURATION_TYPES "${CMAKE_CONFIGURATION_TYPES}" CACHE STRING
    "Reset the configurations to what we need"
    FORCE)
endif()

#Force set the default install path if it is default
if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}/bin/" CACHE PATH "MaNGOS default install prefix" FORCE)
endif(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)

#Output the compiled exes and DLLs to build/bin directory on windows by default
if(WIN32)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

#Set config install path correctly from given path
string(FIND "${CONF_DIR}" ":" CONF_DIR_ABSOLUTE)
if(${CONF_DIR_ABSOLUTE} EQUAL -1)
  #Path was not absolute
  set(CONF_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/${CONF_DIR}")
  if(MSVC)
    set(CONF_COPY_DIR "${CMAKE_BINARY_DIR}/bin/$(ConfigurationName)/${CONF_DIR}")
  endif()
else()
  #Path was absolute
  set(CONF_INSTALL_DIR "${CONF_DIR}")
  if(MSVC)
    set(CONF_COPY_DIR "${CONF_DIR}")
  endif()
endif()

# If win32 put binaries in root folder, else bin.
if(WIN32)
  set(BIN_DIR ${CMAKE_INSTALL_PREFIX}/)
else()
  set(BIN_DIR ${CMAKE_INSTALL_PREFIX}/bin)
endif()

# If win32 put libs in root folder, else bin.
if(WIN32)
  set(LIBS_DIR ${CMAKE_INSTALL_PREFIX}/)
else()
  set(LIBS_DIR ${CMAKE_INSTALL_PREFIX}/lib)
endif()

# For Unix systems set the rpath so that libraries are found
set(CMAKE_INSTALL_RPATH "${LIBS_DIR}")
set(CMAKE_INSTALL_NAME_DIR "${LIBS_DIR}")
# Run out of build tree
set(CMAKE_BUILD_WITH_INSTALL_RPATH OFF)

# Find needed packages and if necessery abort if something important is missing
unset(ACE_INCLUDE_DIR CACHE)
unset(ACE_LIBRARIES CACHE)
unset(ACE_LIBRARIES_DIR CACHE)
unset(ACE_INCLUDE_DIR)
unset(ACE_LIBRARIES)
unset(ACE_LIBRARIES_DIR)
if(ACE_USE_EXTERNAL)
  find_package(ACE)
  if(NOT ACE_FOUND)
    message(FATAL_ERROR
      "This project requires ACE installed when ACE_USE_EXTERNAL is set. Please download the ACE Micro Release Kit from http://download.dre.vanderbilt.edu/ and install it. If this script didn't find ACE and it was correctly installed please set ACE_ROOT to the correct path."
    )
  endif()
  if(EXISTS ${ACE_INCLUDE_DIR}/ace/Stack_Trace.h)
    set(HAVE_ACE_STACK_TRACE_H ON) # config.h.cmake
  endif()
else()
  include(cmake/ImportACE.cmake)
endif()

# Required packages to build
find_package(OpenSSL REQUIRED)

if(POSTGRESQL)
    find_package(PostgreSQL REQUIRED)
else()
    find_package(MySQL REQUIRED)
endif()

# *nix-specific packages
if(UNIX)
  find_package(ZLIB REQUIRED)
endif()

# Add uninstall script and target
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
  IMMEDIATE @ONLY
)

add_custom_target(uninstall
  "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
)

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

# if(CLI)
#   message(STATUS "Build with CLI        : Yes (default)")
#   add_definitions(-DENABLE_CLI)
# else()
#   message(STATUS "Build with CLI        : No")
# endif()

# if(RA)
#   message(STATUS "* Build with RA         : Yes")
#   add_definitions(-DENABLE_RA)
# else(RA)
#   message(STATUS "* Build with RA         : No  (default)")
# endif(RA)

if(SOAP)
  message(STATUS "Support for SOAP      : Yes")
  set(DEFINITIONS ${DEFINITIONS} -DENABLE_SOAP)
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

if(BUILD_TOOLS)
  message(STATUS "Build tools           : Yes (default)")
else()
  message(STATUS "Build tools           : No")
endif()

if(PCH)
  message(STATUS "Use PCH               : Yes (default)")
else()
  message(STATUS "Use PCH               : No")
endif()

if(DEBUG)
  set(CMAKE_BUILD_TYPE Debug)
  message(STATUS "Build in debug-mode   : Yes")
else()
  set(CMAKE_BUILD_TYPE Release)
  message(STATUS "Build in debug-mode   : No  (default)")
endif()
# Handle debugmode compiles (this will require further work for proper WIN32-setups)
if(UNIX)
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g")
endif()

# Set warning levels for different builds
if(UNIX)
  set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} --no-warnings")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} --no-warnings")
  set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Wall -Wfatal-errors -Wextra -Winvalid-pch")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wfatal-errors -Wextra -Winvalid-pch")
elseif(WIN32)
  # Disable warnings in Visual Studio 8 and above and add /MP
  if(MSVC AND NOT CMAKE_GENERATOR MATCHES "Visual Studio 7")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /wd4996 /wd4355 /wd4244 /wd4985 /wd4267 /MP")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /wd4996 /wd4355 /wd4244 /wd4267 /MP")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /wd4996 /wd4355 /wd4244 /wd4985 /wd4267 /MP")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /wd4996 /wd4355 /wd4244 /wd4985 /wd4267 /MP")
  endif()
endif()

# if(SQL)
#   message(STATUS "Install SQL-files     : Yes")
# else()
#   message(STATUS "Install SQL-files     : No  (default)")
# endif()

# Some small tweaks for Visual Studio 7 and above.
if(MSVC)
  # Mark 32 bit executables large address aware so they can use > 2GB address space
  if(PLATFORM MATCHES X86)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /LARGEADDRESSAWARE")
  endif()
endif()

set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${LIBS_DIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Generate revision-extractor
set(GENREV_SRC
  src/tools/genrevision/genrevision.cpp
)

add_executable(genrev
  ${GENREV_SRC}
)

if(WIN32)
  install(
    FILES
      "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libeay32.dll"
      "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/ssleay32.dll"
      "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libmySQL.dll"
    DESTINATION ${LIBS_DIR}
    CONFIGURATIONS Release
  )
  install(
    FILES
      "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libeay32.dll"
      "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/ssleay32.dll"
      "${CMAKE_SOURCE_DIR}/dep/lib/${DEP_ARCH}/libmySQL.dll"
    DESTINATION ${LIBS_DIR}
    CONFIGURATIONS Debug
  )
endif()

if(XCODE)
  if(PLATFORM MATCHES X86)
    set(CMAKE_OSX_ARCHITECTURES i386)
  else()
    set(CMAKE_OSX_ARCHITECTURES x86_64)
  endif()
endif()

# This is used by config.h.cmake
if (SOAP)
  set(ENABLE_SOAP ON)
else()
  set(ENABLE_SOAP OFF)
endif()

add_subdirectory(dep)

# Add definitions for all build types
# Don't place this above 'dep' subdirectory! Because of defines build will crash.
# Remember, this overwrites global preprocessors, thus SOAP, Eluna, SD2 etc. has to be defined twice.
# TODO: Clean this up?

set(DEFINITIONS
  HAVE_CONFIG_H
  MANGOS
)

if(SOAP)
  set(DEFINITIONS ${DEFINITIONS} ENABLE_SOAP)
endif()

if(SCRIPT_LIB_ELUNA)
   set(DEFINITIONS ${DEFINITIONS} ENABLE_ELUNA)
endif()

if(SCRIPT_LIB_SD2)
   set(DEFINITIONS ${DEFINITIONS} ENABLE_SD2)
endif()

if(POSTGRESQL)
  set(DEFINITIONS ${DEFINITIONS} DO_POSTGRESQL)
else()
  set(DEFINITIONS ${DEFINITIONS} DO_MYSQL)
endif()

set(DEFINITIONS_RELEASE NDEBUG)
set(DEFINITIONS_DEBUG) # _DEBUG MANGOS_DEBUG) was here..
if(WIN32)
  set(DEFINITIONS ${DEFINITIONS} WIN32 _WIN32)
  set(DEFINITIONS_RELEASE ${DEFINITIONS_RELEASE} _CRT_SECURE_NO_WARNINGS)
endif()

if(USE_STD_MALLOC)
  set(DEFINITIONS ${DEFINITIONS} USE_STANDARD_MALLOC)
endif()

# Specify the maximum PreCompiled Header memory allocation limit
# Fixes a compiler-problem when using PCH - the /Ym flag is adjusted by the compiler in MSVC2012, hence we need to set an upper limit with /Zm to avoid discrepancies)
# (And yes, this is a verified , unresolved bug with MSVC... *sigh*)
# Make it MSVC specific, otherwise g++ etc pukes
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
    string(REGEX REPLACE "/Zm[0-9]+ *" "" CMAKE_CXX_FLAGS ${CMAKE_CXX_FLAGS})
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Zm500" CACHE STRING "" FORCE)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "FreeBSD")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -stdlib=libc++")
endif()

set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "${DEFINITIONS}")
#Something like this is what i've imagined with the new generator expressions in cmake 3.0 / lfx
#set_directory_properties(PROPERTIES COMPILE_DEFINITIONS "$<$<CONFIG:Release>:"${DEFINITIONS} ${DEFINITIONS_RELEASE}">$<$<CONFIG:Debug>:"${DEFINITIONS} ${DEFINITIONS_DEBUG}">")
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS_RELEASE "${DEFINITIONS_RELEASE}")
set_directory_properties(PROPERTIES COMPILE_DEFINITIONS_DEBUG "${DEFINITIONS_DEBUG}")

add_subdirectory(src)
