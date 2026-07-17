# CLAUDE.md

Context for AI assistants — the Claude GitHub App (`@claude`) and contributors using Claude — working in
this repo. Humans: also read [`doc/CodingStandard.md`](doc/CodingStandard.md).

## Project

**MangosZero** — Vanilla World of Warcraft **1.12.x** server (C++, MySQL/MariaDB). Compatibility target is
**1.12.1–1.12.3 only**; do **not** introduce 1.13+/Classic or later-expansion assumptions.

- **Database changes go in the separate `mangoszero/database` repo**, not here — as transactional, idempotent
  `Rel##_##_###_*.sql` migrations that chain via `db_version`.
- Clone/update **recursively**: `dep`, `src/realmd`, `src/modules/{SD3,Eluna}`, `src/tools/Extractor_projects`
  and `win` are submodules. Never shallow-update a submodule to a non-tip pinned SHA.
- Less-obvious locations: out-of-process services in `src/ipc/` + `src/ah-service/`; scripting in
  `src/modules/` (Eluna = Lua, SD3 = C++, Bots = playerbots).

## Build & test

**C++17** — strict (`-std=c++17`, GNU extensions off); C code is C11. CMake ≥ 3.18; GCC/Clang
(Linux/macOS/BSD) or MSVC ≥ 2015 (Windows). The exact flags CI builds with:

```sh
git clone --recursive https://github.com/mangoszero/server.git && cd server
sudo apt-get install -y git cmake make build-essential \
  libssl-dev libbz2-dev default-libmysqlclient-dev libreadline-dev   # Debian/Ubuntu deps
mkdir -p _build _install && cd _build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../_install \
  -DBUILD_TOOLS=1 -DBUILD_MANGOSD=1 -DBUILD_REALMD=1 -DSOAP=1 \
  -DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD3=1 -DPLAYERBOTS=1 \
  -DUSE_STORMLIB=1 -DBUILD_AH_SERVICE=1 -DPCH=0
make -j"$(nproc)" && make install -j"$(nproc)"
```

Windows: use the EasyBuild helper in `win/`. **A PR MUST keep CI green:** the Linux build compiles with
**both** GCC and Clang, Windows builds on AppVeyor, and Codacy/CodeFactor gate quality. If you touch
`src/ipc/` or `src/ah-service/`, keep `ah-service --selftest` passing (it must print `intent codec selftest OK`
and `ipc selftest OK`).

## Code style

Source of truth: [`doc/CodingStandard.md`](doc/CodingStandard.md). Non-default rules:

- **4-space indent, never tabs**; ~80-column lines.
- **Allman braces**, and **YOU MUST brace single-statement blocks** — even one-line `if`/`for`/`while`. Do
  not de-brace existing ones.
- **One space before `(`, none inside**: `if (x)`, not `if( x )`.
- Doxygen: `///` above a member, `///<` trailing, `/** ... */` multi-line.

## Repository etiquette

- **Branches**: `type/kebab-description` (`feature/…`, `fix/…`, `docs/…`). Never push straight to `master`.
- **Keep PRs small and single-purpose.** Large multi-commit PRs are hard to review and can exceed the
  `@claude` reviewer's budget (it reacts 👀 but may never post). For a big change, ask `@claude` to review one
  subsystem/file range at a time rather than the whole branch.

## Architecture note

The **in-process AuctionHouseBot is the default**; the out-of-process AH service is additive and default-off
(`AH.Service.Enabled`). `mangosd` is the **sole authority** over game state — worker/service "intents" are
re-validated server-side before they are applied.

## Review focus (for `@claude`)

Prioritise: **(1)** correctness/safety in `src/game/` handlers and anything touching live world/DB state or
the `mangosd`↔worker trust boundary; **(2)** coding-standard conformance above; **(3)** build/CI impact (GCC
*and* Clang, Windows, the AH self-test); **(4)** DB-migration correctness (use the `mangoszero/database`
pattern). Keep feedback concrete and minimal-diff; flag correctness/standard issues, not style preferences
the standard doesn't cover.
