# Realmd Authentication Stream Safety Design

## Purpose

Resolve audit findings F-01 and F-02 in the shared multi-client realmd
application, and correct the directly adjacent F-10 header-read defect, without
changing mangosd or the shared networking backends.

The implementation must:

- close unknown, unauthorized, malformed, and protocol-failed auth streams;
- retain incomplete TCP input without consuming any part of its current frame;
- bound pending auth input before allocating or appending it;
- expire authentication handshakes that never complete;
- preserve the existing packet layouts and handler behavior for every client
  family served by realmd;
- leave authenticated realm-list use and patch transfers outside the handshake
  deadline.

## Baseline

- Parent server branch base: `91dd67482a002c6d44538ef9d1275ff9f56ea37b`
- Zero-compatible realmd base:
  `3cf5f9690477a0bc2b33eb17eb1d9016da1d47e8`
- Realmd GitHub default branch: `master`
- Realmd GitHub sweep on 2026-07-23 found PRs #27, #28, and #29 merged,
  no open pull requests, and `39b7846` as the latest branch tip.
- Build verification found that `39b7846` assumes split shared headers from
  realmd PR #27 which do not exist in mangoszero/server, including its current
  upstream `master`. The server continues to pin `3cf5f96`, which contains the
  same provider-exit fix as PR #29 without the incompatible header split.
- Build directory: `E:\Mangos\WIP\Zero\Testing\server_build`
- Install directory: `E:\Mangos\WIP\Zero\Testing\server_install`
- CMake generator: Visual Studio 18 2026
- `BUILD_TESTING=ON`

## Scope

### Included

- `src/realmd/Auth/AuthSocket.*`
- `src/realmd/Auth/AuthServer.*`
- a small protocol guard owned by `src/realmd/Auth/`
- deterministic regression tests owned by the realmd submodule
- `src/realmd/Main.cpp`
- `src/realmd/realmd.conf.dist.in`
- realmd CMake test registration
- build, install, raw-socket regression, and available-client smoke validation

### Excluded

- mangosd handlers and world-session behavior
- IOCP, reactor, and io_uring transport APIs
- realm-list concurrency remediation (F-03)
- extended proof/PIN parsing (F-08/F-09)
- patch immutability remediation (F-06)
- changing accepted-build policy (F-15)
- database schema changes

## Protocol Guard

Add a pure `AuthProtocolGuard` that inspects the pending span before
`AuthSocket` invokes an existing command handler.

The result has three states:

- `Incomplete`: keep the complete pending span unchanged and await more TCP
  bytes;
- `Complete`: identify the complete current frame and allow its existing
  handler to run;
- `Reject`: close the connection without retaining attacker-controlled input.

The guard uses command and authentication state, not client build, so its
behavior is shared by all supported client generations.

For logon and reconnect challenges:

1. Require four bytes before reading the header.
2. Decode the two-byte little-endian declared body length from bytes 2 and 3
   without consuming or using an unaligned pointer.
3. Reject a declared body shorter than the fixed challenge body.
4. Return `Incomplete` until all `4 + declared_body_length` bytes are present.
5. Dispatch only after the complete frame exists.

For fixed-size commands, the guard requires the packet size already consumed by
the current handlers:

- logon proof;
- reconnect proof;
- realm list;
- patch accept;
- patch resume;
- patch cancel.

Static assertions beside the packed wire structures keep the guard constants
equal to the compiled structures.

This preflight removes legitimate short-read failures from the handlers.
Therefore an existing handler returning `false` after preflight is treated as a
protocol or authentication failure and closes the connection.

## Pending-Input Bound

Before appending new bytes, `AuthSocket` checks addition overflow and a hard
pending-input ceiling of 65,539 bytes:

```text
4-byte challenge header + maximum uint16 declared body
```

The append is rejected before allocation when the resulting pending size would
exceed the ceiling. This bounds the worst retained unauthenticated payload while
preserving coalesced TCP frames and the full declared-length domain already
accepted by realmd.

Unknown commands, known commands sent in the wrong state, malformed frames,
ceiling violations, and handler failures all:

1. log the reason without dumping credentials or verifier material;
2. discard pending input;
3. call the thread-safe connection closer once.

