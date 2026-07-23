# Realmd Authentication Stream Safety Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use
> superpowers:subagent-driven-development (recommended) or
> superpowers:executing-plans to implement this plan task-by-task. Steps use
> checkbox (`- [ ]`) syntax for tracking.

**Goal:** Resolve realmd findings F-01 and F-02, plus the adjacent F-10
header-read defect, while preserving every client family served by the shared
realmd application.

**Architecture:** Preflight each pending auth frame through a pure,
build-independent protocol guard before calling the existing handlers. Bound
the pending stream before append and drive a realmd-owned authentication
deadline from the existing 100 ms main loop through weak socket references.

**Tech Stack:** C++17, CMake/CTest, MSVC RelWithDebInfo, PowerShell raw-socket
runtime validation, Python only for small validation-data helpers if needed.

## Global Constraints

- Work only in `E:\Mangos\WIP\Zero\Testing`.
- Parent branch: `fix/realmd-auth-stream-safety`.
- Realmd implementation branch: `fix/auth-stream-safety`, based on
  `3cf5f9690477a0bc2b33eb17eb1d9016da1d47e8`.
- Do not modify mangosd, shared IOCP/reactor/io_uring code, databases, clients,
  or `E:\Mangos\Repos\Zero\server`.
- Keep command framing independent of client build.
- Do not claim an end-to-end expansion smoke pass without launching that
  expansion's client and completing login.
- Use tests first and observe each relevant failure before production changes.
- Follow Allman braces, four spaces, braced single statements, and C++17 with
  extensions disabled.
- Build in `E:\Mangos\WIP\Zero\Testing\server_build`.
- Deploy to `E:\Mangos\WIP\Zero\Testing\server_install`.

---

## File Structure

### Realmd submodule

- `Auth/AuthProtocolGuard.h`: pure stream-state, frame-decision, pending-bound,
  and deadline interfaces.
- `Auth/AuthProtocolGuard.cpp`: command/state framing and deadline
  implementation.
- `Tests/AuthProtocolGuardTests.cpp`: deterministic production-code unit tests.
- `Auth/AuthSocket.h`: timeout-aware socket API and deadline member.
- `Auth/AuthSocket.cpp`: bounded append, guard-driven dispatch, safe challenge
  header reads, and close semantics.
- `Auth/AuthServer.h`: timeout-aware start plus periodic `Update()`.
- `Auth/AuthServer.cpp`: weak socket registry and expiry scan.
- `Main.cpp`: configuration wiring and 100 ms `AuthServer::Update()` call.
- `realmd.conf.dist.in`: documented `AuthSessionTimeout`.
- `CMakeLists.txt`: focused CTest target.

### Parent repository

- `tests/RealmdAuthStreamSmoke.ps1`: loopback invalid-command, fragmentation,
  timeout, and process-survival validation.
- `docs/superpowers/validation/realmd-auth-stream-safety.md`: commands,
  installed-client inventory, actual results, and accurately labelled
  limitations.

---

### Task 1: Establish the isolated implementation branch and baseline

**Files:**

- Read: `E:\Mangos\WIP\Zero\Testing\server`
- Read: `E:\Mangos\WIP\Zero\Testing\server\src\realmd`
- Read: `E:\Mangos\WIP\Zero\Testing\server_build`

**Interfaces:**

- Consumes: parent commit `2318449c`, realmd commit `3cf5f96`.
- Produces: clean realmd branch `fix/auth-stream-safety` and verified baseline.

- [ ] **Step 1: Confirm the designated Testing checkout is isolated and clean**

Run:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server status --short --branch
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd status --short --branch
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd rev-parse HEAD
```

Expected: parent has no uncommitted changes; realmd is clean at `3cf5f96`.

- [ ] **Step 2: Create the realmd implementation branch**

Run:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd switch -c fix/auth-stream-safety
```

Expected: new branch based exactly on `3cf5f96`.

Compatibility note: the GitHub sweep initially identified `39b7846` as the
realmd default-branch tip. The first realmd build proved that tip requires
split shared headers not present in mangoszero/server. Current Zero upstream
still pins `3cf5f96`, so implementation was rebased there before production
changes continued.

- [ ] **Step 3: Reconfigure the existing build after the submodule update**

Run:

