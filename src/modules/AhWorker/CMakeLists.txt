if(WIN32)
    set(ah_service_platform_SRC Console_win.cpp)
else()
    set(ah_service_platform_SRC Console_posix.cpp)
endif()

file(GLOB ah_service_SRC *.cpp *.h)

# Remove both platform files from the glob, then add only the right one.
list(REMOVE_ITEM ah_service_SRC
    "${CMAKE_CURRENT_SOURCE_DIR}/Console_win.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Console_posix.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Usability.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Usability.h"
)

# Usability is shared with the mangosd-side differential drift test (Task 8) via
# a separate test exe, so it is its own small static library. Keep it free of
# game headers (it includes only BrowseMessages.h / Common.h).
add_library(ah_usability STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Usability.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Usability.h")
target_include_directories(ah_usability PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(ah_usability PUBLIC ah_ipc)   # BrowseMessages.h / Common.h

add_executable(ah-service ${ah_service_SRC} ${ah_service_platform_SRC})
target_link_libraries(ah-service PRIVATE ah_usability ah_ipc shared Threads::Threads)
install(TARGETS ah-service DESTINATION ${BIN_DIR}/service-workers/ah-service)
# Substitute @MANGOS_AHBOT_VER@ (the ConfVersion) like mangosd.conf, then
# install the generated file (a raw copy would leave the placeholder literal).
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ah-service.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/ah-service.conf.dist)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ah-service.conf.dist DESTINATION ${CONF_INSTALL_DIR})

# ah-service runs as a STANDALONE process: spawned by mangosd in deployment, but
# also run directly by the CI self-test (and by hand). It links `shared`, which
# pulls in the MySQL/MariaDB client, so on Windows the binary needs that client
# DLL at load time. The DLL is system-found (FindMySQL) and only install()-copied
# to BIN_DIR for mangosd, so a standalone ah-service.exe in the build tree -- or
# in its own install subdir -- can't find it (the CI self-test failed here with
# exit 127). Co-locate the client DLL next to ah-service.exe at build time and in
# the install subdir so it is self-contained. The .dll sits beside or one level
# up (bin/) from the import .lib depending on the install layout, so search both.
if(WIN32 AND MySQL_LIBRARIES)
    get_filename_component(_ahsvc_mysql_libdir "${MySQL_LIBRARIES}" DIRECTORY)
    get_filename_component(_ahsvc_mysql_name "${MySQL_LIBRARIES}" NAME_WE)
    find_file(AHSVC_MYSQL_DLL
        NAMES "${_ahsvc_mysql_name}.dll"
        PATHS "${_ahsvc_mysql_libdir}" "${_ahsvc_mysql_libdir}/.."
        PATH_SUFFIXES "" bin lib
        NO_DEFAULT_PATH)
    if(AHSVC_MYSQL_DLL)
        add_custom_command(TARGET ah-service POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    "${AHSVC_MYSQL_DLL}" "$<TARGET_FILE_DIR:ah-service>"
            COMMENT "ah-service: co-locating MySQL client DLL (${_ahsvc_mysql_name}.dll)")
        install(FILES "${AHSVC_MYSQL_DLL}"
                DESTINATION ${BIN_DIR}/service-workers/ah-service)
    else()
        message(WARNING
            "ah-service: could not find the MySQL/MariaDB client DLL near "
            "${MySQL_LIBRARIES}; the standalone binary (and CI self-test) may "
            "fail to start with a missing-DLL error.")
    endif()
endif()
