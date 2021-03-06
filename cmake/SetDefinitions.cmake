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

if(CMAKE_SIZEOF_VOID_P MATCHES 8)
    set(PLATFORM 64)
else()
    set(PLATFORM 32)
endif()

if(XCODE)
  # Here we add a check for ARM32, as they can't leverage SSE/SSE2
  if(PLATFORM MATCHES 32 AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm")
    set(CMAKE_OSX_ARCHITECTURES ARM32)
  # Default for 32-bit left as i386
  elseif(PLATFORM MATCHES 32)
    set(CMAKE_OSX_ARCHITECTURES i386)
  # Check for ARM64
  elseif(PLATFORM MATCHES 64 AND ${CMAKE_SYSTEM_PROCESSOR} MATCHES "^arm") 
    set(CMAKE_OSX_ARCHITECTURES ARM64)
  # Default for 64-bit left as x86_64
  else()
    set(CMAKE_OSX_ARCHITECTURES x86_64)
  endif()
endif()

#
# Compile definitions
#
set(DEFAULT_COMPILE_DEFS)

# MSVC compiler
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(DEFAULT_COMPILE_DEFS ${DEFAULT_COMPILE_DEFS}
        /D_SCL_SECURE_NO_WARNINGS
        /D_CRT_SECURE_NO_WARNINGS
        /D_WINSOCK_DEPRECATED_NO_WARNINGS
        /D_CRT_NONSTDC_NO_DEPRECATE
        $<$<EQUAL:${PLATFORM},64>: /D_WIN64>
    )
endif()

# MINGW
if (MINGW)
    set(DEFAULT_COMPILE_DEFS ${DEFAULT_COMPILE_DEFS}
        -DWINVER=0x0600
        -D_WIN32_WINNT=0x0600
        $<$<EQUAL:${PLATFORM},32>:
            -DHAVE_SSE2
            -D__SSE2__
        >
        $<$<EQUAL:${PLATFORM},64>: -D_WIN64>
    )
endif()


#
# Compile options
#
set(DEFAULT_COMPILE_OPTS)

# MSVC compiler options
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "MSVC")
    set(DEFAULT_COMPILE_OPTS ${DEFAULT_COMPILE_OPTS}
        /MP           #multiple processes
        /W4           #warning level 4

        $<$<EQUAL:${PLATFORM},32>:/arch:SSE2>

        $<$<CONFIG:Release>:
        /Gw           #whole program global optimization
        /GS-          #no buffer security check
        /GF           #string pooling
        >
        $<$<CONFIG:Debug>:
        /bigobj
        >

        /wd4996
        /wd4267
        /wd4244
        /wd4245
        /wd4458
        /wd4581
        /wd4589
        /wd4131
        /wd4311
        /wd4456
        /wd4127
        /wd4100
        /wd4389
        /wd4189
        /wd4701
        /wd4706
        /wd4703
        /wd4702
        /wd4302
        /wd4305
        /wd4018
        /wd4840
        /wd4101
    )
endif ()

# GCC compiler options
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    set(DEFAULT_COMPILE_OPTS ${DEFAULT_COMPILE_OPTS}
    # Enhanced 32-bit check, now we can use the arch to specify flags
    $<$<STREQUAL:${CMAKE_OSX_ARCHITECTURES},"i386">:
            -msse2
            -mfpmath=sse
        >
    $<$<STREQUAL:${CMAKE_OSX_ARCHITECTURES},"ARM32">:
    # explicit space for compiler flags
    >
        $<$<STREQUAL:${CMAKE_OSX_ARCHITECTURES},"ARM64">:
        # explicit space for compiler flags
        >
        $<$<CONFIG:Debug>:
          -W
          -Wall
          -Wextra
          -Winit-self
          -Wfatal-errors
          -Winvalid-pch
          -g3
        >

        $<$<CONFIG:Release>:
          --no-warnings
      # Suppress compiler note on parameter passing.  See the following
      # GCC BZ for more info:  https://gcc.gnu.org/bugzilla/show_bug.cgi?id=77728
      -Wno-psabi
        >
    )
endif ()

#Clang compiler options
if("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(DEFAULT_COMPILE_OPTS ${DEFAULT_COMPILE_OPTS}
        $<$<CONFIG:Release>:
            -Wno-c++11-narrowing
            -Wno-inconsistent-missing-override
            -Wno-deprecated-register
            -Wno-switch
        >
        $<$<CONFIG:Debug>:
            -W
            -Wall
            -Wextra
            -Winit-self
            -Wfatal-errors
            -Woverloaded-virtual
            -glldb
            -gline-tables-only
        >
    )
endif()

separate_arguments(DEFAULT_COMPILE_DEFS)
separate_arguments(DEFAULT_COMPILE_OPTS)

set_property(DIRECTORY
    PROPERTY COMPILE_DEFINITIONS ${DEFAULT_COMPILE_DEFS}
    PROPERTY COMPILE_OPTIONS     ${DEFAULT_COMPILE_OPTS}
)

unset(DEFAULT_COMPILE_DEFS)
unset(DEFAULT_COMPILE_OPTS)

#
# Misc settings
#
if(MSVC)
    set(CMAKE_VS_INCLUDE_INSTALL_TO_DEFAULT_BUILD ON)
endif()