```powershell
cmake -S E:\Mangos\WIP\Zero\Testing\server `
      -B E:\Mangos\WIP\Zero\Testing\server_build `
      -DBUILD_TESTING=ON `
      -DCMAKE_INSTALL_PREFIX=E:\Mangos\WIP\Zero\Testing\server_install
```

Expected: configure/generate succeeds using the cached Visual Studio 18 2026
generator and existing feature options.

- [ ] **Step 4: Run the existing baseline tests**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo `
      --target auth_crypto_tests network_regression_tests
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
      -C RelWithDebInfo --output-on-failure
```

Expected: existing configured tests pass. If they do not, stop and report the
baseline failure before implementation.

---

### Task 2: Add the pure protocol guard and deterministic RED/GREEN tests

**Files:**

- Create: `src/realmd/Auth/AuthProtocolGuard.h`
- Create: `src/realmd/Auth/AuthProtocolGuard.cpp`
- Create: `src/realmd/Tests/AuthProtocolGuardTests.cpp`
- Modify: `src/realmd/CMakeLists.txt`

**Interfaces:**

- Produces:

```cpp
namespace MaNGOS::Auth
{
enum class StreamState
{
    Challenge,
    LogonProof,
    ReconnectProof,
    Patch,
    Authenticated,
    Closed
};

enum class FrameStatus
{
    Incomplete,
    Complete,
    Reject
};

enum class RejectReason
{
    None,
    UnknownCommand,
    UnauthorizedCommand,
    MalformedLength
};

struct FrameDecision
{
    FrameStatus status;
    RejectReason reason;
    std::size_t frameSize;
};

constexpr std::size_t AuthChallengeHeaderSize = 4;
constexpr std::size_t AuthChallengeMinimumBodySize = 31;
constexpr std::size_t AuthLogonProofSize = 75;
constexpr std::size_t AuthReconnectProofSize = 58;
constexpr std::size_t AuthRealmListSize = 5;
constexpr std::size_t AuthXferAcceptSize = 1;
constexpr std::size_t AuthXferResumeSize = 9;
constexpr std::size_t AuthXferCancelSize = 1;
constexpr std::size_t MaxPendingInput =
    AuthChallengeHeaderSize + UINT16_MAX;

FrameDecision InspectFrame(
    StreamState state, std::uint8_t const* data, std::size_t size);

bool CanAppendPending(std::size_t pending, std::size_t incoming);

class Deadline
{
public:
    using Clock = std::chrono::steady_clock;

    Deadline(Clock::time_point start, std::chrono::seconds timeout);
    bool expired(Clock::time_point now) const;
    void deactivate();
    bool active() const;

private:
    Clock::time_point m_deadline;
    std::atomic<bool> m_active;
};
}
```

- [ ] **Step 1: Write the CTest target and failing tests**

Create `Tests/AuthProtocolGuardTests.cpp` with a minimal local `CHECK` helper.
The tests must:

```cpp
using MaNGOS::Auth::FrameStatus;
using MaNGOS::Auth::InspectFrame;
using MaNGOS::Auth::StreamState;

// A challenge frame contains the four-byte header and a declared 31-byte
// minimum body. Every prefix must remain incomplete.
for (std::size_t split = 0; split < challenge.size(); ++split)
{
    CHECK(InspectFrame(StreamState::Challenge,
                       challenge.data(), split).status ==
          FrameStatus::Incomplete);
}
CHECK(InspectFrame(StreamState::Challenge,
                   challenge.data(), challenge.size()).status ==
      FrameStatus::Complete);

// Repeat with CMD_AUTH_RECONNECT_CHALLENGE.
// Reject body lengths 0..30.
// Reject unknown commands.
// Reject known commands in the wrong state.
// Require 75/58/5/1/9/1 bytes for fixed commands.
// Prove CanAppendPending rejects MaxPendingInput + 1 without overflow.
// Prove deadline expiry at the boundary and deactivate behavior.
```

Update `src/realmd/CMakeLists.txt`:

```cmake
if(BUILD_TESTING)
    add_executable(realmd_auth_protocol_tests
        Tests/AuthProtocolGuardTests.cpp
        Auth/AuthProtocolGuard.cpp
    )
    target_compile_features(realmd_auth_protocol_tests PUBLIC cxx_std_17)
    set_target_properties(realmd_auth_protocol_tests PROPERTIES
        CXX_EXTENSIONS OFF)
    target_include_directories(realmd_auth_protocol_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR})
    add_test(NAME realmd_auth_protocol_tests
        COMMAND realmd_auth_protocol_tests)
