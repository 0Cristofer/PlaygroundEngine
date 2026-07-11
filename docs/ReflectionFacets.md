# Reflection Facets: Design

**Status:** implemented 2026-07-10 (`EnumerationInfo` migrated and renamed `EnumerationFacet`, `StringFacet`, `SequenceFacet`; consumer proof in `ObjectToString`). Generalized 2026-07-11: one `MakeFacets()` hook per type (a `std::tuple`, so a type may carry several facets), the assembly names no facet kind, and each built-in facet is a `{class file, builder file}` pair, the same shape a user follows. Design pass by `engine-architect` on 2026-07-09.

Implementation note: a facet is two files, mirroring the engine's `:X` / `:XBuilder` split. The **class partition** (`StringFacet.cppm`, `SequenceFacet.cppm`, `EnumerationFacet.cppm`) is an encapsulated class, private thunks behind checked methods, imported by consumers; it carries no `<meta>`. The **builder partition** (`Builder/StringFacetBuilder.cppm`, etc.) holds the thunks and the `TypeInfoTraits<T>` specialization that returns the facet. The specializations live beside the facet class, not in the standalone `TypeInfoTraits` module, because a `MakeFacets()` return type names the facet class, a reflection partition that module cannot import; `TypeInfoTraits` stays a separate importable module for the third-party extension story. `Builder/FacetsBuilder.cppm` is the closed engine machinery a facet author never touches: it detects the hook with `requires { TypeInfoTraits<T>::MakeFacets(); }`, keys each entry by the facet's own type, and reads its `Supersedes` flag generically, naming no facet kind. `TypeBuilder` imports the built-in facet builders (its manifest of which facets ship), the same way it imports every other sub-builder. Facets currently model `char` strings only, since `StringFacet::View` yields a `std::string_view`; a non-`char` `std::basic_string` reflects structurally until a wide-string case in reflected data forces the issue.

How a type exposes a semantic protocol (enumeration, string, sequence, later map/optional/handle) through its `TypeInfo`, superseding the raw structural view where that structure is an implementation detail.

## Problem

Recursive reflection now reaches arbitrary types, including `TypeInfo` itself and the std types it pulls in. Some types are not useful reflected raw: a container's fields (allocator pointers, size members) are meaningless to every consumer; what consumers need is the sequence or map protocol. Reflecting libstdc++ internals is also a liability: they are GCC-version-unstable (poison for cross-build stable IDs and mergeable-asset determinism), a builder-robustness minefield, and compile-time/binary bloat per container instantiation.

Enums already have a specialized view (`EnumerationInfo`, renamed `EnumerationFacet` when it moved into the table), stored as a one-off nullable member on `TypeInfo`. One member per special kind does not scale, and a third such view is already committed: the provenance facet of [ReflectionSystem.md](ReflectionSystem.md) use case 8.

## Decision

Two layers, mirroring what .NET actually does (closed CLR kinds on `Type`, open converter registries beside it):

1. **Language-level kinds** stay in `TypeKind` / `TypeTraits`. Closed set, defined by the language, already built.
2. **Semantic protocols** become **facets**: plain structs of data plus function-pointer thunks, captured at reflection-build time where `T` is statically known, stored on `TypeInfo` in one uniform table, queried by facet type.

