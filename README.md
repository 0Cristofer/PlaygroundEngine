# PlaygroundEngine

A cross-platform application engine focused on realtime graphics and simulation, built on bleeding-edge C++ tooling with no compromises for development efficiency.

> **Status: Exploration phase.** Major concepts are being validated in isolation before being unified into a single system. The name will change once the engine takes its final shape.

## Vision

The goal is an engine that sits between Unity and Unreal Engine in philosophy: simple to pick up, but with a strong set of base systems so developers aren't setting up architecture from scratch every time. Games will be written mostly in a high-level managed language (C#), with C++ available where performance demands it, and visual scripting for fast prototyping and reducing programmer dependency.

The integration between these environments — native, managed, and visual — should be seamless.

## Core Systems

| System | Status |
|---|---|
| Application lifecycle | Planned — entry point pattern in place |
| Realtime simulation | Planned — basic World/Component skeleton exists |
| Asset authoring tool | Planned |
| Networking | Planned |
| Native / managed integration | Planned |

**Currently in active development:** C++26 `std::meta` reflection system (`PlaygroundReflection/`) — a prerequisite for serialization, networking replication, and native/managed binding.

## Design Principles

- **Bleeding-edge tooling.** C++26, named modules, `import std`, `std::meta` reflection, GCC 16+, CMake 4.3+. Hard requirements, not aspirational targets.
- **Developer efficiency over compatibility ceremony.** Where the language provides a mechanism, the engine uses it. Legacy patterns are replaced, not accumulated.
- **Clear architecture with defined extension points.** Sensible defaults for common cases; well-defined entry points for advanced customization.
- **Networking from the start.** The simulation model accounts for replication and authority from the beginning, not as a retrofit.

## Building

Requires GCC 16+ (source build at `~/gcc-16`), CMake 4.3+, and Ninja.

```bash
cmake --preset linux
cmake --build --preset linux-debug
```

Output: `build/linux/<Target>/<Config>/`

## Current Targets

| Target | Description |
|---|---|
| `PlaygroundEngine` | Static library — the engine core |
| `PlaygroundGame` | Executable — links the engine, current demo |
| `PlaygroundReflection` | Standalone scratch pad for C++26 `std::meta` reflection |

See [EngineDesign.md](EngineDesign.md) for the full design document, [docs/ReflectionSystem.md](docs/ReflectionSystem.md) for the reflection system design, and [docs/CoreConventions.md](docs/CoreConventions.md) for engine-wide conventions (object model, memory, std policy, error handling).
