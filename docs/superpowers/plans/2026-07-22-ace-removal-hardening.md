# ACE Removal Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct the confirmed ACE-removal regressions while enforcing the approved OpenSSL `>=3.0.0,<4.0.0` policy and preserving existing protocol behavior.

**Architecture:** Keep the new backend split intact. Add focused state/lifetime controls at the existing seams: CMake and provider startup for OpenSSL, IOCP channel and operation lifecycle for Windows networking, existing teardown hooks for Reactor/io_uring, and a small reusable leased-pointer primitive behind `WorldSocket` for `WorldSession` lifetime.

**Tech Stack:** C++17, CMake/CTest, OpenSSL 3.x EVP/provider APIs, Winsock IOCP, POSIX Reactor, Linux io_uring, Visual Studio 18 2026.

## Global Constraints

- Work in `E:\Mangos\WIP\Zero\Testing\server`; use `E:\Mangos\WIP\Zero\Testing\server_build` and install to `E:\Mangos\WIP\Zero\Testing\server_install`.
- Preserve packet formats, database schemas, configuration keys, and gameplay behavior.
- Accept OpenSSL 3.x only. Reject OpenSSL 4.x at configure time and leave the adjacent comment requested by the project lead: support is deferred until the OpenSSL 4.2 LTS migration.
- Never mix provider and runtime library majors. Startup succeeds only after both providers load, their majors agree, and RC4 can be fetched.
- Keep changes local to the confirmed regressions; do not replace OpenSSL, redesign all network backends, or fold this work into unrelated cleanup.
- Write each regression test first, run it to demonstrate the failure where the host platform supports that backend, then make the smallest production change that makes it pass.
- Run the full RelWithDebInfo build, install, daemon smoke tests, dependency inspection, and one bounded read-only Claude review only after targeted tests pass.

---

### Task 1: Add the focused test harness

**Files:**

- Modify: `CMakeLists.txt`
- Create: `tests/CMakeLists.txt`
- Create: `tests/TestSupport.hpp`
- Create: `tests/AuthCryptoTests.cpp`
- Create: `tests/LeaseTests.cpp`
- Create: `tests/NetworkRegressionTests.cpp`

- [ ] **Step 1: Add an opt-in CTest entry point**

Add `BUILD_TESTING` beside the existing root options, defaulting to `OFF`, and add this after `add_subdirectory(src)`:

```cmake
option(BUILD_TESTING "Build focused regression tests" OFF)

# after src has defined shared/game
if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()
```

Keep tests out of ordinary release builds unless explicitly enabled.

- [ ] **Step 2: Define three narrow executables**

Use this target layout in `tests/CMakeLists.txt`:

```cmake
add_executable(auth_crypto_tests AuthCryptoTests.cpp)
target_link_libraries(auth_crypto_tests PRIVATE shared mangos_openssl_strict)
add_test(NAME auth_crypto_tests COMMAND auth_crypto_tests)

add_executable(lease_tests LeaseTests.cpp)
target_include_directories(lease_tests PRIVATE "${PROJECT_SOURCE_DIR}/src/shared")
add_test(NAME lease_tests COMMAND lease_tests)

add_executable(network_regression_tests NetworkRegressionTests.cpp)
target_link_libraries(network_regression_tests PRIVATE shared)
add_test(NAME network_regression_tests COMMAND network_regression_tests)
set_tests_properties(network_regression_tests PROPERTIES TIMEOUT 60)
```

Guard backend-specific cases inside the source with `_WIN32`, `MANGOS_USE_IO_URING`, and the existing POSIX conditions; an unsupported backend prints a skip line but does not hide platform-independent tests.

- [ ] **Step 3: Add minimal assertion support**

`tests/TestSupport.hpp` should provide a failure counter, byte comparison, and a `CHECK` macro which prints file, line, expression, expected bytes, and actual bytes. Each executable returns non-zero if its counter is non-zero. Do not introduce a third-party test framework.

