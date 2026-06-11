# Core Conventions — Object Model, Memory, and Native Code Policy

Engine-wide decisions that surfaced during the reflection system design but apply beyond it. These are inputs to the simulation (ECS) design, the serialization system, and the C# binding layer. Parent document: [EngineDesign.md](../EngineDesign.md).

---

## Object Model

### No garbage collector

The engine has no garbage collector for native objects. Rationale:

- UE's GC primarily exists to serve its scripting layer (UnrealScript, then Blueprint), where users build arbitrary object graphs with no manual lifetime control, plus safe-destruction semantics (stale `UPROPERTY` references nulled after `Destroy()`). This engine gets the first from the .NET GC — gameplay is C#-first — and the second from generational handles (below), without a collector or its frame-time cost.
- A native GC alongside the .NET GC means two collectors that cannot see each other's edges — the source of cross-runtime cycle bugs (see Godot's C#/RefCounted lifetime history).
- Industry practice outside UE agrees: Unity's native core, Godot, and modern ECS designs (EnTT, flecs, Bevy, the Bitsquid/Our Machinery lineage) all use explicit native lifetime plus ID-validated references.

### Generational handles are the canonical object reference

Any reference **to an engine object** — entity, component, asset-as-identity — held across frames or stored in a field is a handle, in C++, in C#, on the wire, and on disk.

A handle is a **contract** — a POD stable ID whose dereference is validated by the engine (`TryGet`) — with two standard backings, chosen by the domain's storage model:

- **Slot + generation** (simulation citizens): objects live by value in engine-owned pool storage; each slot carries a generation counter and the handle is `{slot index, generation}`. Destruction bumps the generation, instantly invalidating every outstanding handle; validation is an index plus an integer compare, and a stale handle can never reach a slot's new occupant.
- **ID table** (tree-owned domains, e.g. UI): objects live wherever their owner placed them (`Poly<T>`/`unique_ptr` boxes); each object receives a never-reused 64-bit ID at construction and self-registers in the domain's `ID → pointer` table (base constructor registers, destructor unregisters — no ceremony). Validation is a table lookup; destroyed objects simply have no entry. This is Godot's `ObjectID`/ObjectDB design.

In both cases destruction is deterministic (`Destroy()`-style or owner-driven), and the handle semantics are identical from the outside.

Properties this buys:

- **`weak_ptr`'s explicitness without its machinery.** A handle field says "this object can die at any time; you must check" — the same honest semantics as `weak_ptr`, but with no shared-ownership prerequisite, no lifetime extension by observers, no cycles by construction, no atomic refcount traffic. Validation is an array index plus an integer compare.
- **The reference is already the ID.** Handles serialize into save files, replicate over the network, and cross into C# as a plain `int64`. There is no pointer↔ID translation layer at any boundary (contrast UE's `FNetworkGUID` mapping).
- **Deterministic destruction timing**, unlike `shared_ptr` graphs where whichever owner drops the last reference runs the destructor cascade.

Discipline: the pointer returned by `TryGet` is a transient borrow — used within the current scope, never stored in a member. Same rule as not stashing the result of `weak_ptr::lock()`.

### Simulation direction: ECS

The simulation will be a full ECS. The current `World`/`GameObject`/`ComponentBase` skeleton is a placeholder and will be **replaced**, not evolved.

