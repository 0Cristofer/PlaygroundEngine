# Reflection System, Design

Use cases and requirements for the reflection system: the substrate for serialization, replication, editor tooling, GPU interop, and C#/visual-scripting integration. Engine-wide decisions referenced here live in [CoreConventions.md](CoreConventions.md).

**Status:** use cases defined. Base API (`TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry`) not yet designed.

## Constraints

1. **`std::meta` never leaks into the public API.** This is a layering rule first: upper layers never care how metadata was produced. Compiler portability is a conformance bet, not an engine-tooling commitment: desktop timelines assume Clang and MSVC reach usable C++26 reflection soon enough (Windows realistically lands on Clang first), and consoles are aspirational and receive no design budget now. A pre-build generator populating the same API from annotated source stays *possible* under this rule, as a last resort rather than a plan. The binding constraint on metadata shape is use case 8: runtime providers must be able to construct the same API without `std::meta`.
2. **Two layers, one source of truth.** A typed compile-time layer (serializers, generators, real types, no erasure) and an erased runtime layer (editor, visual scripting, module loading, uniform signatures over a registry). The erased layer is generated from the typed layer, never maintained in parallel. Validated patterns: `InvokeStaticTyped<Fn>` / `InvokeStatic` in `PlaygroundReflection/src/construction.h`.
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
- Entity references are handles, already stable wire IDs.

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
- `TypeInfo` must be constructible at runtime, script types are defined by data, not compiled code.
- Erased op-tables (construct/get/set/invoke) are per-type and provider-supplied (native thunks, managed runtime, interpreter).
- `TypeInfo` carries a provenance facet (native/managed/script) through the facet mechanism ([ReflectionFacets.md](ReflectionFacets.md)); byte layout is optional (absent for script types).
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
- Reflection runs at boundaries (load, save, replicate, edit, bind), never in the frame loop.

## The Erased Op-Table

Constraint 2 splits reflection into a typed compile-time layer and an erased runtime layer. The **op-table** is the concrete form of that erased layer: per type, the set of type-erased operations a consumer can perform on a `void*` without knowing the static type. It is the reflection system's own vtable, with two differences: it lives outside the object (referenced from `TypeInfo`, not embedded in every instance), and it is generated from metadata rather than emitted by the compiler.

The operations are everything that can be done to a type or an instance of it:
- **construct** / **destroy**: lifetime, placement-agnostic (return by value or into a caller-supplied slot; see Object Model Consequences).
- **get field** / **set field**: erased property access, addressed by stable member id.
- **invoke**: call a reflected function with erased arguments and return value.
- **stringify** / **serialize**: render or encode an instance.

**Where the ops live.** The table is not a monolith that supersedes `FieldInfo` / `FuncInfo`. The member-scoped ops are leaves on the metadata objects that own them: `invoke` on `FuncInfo`, `get` / `set` on `FieldInfo`. Only the type-scoped ops (`construct`, `destroy`, `stringify`) have no member to attach to and hang on `TypeInfo` directly. The op-table proper is the type-level aggregator that dispatches into those leaves. This is why the erased surface can be grown one thunk at a time before any unified table exists: `TypeInfo`'s stringify thunk and `FuncInfo`'s invoke thunk are already op-table entries under this model. Facet thunks (`StringFacet`, `SequenceFacet`, see [ReflectionFacets.md](ReflectionFacets.md)) are op-table leaves in the same sense: type-scoped, erased, `TypedRef`-ABI, and provider-fillable at runtime.

**Two dimensions the table must eventually span:**
- **Providers** (use case 8): native tables are filled by the compile-time builder via `std::meta`, managed types by the .NET runtime, script types by the interpreter. One table shape, three fillers. This is the mechanism that lets a single `TypeInfo` contract cover all three runtimes.
- **Storage** (ECS consequences): one generated table per component type drives serialization and replication over contiguous storage, and its layout data lets C# read component memory directly.

**Not being implemented now.** The erased ops are being built incrementally on the existing metadata objects (`TypeInfo`, `FieldInfo`, `FuncInfo`), not as the unified op-table abstraction. Consolidating them into a first-class per-type table, with the provider indirection and a uniform erased-reference type, is deferred. Each reason is a missing input to that convergence point:
- **Stable member identity.** A type-level `getField(obj, id)` / `invoke(obj, id, ...)` addresses members by stable id; that scheme is the highest-priority open question below and is not yet fixed.
- **Storage model.** `construct`'s signature (return-by-value vs. placement into chunk storage) depends on the ECS layout, which does not exist yet (the GameObject model is a placeholder).
- **Provider seam.** Only the native provider exists; designing the multi-provider indirection against a single provider risks fixing the wrong seam.
- **Erased-reference / const model.** A bare `void*` discards const, so an erased mutating call cannot be rejected at the boundary. How the erased reference carries mutability (and provenance) is a table-wide decision, not a per-op one. As an exploratory spike, `FuncInfo` enforces const at the leaf via split `Invoke(void*)` / `InvokeConst(const void*)` entry points (const-qualification is metadata; the type system decides which entry point a caller can reach). This borrows C++'s own const system rather than inventing an erased-reference type. The fat `{ptr, mutability}` reference is now prototyped as `TypedRef` for the invoke leaf (see [Erased invocation ABI](#erased-invocation-abi-the-invoke-leaf) below); its table-wide adoption across every op is still the open decision.

