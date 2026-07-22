# ACE Removal Hardening Design

## Goal

Correct the confirmed regressions left by the ACE removal in PRs #448 and #452
without redesigning the new networking layer. Preserve existing protocol behavior,
support OpenSSL 3.x only for this release, and add focused regression coverage for
the affected authentication and transport paths.

## Scope

This change covers:

- enforcing OpenSSL versions `>=3.0.0` and `<4.0.0`;
- detecting a legacy-provider/library major-version mismatch at startup;
- draining final IOCP response bytes before closing a connection;
- draining cancelled IOCP operations before destroying their contexts;
- cleaning up Reactor connections whose poller registration fails;
- recording peer addresses in the io_uring backend;
- preventing `WorldSession` deletion while the network thread is using it; and
- adding fixed-width BigNumber and legacy-authentication regression vectors.

Replacing OpenSSL with LibTomCrypt/LibTomMath, supporting OpenSSL 4.x, and
restructuring all backends around a new shared state machine are explicitly out of
scope. OpenSSL 4 support will be reconsidered with the OpenSSL 4.2 LTS migration.

## OpenSSL Policy

The existing minimum remains OpenSSL 3.0. CMake will reject any discovered version
greater than or equal to 4.0.0 immediately after discovery. The upper-bound check
will have an adjacent comment stating that OpenSSL 4.x is intentionally deferred
until the OpenSSL 4.2 LTS migration.

The provider manager will query the loaded legacy provider's version and compare
its major version with the linked OpenSSL library. A mismatch is fatal and reports
the library version, provider version, and the effective `OPENSSL_MODULES` value so
an administrator can correct the provider path. The server must never continue
running with mixed provider and library majors.

## IOCP Connection Lifecycle

Session-requested closure is graceful. A close request marks the connection as
close-after-drain and wakes an IOCP worker. If no output is queued, teardown begins
immediately. Otherwise, the active send completes, including short-write retries,
and teardown begins only after the send queue becomes empty. Transport errors and
server shutdown remain abortive because delivery is no longer possible or no
longer required.

Every submitted AcceptEx, receive, send, and control operation is counted. Shutdown
uses this order:

1. stop accepting and prevent new operations from being submitted;
2. close the listener and mark all live connections dead while workers still run;
3. allow cancellation completions to drain and release their operation references;
4. wait until the outstanding-operation count reaches zero;
5. post worker shutdown completions and join the workers; and
6. close the completion port and release Winsock.

This preserves every `OVERLAPPED` and connection context until Windows has returned
the corresponding completion. Accept contexts follow the same accounting and are
freed by their completion path.

## Reactor and io_uring Corrections

If a Reactor worker cannot add a handed-off connection to its poller, it will
disarm the outbound channel and invoke `session->onClose()` before closing the file
descriptor and deleting the connection. This matches normal Reactor teardown and
prevents later session callbacks from reaching freed state.

The io_uring accept loop will provide a `sockaddr_in` buffer to `accept4()`, convert
the accepted IPv4 address with `inet_ntop()`, and call `setPeerAddress()` before
`onConnect()`. Authentication, IP locking, bans, and logging therefore receive the
same address information as the Reactor and IOCP backends.

## WorldSession Lifetime

`WorldSocket` will expose a scoped session guard rather than returning an
unprotected raw pointer to the network thread. The guard holds the socket's session
mutex for the complete duration of each network-side use. Clearing the session in
`CloseSocket()` therefore waits for active network uses to finish before the world
thread can complete `WorldSession` destruction.

The guarded regions are limited to packet enqueueing, ping/session field access,
Eluna packet callbacks, and exception logging. Authentication, which starts with no
session and installs a newly allocated one, remains outside an existing-session
guard. The mutex must permit safe same-thread re-entry if a guarded callback closes
its socket.

## Regression Testing

Tests will be written before each production correction and must demonstrate the
previous failure where practical.

Authentication coverage includes:

- fixed-width BigNumber output with leading zero padding in both byte orders;
- zero and already-full-width BigNumber values;
- known SHA-1, HMAC-SHA1, and RC4 vectors; and
- provider and linked-library major-version agreement.

Transport coverage includes:

- a loopback session that queues a final response and immediately requests close,
  asserting that the client receives all bytes before EOF;
- IOCP shutdown with pending accepts and receives, repeated to expose hangs or
  lifetime errors;
- peer-address delivery before `onConnect()` on supported backends; and
- cleanup notification when Reactor poller registration is deliberately rejected.

After targeted tests pass, validation consists of the configured RelWithDebInfo
build, inspection for compiler/linker errors, the AH-service self-test, daemon
startup smoke tests, and dependency inspection confirming that Windows daemons
import only `libcrypto-3-x64.dll`.

## Error Handling and Compatibility

OpenSSL 4.x configuration fails with an actionable CMake error rather than creating
an unsupported binary. A provider mismatch fails daemon startup before network or
database service begins. Network teardown remains idempotent so simultaneous peer,
session, and shutdown close paths cannot perform cleanup twice.

No packet formats, database schemas, configuration keys, or public gameplay
behavior change. The io_uring correction restores the peer-address behavior already
provided by the other backends.
