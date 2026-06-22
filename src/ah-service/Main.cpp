/**
 * MaNGOS is a full featured server for World of Warcraft, supporting
 * the following clients: 1.12.x, 2.4.3, 3.3.5a, 4.3.4a and 5.4.8
 *
 * Copyright (C) 2005-2025 MaNGOS <https://www.getmangos.eu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/**
 * @file Main.cpp
 * @brief ah-service entry point.
 *
 * Modes:
 *   --selftest           Run an in-process loopback test (server + client,
 *                        full handshake + IPC_ECHO round-trip), then exit 0.
 *
 *   --port <p>           Connect to mangosd IPC server on this port.
 *   --secret <s>         Shared secret for handshake authentication.
 *   --botguid <g>        (reserved; not used in M1)
 *   --config <path>      (reserved; not used in M1)
 *
 * Normal mode: connect, handshake, then loop handling:
 *   IPC_HEARTBEAT  → IPC_HEARTBEAT_ACK
 *   IPC_ECHO       → IPC_ECHO_REPLY (body echoed back)
 *   IPC_SHUTDOWN   → IPC_SHUTDOWN_ACK, then exit
 */

#include "IpcVersion.h"
#include "IpcChannel.h"
#include "IpcMessage.h"
#include "IpcOpcodes.h"
#include "Threading/Threading.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

// ---------------------------------------------------------------------------
// Self-test: in-process loopback
// ---------------------------------------------------------------------------

/**
 * @brief Run a full in-process loopback self-test.
 *
 * Starts IpcServer + IpcClient in-process on 127.0.0.1:17878, performs
 * the handshake, sends IPC_ECHO, waits for IPC_ECHO_REPLY, then shuts
 * down both sides.
 *
 * @return 0 on success, 1 on failure.
 */
static int RunSelfTest()
{
    printf("ah-service selftest starting (proto v%u)\n", IPC_PROTOCOL_VERSION);
    fflush(stdout);

    const char*  host   = "127.0.0.1";
    const uint16 port   = 17878;
    const char*  secret = "selftest-secret";

    // --- Start server ---
    IpcServer srv;
    if (!srv.Start(host, port, secret))
    {
        fprintf(stderr, "selftest FAILED: IpcServer::Start failed\n");
        return 1;
    }

    // Give the acceptor a moment to bind before the client connects.
    ACE_Based::Thread::Sleep(50);

    // --- Connect client ---
    IpcClient cli;
    if (!cli.Connect(host, port, secret))
    {
        fprintf(stderr, "selftest FAILED: IpcClient::Connect failed\n");
        srv.Stop();
        return 1;
    }

    // --- Wait for handshake (up to 3 s) ---
    const int maxWaitMs = 3000;
    int waited = 0;
    while (!srv.Connected() || !cli.Connected())
    {
        if (waited >= maxWaitMs)
        {
            fprintf(stderr, "selftest FAILED: handshake timed out (srv=%s cli=%s)\n",
                    srv.Connected() ? "live" : "not-live",
                    cli.Connected() ? "live" : "not-live");
            cli.Stop();
            srv.Stop();
            return 1;
        }
        ACE_Based::Thread::Sleep(20);
        waited += 20;
    }

    printf("selftest: handshake complete in ~%d ms\n", waited);
    fflush(stdout);

    // --- Send IPC_ECHO from server to client ---
    IpcMessage echo;
    echo.op = IPC_ECHO;
    const char* payload = "ping-selftest";
    echo.body.append(reinterpret_cast<const uint8*>(payload), strlen(payload));

    if (!srv.SendFrame(echo))
    {
        fprintf(stderr, "selftest FAILED: srv.SendFrame(IPC_ECHO) failed\n");
        cli.Stop();
        srv.Stop();
        return 1;
    }

    // The client main loop handles IPC_ECHO → IPC_ECHO_REPLY automatically.
    // We need to run it briefly: pump the client inbound queue by running
    // a mini-loop here, then check for IPC_ECHO_REPLY on the server side.

    // Client loop: handle one IPC_ECHO.
    const int echoWaitMs = 2000;
    int echoWaited = 0;
    bool gotReply = false;

    while (!gotReply)
    {
        // Let client process its inbound queue (IPC_ECHO received → send reply).
        IpcMessage clientMsg;
        if (cli.PopInbound(clientMsg))
        {
            if (clientMsg.op == IPC_ECHO)
            {
                // Client side: send IPC_ECHO_REPLY with same body.
                IpcMessage reply;
                reply.op   = IPC_ECHO_REPLY;
                reply.body = clientMsg.body;
                cli.SendFrame(reply);
            }
        }

        // Check server side for the reply.
        IpcMessage srvMsg;
        if (srv.PopInbound(srvMsg))
        {
            if (srvMsg.op == IPC_ECHO_REPLY)
            {
                gotReply = true;
                break;
            }
        }

        if (echoWaited >= echoWaitMs)
        {
            break;
        }

        ACE_Based::Thread::Sleep(20);
        echoWaited += 20;
    }

    cli.Stop();
    srv.Stop();

    if (!gotReply)
    {
        fprintf(stderr, "selftest FAILED: no IPC_ECHO_REPLY after %d ms\n", echoWaitMs);
        return 1;
    }

    printf("ipc selftest OK\n");
    fflush(stdout);
    return 0;
}

