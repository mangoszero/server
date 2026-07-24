# Realmd State, Session, and Patch Hardening Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Eliminate realm-list publication races, require successful session-key persistence, and make realmd patch upgrades explicit and artifact-consistent.

**Architecture:** Realm refreshes build immutable snapshots and atomically publish them to lock-free readers. Authentication persists the world session key with one checked synchronous update. Patch policy is injected into each auth session, and one retained `PatchArtifact` supplies the advertised size, digest, resume validation, and transferred bytes.

**Tech Stack:** C++17, CMake/CTest, OpenSSL EVP, MaNGOS database API, Windows MSVC Release integration build, PowerShell runtime smoke tests.

## Global Constraints

- Production changes live in the shared `src/realmd` submodule.
- MaNGOS Zero at `E:\Mangos\WIP\Zero\Testing\server` is the integration host.
- Preserve all normal login behavior for builds not explicitly forced to patch.
- `Patch.Enable = 1` preserves unsupported-build patch offers by default.
- `Patch.ForceBuilds = ""` is empty by default and matches exact source builds only.
- Do not implement archive construction, publisher signing, client verifier changes, same-build content delivery, PIN authentication, or build-range changes.
- Use only APIs already present in Zero, One, and Two; `Database::DirectPExecute` is available in all three.
- Keep each production change behind a failing targeted test.
- Obtain one read-only Claude review after targeted checks pass.
- Do not update the Zero gitlink to an unmerged realmd commit on `master`.

---

### Task 1: Publish Immutable Realm Snapshots

**Files:**
- Create: `src/realmd/Realm/RealmSnapshot.h`
- Create: `src/realmd/Tests/RealmSnapshotTests.cpp`
- Modify: `src/realmd/Realm/RealmList.h`
- Modify: `src/realmd/Realm/RealmList.cpp`
- Modify: `src/realmd/Auth/AuthSocket.cpp`
- Modify: `src/realmd/CMakeLists.txt`

**Interfaces:**
- Produces: `RealmSnapshot`, `RealmSnapshotStore::Load()`, `RealmSnapshotStore::Publish(std::shared_ptr<RealmSnapshot>)`, `RealmListView`, and `RealmRefreshGate::RunIfDue(time_t, Callable)`.
- Produces: `RealmList::GetRealmsForBuild(uint32) const -> RealmListView`.
- Consumes: the existing `Realm`, `RealmVersion`, build mapping, and database row layout.

- [ ] **Step 1: Create the submodule feature branch**

Run:

```powershell
git -C src/realmd checkout -b fix/state-session-patch-hardening 253788fc3190e10068d07c139ee44fdba7085ea4
git -C src/realmd status --short --branch
```

Expected: branch `fix/state-session-patch-hardening`, with a clean submodule worktree.

- [ ] **Step 2: Add failing snapshot lifetime and concurrency tests**

Add `RealmSnapshotTests.cpp` with a local `CHECK` harness and these cases:

```cpp
auto first = std::make_shared<RealmSnapshot>();
auto [firstIt, inserted] = first->realms.emplace("First", Realm{});
CHECK(inserted);
firstIt->second.name = "First";
firstIt->second.m_ID = 1;
first->realmsByVersion[REALM_VERSION_VANILLA].push_back(&firstIt->second);

RealmSnapshotStore store;
store.Publish(first);
RealmListView oldView(store.Load(), REALM_VERSION_VANILLA);

auto second = std::make_shared<RealmSnapshot>();
auto [secondIt, insertedSecond] = second->realms.emplace("Second", Realm{});
CHECK(insertedSecond);
secondIt->second.name = "Second";
secondIt->second.m_ID = 2;
second->realmsByVersion[REALM_VERSION_VANILLA].push_back(&secondIt->second);
store.Publish(second);

CHECK(oldView.size() == 1);
CHECK((*oldView.begin())->name == "First");
RealmListView newView(store.Load(), REALM_VERSION_VANILLA);
CHECK((*newView.begin())->name == "Second");
```

