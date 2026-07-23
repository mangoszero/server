add_library(proto INTERFACE)
target_include_directories(proto INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(proto INTERFACE shared)