- [ ] **Step 4: Configure the targeted build**

Run:

```powershell
cmake -S E:\Mangos\WIP\Zero\Testing\server `
  -B E:\Mangos\WIP\Zero\Testing\server_build `
  -DBUILD_TESTING=ON `
  -DOPENSSL_ROOT_DIR=C:\OpenSSL-Win64
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target auth_crypto_tests lease_tests network_regression_tests --parallel
```

Expected: configure and the empty/skeleton test targets build against the OpenSSL 3.x installation.

- [ ] **Step 5: Commit the harness**

```powershell
git add CMakeLists.txt tests
git commit -m "test: add ACE removal regression harness"
```

---

### Task 2: Enforce OpenSSL 3.x and validate legacy authentication

**Files:**

- Modify: `CMakeLists.txt:148`
- Modify: `src/shared/Auth/OpenSSLProvider.h`
- Modify: `src/shared/Auth/OpenSSLProvider.cpp:82-178`
- Test: `tests/AuthCryptoTests.cpp`

- [ ] **Step 1: Write fixed authentication vectors**

Add these exact cases before changing production code:

```cpp
// BigNumber wire order: false is padded big-endian; true/default is little-endian.
BigNumber n;
n.SetHexStr("1234");
CHECK_BYTES(n.AsByteArray(4, false), 4, {0x00, 0x00, 0x12, 0x34});
CHECK_BYTES(n.AsByteArray(4, true),  4, {0x34, 0x12, 0x00, 0x00});
CHECK_BYTES(n.AsByteArray(2, false), 2, {0x12, 0x34});
CHECK_BYTES(n.AsByteArray(2, true),  2, {0x34, 0x12});

BigNumber zero;
CHECK_BYTES(zero.AsByteArray(4, false), 4, {0x00, 0x00, 0x00, 0x00});
CHECK_BYTES(zero.AsByteArray(4, true),  4, {0x00, 0x00, 0x00, 0x00});

Sha1Hash sha1;
sha1.UpdateData(std::string("abc"));
sha1.Finalize();
CHECK_HEX(sha1.GetDigest(), sha1.GetLength(),
          "a9993e364706816aba3e25717850c26c9cd0d89d");

std::array<uint8, 20> hmacKey{};
hmacKey.fill(0x0b);
HMACSHA1 hmac(static_cast<uint32>(hmacKey.size()), hmacKey.data());
hmac.UpdateData(std::string("Hi There"));
hmac.Finalize();
CHECK_HEX(hmac.GetDigest(), hmac.GetLength(),
          "b617318655057264e28bc0b6fb378c8ef146be00");

uint8 rc4Key[] = {'K', 'e', 'y'};
uint8 rc4Data[] = {'P', 'l', 'a', 'i', 'n', 't', 'e', 'x', 't'};
ARC4 rc4(rc4Key, sizeof(rc4Key));
rc4.UpdateData(sizeof(rc4Data), rc4Data);
CHECK_HEX(rc4Data, sizeof(rc4Data), "bbf316e8d940af0ad3");
```

Construct `OpenSSLProviderManager` before the RC4 case and assert `IsInitialized()`. Also query `GetLegacyProvider().Version()` and compare its parsed major with `(OpenSSL_version_num() >> 28) & 0x0f`.

- [ ] **Step 2: Run the tests red in the known mixed environment**

First run with the 4.0.1 runtime and the existing 3.x `OPENSSL_MODULES` path:

```powershell
$env:OPENSSL_MODULES='C:\OpenSSL-Win64\bin'
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R auth_crypto_tests --output-on-failure
```

Expected before the startup validation: the explicit major-agreement assertion exposes the mixed runtime/provider installation, or RC4 initialization fails. Record which assertion fails; do not copy both DLL majors into the test directory.

- [ ] **Step 3: Add the configure-time upper bound and requested comment**

Immediately after `find_package(OpenSSL 3.0 REQUIRED)` add:

```cmake
# OpenSSL 4.x support is intentionally deferred until the OpenSSL 4.2 LTS migration.
if(OPENSSL_VERSION VERSION_GREATER_EQUAL "4.0.0")
  message(FATAL_ERROR
    "Unsupported OpenSSL ${OPENSSL_VERSION}: MaNGOS currently requires >=3.0.0 and <4.0.0")