Add a repeated publisher with four concurrent readers. Every loaded view must
contain exactly one realm and that realm's `name` and `m_ID` must describe the
same generation.

Add a refresh-gate test:

```cpp
RealmRefreshGate gate(20, 100);
std::atomic<int> entered{0};
std::vector<std::thread> callers;
for (int i = 0; i < 8; ++i)
{
    callers.emplace_back([&]
    {
        gate.RunIfDue(100, [&] { entered.fetch_add(1); });
    });
}
for (auto& caller : callers)
{
    caller.join();
}
CHECK(entered.load() == 1);
```

Register a new `realmd_realm_snapshot_tests` executable in
`src/realmd/CMakeLists.txt`.

- [ ] **Step 3: Run the new target and verify the red state**

Run:

```powershell
cmake -S . -B ..\server_build
cmake --build ..\server_build --config Release --target realmd_realm_snapshot_tests -- /m
```

Expected: compilation fails because `RealmSnapshot.h` and its interfaces do not
exist.

- [ ] **Step 4: Define the immutable snapshot types**

Move `RealmAddress`, `RealmBuildInfo`, `RealmVersion`, `RealmBuilds`, and
`Realm` from `RealmList.h` into `RealmSnapshot.h`; keep them transitively
available by including the new header from `RealmList.h`.

Define:

```cpp
using RealmMap = std::map<std::string, Realm>;
using RealmStlList = std::list<Realm const*>;

struct RealmSnapshot
{
    RealmMap realms;
    std::array<RealmStlList, REALM_VERSION_COUNT> realmsByVersion;
};

class RealmSnapshotStore
{
public:
    using SnapshotPtr = std::shared_ptr<RealmSnapshot const>;

    RealmSnapshotStore()
        : m_snapshot(std::make_shared<RealmSnapshot>())
    {
    }

    SnapshotPtr Load() const
    {
        return std::atomic_load_explicit(
            &m_snapshot, std::memory_order_acquire);
    }

    void Publish(std::shared_ptr<RealmSnapshot> snapshot)
    {
        SnapshotPtr immutable = std::move(snapshot);
        std::atomic_store_explicit(
            &m_snapshot, std::move(immutable), std::memory_order_release);
    }

private:
    SnapshotPtr m_snapshot;
};
```

Define `RealmListView` so it retains a `SnapshotPtr`, selects one immutable
`RealmStlList`, and exposes `begin()`, `end()`, `size()`, and `empty()`.

```cpp
class RealmListView
{
public:
    using const_iterator = RealmStlList::const_iterator;

    RealmListView(
        RealmSnapshotStore::SnapshotPtr snapshot,
        RealmVersion version)
        : m_snapshot(std::move(snapshot)),
          m_realms(&m_snapshot->realmsByVersion[static_cast<std::size_t>(version)])
    {
    }

    const_iterator begin() const { return m_realms->begin(); }
    const_iterator end() const { return m_realms->end(); }
    std::size_t size() const { return m_realms->size(); }
    bool empty() const { return m_realms->empty(); }

private:
    RealmSnapshotStore::SnapshotPtr m_snapshot;
    RealmStlList const* m_realms;
};
```

Define `RealmRefreshGate` with an internal mutex, interval, and next-update
time:

```cpp
class RealmRefreshGate
{
public:
    RealmRefreshGate(uint32 interval = 0, time_t nextUpdate = 0)
        : m_interval(interval), m_nextUpdate(nextUpdate)
    {
    }

    void Reset(uint32 interval, time_t nextUpdate)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_interval = interval;
        m_nextUpdate = nextUpdate;
    }

    template<class Refresh>
    bool RunIfDue(time_t now, Refresh&& refresh)
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_interval || m_nextUpdate > now)
        {
            return false;
        }

        m_nextUpdate = now + m_interval;
        std::forward<Refresh>(refresh)();
        return true;
    }

private:
    std::mutex m_mutex;
    uint32 m_interval;
    time_t m_nextUpdate;
};
```

