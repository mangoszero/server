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

# =============================================================================
# mangos-height-check -- scores baked tiles against the world database's own
# ground-only creature spawns. It links `terrain` and nothing else: no database,
# no game, no client. The corpus arrives as a CSV so the tool never needs either.
# =============================================================================

add_executable(mangos-height-check HeightCheck.cpp)

target_link_libraries(mangos-height-check PRIVATE terrain)

set_target_properties(mangos-height-check PROPERTIES FOLDER "tools")

install(TARGETS mangos-height-check DESTINATION ${BIN_DIR}/tools)
