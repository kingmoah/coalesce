---
kind: build_system
name: Makefile-driven C Build with Cross-OS Support and Phased Test Suite
category: build_system
scope:
    - '**'
source_files:
    - makefile
---

# Build System

## Approach

The project uses a single top-level GNU Makefile (`makefile`) as the sole build orchestration mechanism. There is no Dockerfile, no CI pipeline file (no `.github/`, no `ci/`), no shell-based build wrappers, and no package manager manifest — just `gcc` invoked directly from Make.

## Key Files

- `makefile` — the only build definition in the repository.
- `src/*.c` / `src/*.h` — library sources compiled into object files under `build/`.
- `tests/test_phase[1234].c` — four phased test executables; `test_c25519_debug.c` exists but has no corresponding target in the Makefile.
- `build/` — output directory for all `.o` artifacts and final binaries.

## Architecture of the Makefile

### OS Detection & Portability

The Makefile detects Windows via `$(OS)` and switches:
- Executable suffix: `.exe` on Windows, empty on POSIX.
- Shell commands: `del /f /q` vs `rm -f`, `mkdir` vs `mkdir -p`.
- Linker flags: `-lws2_32 -ladvapi32` on Windows, none otherwise.

This is the only cross-platform logic in the build system.

### Compilation Model

- Compiler: `gcc` (hardcoded).
- Flags: `-Wall -O2` (warnings enabled, optimization on).
- Source discovery: `SRCS = $(wildcard $(SRCDIR)/*.c)` auto-discovers every C file in `src/`.
- Object layout: `src/foo.c` → `build/foo.o` via `$(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.o,$SRCS)`.
- Default target `all` builds the CLI binary `build/coalesce.exe` (or `build/coalesce` on POSIX).

### Library Objects for Tests

A derived variable `LIB_OBJS = $(filter-out $(BUILDDIR)/main.o,$OBJS)` excludes `main.o` so test binaries can link against the rest of the library while providing their own `main()`.

### Test Targets

Four test executables are built, each linking a different subset of the library:

| Target | Linked objects | Purpose |
|---|---|---|
| `test_phase1` | `buffer.o` | Buffer primitives |
| `test_phase2` | `buffer.o`, `rand.o`, `sha256.o`, `sha512.o`, `fe25519.o`, `curve25519.o`, `ed25519.o`, `aes.o`, `kex.o`, `base64.o` | Cryptographic primitives |
| `test_phase3` | All library objects except `main.o` | Session-layer framing |
| `test_phase4` | All library objects except `main.o` | Full session tests |

The `test` target builds all four and then runs them sequentially via `./$(TESTn_TARGET)`.

### Clean Target

On POSIX it removes `build/*.o`, the main binary, and all four test binaries. On Windows it invokes `cmd /c "if exist build (del ... *.*)"` to wipe the directory contents.

## Conventions and Constraints

- **Single compiler**: `gcc` is hardcoded; no support for clang or other toolchains.
- **No dependency tracking**: The Makefile does not generate or use `.d` include dependency files; recompilation happens at the source-file granularity based on explicit object-to-source mapping.
- **Object reuse**: Library objects are shared between the main binary and all test binaries rather than being rebuilt per target.
- **Test naming convention**: Phase-numbered test files (`test_phase1.c` … `test_phase4.c`) map one-to-one to targets; adding a new phase requires adding both a source file and an explicit rule.
- **Windows-only linkage**: Network/socket functionality lives behind conditional `-lws2_32 -ladvapi32` linkage, implying the codebase contains Windows-specific networking paths guarded by preprocessor macros elsewhere.
- **No packaging/release step**: There is no install target, no archive creation, no version stamping, and no distribution script — the deliverable is the raw executable(s) under `build/`.
- **No CI or containerization**: No GitHub Actions, GitLab CI, Dockerfile, or similar automation was found in the repository.
- **`test_c25519_debug.c` is orphaned**: A debug test source exists under `tests/` but has no Makefile target, so it is not built by default.