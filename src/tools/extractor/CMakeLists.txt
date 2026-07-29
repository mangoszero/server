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
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

# =============================================================================
# The offline baker, in three layers that each depend on strictly less than the
# one above:
#
#   extractor_client  parsers and model loaders. Pure byte-pushing over a buffer
#                     -- no DBC, no MPQ container, no world. That is what keeps
#                     it linkable, and testable, on its own.
#   extractor_data    the DBC stores and tile assembly. Compiles the server's own
#                     DBCFileLoader rather than linking `shared`, which would drag
#                     in MySQL and OpenSSL as PUBLIC dependencies.
#   extractor_mpq     the StormLib container reader.
# =============================================================================

set(SRC_GRP_CLIENT
    client/AdtParser.cpp
    client/AdtParser.hpp
    client/ChunkReaders.hpp
    client/IMpqArchive.hpp
    client/M2Parser.cpp
    client/M2Parser.hpp
    client/WdtParser.cpp
    client/WdtParser.hpp
    client/WmoParser.cpp
    client/WmoParser.hpp
)
source_group("client" FILES ${SRC_GRP_CLIENT})

add_library(extractor_client STATIC ${SRC_GRP_CLIENT})

target_include_directories(extractor_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/client
)

target_link_libraries(extractor_client
    PUBLIC
        terrain
)

set_target_properties(extractor_client PROPERTIES FOLDER "tools")

set(SRC_GRP_NAV
    nav/NavMeshBuilder.cpp
    nav/NavMeshBuilder.hpp
)
source_group("nav" FILES ${SRC_GRP_NAV})

set(SRC_GRP_DATA
    client/ModelLoaders.cpp
    client/ModelLoaders.hpp
    client/MpqTileSource.cpp
    client/MpqTileSource.hpp
    stores/LiquidTypeStore.hpp
    stores/MapDbcStore.hpp
    stores/MpqDbcLoader.hpp
    ${CMAKE_SOURCE_DIR}/src/shared/DataStores/DBCFileLoader.cpp
)
source_group("data" FILES ${SRC_GRP_DATA})

add_library(extractor_data STATIC ${SRC_GRP_DATA} ${SRC_GRP_NAV})

target_include_directories(extractor_data
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/shared
        ${CMAKE_SOURCE_DIR}/src/game
        # For MoveMapSharedDefines.h: the on-disk mmtile header is the server's
        # declaration, included rather than copied.
        ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
)

target_link_libraries(extractor_data
    PUBLIC
        extractor_client
        RecastNavigation::Recast
        RecastNavigation::Detour
)

set_target_properties(extractor_data PROPERTIES FOLDER "tools")

add_library(extractor_mpq STATIC
        client/StormLibArchive.cpp
        client/StormLibArchive.hpp
    )

target_link_libraries(extractor_mpq
    PUBLIC
        extractor_data
    PRIVATE
        stormlib
)

set_target_properties(extractor_mpq PROPERTIES FOLDER "tools")

# The console is compiled IN rather than linked from `shared`, which exports MySQL and
# OpenSSL as PUBLIC dependencies the baker has no use for. It carries no dependency of
# its own, which is what makes that possible.
add_executable(mangos-extractor
    Extractor.cpp
    ExtractorConsole.cpp
    ExtractorConsole.hpp
    ${CMAKE_SOURCE_DIR}/src/shared/Console/ConsoleUI.cpp
    ${CMAKE_SOURCE_DIR}/src/shared/Console/Terminal.cpp
    $<$<BOOL:${WIN32}>:${CMAKE_CURRENT_SOURCE_DIR}/mangos-extractor.rc>
)

target_include_directories(mangos-extractor
    PRIVATE ${CMAKE_SOURCE_DIR}/src/shared/Console)

target_link_libraries(mangos-extractor
    PRIVATE
        extractor_mpq
        # The baker threads its per-tile work; relying on another target's link line to
        # drag in the thread library is an accident, not a dependency.
        Threads::Threads
        # SHBrowseForFolder and the COM apartment it needs.
        $<$<BOOL:${WIN32}>:shell32>
        $<$<BOOL:${WIN32}>:ole32>
)

# The icon and version block are compiled from revision_data.h, which genrev writes
# into the build tree.
if(WIN32)
    target_include_directories(mangos-extractor PRIVATE ${CMAKE_BINARY_DIR}/src/shared)
    if(TARGET revision_data.h)
        add_dependencies(mangos-extractor revision_data.h)
    endif()
endif()

set_target_properties(mangos-extractor PROPERTIES FOLDER "tools")
install(TARGETS mangos-extractor DESTINATION ${BIN_DIR}/tools)

# Beside the binary, because that is where the tool looks for it when `--vessels` is
# not given. It is data, not configuration: it changes when the client does.
install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/vessels.txt
              ${CMAKE_CURRENT_SOURCE_DIR}/offmesh.txt
        DESTINATION ${BIN_DIR}/tools)

# And beside the BUILT binary as well, not only the installed one. The tool resolves
# both defaults relative to its own executable, so a build tree that lacks them is a
# baker that silently runs without the hand-authored input -- which for offmesh.txt
# means a navmesh missing exactly the links nobody thinks to test.
add_custom_command(TARGET mangos-extractor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/vessels.txt
        ${CMAKE_CURRENT_SOURCE_DIR}/offmesh.txt
        $<TARGET_FILE_DIR:mangos-extractor>)

# A window in front of the baker, Windows only. It SPAWNS mangos-extractor and reads its
# stdout -- it links nothing of the baker, so the two cannot drift into each other.
if(WIN32)
    add_executable(mangos-extractor-gui WIN32
        ExtractorGui.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mangos-extractor-gui.rc)
    target_link_libraries(mangos-extractor-gui PRIVATE shell32 ole32 comctl32 comdlg32)
    # The .rc includes revision_data.h, which genrev writes into the build tree.
    target_include_directories(mangos-extractor-gui PRIVATE ${CMAKE_BINARY_DIR}/src/shared)
    if(TARGET revision_data.h)
        add_dependencies(mangos-extractor-gui revision_data.h)
    endif()
    set_target_properties(mangos-extractor-gui PROPERTIES FOLDER "tools")
    install(TARGETS mangos-extractor-gui DESTINATION ${BIN_DIR}/tools)
endif()
