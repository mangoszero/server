add_library(proto STATIC
  ClientConnection.cpp ClientConnection.h
  Listener.cpp Listener.h
  PacketCodec.cpp PacketCodec.h
  IClientLink.h IWorldGateway.h Opcodes.h)
target_include_directories(proto PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(proto PUBLIC shared PRIVATE mangos_openssl_strict)