`RunIfDue` holds the mutex through the supplied refresh callback, advances the
deadline once, and returns whether the callback ran.

- [ ] **Step 5: Refactor RealmList to build privately and publish once**

Change the private methods to:

```cpp
std::shared_ptr<RealmSnapshot> BuildSnapshot(bool init);
void AddRealmToBuildList(RealmSnapshot& snapshot, Realm const& realm);
void UpdateRealm(
    RealmSnapshot& snapshot,
    uint32 ID,
    std::string const& name,
    RealmAddress const& address,
    RealmAddress const& localAddress,
    RealmAddress const& localSubnetmask,
    uint32 port,
    uint8 icon,
    RealmFlags realmflags,
    uint8 timezone,
    AccountTypes allowedSecurityLevel,
    float population,
    std::string const& builds);
```

`Initialize` builds and publishes the initial snapshot. `UpdateIfNeed` calls
`RealmRefreshGate::RunIfDue`, builds a complete replacement, and performs one
`RealmSnapshotStore::Publish`.

Replace the iterator-pair API with:

```cpp
RealmListView RealmList::GetRealmsForBuild(uint32 build) const
{
    return RealmListView(m_snapshots.Load(), BelongsToVersion(build));
}
```

Make `size()` read from one loaded snapshot. Remove
`GetIteratorsForBuild` and `NumRealmsForBuild`.

- [ ] **Step 6: Serialize one owning view per realm-list response**

In `AuthSocket::LoadRealmlist`, replace separate iterator and count calls with:

```cpp
RealmListView const realms = sRealmList.GetRealmsForBuild(_build);
uint32 const numRealms = static_cast<uint32>(realms.size());
```

Change both serialization loops to:

```cpp
for (Realm const* realm : realms)
{
    // Existing field and character-count serialization uses realm->...
}
```

Do not hold the refresh mutex or any snapshot-store lock while querying
`realmcharacters`.

- [ ] **Step 7: Run focused and existing realmd tests**

Run:

```powershell
cmake -S . -B ..\server_build
cmake --build ..\server_build --config Release --target realmd_realm_snapshot_tests realmd_auth_protocol_tests realmd -- /m
ctest --test-dir ..\server_build -C Release -R "^realmd_(realm_snapshot|auth_protocol)_tests$" --output-on-failure
```

Expected: both tests pass and `realmd.exe` links.

- [ ] **Step 8: Commit the snapshot change**

Run:

```powershell
git -C src/realmd add Realm Auth/AuthSocket.cpp Tests/RealmSnapshotTests.cpp CMakeLists.txt
git -C src/realmd commit -m "fix: publish immutable realm snapshots"
```

Expected: one focused realmd commit with no parent files staged.

---

### Task 2: Require Successful Session-Key Publication

**Files:**
- Create: `src/realmd/Tests/CheckAuthSafety.cmake`
- Modify: `src/realmd/Auth/AuthSocket.cpp`
- Modify: `src/realmd/CMakeLists.txt`

**Interfaces:**
- Consumes: `Database::DirectPExecute(const char*, ...) -> bool`.
- Produces: fail-closed authentication when the account update returns false.

- [ ] **Step 1: Add a failing source-policy regression test**

Create `CheckAuthSafety.cmake`:

