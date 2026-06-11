# Reflection System — Design

The reflection system is the substrate for most of the engine's core systems: serialization, networking replication, editor tooling, GPU interop, and the C++/C# binding layer. This document records the use cases the system must serve and the requirements each one contributes. The base API design builds on this document.

Engine-wide decisions that emerged from this design (object model, memory, std policy, error handling) live in [CoreConventions.md](CoreConventions.md).

**Status:** use cases defined; memory/object-model implications settled. Base API (`TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry`) not yet designed.

---

## Structural Constraints

Two constraints apply before any use case:

1. **`std::meta` must not leak into the public API.** On platforms without C++26 reflection (consoles lagging upstream compilers), the same API must be implementable by a pre-build code generator emitting registration code from the same annotated source. Game and engine code written against the reflection API is unchanged either way. `std::meta::info` and splicers are implementation details of the default backend.

2. **Two layers, one source of truth.** Consumers split cleanly into two shapes:
   - **Typed (compile-time) layer** — generic code that knows the type it is working with: serializers, binding generators, shader generators. Gets real types, no erasure cost. Validated pattern: `InvokeStaticTyped<Fn>` in `PlaygroundReflection/src/construction.h`.
   - **Erased (runtime) layer** — code that discovers types at runtime by name: editor property grids, visual scripting, module loading. Uniform signatures (`std::any`-style get/set/invoke) over a runtime registry.

   The erased layer is built from the typed layer, never maintained in parallel.

---

## Use Cases

### 1. Serialization (assets, save files, config)

Walk a type's fields with names, types, and annotations; recurse into nested types and containers; construct objects from data via annotated factories (constructor splicing is not available in P2996 — the `[[=Factory{}]]` static-function pattern is the validated alternative).

**Key considerations**
- The serializer is **format-agnostic**: it walks reflected data and hands it to a pluggable backend (text or binary). The format decision is deferred cheaply because of this.
- Development assets are **text**; shipped assets are **cooked binary** from the same schema. Bulk payloads (textures, meshes, audio) always live in binary sidecar files referenced from the text manifest (the glTF model) — they are unmergeable by nature and would poison text diffs.
- Mergeable source-controlled assets require more than "text": output must be **deterministic** (same asset → byte-identical file; declaration-order field walks give this naturally) and array-like data needs **stable element IDs** so merges can distinguish "moved" from "replaced".
- **Reflected data is a tree, not a graph** (see CoreConventions): owned fields serialize inline at their single home; all cross-references are handle/asset IDs. The serializer needs no graph-aliasing machinery.

### 2. Network replication

A subset of serialization (annotated fields only) plus requirements serialization does not have: **stable field and type identity across builds and binaries** (declaration order and `typeid` are not stable enough to sync on), change detection hooks, and authority/ownership metadata.

**Key considerations**
- This is the use case that forces **stable IDs** into the core of `TypeInfo`/`FieldInfo`, per the networking-from-the-start principle. It cannot be bolted on later.
- Replication wants **byte-level layout** information for efficient delta encoding — shared requirement with GPU interop and C# direct component access.
- Replicated references to entities are handles, which are already stable IDs on the wire — no pointer↔NetGUID translation layer.

### 3. C# binding generation

A build-time generator emits C# wrapper classes and native thunks from the reflected type surface: methods, parameters, return types, factories.

