add_executable(auth_crypto_tests AuthCryptoTests.cpp)
target_link_libraries(auth_crypto_tests PRIVATE shared mangos_openssl_strict)
add_test(NAME auth_crypto_tests COMMAND auth_crypto_tests)

add_executable(lease_tests LeaseTests.cpp)
target_include_directories(lease_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/shared")
target_link_libraries(lease_tests PRIVATE Threads::Threads)
add_test(NAME lease_tests COMMAND lease_tests)

add_executable(network_regression_tests NetworkRegressionTests.cpp)
target_link_libraries(network_regression_tests PRIVATE shared Threads::Threads)
add_test(NAME network_regression_tests COMMAND network_regression_tests)
set_tests_properties(network_regression_tests PROPERTIES TIMEOUT 60)
