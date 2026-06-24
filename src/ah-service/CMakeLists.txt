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
)

add_executable(ah-service ${ah_service_SRC} ${ah_service_platform_SRC})
target_link_libraries(ah-service PRIVATE ah_ipc shared Threads::Threads)
install(TARGETS ah-service DESTINATION ${BIN_DIR}/service-workers/ah-service)
# Substitute @MANGOS_AHBOT_VER@ (the ConfVersion) like mangosd.conf, then
# install the generated file (a raw copy would leave the placeholder literal).
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ah-service.conf.dist.in ${CMAKE_CURRENT_BINARY_DIR}/ah-service.conf.dist)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ah-service.conf.dist DESTINATION ${CONF_INSTALL_DIR})
