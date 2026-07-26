# Reflection Injection

Generating data members from reflected information, using a C++26 `consteval` block plus
`std::meta::define_aggregate`. Validated in `PlaygroundReflection/src/member_injection.h` and
`member_accessors.h` on GCC 17.0.0 experimental (20260714, the `~/gcc-16` build).

**Status: explored and deliberately deferred.** Nothing here is in `PlaygroundEngine/src/Reflection/`. The
mechanism works and one tier of it is genuinely valuable, but the reflection core is not taking new surface
until it has real consumers (serialization, replication, the editor). This document exists so the findings
and the reasoning survive until then.

## Why this matters at all: no UHT

Unreal's `PostEditChangeProperty` matching is written as
`PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(AMyActor, Health)`. That macro can
check the name only because Unreal Header Tool already parsed the header and generated the reflection data
for it. UHT is a large external C++ program, a separate build step, a build-order dependency, and a parser
that has to keep chasing the language. `DOREPLIFETIME` for replication has the same prerequisite.

Here the language supplies the same guarantee with no tool, no codegen step, and no annotation macros, which
is the "where the language provides a mechanism, use it" principle in `CLAUDE.md` paying out on the largest
single piece of engine tooling Unreal maintains. The matching is also *better* than Unreal's: a reflected
field index is a constant expression, so it can be a `case` label, making property dispatch a jump table
rather than a chain of `FName` comparisons.

## The mechanism and its one hard rule

`define_aggregate` is the only code injection in C++26. Token sequences (P3294) did not land, so it can add
**non-static data members and nothing else**: never member functions, never static members, never base
classes. Per member you control four things: name, type, alignment, and bit width. There are also no default
member initializers.

The rule that shapes every use: `define_aggregate` must be evaluated from a `consteval` block, and **no
scope may intervene** between that block and the target class. GCC diagnoses violations as
`'X' intervenes between 'consteval' block 'define_aggregate' is evaluated from and 'Y' scope`.

The consequence is not obvious and cost real time to find. Declaring the target as a **nested class of the
same class template** that holds the block satisfies the rule, so injection becomes automatic per `T` with
no registration site anywhere:

```cpp
template <typename T>
class Fields
{
public:
    struct Storage;                  // nested: shares the block's scope
    consteval { /* ... */ std::meta::define_aggregate(^^Storage, memberSpecs); }
};
```

What does **not** work, each confirmed by compile:

- A block inside a class template or function template completing a class declared at *namespace* scope.
  That scope intervenes.
- The P2996 paper idiom `template <class T> using Foo = [: MakeFoo<T>() :];`, which fails with
  `'define_aggregate' not evaluated from 'consteval' block`. The final wording is stricter than the paper.
- A namespace-scope block completing a namespace-scope class does work, but needs one explicit registration
  line per type, so the nested form is strictly better.

## Filling the generated members

Injected members are positional and cannot carry default initializers, so a constructor body has no way to
name them one at a time. A `static consteval` factory pack-expanding an `index_sequence` into a single
aggregate initializer does, and the result binds to a `static constexpr` member, so there is no instance and
no runtime cost. The factory must be declared *before* the member that calls it, since unqualified lookup in
a dependent class will not find it afterwards.

## Generating behaviour without injecting functions

The important realisation: injection cannot add a function, but an injected member's **type** can be a
template that carries behaviour, bound to one specific field by its reflection. `std::meta::info` is
structural, so it works as a non-type template parameter, and `std::meta::substitute` instantiates the
template at consteval time:

```cpp
template <std::meta::info MetaField>
struct FieldAccessor
{
    using Owner = [:std::meta::parent_of(MetaField):];
    using Type  = [:std::meta::type_of(MetaField):];
    static constexpr const Type& Get(const Owner& owner) { return owner.[:MetaField:]; }
};
```

The behaviour is written once and specialises per field. With `.no_unique_address = true` the accessors are
stateless, so a whole table costs one byte, and `Get` returns `const int&` rather than an erased handle.

This is the **non-erased twin of the existing op-table**. Compare the two paths to the same field:

```cpp
FieldGetter = std::expected<void, FieldError> (*)(const void*, const TypedRef&);  // erased tier
static constexpr const int& Get(const Player&);                                   // typed tier
```

