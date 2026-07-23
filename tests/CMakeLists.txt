add_executable(auth_crypto_tests AuthCryptoTests.cpp)
target_link_libraries(auth_crypto_tests PRIVATE shared mangos_openssl_strict)
add_test(NAME auth_crypto_tests COMMAND auth_crypto_tests)
if(WIN32)
  find_file(MANGOS_TEST_OPENSSL_CRYPTO_DLL
    NAMES libcrypto-3-x64.dll libcrypto-3.dll
    HINTS "${OPENSSL_ROOT_DIR}/bin"
    NO_DEFAULT_PATH)
  if(NOT MANGOS_TEST_OPENSSL_CRYPTO_DLL)
    message(FATAL_ERROR "Could not find the OpenSSL 3.x crypto runtime DLL under ${OPENSSL_ROOT_DIR}/bin")
  endif()
  add_custom_command(TARGET auth_crypto_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MANGOS_TEST_OPENSSL_CRYPTO_DLL}" "$<TARGET_FILE_DIR:auth_crypto_tests>"
    COMMENT "auth_crypto_tests: co-locating the OpenSSL 3.x crypto runtime")
endif()

add_executable(network_regression_tests NetworkRegressionTests.cpp)
target_link_libraries(network_regression_tests PRIVATE shared Threads::Threads)
add_test(NAME network_regression_tests COMMAND network_regression_tests)
set_tests_properties(network_regression_tests PROPERTIES TIMEOUT 60)

add_test(NAME proto_boundary
  COMMAND ${CMAKE_COMMAND}
    -DPROTO_DIR=${PROJECT_SOURCE_DIR}/src/proto
    -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckProtoBoundary.cmake)

add_executable(protocol_tests ProtocolTests.cpp)
target_link_libraries(protocol_tests PRIVATE proto shared Threads::Threads mangos_openssl_strict)
add_test(NAME protocol_tests COMMAND protocol_tests)
set_tests_properties(protocol_tests PROPERTIES TIMEOUT 5)
if(WIN32)
  add_custom_command(TARGET protocol_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MANGOS_TEST_OPENSSL_CRYPTO_DLL}" "$<TARGET_FILE_DIR:protocol_tests>"
    COMMENT "protocol_tests: co-locating the OpenSSL 3.x crypto runtime")
endif()

add_executable(database_concurrency_tests DatabaseConcurrencyTests.cpp)
target_link_libraries(database_concurrency_tests PRIVATE shared Threads::Threads)
add_test(NAME database_concurrency_tests COMMAND database_concurrency_tests)
if(WIN32)
  get_filename_component(MANGOS_TEST_MYSQL_DLL_DIR "${MySQL_LIBRARY}" DIRECTORY)
  get_filename_component(MANGOS_TEST_MYSQL_DLL_NAME "${MySQL_LIBRARY}" NAME)
  string(REPLACE ".lib" ".dll" MANGOS_TEST_MYSQL_DLL_NAME
    "${MANGOS_TEST_MYSQL_DLL_NAME}")
  add_custom_command(TARGET database_concurrency_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${MANGOS_TEST_MYSQL_DLL_DIR}/${MANGOS_TEST_MYSQL_DLL_NAME}"
            "$<TARGET_FILE_DIR:database_concurrency_tests>"
    COMMENT "database_concurrency_tests: co-locating the MariaDB client runtime")
endif()

add_executable(session_mailbox_tests
  SessionMailboxTests.cpp
  ${PROJECT_SOURCE_DIR}/src/game/Server/SessionMailbox.cpp)
target_include_directories(session_mailbox_tests PRIVATE
  ${PROJECT_SOURCE_DIR}/src/game/Server)
target_link_libraries(session_mailbox_tests PRIVATE shared Threads::Threads)
add_test(NAME session_mailbox_tests COMMAND session_mailbox_tests)

add_test(NAME world_network_boundary
  COMMAND ${CMAKE_COMMAND}
    -DSOURCE_ROOT=${PROJECT_SOURCE_DIR}
    -DEXPECT_LEGACY_REMOVED=ON
    -P ${CMAKE_CURRENT_SOURCE_DIR}/CheckWorldNetworkBoundary.cmake)
