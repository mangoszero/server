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

# =============================================================================
# Unit tests. -DWITH_TESTS=1 builds them; -DWITH_NET_TESTS=1 adds the socket tests.
#
# Scope is deliberately narrow: the pieces that are pure functions over bytes and
# whose failure modes are invisible in ordinary use -- packet reassembly under
# fragmentation, and a zero-padding bug that shows about one login in 256.
#
# THE TWO SWITCHES ARE DISJOINT. The socket tests bind real listeners and cost more
# wall-clock than the whole rest of the suite, so WITH_TESTS never implies them. They
# are not run in CI. Ask for them by name when a change touches the reactor.
#
# No test framework is vendored; see TestHarness.h.
# =============================================================================

set(SRC_GRP_TESTS
    main.cpp
    TestHarness.h
    BigNumberTest.cpp
    Utf8Test.cpp
    ConfigTest.cpp
    ByteBufferTest.cpp
    EventProcessorHostileTest.cpp
    SessionMailboxTest.cpp
    SessionProtocolPolicyTest.cpp
    WorldGatewayAccountTest.cpp
    WardenModuleCatalogTest.cpp
    WardenCheckFixtures.h
    WardenCheckCatalogTest.cpp
    WardenConfigurationTest.cpp
    WardenCryptoContextTest.cpp
    WardenPacketCodecTest.cpp
    WardenCheckPlannerTest.cpp
    WardenEnforcementPolicyTest.cpp
    WardenAuditStoreTest.cpp
    WardenIncidentStoreTest.cpp
    WardenServerTest.cpp
    DatabaseConcurrencyTest.cpp
    DatabaseVersionTest.cpp
    OpenSSLProviderTest.cpp
    ClientParserTest.cpp
    TerrainModelTest.cpp
    TileSerializerTest.cpp
    ModelMapTest.cpp
    NavBinningTest.cpp
    DynamicCollisionTest.cpp
    PlacementTest.cpp
    PlayerbotOutOfRangeMoverTest.cpp
    RandomBotClassPolicyTest.cpp
    PlayerbotMountPolicyTest.cpp
    PlayerbotSecurityPolicyTest.cpp
    PlayerbotLogPolicyTest.cpp
    PlayerbotEventQueueTest.cpp
    PlayerbotPacketPolicyTest.cpp
    PlayerbotPerformanceMonitorTest.cpp
    # Compiled in, not linked from `game`: game.lib pulls the whole server, down to the
    # database globals that only mangosd defines. These three know nothing of it.
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers/DynamicCollision.cpp
    ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers/GameObjectModel.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Server/SessionMailbox.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Server/WorldGatewayAccount.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenProtocol.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenConfiguration.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenCryptoContext.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenPacketCodec.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenCheckPlanner.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenEvidence.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenEnforcementPolicy.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenServer.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenManager.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenModuleWin5875Data.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenModuleCatalog.cpp
    ${CMAKE_SOURCE_DIR}/src/game/Warden/WardenCheckCatalog.cpp
    ByteBufferStressTest.cpp
    CryptoStressTest.cpp
    CodecStressTest.cpp
    PacketCodecTest.cpp
)

# The socket tests. Off unless asked for -- see the note above.
if(WITH_NET_TESTS)
    list(APPEND SRC_GRP_TESTS GracefulCloseTest.cpp)
    list(APPEND SRC_GRP_TESTS NetworkStressTest.cpp)
endif()

source_group("tests" FILES ${SRC_GRP_TESTS})

add_executable(mangos_tests ${SRC_GRP_TESTS})

target_include_directories(mangos_tests
    PRIVATE
        ${CMAKE_SOURCE_DIR}/src/game/WorldHandlers
        ${CMAKE_SOURCE_DIR}/src/game/Server
        ${CMAKE_SOURCE_DIR}/src/game/Warden)

target_link_libraries(mangos_tests
    PRIVATE
        proto
        shared
        extractor_client
        extractor_data
        Threads::Threads
        mangos_openssl_strict
)

add_test(NAME mangos_tests COMMAND mangos_tests)

add_test(NAME proto_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckProtoBoundary.cmake)

add_test(NAME build_policy
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckBuildPolicy.cmake)

add_test(NAME warden_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckWardenBoundary.cmake)

add_test(NAME spatial_boundary
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckSpatialBoundary.cmake)

add_test(NAME movement_state_seed
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckMovementStateSeed.cmake)

add_test(NAME bot_relocation_notifies
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_ROOT=${CMAKE_SOURCE_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckBotRelocationNotifies.cmake)

# Put the runtime DLLs beside the test binary so it can be run from the build tree.
# A convenience, never a gate: the search is derived from the library that was actually
# found, and a miss warns rather than failing the configure.
if(WIN32)
    get_filename_component(MANGOS_SSL_LIB_DIR "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
    get_filename_component(MANGOS_SSL_ROOT "${MANGOS_SSL_LIB_DIR}" DIRECTORY)

    find_file(MANGOS_TEST_OPENSSL_CRYPTO_DLL
        NAMES libcrypto-3-x64.dll libcrypto-3.dll libcrypto.dll
        HINTS "${MANGOS_SSL_ROOT}/bin" "${MANGOS_SSL_LIB_DIR}" "${OPENSSL_ROOT_DIR}/bin")

    if(MANGOS_TEST_OPENSSL_CRYPTO_DLL)
        add_custom_command(TARGET mangos_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_OPENSSL_CRYPTO_DLL}"
                "$<TARGET_FILE_DIR:mangos_tests>")
    else()
        message(WARNING
            "OpenSSL runtime DLL not found; mangos_tests may need it on PATH to run.")
    endif()

    get_filename_component(
        MANGOS_TEST_MYSQL_DLL_DIR "${MySQL_LIBRARY}" DIRECTORY)
    get_filename_component(
        MANGOS_TEST_MYSQL_DLL_NAME "${MySQL_LIBRARY}" NAME)
    string(REPLACE ".lib" ".dll" MANGOS_TEST_MYSQL_DLL_NAME
        "${MANGOS_TEST_MYSQL_DLL_NAME}")
    if(EXISTS "${MANGOS_TEST_MYSQL_DLL_DIR}/${MANGOS_TEST_MYSQL_DLL_NAME}")
        add_custom_command(TARGET mangos_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_MYSQL_DLL_DIR}/${MANGOS_TEST_MYSQL_DLL_NAME}"
                "$<TARGET_FILE_DIR:mangos_tests>")
    endif()
    set_tests_properties(mangos_tests PROPERTIES
        ENVIRONMENT "OPENSSL_MODULES=")

    # Match the release layout and leave OPENSSL_MODULES empty: provider discovery
    # must work from ossl-modules beside the executable, not from the build host.
    find_file(MANGOS_TEST_OPENSSL_LEGACY_DLL
        NAMES legacy.dll
        HINTS "${MANGOS_SSL_ROOT}" "${OPENSSL_ROOT_DIR}"
        PATH_SUFFIXES bin/ossl-modules bin ossl-modules lib/ossl-modules ""
        NO_DEFAULT_PATH)

    if(MANGOS_TEST_OPENSSL_LEGACY_DLL)
        add_custom_command(TARGET mangos_tests POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory
                "$<TARGET_FILE_DIR:mangos_tests>/ossl-modules"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MANGOS_TEST_OPENSSL_LEGACY_DLL}"
                "$<TARGET_FILE_DIR:mangos_tests>/ossl-modules/legacy.dll")
    else()
        message(WARNING
            "OpenSSL legacy provider (legacy.dll) not found; mangos_tests may fail its RC4 cases.")
    endif()
endif()
