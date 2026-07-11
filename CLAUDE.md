# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A cross-platform application engine focused on realtime graphics and simulation. Currently in the **exploration phase**: major concepts are validated in isolation before being unified. See [EngineDesign.md](EngineDesign.md) for the full design document — read it before making architectural decisions.

The five target core systems are: application lifecycle, realtime simulation, asset authoring tool, networking, and native/managed (C++/C#) integration. All are at the concept or early groundwork stage; the current active area is the **C++26 reflection system** (`PlaygroundReflection/`), which is a prerequisite for serialization, networking replication, and the C# binding layer. Its design doc (use cases, requirements) is [docs/ReflectionSystem.md](docs/ReflectionSystem.md).

Key design principles relevant to coding decisions:
- Where the language provides a mechanism, use it — no macro-based annotation systems, no engine-specific idioms where C++ suffices.
- Old approaches are replaced when a better one exists, not kept alongside it.
- Networking and replication are considered in system design from the start, not retrofitted.
- Engine-wide conventions are settled in [docs/CoreConventions.md](docs/CoreConventions.md): no garbage collector, generational handles as the canonical reference to engine objects, ECS as the simulation direction, std usage policy (deny-list, pmr seam), and the error-handling zones (native runtime code uses `std::expected`/error codes, never depends on exceptions). Consult it before design-adjacent changes.

The project deliberately rides the bleeding edge of the C++ toolchain: **C++26, named modules, `import std`, and `std::meta` reflection**. This drives most of the non-obvious constraints below.

## Working method

Applies to every agent. Autonomous runs also load [.claude/agents/autonomous-worker.md](.claude/agents/autonomous-worker.md), which overrides only the *Interaction* rules below; process, style, and validation still hold. The five steps are a loop, not a line: if a later step invalidates an earlier one, go back. In pair mode raise it with the user; in autonomous mode reanalyze and resketch.

**Right-size the process first.** Classify the task and state the level you are working at:
- *Mechanical*: a feature inside settled architecture (e.g. an editor asset picker). Skip literature review and formal sketching; analyze lightly, implement, test.
- *Substantial*: a new API or non-trivial change within an existing system. Full process, lighter on step 1's industry survey.
- *Architectural*: a new system or cross-system contract (render pipeline, asset management, reflection). Full depth; step 1 is the most important part.

Do not run the heavy path on light work.

**1. Analyze**
- Survey what already exists first: read the surrounding code and the relevant design doc before reasoning about solutions.
- What exactly is being solved? Exact scope? Constraints and goals?
- How does industry/literature solve it (consult `engine-architect`)? Applicable here? Drawbacks, why it was done that way, can we do better?
- Common use cases? Should it be broken into smaller pieces?
- For any new type or API, consider its serialization, replication, and C#-boundary implications, and which error-handling zone it lives in (see [docs/CoreConventions.md](docs/CoreConventions.md)).
- Iterate until the direction is clear.

**2. Plan & sketch**
- Define architecture layers, the user-facing API/use cases, interaction points with other systems/layers.
- Sketch the main classes and data structures; mark work with TODOs.
- Re-read the plan; restart it if it does not hold.

**3. Implement**
- Follow the hard rules: formatting/naming/braces are in `.editorconfig`, enforced by `.clang-tidy`; build is warnings-as-errors.
- Validate a bleeding-edge language or library assumption with a throwaway compile before building on it (GCC 16 has known gaps: LTO disabled, `-freflection` quirks).
- Implement TODOs incrementally, validating each against the `scratch` test case (see [docs/TestingSystem.md](docs/TestingSystem.md)).
- When done, review and remove leftovers.

**4. Test**
- From the analysis, write tests for the real use cases and edge cases; promote settled behavior out of `scratch` into named tests.
- Tests must exercise real logic: clean, clear, non-redundant. Run `ctest -C Debug` and report actual output.

**5. Finalize**
- Run `/code-review` on the diff; for architectural work, have `engine-architect` review the design. Self-reading your own code is not an unbiased review.
- Confirm it builds clean and tests pass. That is the definition of done.
- Note next steps, improvements, and what was left out of scope. For a new system, sketch how a user would use it.

**Interaction (pair mode, the default)**
- The user guides the process and takes the final stance on each step; surface trade-offs at genuine forks and let them decide.
- Push the user's assumptions: ask questions, think outside the box, rather than accepting the framing.
- Confirm before large, irreversible, or outward-facing actions; do not run ahead.

**Code style** (mechanical rules live in `.editorconfig` / `.clang-tidy`; follow, do not restate)
- Never use the '—' (em dash) character in prose, comments, commit messages, or reports; it reads as machine-written. Use commas, parentheses, colons, or separate sentences.
- Names spelled out in full, unless a very common acronym.
- Always use designated initializers when constructing structs (`Foo{.x = 1, .y = 2}`), so field intent is explicit and initialization survives field reordering.
- Every `if`/`else` always uses braces, even a single-statement body (`if (x) { Foo(); }`, never `if (x) Foo();`).
- Blank lines separate distinct logic blocks.
- Functions are clear in intent and do one job; split when doing too much (`OnClick()` calls `Purchase()`; it is not itself the purchase).
- Class interfaces read like documentation; keep them clean, and if internals must surface, organize them clearly.
- Comments only where logic is genuinely complex or assumes low-level knowledge; code should be self-documenting. Put comments inside their block, never floating above it where they would read as API docs (real API docs come later, once the surface stabilizes).

## Design rationale notes

Background for the decisions in `docs/` — useful when evaluating proposals or extending designs; the docs state only the decisions.

- **No native GC:** UE's GC exists chiefly to serve Blueprint-style script object graphs; here that role belongs to the .NET GC on the C# side. Two collectors that can't see each other's edges cause boundary bugs (Godot's C# RefCounted history).
- **Handles over `weak_ptr`:** same explicit may-be-dead semantics (check before use), but POD — serializable, replicable, crosses to C# as an `int64` — with no shared-ownership prerequisite and no cycles. Avoids UE's pointer↔NetGUID translation layer, Unity's cached-pointer "fake null", and Godot's cross-boundary refcount dance.
- **`Poly<T>` capture-at-erasure:** chosen so engine correctness never depends on RTTI; P2996 cannot enumerate derived classes and vtable-pointer maps are UB, so capturing `TypeInfo` where the concrete type is still statically known is the only clean mechanism.
- **Tree-of-ownership:** matches practice (in UE: owned `UPROPERTY` subobject vs. `UPROPERTY(Transient)` reference); it is what makes text assets mergeable and removes graph-aliasing machinery from serializers. Assets are the multi-referenced exception, handled by never serializing inline.
- **UI as retained polymorphic trees:** ECS-for-UI is a known poor fit (deep heterogeneous hierarchies, tree ownership); every shipping retained UI (Slate, Qt, Godot Control) is an object tree. Handle-style cross-references rather than Slate's shared-pointer web, because UI is driven from C#.
- **No std wrapper aliases:** `vector`/`string`/`span` essentially never get wholesale-replaced; the genuinely weak pieces (node-based maps, regex, iostreams) get replaced individually when profiling shows it.
- **Error zones split by "ships in the console runtime?"** not "system vs gameplay" — meta-game code (store, customization) ships on console but is C#, which always has exceptions; native runtime code must not depend on unwinding so `-fno-exceptions` stays available.
- **Cross-runtime type model:** UE's `UClass` (Blueprint classes are first-class types) is the model; Unity's split native/C# metadata worlds are the cautionary tale. Unify the TypeInfo contract, not the per-runtime mechanisms.
- **pmr as the allocator seam:** the seam (signatures, `memory_resource*` params) is invasive to retrofit; the pools are not. Hence seam now, implementations later.
- **Pure DI at the root, no container:** an engine is a small, tightly ordered graph where order is the contract; containers move dependency errors from compile time to startup and hide construction order (Seemann's Pure DI threshold, Nystrom on locators). Explicit wiring is also the fork seam: every system has one birthplace, so swapping one is a root-only diff. Godot's `Main::setup` is the precedent; UE's `FEngineLoop` shows globals save no lines, they only hide the graph.
- **Out-of-process play, in-process editing:** the editor's truth is authoring data (its world is a derived preview), so a child-process crash loses nothing, and spawning the real game target kills the PIE-vs-standalone divergence bug class (Godot precedent). Full out-of-process *editing* was rejected: the edit loop needs same-frame feedback, and Wayland forbids foreign-window embedding. Live edit rides the replication substrate (reflection op-tables, handles as wire ids, `InputCommand` PODs), so the remote inspector doubles as the integration test of serialization, replication, and the C# binding contract.

## Toolchain constraints

- **GCC 16+ is required and the only supported compiler.** It must be a source build located at `~/gcc-16` (the `linux` preset hardcodes `~/gcc-16/bin/{gcc,g++}`).
- **MSVC cannot currently be used** even though the CMake has Windows presets/flags: the reflection demo needs `-freflection`, which is GCC-only. Treat the Windows preset as aspirational, not working.
- CMake 4.3+ and Ninja are required. `import std` support is gated behind the experimental `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` UUID set in the root `CMakeLists.txt` — that UUID is CMake-version-specific and must be updated when bumping CMake.
- All warnings are errors (`COMPILE_WARNING_AS_ERROR ON`, `-Wall -Wextra -Wpedantic`). `-freflection` is applied globally via `add_compile_options` in the root CMakeLists.

## Build & run

```bash
cmake --preset linux              # configure (Ninja Multi-Config, GCC 16)
cmake --build --preset linux-debug   # or linux-dev (RelWithDebInfo) / linux-release

./build/linux/PlaygroundGame/Debug/PlaygroundGame
./build/linux/PlaygroundReflection/Debug/PlaygroundReflection
```

Build output is under `build/linux/<Target>/<Config>/`. In-source builds are blocked by a `FATAL_ERROR` guard.

Tests use **doctest** + **CTest** (`PlaygroundTests` target). Build with `cmake --build --preset linux-debug --target PlaygroundTests`, then `cd build/linux && ctest -C Debug` (or `ctest -R <name>` for one). See [docs/TestingSystem.md](docs/TestingSystem.md) for the workflow, how to write a test, and constraints (read it before adding tests). Note: tests run from CLion or terminal `ctest` — **Rider cannot run them** (its C++ test runner is MSBuild-bound and fails on this CMake/WSL project).

### Testing expectations

Use `PlaygroundTests` (doctest + CTest) as the working ground for validating engine behavior, not throwaway `main()`s — see [docs/TestingSystem.md](docs/TestingSystem.md).

- **Explore in the `scratch` case.** Behavior needing on-demand validation (reflection capabilities, engine output) goes in the shared, ephemeral `scratch` case — overwrite it freely; don't accumulate provisional tests. For purely toolchain/compile-level questions, a throwaway compile is the right tool — the harness's value is engine linkage + logging.
- **Durable API that settles ⇒ test.** Such an API isn't done until a named test validates what was implemented; promote the scratch exploration into it (characterization tests pinning implementation-defined output are fine). Run `ctest -C Debug` and report. Exempt placeholder scaffolding slated for replacement (the GameObject/World/Component skeleton).
- **Permissive in between.** Don't gold-plate — spikes, trivial/obvious changes, and pure refactors owe no new test. When unsure, lean toward writing it.

## Targets

- **PlaygroundEngine** — static library; the engine. Depends on spdlog (header-only, fetched via `FetchContent`).
- **PlaygroundGame** — executable; links `PlaygroundEngine`. The actual game/demo.
- **PlaygroundReflection** — standalone executable, unrelated to the engine. A scratch pad for `std::meta` (C++26 reflection: `^^T`, `template for`, `[:member:]` splicers). Build-guarded to GCC 16+.
- **PlaygroundTests** — executable; links `PlaygroundEngine`. doctest-based test suite, run via CTest. See [docs/TestingSystem.md](docs/TestingSystem.md).

## Architecture

### Inverted entry point
The **engine owns `main()`** (`PlaygroundEngine/src/main.cpp`), not the game. The game supplies `PlaygroundEngine::GetAppDescriptor(CommandLine*)` (declared in the only public header, `include/PlaygroundEngine/EntryPoint.h`; implemented in `PlaygroundGame/src/PlaygroundGame.cpp`). Flow:

```
main() → GetAppDescriptor() → AppDescriptorBase
       → ::GetEngine() → Engine(appDescriptor)   // Engine ctor runs Log::Init(), builds the App + World, calls App::OnInitialized()
       → Engine::Run()                            // calls App::OnRun(world) then World::Run()
```

A game is created by subclassing `AppDescriptorBase` (factory: `GetApp()`) and `AppBase` (`OnInitialized()` + `OnRun(World*)`). See `PlaygroundGame/src/App.{cppm,cpp}` for the canonical example.

### Module layout
`PlaygroundEngine` (the primary module interface, defined in `Engine.cppm`) is the umbrella: it `export import`s `.World` and `.GameObject`. Other named modules: `.App`, `.World`, `.GameObject`, `.Log`, `.Components` (re-exports `.Components.ComponentBase` and `.Components.TransformComponent`). Importing `PlaygroundEngine` gives you the engine surface; `import PlaygroundEngine.App` is needed separately for `AppBase`.

### Entity/component model
- `World` owns `GameObject`s (`std::vector<unique_ptr>`); `World::Run()` updates them.
- `GameObject` owns `ComponentBase`s in an `unordered_map<ComponentId, unique_ptr>`. Type→id mapping is a per-type `static` counter (`GetComponentId<T>()` caches a value from `IncrementComponentId()`), so **each component type can appear at most once per GameObject** — `AddComponent` asserts on duplicates, `GetComponent` asserts if absent.
- New components subclass `ComponentBase` (pure-virtual `Update()`) and should be re-exported from `Components.cppm`. `TransformComponent` is the template to copy.
- **This model is a placeholder.** The decided direction is a full ECS — entities as generational handles, components as concrete value types in contiguous storage (see `docs/CoreConventions.md`). The skeleton will be replaced, not evolved; don't build new infrastructure on the GameObject model.

### Logging
spdlog, header-only (`SPDLOG_HEADER_ONLY`). Use the `LOG_TRACE/INFO/WARN/ERROR/FATAL` macros from `include/PlaygroundEngine/Log.h`. Because macros can't cross module boundaries, files that log must `#include "PlaygroundEngine/Log.h"` in the **global module fragment** (`module;` ... before the `module X;` line) — see `Engine.cpp` and `PlaygroundGame/src/App.cpp`.

## Module file conventions

- `.cppm` = module interface (exports). `.cpp` = implementation unit (`module X;` with no `export`). Both must be listed in `target_sources`: `.cpp` under `PRIVATE`, `.cppm` under `FILE_SET CXX_MODULES`. **Adding a new module means editing the target's `CMakeLists.txt` in both places.**
- Config macros: `PGE_DEV` (Debug/RelWithDebInfo) and `PGE_RELEASE` (Release) are defined on the engine target.
- PCH is intentionally not used (GCC doesn't support PCH with modules — see git history). Games must `import std` themselves because GCC doesn't reliably export templated `operator new` across modules.