A convergence point is designed after the things converging into it. The op-table becomes a dedicated design pass once the stable-id scheme exists, or when the second provider or the ECS storage model forces its shape.

### Erased invocation ABI (the invoke leaf)

The `invoke` leaf on `FuncInfo` is a borrowed-pointer, type-tagged ABI, not a value-boxing one. An earlier `std::any` sketch was rejected: `std::any` owns values, so it cannot represent reference or out-parameters and forces every argument and return type to be copy-constructible. The chosen model follows the UE `UFunction` / Qt `void**` / Mono `void**` / libffi convergence: the caller owns all argument and return storage, and the erased layer only borrows it or constructs into it, never owns.

- **Erased reference.** `TypedRef { const TypeInfo* type; void* data; bool isConst; }` is a borrowed, type-tagged, const-tagged pointer to caller storage. It is used for both arguments and the return slot, and is the concrete prototype of the table-wide `{ptr, mutability}` erased reference deferred above; `get` / `set` field are expected to adopt it at op-table convergence.
- **Entry points.** `Invoke(void*, ...)` and `Invoke(const void*, ...)` are two overloads, thin forwarders over a single stored thunk pointer (the shape that lets a managed or script provider fill the same slot). Overload resolution on the object pointer's constness routes the caller: a const instance can only bind the `const void*` overload, which returns `ConstViolation` for a mutating function. Statics report `IsConst() == true` and ignore the object pointer.
- **Argument binding.** The generated thunk binds each argument by the parameter's own type: mutable-ref params bind (and reject a const `TypedRef`), const-ref params bind, rvalue-ref and move-only by-value params move, copyable by-value params copy. A copyable argument survives the call; the erased layer does not consume it. (Opt-in move for copyable by-value params is a future additive `TypedRef` flag, deferred until a profiled boundary needs it.)
- **Return slot.** The thunk constructs the return into the caller's `ret` storage, so move-only returns work and no copyability is required. A **reference return** is erased as a pointer to the referent (stored into the slot and tagged as that pointer type, which keeps it distinct from a same-typed value return); the typed sugar surfaces it as `std::reference_wrapper<T>`. `ret` is type-checked like an argument (`ReturnTypeMismatch`, including a value slot handed to a `void` function, which is rejected without calling it); `ret == {}` discards a value return and also covers `void`.
- **Errors.** `std::expected<void, InvokeError>` with `ArityMismatch`, `TypeMismatch`, `ConstViolation`, `ReturnTypeMismatch`, plus the failing argument index. These checks are the malformed-input defense for the RPC path (use case 2), not only caller ergonomics.
- **Typed sugar.** `funcInfo.InvokeAs<R>(obj, args...)` builds the `TypedRef` array from real C++ arguments and moves `R` out; the object pointer's constness selects the matching `Invoke` overload. It tags each argument with the argument's own reflected type, so a mismatch is rejected rather than reinterpreted. This is why the metadata types and the builder that produces `TypeOf` share one module: the member's out-of-line definition needs `TypeOf`. It is runtime-checked convenience; the compile-time-checked call (`Fn` known statically) is the separate typed layer (`InvokeStaticTyped<Fn>`), not this.

Overload disambiguation and cross-build function identity are deferred to the stable-id scheme: `TypeInfo` exposes the whole overload set (`FindFunctionsByName`) and the caller selects one `FuncInfo`.

## Converging Requirements

| Requirement | Demanded by |
|---|---|
| Stable, name-based type & field identity | Replication, hot reload, polymorphic serialization, asset references, handle save/load remap |
| Field enumeration: name, type, annotations | All |
| Template instantiation introspection (containers + vocabulary types), *satisfied by facets ([ReflectionFacets.md](ReflectionFacets.md))* | Serialization, replication, C# binding |
| Byte-level layout (offset, size, alignment) | GPU interop, replication delta encoding, binary cooking, C# direct component access |
| Function reflection + typed invocation | C# binding, factories |
| Erased invocation + runtime registry | Editor, visual scripting, hot reload |
| Runtime-defined types + provider-based registry | Visual scripting, C# projection, editor |
| Construction via annotated factories, placement-agnostic | Serialization, spawning, C# binding |
| Unified type identity with the ECS component registry | Simulation, serialization, replication |

## Open Questions

- **Stable ID scheme**, how type/field IDs are derived (qualified-name hash? annotation override for renames?), how renames migrate, and whether type/field/object/asset identity unify into one scheme. Highest priority: consumed by replication, hot reload, save/load, the C# boundary, and all three runtimes.
  - *Settled within one build:* runtime `TypeInfo` instance identity is canonical per type. `TypeOfMeta` dealiases before caching, so every alias spelling (`std::uint16_t`, `std::underlying_type_t<E>`, `unsigned short`) resolves to one `TypeInfo` and pointer identity equals type identity, which is what annotation matching, serialization, and C# dedup compare on. Open here is only the *cross-build, name-based* ID.
- **Text asset format**, YAML, TOML, or custom; deferred until the asset system (the backend is pluggable).
- **Op-table consolidation**, when to unify the incrementally-built erased ops into a first-class per-type table (see [The Erased Op-Table](#the-erased-op-table)). Gated on the stable-id scheme, the ECS storage model, and the arrival of a second provider.

## Next Step

Design the base API: `TypeInfo`, `FieldInfo`, `FuncInfo`, `TypeRegistry`, shapes, ownership, the typed and erased surfaces, and how providers populate them.
