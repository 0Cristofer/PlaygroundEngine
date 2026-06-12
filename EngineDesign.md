# Engine Design

This document is the authoritative starting point for new developers and agents working on this project. It covers what the engine is, what it is not, what it is trying to become, and the principles that guide every decision along the way.

---

## What This Is

A cross-platform application engine focused on realtime graphics and simulation. The engine targets desktop (Windows, Linux, macOS) and current-generation consoles as future goals.

It is built on bleeding-edge C++ tooling — C++26, named modules, `import std`, `std::meta` reflection — and uses those capabilities to build systems that would not be possible or practical with older standards.

The project is currently in an **exploration phase**: major concepts are being validated and stress-tested in isolation before being unified. The codebase is intentionally a playground. The name will change once the engine takes its final shape.

---

## What It Will Be

The target is an engine that sits between Unity and Unreal Engine in philosophy and user experience.

**From Unity:** simplicity of onboarding. A developer should be able to pick up the engine and be productive quickly. The architecture should be clear and have sensible defaults.

**From Unreal:** depth and robustness. A strong set of base systems should mean developers are not reinventing architecture on every project. The engine should have opinions.

**Beyond both:** the integration between programming environments should be seamless and first-class.

### Programming Environments

Games will be written across three environments depending on context:

- **C++ (native):** the engine itself, performance-critical systems, low-level platform code. Advanced game code that needs it.
- **C# (managed):** the primary language for gameplay code. High-level, strongly typed, ergonomic. Runs in an embedded managed runtime.
- **Visual scripting:** fast prototyping, designer-facing logic, reducing programmer dependency for tasks that don't require code.

The boundary between these should be invisible to the user. Calling a C++ system from C#, or wiring a visual script to a C# event, should not require manual binding or boilerplate.

### Networking

Networking is a first-class system, not a bolt-on. The architecture assumes networked state from the beginning — entity ownership, replication, authority. This shapes how the simulation and component model are designed, not just what APIs sit on top.

---

## Core Systems

These are the five pillars the engine needs to have working together — in their simplest form — before it graduating from the exploration phase. All are currently at the concept level; some have initial groundwork in place.

### 1. Application Lifecycle `planned`
Window creation, input handling, the main loop, platform abstraction. The entry point pattern is in place — the engine owns `main()` and the game supplies a descriptor — but the full lifecycle system is not yet designed.

### 2. Realtime Simulation `planned`
The world, entities, components, and the update loop. The decided direction is a full ECS — entities as generational handles, components as value types ([docs/CoreConventions.md](docs/CoreConventions.md)); the existing skeleton (World, GameObject, ComponentBase) is a placeholder that will be replaced. The design needs to account for networking from the start — replication, authority, and determinism influence the structure, not just the API surface.

### 3. Asset Authoring Tool `planned`
An editor for creating, importing, and configuring assets. Depends on the reflection and serialization system — asset data is reflected C++ structs, and the editor reads and writes serialized state directly.

### 4. Networking `planned`
State replication, authority model, transport abstraction. The intent is for networking to be transparent for single-player use while remaining fully accessible when needed. The reflection system is the substrate — replicated fields are annotated, and the networking layer reads those annotations.

### 5. Native / Managed Integration `planned`
The bridge between C++ and C#. The C++ reflection system provides the type metadata; a binding generator emits C# wrapper classes from it. The boundary lifetime and error models are settled in [docs/CoreConventions.md](docs/CoreConventions.md).

---

## Design Principles

### Bleeding-edge tooling

C++26, GCC 16+, CMake 4.3+, Ninja, `import std`, `std::meta` reflection are hard requirements for the C++ side of the engine. The project does not maintain compatibility with older compilers or standards, and moves forward as the tooling does.

The tradeoff is a narrower compiler support surface in exchange for capabilities — compile-time reflection, zero-cost abstractions, module-based encapsulation — that directly shape the engine's architecture and what game developers can express.

### Developer efficiency over compatibility ceremony

