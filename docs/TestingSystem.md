# Testing System

How automated tests are built, run, and written in this project. Engine-wide decisions referenced here (error-handling zones, std policy) live in [CoreConventions.md](CoreConventions.md).

**Status:** in use for unit/characterization tests. The custom harness layer (environment gating, in-engine/latent tests, image comparison) is anticipated but not built, see [Direction](#direction).

## Libraries & tooling

| Piece | Choice | Why |
|---|---|---|
| Test framework | **doctest** (`FetchContent`, pinned tag, `SYSTEM`) | Minimal standard-library footprint, so it coexists with `import std` in the same TU on this toolchain; fastest compile; drivable from our own `main()`. Validated against GCC 16 + `-freflection`. |
| Runner | **CTest** | Native CMake fit. Orchestrates the test binary; `ctest -R` runs a single case; integrates with CLion and CI. |
| Discovery | **`doctest_discover_tests`** | Registers each `TEST_CASE` as its own CTest entry automatically, no hand-maintained `add_test` lists. |

doctest is pulled in `SYSTEM` so its headers don't trip our warning bar; only our own test code is held to `-Wall -Wextra -Wpedantic` + warnings-as-errors (the same bar as the rest of the project).

## Layout

One executable, `PlaygroundTests`, links the engine and runs all tests. Source lives in `PlaygroundTests/src/`:

- **`TestMain.cpp`**, our own entry point (`DOCTEST_CONFIG_IMPLEMENT` + `main()`). doctest supplies the registry and runner; we own `main()` so future harness concerns (environment setup, custom reporters, extra CLI flags) have a home. doctest's bundled static-main lib is disabled (`DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF`).
- **`<System>Tests.cpp`**, one file per engine system under test (`ReflectionTests.cpp`, `LogTests.cpp`, …). Add a new file to `target_sources` in `PlaygroundTests/CMakeLists.txt`.
- **`Scratch.cpp`**, a dev scratchpad, not a real test (see [below](#scratch-case)).

## Workflow

### Build & run

```bash
cmake --build --preset linux-debug --target PlaygroundTests   # build
cd build/linux && ctest -C Debug                              # run all
ctest -C Debug -R "reflected type name"                       # run one (by name)
ctest -C Debug -N                                             # list (after a build)
```

Or run the binary directly: `./build/linux/PlaygroundTests/Debug/PlaygroundTests` (pass `--test-case=…`, `--help` for doctest options).

### Writing a test

```cpp
#include <doctest/doctest.h>

import std;                        // test TUs allocate; needed for operator new across modules
import PlaygroundEngine.Reflection;

TEST_CASE("reflected type name is correct")
{
    CHECK(PgE::TypeOf<int>().GetDisplayName() == "int");
}
```

- A test TU imports the engine module(s) it exercises and includes `<doctest/doctest.h>` textually (doctest is not a module).
- To use the logging macros (`PGE_LOG`), `#include "PlaygroundEngine/Log.h"` in the **global module fragment** (`module;` … before any import), same rule as engine code, because macros can't cross module boundaries.
- Prefer stable inputs in assertions. Reflected display strings are implementation-defined for user types (often namespace-qualified) and stable for fundamental types, characterization tests (e.g. `LogTests`, the field-walk cases) deliberately pin current behavior and double as tripwires for toolchain churn.

### Scratch case

`Scratch.cpp` holds a `TEST_CASE("scratch" * doctest::skip())`, a hidden, skipped case for running engine code by hand during development (the harness already links the engine and inits logging). Skipped by default so normal and CI runs ignore it.

Run it via the shared **"Scratchpad test"** IDE run configuration (`.run/`), which builds, runs, and can debug it, the usual way, since you'll often want a debugger on it. Or directly:

```bash
./build/linux/PlaygroundTests/Debug/PlaygroundTests --test-case=scratch --no-skip
```

## Considerations

- **Tests are tooling-zone code.** Per [CoreConventions.md](CoreConventions.md#error-handling), tests compile with exceptions on (doctest aborts an assertion by throwing). This does not relax the rule that native *runtime* code must build and behave under `-fno-exceptions`; a separate compile-only check guards that, independent of the test framework.
- **`-freflection` stays global, by necessity.** It cannot be scoped to first-party targets: CMake builds the `import std` module via an internal target that reads the *global* options, and a std-module BMI built without `-freflection` while consumers import it with it fails (`conflicting type for imported declaration`). The cost is that the flag leaks onto `FetchContent` deps; any dep sub-target compiling below C++26 must be exempted case-by-case (we disable doctest's static-main lib for this reason).
- **IDE support.** CLion runs the gutter ▶ / CTest tree correctly (CMake + WSL aware). **Rider cannot run these tests**, its C++ unit-test runner is built on the MSBuild/Visual-Studio project model and fails against a CMake-over-WSL-GCC project (`MSB4025`). Use CLion or terminal `ctest`. Rider remains the right tool for the future C# side.

## Direction

Single executable is correct while there is essentially one system under test. The intended evolution, adopt when a second system needs isolation, or the first environment-gated (e.g. GPU/renderer) test appears:

- Split into **one test executable per system** (`ReflectionTests`, `ECSTests`, `RenderTests`, …), each linking only what it needs, sharing a small **test-support library** (the `TestMain` entry point + harness/environment seam).
- Use CTest **`LABELS`** as the environment seam (e.g. `gpu`) so headless CI runs `ctest -LE gpu` and never links the renderer.

Keep `TestMain.cpp` a clean standalone entry point so promoting it to that shared support library is a move, not a rewrite. Do not split prematurely, logical grouping (doctest `TEST_SUITE`, CTest labels) covers organization within one binary until physical isolation is actually needed.
