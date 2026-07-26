# Realmd Multi-Client Build Policy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make shared realmd build acceptance exact and select IP-lock authentication results from the connecting client's runtime family.

**Architecture:** Introduce one pure build-policy table containing version metadata and `RealmVersion` for every supported build. Realm lookup and a small auth-result policy consume that same table, eliminating both the open-ended Shadowlands range and the parent compile-time `CLASSIC` decision.

**Tech Stack:** C++17, CMake/CTest, MSVC Release, existing PowerShell installed-runtime smoke.

## Global Constraints

- Work only in `E:\Mangos\WIP\Zero\Testing`.
- Production and focused-test changes belong in the shared `src/realmd` submodule.
- Preserve all exact builds currently listed: 5875, 6005, 6141, 8606, 12340, 15595, 18273, 18414, 21742, 26972, 35662, and 40000.
- Unknown builds, including 39999, 40001, and 65535, must not resolve as supported.
- Vanilla IP-lock mismatch returns `WOW_FAIL_DB_BUSY`; explicit TBC-and-later builds return `WOW_FAIL_LOCKED_ENFORCED`; unknown builds conservatively return `WOW_FAIL_DB_BUSY`.
- Do not modify SRP, patch transport, realm packet layouts, PIN authentication, telemetry proof records, or mangosd.
- Keep source, build, and install roots separate; deploy only to `E:\Mangos\WIP\Zero\Testing\server_install`.
- Do not start or stop the user's normal server processes.

---

### Task 1: Create one exact client-build policy

**Files:**
- Create: `src/realmd/Realm/ClientBuildPolicy.h`
- Create: `src/realmd/Realm/ClientBuildPolicy.cpp`
- Create: `src/realmd/Tests/ClientBuildPolicyTests.cpp`
- Modify: `src/realmd/CMakeLists.txt`
- Modify: `src/realmd/Realm/RealmSnapshot.h`
- Modify: `src/realmd/Realm/RealmList.h`
- Modify: `src/realmd/Realm/RealmList.cpp`

**Interfaces:**
- Produces: `ClientBuildPolicy const* FindClientBuildPolicy(uint32 build)`.
- Preserves: `RealmBuildInfo const* FindBuildInfo(uint16 build)`, now exact-only.
- `ClientBuildPolicy` contains `RealmBuildInfo buildInfo` and `RealmVersion realmVersion`.

- [ ] **Step 0: Create the isolated realmd feature branch and verify baseline**

```powershell
git -C src/realmd status --short --branch
git -C src/realmd switch -c fix/multiclient-build-policy
ctest --test-dir ..\server_build -C Release -R "^realmd_" --output-on-failure
```

Expected: realmd starts clean at current `origin/master`, and all five existing
realmd tests pass before changes.

- [ ] **Step 1: Write the failing exact-policy test**

Add a `realmd_client_build_policy_tests` CMake target initially containing only
`Tests/ClientBuildPolicyTests.cpp`. The test includes the wished-for
`Realm/ClientBuildPolicy.h` API and checks literal expected metadata/families:

