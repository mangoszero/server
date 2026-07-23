# Realmd Live Log Flush Design

Date: 2026-07-23

## Problem

The shared logger fully buffers normal, detail, and debug file output in a
64 KiB buffer. Mangosd calls `Log::Flush()` approximately once per second, but
realmd does not. During a low-volume realmd session, authentication records
therefore remain invisible on disk until the buffer fills or the process exits.

The Classic build-5875 login run demonstrated the defect: two successful login
and realm-list exchanges were emitted while realmd was active, but they became
visible in the timestamped log only when orderly shutdown closed the file.

## Scope

Change only the realmd submodule and its existing parent runtime smoke test.
Do not change the shared logger, mangosd, configuration format, database, or
client-specific protocol behavior.

## Design

Use realmd's existing 100 ms housekeeping loop to call `sLog.Flush()` every ten
iterations, nominally once per second. The counter is local to the main thread,
so it needs no additional synchronization. `Log::Flush()` already serializes
access to the buffered main log using the logger's file mutex.

Call `sLog.Flush()` once more during orderly realmd shutdown after network
workers have stopped and after the final `Halting process...` record is emitted.
This makes late shutdown records visible immediately and does not rely solely
on static destruction and `fclose()`.

Retain full buffering and avoid per-line flushing. This preserves the logging
performance behavior introduced for debug-level file output.

## Verification

Extend `tests/RealmdAuthStreamSmoke.ps1` with a live-log assertion:

1. Start the installed realmd with a temporary debug-level, timestamp-disabled
   log file.
2. Send the existing complete Classic build-5875 challenge while realmd remains
   active.
3. Wait slightly longer than the one-second flush interval.
4. Assert that the debug line for the received auth command is visible on disk
   before stopping the tracked realmd process.

Run this assertion against the current binary first and record the expected
failure. After implementation, rebuild and install realmd, then require the
same assertion to pass. Finish with the focused protocol test, the complete
CTest suite, the full build, and the existing hostile/fragmented/idle runtime
checks.

## Compatibility and Failure Handling

The change is independent of client build and expansion. It affects only file
log visibility. If `fflush()` encounters an operating-system error,
`Log::Flush()` remains best-effort as designed; authentication and networking
continue unaffected.