endif()
```

Verify both sides using separate build directories so the working build cache is not contaminated:

```powershell
cmake -S E:\Mangos\WIP\Zero\Testing\server `
  -B E:\Mangos\WIP\Zero\Testing\openssl4-reject `
  -DOPENSSL_ROOT_DIR=C:\OpenSSL-Win64-401
```

Expected: configure fails with the new actionable `<4.0.0` message.

```powershell
cmake -S E:\Mangos\WIP\Zero\Testing\server `
  -B E:\Mangos\WIP\Zero\Testing\openssl3-accept `
  -DOPENSSL_ROOT_DIR=C:\OpenSSL-Win64 `
  -DBUILD_MANGOSD=OFF -DBUILD_REALMD=OFF -DBUILD_TOOLS=OFF
```

Expected: OpenSSL version validation succeeds.

- [ ] **Step 4: Expose and validate provider version information**

Add this non-owning query to `OpenSSLProvider`:

```cpp
std::string Version() const;
```

Implement it with `OSSL_PROVIDER_get_params`, `OSSL_PROV_PARAM_VERSION`, and an `OSSL_PARAM_construct_utf8_ptr` parameter. Return an empty string if the handle or version query is unavailable.

In `OpenSSLProviderManager`, after both providers load:

1. Read `OpenSSL_version_num()` and `OpenSSL_version(OPENSSL_VERSION)`.
2. Parse the leading decimal component of `m_legacyProvider.Version()` with `std::from_chars`; require the whole major component to parse and require it to equal the runtime major.
3. On mismatch or an unreadable version, log the runtime string, provider version, and `std::getenv("OPENSSL_MODULES")` (printing `<unset>` when null), leave `m_initialized == false`, and return.
4. Fetch `RC4` with `EVP_CIPHER_fetch(nullptr, "RC4", nullptr)`, fail initialization if null, and free it immediately after the probe.
5. Set `m_initialized = true` only after all checks pass.

Do not log “OpenSSL 3.x providers loaded successfully” before the version and RC4 probes pass.

- [ ] **Step 5: Rebuild and run green with one OpenSSL 3.x installation**

```powershell
$env:OPENSSL_MODULES='C:\OpenSSL-Win64\bin'
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target auth_crypto_tests --parallel
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R auth_crypto_tests --output-on-failure
```

Expected: BigNumber, SHA-1, HMAC-SHA1, RC4, provider-load, and major-agreement cases all pass using only OpenSSL 3.x.

- [ ] **Step 6: Commit the policy and authentication coverage**

```powershell
git add CMakeLists.txt src/shared/Auth/OpenSSLProvider.h src/shared/Auth/OpenSSLProvider.cpp tests/AuthCryptoTests.cpp
git commit -m "fix: enforce OpenSSL 3 and validate legacy crypto"
```

---

### Task 3: Make IOCP close-after-drain atomic

**Files:**

- Modify: `src/shared/net/iocp/IocpServer.hpp:54-140`
- Modify: `src/shared/net/iocp/IocpServer.cpp:41-113,239-408`
- Test: `tests/NetworkRegressionTests.cpp`

- [ ] **Step 1: Write the Windows loopback regressions**

Create a small `ISession` fixture that records its `Sender` and `Closer`. For the final-response case, its `onData()` must perform the same order as `WorldSocket`: call `sender(finalBytes)`, then `closer()`, then return `{}`. Connect a Winsock loopback client, send one byte, read until EOF, and assert the exact final payload arrived before EOF.