endif()
```

- [ ] **Step 2: Configure and verify RED**

Run:

```powershell
cmake -S E:\Mangos\WIP\Zero\Testing\server `
      -B E:\Mangos\WIP\Zero\Testing\server_build `
      -DBUILD_TESTING=ON
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo --target realmd_auth_protocol_tests
```

Expected: compilation/link failure because the guard implementation is absent
or returns unimplemented decisions. The failure must be attributable to the
missing behavior, not a malformed test.

- [ ] **Step 3: Implement the minimal guard**

Implement `InspectFrame` as:

```cpp
FrameDecision InspectFrame(
    StreamState state, std::uint8_t const* data, std::size_t size)
{
    if (!data || size == 0)
    {
        return {FrameStatus::Incomplete, RejectReason::None, 0};
    }

    std::size_t required = 0;
    switch (data[0])
    {
        case CMD_AUTH_LOGON_CHALLENGE:
        case CMD_AUTH_RECONNECT_CHALLENGE:
        {
            if (state != StreamState::Challenge)
            {
                return {FrameStatus::Reject,
                        RejectReason::UnauthorizedCommand, 0};
            }
            if (size < AuthChallengeHeaderSize)
            {
                return {FrameStatus::Incomplete, RejectReason::None, 0};
            }
            std::uint16_t const body =
                static_cast<std::uint16_t>(data[2]) |
                (static_cast<std::uint16_t>(data[3]) << 8);
            if (body < AuthChallengeMinimumBodySize)
            {
                return {FrameStatus::Reject,
                        RejectReason::MalformedLength, 0};
            }
            required = AuthChallengeHeaderSize + body;
            break;
        }
        // Map every fixed command to its one permitted state and exact size.
        default:
            return {FrameStatus::Reject, RejectReason::UnknownCommand, 0};
    }

    if (size < required)
    {
        return {FrameStatus::Incomplete, RejectReason::None, required};
    }
    return {FrameStatus::Complete, RejectReason::None, required};
}
```

Implement overflow-safe pending admission:

```cpp
bool CanAppendPending(std::size_t pending, std::size_t incoming)
{
    return pending <= MaxPendingInput &&
           incoming <= MaxPendingInput - pending;
}
```

Implement `Deadline` so timeout zero starts inactive, expiry uses `now >=
m_deadline`, and `deactivate()` is idempotent.

- [ ] **Step 4: Verify GREEN**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo --target realmd_auth_protocol_tests
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
      -C RelWithDebInfo -R realmd_auth_protocol_tests --output-on-failure
```

Expected: focused test passes with zero failures.

- [ ] **Step 5: Commit Task 2 inside realmd**

Run:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd add `
    Auth/AuthProtocolGuard.h Auth/AuthProtocolGuard.cpp `
    Tests/AuthProtocolGuardTests.cpp CMakeLists.txt
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd commit `
    -m "test: define auth stream safety contract"
```

---

### Task 3: Integrate bounded transactional framing into AuthSocket

**Files:**

- Modify: `src/realmd/Auth/AuthSocket.h`
- Modify: `src/realmd/Auth/AuthSocket.cpp`
- Modify: `src/realmd/Tests/AuthProtocolGuardTests.cpp`

**Interfaces:**

- Consumes: `MaNGOS::Auth::InspectFrame`, `CanAppendPending`, and `Deadline`.
- Produces:

```cpp
explicit AuthSocket(std::chrono::seconds authTimeout);
bool ExpireAuthentication(
    MaNGOS::Auth::Deadline::Clock::time_point now);
```

- [ ] **Step 1: Extend tests for retained-byte and coalescing decisions**

Add a test helper that feeds a vector in every split and verifies:

```cpp
auto decision = InspectFrame(StreamState::Challenge,
                             bytes.data(), split);
CHECK(decision.status == FrameStatus::Incomplete);
CHECK(std::equal(bytes.begin(), bytes.begin() + split,
                 retained.begin()));
```

Add complete-frame-plus-next-frame cases and assert `frameSize` identifies only
the first frame. Add all accepted build numbers to challenge frames and assert
identical guard decisions:

```cpp
for (std::uint16_t build : {
    5875, 6005, 6141, 8606, 12340, 15595,
    18273, 18414, 21742, 26972, 35662, 40000})
{
    auto frame = MakeChallenge(CMD_AUTH_LOGON_CHALLENGE, build, "TEST");
    CHECK(InspectFrame(StreamState::Challenge,
                       frame.data(), frame.size()).status ==
          FrameStatus::Complete);
}
```

- [ ] **Step 2: Run focused tests before AuthSocket integration**

Run the focused test and record that guard tests pass while the live F-01/F-02
runtime reproduction still fails against the installed baseline. This is the
RED evidence for the socket wiring.

- [ ] **Step 3: Add the AuthSocket deadline and state mapping**

In `AuthSocket.h`:

```cpp
#include "AuthProtocolGuard.h"
#include <chrono>

explicit AuthSocket(std::chrono::seconds authTimeout);
bool ExpireAuthentication(
    MaNGOS::Auth::Deadline::Clock::time_point now);

MaNGOS::Auth::StreamState stream_state() const;
void deactivate_auth_deadline();

MaNGOS::Auth::Deadline m_authDeadline;
```

Keep `eStatus` private. Map it to guard states in a total `switch` and return
`Closed` only for `STATUS_CLOSED`.

- [ ] **Step 4: Bound append and preflight dispatch**

At the start of `onData`:

```cpp
if (!MaNGOS::Auth::CanAppendPending(m_readBuf.size(), len))
{
    DEBUG_LOG("[Auth] Closing connection: pending input limit exceeded");
    m_readBuf.clear();
    close_connection();
    return {};
}
m_readBuf.insert(m_readBuf.end(), data, data + len);
```

Before table dispatch:

```cpp
auto const decision = MaNGOS::Auth::InspectFrame(
    stream_state(), m_readBuf.data() + m_readPos, recv_len());

if (decision.status == MaNGOS::Auth::FrameStatus::Incomplete)
{
    break;
}
if (decision.status == MaNGOS::Auth::FrameStatus::Reject)
{
    DEBUG_LOG("[Auth] Closing rejected auth stream, reason %u",
              static_cast<unsigned>(decision.reason));
    m_readBuf.clear();
    m_readPos = 0;
    close_connection();
    return {};
}
```

Record `frameStart = m_readPos`. After a successful handler, require:

```cpp
if (m_readPos - frameStart != decision.frameSize)
{
    DEBUG_LOG("[Auth] Handler consumed an unexpected frame length");
    close_connection();
    break;
}
```

If a preflighted handler returns `false`, close immediately. Unknown and
wrong-state commands are rejected by the guard before the table is called.

- [ ] **Step 5: Make both challenge header reads safe**

Replace consuming-before-complete assumptions with the guard invariant. Decode:

```cpp
std::uint16_t const remaining =
    static_cast<std::uint16_t>(buf[2]) |
    (static_cast<std::uint16_t>(buf[3]) << 8);
```

Remove both invalid/endian pointer expressions, including the reconnect
`*((uint16*)(buf[0]))` expression. Keep defensive length checks, but they must
not consume or return incomplete after guard approval.

Add:

```cpp
static_assert(sizeof(sAuthLogonChallenge_C) ==
              MaNGOS::Auth::AuthChallengeHeaderSize +
              MaNGOS::Auth::AuthChallengeMinimumBodySize);
static_assert(sizeof(sAuthLogonProof_C) ==
              MaNGOS::Auth::AuthLogonProofSize);
static_assert(sizeof(sAuthReconnectProof_C) ==
              MaNGOS::Auth::AuthReconnectProofSize);
```

- [ ] **Step 6: Deactivate the deadline at terminal states**

Deactivate before entering `STATUS_PATCH`, on successful normal/reconnect
authentication, in `onClose()`, and in `close_connection()`. Implement
`ExpireAuthentication(now)`:

```cpp
bool AuthSocket::ExpireAuthentication(Deadline::Clock::time_point now)
{
    if (!m_authDeadline.expired(now))
    {
        return false;
    }
    DEBUG_LOG("[Auth] Authentication deadline expired for '%s'",
              get_remote_address().c_str());
    close_connection();
    return true;
}
```