Why the protocol layer lives in the reflection system and not in each consumer: once a consumer holds only a `TypeInfo`, compile-time knowledge is gone; erased consumers (editor, C# boundary, script VM) cannot run template detection after the fact, unlike C#, where a serializer can interrogate a `Type` for interfaces at runtime. Thunks must be captured where the concrete type is statically known. This is the same capture-at-erasure rationale as `Poly<T>` and the existing stringify and field thunks.

Precedent followed: Qt (`QMetaType` plus registered `QSequentialIterable` / `QAssociativeIterable` erased iteration ops). Anti-pattern avoided: Unreal's `FArrayProperty` / `FMapProperty` subclass tree, which welds the protocol into a closed class hierarchy and made container nesting painful for years. Here, nesting is free: `SequenceFacet::Element` is a `TypeReference` that may resolve to a type that itself has a facet.

## Design

### Storage on TypeInfo

```cpp
export struct FacetEntry
{
    // Key must stay a lazy TypeReference, never a resolved const TypeInfo*. Resolving the key's
    // TypeInfo while another TypeInfo's consteval construction is in flight recreates the TypeOfMeta
    // recursion knot that TypeReference exists to break.
    TypeReference Key;
    const void* Data = nullptr;
};
```

`TypeInfo` gains `std::span<const FacetEntry> _facets`, replacing the `const EnumerationInfo* _enumeration` member and constructor parameter. Accessors:

```cpp
std::span<const FacetEntry> GetFacets() const;

template <typename Facet>
const Facet* GetFacet() const
{
    for (const FacetEntry& entry : _facets)
        if (&entry.Key.Get() == &TypeOf<Facet>())
            return static_cast<const Facet*>(entry.Data);
    return nullptr;
}
```

Decisions baked in:

- **Generic span from day one**, not named pointers behind a generic accessor. Three facets exist or are committed (enumeration, sequence family, provenance); named pointers would be a parallel old path that gets migrated anyway. A linear scan over a 0 to 3 entry constant array is optimal at that size, and reflection runs at boundaries, never in the frame loop.
- **The key is the facet type's own `TypeInfo`.** Pointer identity of `TypeInfo` is already the settled in-build identity rule; a parallel tag/id scheme would be a second identity system for no gain. It also makes facet presence enumerable data for free: tooling walks `GetFacets()`, resolves each key to a display name, and the facet contents are themselves reflected structs. No extra mechanism for the editor.
- **Canonical comparison is on the resolved `TypeInfo` address.** Comparing resolver function pointers (`entry.Key.Resolve == &detail::TypeOfMeta<^^F>`) is an equivalent fast path but not the canonical identity; resolver symbol identity has GCC 16 sharp edges (see the dealias handling in `MetaCommon.cppm`).
- No `GetEnumeration()` accessor: enumeration is reached through the generic `GetFacet<EnumerationFacet>()` like every other facet (a dedicated forwarder was tried and dropped as adding nothing over the generic query). The typed enum sugar that remains, `EnumToName` / `EnumFromName`, sits in the reflection API layer, not on `TypeInfo`.

### Attachment: TypeInfoTraits is the single seam

One path, not two. `TypeInfoTraits<T>` already is the per-type customization surface with constrained partial specializations as its idiom (the enum specialization proves the pattern under GCC 16). Facet provision lives there too; `TypeBuilder` asks the trait, it does not run its own concept checks. Std containers, user containers, and future vocabulary types (`Handle<T>`, `AssetRef<T>`) all use the same seam, and precedence (explicit user specialization beats constrained default) is answered by the language's partial-ordering rules rather than an engine rule.

A specialization that provides facets exposes **one** consteval hook, `MakeFacets()`, returning a `std::tuple` of them. One hook, not one per kind: the assembly then never names a facet type, and a type that carries several facets (or none, `std::tuple{}`) falls out of the same seam as one that carries a single facet. The tuple is the minimal type that expresses "zero-or-more heterogeneous facets", since facet classes share no base by design.

```cpp
template <typename T, typename Allocator>
struct TypeInfoTraits<std::vector<T, Allocator>> : TypeInfoTraitsDefaults
{
    static consteval auto MakeFacets()   // std::tuple<SequenceFacet>
    {
        return std::tuple{detail::MakeResizableSequenceFacet<std::vector<T, Allocator>>()};
    }
};
```

`Builder/FacetsBuilder.cppm` detects the hook with a `requires` expression and assembles the entry table, keying each entry off `decltype` of the tuple element, so it names no facet kind:

```cpp
template <std::meta::info MetaType>
consteval auto MakeFacetsFromType();   // std::array<FacetEntry, N>, N = tuple_size of the hook's result
```

Each facet instance is a `static constexpr` local (one per tuple element) whose address goes into its entry. The enumeration facet flows through this same hook (its `MakeFacets()` returns `std::tuple{MakeEnumerationFacet<T>()}`); the old `is_enum_type` branch in `MakeType` is gone, so an enum is no longer a special case in the assembly.

Detection starts with concrete std partial specializations (`std::vector`, `std::array`, `T[N]`, `std::basic_string`, `std::basic_string_view`). A duck-typed `SequenceContainerShape` concept as a constrained default is the documented growth path once a real out-of-std container needs it; until then, third-party containers extend by explicit specialization. When constrained specializations do accumulate, keep the shape concepts mutually exclusive by construction (note `std::basic_string` matches any naive sequence shape; its explicit specialization wins by language rules, but subsumption ordering should be deliberate).

Traits over annotations for extension, for two reasons: annotation values must be structural and cannot carry thunks, and annotations attach at the type's definition, while the prime extension case is adapting a third-party container whose definition cannot be touched.

### Superseding the structural view

The rule "facets supersede the raw view" applies on both sides:

- **Builder-side:** a type whose trait provides a *superseding* facet gets empty field and function spans. This is what stops recursive reflection at protocol boundaries: reflecting `std::vector<T>` emits its `TypeInfo`, its `SequenceFacet`, and recursion into `T`, and nothing else. `TypeTraits` (size, alignment, predicates) is still filled unconditionally; those are real facts either way.
- **Consumer-side:** consumers check facets first (string, enumeration, sequence, later map), and fall back to the field walk only when no superseding facet is present.

Superseding is a property of the facet kind, not of the mechanism, and it is declared *on the facet*, not decided by the assembly: a facet class that sets `static constexpr bool Supersedes = true` empties the structural view. `FacetsBuilder` reads it generically (`requires { Facet::Supersedes; }`, false when absent) and folds it across the tuple, so it never enumerates which kinds supersede. `StringFacet`, `SequenceFacet`, and `EnumerationFacet` declare it; a facet that adds information alongside the fields (the future provenance facet) omits the member and coexists with the structural view.

`TypeInfoTraits::IsLeaf` is this rule in embryo and is **replaced**, not kept alongside (old approaches are removed when a better one exists; two truths that must agree is drift by design). `FieldsBuilder` / `FunctionsBuilder` consume a builder-side "provides a superseding facet" predicate instead. `Stringify` stays where it is: it is a universal type-scoped op (every type has one, with the defaults fallback chain), and universal ops do not belong in an optional-presence mechanism. `TypeInfoTraits` survives as the single customization surface providing both `Stringify` and facets.

One future escape hatch, noted but not built: a type that wants to be atomic with no protocol at all (opaque). No case exists today; `TypeKind` plus annotations cover the known ones.

### First facets and their thunk surfaces

Shipped in this order, each proving the next layer:

**1. `EnumerationFacet` (formerly `EnumerationInfo`), migrated.** Zero new semantics; proves the storage. The class itself is unchanged, only where its address is stored.

**2. `StringFacet`.** What lets serializers stop special-casing strings. An encapsulated class: the thunks are private, reached through checked methods, like `FieldInfo` and `EnumerationFacet` (not a naked struct of function pointers, which would expose the raw thunk layer the rest of the reflection surface keeps private).

```cpp
export class StringFacet
{
public:
    static constexpr bool Supersedes = true;
    constexpr StringFacet(ViewThunk view, AssignThunk assign);

    std::string_view View(const void* obj) const;            // read half, always present
    bool CanAssign() const;
    std::expected<void, FacetError> Assign(void* obj, std::string_view) const;  // NotWritable if absent
private:
    ViewThunk _view = nullptr;
    AssignThunk _assign = nullptr;
};
```

`std::string` provides both thunks; `std::string_view` provides the view with the assign thunk null, and `Assign` then reports `NotWritable`. Nullable thunks encoding capability is the existing house style (`FieldInfo`'s nullable getter/setter/referencer); the class surfaces it as `CanAssign()` plus an `expected`-returning `Assign`, so no consumer touches a raw pointer.

**3. `SequenceFacet`** for `std::vector`, `std::array`, C arrays (`TypeKind::Array`), and `std::span` (both extents), likewise an encapsulated class.

```cpp
export class SequenceFacet
{
public:
    static constexpr bool Supersedes = true;
    constexpr SequenceFacet(TypeReference element, SizeThunk, ElementRefThunk, ConstElementRefThunk,
                            ClearThunk, ReserveThunk, AppendThunk);

    const TypeInfo& ElementType() const;
    std::size_t Size(const void* obj) const;
    TypedRef ElementRef(void* obj, std::size_t) const;          // overloaded on const-ness of the
    TypedRef ElementRef(const void* obj, std::size_t) const;    // object, like FieldInfo::GetRef

    bool CanMutateElements() const;                             // false for a read-only view
    bool CanClear() const;  bool CanReserve() const;  bool CanAppend() const;
    std::expected<void, FacetError> Clear(void* obj) const;                     // NotWritable if fixed-size
    std::expected<void, FacetError> Reserve(void* obj, std::size_t) const;
    std::expected<void, FacetError> Append(void* obj, const TypedRef&) const;
private:
    TypeReference _element;
    // size / element-ref (mutable + const, mutable nullable) / clear / reserve / append thunks
};
```

- The two `ElementRef` overloads pick the mutable or const element by the const-ness of the object pointer, so `ObjectToString` (holding a `const void*`) reaches the const path with no explicit selector, the same overload pattern as `FieldInfo::GetRef`.
- **Index-based random access, not cursors.** Serialization needs ordered traversal, replication delta encoding needs random access, and the std deny-list makes non-random-access containers rare in reflected data. The contract documents expected O(1) access. An erased-cursor protocol is a later additive member if a real container demands it; erased cursor state is a genuinely hard problem not worth buying now.
- **Three capability tiers**, all reported through the `CanX` queries so a consumer discovers what a sequence supports rather than assuming:
  - *Resizable* (`std::vector`): clear / reserve / append present.
  - *Fixed-size* (`std::array`, `T[N]`, a mutable `std::span<T>`): clear / reserve / append null (report `NotWritable`); elements still mutable in place through `ElementRef`, so `CanMutateElements()` is true.
  - *Read-only view* (`std::span<const T>`): additionally the **mutable** element ref is absent, so `CanMutateElements()` is false and the mutable `ElementRef(void*)` overload is a precondition violation (assert); the const `ElementRef(const void*)` and `Size` are always present. The builder detects this from the const references `operator[]` yields, in one `MakeFixedSequenceFacet` helper shared by `std::array` and `std::span`.
- **Views are borrows, not inline data.** A `std::span` never owns its elements; the facet states only the in-place read/mutate capability, so a serializer treats a span as a reference (per the tree-of-ownership rule), not something to serialize inline. `span`'s element constness is independent of the span's own constness, which is why the read-only detection keys on the element reference type, not on how the span is spelled at the query site.
- `FacetError` mirrors the `FieldError` shape (struct wrapping an enum: `NotWritable`, `TypeMismatch`; exact cases settled at implementation).

### Mutation semantics

`Append` honors the `TypedRef` move protocol exactly as `FieldSetThunk` does: move when `Movable && !IsConst` and the element type is move-insertable, copy otherwise, `NotWritable` when neither works. The deserializer's rebuild loop is `Clear`, `Reserve`, then per element: construct into a stack slot (default construction or annotated factory), `Append` with `.Movable = true`. That covers move-only and non-default-constructible elements without an emplace protocol. Elements that are neither movable nor copyable cannot live in a facet-mutated sequence; accepted and documented.

True construct-in-place (container grows and hands back a `TypedRef` to raw storage for a factory to fill) is the shape the ECS chunk-construction path will eventually want, but its signature depends on the placement-agnostic construct op that the op-table section of ReflectionSystem.md explicitly defers. Facet structs are in-process metadata with no cross-build ABI, so adding it later is a cheap additive member. Not designed now.

### Invariants for the C# boundary and future providers

Facets are part of the unified `TypeInfo` contract ("one table shape, three fillers"). Three invariants keep runtime providers able to fill them:

1. Facet classes are plain data plus function pointers behind a `constexpr` constructor: `constexpr`-constructible but never consteval-bound, so a managed provider can construct one at runtime with thunks pointing into managed shims. (Encapsulation is only method sugar over that layout; the stored form is still a POD-shaped record.)
2. Thunk signatures stay on the `TypedRef` ABI (blittable `{const TypeInfo*, void*, bool, bool}`), so the same entry points serve the C#-driven editor's erased access path.
3. No templates and no `std::meta` in the stored form; `Element` is a `TypeReference`, template-ness lives only in the builder. The binding requirement here is the runtime-provider one (use case 8); it incidentally keeps facets populatable by a pre-build generator if one is ever needed.

For the binding generator, facets are the structural input for marshaling-shape derivation (`SequenceFacet` says "crosses the boundary as a sequence of Element"), the same way handle/blittable/opaque is derived structurally. The generator reads `Element` at build time and calls no thunks initially.

## Implementation plan

0. **Validation gate first.** The one identified GCC 16 risk: consteval assembly of a variable-size facet table (branch-dependent entry count, `static constexpr` locals inside a consteval-adjacent function template, addresses escaping into constant-initialized data). Validate this exact pattern in the `scratch` test case or `PlaygroundReflection/` before building on it. Fallback if it misbehaves: a per-type `static constexpr` named aggregate holding the facet instances plus the entry array (uglier, mechanically simpler for the compiler).
1. **Mechanism plus enum migration.** Add `FacetEntry`, the span member, `GetFacet` / `GetFacets` to `TypeInfo`; new `:Facets` partition for the facet structs and `FacetError`; `MakeType` builds the entry table; `GetEnumeration()` becomes a forwarder. Existing enum tests must pass unchanged.
2. **`StringFacet`.** Trait hooks on `basic_string` / `basic_string_view`, `:FacetsBuilder` detection, builder-side suppression predicate replacing `IsLeaf` (delete `IsLeaf`; update `FieldsBuilder` / `FunctionsBuilder`).
3. **`SequenceFacet`.** Trait hooks for `vector` / `array` / `T[N]`, thunk generation, `TypedRef` move protocol on `Append`.
4. **Consumer proof.** `ObjectToString` adopts the facet-first rule: strings via `StringFacet`, sequences rendered element-wise via `SequenceFacet`, fallback to the field walk. This is the in-tree consumer that validates the convention before a serializer exists.
5. **Tests** (promote from scratch): enum behavior unchanged post-migration; string view/assign through the facet; vector erased read walk; rebuild via clear/reserve/append; move-only element append; `std::array` and `T[N]` with null mutation thunks but mutable element refs; nested `vector<vector<int>>`; reflecting a std container no longer emits structural fields (the recursion firewall); `ToString` of a container.
6. New module files land in `PlaygroundEngine/CMakeLists.txt` under `FILE_SET CXX_MODULES`. As generalized, each facet is a class/builder pair: `Facets.cppm` (shared `FacetEntry` / `FacetError` vocabulary) and `Builder/FacetsBuilder.cppm` (the kind-agnostic assembly), plus `StringFacet.cppm` / `SequenceFacet.cppm` / `EnumerationFacet.cppm` and their `Builder/*FacetBuilder.cppm` counterparts.

## Risks

- **Consteval facet-table assembly** (step 0 above): the pattern is squarely in known GCC 16 quirk territory; validate before designing details on top.
- **Key laziness discipline:** any future convenience that resolves a facet key at build time (a `GetFacet` overload taking `const TypeInfo&`, an eager cache) reintroduces the construction cycle. The invariant is stated on `FacetEntry` the way `TypeReference` states its own.
- **Constrained-specialization ambiguity:** as constrained `TypeInfoTraits` specializations accumulate, two non-ordered constraints fail the build with an ambiguity. Good failure mode, but anticipate it: keep shape concepts mutually exclusive by construction.
- **Per-instantiation cost:** every reflected `vector<T>` emits a `TypeInfo`, a facet, and thunks. Inherent and acceptable; field suppression is the main mitigation. Watch once real asset types multiply.

## Out of scope (first implementation)

- **`AssociativeFacet`.** Deferred not only because reflected maps do not exist yet: unordered iteration order is nondeterministic across runs, so mergeable text assets and delta encoding need serializer-side key ordering, which wants an erased ordering the facet cannot cheaply provide. Solve when a real reflected map exists. When it comes: `InsertOrAssign(void*, key TypedRef, value TypedRef)` under the same move protocol.
- `OptionalFacet`, `HandleFacet` / `AssetRef` facets (the vocabulary types do not exist yet).
- Erased-cursor iteration; construct-in-place emplace (gated on the op-table / ECS storage design).
- Any C# surface (the generator does not exist; facets only need to not block it, per the three invariants above).
- The opaque escape hatch.
- Any pre-build generator consideration. Per the Constraint 1 posture in [ReflectionSystem.md](ReflectionSystem.md), a generator is an option preserved by the no-`std::meta`-in-stored-form rule, not a requirement; the rule itself is owed to runtime providers.

## Documentation follow-ups (when this lands)

In [ReflectionSystem.md](ReflectionSystem.md): add facets to the erased op-table section (facet thunks are op-table leaves, same as stringify/invoke); point the provenance-facet line at this mechanism; mark the converging-requirements row "Template instantiation introspection (containers + vocabulary types)" as satisfied by facets. In [CoreConventions.md](CoreConventions.md): cross-reference the vocabulary table to `HandleFacet` / `AssetRef` facets when those ship.