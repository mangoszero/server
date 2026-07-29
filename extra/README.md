# extra/

Everything in this tree is **support material**, not server code. Nothing here is
compiled into `mangosd` or `realmd`, and nothing under `src/` includes or links
against it. It was moved out of the repository root so that the top level shows
only what the build actually consumes: `src/`, `dep/` and `cmake/`.

Delete any subdirectory here and the servers still build.

| Directory | What it is |
|---|---|
| [`doc/`](doc/) | All project documentation, and the Doxygen configuration that turns the annotated headers under `src/` into browsable API docs. This is the former `doc/` and `docs/` merged; they had no filenames in common, so the merge lost nothing. |
| [`docker/`](docker/) | Container build files: one Dockerfile per server plus a `docker-compose.yml` that stands both up alongside a database. Formerly `dockercontainer/`. |
| [`linux/`](linux/) | `getmangos.sh`, the interactive build-and-install script for Linux distributions. It clones, configures, compiles and can install the databases; it is a convenience wrapper around the same CMake build described in the top-level README, not a required step. |
| [`win/`](win/) | **Git submodule** ([mangostools/EasyBuild](https://github.com/mangostools/EasyBuild)). A Windows GUI that fetches dependencies and drives the MSVC build. Because it is a submodule, its path is recorded in `.gitmodules` at the repository root — move it and that file has to move with it. |

## Documentation

The two documents most worth reading before changing anything:

- [`doc/CodingStandard.md`](doc/CodingStandard.md) — the house style.
- [`doc/ChangeLog.md`](doc/ChangeLog.md) — what changed between releases.

To generate the API documentation, see [`doc/generate_api_docs.md`](doc/generate_api_docs.md).

## A note on paths

Several of these directories are referenced by path from outside the repository —
container builds point at
`extra/docker/DockerFile-*`. If you move one of them, grep for the old path
across `.github/` and this file before assuming nothing depended on it.
