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

add_executable(lease_tests LeaseTests.cpp)
target_include_directories(lease_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/shared")
target_link_libraries(lease_tests PRIVATE Threads::Threads)
add_test(NAME lease_tests COMMAND lease_tests)

add_executable(network_regression_tests NetworkRegressionTests.cpp)
target_link_libraries(network_regression_tests PRIVATE shared Threads::Threads)
add_test(NAME network_regression_tests COMMAND network_regression_tests)
set_tests_properties(network_regression_tests PROPERTIES TIMEOUT 60)