- Entities are generational handles natively. Components are **concrete value types** stored contiguously by type — no virtual functions, no runtime polymorphism in component storage. Struct inheritance for field reuse is fine (still a concrete value type, flattened by reflection); `Derived`-through-`Base*` polymorphism is not — that intent is expressed by composition (component combinations matched by systems).
- **Facade rule:** a thin wrapper presenting a simpler object-style API for small games is a future goal, and it is allowed precisely because it **owns no state**. It is sugar over entities and components — serialization, replication, and reflection see only ECS state. A facade that becomes a second object model (Unity's GameObject/DOTS split) is the failure mode to avoid.
- Reflection operates at boundaries — load, save, replicate, edit, bind. The frame loop iterates typed component storage and never touches reflection machinery.

### Not everything is ECS — owned polymorphic trees

The ECS direction applies to the **simulation**. Some domains are correctly modeled as retained object trees with runtime polymorphism — the anticipated case is **UI**, where widget hierarchies are deep, heterogeneous, and event-driven, ownership genuinely is the tree (parent owns children), and every serious retained UI system (Slate, Qt, Godot Control) is built this way. ECS-for-UI is a known poor fit.

Such domains already fit the conventions: parent-owns-children is tree-of-ownership; heterogeneous children are boxed polymorphic data (`Poly<T>` — UI is expected to be its biggest consumer, e.g. layout files as type-discriminated trees); non-owning cross-references (focus targets, bindings) use ID/handle-style references rather than `shared_ptr`/`weak_ptr` webs, especially since UI is authored and driven from C#. Whether widgets share the entity handle scheme is a UI-system design question.

Entity/component creation does not go through smart-pointer factories: pools construct in place in a slot and return a handle. `make_unique`/`make_shared` remain the normal tools for plumbing ownership (subsystems, asset payloads) — see the table below.

### Reference vocabulary types

Reflected fields express reference semantics through a small set of engine vocabulary types, each a "known wrapper" to the reflection layer:

| Type | Meaning | Serializes as |
|---|---|---|
| by value / `unique_ptr<T>` | Owned — exactly one serialization home | Inline, at the owner |
| `Handle<T>` | Cross-reference to an engine object; may die at any time | Stable object ID |
| `AssetRef<T>` | Asset identity; assets are multi-referenced by design | Asset GUID, resolved by the asset system |
| `Poly<T>` | Boxed polymorphic data (backend payloads, messages, config) | Type discriminator + inline fields |

`Poly<T>` resolves the "dynamic type of a base-referenced object" problem by **capturing identity at the point of erasure**: its templated constructor/assignment sees the concrete type at compile time and stores the `TypeInfo` alongside the pointer. No RTTI dependency, no universal base class, no `GetType()` virtual, deterministic across DLL boundaries. (Alternatives rejected: `typeid`-keyed registry couples engine correctness to RTTI being enabled; P2996 cannot enumerate derived classes; vtable-pointer maps are implementation-specific UB.)

---

## Ownership & Memory

### Where each tool lives

| Tool | Role | Example |
|---|---|---|
| `unique_ptr` | Internal ownership within systems — the workhorse | A subsystem owning its platform implementation; pools owned by the World |
| `shared_ptr` | Genuinely shared lifetime of **non-entity resources**, justified case-by-case, never the default | An asset payload kept alive by both the cache and in-flight GPU work |
| `weak_ptr` | Alongside those `shared_ptr` cases | A cache index that shouldn't pin payloads |
| Raw pointer / reference | Non-owning transient borrow | Function parameters; `TryGet` results within a scope |
| `Handle<T>` / `AssetRef<T>` / `Poly<T>` | References to engine objects / assets / boxed polymorphic data | Component fields, gameplay state |

Rule of thumb: **simulation citizens (spawnable, destroyable, serializable, replicable) are referred to by handle; plumbing is owned by smart pointers.** `shared_ptr` does not appear in reflected gameplay data; in practice C# gameplay code sees only handles and value types.

In UE terms: an owned `UPROPERTY` subobject maps to an owned field; a `UPROPERTY(Transient)` non-owning reference maps to `Handle<T>` — except the handle can serialize as a stable ID, so the relationship survives save/load instead of being re-wired in `BeginPlay`.

### Ownership is a tree

Reflected data forms a tree, not a graph: every object has exactly one serialization home (its owner), and every other mention is an ID. Aliased shared ownership is not part of the reflected data model, so serializers need no "seen this pointer before" graph machinery. Assets — the legitimately multi-referenced case — achieve this by never being serialized inline anywhere: an asset has its own file, and every reference is a GUID.

### The allocator seam

The engine controls low-level memory through two entry points into one funnel: an eventual global `operator new`/`delete` override (the *ambient* route — plain `std` containers, `make_unique`, third-party allocations all land in the engine allocator automatically, with tracking and budgets), and `std::pmr` memory resources (the *deliberate* route — for code where placement and recycling pattern are part of the design). pmr resources draw their upstream pages from the same engine allocator, so everything is observed in one place. A third tier bypasses `new` entirely because it *is* allocator code: ECS chunk/pool storage and GPU memory.

The convention that must exist **from day one** (because it is invasive to retrofit) is the *seam*, not the implementations: systems where allocation pattern matters accept a `memory_resource*`, and **reflection-driven construction is placement-agnostic** — factories return by value (as the validated `[[=Factory{}]]` pattern does) or accept a placement target, never hard-coding where the object lives, so the caller can direct construction to the heap, an arena, or a pool slot. Plain `new` is a fine default precisely because it is engine-defined.

Granularity: this does **not** mean `std::pmr::` containers everywhere. Game code never thinks about allocators. Engine code uses plain `std` by default — it rides the ambient route; pmr containers and `memory_resource*` parameters (defaulted to the global resource) appear only in systems where the allocation pattern is a design property — per-frame scratch, asset streaming, anything allocating inside the frame loop. Since `std::pmr::vector<T>` is `std::vector<T, polymorphic_allocator<T>>`, converting one subsystem later is a local change — and module boundaries prefer allocator-blind views (`std::span`) anyway.

---

## Native/Managed Boundary

Gameplay is C#-first on a modern .NET runtime. The lifetime model for the boundary:

- **Engine objects cross the boundary as handles only.** A C# entity reference is a *struct* wrapping the same 64-bit handle native code uses — no native pointers in managed objects, no finalizers or `Dispose()` for engine objects, no lifetime coupling between the two runtimes. The .NET GC collects wrapper structs on its own schedule; nothing native happens.
- The API is `IsValid` / `TryGet` — honest about distributed liveness. Since `Entity` is a value type, `entity == null` does not compile. This deliberately rejects Unity's cached-pointer model (the "fake null" where `?.`, `is null`, and `ReferenceEquals` silently bypass the lifetime check) and Godot's shared-refcount-across-the-boundary model (the strong/weak GCHandle dance, still receiving correctness fixes in 2025).
- **Native → managed callbacks** (events, gameplay hooks) are the one legitimate use of GCHandles: an explicit subscription object owns a strong GCHandle to the delegate, with deterministic unregistration tied to the subscriber's handle lifetime. Scoped and auditable — not a pervasive wrapper-pinning scheme.
- **The generated binding layer owns error-model translation** (see Error Handling below): native `std::expected` failures surface as idiomatic C# exceptions; exceptions never cross the boundary in either direction.
- Performance goal: component data is accessed from C# by **viewing native memory directly** through layout-matched structs (the Unity DOTS approach), not per-field marshaling. This is why byte-level layout is a core reflection requirement.

---

## Standard Library Policy

Default: **use std directly.** No wholesale wrapper aliases (`pge::Vector` "just in case") — that is compatibility ceremony. The known-weak pieces are replaced individually when profiling proves it, per replace-don't-accumulate.

| Keep — genuinely good | Avoid | Why |
|---|---|---|
| `vector`, `string`, `string_view`, `span`, `array`, `optional`, `expected`, `variant`, `format`, `print`, `chrono`, `atomic`, `jthread`, `type_traits`, concepts, `std::meta` | `unordered_map`/`unordered_set` in hot paths | Standard mandates node-based buckets; open-addressing maps are 2–5× faster, drop-in replacements exist |
| | `map`, `set`, `list` | Node/tree pointer-chasing; sorted `vector` usually wins |
| | `std::regex` | Famously broken-slow |
| | iostreams | Heavy, locale-entangled; `std::format`/spdlog cover everything |
| | `std::function` in hot paths, `std::async` | Heap-allocating erasure; `std::async`'s execution model is unsuitable for a job system |
| | Throwing-API forms in runtime code: `at()`, `stoi`/`stof` (→ `from_chars`), value `any_cast` (→ pointer form) | See Error Handling — runtime code must not depend on exceptions |

Allocation-aware containers use the `std::pmr` seam (above).

---

## Error Handling & Build-Flag Policy

Three zones, split by **"does this code ship in the console runtime?"** — not by "system vs. gameplay":

| Zone | Error model |
|---|---|
| **C# (all of it, including shipping console builds)** | Exceptions, idiomatic — always available, the .NET runtime provides them under AOT too. Native errors arriving through generated bindings are thrown as C# exceptions. |
| **Native runtime code** (simulation, renderer, asset loading, networking) | `std::expected` / error codes / asserts. Must build and behave identically under `-fno-exceptions` — never depends on unwinding. |
| **Native tooling-only code** (editor, importers/cooking, build tools) | Exceptions permitted — also a practical necessity for third-party tooling libraries. Caught at module edges, converted to the engine error model, never propagated into runtime code. |

Supporting decisions:

- **During exploration, everything compiles with exceptions ON** (near-zero happy-path cost; the discipline above prevents dependence). The shipping endgame is `-fno-exceptions` for runtime binaries, where libstdc++ turns internal throw sites into `abort()` — acceptable because by then a throw only signals a bug, not an expected error.
- Exceptions must never cross a boundary between exceptions-on and exceptions-off code (no unwind tables → `terminate`), nor the native/managed boundary in either direction.
- Code migrating from tooling to runtime (e.g. an importer becoming runtime hot-loading) gets its error handling rewritten at that moment — a conscious cost under replace-don't-accumulate.
- **RTTI:** engine correctness is independent of RTTI (`-fno-rtti` remains viable for shipping builds). `Poly<T>`'s capture-at-erasure design was chosen partly for this.
- Consoles do not strictly prohibit exceptions/RTTI (PS5/Switch toolchains support both); shipping with them off is a size/culture norm. The policy keeps the flip available without betting on it.

---

## Open Questions

- **Handle granularity** — per-entity only, or per-component too. Belongs to the simulation (ECS) design.
- **One handle scheme or siblings** — whether asset GUIDs and entity handles share an identity scheme. Tied to the stable-ID design in [ReflectionSystem.md](ReflectionSystem.md).
- **C# event subscription lifetime** — needs a focused design pass when binding generator work starts; the one boundary area where every engine has shipped bugs.
