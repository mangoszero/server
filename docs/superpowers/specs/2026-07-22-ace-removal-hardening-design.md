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
its major version with the runtime OpenSSL library version. A mismatch is fatal and
reports the library version, provider version, and the effective `OPENSSL_MODULES`
value so an administrator can correct the provider path. Startup must load both the
default and legacy providers and successfully fetch RC4 before initialization is
declared complete. The server must never continue running with mixed provider and
library majors or defer a missing-provider failure until player authentication.

## IOCP Connection Lifecycle

Session-requested closure is graceful. A close request marks the connection as
close-after-drain and wakes an IOCP worker. If no output is queued, teardown begins
immediately. Otherwise, the active send completes, including short-write retries,
and teardown begins only after the send queue becomes empty. Transport errors and
server shutdown remain abortive because delivery is no longer possible or no
longer required.

Send enqueue, the transition to close-after-drain, and the final empty-queue check
are serialized by the same channel-state lock. Once close-after-drain begins, later
cross-thread sends are rejected atomically. Bytes accepted before that transition
remain queued and must drain before teardown. This removes the queue-empty/check
race without relying on the unrelated WorldSession lifetime lock.

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

The accounting invariant applies to every initial submission and resubmission.
Submission increments before entering Winsock; a synchronous error other than
`WSA_IO_PENDING` decrements immediately because no completion will follow; and a
dequeued completion decrements exactly once. Beginning shutdown closes or cancels
the listener and every live socket so pending operations generate cancellation
completions. No receive, send, accept, or control path may resubmit after shutdown
begins. Worker-shutdown sentinel packets are outside operation accounting, and a
condition variable signals the transition to zero outstanding operations.

## Reactor and io_uring Corrections

If a Reactor worker cannot add a handed-off connection to its poller, it will
disarm the outbound channel and invoke `session->onClose()` before closing the file
descriptor and deleting the connection. This matches normal Reactor teardown and
prevents later session callbacks from reaching freed state.

The io_uring accept loop will provide an initialized `sockaddr_in` buffer and length
to `accept4()`, verify `AF_INET`, convert the accepted IPv4 address with
`inet_ntop()`, and call `setPeerAddress()` before `onConnect()`. All current
listeners are deliberately IPv4-only. Authentication, IP locking, bans, and logging
therefore receive the same address information as the Reactor and IOCP backends.

## WorldSession Lifetime

`WorldSocket` will expose a move-only session lease rather than returning an
unprotected raw pointer to the network thread. Acquiring a lease verifies the
current pointer and increments an active-use count while holding the socket's short
state mutex; releasing it decrements the count and signals waiters. The mutex is not
held while calling `WorldSession`, Eluna, or other world-side code.

`CloseSocket()` marks the socket closed and detaches the published session pointer
under the state mutex, preventing new leases, but does not wait. This permits an
Eluna callback holding a lease to close its own socket without self-deadlocking.
Before every `WorldSession` deletion, the world thread performs a detach-and-wait on
the same socket state, then deletes only after the active-use count reaches zero.
All existing deletion sites use this path, including world shutdown and queued
sessions. Publishing a newly authenticated session uses the same mutex.

Network code must not cache a lease across a close-capable callback and then keep
using the logically closed session. It releases or rechecks the lease after such a
callback. Authentication, which starts with no published session and installs a
newly allocated one, remains outside an existing-session lease.

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
- a deterministic cross-thread enqueue racing the drain-to-close transition,
  proving pre-close bytes drain and post-close sends are rejected;
- IOCP shutdown with pending accepts and receives, repeated to expose hangs or
  lifetime errors;
- deterministic synchronous Winsock-submission failure accounting where the test
  seam permits it;
- peer-address delivery before `onConnect()` on supported backends; and
- cleanup notification when Reactor poller registration is deliberately rejected;
  and
- a concurrent session lease/use versus world-thread detach-and-delete test,
  including a close-capable callback that does not run under the state mutex.

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