```cmake
file(READ "${REALMD_SOURCE}/Auth/AuthSocket.cpp" AUTH_SOCKET)

foreach(FORBIDDEN_TEXT "FLUSH TABLES" "keyVerified")
  string(FIND "${AUTH_SOCKET}" "${FORBIDDEN_TEXT}" POSITION)
  if(NOT POSITION EQUAL -1)
    message(FATAL_ERROR "Forbidden auth publication pattern remains: ${FORBIDDEN_TEXT}")
  endif()
endforeach()

string(FIND "${AUTH_SOCKET}"
  "if (!LoginDatabase.DirectPExecute(" CHECKED_UPDATE)
if(CHECKED_UPDATE EQUAL -1)
  message(FATAL_ERROR "Session-key update is not checked synchronously")
endif()

string(FIND "${AUTH_SOCKET}" "SendProof(sha);" SEND_PROOF)
if(SEND_PROOF EQUAL -1 OR CHECKED_UPDATE GREATER SEND_PROOF)
  message(FATAL_ERROR "Successful proof can precede checked session-key publication")
endif()
```

Register it as `realmd_auth_safety_policy`.

- [ ] **Step 2: Run the policy test and verify the red state**

Run:

```powershell
cmake -S . -B ..\server_build
ctest --test-dir ..\server_build -C Release -R "^realmd_auth_safety_policy$" --output-on-failure
```

Expected: failure reporting `FLUSH TABLES`.

- [ ] **Step 3: Replace the transaction, flush, and poll**

Replace the current publication block with:

```cpp
LoginDatabase.escape_string(_os);
bool const published = LoginDatabase.DirectPExecute(
    "UPDATE `account` SET `sessionkey` = '%s', `last_ip` = '%s', "
    "`last_login` = NOW(), `locale` = '%u', `os` = '%s', "
    "`failed_logins` = 0 WHERE `username` = '%s'",
    K_hex,
    get_remote_address().c_str(),
    GetLocaleByName(_localizationName),
    _os.c_str(),
    _safelogin.c_str());

if (!published)
{
    sLog.outError(
        "[Auth] Failed to publish session key for account %s",
        _login.c_str());
    OPENSSL_free(const_cast<char*>(K_hex));
    close_connection();
    return false;
}

OPENSSL_free(const_cast<char*>(K_hex));
```

Delete `START TRANSACTION`, `COMMIT`, `FLUSH TABLES`, the verification query,
the 1,000-attempt loop, and its sleep. Leave `SendProof`, `STATUS_AUTHED`, the
deadline transition, and counters after the checked update.

- [ ] **Step 4: Run focused tests and build**

Run:

```powershell
cmake --build ..\server_build --config Release --target realmd -- /m
ctest --test-dir ..\server_build -C Release -R "^realmd_(auth_safety_policy|realm_snapshot_tests|auth_protocol_tests)$" --output-on-failure
```

Expected: policy, snapshot, and protocol tests pass.

- [ ] **Step 5: Commit checked publication**

Run:

```powershell
git -C src/realmd add Auth/AuthSocket.cpp Tests/CheckAuthSafety.cmake CMakeLists.txt
git -C src/realmd commit -m "fix: require session-key publication success"
```

Expected: a second focused realmd commit.

---

### Task 3: Add Explicit and Artifact-Consistent Patch Delivery

**Files:**
- Create: `src/realmd/Auth/PatchPolicy.h`
- Create: `src/realmd/Auth/PatchPolicy.cpp`
- Create: `src/realmd/Auth/PatchArtifact.h`
- Create: `src/realmd/Auth/PatchArtifact.cpp`
- Create: `src/realmd/Tests/PatchSafetyTests.cpp`
- Create: `src/realmd/Tests/CheckPatchSafety.cmake`
- Modify: `src/realmd/Auth/PatchHandler.h`
- Modify: `src/realmd/Auth/PatchHandler.cpp`
- Modify: `src/realmd/Auth/AuthSocket.h`
- Modify: `src/realmd/Auth/AuthSocket.cpp`
- Modify: `src/realmd/Auth/AuthServer.h`
- Modify: `src/realmd/Auth/AuthServer.cpp`
- Modify: `src/realmd/Main.cpp`
- Modify: `src/realmd/realmd.conf.dist.in`
- Modify: `src/realmd/CMakeLists.txt`

