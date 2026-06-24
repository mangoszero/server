# CLAUDE.md

Context for AI assistants — the **Claude GitHub App** (`@claude` mentions) and contributors
using Claude Code / Claude.ai — working in this repository. Human contributors should also read
[`doc/CodingStandard.md`](doc/CodingStandard.md) and the installation guides linked from the README.

## What this is

**MangosZero** is the **Vanilla World of Warcraft** server of the MaNGOS project: an open-source
C++ game server that emulates retail patches **1.12.1 / 1.12.2 / 1.12.3**. It **does not** support
1.13.x (Classic) or any later expansion. Game and account data live in **MySQL / MariaDB**.
Licensed **GPL-2.0** (with an OpenSSL linking exception — see [`LICENSE`](LICENSE)).

This repository is the **server only**. Related repositories:

- **`mangoszero/database`** — world/realm/character SQL plus incremental migrations. Schema or
  reference-data changes belong there, **not** in this repo.
- Git submodules (see [`.gitmodules`](.gitmodules)): `dep` (mangosDeps), `src/realmd`,
  `src/modules/SD3` (ScriptDev3), `src/modules/Eluna`, `src/tools/Extractor_projects`,
  `win` (EasyBuild). Always clone and update **recursively**.

## Repository layout

- `src/` — server source
  - `src/mangosd/` — the world-server daemon entry point
  - `src/realmd/` — authentication / realm server (submodule)
  - `src/game/` — core gameplay: objects, packet handlers, AI, spells, etc. (e.g. `src/game/AuctionHouseBot/`)
  - `src/shared/` — shared infrastructure (database, threading, logging, networking)
  - `src/ipc/`, `src/ah-service/` — out-of-process service substrate and the Auction House service worker
  - `src/modules/` — `Eluna` (Lua scripting), `SD3` (ScriptDev3 C++ scripts), `Bots` (playerbots)
  - `src/tools/` — map / vmap / mmap extractors (submodule)
  - `src/genrev/` — build revision generation
- `cmake/` — build-system modules; `dep/` — bundled third-party libraries
- `doc/` — project documentation (coding standard, AuctionHouseBot, EventAI, ScriptDev, spells, …)
- `apps/`, `contrib/`, `linux/`, `dockercontainer/` — tooling, helpers, and packaging

## Build & test

CMake (≥ 3.12), C++. Builds on Linux / macOS / BSD (GCC or Clang) and Windows (MSVC ≥ 2015).
These are the exact flags CI uses:

```sh
git clone --recursive https://github.com/mangoszero/server.git
cd server
# Debian/Ubuntu build dependencies:
sudo apt-get install -y git cmake make build-essential \
  libssl-dev libbz2-dev default-libmysqlclient-dev libace-dev libreadline-dev
mkdir -p _build _install && cd _build
cmake .. \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=../_install \
  -DBUILD_TOOLS=1 -DBUILD_MANGOSD=1 -DBUILD_REALMD=1 -DSOAP=1 \
  -DSCRIPT_LIB_ELUNA=1 -DSCRIPT_LIB_SD3_GATE=1 -DPLAYERBOTS=1 \
  -DUSE_STORMLIB=1 -DBUILD_AH_SERVICE=1 -DPCH=0
make -j"$(nproc)" && make install -j"$(nproc)"
```

On Windows, use the EasyBuild helper in `win/`, or generate a Visual Studio solution with CMake.

A PR must keep the CI gates green:

- **Linux Build (GCC + Clang)** — GitHub Actions; builds **both** compilers on push to
  `master`/`devel` and on PRs to `master`.
- **Windows** — AppVeyor.
- **AH service IPC self-test** — CI runs `ah-service --selftest` and requires the output lines
  `intent codec selftest OK` and `ipc selftest OK`. Keep this passing if you touch `src/ipc/`
  or `src/ah-service/`.
- **Codacy** and **CodeFactor** — static-analysis quality gates.

## Coding standards

[`doc/CodingStandard.md`](doc/CodingStandard.md) is the source of truth. The rules reviewers
care about most:

- **Indent with 4 spaces — never tabs.** Aim for **80-column** lines; split long lines and align
  continuations to the position after the opening bracket.
- **Allman braces** — opening and closing brace each on their own line, closing brace under the opening:
  ```cpp
  if (something)
  {
      ...;
  }
  ```
- **Always brace single-statement blocks** (`if` / `for` / `while`), even one-liners — the standard
  requires it for clarity. Do **not** de-brace existing single-statement blocks.
- **One space before `(`, none inside**: `if (x)`, not `if( x )` or `if (x )`.
- **Doxygen comments**: `///` above a member, `///<` trailing on the same line, `/** ... */` for
  multi-line blocks.
- Class layout: indented `public:` / `private:` / `protected:` sections; constructor initializer
  lists on the next line, indented.
- Match the style of the surrounding code and keep diffs minimal and focused.

## Conventions & gotchas

- **Database changes live in `mangoszero/database`**, applied as transactional, idempotent
  `Rel##_##_###_*.sql` migrations that chain through `db_version` — never as ad-hoc schema edits here.
- **Submodules**: never shallow-update a submodule to a non-tip pinned SHA; update recursively with
  full history.
- **Keep the build green** — the project explicitly values green CI across *both* GCC and Clang and
  on Windows.
- The **in-process AuctionHouseBot is the default**; the out-of-process AH service is additive and
  default-off (`AH.Service.Enabled`). `mangosd` remains the **sole authority** over game state —
  any worker/service "intents" are re-validated server-side before being applied.
- Compatibility target is **1.12.x only** — do not introduce later-expansion assumptions.

## Review focus for the Claude GitHub App

When reviewing a PR here, prioritise, in order:

1. **Correctness & safety** in `src/game/` handlers and anything touching live world or DB state —
   auth, packet parsing, persistence, and the trust boundary between `mangosd` and any worker/service.
2. **Coding-standard conformance** (above) — flag tab/brace/spacing violations.
3. **Build & CI impact** — would this break the GCC *or* Clang build, the Windows build, or the
   `ah-service --selftest`?
4. **DB-migration correctness** when a server change implies schema/data — point contributors at the
   `mangoszero/database` migration pattern.
5. Keep feedback **concrete and minimal-diff**, and respect the project's small, friendly community.

For very large PRs, expect to **scope** the review — ask for a focused file range or subsystem rather
than attempting dozens of commits at once.