For the deterministic race case, coordinate a producer and close requester with `std::promise/std::future`:

1. producer sends `PRE` and signals that the call returned;
2. closer requests close and signals that the transition returned;
3. producer attempts `POST`;
4. client reads through EOF and must receive exactly `PRE`.

Use a helper that reserves an ephemeral IPv4 loopback port by binding a temporary socket to port zero, reading `getsockname()`, closing it, and immediately starting `IocpServer` on that port. Retry the reserve/start sequence up to five times to avoid a transient port race.

- [ ] **Step 2: Run the final-response case red**

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target network_regression_tests --parallel
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R network_regression_tests --output-on-failure
```

Expected before the fix: the current `SendChannel::requestClose()` closes the socket while queued output is still in flight, so the final-response/race assertion fails intermittently or consistently under repetition.

- [ ] **Step 3: Add one counted IOCP control operation per close transition**

Extend `IoType` with `Control`, add `ControlOv`, and add one `ControlOv closeOv` plus `std::atomic<bool> controlPending{false}` to `ConnCtx`. Give `ConnCtx` a pointer/reference to its owning `IocpServer` so it can post the control completion without exposing the completion port publicly.

Change `SendChannel` state to:

```cpp
std::mutex mu;
ConnCtx* ctx = nullptr;
bool closeRequested = false;
SendQueue out;
```

Under `mu`, `post()` must reject data when `ctx == nullptr || closeRequested`; otherwise it appends and starts the send before releasing the same state lock. Under the same `mu`, `requestClose()` changes `closeRequested` exactly once and asks the owner to post the counted control operation. `disarm()` nulls `ctx`, keeps `closeRequested` true, and closes the flow gate.

- [ ] **Step 4: Drain only bytes accepted before close**

Add `handleControl(ConnCtx*)` and a helper which, while holding `channel->mu`, tests `closeRequested && channel->out.empty()`. If true, release the channel lock and call idempotent `markDead(ctx)`. If false, leave the active send to drain.

After every send completion, consume the reported byte count, re-post a short-write remainder when present, and repeat the same locked close-and-empty decision. Never accept a new cross-thread send after the close transition.

Transport error and shutdown paths remain abortive and go straight to `markDead`.

- [ ] **Step 5: Run the close tests repeatedly**

Make the test executable repeat both IOCP close cases 100 times in-process, then run:

```powershell
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R network_regression_tests --repeat until-fail:5 --output-on-failure
```

Expected: every client receives all pre-close bytes, no client receives post-close bytes, all connections reach EOF, and the test exits without a hang.

- [ ] **Step 6: Commit graceful IOCP close**

```powershell
git add src/shared/net/iocp/IocpServer.hpp src/shared/net/iocp/IocpServer.cpp tests/NetworkRegressionTests.cpp
git commit -m "fix: drain IOCP output before session close"
```

---

### Task 4: Drain all IOCP operations before teardown

**Files:**

- Modify: `src/shared/net/iocp/IocpServer.hpp:54-180`
- Modify: `src/shared/net/iocp/IocpServer.cpp:63-279`
- Test: `tests/NetworkRegressionTests.cpp`

- [ ] **Step 1: Add operation-accounting tests and a pending-I/O shutdown test**

Factor a small `OutstandingOperations` class in the IOCP header with `begin()`, `complete()`, `waitForZero()`, and a test-only/read-only `count()` accessor. Test these invariants without Winsock:

```cpp
OutstandingOperations ops;
ops.begin();                 // submission entered the OS wrapper
CHECK(ops.count() == 1);
ops.complete();              // synchronous non-pending failure: no completion follows
CHECK(ops.count() == 0);

