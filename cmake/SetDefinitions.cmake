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

# =============================================================================
# Compiler definitions and options, applied to everything below this directory.
#
# This file used to end with:
#
#     set_property(DIRECTORY
#         PROPERTY COMPILE_DEFINITIONS ${DEFAULT_COMPILE_DEFS}
#         PROPERTY COMPILE_OPTIONS     ${DEFAULT_COMPILE_OPTS})
#
# set_property takes exactly one property. CMake reads the *last* PROPERTY
# keyword as the name and folds everything before it into the value list, so
# COMPILE_DEFINITIONS was never set at all and the definitions were appended to
# COMPILE_OPTIONS instead. They still reached the compiler -- /D and -D happen to
# be valid options too -- which is why nobody noticed, but the definitions
# property was dead and the spelling was locked to one compiler's flag syntax.
#
# add_compile_definitions() / add_compile_options() are the modern spellings and
# cannot be combined by accident: definitions are written without any /D or -D,
# and CMake emits the right prefix per compiler.
# =============================================================================

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(PLATFORM 64)
else()
    set(PLATFORM 32)
endif()

if(XCODE)
    if(PLATFORM EQUAL 32 AND CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(CMAKE_OSX_ARCHITECTURES ARM32)
    elseif(PLATFORM EQUAL 32)
        set(CMAKE_OSX_ARCHITECTURES i386)
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^arm")
        set(CMAKE_OSX_ARCHITECTURES ARM64)
    else()
        set(CMAKE_OSX_ARCHITECTURES x86_64)
    endif()
endif()

# -----------------------------------------------------------------------------
# Definitions
# -----------------------------------------------------------------------------
if(WIN32)
    # NOMINMAX is not optional. Without it <windows.h> defines min and max as
    # function-like macros, which then mangle every std::min / std::max in a
    # translation unit that transitively reaches a Windows header -- the failure
    # reads as "C2589: '(': illegal token on right side of '::'" and points at
    # perfectly correct code. The tree used to paper over this with #undef min /
    # #undef max buried at the bottom of Common.h; suppressing the macros at the
    # source is both smaller and reliable.
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
endif()

if(MSVC)
    add_compile_definitions(
        _CRT_SECURE_NO_WARNINGS
        _CRT_NONSTDC_NO_DEPRECATE
        _WINSOCK_DEPRECATED_NO_WARNINGS
    )
    # Dropped from the historical list:
    #   _WIN64                     - the compiler defines it itself on x64.
    #   _SCL_SECURE_NO_WARNINGS    - the checked-iterator warnings it silenced
    #                                were removed from the toolset years ago.
endif()

if(MINGW)
    add_compile_definitions(
        WINVER=0x0600
        _WIN32_WINNT=0x0600
    )
    if(PLATFORM EQUAL 32)
        add_compile_definitions(HAVE_SSE2 __SSE2__)
    endif()
endif()

# -----------------------------------------------------------------------------
# Options
# -----------------------------------------------------------------------------
if(MSVC)
    add_compile_options(
        /MP                                     # parallel compilation
        /W4
        $<$<EQUAL:${PLATFORM},32>:/arch:SSE2>
        $<$<CONFIG:Release>:/Gw>                # whole-program global data opt
        $<$<CONFIG:Release>:/GF>                # string pooling
        $<$<CONFIG:Debug>:/bigobj>

        # Warning suppressions. Kept as-is rather than trimmed blind: each would
        # need a build to prove it is no longer raised, and a silent regression
        # here is a wall of noise rather than a compile error.
        /wd4018 /wd4100 /wd4101 /wd4127 /wd4131 /wd4189 /wd4244 /wd4245
        /wd4267 /wd4302 /wd4305 /wd4311 /wd4389 /wd4456 /wd4458 /wd4581
        /wd4589 /wd4701 /wd4702 /wd4703 /wd4706 /wd4840 /wd4996
    )
    # /GS- (no buffer security check) is deliberately gone. It traded a
    # documented mitigation against stack-buffer overruns for a few percent, in
    # a server that parses hostile network input for a living.
endif()

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    add_compile_options(
        $<$<CONFIG:Debug>:-Wall>
        $<$<CONFIG:Debug>:-Wextra>
        $<$<CONFIG:Debug>:-Winit-self>
        $<$<CONFIG:Debug>:-Winvalid-pch>
        $<$<CONFIG:Debug>:-g3>
        # Parameter-passing ABI note, not actionable:
        # https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77728
        $<$<CONFIG:Release>:-Wno-psabi>
    )
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "i386")
        add_compile_options(-msse2 -mfpmath=sse)
    endif()
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(
        $<$<CONFIG:Release>:-Wno-c++11-narrowing>
        $<$<CONFIG:Release>:-Wno-inconsistent-missing-override>
        $<$<CONFIG:Release>:-Wno-switch>
        $<$<CONFIG:Debug>:-Wall>
        $<$<CONFIG:Debug>:-Wextra>
        $<$<CONFIG:Debug>:-Winit-self>
        $<$<CONFIG:Debug>:-Woverloaded-virtual>
        $<$<CONFIG:Debug>:-g>
    )
    # -Wno-deprecated-register dropped: the `register` keyword was removed in
    # C++17, which this tree now requires, so the warning cannot be raised.
endif()

if(MSVC)
    set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
endif()
