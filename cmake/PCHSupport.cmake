# CMake precompiled header macro
# Distributed under the MIT Software License
# Copyright (c) 2015-2017 Borislav Stanimirov
#
# Permission is hereby granted, free of charge, to any person obtaining a copy of 
# this software and associated documentation files (the "Software"), to deal in 
# the Software without restriction, including without limitation the rights to 
# use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
# of the Software, and to permit persons to whom the Software is furnished to do 
# so, subject to the following conditions:
# The above copyright notice and this permission notice shall be included in all 
# copies or substantial portions of the Software.
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
# SOFTWARE.
#
# ADD_CXX_PCH(TARGET_NAME PRECOMPILED_HEADER PRECOMPILED_SOURCE)
#
# Sets a precompiled header for a given target
#
# TARGET_NAME - Name of the target. Only valid after add_library or add_executable
# PRECOMPILED_HEADER - Header file to precompile
# PRECOMPILED_SOURCE - MSVC specific source to do the actual precompilation. Ignored on other platforms
#

function(ADD_CXX_PCH TARGET_NAME PRECOMPILED_HEADER PRECOMPILED_SOURCE)
    get_filename_component(PRECOMPILED_HEADER_NAME ${PRECOMPILED_HEADER} NAME)

    if(MSVC)
        target_compile_options(${TARGET_NAME}
            PRIVATE
                /FI${PRECOMPILED_HEADER_NAME}
                /Yu${PRECOMPILED_HEADER_NAME}
        )
        SET_SOURCE_FILES_PROPERTIES(${PRECOMPILED_SOURCE}
            PROPERTIES
                COMPILE_FLAGS /Yc${PRECOMPILED_HEADER_NAME}
        )
    elseif(CMAKE_GENERATOR STREQUAL Xcode)
        set_target_properties(
            ${TARGET_NAME}
            PROPERTIES
                XCODE_ATTRIBUTE_GCC_PREFIX_HEADER "${PRECOMPILED_HEADER}"
                XCODE_ATTRIBUTE_GCC_PRECOMPILE_PREFIX_HEADER "YES"
            )
    elseif(CMAKE_COMPILER_IS_GNUCC OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(CMAKE_COMPILER_IS_GNUCC)
            set(SFX gch)
        else()
            set(SFX pch)
        endif()

        # Create and set output directory.
        set(OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_${SFX}")
        set(OUTPUT_NAME "${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}.${SFX}")

        make_directory(${OUTPUT_DIR})

        # Export compiler flags via a generator to a response file
        set(PCH_FLAGS_FILE "${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}.rsp")
        set(_include_directories "$<TARGET_PROPERTY:${TARGET_NAME},INCLUDE_DIRECTORIES>")
        set(_compile_definitions "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_DEFINITIONS>")
        set(_compile_flags "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_FLAGS>")
        set(_compile_options "$<TARGET_PROPERTY:${TARGET_NAME},COMPILE_OPTIONS>")
        set(_include_directories "$<$<BOOL:${_include_directories}>:-I$<JOIN:${_include_directories},\n-I>\n>")
        set(_compile_definitions "$<$<BOOL:${_compile_definitions}>:-D$<JOIN:${_compile_definitions},\n-D>\n>")
        set(_compile_flags "$<$<BOOL:${_compile_flags}>:$<JOIN:${_compile_flags},\n>\n>")
        set(_compile_options "$<$<BOOL:${_compile_options}>:$<JOIN:${_compile_options},\n>\n>")

        file(GENERATE OUTPUT ${PCH_FLAGS_FILE} CONTENT "${_compile_definitions}${_include_directories}${_compile_flags}${_compile_options}\n")
        file(GENERATE OUTPUT ${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME} CONTENT "")

        # Gather global compiler options, definitions, etc.
        string(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" CXX_FLAGS)
        set(COMPILER_FLAGS "${${CXX_FLAGS}} ${CMAKE_CXX_FLAGS}")
        separate_arguments(COMPILER_FLAGS)

        set(CXX_STD c++11)

        add_custom_command(
            OUTPUT ${OUTPUT_NAME}
            COMMAND ${CMAKE_CXX_COMPILER} @${PCH_FLAGS_FILE} ${COMPILER_FLAGS} -x c++-header -std=${CXX_STD} -o ${OUTPUT_NAME} ${PRECOMPILED_HEADER}
            DEPENDS ${PRECOMPILED_HEADER}
        )

        add_custom_target(${TARGET_NAME}_${SFX} DEPENDS ${OUTPUT_NAME})
        add_dependencies(${TARGET_NAME} ${TARGET_NAME}_${SFX})

        target_compile_options(${TARGET_NAME}
            PRIVATE
                -include ${OUTPUT_DIR}/${PRECOMPILED_HEADER_NAME}
                -Winvalid-pch
        )
    endif()
endfunction()
