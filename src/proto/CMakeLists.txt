add_library(proto STATIC
  PacketCodec.cpp PacketCodec.h
  IClientLink.h IWorldGateway.h Opcodes.h)
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(proto PUBLIC shared PRIVATE mangos_openssl_strict)