**Interfaces:**
- Produces: `PatchPolicy::Parse(bool, std::string const&) -> std::optional<PatchPolicy>`.
- Produces: `PatchPolicy::ShouldPatch(uint16_t, bool supported) const -> bool`.
- Produces: `PatchArtifact::Open(std::string const&) -> std::unique_ptr<PatchArtifact>`.
- Produces: `StartPatchTransfer(..., std::unique_ptr<PatchArtifact>, uint64_t) -> bool`.
- Consumes: exact client build, locale, SRP proof result, network sender/closer, and flow control.

- [ ] **Step 1: Add failing patch-policy tests**

Add tests that assert:

```cpp
auto defaults = PatchPolicy::Parse(true, "");
CHECK(defaults.has_value());
CHECK(defaults->ShouldPatch(5000, false));
CHECK(!defaults->ShouldPatch(5875, true));

auto forced = PatchPolicy::Parse(true, "5875 6005");
CHECK(forced.has_value());
CHECK(forced->ShouldPatch(5875, true));
CHECK(forced->ShouldPatch(6005, true));
CHECK(!forced->ShouldPatch(6141, true));

auto disabled = PatchPolicy::Parse(false, "5875");
CHECK(disabled.has_value());
CHECK(!disabled->ShouldPatch(5000, false));
CHECK(!disabled->ShouldPatch(5875, true));

CHECK(!PatchPolicy::Parse(true, "0").has_value());
CHECK(!PatchPolicy::Parse(true, "65536").has_value());
CHECK(!PatchPolicy::Parse(true, "5875x").has_value());
```

Register `realmd_patch_safety_tests` with `PatchPolicy.cpp` and
`PatchArtifact.cpp`.

- [ ] **Step 2: Run the patch target and verify the red state**

Run:

```powershell
cmake -S . -B ..\server_build
cmake --build ..\server_build --config Release --target realmd_patch_safety_tests -- /m
```

Expected: compilation fails because `PatchPolicy` and `PatchArtifact` do not
exist.

- [ ] **Step 3: Implement exact forced-build policy**

Define:

```cpp
class PatchPolicy
{
public:
    static std::optional<PatchPolicy> Parse(
        bool enabled, std::string const& forcedBuilds);

    bool ShouldPatch(std::uint16_t build, bool supported) const
    {
        return m_enabled &&
            (!supported || m_forcedBuilds.count(build) != 0);
    }

private:
    bool m_enabled = true;
    std::set<std::uint16_t> m_forcedBuilds;
};
```

Parse whitespace-separated tokens with `std::from_chars`. Reject an empty
numeric value, trailing characters, zero, and values above `uint16_t` maximum.
Duplicates collapse through the set.

- [ ] **Step 4: Add failing retained-artifact tests**

Use a unique file under `std::filesystem::temp_directory_path()` containing
`abcdef`. Assert:

```cpp
auto artifact = PatchArtifact::Open(path.string());
CHECK(artifact != nullptr);
CHECK(artifact->size() == 6);
CHECK(Hex(artifact->digest()) == "e80b5017098950fc58aad83c8c14978e");

CHECK(artifact->Seek(0));
CHECK(ReadAll(*artifact) == "abcdef");
CHECK(artifact->Seek(3));
CHECK(ReadAll(*artifact) == "def");
CHECK(artifact->Seek(artifact->size()));
CHECK(ReadAll(*artifact).empty());
CHECK(!artifact->Seek(artifact->size() + 1));
CHECK(PatchArtifact::Open(missingPath.string()) == nullptr);
CHECK(PatchArtifact::Open(std::filesystem::temp_directory_path().string()) == nullptr);
```

Expected red state: artifact declarations exist after Step 3 only if the header
was scaffolded; otherwise compilation still reports the missing artifact API.

- [ ] **Step 5: Implement the move-only patch artifact**

Define a move-only class holding one `std::ifstream`, `uint64_t` size, and
`std::array<std::uint8_t, MD5_DIGEST_LENGTH>` digest:

```cpp
class PatchArtifact
{
public:
    static std::unique_ptr<PatchArtifact> Open(std::string const& path);

    PatchArtifact(PatchArtifact&&) = default;
    PatchArtifact& operator=(PatchArtifact&&) = default;
    PatchArtifact(PatchArtifact const&) = delete;
    PatchArtifact& operator=(PatchArtifact const&) = delete;

    std::uint64_t size() const;
    std::array<std::uint8_t, MD5_DIGEST_LENGTH> const& digest() const;
    bool Seek(std::uint64_t offset);
    std::streamsize Read(std::uint8_t* destination, std::size_t capacity);
    bool bad() const;
};
```

`Open` opens with `std::ios::binary | std::ios::ate`, rejects non-positive or
unrepresentable size, seeks to zero, computes MD5 through EVP from that stream,
checks `bad()`, finalizes exactly `MD5_DIGEST_LENGTH` bytes, clears EOF state,
and rewinds the same stream.

- [ ] **Step 6: Pass ownership into the transfer thread**

Delete `PatchCache`. Change the transfer signature to:

```cpp
bool StartPatchTransfer(
    net::Sender sender,
    net::Closer closer,
    std::shared_ptr<net::FlowControl> flow,
    std::unique_ptr<PatchArtifact> artifact,
    std::uint64_t startOffset);
```

Reject a null artifact or `startOffset > artifact->size()`. Seek before spawning
the thread. Move the artifact into the thread lambda and read each
`CMD_XFER_DATA` payload through `artifact->Read`. Keep the existing 4 KiB chunks,
256 KiB backpressure ceiling, delayed start, and close-on-read-error behavior.

- [ ] **Step 7: Inject patch policy into auth sessions**

Extend:

```cpp
bool AuthServer::Start(
    uint16_t port,
    std::string const& bindIp,
    std::chrono::seconds authTimeout,
    PatchPolicy patchPolicy);
```

Store the policy in `AuthServer::Impl` and pass it to every new `AuthSocket`.
Store it by value in `AuthSocket`.

In `Main.cpp`:

```cpp
bool const patchEnabled =
    sConfig.GetBoolDefault("Patch.Enable", true);
auto patchPolicy = PatchPolicy::Parse(
    patchEnabled,
    sConfig.GetStringDefault("Patch.ForceBuilds", ""));
if (!patchPolicy)
{
    sLog.outError("Invalid Patch.ForceBuilds configuration");
    return 1;
}
```

Pass `std::move(*patchPolicy)` to `AuthServer::Start`.

- [ ] **Step 8: Move patch selection behind verified SRP**

Remove the unsupported-build offer from the beginning of
`_HandleLogonProof`. After `M1` matches and before session-key publication:

```cpp
bool const supported = FindBuildInfo(_build) != nullptr;
if (_patchPolicy.ShouldPatch(_build, supported))
{
    return OfferPatch();
}
if (!supported)
{
    SendInvalidVersion();
    return true;
}
```

`OfferPatch` constructs `./patches/<build><locale>.mpq`, calls
`PatchArtifact::Open`, and uses that artifact's size and digest in
`XFER_INITIATE`. It stores the artifact in
`std::unique_ptr<PatchArtifact> m_patchArtifact`, sends
`WOW_FAIL_VERSION_UPDATE`, enters `STATUS_PATCH`, and deactivates the auth
deadline.

If a required artifact cannot be opened, log its exact path, send the existing
invalid-version response, and remain unauthenticated.

`BeginPatchStream` moves `m_patchArtifact` into `StartPatchTransfer`; it never
reopens the pathname.

- [ ] **Step 9: Add patch configuration documentation**

Add to `realmd.conf.dist.in`:

```ini
# Patch.Enable
#     Allow realmd to offer build+locale MPQ upgrades.
#     Default: 1
#
# Patch.ForceBuilds
#     Exact accepted source builds that must consume a staged upgrade before
#     login. Whitespace-separated. Empty preserves normal accepted-build login.
#     Default: ""

Patch.Enable           = 1
Patch.ForceBuilds      = ""
```

