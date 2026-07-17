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

include(CheckCXXCompilerFlag)

# Clang re-instantiates the PCH's templates in EVERY translation unit that loads it, which for
# a header set this size hands most of the saving straight back. This bakes the instantiations
# into the PCH once instead. GCC and MSVC already do the equivalent. (Clang >= 11.)
check_cxx_compiler_flag("-fpch-instantiate-templates" HAVE_FPCH_INSTANTIATE_TEMPLATES)

# ADD_CXX_PCH(TARGET_NAME PRECOMPILED_HEADER [PRECOMPILED_SOURCE])
#
# PRECOMPILED_SOURCE is accepted for backwards compatibility with the old hand-rolled MSVC
# /Yc scheme and ignored -- target_precompile_headers generates its own translation unit.
function(ADD_CXX_PCH TARGET_NAME PRECOMPILED_HEADER)
	if(NOT TARGET ${TARGET_NAME})
		message(FATAL_ERROR "ADD_CXX_PCH: '${TARGET_NAME}' is not a target.")
	endif()

	# Guard, because the failure this catches is invisible. ADD_CXX_PCH(Bots ${bots_PCH}) with
	# bots_PCH unset collapses to ADD_CXX_PCH(Bots), which used to reach
	# target_precompile_headers with an EMPTY header list -- a silent no-op. Bots' 277 sources
	# went years without the precompiled header they all textually include anyway. Fail loudly.
	if(NOT PRECOMPILED_HEADER)
		message(FATAL_ERROR "ADD_CXX_PCH(${TARGET_NAME}): no precompiled header given.")
	endif()

	get_filename_component(_pch "${PRECOMPILED_HEADER}" ABSOLUTE
		BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

	if(NOT EXISTS "${_pch}")
		message(FATAL_ERROR "ADD_CXX_PCH(${TARGET_NAME}): header '${_pch}' does not exist.")
	endif()

	target_precompile_headers(${TARGET_NAME} PRIVATE "${_pch}")

	if(HAVE_FPCH_INSTANTIATE_TEMPLATES)
		target_compile_options(${TARGET_NAME} PRIVATE -fpch-instantiate-templates)
	endif()
endfunction()
