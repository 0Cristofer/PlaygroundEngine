# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

A sandbox for implementing game/game-engine concepts (mostly) from scratch. Current goal: the simplest possible 3D renderer. Some parts follow industry standards, others are deliberately simplified.

The project deliberately rides the bleeding edge of the C++ toolchain: **C++26, named modules, `import std`, and `std::meta` reflection**. This drives most of the non-obvious constraints below.

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

There is **no test framework wired in** (no `enable_testing`/`add_test`, no Catch2/GTest/doctest). "Running tests" currently means building and running the executables.

## Targets

- **PlaygroundEngine** — static library; the engine. Depends on spdlog (header-only, fetched via `FetchContent`).
- **PlaygroundGame** — executable; links `PlaygroundEngine`. The actual game/demo.
- **PlaygroundReflection** — standalone executable, unrelated to the engine. A scratch pad for `std::meta` (C++26 reflection: `^^T`, `template for`, `[:member:]` splicers). Build-guarded to GCC 16+.

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

### Logging
spdlog, header-only (`SPDLOG_HEADER_ONLY`). Use the `LOG_TRACE/INFO/WARN/ERROR/FATAL` macros from `include/PlaygroundEngine/Log.h`. Because macros can't cross module boundaries, files that log must `#include "PlaygroundEngine/Log.h"` in the **global module fragment** (`module;` ... before the `module X;` line) — see `Engine.cpp` and `PlaygroundGame/src/App.cpp`.

## Module file conventions

- `.cppm` = module interface (exports). `.cpp` = implementation unit (`module X;` with no `export`). Both must be listed in `target_sources`: `.cpp` under `PRIVATE`, `.cppm` under `FILE_SET CXX_MODULES`. **Adding a new module means editing the target's `CMakeLists.txt` in both places.**
- Config macros: `PGE_DEV` (Debug/RelWithDebInfo) and `PGE_RELEASE` (Release) are defined on the engine target.
- PCH is intentionally not used (GCC doesn't support PCH with modules — see git history). Games must `import std` themselves because GCC doesn't reliably export templated `operator new` across modules.