## Authentication Deadline

Add:

```ini
AuthSessionTimeout = 30
```

The value is seconds. `0` explicitly disables the deadline; the distributed
configuration and normal default remain 30 seconds.

`AuthSocket` records a steady-clock deadline at construction. An atomic flag
states whether the deadline is active. It is active only during:

- initial challenge;
- logon proof;
- reconnect proof.

It is deactivated when the socket:

- authenticates;
- enters patch-transfer command state;
- closes.

`AuthServer` keeps weak references to sockets created by its session factory.
The existing 100 ms realmd housekeeping loop calls `AuthServer::Update()`.
`Update()` removes expired weak references and asks live sockets to close when
their active authentication deadline has elapsed.

The session closer is already documented as thread-safe, so this design needs
no transport change and no additional watchdog thread.

## Multi-Client Compatibility

No framing decision depends on `_build`. The guard validates only the command,
current protocol state, and existing wire-frame length. The existing handlers
continue selecting response layouts by build.

The accepted-build validation matrix is:

| Family | Builds |
| --- | --- |
| Vanilla | 5875, 6005, 6141 |
| The Burning Crusade | 8606 |
| Wrath of the Lich King | 12340 |
| Cataclysm | 15595 |
| Mists of Pandaria | 18273, 18414 |
| Warlords of Draenor | 21742 |
| Legion | 26972 |
| Battle for Azeroth | 35662 |
| Shadowlands table entry | 40000 |

For every installed client family, smoke validation must perform an actual
account login and retrieve the realm list. Build 5875 must additionally reach a
populated character-selection screen through mangosd.

When an installation is not locally available, a packet-level regression must
send that build's challenge/proof framing and verify that the new guard does not
reject it. Such a result is recorded as packet compatibility, not as a claimed
end-to-end client smoke pass.

## Testing

### Deterministic unit tests

Add a focused realmd CTest executable that exercises production guard code:

- every split point of valid logon and reconnect challenges;
- four-byte-header-only and packed-minimum/body split cases;
- unchanged retained bytes for every incomplete case;
- malformed declared sizes;
- every fixed-size command one byte short and complete;
- unknown commands;
- commands valid in another state;
- pending-size overflow and integer-overflow-safe append checks;
- deadline before, at, and after expiry;
- deadline deactivation for authenticated and patch states.

Tests must be written and observed failing before production implementation.

### Built loopback regression

Run the installed realmd on loopback with a validation configuration and a
short timeout:

- send an invalid first byte followed by repeated chunks and observe prompt
  EOF while realmd remains alive;
- send valid challenge packets at every split point and compare the response to
  an unsplit request;
- connect without sending a complete handshake and observe timeout closure;
- send coalesced valid frames where the protocol state permits them.

### Build and installation

Use the existing configured build:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build --config RelWithDebInfo --target realmd_auth_protocol_tests realmd
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build -C RelWithDebInfo -R realmd_auth_protocol_tests --output-on-failure
cmake --install E:\Mangos\WIP\Zero\Testing\server_build --config RelWithDebInfo
```

Run the full configured CTest suite once at phase end.

## Repository Ownership

Production and unit-test changes are committed inside the realmd submodule on
`fix/auth-stream-safety`, based on the server-compatible `3cf5f96`.

The parent branch `fix/realmd-auth-stream-safety` records:

- this design and implementation plan;
- the tested realmd submodule pointer;
- validation evidence that is appropriate for the repository.

No changes are made to mangosd source, database repositories, client files, or
the original source checkout outside `E:\Mangos\WIP\Zero\Testing`.

## Completion Criteria

- F-01 invalid-command retention is no longer reproducible.
- Pending input is bounded before append.
- Incomplete legal TCP frames remain byte-for-byte intact.
- F-02 succeeds at every challenge split point.
- F-10 no longer forms an invalid reference from the command byte.
- Authentication timeout closes idle handshakes and exempts authenticated and
  patch-transfer states.
- Focused and full tests pass.
- Realmd builds and installs into `server_install`.
- Available-client smoke tests pass and unavailable-client coverage is labelled
  accurately.
- An independent correctness/security/compatibility review reports no blocking
  or important unresolved finding.
