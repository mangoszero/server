# Realmd Auth Stream Safety Validation

Date: 2026-07-23

## Result

F-01, F-02, and the adjacent F-10 endian defect are fixed in the realmd
submodule. The installed Testing realmd now:

- rejects unknown, unauthorized, malformed, and handler-failed auth streams;
- caps pending unauthenticated input at the largest representable challenge
  frame;
- waits for a complete frame before any handler consumes bytes;
- closes incomplete authentication after a configurable deadline;
- exempts authenticated realm-list sessions and patch transfers from that
  deadline;
- flushes buffered debug records about once per second while realmd remains
  active.

No mangosd production source, shared network backend, database, protected
source checkout, or client file was changed.

## Revisions

- Parent branch: `fix/realmd-auth-stream-safety`
- Parent implementation commit: `3526737067c418c422ac33b8ef7286e72c749385`
- Parent live-log regression commit: `dd046df57ca0ecff1428f059381feb5d2981fc2c`
- Realmd branch: `fix/auth-stream-safety`
- Realmd implementation tip: `a571d214aac54491c89375a4a0645a782ecc1fd8`
- Realmd-compatible base: `3cf5f9690477a0bc2b33eb17eb1d9016da1d47e8`

The GitHub sweep found realmd default-branch tip `39b7846`, with PRs #27,
#28, and #29 merged and no open pull requests. A build against Zero proved
that tip is not currently parent-compatible: realmd PR #27 includes
`Common/Locales.h` and `Common/TimeConstants.h`, neither of which exists in
mangoszero/server, including current upstream `master`. Zero continues to pin
`3cf5f96`, which carries the same provider-exit fix as PR #29 without that
header split. The hardening branch was rebased to the compatible commit before
production changes continued.

## RED Evidence

The raw-socket test was first run against the pre-change installed realmd:

```text
Realmd auth-stream smoke failures:
 - unknown command input was retained instead of closing promptly
 - fragmented build-5875 challenge did not match the complete-packet response
 - idle unauthenticated connection was not closed by AuthSessionTimeout
```

The protocol-guard test was also introduced before its implementation; CMake
configuration failed because `Auth/AuthProtocolGuard.cpp` did not yet exist,
proving the new test target was wired to the intended production component.

The live-log assertion was then run against the pre-flush installed binary.
Authentication still succeeded, but the smoke test failed with:

```text
debug auth records were not flushed to the live log within 3 seconds
```

The shared logger uses a 64 KiB full buffer. Mangosd flushes it periodically,
but realmd had no corresponding call, so normal and debug records became
visible only when the buffer filled or `fclose()` ran during shutdown.

## Automated GREEN Evidence

Focused guard test:

```powershell
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R realmd_auth_protocol_tests --output-on-failure
```

Result: `1/1` passed.

Phase-end build:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo
```

Result: success, including `realmd`, `mangosd`, tools, modules, and test
targets.

Phase-end CTest:

```powershell
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo --output-on-failure
```

Result: `4/4` passed:

- `realmd_auth_protocol_tests`
- `auth_crypto_tests`
- `lease_tests`
- `network_regression_tests`

Installed-runtime test:

```powershell
E:\Mangos\WIP\Zero\Testing\server\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe `
  -ConfigPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.conf `
  -Port 43724 -TimeoutSeconds 2