- [ ] **Step 7: Build and run focused tests**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo --target realmd_auth_protocol_tests realmd
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
      -C RelWithDebInfo -R realmd_auth_protocol_tests --output-on-failure
```

Expected: compile and focused tests pass.

- [ ] **Step 8: Commit Task 3 inside realmd**

Run:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd add `
    Auth/AuthSocket.h Auth/AuthSocket.cpp `
    Tests/AuthProtocolGuardTests.cpp
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd commit `
    -m "fix: bound and frame auth input safely"
```

---

### Task 4: Wire the realmd-owned authentication watchdog

**Files:**

- Modify: `src/realmd/Auth/AuthServer.h`
- Modify: `src/realmd/Auth/AuthServer.cpp`
- Modify: `src/realmd/Main.cpp`
- Modify: `src/realmd/realmd.conf.dist.in`
- Create: `tests/RealmdAuthStreamSmoke.ps1`

**Interfaces:**

- Produces:

```cpp
bool AuthServer::Start(
    std::uint16_t port,
    std::string const& bindIp,
    std::chrono::seconds authTimeout);

void AuthServer::Update();
```

- [ ] **Step 1: Write the failing runtime smoke test**

Create `tests/RealmdAuthStreamSmoke.ps1` with parameters:

```powershell
param(
    [Parameter(Mandatory)] [string] $RealmdPath,
    [Parameter(Mandatory)] [string] $ConfigPath,
    [int] $Port = 43724,
    [int] $TimeoutSeconds = 2
)
```

The script must:

1. create a unique temporary directory;
2. copy the supplied config without printing its contents;
3. replace `BindIP`, `RealmServerPort`, `LogsDir`, and `PidFile` in the
   temporary copy, and replace or append `AuthSessionTimeout` so the script
   also works against the pre-change configuration;
4. start only the requested realmd executable hidden with the install directory
   as working directory;
5. wait conditionally for the loopback port;
6. send `0xFF` followed by repeated chunks and require prompt EOF;
7. send complete and every-split build-5875 logon challenges and require the
   same command/result response prefix;
8. open an idle socket and require EOF after the configured deadline;
9. prove the realmd process remains alive after each hostile connection;
10. stop only its tracked PID in `finally`;
11. remove only the validated temporary directory.

- [ ] **Step 2: Run runtime RED against the baseline installed realmd**

Run:

```powershell
powershell -ExecutionPolicy Bypass -File `
  E:\Mangos\WIP\Zero\Testing\server\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe `
  -ConfigPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.conf `
  -Port 43724 -TimeoutSeconds 2
```

Expected: at least invalid-command closure, fragmentation, or timeout assertion
fails against the baseline. If realmd cannot start because the configured
database is unavailable, record that environmental blocker and rely on the
deterministic tests until the database is available; do not fake a pass.

- [ ] **Step 3: Implement AuthServer weak tracking**

`AuthServer::Impl` gains:

```cpp
net::Server server;
std::mutex socketsMutex;
std::vector<std::weak_ptr<AuthSocket>> sockets;
std::chrono::seconds authTimeout{30};
```

The session factory creates `std::shared_ptr<AuthSocket>`, registers a weak
reference under the mutex, and returns it as `net::ISession`.

`Update()` locks only long enough to discard expired weak references and obtain
strong references. It releases the mutex before calling
`ExpireAuthentication(Clock::now())`.

- [ ] **Step 4: Wire configuration and housekeeping**

Read:

```cpp
int32 const configuredAuthTimeout =
    sConfig.GetIntDefault("AuthSessionTimeout", 30);
uint32 const authTimeoutSeconds =
    configuredAuthTimeout < 0
        ? 30u : static_cast<uint32>(configuredAuthTimeout);
```

Log and use 30 for negative values. Pass the duration to `AuthServer::Start`.
Call `authServer.Update()` once per existing 100 ms main-loop iteration.

Document in `realmd.conf.dist.in`:

```ini
#    AuthSessionTimeout
#        Maximum seconds allowed to complete logon or reconnect authentication.
#        Authenticated sessions and patch transfers are exempt.
#        Default: 30
#                 0 (Disabled)
AuthSessionTimeout      = 30
```

- [ ] **Step 5: Build, install, and verify runtime GREEN**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo --target realmd realmd_auth_protocol_tests
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
      -C RelWithDebInfo -R realmd_auth_protocol_tests --output-on-failure