The erased tier stays mandatory for foreign callers (C#, UGC, Wasm) per the caller-agnostic dispatch
decision. The typed tier is for native call sites, where erasure buys nothing and costs inlining.

### It is demand-gated more finely than `TypeOf`

Generating accessors for **every** field, including a `[[deprecated]]` one, compiles clean under the
engine's `-Werror`, as long as nothing calls them. Calling one fails at the *call site* with the deprecation
named. So this tier is demand-gated at function-body granularity, finer than the per-type `TypeOf` /
`TypeIdOf` split built for exactly that problem: ill-formed and deprecated members cost nothing until
touched, and when they break they blame the right line instead of erupting inside a builder.

## Catalogue of transforms

Everything generatable is a permutation of a reflected member list: retype, filter, reorder, re-align,
bit-pack, or fan out. All of these are validated in the spike.

| Generated | Transform | Measured | Engine relevance |
| --- | --- | --- | --- |
| `Fields<T>` | field to `FieldData<Field>` | typed, named, zero-size | ad-hoc metadata access |
| `Accessors<T>` | field to `FieldAccessor<field>` | 1 byte for 3 accessors | typed access tier |
| `StructureOfArrays<T, N>` | field to `array<Field, N>` | works | ECS storage |
| `Patch<T>` | field to `optional<Field>` | per-field set/unset | network delta, editor multi-select |
| `DirtyFlags<T>` | field to 1-bit field | 3 fields in 1 byte | replication dirty mask |
| `Std140<T>` | force GPU alignment | native 32 vs std140 48 | upload layout oracle |
| `Compact<T>` | reorder by alignment | 32 to 16 bytes | storage packing |
| `ReplicatedFields<T>` | keep annotated only | 12 to 8 bytes | wire payload type |
| `PerEnumerator<E, Slot>` | member per enumerator | named slots | stat buckets |

Two are worth singling out.

**`Std140<T>` as an oracle, not a replacement.** Generate the layout the GPU expects, then `static_assert`
that the struct actually uploaded agrees with it field by field. That converts a silent CPU/GPU layout
mismatch, otherwise debuggable only by staring at garbage on screen, into a compile error, with no runtime
cost and no change to existing structs. The spike shows a naturally ordered uniform block failing the check
and a reordered one passing.

**`ReplicatedFields<T>` as a type-level guarantee.** The wire payload becomes a type that physically cannot
hold an unreplicated field, rather than a runtime filter that has to be trusted.

`Compact<T>` and `StructureOfArrays<T, N>` have the best numbers and the worst fit: game code writes `T`, so
they only help where the *storage* is generated and all access routes through reflection. That is an
ECS-sized commitment, and reordering silently invalidates any offset arithmetic or `memcpy` between the two
forms.

## Traps, both of the same shape

**Identity against the canonical field list.** `TypeBuilder.cppm` holds `static constexpr auto Fields` per
type and `TypeInfo::GetFields()` returns a span over it, so those `FieldInfo` objects have stable, unique
addresses. A matcher must therefore inject *indices or pointers into that array*, never copies of
`FieldInfo`. Injecting copies makes `&changed == &Fields<Player>::Of.health` fail forever, and it fails at
runtime rather than at compile time.

**Superseding facets.** `MakeFieldsFromType` returns an empty array when `ProvidesSupersedingFacet<T>()`, so
"the fields of `T`" is already not "T's non-static data members". Any generated tier that walks
`nonstatic_data_members_of` directly will silently disagree with `GetFields()` for every type carrying a
string or sequence facet: the erased path reports no fields while the typed path generates accessors into
the type's guts. Whatever gets built must derive from the same filtered list the builders use.

Both are the same failure: two sources of truth for what a type's fields are, diverging silently.

## The ceiling

Injected names must be **unique valid identifiers**. That is the hard limit on the whole technique:

- Per-field generation is fine. Per-function generation breaks on the first overload pair, and on operators,
  whose names are not identifiers.
- Flattening inherited fields collides when two bases declare the same field name.
  `nonstatic_data_members_of` returns only direct members, so `Fields<Derived>` for a `Derived` that adds no
  fields of its own is legitimately empty.
- Injection is not idempotent for a namespace-scope target: completing the same class twice in one
  translation unit is a hard error. The nested-in-template form sidesteps this, since each specialization is
  completed once.

Module notes: the nested form is CMI-safe, and two separate consumer translation units may instantiate the
same specialization and agree. One trap is that a consumer instantiating an injection helper defined in
another module must itself `import std;`, or GCC fails with `couldn't look up 'std::vector'` inside the
helper, leaves the class incomplete, and then misreports it as `too many initializers`.

## What to do when a consumer appears

Ranked by value, given a real consumer:

1. **`FieldAccessor` / the typed tier.** The only part that is not sugar. Build it from
   `MakeFieldsFromType`'s member list, not raw reflection, so the tiers cannot diverge. Add `Get`/`Set`
   only, and leave `Serialize`/`Replicate` until the serializer or replication layer actually asks.
2. **The `Std140<T>` oracle.** Self-contained, immediately useful to the Vulkan work, and it commits the
   reflection system to nothing.
3. **Property matching.** Prefer the injection-free form, `FieldIndexOf<^^T::Field>()`: it is a few lines,
   equally compile-time checked, works on private and overloaded names, and has no name-uniqueness ceiling.
   What injection adds over it is autocomplete, not safety. Add the injected `FieldIndex<T>::Of` variant only
   if the matching surface turns out to be game-facing, where discoverability earns its keep. Note that a
   positional index is not a stable wire id: inserting or reordering a field changes the meaning of every
   previously serialized payload, which is why Unreal pays for property checksums. A wire id needs to be
   stable by construction.