// ---------------------------------------------------------------------------
// Normal child loop
// ---------------------------------------------------------------------------

static void PrintUsage(const char* argv0)
{
    fprintf(stderr,
            "Usage: %s --port <port> --secret <secret> [--botguid <guid>] [--config <path>]\n"
            "       %s --selftest\n",
            argv0, argv0);
}

int main(int argc, char** argv)
{
    printf("ah-service (ipc proto v%u) starting\n", IPC_PROTOCOL_VERSION);

    // --- Parse arguments ---
    bool selfTest = false;
    uint16 port   = 0;
    const char* secret  = nullptr;
    // botguid and config are reserved; parsed but not used in M1.

    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--selftest") == 0)
        {
            selfTest = true;
        }
        else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            port = static_cast<uint16>(atoi(argv[++i]));
        }
        else if (strcmp(argv[i], "--secret") == 0 && i + 1 < argc)
        {
            secret = argv[++i];
        }
        else if (strcmp(argv[i], "--botguid") == 0 && i + 1 < argc)
        {
            ++i; // consume; reserved
        }
        else if (strcmp(argv[i], "--config") == 0 && i + 1 < argc)
        {
            ++i; // consume; reserved
        }
    }

    if (selfTest)
    {
        return RunSelfTest();
    }

    if (port == 0 || secret == nullptr)
    {
        PrintUsage(argv[0]);
        return 1;
    }

    // --- Normal mode: connect to mangosd IPC server ---
    IpcClient cli;
    if (!cli.Connect("127.0.0.1", port, secret))
    {
        fprintf(stderr, "ah-service: IpcClient::Connect failed\n");
        return 1;
    }

    printf("ah-service: connecting to mangosd on port %u\n", port);

    // Wait for handshake (up to 10 s).
    const int handshakeTimeoutMs = 10000;
    int waited = 0;
    while (!cli.Connected())
    {
        if (waited >= handshakeTimeoutMs)
        {
            fprintf(stderr, "ah-service: handshake timed out — exiting\n");
            cli.Stop();
            return 1;
        }
        ACE_Based::Thread::Sleep(50);
        waited += 50;
    }

    printf("ah-service: handshake complete — entering service loop\n");

    // --- Service loop ---
    volatile bool stop = false;

    while (!stop)
    {
        IpcMessage msg;
        while (cli.PopInbound(msg))
        {
            switch (msg.op)
            {
                case IPC_HEARTBEAT:
                {
                    IpcMessage ack;
                    ack.op = IPC_HEARTBEAT_ACK;
                    cli.SendFrame(ack);
                    break;
                }
                case IPC_ECHO:
                {
                    IpcMessage reply;
                    reply.op   = IPC_ECHO_REPLY;
                    reply.body = msg.body;
                    cli.SendFrame(reply);
                    break;
                }
                case IPC_SHUTDOWN:
                {
                    printf("ah-service: received IPC_SHUTDOWN — exiting\n");
                    IpcMessage ack;
                    ack.op = IPC_SHUTDOWN_ACK;
                    cli.SendFrame(ack);
                    stop = true;
                    break;
                }
                default:
                    // Unknown opcode — ignore silently for now.
                    break;
            }
        }

        if (!cli.Connected())
        {
            fprintf(stderr, "ah-service: connection lost — exiting\n");
            stop = true;
            break;
        }

        ACE_Based::Thread::Sleep(10);
    }

    cli.Stop();
    printf("ah-service: shutdown complete\n");
    return 0;
}
