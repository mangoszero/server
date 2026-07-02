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

# SD3 and Eluna link the `game` library, which only exists when BUILD_MANGOSD.
# Guard on BOTH the script-lib option AND BUILD_MANGOSD so a worker-only
# configure (BUILD_MANGOSD=OFF, BUILD_AH_SERVICE=ON) does not try to build them.
if(BUILD_MANGOSD AND SCRIPT_LIB_SD3)
    add_subdirectory(SD3)
endif()

if(BUILD_MANGOSD AND SCRIPT_LIB_ELUNA)
    add_subdirectory(Eluna)
endif()

# The out-of-process auction-house worker. Self-contained: links only the IPC
# wire contract (ah_ipc) + shared; never the mangosd game library. Gated by the
# existing BUILD_AH_SERVICE option.
if(BUILD_AH_SERVICE)
    add_subdirectory(AhWorker)
endif()