cmake --install E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo
powershell -ExecutionPolicy Bypass -File `
  E:\Mangos\WIP\Zero\Testing\server\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe `
  -ConfigPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.conf `
  -Port 43724 -TimeoutSeconds 2
```

Expected: focused CTest and every runtime assertion pass.

- [ ] **Step 6: Commit Task 4 in both repositories**

Inside realmd:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd add `
    Auth/AuthServer.h Auth/AuthServer.cpp Main.cpp realmd.conf.dist.in
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd commit `
    -m "fix: expire incomplete auth handshakes"
```

Inside the parent:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server add `
    src/realmd tests/RealmdAuthStreamSmoke.ps1
git -C E:\Mangos\WIP\Zero\Testing\server commit `
    -m "test: validate realmd auth stream safety"
```

---

### Task 5: Validate multi-client compatibility and phase-end quality

**Files:**

- Create:
  `docs/superpowers/validation/realmd-auth-stream-safety.md`
- Read: installed clients under `E:\Mangos\Clients`
- Read: audit evidence under
  `E:\Mangos\WIP\Zero\Testing\codex\classic_login_protocol_audit`

**Interfaces:**

- Consumes: completed realmd commits and installed binary.
- Produces: evidence-labelled compatibility and completion report.

- [ ] **Step 1: Run every accepted-build packet test**

The focused CTest must exercise:

```text
5875, 6005, 6141, 8606, 12340, 15595,
18273, 18414, 21742, 26972, 35662, 40000
```

Expected: each challenge is complete only at its declared frame boundary and is
not rejected because of build.

- [ ] **Step 2: Inventory installed clients**

List client roots under `E:\Mangos\Clients` and record which accepted builds
have a launchable client. Do not infer an installation from a directory name
alone; verify the executable version/build where possible.

- [ ] **Step 3: Perform actual installed-client smoke tests**

For each installed supported client:

1. point it at the Testing realmd;
2. log in with a Testing account;
3. retrieve and display the realm list;
4. record build, locale, success/failure, and log evidence.

For build 5875, select the Testing realm and verify a populated character screen.
Do not enter the world; that remains outside this package.

Unavailable clients are marked `NOT INSTALLED — packet framing only`, never
`PASS`.

- [ ] **Step 4: Run phase-end automated verification**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
      --config RelWithDebInfo
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
      -C RelWithDebInfo --output-on-failure
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd diff --check master..HEAD
git -C E:\Mangos\WIP\Zero\Testing\server diff --check 2318449c..HEAD
```

Expected: build and all configured tests pass; diff checks are clean.

- [ ] **Step 5: Verify repository boundaries**

Run:

```powershell
git -C E:\Mangos\Repos\Zero\server status --short
git -C E:\Mangos\WIP\Zero\Testing\server status --short --branch
git -C E:\Mangos\WIP\Zero\Testing\server\src\realmd status --short --branch
```

Expected: original repository unchanged; Testing parent and submodule clean on
their feature branches.

- [ ] **Step 6: Write validation evidence**

Record:

- GitHub sweep and baseline revisions;
- RED/GREEN test commands;
- focused/full pass counts;
- build and install result;
- raw-socket runtime results;
- per-build packet results;
- actual installed-client smoke results;
- unavailable client families;
- remaining uncertainty and the next finding, F-03.

- [ ] **Step 7: Commit validation and submodule pointer**

Run:

```powershell
git -C E:\Mangos\WIP\Zero\Testing\server add `
    src/realmd `
    docs/superpowers/validation/realmd-auth-stream-safety.md
git -C E:\Mangos\WIP\Zero\Testing\server commit `
    -m "docs: record realmd auth safety validation"
```

- [ ] **Step 8: Obtain one bounded cross-model review**

After targeted and phase-end checks pass, request one read-only Claude review
covering only:

- goal and constraints;
- realmd and parent diffs;
- completed tests;
- multi-client compatibility;
- security, races, lifetime, and protocol-framing risks;
- known limitations.

The reviewer must not edit files or invoke another reviewer. Address BLOCKING
and IMPORTANT findings with one focused correction/re-review at most.

---

## Execution Notes

Record each task's RED/GREEN commands, pass counts, commits, build/install
result, runtime evidence, client inventory, and review verdict here during
execution.