Document the staging filename, atomic replacement requirement, signed-archive
requirement, and the exclusion of same-build content pushes.

- [ ] **Step 10: Add a patch source-policy test**

`CheckPatchSafety.cmake` must reject:

```cmake
foreach(FORBIDDEN_TEXT "class PatchCache" "GetMD5(" "StartPatchTransfer(m_sender, m_closer, m_flowControl, _patchPath")
```

It must require `std::unique_ptr<PatchArtifact>`,
`startOffset > artifact->size()`, `Patch.ForceBuilds`, and the placement of
`_patchPolicy.ShouldPatch` after the `memcmp(M.AsByteArray(), lp.M1, 20)`
success branch. It must also require the null-artifact branch to call
`SendInvalidVersion()` before returning.

Register the check as `realmd_patch_safety_policy`.

- [ ] **Step 11: Run all focused realmd checks**

Run:

```powershell
cmake -S . -B ..\server_build
cmake --build ..\server_build --config Release --target realmd_patch_safety_tests realmd_realm_snapshot_tests realmd_auth_protocol_tests realmd -- /m
ctest --test-dir ..\server_build -C Release -R "^realmd_" --output-on-failure
```

Expected: patch, snapshot, protocol, auth-policy, and patch-policy tests pass.

- [ ] **Step 12: Commit patch delivery**

Run:

```powershell
git -C src/realmd add Auth Main.cpp realmd.conf.dist.in Tests CMakeLists.txt
git -C src/realmd commit -m "fix: make patch delivery explicit and immutable"
```

Expected: the third focused realmd commit.

---

### Task 4: Validate and Review the Shared Realmd Change

**Files:**
- Verify only; production edits are limited to concrete BLOCKING or IMPORTANT review findings.

**Interfaces:**
- Consumes: the three realmd commits from Tasks 1–3.
- Produces: build, test, runtime, compatibility, and external-review evidence.

- [ ] **Step 1: Inspect final scope**

Run:

```powershell
git -C src/realmd status --short --branch
git -C src/realmd log --oneline 253788fc..HEAD
git -C src/realmd diff --check 253788fc..HEAD
git -C src/realmd diff --stat 253788fc..HEAD
git status --short --branch
```

Expected: three realmd commits; parent shows only the submodule pointer plus the
already committed design and plan documents.

- [ ] **Step 2: Run the full Release build and install**

Run:

```powershell
cmake -S . -B ..\server_build
cmake --build ..\server_build --config Release --target INSTALL -- /m
```

If the foreground window yields while MSBuild continues, wait for that exact
process to finish and rerun the incremental `INSTALL` target to obtain exit
code zero.

- [ ] **Step 3: Run the full Zero test suite**

Run:

```powershell
ctest --test-dir ..\server_build -C Release --output-on-failure
```

Expected: every registered test passes, including all new `realmd_*` tests.

- [ ] **Step 4: Run installed runtime smoke**

Run:

```powershell
& .\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath ..\server_install\realmd.exe `
  -ConfigPath ..\server_install\realmd.conf
```

Expected: prompt rejection, fragmented challenge, live debug log, idle timeout,
and process-survival checks pass.

Run:

```powershell
$buildHash = (Get-FileHash ..\server_build\src\realmd\Release\realmd.exe -Algorithm SHA256).Hash
$installHash = (Get-FileHash ..\server_install\realmd.exe -Algorithm SHA256).Hash
if ($buildHash -ne $installHash) { throw "Installed realmd differs from build output" }
```

Expected: hashes match.

- [ ] **Step 5: Obtain the required read-only Claude review**

Create a timestamped ledger under the system temporary directory. Supply only
the goal, constraints, `253788fc..HEAD` diff, completed tests, known absence of
an end-to-end signed patch archive, and these review questions:

- Can a reader observe a partial or invalid realm snapshot?
- Can refresh deadlock or serialize readers behind database work?
- Can authentication succeed after a failed session-key update?
- Can advertised patch bytes differ from transferred bytes?
- Can forced-patch configuration affect an unlisted accepted build?
- Are Zero, One, and Two parent API assumptions valid?

Run Claude with:

```powershell
claude -p --permission-mode plan --max-turns 6 `
  --output-format stream-json --include-partial-messages --verbose `
  $reviewPrompt | Tee-Object -FilePath $ledger
