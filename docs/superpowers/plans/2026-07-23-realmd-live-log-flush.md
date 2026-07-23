# Realmd Live Log Flush Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make normal and debug realmd file-log records visible within approximately one second while the daemon remains active.

**Architecture:** Keep the shared 64 KiB file buffering intact. Extend the installed-runtime smoke test to require a live auth debug line, then flush the shared logger every ten iterations of realmd's existing 100 ms housekeeping loop and once after the final shutdown record.

**Tech Stack:** C++17, MaNGOS shared `Log`, PowerShell runtime smoke test, CMake/MSBuild, CTest.

## Global Constraints

- Limit production and test changes to the realmd submodule and its existing
  parent runtime smoke test; update the existing validation record with results.
- Do not change the shared logger, mangosd, configuration format, database, or client protocol behavior.
- Retain fully buffered log files; do not restore per-line flushing.
- Keep the implementation independent of client build and expansion.

---

### Task 1: Reproduce Missing Live Log Visibility

**Files:**
- Modify: `tests/RealmdAuthStreamSmoke.ps1`

**Interfaces:**
- Consumes: the existing temporary realmd configuration, build-5875 challenge, and tracked realmd process.
- Produces: a runtime assertion that `[Auth] Received command 0` reaches `realmd-smoke.log` before process shutdown.

- [ ] **Step 1: Configure a deterministic debug log filename**

Add these settings beside the existing `LogFile` override:

```powershell
$config = Set-ConfigValue -Lines $config -Name "LogLevel" -Value "3"
$config = Set-ConfigValue -Lines $config -Name "LogFileLevel" -Value "3"
$config = Set-ConfigValue -Lines $config -Name "LogTimestamp" -Value "0"
```

Set the path after `$stderrPath`:

```powershell
$logPath = Join-Path $testRoot "realmd-smoke.log"
```

- [ ] **Step 2: Add the live-log assertion**

Immediately after the complete challenge returns a response, poll for no longer
than three seconds while realmd remains active:

```powershell
$liveLogDeadline = [DateTime]::UtcNow.AddSeconds(3)
$liveAuthLineVisible = $false
while ([DateTime]::UtcNow -lt $liveLogDeadline) {
    if ((Test-Path -LiteralPath $logPath) -and
        (Select-String -LiteralPath $logPath `
            -SimpleMatch "[Auth] Received command 0" `
            -Quiet)) {
        $liveAuthLineVisible = $true
        break
    }
    Start-Sleep -Milliseconds 100
}

if (-not $liveAuthLineVisible) {
    $failures.Add(
        "debug auth records were not flushed to the live log within 3 seconds"
    )
}
```

Extend the success text to include `live debug log`.

- [ ] **Step 3: Run the installed-runtime test and verify RED**

Run:

```powershell
E:\Mangos\WIP\Zero\Testing\server\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe `
  -ConfigPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.conf
```

Expected: FAIL containing:

```text
debug auth records were not flushed to the live log within 3 seconds
```

- [ ] **Step 4: Commit the failing regression test**

```powershell
git add tests/RealmdAuthStreamSmoke.ps1
git commit -m "test: require live realmd debug logs"
```

### Task 2: Flush Realmd Logs Periodically and at Shutdown

**Files:**
- Modify: `src/realmd/Main.cpp`

**Interfaces:**
- Consumes: `Log::Flush()`, already supplied by the parent shared library.
- Produces: periodic live-log visibility and explicit orderly-shutdown durability.

- [ ] **Step 1: Add the periodic flush counter**

Initialize it beside the existing housekeeping counters:

```cpp
uint32 logFlushCounter = 0;
```

After `authServer.Update()` in the 100 ms loop, add:

```cpp
if ((++logFlushCounter) >= 10)
{
    logFlushCounter = 0;
    sLog.Flush();
}
```

- [ ] **Step 2: Add the final orderly-shutdown flush**

After the final shutdown record, add:

```cpp
sLog.outString("Halting process...");
sLog.Flush();
return exitCode;
```

- [ ] **Step 3: Build and install the focused target**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo --target realmd
cmake --install E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo
```

Expected: both commands exit zero and install the rebuilt `realmd.exe`.

- [ ] **Step 4: Run the runtime test and verify GREEN**

Run the Task 1 runtime command again.

Expected:

```text
Realmd auth-stream smoke passed: prompt reject, fragmented challenge, live debug log, idle timeout, process survival.
```

- [ ] **Step 5: Commit the realmd implementation**

```powershell
git -C src/realmd add Main.cpp
git -C src/realmd commit -m "fix: flush realmd logs during runtime"
```

### Task 3: Phase-End Validation and Deployment Record

**Files:**
- Modify: `src/realmd` parent submodule pointer
- Modify: `docs/superpowers/validation/realmd-auth-stream-safety.md`

**Interfaces:**
- Consumes: the committed parent smoke test and committed realmd implementation.
- Produces: a clean parent revision, refreshed Testing installation, and recorded live-client/logging evidence.

- [ ] **Step 1: Run focused and complete verification**

Run:

```powershell
cmake --build E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo
ctest --test-dir E:\Mangos\WIP\Zero\Testing\server_build `
  -C RelWithDebInfo --output-on-failure
cmake --install E:\Mangos\WIP\Zero\Testing\server_build `
  --config RelWithDebInfo
E:\Mangos\WIP\Zero\Testing\server\tests\RealmdAuthStreamSmoke.ps1 `
  -RealmdPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe `
  -ConfigPath E:\Mangos\WIP\Zero\Testing\server_install\realmd.conf
```

Expected: build and install exit zero, CTest reports `4/4` passed, and the
runtime smoke includes `live debug log`.

- [ ] **Step 2: Record the manual Classic result and logging fix**

Update the validation report to record:

- two successful build-5875 authentication and realm-list exchanges at
  `17:50:01` and `17:50:26`;
- manual arrival at the Classic character-selection screen;
- the live-log buffering root cause and periodic-flush correction;
- final build, test, runtime, deployment, and revision evidence.

- [ ] **Step 3: Verify repository and installation state**

Run:

```powershell
git diff --check
git -C src/realmd diff --check
git status --short
git -C src/realmd status --short
Get-FileHash -Algorithm SHA256 `
  E:\Mangos\WIP\Zero\Testing\server_build\src\realmd\RelWithDebInfo\realmd.exe
Get-FileHash -Algorithm SHA256 `
  E:\Mangos\WIP\Zero\Testing\server_install\realmd.exe
```

Expected: diff checks pass, only the intended parent pointer and validation
record remain before the final parent commit, the child is clean, and the two
binary hashes match.

- [ ] **Step 4: Commit the parent pointer and validation evidence**

```powershell
git add src/realmd docs/superpowers/validation/realmd-auth-stream-safety.md
git commit -m "docs: validate live realmd log flushing"
```

- [ ] **Step 5: Perform final cleanliness verification**

Run:

```powershell
git status --short
git -C src/realmd status --short
git -C E:\Mangos\Repos\Zero\server status --short
```

Expected: all three commands produce no entries.
