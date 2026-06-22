#include "IpcVersion.h"
#include "IpcMessage.h"
#include "BoundedQueue.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// Temporary in-process round-trip smoke test (replaced wholesale in Task 3).
static void RunSmokeTest()
{
    // Build an IPC_ECHO message with a simple string body.
    IpcMessage msg;
    msg.op = IPC_ECHO;

    const char* payload = "hello ipc";
    msg.body.append(reinterpret_cast<const uint8*>(payload), strlen(payload));

    // Encode into a wire buffer.
    ByteBuffer wire;
    msg.Encode(wire);

    // Decode back from the same buffer.
    IpcMessage decoded;
    std::string err;

    if (!IpcMessage::Decode(wire, decoded, err))
    {
        printf("frame round-trip FAILED: %s\n", err.c_str());
        exit(1);
    }

    // Validate opcode.
    if (decoded.op != IPC_ECHO)
    {
        printf("frame round-trip FAILED: opcode mismatch (%u)\n", static_cast<unsigned>(decoded.op));
        exit(1);
    }

    // Validate body length and content.
    size_t payloadLen = strlen(payload);

    if (decoded.body.size() != payloadLen)
    {
        printf("frame round-trip FAILED: body size mismatch (%zu vs %zu)\n",
               decoded.body.size(), payloadLen);
        exit(1);
    }

    if (memcmp(decoded.body.contents(), payload, payloadLen) != 0)
    {
        printf("frame round-trip FAILED: body content mismatch\n");
        exit(1);
    }

    printf("frame round-trip OK\n");
}

int main(int argc, char** argv)
{
    printf("ah-service (ipc proto v%u) starting\n", IPC_PROTOCOL_VERSION);
    RunSmokeTest();
    return 0;
}
