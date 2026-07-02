file(GLOB ah_ipc_SRC *.cpp *.h)
add_library(ah_ipc STATIC ${ah_ipc_SRC})
target_include_directories(ah_ipc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ah_ipc PUBLIC shared)   # shared brings ace, ByteBuffer, LockedQueue, Config