**Key considerations**
- Main consumer of **function** reflection (not just fields) and of the typed invocation path.
- Establishes the pattern of reflection consumers that **emit source code in another language** at build time — shader generation reuses it.
- The generated boundary owns **error-model translation**: native `std::expected` failures surface as idiomatic C# exceptions; exceptions never cross the boundary in either direction (CoreConventions, Error Handling).
- The generator distinguishes three marshaling shapes, derived structurally rather than by per-type annotation: **handle-referenced engine objects** (value-type C# wrappers around handles — no native pointers, no finalizers), **value-marshaled types** (blittable structs, ideally viewing native memory directly), and **opaque resources**.

### 4. GPU interop / shader parameter generation

C++ structs that mirror GPU-side data (uniform/constant buffers, vertex layouts) must match shader layout rules (std140/std430) byte for byte. Reflection either generates the shader-side declaration from the C++ struct, or `static_assert`s the layouts match at compile time.

**Key considerations**
- Pulls **byte-level layout** (offset, size, alignment) into `FieldInfo` requirements.
- Distinct from "shader reflection" in the graphics sense (introspecting compiled shaders) — that metadata comes from the shader toolchain, not from this system.
- The shader generator itself ships with the rendering work, not the reflection milestone; reflection only needs to carry the layout data.

### 5. Editor / asset tooling

Browse types **by name at runtime**, get/set properties on instances whose static type the tool does not know, read annotations for display metadata (ranges, categories, names).

**Key considerations**
- The use case that requires the **runtime registry and erased layer** — uniform property access and invocation over `TypeInfo` looked up at runtime.

### 6. Visual scripting

Discovery of callable functions by name/category and erased invocation with runtime-assembled arguments.

**Key considerations**
- Same machinery as the editor use case, weighted toward functions. The any-erased registry pattern is validated in `PlaygroundReflection/src/` (generic registry, `InvokeStatic`).

### 7. Dynamic modules / hot reload

Types arriving and leaving at runtime (DLL load/unload). Each binary registers its types into the shared registry on load; compile-time reflection only sees the binary it is compiled in, so the runtime registry is the aggregation point.

**Key considerations**
- Forces **unregistration** and registry lifetime rules into the design.
- Type identity across module boundaries must be **name-based** (raw `typeid` comparison is fragile across DLLs) — converges with replication's stable-identity requirement.

---

## Memory & Object Model Implications

The engine-wide decisions — no garbage collector, generational handles as the canonical object reference, ownership as a tree — are recorded in [CoreConventions.md](CoreConventions.md). Consequences for reflection specifically:

- **Reference semantics are read from a small set of known wrapper types**, not markup: by-value / `unique_ptr<T>` = owned, serialize inline; `Handle<T>` = engine-object cross-reference, serializes as an ID, never causes a load; `AssetRef<T>` = asset GUID resolved by the asset system; `Poly<T>` = boxed polymorphic data carrying its dynamic `TypeInfo`. `shared_ptr` is engine-internal plumbing and does not appear in reflected data.
- **No graph-aliasing machinery.** With tree-of-ownership data, the "serialize once + back-reference" pass is gone. The one residual requirement: live handles are remapped to **file-local stable IDs** on save and back on load (runtime slot indices are not stable across sessions) — the same stable-ID machinery the mergeable-text-asset requirement already demands.
- **Polymorphic identity — resolved** (previously an open question), split by direction:
  - *Data → object* (backend payloads, config with type discriminators): registry lookup by name + construct-by-name. Always a core requirement; nothing new.
  - *Object → data*: `Poly<T>` captures the dynamic `TypeInfo` at the point of erasure — its templated constructor sees the concrete type at compile time. No RTTI dependency, no universal base class, deterministic across DLLs. (P2996 cannot enumerate derived classes; vtable-pointer maps are UB — capture-at-erasure is the mechanism.)
- **Template instantiation introspection** is a core requirement, with the vocabulary types (`Handle<T>`, `AssetRef<T>`, `Poly<T>`) as first-class known wrappers alongside containers (`vector<T>`, `optional<T>`, …).
- **Construction is placement-agnostic.** Plain `new` is engine memory anyway (global override — CoreConventions); what the construction API guarantees is that it never hard-codes where the object lives: factories return by value (the validated `[[=Factory{}]]` pattern — `Weapon::Create` returns `Weapon`, `InvokeStaticTyped` allocates nothing) or accept a placement target, so the caller directs construction to heap, arena, or chunk slot. Component deserialization constructs directly into pool storage; the `Injected` parameter pattern accommodates slot/handle injection.

---

## Simulation (ECS) Implications

The simulation will be a full ECS (CoreConventions). For reflection:

- **Components are concrete value types** — no runtime polymorphism in component storage, so dynamic-type lookup is confined to boundary data (`Poly<T>`), out of the simulation path entirely. Struct inheritance for field reuse is fine; reflection flattens base-class fields.
- **The ECS component registry and the reflection `TypeRegistry` must be one system** (or two views over one identity): component ID, size, alignment, and layout are the same facts `TypeInfo` carries. No parallel registration.
- **World serialization and replication operate per component type over contiguous storage**: the typed layer generates one erased operation table per component type ("serialize/replicate N components of this type from this span"). Byte-level layout is promoted from useful to central — including the goal of C# viewing component memory directly through layout-matched structs instead of per-field marshaling.
- **Reflection stays at the boundaries** (load, save, replicate, edit, bind); the frame loop iterates typed component storage and never touches reflection machinery.
- A simple-game **facade owns no state** (CoreConventions facade rule), so reflection, serialization, and replication never know it exists — they see only entities and components.

---

## Converging Requirements

Independent use cases keep landing on the same few core requirements — a sign the base API can stay small:

| Requirement | Demanded by |
|---|---|
| Stable, name-based type & field identity | Replication, hot reload, polymorphic serialization, asset references, handle save/load remap |
| Field enumeration: name, type, annotations | All |
| Template instantiation introspection (containers + vocabulary types) | Serialization, replication, C# binding |
| Byte-level layout (offset, size, alignment) | GPU interop, replication delta encoding, binary cooking, C# direct component access |
| Function reflection + typed invocation | C# binding, factories |
| Erased invocation + runtime registry | Editor, visual scripting, hot reload |
| Construction via annotated factories, placement-agnostic | Serialization, spawning, C# binding |
| Unified type identity with the ECS component registry | Simulation, serialization, replication |

---

## Open Questions

- **Stable ID scheme** — how type/field IDs are derived (qualified-name hash? explicit annotation override for renames?), how renames are migrated, and how this unifies with object-handle save/load remapping and asset GUIDs into one identity story. Priority raised: replication, hot reload, asset references, and the C# boundary all consume it.
- **Text asset format** — not JSON (no comments, noisy diffs); YAML, TOML, or custom — deferred until the asset system, since the serializer backend is pluggable.

Resolved this round: polymorphic identity (→ `Poly<T>` capture-at-erasure plus registry construct-by-name; see Memory & Object Model Implications). Broader open questions (handle granularity, single vs. sibling ID schemes, C# subscription lifetime) are tracked in [CoreConventions.md](CoreConventions.md).

---

## Next Step

Design the base API: `TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry` — shapes, ownership, how the typed and erased layers expose them, and how the `std::meta` backend populates them.
