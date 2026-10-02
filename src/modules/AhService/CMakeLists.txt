# SPDX-License-Identifier: GPL-3.0-or-later
#
# MaNGOS is a full featured server for World of Warcraft, supporting
# the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
#
# Copyright (C) 2005-2026 MaNGOS <https://www.getmangos.eu>
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
# along with this program. If not, see <https://www.gnu.org/licenses/>.

add_subdirectory(ipc)
add_subdirectory(worker)

if(BUILD_MANGOSD)
    if(NOT BUILD_AHBOT)
        message(FATAL_ERROR "BUILD_AH_SERVICE needs BUILD_AHBOT: the worker lists as the auction house bot's character.")
    endif()

    file(GLOB ahservice_world_SRC world/*.cpp world/*.h)
    file(GLOB ahservice_tests_SRC tests/*.cpp)

    add_library(ahservice_module OBJECT
        ${ahservice_world_SRC}
        ${ahservice_tests_SRC}
        worker/difftest/AhUsabilityRef.cpp
        worker/difftest/AhUsabilityRef.h
    )

    target_include_directories(ahservice_module
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}/world
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/worker/difftest
            ${CMAKE_SOURCE_DIR}/src/mangosd
            $<$<BOOL:${SCRIPT_LIB_ELUNA}>:${CMAKE_SOURCE_DIR}/src/shared/Compat/eluna>
    )
    target_link_libraries(ahservice_module PUBLIC game ah_ipc ahbot_module)

    target_link_libraries(mangosd PRIVATE ahservice_module)
endif()