Where the language provides a mechanism, the engine uses it rather than building its own layer on top. This applies to both engine internals and what game developers write. Engine-specific idioms and macro-based annotation systems (e.g. UE's `UCLASS()`/`UPROPERTY()`) are patterns that existed because the language lacked the features — with C++26 they are no longer necessary.

When a better approach exists, the old one is replaced rather than kept alongside it. Breaking changes are acceptable when they result in a cleaner system.

### Clear architecture with defined extension points

The default path through the engine should be obvious and sufficient for common use cases. Advanced users should be able to customize behavior at well-defined points — not by working around the architecture, but through interfaces it exposes deliberately.

### Networking considered from the start

The entity and simulation model is designed with replication and authority in mind from the beginning. Retrofitting networking onto a system designed for single-player typically requires structural changes; building with it in scope avoids that.

### Multithread-first

The engine assumes parallel execution from the start. The concrete threading model (job system, scheduling) belongs to the engine architecture design, not yet written. Standing rule until then: systems designed today must not bake in single-threaded assumptions.

---

## Architecture Overview

### Entry Point (implemented)

The engine owns `main()`. The game supplies a descriptor:

```
main()
  → GetAppDescriptor()    // game-provided
  → Engine()              // initializes logging, builds App + World
  → Engine::Run()         // App::OnRun() → World::Run()
```

This inversion means the engine controls initialization order, platform setup, and teardown. The game plugs into well-defined lifecycle hooks.

### Entity / Component Model (placeholder — ECS planned)

The current skeleton (`World` owns `GameObject`s, `GameObject` owns `ComponentBase`s keyed by type) is a sketch and will be **replaced, not evolved**. The decided direction is a full ECS: entities as generational handles, components as concrete value types in contiguous per-type storage, behavior expressed by composition. An optional thin facade may later present a simpler object-style API for small games; it owns no state, so engine systems see only ECS state.

Object model, handle, and memory conventions are recorded in [docs/CoreConventions.md](docs/CoreConventions.md).

### Reflection System (in design)

See [docs/ReflectionSystem.md](docs/ReflectionSystem.md) for the system's design document (use cases and requirements).

C++26 `std::meta` reflection is the substrate for serialization, editor tooling, networking replication, and native/managed binding. The reflection API (`TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry`) is what engine systems and game code interact with. The implementation uses `^^T`, `template for`, and splicers internally — game code never touches `std::meta` directly.

Fields and functions are annotated to control reflection behavior:

```cpp
struct Weapon {
    [[=Factory{}]]
    static Weapon Create([[=Injected{}]] int id, [[=Serialized{}]] std::string name);
};
```

The reflection system finds these annotations at compile time. On platforms without C++26 support, a pre-build code generator produces equivalent registration code from the same annotated source — game code is unchanged.

### Module Layout (engine)

`PlaygroundEngine` is the umbrella module, re-exporting `.World`, `.GameObject`, `.Components`. Game code imports `PlaygroundEngine` for the engine surface and `PlaygroundEngine.App` for the application base classes.

### Logging

spdlog, header-only. Use `LOG_TRACE/INFO/WARN/ERROR/FATAL` macros. Files that log must `#include "PlaygroundEngine/Log.h"` in their global module fragment (before the `module X;` line).

---

## Toolchain Reference

| Tool | Requirement |
|---|---|
| Compiler | GCC 16+ (source build at `~/gcc-16`) |
| C++ Standard | C++26 (`-std=c++26`, `-freflection`) |
| Build system | CMake 4.3+, Ninja |
| `import std` | Gated behind `CMAKE_EXPERIMENTAL_CXX_IMPORT_STD` UUID (CMake-version-specific) |

MSVC is not currently supported (no `-freflection`). The Windows CMake preset exists as a forward-looking target. All warnings are errors.

```bash
cmake --preset linux
cmake --build --preset linux-debug    # or linux-dev / linux-release
```

---

## Current Exploration Areas

| Area | Location | Status |
|---|---|---|
| C++26 `std::meta` reflection | `PlaygroundReflection/` | Active — validating capabilities |
| Field printing, annotation registry | `PlaygroundReflection/src/` | Done |
| Generic any-erased function registry | `PlaygroundReflection/src/` | Done |
| JSON deserialization via reflection | `PlaygroundReflection/src/` | Done |
| Construction via annotated factory | `PlaygroundReflection/src/` | Done |
| Reflection system API design | [docs/ReflectionSystem.md](docs/ReflectionSystem.md) | Active — use cases defined |
| Core conventions (object model, memory, std, errors) | [docs/CoreConventions.md](docs/CoreConventions.md) | Defined |
| 3D renderer | `PlaygroundGame/` | Early |

---

## What This Project Is Not

- **Not production-ready.** The exploration phase will be long. APIs will break. Architecture will change.
- **Not a general-purpose engine today.** The current codebase is a validated foundation, not a feature-complete system.
- **Not conservative.** If a feature requires GCC 16 and a source build, that is acceptable. Breadth of compiler support is not a goal during the exploration phase.