ops.begin();
auto completion = std::async(std::launch::async, [&] { ops.complete(); });
ops.waitForZero();           // completion path wakes the waiter
completion.get();
CHECK(ops.count() == 0);
```

Then add a Windows integration case which starts `IocpServer`, connects idle clients so receives remain pending, leaves the initial AcceptEx operations pending, and calls `stop()`. Repeat at least 50 times and require every stop to finish within five seconds.

- [ ] **Step 2: Run the shutdown test red under a diagnostic build**

Run the focused test first. If the current force-delete does not fail normally, also run the executable under Visual Studio’s native debugger/Application Verifier with heap checks enabled; the pre-fix behavior is still documented as unsafe because contexts are deleted before cancelled completions are dequeued.

- [ ] **Step 3: Count every real completion-producing operation**

Add `OutstandingOperations m_operations` to `IocpServer`. Apply the same obligation to AcceptEx, receive, send, and the close control operation:

```text
before submission                  -> begin()
synchronous error != pending       -> complete() immediately
dequeued completion                -> complete() exactly once
worker shutdown sentinel           -> not counted
```

For every completion keyed by `ConnCtx` (receive, send, and control), keep a `ConnCtx` reference alongside the server-wide count: add the reference before submission; on synchronous failure, release the reference and complete the count; on a dequeued completion, release and complete exactly once after dispatch.

Check the return value from `AcceptEx`; on a synchronous error other than `ERROR_IO_PENDING`, close `clientSock`, delete `AcceptOv`, and complete immediately. Otherwise only the accept completion path frees it.

When an accept completion is dequeued after `m_running` became false, close its `clientSock` and free the accept object without calling `handleAccept()`. This prevents a late successful accept from publishing a new connection or receive after the shutdown snapshot.

- [ ] **Step 4: Reorder shutdown while workers remain alive**

Implement this exact order in `IocpServer::stop()`:

1. `m_running.exchange(false)` prevents all resubmission.
2. Shutdown and close `m_listen` so pending AcceptEx calls complete as cancelled.
3. Snapshot `m_conns` under `m_connsMu`, then call idempotent `markDead()` for each connection outside the set lock; this disarms channels, calls `onClose()`, closes sockets, removes initial live references, and causes pending receives/sends to complete.
4. `m_operations.waitForZero()` while workers continue dequeuing cancellations.
5. Post one uncounted `SHUTDOWN_KEY` per worker, join, and clear the workers.
6. Close the IOCP handle and balance Winsock startup.

Delete the force-free loop entirely. No path may post another accept, receive, send, or control operation after `m_running` becomes false.

- [ ] **Step 5: Verify shutdown and connection accounting**

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target network_regression_tests --parallel
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R network_regression_tests --repeat until-fail:10 --output-on-failure
```

Expected: no timeout, use-after-free, leaked open `WorldSocket` count, or non-zero outstanding operation count after each stop.

- [ ] **Step 6: Commit safe IOCP shutdown**

```powershell
git add src/shared/net/iocp/IocpServer.hpp src/shared/net/iocp/IocpServer.cpp tests/NetworkRegressionTests.cpp
git commit -m "fix: drain IOCP operations during shutdown"
```

---

### Task 5: Restore backend teardown and peer-address parity

**Files:**

- Modify: `src/shared/net/reactor/ReactorServer.cpp:278-294`
- Modify: `src/shared/net/uring/UringServer.cpp:153-186`
- Test: `tests/NetworkRegressionTests.cpp`

- [ ] **Step 1: Add a rejecting-poller Reactor test**

On POSIX, supply `ReactorServer` a coordinated fake `PollerFactory`: the accept poller reports the listener as readable after the client connects; the worker poller returns `false` from `add()`. The session fixture records `onClose()` with a condition variable. Assert `onConnect()` happens first, `onClose()` happens exactly once, and a retained `Sender` becomes a harmless no-op after rejection.

- [ ] **Step 2: Make rejected registration follow normal teardown**

In the `!w.poller->add(...)` branch, perform this order before deleting the connection:

```cpp
if (conn->channel)
    conn->channel->disarm();
if (conn->session)
    conn->session->onClose();
::close(conn->fd);
delete conn;
```

Use the same disarm/`onClose()` ordering for connections left in `Worker::incoming` during `stop()`, because those also passed `onConnect()`.

- [ ] **Step 3: Add an io_uring peer-order test**

When `MANGOS_USE_IO_URING` is enabled, start a loopback `UringServer` and record callback order plus the address. Assert `setPeerAddress("127.0.0.1")` occurs before `onConnect()`.

- [ ] **Step 4: Populate the accepted IPv4 address**

Replace the null address arguments to `accept4()` with:

```cpp
sockaddr_in peer{};
socklen_t peerLen = sizeof(peer);
int cfd = ::accept4(m_listen, reinterpret_cast<sockaddr*>(&peer),
                    &peerLen, SOCK_CLOEXEC);
```

After creating the session and before assigning channels or calling `onConnect()`, require `peer.sin_family == AF_INET`, convert with `inet_ntop(AF_INET, &peer.sin_addr, ...)`, and call `conn->session->setPeerAddress(peerIp)`. If the family or conversion is invalid, close the accepted fd and continue without publishing the session.

- [ ] **Step 5: Run the supported backend cases**

On Windows, confirm the source compiles through the normal target but note Reactor/io_uring cases as platform-skipped. On Linux CI or a Linux worktree run:

```bash
cmake -S . -B ../server_build_linux -DBUILD_TESTING=ON -DWITH_IO_URING=ON
cmake --build ../server_build_linux --target network_regression_tests -j
ctest --test-dir ../server_build_linux -R network_regression_tests --output-on-failure
```

Expected: Reactor rejection invokes cleanup once; io_uring delivers `127.0.0.1` before `onConnect()`. If liburing is unavailable, run the Reactor case and report io_uring as unexecuted rather than claiming it passed.

- [ ] **Step 6: Commit backend parity fixes**

```powershell
git add src/shared/net/reactor/ReactorServer.cpp src/shared/net/uring/UringServer.cpp tests/NetworkRegressionTests.cpp
git commit -m "fix: complete backend accept teardown state"
```

---

### Task 6: Lease WorldSession use across network callbacks

**Files:**

- Create: `src/shared/Threading/LeasedPtr.h`
- Modify: `src/game/Server/WorldSocket.h:93-111`
- Modify: `src/game/Server/WorldSocket.cpp:77-125,270-353,570-624`
- Modify: `src/game/Server/WorldSession.h:896`
- Modify: `src/game/Server/WorldSession.cpp:147-180`
- Test: `tests/LeaseTests.cpp`

- [ ] **Step 1: Write deterministic lease lifetime tests**

Test a move-only `LeasedPtr<Probe>::Lease` with barriers, not sleeps:

1. publish a stack `Probe`, acquire a lease on the simulated network thread, and block while holding it;
2. call `detachAndWait()` asynchronously on the simulated world thread and assert the future is not ready;
3. release the lease and assert detach completes;
4. assert a new acquire returns an empty lease;
5. assert moving a lease transfers the single active-use obligation;
6. while a lease is held, invoke a callback which calls non-waiting `detach()` and assert the callback returns (the self-close case), then release the lease and call `waitForNoLeases()` successfully.

Use `static_assert(!std::is_copy_constructible_v<Lease>)` and the corresponding copy-assignment assertion.

- [ ] **Step 2: Implement the header-only leased pointer**

`LeasedPtr<T>` owns no `T`; it contains only a mutex, condition variable, raw published pointer, and active count. Its nested `Lease` stores the owner plus pointer, supports move construction/assignment, `operator->`, `get()`, and explicit bool, and decrements/signals exactly once on destruction.

The required methods are:

```cpp
Lease acquire();                 // lock, reject null, increment, return lease
void publish(T* value);          // lock; require no different published value
void detach();                   // lock; set published pointer null; never wait
void detachAndWait();            // lock, null pointer, wait active == 0
void waitForNoLeases();          // wait active == 0 after an earlier detach
```

Never hold the state mutex while caller code uses `T`.

- [ ] **Step 3: Run the lease tests red then green**

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target lease_tests --parallel
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo -R lease_tests --repeat until-fail:20 --output-on-failure
```

Expected: deterministic completion with no deadlock; acquire after detach is always empty.

- [ ] **Step 4: Replace `WorldSocket` raw-pointer reads with leases**

Replace `m_SessionLock` plus `m_Session` with `LeasedPtr<WorldSession> m_session`. Make private `GetSession()` return `SessionLease`, and make `SetSession()` call `publish()`.

In `ProcessIncoming()` and both `HandlePing()` lookups, use a local lease and access with `lease.get()`/`lease->`. Do not carry a lease from `ProcessIncoming()` into `HandlePing()`, which acquires its own narrow leases. The authentication branch begins with no published session and may install the newly allocated session normally.

Eluna receives `lease.get()`, but the state mutex is not held. For the keep-alive callback, do not use that lease after the callback returns. This permits an Eluna callback to call `CloseSocket()` without waiting on itself.

- [ ] **Step 5: Preserve a deletion-time route back to the socket state**

Add `std::weak_ptr<WorldSocket> m_OwningSocket` immediately before `m_Socket` in `WorldSession`'s member declaration order. Initialize it from the constructor’s `sock` before moving the strong reference into `m_Socket`. This weak owner survives the existing logical `m_Socket = nullptr` disconnect behavior without prolonging the socket’s lifetime.

At the very start of `WorldSession::~WorldSession()`, before player logout or any other member teardown:

```cpp
if (std::shared_ptr<WorldSocket> socket = m_OwningSocket.lock())
    socket->DetachSessionAndWait();
```

Expose `WorldSocket::DetachSessionAndWait()` only for this deletion path; it delegates to `m_session.detachAndWait()`. Because all three deletion sites execute the destructor, this single mandatory gate covers `World::~World()` sessions, queued sessions, and `World::UpdateSessions()` without duplicating a fragile pre-delete call.

Change `CloseSocket()` and `onClose()` to call non-waiting `m_session.detach()` before invoking the transport closer. They must never wait on the network thread.

- [ ] **Step 6: Audit all session uses and deletions**

Run:

```powershell
rg -n "GetSession\(|delete .*Session|delete session|delete pSession|m_Session" `
  src/game/Server/WorldSocket.* src/game/Server/WorldSession.* src/game/WorldHandlers/World.cpp
```

Expected: `WorldSocket` has no unlocked raw session getter/member; every network-thread session dereference is protected by a lease; the known deletes remain covered by the destructor’s detach-and-wait gate.

- [ ] **Step 7: Commit session lifetime hardening**

```powershell
git add src/shared/Threading/LeasedPtr.h src/game/Server/WorldSocket.h src/game/Server/WorldSocket.cpp src/game/Server/WorldSession.h src/game/Server/WorldSession.cpp tests/LeaseTests.cpp
git commit -m "fix: lease WorldSession across network callbacks"
```

---

### Task 7: Phase-end verification and bounded Claude review

**Files:**

- Review: all files changed since `3026a3ca`
- Update only if a concrete BLOCKING or IMPORTANT finding proves a correctness gap

- [ ] **Step 1: Run all focused tests from a clean configure**

```powershell
$env:OPENSSL_MODULES='C:\OpenSSL-Win64\bin'
cmake -S E:\Mangos\WIP\Zero\Testing\server `
  -B E:\Mangos\WIP\Zero\Testing\server_build `
  -DCMAKE_INSTALL_PREFIX=E:\Mangos\WIP\Zero\Testing\server_install `
  -DBUILD_TESTING=ON `
  -DOPENSSL_ROOT_DIR=C:\OpenSSL-Win64
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target auth_crypto_tests lease_tests network_regression_tests --parallel
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo --output-on-failure
```

