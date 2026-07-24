# Realmd Realm State, Session-Key, and Patch Safety Design

Date: 2026-07-24

## Context

The Classic login audit identified three remaining shared-realmd risks:

- F-03: realm refresh mutates shared containers while network workers may still
  hold iterators and pointers into them.
- F-16: successful SRP proof performs a queued transaction, `FLUSH TABLES`, and
  up to ten seconds of polling, then authenticates even if the new session key
  was never observed.
- F-06: patch size, digest, and transferred bytes can describe different file
  generations, and resume offsets are not bounded by the advertised size.

It also identified F-05: accepted client builds never enter the patch path, so
an operator cannot deliberately require an upgrade from an otherwise supported
source build.

The implementation will live in the shared `mangos/realmd` repository. MaNGOS
Zero will be the integration, build, and runtime-validation host.

## Goals

- Realm-list readers always serialize one complete, immutable generation.
- At most one worker refreshes realm state at a time.
- Realm refresh never invalidates data still used by an existing response.
- Successful authentication is reported only after the session-key update
  completes successfully.
- No global table flush or database polling occurs during authentication.
- Patch metadata and transferred bytes come from one retained file generation.
- Invalid patch resume offsets fail cleanly.
- Operators can explicitly require an upgrade from selected accepted builds
  without changing default login behavior.
- The changes remain compatible with the parent APIs currently present in
  MaNGOS Zero, One, and Two.

## Non-goals

- Constructing or signing Blizzard patch archives.
- End-to-end client patch ingestion, until a suitable signed archive is
  available.
- Same-build content deployment, which requires persistent client receipt
  tracking to avoid an update loop.
- PIN authentication, extended proof records, or build allow-list changes.
- Character-screen or other mangosd behavior.
- Migrating Three or Four to the shared realmd submodule.

## F-03: Immutable Realm Snapshots

### Snapshot contents

Realmd will introduce an internal `RealmSnapshot` containing:

- the name-keyed realm map; and
- the per-client-version lists of pointers into that map.

The snapshot is fully built before publication and is never modified after it
becomes visible to readers. Map node addresses remain stable for the lifetime of
that snapshot, so the existing per-version pointer lists remain valid.

### Publication and refresh

`RealmList` will hold a `std::shared_ptr<const RealmSnapshot>`. Readers load it
with the C++17 atomic shared-pointer operations. A refresh builds a new mutable
snapshot privately and publishes it with one atomic store only after all realms
and version lists are complete.

A refresh mutex protects the update deadline and ensures only one database
refresh runs at a time. Readers do not take this mutex and therefore never hold
a realm-list lock while performing per-realm character-count database queries.

The initial empty snapshot is valid, so readers do not require null checks.
Database results preserve the current ordering and build-to-version behavior.

### Reader API

The iterator-pair and separate count calls will be replaced by one owning
`RealmListView`. The view retains the snapshot and exposes the selected
per-version list. `AuthSocket::LoadRealmlist` obtains exactly one view and uses
it for both the advertised count and every serialized record.

This prevents count/iterator generation mismatches and keeps the referenced
realms alive until response construction finishes, even if another worker
publishes a newer snapshot concurrently.

## F-16: Checked Session-Key Publication

The current transaction, `FLUSH TABLES`, verification query, and sleep loop will
be replaced by one synchronous `LoginDatabase.DirectPExecute` containing the
existing account update. This API locks the parent's synchronous database
connection, executes the statement immediately under normal autocommit
semantics, and returns the real database result.

On success, realmd continues with `SendProof`, enters `STATUS_AUTHED`, and
deactivates the authentication deadline.

On failure, realmd logs the account and database-publication failure, releases
the generated key string, closes the connection, and returns without sending a
successful proof or changing the authenticated counters. The work will not
introduce client-family result-code changes; those belong to F-14.

## F-05 and F-06: Explicit, Immutable Patch Delivery

### Patch policy

Realmd will add:

```ini
Patch.Enable = 1
Patch.ForceBuilds = ""
```

`Patch.Enable` preserves the existing default: when enabled, an unsupported
build receives `<sourceBuild><locale>.mpq` if that archive exists and otherwise
receives the invalid-version result. When disabled, no patch is offered.

`Patch.ForceBuilds` is an exact, whitespace-separated list of accepted source
builds that must upgrade. A configured forced build receives its matching
archive instead of completing normal authentication. If its required archive is
missing or invalid, login fails closed and the configuration error is logged.
An accepted build not listed in `Patch.ForceBuilds` continues to log in
normally.

The forced-build decision is made from the client's exact reported build. No
range or future-build matching is introduced.

### Authentication ordering

Patch selection moves until after the client's SRP proof has been verified.
This preserves the protocol response point while preventing an unauthenticated
peer from causing large patch files to be opened and hashed.

A client selected for patching does not publish a world session key and does
not enter `STATUS_AUTHED`. It receives the existing version-update and transfer
messages and enters `STATUS_PATCH`.

### Patch artifact

`PatchArtifact::Open` will:

1. open the archive once;
2. determine its size from that open stream;
3. hash that same stream;
4. rewind it; and
5. retain the stream, size, and digest as one move-only artifact.

The artifact is stored on the auth session after advertisement and moved to the
transfer worker after accept or resume. Advertisement and transfer therefore
cannot open two different path generations. The path-only digest cache is
removed so replacing a staged archive cannot retain a stale digest.

Resume is accepted only when `offset <= advertised_size`. Offset equal to size
is a valid completed transfer; an offset above size closes cleanly without
starting a transfer.

Files must be staged completely before being made visible under their final
name. In-place mutation of an already staged archive is unsupported; operators
replace archives atomically and restart or restage before offering them.

## Error Handling

- A rejected or failed realm refresh cannot expose a partially built snapshot.
- Existing readers retain their previous snapshot until they finish.
- Session-key update failure is fail-closed.
- Missing, empty, unreadable, or unhashable required patches fail closed.
- Patch transfer uses exactly the generation whose size and digest were
  advertised.
- Out-of-range patch resumes fail closed.
- Existing protocol framing and authentication deadlines remain unchanged.

## Tests

Test-first coverage will include:

- an old realm view remaining valid after a new snapshot is published;
- count and iteration coming from the same snapshot generation;
- concurrent snapshot readers and publishers under repeated generation changes;
- only one refresh owner entering the publication path at a time;
- a source-policy regression check prohibiting `FLUSH TABLES`, the
  `keyVerified` polling loop, and unchecked session-key publication;
- forced-build policy parsing and exact-build matching;
- accepted-build login behavior when it is not forced;
- patch artifact size and digest consistency;
- transfer from the retained artifact rather than reopening its path;
- resume at zero, within the file, exactly at end, and beyond end;
- missing and unreadable forced-patch failure;
- all existing realmd auth protocol tests.

Phase-end validation will include:

- the complete MaNGOS Zero Release build and install;
- all Zero CTest tests;
- the hostile auth-stream runtime smoke test;
- synthetic-file patch advertisement and transfer tests;
- a normal Classic authentication and realm-list smoke test when a live client
  is available; and
- one read-only Claude review focused on concurrency, database visibility,
  patch integrity, compatibility, and fail-closed behavior.

## Delivery

The shared realmd change will be one pull request with three focused commits:

1. publish immutable realm snapshots; and
2. require checked session-key publication; and
3. make patch upgrades explicit and artifact-consistent.

After realmd is merged, MaNGOS Zero will receive one parent commit updating the
submodule pointer. Other cores can then advance independently after their normal
build and login smoke tests.
