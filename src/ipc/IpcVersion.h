#ifndef AH_IPC_VERSION_H
#define AH_IPC_VERSION_H
#include "Common.h"
// Bump on ANY incompatible wire change. Carried in handshake AND every frame header.
static const uint16 IPC_PROTOCOL_VERSION = 1;

uint16 IpcProtocolVersion();
#endif
