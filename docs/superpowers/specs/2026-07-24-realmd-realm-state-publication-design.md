# Realmd Realm State and Session-Key Publication Design

Date: 2026-07-24

## Context

The Classic login audit identified two remaining shared-realmd risks:

- F-03: realm refresh mutates shared containers while network workers may still
  hold iterators and pointers into them.
- F-16: successful SRP proof performs a queued transaction, `FLUSH TABLES`, and
  up to ten seconds of polling, then authenticates even if the new session key
  was never observed.

The implementation will live in the shared `mangos/realmd` repository. MaNGOS
Zero will be the integration, build, and runtime-validation host.

## Goals

- Realm-list readers always serialize one complete, immutable generation.
- At most one worker refreshes realm state at a time.
- Realm refresh never invalidates data still used by an existing response.
- Successful authentication is reported only after the session-key update
  completes successfully.
- No global table flush or database polling occurs during authentication.
- The changes remain compatible with the parent APIs currently present in
  MaNGOS Zero, One, and Two.

## Non-goals

- Patch packaging, patch signing, and same-build patch deployment.
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

## Error Handling

- A rejected or failed realm refresh cannot expose a partially built snapshot.
- Existing readers retain their previous snapshot until they finish.
- Session-key update failure is fail-closed.
- Existing protocol framing and authentication deadlines remain unchanged.

## Tests

Test-first coverage will include:

- an old realm view remaining valid after a new snapshot is published;
- count and iteration coming from the same snapshot generation;
- concurrent snapshot readers and publishers under repeated generation changes;
- only one refresh owner entering the publication path at a time;
- a source-policy regression check prohibiting `FLUSH TABLES`, the
  `keyVerified` polling loop, and unchecked session-key publication;
- all existing realmd auth protocol tests.

Phase-end validation will include:

- the complete MaNGOS Zero Release build and install;
- all Zero CTest tests;
- the hostile auth-stream runtime smoke test;
- a normal Classic authentication and realm-list smoke test when a live client
  is available; and
- one read-only Claude review focused on concurrency, database visibility,
  compatibility, and fail-closed behavior.

## Delivery

The shared realmd change will be one pull request with two focused commits:

1. publish immutable realm snapshots; and
2. require checked session-key publication.

After realmd is merged, MaNGOS Zero will receive one parent commit updating the
submodule pointer. Other cores can then advance independently after their normal
build and login smoke tests.