Expected: all supported tests pass; unsupported backend cases are explicitly reported as not executed.

- [ ] **Step 2: Run the full phase-end build and install once**

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --parallel
cmake --install E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo
```

Expected: no new compiler or linker errors. Inspect the build output for new warnings in the changed files and fix only correctness-relevant ones.

- [ ] **Step 3: Run existing service and daemon smoke checks**

```powershell
& E:\Mangos\WIP\Zero\Testing\server_install\service-workers\ah-service\ah-service.exe --selftest
```

Expected: `ah-service selftest` completes successfully.

Start `realmd.exe` from the install directory with only the OpenSSL 3.x DLL/module installation available, confirm it reaches its listening state, then stop it normally. Start `mangosd.exe`, confirm provider initialization and normal world startup, then stop it normally. Do not treat the previously investigated one-off cold `LoadContinents` tick as an ACE-removal regression unless it reproduces after warm startup.

- [ ] **Step 4: Prove installed daemons import only OpenSSL 3.x**

Use Visual Studio `dumpbin` from a developer shell:

```powershell
dumpbin /dependents E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe
dumpbin /dependents E:\Mangos\WIP\Zero\Testing\server_install\mangosd.exe
Get-ChildItem E:\Mangos\WIP\Zero\Testing\server_install\libcrypto-*-x64.dll
```

Expected: both daemons name `libcrypto-3-x64.dll`; the installed runtime directory no longer needs or contains `libcrypto-4-x64.dll`. Remove the stale 4.x DLL only as part of reinstall cleanup after confirming the exact path and that it is generated install output.

- [ ] **Step 5: Obtain one read-only Claude correctness review**

After all local checks pass, run exactly one bounded review:

```powershell
$prompt = @'
Review the ACE-removal hardening changes in this repository, diff range 3026a3ca..HEAD.
Goal: OpenSSL >=3,<4 with provider/runtime major agreement and RC4 startup probe; atomic IOCP close-after-drain; IOCP cancellation drain before context destruction; Reactor rejected-add cleanup; io_uring peer address before onConnect; WorldSession leases preventing delete/use races.
Constraints: preserve protocols and architecture; OpenSSL 4 deferred to 4.2 LTS; do not edit files; do not invoke another reviewer.
Completed checks: focused CTest targets, RelWithDebInfo full build/install, AH-service selftest, daemon startup, dumpbin dependency inspection.
Review only correctness, regressions, security, compatibility, unsupported assumptions, and realistic missing coverage. Return BLOCKING, IMPORTANT, and MINOR findings with file:line references, then APPROVE, APPROVE WITH FOLLOW-UP, or BLOCK.
'@
claude -p --permission-mode plan --max-turns 6 $prompt
```

If Claude is unavailable or times out, report that fact; do not replace it with internal reviewer agents. Fix a concrete BLOCKING/IMPORTANT correctness issue with one focused test and allow at most one focused re-review. Defer style-only or speculative findings to the final report.

- [ ] **Step 6: Final audit and status**

```powershell
git status --short
git log --oneline 3026a3ca..HEAD
$unfinished = @('TO' + 'DO', 'T' + 'BD', 'FIX' + 'ME') -join '|'
rg -n $unfinished docs/superpowers/plans/2026-07-22-ace-removal-hardening.md tests src/shared/Auth/OpenSSLProvider.* src/shared/net src/game/Server/WorldSocket.* src/game/Server/WorldSession.*
```

Expected: only intentional changes remain, the plan contains no unfinished markers, and each requirement maps to a test or a clearly reported platform-only validation. Report commands, results, Claude’s verdict, unexecuted Linux-only coverage, and any remaining risk.
