#include "IpcVersion.h"
#include <cstdio>
int main(int argc, char** argv)
{
    printf("ah-service (ipc proto v%u) starting\n", IPC_PROTOCOL_VERSION);
    return 0;
}