```

Result:

```text
Realmd auth-stream smoke passed: prompt reject, fragmented challenge,
live debug log, idle timeout, process survival.
```

The runtime test used a loopback-only temporary configuration, tested every
split of the 40-byte build-5875 challenge, verified an auth debug record reached
disk while realmd was still active, stopped only its tracked realmd PID, and
removed only its validated temporary directory.

Both repository diff checks passed:

```powershell
git -C src/realmd diff --check 3cf5f96..HEAD
git diff --check 2318449c..HEAD
```

## Multi-client Compatibility

The framing guard is build-independent. Its unit test exercises challenge
boundaries for every accepted realmd build:

```text
5875, 6005, 6141, 8606, 12340, 15595,
18273, 18414, 21742, 26972, 35662, 40000
```

Executable version metadata confirms these installed clients:

| Build | Version | Executable present | Automated evidence | Interactive UI login |
|---:|---|---|---|---|
| 5875 | 1.12.1 | Yes | Guard test and every-split installed-realmd smoke PASS | PASS through authentication and realm list |
| 8606 | 2.4.3 | Yes | Guard framing PASS | PASS through authentication and realm list |
| 12340 | 3.3.5 | Yes | Guard framing PASS | PASS through authentication and realm list |
| 15595 | 4.3.4 | Yes | Guard framing PASS | PASS through authentication and realm list |
| 18414 | 5.4.8 | Yes | Guard framing PASS | Not run |

Accepted builds `6005`, `6141`, `18273`, `21742`, `26972`, `35662`, and
`40000` do not have a matching installed executable in the client inventory;
their result is packet-framing PASS only, not an end-to-end client PASS. The
installed WoD executable is build `20886`, not accepted build `21742`.

The user manually authenticated the installed Classic, TBC, WotLK, and Cata
clients against the installed realmd and received their realm lists. The world
servers were deliberately not running, so these results stop at the realmd
boundary and do not claim realm entry or character selection. The debug log
records successful challenge, proof, and realm-list exchanges for the manual
test window without malformed-frame, unauthorized-command, timeout, or
consumption errors.

## Repository and Deployment Boundaries

- Protected source `E:\Mangos\Repos\Zero\server` remains clean.
- Testing parent and realmd submodule are clean on their feature branches.
- Current binaries and distribution configuration were installed to
  `E:\Mangos\WIP\Zero\Testing\server_install`.
- `realmd.conf.dist` documents `AuthSessionTimeout = 30`; zero disables it.
- Existing `realmd.conf` was not overwritten. The runtime test injects the
  setting only into its temporary configuration.

## Remaining Work

The next realmd BLOCKING finding is F-03: realm refresh can race concurrent
realm-list serialization. It should be handled as a separate change with
snapshot or reader/writer synchronization and concurrent refresh coverage.

Credentialed realm-entry and character-selection checks require matching world
servers and remain manual validation steps for every expansion. MoP also still
requires a manual realmd authentication and realm-list pass. These are not
correctness blockers for the build-independent framing, deadline, or logging
implementation.

## Cross-model Review

Claude Opus 4.8 performed the required read-only review after targeted tests.
Its initial verdict was `APPROVE WITH FOLLOW-UP`, with no blocking findings.
The review requested:

- direct alignment between the guard constants and realm-list/XFER handler
  consumption;
- confirmation that the constructor initializer order is warning-safe;
- confirmation that watchdog closure is supported across the network backends.

The handler consumption was changed to use the guard's exact frame constants.
Declaration inspection confirmed the initializer list follows member declaration
order. IOCP, reactor, and io_uring inspection confirmed that `requestClose`
serializes its state and transfers closure to the owning worker; captured shared
channel ownership protects the handoff.

The permitted focused re-review returned `APPROVE`: all three important
follow-ups were resolved, with no blocking or important findings. The only
minor note was to confirm the committed diff includes the constant-consumption
edits; commit `0243671` does.

Review ledgers (outside the repository):

- Initial review:
  `C:\Users\Mark\AppData\Local\Temp\realmd-auth-review-20260723-173117.jsonl`
- Focused re-review:
  `C:\Users\Mark\AppData\Local\Temp\realmd-auth-rereview-20260723.jsonl`

The subsequent live-log change is a small lifecycle integration using the
existing thread-safe `Log::Flush()` API. Per repository policy, this local
low-risk follow-up did not require another external review; it was verified by
an explicit failing-then-passing installed-runtime regression.