```

Poll the ledger at least once per minute. Fix only concrete BLOCKING or
IMPORTANT findings, run the smallest relevant check, and permit at most one
focused re-review.

- [ ] **Step 6: Re-run final verification after any review fix**

Run:

```powershell
cmake --build ..\server_build --config Release --target INSTALL -- /m
ctest --test-dir ..\server_build -C Release --output-on-failure
& .\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath ..\server_install\realmd.exe `
  -ConfigPath ..\server_install\realmd.conf
git -C src/realmd diff --check 253788fc..HEAD
```

Expected: build, complete CTest, runtime smoke, and diff checks pass.

---

### Task 5: Publish Realmd and Land the Merged Pointer in Zero

**Files:**
- Modify after realmd merge: `src/realmd` gitlink
- Include in the Zero branch: `docs/superpowers/specs/2026-07-24-realmd-realm-state-publication-design.md`
- Include in the Zero branch: `docs/superpowers/plans/2026-07-24-realmd-state-session-patch-hardening.md`

**Interfaces:**
- Consumes: reviewed and verified realmd feature branch.
- Produces: one realmd pull request, then one Zero commit pointing to the
  squash-merged realmd master.

- [ ] **Step 1: Publish the realmd feature branch**

Use the `github:yeet` workflow. Confirm the submodule diff and three commits,
then run:

```powershell
git -C src/realmd push -u origin fix/state-session-patch-hardening
```

Open a ready-for-review realmd PR titled:

```text
Harden realm state, session publication, and patch delivery
```

The PR description must cover F-03, F-16, F-05/F-06, tests, compatibility, and
the deferred signed-client-patch ingestion test.

- [ ] **Step 2: Wait for review, checks, and squash merge**

Do not advance Zero `master` to the feature commit. After the realmd PR is
squash merged:

```powershell
git -C src/realmd fetch origin master
$realmdPr = gh pr list --repo mangos/realmd `
  --head fix/state-session-patch-hardening --state merged `
  --json number --jq ".[0].number"
if (-not $realmdPr) { throw "Merged realmd PR was not found" }
$mergedRealmd = gh pr view $realmdPr --repo mangos/realmd `
  --json mergeCommit --jq ".mergeCommit.oid"
git -C src/realmd cat-file -e "$mergedRealmd`^{commit}"
if ($LASTEXITCODE -ne 0) { throw "Merged realmd commit is unavailable locally" }
git -C src/realmd checkout --detach $mergedRealmd
```

- [ ] **Step 3: Verify the exact merged pointer in Zero**

Run the full Release `INSTALL`, full CTest, runtime smoke, and hash comparison
again against the detached merged realmd commit.

Expected: the merged commit reproduces all pre-merge evidence.

- [ ] **Step 4: Commit and publish the Zero pointer**

Run:

```powershell
git add -- src/realmd `
  docs/superpowers/specs/2026-07-24-realmd-realm-state-publication-design.md `
  docs/superpowers/plans/2026-07-24-realmd-state-session-patch-hardening.md
git commit -m "chore: integrate realmd state and patch hardening"
```

Use the `github:yeet` workflow to push
`fix/realmd-realm-state-publication` and open the Zero integration PR. Its
description must state the exact merged realmd SHA and the final build, CTest,
runtime-smoke, and binary-hash evidence.
