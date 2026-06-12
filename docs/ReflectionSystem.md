# Reflection System — Design

Use cases and requirements for the reflection system: the substrate for serialization, replication, editor tooling, GPU interop, and C#/visual-scripting integration. Engine-wide decisions referenced here live in [CoreConventions.md](CoreConventions.md).

**Status:** use cases defined. Base API (`TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry`) not yet designed.

## Constraints

1. **`std::meta` never leaks into the public API.** On platforms without C++26 reflection, the same API is populated by a pre-build code generator reading the same annotated source. Game and engine code is identical either way.
2. **Two layers, one source of truth.** A typed compile-time layer (serializers, generators — real types, no erasure) and an erased runtime layer (editor, visual scripting, module loading — uniform signatures over a registry). The erased layer is generated from the typed layer, never maintained in parallel. Validated patterns: `InvokeStaticTyped<Fn>` / `InvokeStatic` in `PlaygroundReflection/src/construction.h`.
3. **Immutable between mutation barriers.** The registry mutates only at defined points (startup, module load/unload, editor asset changes); reads are lock-free in between. No lazy registration. Metadata thread-safety does not extend to the objects accessed through it.
4. **Bottom layer.** Reflection depends on no engine concepts. Contact points with the (undesigned) engine architecture are only: who owns the registry, and when registration phases run.

## Use Cases

### 1. Serialization (assets, saves, config)
Field walk (names, types, annotations), nested types and containers, construction via annotated factories (constructor splicing is unsupported in P2996; `[[=Factory{}]]` static functions are the pattern).
- Format-agnostic: pluggable text/binary backends. Development assets are text, shipped assets are cooked binary, bulk payloads (textures, meshes, audio) live in binary sidecars referenced from the text manifest.
- Mergeable source-controlled assets require deterministic output and stable element IDs.
- Reflected data is a tree (CoreConventions): owned fields inline, cross-references as IDs. No graph-aliasing machinery.

### 2. Network replication
The annotated-field subset of serialization, plus: stable type/field identity across builds and binaries, change detection hooks, authority metadata.
- Forces stable IDs into the core design.
- Uses byte-level layout for delta encoding.
- Entity references are handles — already stable wire IDs.

### 3. C# binding generation
A build-time generator emits C# wrappers and native thunks from the reflected type surface (methods, parameters, return types, factories).
- Main consumer of function reflection and the typed invocation path.
- Bindings translate error models (`std::expected` ⇄ C# exceptions).
- Three marshaling shapes, derived structurally: handle-referenced engine objects, value-marshaled (blittable) types, opaque resources.

### 4. GPU interop
Generate shader-side struct declarations from C++ structs, or `static_assert` layout match (std140/std430).
- Requires byte-level layout (offset, size, alignment) in `FieldInfo`.
- Distinct from compiled-shader reflection, which comes from the shader toolchain.

### 5. Editor / asset tooling
Browse types by name at runtime, erased property get/set on instances of statically unknown type, display annotations. Requires the runtime registry and erased layer.

### 6. Visual scripting
Function discovery by name/category plus erased invocation. Same machinery as 5, weighted toward functions.

### 7. Dynamic modules / hot reload
Types arrive and leave at runtime (DLL load/unload); each binary registers into the shared registry.
- Requires unregistration and registry lifetime rules.
- Type identity across module boundaries is name-based, not `typeid`-based.

### 8. Cross-runtime type model
One `TypeInfo` contract over C++, C#, and visual scripting. Each runtime keeps its own reflection mechanism (`std::meta`, .NET metadata, script asset definitions) and projects into the registry through a **provider**.
- `TypeInfo` must be constructible at runtime — script types are defined by data, not compiled code.
- Erased op-tables (construct/get/set/invoke) are per-type and provider-supplied (native thunks, managed runtime, interpreter).
- `TypeInfo` carries a provenance facet (native/managed/script); byte layout is optional (absent for script types).
- Generated C# wrappers are derivative of native `TypeInfo` and never re-project; only attributed, authored C# types register.
- Stable IDs span all provenances.

## Object Model Consequences

- Reference semantics are read from the type system: by-value / `unique_ptr<T>` owned-inline; `Handle<T>`, `AssetRef<T>`, `Poly<T>` as known wrappers (CoreConventions vocabulary table). No `shared_ptr` in reflected data.
- Live handles remap to file-local stable IDs on save and back on load.
- Polymorphic identity: *data → object* via registry construct-by-name (data carries the discriminator); *object → data* via `Poly<T>`, which captured the `TypeInfo` at erasure. (P2996 cannot enumerate derived classes; vtable-pointer maps are UB.)
- Template introspection covers containers and the vocabulary types.
- Construction is placement-agnostic: factories return by value (the validated pattern) or accept a placement target; component deserialization constructs directly into chunk storage. `Injected` parameters cover slot/handle injection.

## ECS Consequences

- Components are concrete value types; dynamic-type lookup is confined to boundary data (`Poly<T>`), out of the simulation path.
- The ECS component registry and the reflection `TypeRegistry` are one system (or two views of one identity).
- World serialization/replication run per component type over contiguous storage, via one generated erased op-table per component type. Layout data enables C# direct access to component memory.
- Reflection runs at boundaries (load, save, replicate, edit, bind) — never in the frame loop.

## Converging Requirements

| Requirement | Demanded by |
|---|---|
| Stable, name-based type & field identity | Replication, hot reload, polymorphic serialization, asset references, handle save/load remap |
| Field enumeration: name, type, annotations | All |
| Template instantiation introspection (containers + vocabulary types) | Serialization, replication, C# binding |
| Byte-level layout (offset, size, alignment) | GPU interop, replication delta encoding, binary cooking, C# direct component access |
| Function reflection + typed invocation | C# binding, factories |
| Erased invocation + runtime registry | Editor, visual scripting, hot reload |
| Runtime-defined types + provider-based registry | Visual scripting, C# projection, editor |
| Construction via annotated factories, placement-agnostic | Serialization, spawning, C# binding |
| Unified type identity with the ECS component registry | Simulation, serialization, replication |

## Open Questions

- **Stable ID scheme** — how type/field IDs are derived (qualified-name hash? annotation override for renames?), how renames migrate, and whether type/field/object/asset identity unify into one scheme. Highest priority: consumed by replication, hot reload, save/load, the C# boundary, and all three runtimes.
- **Text asset format** — YAML, TOML, or custom; deferred until the asset system (the backend is pluggable).

## Next Step

Design the base API: `TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry` — shapes, ownership, the typed and erased surfaces, and how providers populate them.
