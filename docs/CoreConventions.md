# Core Conventions

Engine-wide decisions: object model, memory, native/managed boundary, std usage, error handling. Parent document: [EngineDesign.md](../EngineDesign.md). Rationale notes live in [CLAUDE.md](../CLAUDE.md).

## Object Model

- **No garbage collector.** Native lifetime is explicit. Gameplay object graphs are the .NET GC's job, on the C# side.
- **Generational handles are the canonical reference to engine objects** (entities, components, assets), in C++, in C#, on the wire, on disk. A handle is a POD stable ID, validated on dereference: `TryGet(handle)` returns a pointer or null. Handles never own; destruction is deterministic and invalidates all outstanding handles immediately.
  - Backings: **slot + generation** for pooled simulation objects; **ID table** for tree-owned objects (e.g. UI widgets), registered/unregistered by the object's constructor/destructor.
  - A field that references an object it does not own is a handle. The `TryGet` result is a transient borrow, used in scope, never stored.
- **The simulation is a full ECS**: entities are handles; components are concrete value types in contiguous per-type storage; behavior is composition. The current `World`/`GameObject` skeleton is a placeholder and will be replaced. No runtime polymorphism in component storage; struct inheritance for field reuse is allowed.
- **Not everything is ECS.** Retained object trees with runtime polymorphism are the correct model for some domains (UI is the anticipated case): parent owns children, cross-references are handles.
- A simple-game **facade** over the ECS may exist later. It owns no state; engine systems see only ECS state.
- **Reference vocabulary in reflected data:**

  | Field type | Meaning | Serializes as |
  |---|---|---|
  | by value / `unique_ptr<T>` | Owned | Inline, at the owner |
  | `Handle<T>` | Non-owning reference to an engine object | Stable object ID |
  | `AssetRef<T>` | Asset reference | Asset GUID |
  | `Poly<T>` | Owned, polymorphic (boxed) | Type discriminator + fields |

  `Poly<T>` captures the concrete type's `TypeInfo` at the point of erasure (templated constructor), no RTTI, no common base class required.

## Ownership & Memory

- **Ownership is a tree.** Every object has one serialization home; every other mention is an ID. Assets are never serialized inline, always referenced by GUID.
- **Smart pointers are for plumbing, not gameplay references.** `unique_ptr` is the default owner. `shared_ptr` is rare and justified case-by-case (shared non-entity resources, e.g. asset payloads pinned by in-flight GPU work); `weak_ptr` only alongside those. Raw pointers/references are transient borrows. `shared_ptr` does not appear in reflected data.
- **Entities and components are not created with `make_unique`**: pools construct in place and return a handle.
- **Memory funnel, two entry points, one allocator:**
  - *Ambient:* a global `operator new`/`delete` override (eventual) routes all plain allocations, `std` containers, `make_unique`, third-party, through the engine allocator with tracking and budgets.
  - *Deliberate:* `std::pmr` memory resources where the allocation pattern is a design property (per-frame arenas, asset streaming, anything allocating inside the frame loop). pmr upstreams chain to the same engine allocator.
  - *Structural:* ECS chunk storage and GPU memory are allocator code themselves and bypass `new`.
- **The day-one requirement is the seam, not the pools**: pattern-sensitive systems take a `memory_resource*` (defaulted to the global resource), and reflection-driven construction is placement-agnostic, factories return by value or accept a placement target. Plain `new` is a valid default because it is engine-defined.
- Plain `std` containers by default; `pmr` only where the pattern matters. Game code never sees allocators. Scratch arenas are thread-local (the engine is multithread-first).

## Native/Managed Boundary

- Engine objects cross the boundary **as handles only**. C# wrappers are value types: no native pointers, no finalizers, no `Dispose`. Liveness API: `IsValid`/`TryGet`.
- Native→managed callbacks go through explicit subscription objects owning GCHandles, unregistered deterministically with the subscriber's lifetime.
- Generated bindings translate error models: native `std::expected` ⇄ C# exceptions. Exceptions never cross the boundary in either direction.
- Performance goal: C# accesses component data through layout-matched structs viewing native memory directly, not per-field marshaling.

## Standard Library

Use `std` directly, no engine wrapper aliases. Components are replaced individually when profiling demands it.

Deny-list for runtime code:

| Avoid | Use instead |
|---|---|
| `std::regex` | anything else |
| iostreams | `std::format`/`std::print`, spdlog |
| `map`, `set`, `list` | `vector` (sorted if needed) |
| `unordered_map`/`unordered_set` in hot paths | open-addressing map (adopt when profiling shows it) |
| `std::function` in hot paths, `std::async` | templates/`function_ref`; engine job system |
| Throwing forms: `at()`, `stoi`/`stof`, value `any_cast` | indexing + checks, `from_chars`, pointer `any_cast` |

## Error Handling

Zones split by **"does this native code ship in the console runtime?"**:

| Zone | Error model |
|---|---|
| C# (all, including shipping console builds) | Exceptions, idiomatic. Native errors arrive as thrown exceptions via bindings. |
| Native runtime code | `std::expected` / error codes / asserts. Must build and behave identically under `-fno-exceptions`. |
| Native tooling-only code (editor, importers, build tools) | Exceptions permitted; caught at module edges, converted to the engine error model. |

- Exploration builds compile with exceptions ON; runtime code must not depend on them. Shipping runtime binaries target `-fno-exceptions`.
- Exceptions never cross an exceptions-on/off boundary or the native/managed boundary.
- Code migrating from tooling to runtime gets its error handling rewritten at that moment.
- **RTTI:** engine correctness is independent of RTTI (`-fno-rtti` stays viable).

## Open Questions

- **Handle granularity**, per-entity only, or per-component too. Belongs to the simulation design.
- **One handle scheme or siblings**, whether asset GUIDs and entity handles share an identity scheme. Tied to the stable-ID design in [ReflectionSystem.md](ReflectionSystem.md).
- **C# event subscription lifetime**, needs a focused pass when binding generator work starts.
