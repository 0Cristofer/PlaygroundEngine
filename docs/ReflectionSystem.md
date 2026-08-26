# Reflection System, Design

Use cases and requirements for the reflection system: the substrate for serialization, replication, editor tooling, GPU interop, and C#/visual-scripting integration. Engine-wide decisions referenced here live in [CoreConventions.md](CoreConventions.md).

**Status:** the typed core is implemented and tested (`TypeInfo`, `FieldInfo`, `FuncInfo` and the rest of the
type model, in `PlaygroundEngine/src/Reflection/`). `TypeRegistry` is the one piece of the base API still
undesigned, and it belongs to the layer above: everything the registry needs from the core exists, since a
`TypeInfo` stores only non-owning spans and requires no static storage duration. The known core-level
limitation is invoker-forced instantiation ([ReflectionInternals.md](ReflectionInternals.md#invokers-defeat-lazy-member-instantiation)).

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
- Method/operator/conversion/constructor metadata is materialized through `TypeOf<T>` (the invoke handle), not the metadata handle `TypeMetaOf<T>` which exposes empty op-lists; a bound type is named for invocation, which authored engine types always satisfy. See [ReflectionInternals.md](ReflectionInternals.md) (the two tiers).
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
- `TypeInfo` carries a provenance facet (native/managed/script) through the facet mechanism ([ReflectionInternals.md, Facets](ReflectionInternals.md#facets)); byte layout is optional (absent for script types).
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

**Where the ops live.** The table is not a monolith that supersedes `FieldInfo` / `FuncInfo`. The member-scoped ops are leaves on the metadata objects that own them: `invoke` on `FuncInfo`, `get` / `set` on `FieldInfo`. Only the type-scoped ops (`construct`, `destroy`, `stringify`) have no member to attach to and hang on `TypeInfo` directly. The op-table proper is the type-level aggregator that dispatches into those leaves. This is why the erased surface can be grown one thunk at a time before any unified table exists: `TypeInfo`'s stringify thunk and `FuncInfo`'s invoke thunk are already op-table entries under this model. Facet thunks (`StringFacet`, `SequenceFacet`, see [ReflectionInternals.md, Facets](ReflectionInternals.md#facets)) are op-table leaves in the same sense: type-scoped, erased, `TypedRef`-ABI, and provider-fillable at runtime.

**Two dimensions the table must eventually span:**
- **Providers** (use case 8): native tables are filled by the compile-time builder via `std::meta`, managed types by the .NET runtime, script types by the interpreter. One table shape, three fillers. This is the mechanism that lets a single `TypeInfo` contract cover all three runtimes.
- **Storage** (ECS consequences): one generated table per component type drives serialization and replication over contiguous storage, and its layout data lets C# read component memory directly.

**Not being implemented now.** The erased ops are being built incrementally on the existing metadata objects (`TypeInfo`, `FieldInfo`, `FuncInfo`), not as the unified op-table abstraction. Consolidating them into a first-class per-type table, with the provider indirection and a uniform erased-reference type, is deferred. Each reason is a missing input to that convergence point:
- **Stable member identity.** A type-level `getField(obj, id)` / `invoke(obj, id, ...)` addresses members by stable id; that scheme is the highest-priority open question below and is not yet fixed.
- **Storage model.** `construct`'s signature (return-by-value vs. placement into chunk storage) depends on the ECS chunk layout, which does not exist yet (the first iteration holds components behind `shared_ptr` in map indexes).
- **Provider seam.** Only the native provider exists; designing the multi-provider indirection against a single provider risks fixing the wrong seam.

A convergence point is designed after the things converging into it. The op-table becomes a dedicated design pass once the stable-id scheme exists, or when the second provider or the ECS storage model forces its shape.

### Erased ABI

**The erased-reference / const model is settled: every erased op takes its object as a `TypedRef`.** C++'s own const system was the earlier answer (split `Invoke(void*)` / `Invoke(const void*)` overloads, with the type system routing the caller), and it guards only the first hop: the moment an op hands back a borrow, constness is a runtime bool and the caller is holding a `void*` again. Each consumer then re-derived it, which the debug panel proved by carrying a `readOnly` flag through five functions. `(const TypeInfo*, void*)` was already being passed as a pair through every erased path; naming that pair is what makes it verifiable.

- **The object is a borrow.** `GetValue` / `SetValue` / `GetRef`, the `TypeInfo` field forwarders, `Invoke`, `Stringify`, `Destroy`, and every facet op take `const TypedRef&`. `void*` survives only inside `TypedRef::Data`, in the thunk aliases a provider fills, and in the erased metadata payloads (`AnnotationInfo::Value`, `FacetEntry::Data`).
- **Three behaviors, not one.** Ref-returning ops (`GetRef`, `SequenceFacet::ElementRef`, `BaseInfo::Upcast`) **propagate** the object's `IsConst` into the borrow they return. Value reads (`GetAs`, `Stringify`) only **check**. Mutating ops (`SetValue`, `Append`, `Clear`, `Reserve`, `Assign`, a non-const `Invoke`) **reject** with `ConstViolation`. Constness does not come back out of a value read.
- **A borrow is never an offer.** Every ref-returning op sets `Movable = false` explicitly.
- **`TypedRef::Dereference()` is the pointer hop**, and the one place cv nodes are produced and consumed: it
  loads the pointee, peels its cv nodes, and reports `NullPointer` / `NotAPointer` / `NotAnObjectPointer`
  (a function pointer names no object). It does **not** carry the borrow's own constness across, because
  C++ const stops at a pointer: `Foo* const p` still writes through `p->x`. Only the pointee type's const
  reaches the result.
- **The object's type is checked, as an error value.** `FieldError::ObjectTypeMismatch` / `InvokeError::ObjectTypeMismatch`, not a contract: contracts are `ignore` in Release, and a mistagged object is a wrong-offset read into unrelated memory, which has to be caught in a shipping build and at a decode boundary alike. It is the same choice the argument tags already make. `FieldInfo` / `FunctionInfo` carry `GetDeclaringType()` and `BaseInfo` carries `GetDerivedType()` to answer it.
- **Leaf strict, aggregator adjusts.** A member thunk indexes from its declaring type's layout, so it requires the borrow to name that type; an inherited member is reached by walking `GetBases()` and calling `BaseInfo::Upcast(const TypedRef&)`, one hop at a time. Peeling per direct base is what keeps it unambiguous under repeated non-virtual bases, where a single derived-to-base search has no unique answer (virtual inheritance is rejected outright, which is why repetition is possible at all). Finding an inherited member by name, and adjusting the borrow on the caller's behalf, belongs to the aggregator layer and lands with the stable-id scheme.
- **No object at all is its own case.** A static or free function ignores the object and is called through `Invoke(args, ret)` / `InvokeStaticAs<R>(args...)`; a member reached without one answers `ObjectRequired`. `InvokeAs<R>(objectRef, args...)` takes an already-upcast borrow, which is how an inherited function is invoked.
- **Destroying through a read-only borrow is allowed**, deliberately. C++ destroys const objects routinely, and an erased destroy that refused would be narrower than the language it projects. What guards a lifetime is ownership, not a borrow's mutability.
- **A facet checks its owner too.** The builder stamps the providing type onto any facet that offers a
  `SetOwnerType` hook (read generically, the way `Supersedes` is), and each op rejects an object of another
  type: `FacetError::ObjectTypeMismatch` where there is an error channel, a `pre` on the value-returning ops
  (`Size`, `ElementRef`, `View`, `Value`) which have none. Closing that asymmetry means giving those ops an
  error channel, which is deferred.
- **`TypedRef` is the in-process ABI, not a wire format.** `Type` is a process-local pointer, so replication and the remote inspector carry a stable id and resolve it at the decode boundary. That id scheme is deferred (see the open questions).
- **`Type` is the static type at the erasure site**, not the most-derived type: `TypedRefOf(baseLvalue)` on a derived object tags the base. Dynamic identity is `Poly<T>`'s capture-at-erasure job, not this.
- **`Movable` on an object ref is currently ignored by every op.** It is reserved, with the meaning it already carries on an argument: when rvalue-ref-qualified members become invocable, it is the caller's offer to move out of the object. A const ref is never an offer.
- **`TypedRef` models mutability, not the full cv set.** `TypedRefOf` tolerates a volatile lvalue, but there is no `IsVolatile`; volatile members are excluded at thunk-build time.

#### The invoke leaf

The `invoke` leaf on `FuncInfo` is a borrowed-pointer, type-tagged ABI, not a value-boxing one. An earlier `std::any` sketch was rejected: `std::any` owns values, so it cannot represent reference or out-parameters and forces every argument and return type to be copy-constructible. The chosen model follows the UE `UFunction` / Qt `void**` / Mono `void**` / libffi convergence: the caller owns all argument and return storage, and the erased layer only borrows it or constructs into it, never owns.

- **Erased reference.** `TypedRef { const TypeInfo* type; void* data; bool isConst; }` is a borrowed, type-tagged, const-tagged pointer to caller storage. It is used for the object, the arguments, and the return slot alike, which is what makes the erased surface one shape for a provider to fill. `TypedRefOf(object)` is the one entry point that builds it from a live object: it reads `IsConst` and `Movable` off the value category the caller wrote, so `std::move` at the call site is the offer to move out, and the single `const_cast` the erased model needs lives there rather than at each site. It reaches `TypeMetaOf`, so erasing a value never materializes an op. An output slot over uninitialized storage is not an erased object and is still built by hand.
- **Entry points.** `Invoke(const TypedRef& object, ...)` is a thin forwarder over a single stored thunk pointer (the shape that lets a managed or script provider fill the same slot). The borrow's `IsConst` decides: a mutating function reached through a read-only object returns `ConstViolation`. Statics report `IsConst() == true`, and `CallsWithoutObject()` is what the erased path branches on, since a free function is not a static member but ignores the object just the same.
- **Argument binding.** The generated thunk binds each argument by the parameter's own type: mutable-ref params bind (and reject a const `TypedRef`), const-ref params bind, rvalue-ref and move-only by-value params move, copyable by-value params copy. A copyable argument survives the call; the erased layer does not consume it. (Opt-in move for copyable by-value params is a future additive `TypedRef` flag, deferred until a profiled boundary needs it.)
- **Return slot.** The thunk constructs the return into the caller's `ret` storage, so move-only returns work and no copyability is required. A **reference return** is erased as a pointer to the referent (stored into the slot and tagged as that pointer type, which keeps it distinct from a same-typed value return); the typed sugar surfaces it as `std::reference_wrapper<T>`. `ret` is type-checked like an argument (`ReturnTypeMismatch`, including a value slot handed to a `void` function, which is rejected without calling it); `ret == {}` discards a value return and also covers `void`.
- **Errors.** `std::expected<void, InvokeError>` with `ArityMismatch`, `TypeMismatch`, `ConstViolation`, `ReturnTypeMismatch`, `ObjectTypeMismatch`, `ObjectRequired`, plus the failing argument index. The object errors are separate from `TypeMismatch`, which names an argument by index; the object is not an argument. These checks are the malformed-input defense for the RPC path (use case 2), not only caller ergonomics.
- **Typed sugar.** `funcInfo.InvokeAs<R>(object, args...)` takes the real object and builds the `TypedRef` array from real C++ arguments, then moves `R` out. Taking the typed object is what keeps the first-hop const guarantee a compile error: the erased form cannot know statically whether its borrow is writable, so the `*As` family is where the type system still answers. An overload takes the object already erased, constrained so a `TypedRef` never binds the deduced form and gets erased a second time. The object-less form is a separate name (`InvokeStaticAs`), since a deduced object parameter cannot be told from a first argument. It tags each argument with the argument's own reflected type, so a mismatch is rejected rather than reinterpreted. This is why the metadata types and the builder that produces `TypeOf` share one module: the member's out-of-line definition needs `TypeOf`. It is runtime-checked convenience; the compile-time-checked call (`Fn` known statically) is the separate typed layer (`InvokeStaticTyped<Fn>`), not this.

Overload disambiguation and cross-build function identity are deferred to the stable-id scheme: `TypeInfo` exposes the whole overload set (`FindFunctionsByName`) and the caller selects one `FuncInfo`.

## Converging Requirements

| Requirement | Demanded by |
|---|---|
| Stable, name-based type & field identity | Replication, hot reload, polymorphic serialization, asset references, handle save/load remap |
| Field enumeration: name, type, annotations | All |
| Template instantiation introspection (containers + vocabulary types), *satisfied by facets ([ReflectionInternals.md, Facets](ReflectionInternals.md#facets))* | Serialization, replication, C# binding |
| Byte-level layout (offset, size, alignment) | GPU interop, replication delta encoding, binary cooking, C# direct component access |
| Function reflection + typed invocation | C# binding, factories |
| Erased invocation + runtime registry | Editor, visual scripting, hot reload |
| Runtime-defined types + provider-based registry | Visual scripting, C# projection, editor |
| Construction via annotated factories, placement-agnostic | Serialization, spawning, C# binding |
| Unified type identity with the ECS component registry | Simulation, serialization, replication |

## Open Questions

- **Per-type customization, one mechanism or four.** Every erased consumer (debug panel, serializer,
  replicator, C# generator) eventually wants per-type behavior for the same types: a quaternion edited as
  Euler angles, a colour as a swatch, a handle as a picker. Three tiers exist for attaching that, and the
  selection rule is whether the metadata is a property *of the type* (annotation, or `TypeInfoTraits` for a
  type you do not own) or of *a consumer's relationship to the type* (the runtime registry, so five
  consumers' policies do not accrete onto one declaration). Open: whether the registry is drawer-specific
  or general, whether it is keyed per type or also per field, and how game code registers into it. The
  trigger to design it is the third custom type, since facet kinds are a closed set but custom types are
  not. Precedent: Unreal's `IPropertyTypeCustomization`, Unity's `CustomPropertyDrawer`, Godot's
  `EditorInspectorPlugin`, all of which are runtime-registered because game code must be able to add one.

- **Stable ID scheme**, how type/field IDs are derived (qualified-name hash? annotation override for renames?), how renames migrate, and whether type/field/object/asset identity unify into one scheme. Highest priority: consumed by replication, hot reload, save/load, the C# boundary, and all three runtimes.
  - *Settled within one build:* runtime `TypeInfo` instance identity is canonical per type. `TypeMetaOf` dealiases before caching, so every alias spelling (`std::uint16_t`, `std::underlying_type_t<E>`, `unsigned short`) resolves to one `TypeInfo` and pointer identity equals type identity, which is what annotation matching, serialization, and C# dedup compare on. Open here is only the *cross-build, name-based* ID.
- **Text asset format**, YAML, TOML, or custom; deferred until the asset system (the backend is pluggable).
- **Op-table consolidation**, when to unify the incrementally-built erased ops into a first-class per-type table (see [The Erased Op-Table](#the-erased-op-table)). Gated on the stable-id scheme, the ECS storage model, and the arrival of a second provider.

## Next Step

The invoker-forced-instantiation limitation is **closed for types**: op materialization, both the invokers and
the function/operator/conversion/constructor *lists* themselves (building a list reifies member bodies, so the
list is deferred too), happens only through `TypeOf<T>`. Reflecting a type as metadata (`TypeMetaOf<T>`) can no
longer fail to compile because of a member nobody calls (see
[ReflectionInternals.md](ReflectionInternals.md), the two tiers).

The residual gap is the parallel path `NamespaceInfo::GetFunctions()`, which reflects free functions with no
demand tier and carries the same reification hazard for an ill-formed-`constexpr` free function. Beyond that, the
core is settled; the registry, the provider seam, stable IDs, `Poly<T>` and the erased sugar (`Equals`, `CopyTo`)
are all the layer above.