```cpp
struct ExpectedBuild
{
    uint32 build;
    RealmVersion version;
    int major;
    int minor;
    int bugfix;
    int hotfix;
};

ExpectedBuild const supported[] =
{
    {5875, REALM_VERSION_VANILLA, 1, 12, 1, ' '},
    {6005, REALM_VERSION_VANILLA, 1, 12, 2, ' '},
    {6141, REALM_VERSION_VANILLA, 1, 12, 3, ' '},
    {8606, REALM_VERSION_TBC, 2, 4, 3, ' '},
    {12340, REALM_VERSION_WOTLK, 3, 3, 5, 'a'},
    {15595, REALM_VERSION_CATA, 4, 3, 4, ' '},
    {18273, REALM_VERSION_MOP, 5, 4, 8, ' '},
    {18414, REALM_VERSION_MOP, 5, 4, 8, ' '},
    {21742, REALM_VERSION_WOD, 6, 2, 4, ' '},
    {26972, REALM_VERSION_LEGION, 7, 3, 5, ' '},
    {35662, REALM_VERSION_BFA, 8, 3, 7, ' '},
    {40000, REALM_VERSION_SHADOWLANDS, 9, 0, 0, ' '},
};

for (ExpectedBuild const& expected : supported)
{
    ClientBuildPolicy const* policy = FindClientBuildPolicy(expected.build);
    CHECK(policy != nullptr);
    CHECK(policy->realmVersion == expected.version);
    CHECK(policy->buildInfo.build == static_cast<int>(expected.build));
    CHECK(policy->buildInfo.major_version == expected.major);
    CHECK(policy->buildInfo.minor_version == expected.minor);
    CHECK(policy->buildInfo.bugfix_version == expected.bugfix);
    CHECK(policy->buildInfo.hotfix_version == expected.hotfix);
    CHECK(FindBuildInfo(static_cast<uint16>(expected.build)) ==
          &policy->buildInfo);
}

uint32 const unsupported[] = {0, 1, 39999, 40001, 65535};
for (uint32 build : unsupported)
{
    CHECK(FindClientBuildPolicy(build) == nullptr);
    CHECK(FindBuildInfo(static_cast<uint16>(build)) == nullptr);
}
```

- [ ] **Step 2: Run the test and observe RED**

Run:

```powershell
cmake -S . -B ..\server_build -DBUILD_TESTING=ON
cmake --build ..\server_build --config Release --target realmd_client_build_policy_tests -- /m
```

Expected: compilation fails because `Realm/ClientBuildPolicy.h` and its API do
not exist.

- [ ] **Step 3: Implement the exact build table**

Create `ClientBuildPolicy.h`:

```cpp
#ifndef MANGOS_H_CLIENTBUILDPOLICY
#define MANGOS_H_CLIENTBUILDPOLICY

#include "RealmSnapshot.h"

struct ClientBuildPolicy
{
    RealmBuildInfo buildInfo;
    RealmVersion realmVersion;
};

ClientBuildPolicy const* FindClientBuildPolicy(uint32 build);
RealmBuildInfo const* FindBuildInfo(uint16 build);

#endif
```

Create `ClientBuildPolicy.cpp` with the twelve literal entries from Step 1 and
an exact linear lookup. `FindBuildInfo` returns the embedded `buildInfo` only
when exact lookup succeeds:

```cpp
ClientBuildPolicy const* FindClientBuildPolicy(uint32 build)
{
    for (ClientBuildPolicy const& policy : SupportedClientBuilds)
    {
        if (static_cast<uint32>(policy.buildInfo.build) == build)
        {
            return &policy;
        }
    }
    return nullptr;
}

RealmBuildInfo const* FindBuildInfo(uint16 build)
{
    ClientBuildPolicy const* policy = FindClientBuildPolicy(build);
    return policy ? &policy->buildInfo : nullptr;
}
```

Add `Realm/ClientBuildPolicy.cpp` to the test target. Remove the old table and
`FindBuildInfo` implementation from `RealmList.cpp` and its declaration from
`RealmSnapshot.h`.

- [ ] **Step 4: Make realm-family grouping consume the same policy**

Remove `RealmBuildVersionMap`, `m_buildToVersion`, `InitBuildToVersion`, and the
initialization call. Replace `BelongsToVersion` with:

```cpp
RealmVersion RealmList::BelongsToVersion(uint32 build) const
{
    ClientBuildPolicy const* policy = FindClientBuildPolicy(build);
    return policy ? policy->realmVersion : REALM_VERSION_VANILLA;
}
```

Include `ClientBuildPolicy.h` in `RealmList.cpp`. The Vanilla fallback is only
for existing malformed/custom realm database rows; it does not make a client
build supported.

- [ ] **Step 5: Run GREEN checks**

Run:

```powershell
cmake -S . -B ..\server_build -DBUILD_TESTING=ON
cmake --build ..\server_build --config Release --target realmd_client_build_policy_tests realmd_realm_snapshot_tests realmd -- /m
ctest --test-dir ..\server_build -C Release -R "^realmd_(client_build_policy|realm_snapshot)_tests$" --output-on-failure
```

Expected: both focused tests pass and realmd builds.

- [ ] **Step 6: Commit Task 1 in the realmd submodule**

```powershell
git -C src/realmd add CMakeLists.txt Realm/ClientBuildPolicy.h Realm/ClientBuildPolicy.cpp Realm/RealmSnapshot.h Realm/RealmList.h Realm/RealmList.cpp Tests/ClientBuildPolicyTests.cpp
git -C src/realmd commit -m "fix: require explicit supported client builds"
```

---

### Task 2: Select IP-lock results from runtime family

**Files:**
- Create: `src/realmd/Auth/AuthResultPolicy.h`
- Create: `src/realmd/Auth/AuthResultPolicy.cpp`
- Modify: `src/realmd/Auth/AuthSocket.cpp`
- Modify: `src/realmd/CMakeLists.txt`
- Modify: `src/realmd/Tests/ClientBuildPolicyTests.cpp`

**Interfaces:**
- Consumes: `FindClientBuildPolicy(uint32 build)` from Task 1.
- Produces: `eAuthCmdResult LockedAccountResultForBuild(uint32 build)`.

- [ ] **Step 1: Write the failing auth-result tests**

Include `Auth/AuthResultPolicy.h` and add:

```cpp
uint32 const vanilla[] = {5875, 6005, 6141};
for (uint32 build : vanilla)
{
    CHECK(LockedAccountResultForBuild(build) == WOW_FAIL_DB_BUSY);
}

uint32 const later[] =
{
    8606, 12340, 15595, 18273, 18414, 21742,
    26972, 35662, 40000
};
for (uint32 build : later)
{
    CHECK(LockedAccountResultForBuild(build) ==
          WOW_FAIL_LOCKED_ENFORCED);
}

uint32 const unknown[] = {0, 39999, 40001, 65535};
for (uint32 build : unknown)
{
    CHECK(LockedAccountResultForBuild(build) == WOW_FAIL_DB_BUSY);
}
```

- [ ] **Step 2: Run the test and observe RED**

Run:

```powershell
cmake --build ..\server_build --config Release --target realmd_client_build_policy_tests -- /m
```

Expected: compilation fails because `Auth/AuthResultPolicy.h` and
`LockedAccountResultForBuild` do not exist.

- [ ] **Step 3: Implement the runtime result policy**

Create `AuthResultPolicy.h`:

```cpp
#ifndef MANGOS_H_AUTHRESULTPOLICY
#define MANGOS_H_AUTHRESULTPOLICY

#include "AuthCodes.h"
#include "Platform/Define.h"

eAuthCmdResult LockedAccountResultForBuild(uint32 build);

#endif
```

Create `AuthResultPolicy.cpp`:

```cpp
#include "AuthResultPolicy.h"
#include "Realm/ClientBuildPolicy.h"

eAuthCmdResult LockedAccountResultForBuild(uint32 build)
{
    ClientBuildPolicy const* policy = FindClientBuildPolicy(build);
    if (!policy || policy->realmVersion == REALM_VERSION_VANILLA)
    {
        return WOW_FAIL_DB_BUSY;
    }
    return WOW_FAIL_LOCKED_ENFORCED;
}
```

Add `Auth/AuthResultPolicy.cpp` to the focused test target.

- [ ] **Step 4: Remove compile-time result selection**

Include `AuthResultPolicy.h` in `AuthSocket.cpp` and replace the `#if
defined(CLASSIC)` block with:

```cpp
pkt << static_cast<uint8>(LockedAccountResultForBuild(_build));
```

- [ ] **Step 5: Run GREEN checks and a compile-time-independence check**

Run:

```powershell
cmake -S . -B ..\server_build -DBUILD_TESTING=ON
cmake --build ..\server_build --config Release --target realmd_client_build_policy_tests realmd -- /m
ctest --test-dir ..\server_build -C Release -R "^realmd_client_build_policy_tests$" --output-on-failure
rg -n "#if defined\(CLASSIC\)|WOW_FAIL_DB_BUSY|WOW_FAIL_LOCKED_ENFORCED" src/realmd/Auth/AuthSocket.cpp src/realmd/Auth/AuthResultPolicy.cpp
```

Expected: the focused test passes, realmd builds, and the IP-lock path contains
no compile-time `CLASSIC` branch.

- [ ] **Step 6: Commit Task 2 in the realmd submodule**

```powershell
git -C src/realmd add CMakeLists.txt Auth/AuthResultPolicy.h Auth/AuthResultPolicy.cpp Auth/AuthSocket.cpp Tests/ClientBuildPolicyTests.cpp
git -C src/realmd commit -m "fix: select auth results by client build"
```

---

### Task 3: Validate and review the shared realmd change

**Files:**
- Read: all Task 1 and Task 2 diffs
- Test: all registered Zero CTest tests
- Runtime: `tests/RealmdAuthStreamSmoke.ps1`

**Interfaces:**
- Consumes: committed realmd feature branch from Tasks 1 and 2.
- Produces: a review-ready local realmd branch and refreshed Zero installation.

- [ ] **Step 1: Run focused realmd validation**

```powershell
cmake -S . -B ..\server_build -DBUILD_TESTING=ON -DCMAKE_INSTALL_PREFIX=E:\Mangos\WIP\Zero\Testing\server_install
cmake --build ..\server_build --config Release --target realmd_client_build_policy_tests realmd_auth_protocol_tests realmd_realm_snapshot_tests realmd -- /m
ctest --test-dir ..\server_build -C Release -R "^realmd_" --output-on-failure
```

Expected: every realmd test passes.

- [ ] **Step 2: Run phase-end build, tests, smoke, and hash gate**

Confirm no compiler/linker process is active, then run:

```powershell
cmake --build ..\server_build --config Release --target INSTALL -- /m
ctest --test-dir ..\server_build -C Release --output-on-failure
& .\tests\RealmdAuthStreamSmoke.ps1 -RealmdPath ..\server_install\realmd.exe -ConfigPath ..\server_install\realmd.conf
$buildHash = (Get-FileHash ..\server_build\src\realmd\Release\realmd.exe -Algorithm SHA256).Hash
$installHash = (Get-FileHash ..\server_install\realmd.exe -Algorithm SHA256).Hash
if ($buildHash -ne $installHash) { throw "Installed realmd differs from build output" }
```

Expected: Release install succeeds, complete CTest is green, installed smoke
passes, and hashes match.

- [ ] **Step 3: Obtain one bounded read-only Claude review**

Create a timestamped JSONL ledger outside the repository. Run Claude Code with
`--permission-mode plan --max-turns 6 --output-format stream-json
--include-partial-messages --verbose`, providing only the goal, constraints,
realmd diff, completed tests, compatibility risks, and unresolved questions.
Require BLOCKING/IMPORTANT/MINOR findings followed by APPROVE, APPROVE WITH
FOLLOW-UP, or BLOCK. Claude must not edit files or invoke another reviewer.

Expected: no BLOCKING or IMPORTANT finding remains. Apply at most one focused
correction/re-review cycle if concrete correctness evidence requires it.

- [ ] **Step 4: Verify final scope**

```powershell
git -C src/realmd diff --check origin/master...HEAD
git -C src/realmd diff --stat origin/master...HEAD
git -C src/realmd status --short --branch
git status --short --branch
```

Expected: realmd contains only the planned policy/tests; the parent contains
the committed design/plan and an intentionally modified `src/realmd` gitlink.
Do not commit the parent gitlink until the realmd PR is merged.

- [ ] **Step 5: Stop at the publication boundary**

Report the local branch, commits, validation, review result, and proposed PR
title/body. Wait for explicit push/PR authorization.
