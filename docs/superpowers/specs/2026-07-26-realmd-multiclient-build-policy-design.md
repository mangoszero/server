# Realmd Multi-Client Build Policy Design

**Goal:** Make the shared realmd accept only explicitly supported client builds
and choose build-sensitive authentication results at runtime rather than from
the parent core's compile-time expansion setting.

## Scope

This change resolves audit findings F-14 and F-15 in the shared `realmd`
submodule. It does not change SRP, patch transport, realm packet layouts, PIN
authentication, telemetry proof records, or mangosd behavior.

The current explicit builds remain supported:

- Vanilla: 5875, 6005, 6141
- The Burning Crusade: 8606
- Wrath of the Lich King: 12340
- Cataclysm: 15595
- Mists of Pandaria: 18273, 18414
- Warlords of Draenor: 21742
- Legion: 26972
- Battle for Azeroth: 35662
- Shadowlands: 40000

Unknown values, including builds above 40000, are unsupported. They follow the
existing invalid-version or configured patch-policy path after valid SRP proof.

## Considered Approaches

### 1. Shared explicit build policy (selected)

Keep one table containing each supported build's version metadata and runtime
family. Both supported-build lookup and build-family lookup consume that table.
Authentication uses the returned family to select the IP-lock result.

This removes the unsafe open-ended range and avoids duplicating build lists.

### 2. Local conditional fixes

Remove the high-build range in `FindBuildInfo` and add an independent build
switch in `AuthSocket`. This has the smallest initial diff but creates two build
policies that can drift, so it is rejected.

### 3. Configurable accepted ranges

Allow operators to configure build ranges. This would reintroduce acceptance of
packet layouts that realmd has not proved compatible and would complicate patch
selection. It is rejected.

## Design

The realm build table will associate every `RealmBuildInfo` entry with its
`RealmVersion`. Lookup is exact for every entry, including build 40000.

Two read-only operations expose the policy:

1. find exact version metadata for a client build;
2. find the runtime realm/client family for a client build.

Realm-list grouping and authentication result selection use the same family
lookup. Existing fallback behavior for malformed/custom realm database rows can
remain Vanilla where required internally, but it must not make an unknown
connecting client supported.

For an IP-locked account whose source address does not match:

- explicit Vanilla builds return `WOW_FAIL_DB_BUSY`, which Classic understands;
- explicit TBC-and-later builds return `WOW_FAIL_LOCKED_ENFORCED`;
- unknown builds return the conservative generic `WOW_FAIL_DB_BUSY` because
  their supported result vocabulary is not known.

The compile-time `CLASSIC` branch is removed from this decision. No parent core
can therefore impose its own expansion semantics on every client handled by the
shared realmd process.

## Error Handling

Unknown builds remain subject to the current fail-closed authentication and
patch policy. This package does not expose build support through account lookup
results and does not broaden patch eligibility.

No configuration migration is required. Operators that intentionally test a
new client build must first add its exact build metadata and family in code so
packet handling can be reviewed with it.

## Verification

Tests will demonstrate the behavior before implementation and then cover:

- every currently supported build resolves to its existing version metadata;
- 39999, 40001, and 65535 are rejected as unsupported;
- Vanilla builds select `WOW_FAIL_DB_BUSY`;
- every explicitly supported later build selects
  `WOW_FAIL_LOCKED_ENFORCED`;
- unknown builds select the conservative generic result;
- the parent-core compile-time expansion does not affect the decision;
- all existing realmd and Zero tests remain green.

Phase-end validation uses the designated Zero Testing checkout: complete Release
install, full CTest, installed auth-stream smoke, and build/install hash match.
Available client-family login smokes remain a separate user-driven gate.

## Delivery

Production and focused-test changes are committed in the shared `realmd`
submodule on a feature branch based on current realmd master. After review and
merge, Zero receives a separate commit advancing only the final realmd gitlink.
