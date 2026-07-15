# Reflection Extraction, Implementation

What the reflection system pulls out of `std::meta`, where it is stored, and by what mechanism. Scope: the metadata surface only. It says nothing about identifiers, wire formats, or migration; those belong to the consumers (see the reasoning in [ReflectionSystem.md](ReflectionSystem.md)).

**Status:** implemented. Every fact below was re-validated against **GCC 17.0.0** (2026-07-14) after the
compiler upgrade, and all of them still hold; the Part 3 table is unchanged. The implementation knowledge (the
guards, the cv-node rule, the value/address split) lives in [ReflectionInternals.md](ReflectionInternals.md);
this document keeps the reasoning about *what* to capture and *why*.

Two things the analysis did not foresee, both recorded where they bite in
[ReflectionInternals.md](ReflectionInternals.md):

- **`parent_of` throws** for an entity with no parent (a fundamental type, `void`, a pointer), so the scope
  walk is guarded by `has_parent`. Likewise `template_of` throws for a non-instance.
- **A cv-qualified class still reports `is_class_type == true` and enumerates its members**, so a `const Foo`
  node had to be excluded from the member walk explicitly, or it would duplicate `Foo`'s whole structure under
  a second identity. Reaching `const Foo` at all is new: it arrives through pointer decomposition.
- **Some member functions have no address**, which forced the invoke path to take one only where access
  control demands it. See the invocation note in [ReflectionInternals.md](ReflectionInternals.md).

## Why this document exists

Reflection's job is to express the language and the type structure, nothing more. Consumers (serialization, replication, the C# generator, the editor) derive their own identifiers, and restrict their own scope, from what is exposed here. That split has a hard consequence: **a consumer cannot recover what reflection did not store.** Every `std::meta` query runs at `consteval`; a fact not captured at build time is gone for good. So the only question this document answers is which facts to capture.

The corollary is that the work below is decidable now, independent of any identifier scheme. Nothing here commits a consumer to anything.

## Part 1: What is currently wrong or shallow

Findings against the current builders, each confirmed by a throwaway compile.

### Incorrect

- **A fundamental type has no identifier.** `has_identifier(^^int)` is `false`, so `IdentifierOf` returns `{}` and `TypeOf<int>().GetIdentifier()` is **empty**. The only place "int" appears is the display name, which `DeclarationInfo` documents as "diagnostic text, always present, never a key." So the most common types in the language currently have no structural name at all. Any consumer walking fields to build a name hits a hole on the first `int`.

- **Static member functions are indistinguishable from const member functions.** `FunctionsBuilder.cppm:221` computes `constCallable = is_const(f) || is_static_member(f)`, and that single bool is all `FunctionInfo` stores. `IsConst()` therefore returns `true` for a static, and there is no `IsStatic()`. The conflation is correct for the invoke path (a static ignores the object pointer, so it is callable on a const instance) but it is a lie as *metadata*: a C# generator projects a static method differently from a const one and cannot tell them apart.

- **The return type is decayed, but the invoke thunk is not.** `FunctionsBuilder.cppm:223` stores `remove_cvref(return_type_of(f))`, so a function returning `const std::string&` reports a return type of `std::string`. Meanwhile `InvokeThunkImpl` has a dedicated reference-return path that erases the referent as a pointer. Behavior and metadata disagree: `InvokeAs<const std::string&>` works, but nothing in the metadata tells a caller that it may.

- **Fields do not record access.** `AccessKind` exists, but only on `BaseInfo`. The builders walk members with `access_context::unchecked()`, so private and protected fields are reflected, and nothing marks them as such. A serializer walking `GetFields()` today cannot avoid a private member.

### Shallow

- **Bitfields have an offset but no width.** `FieldInfo` takes a `bitOffset`; `bit_size_of` is never called. `unsigned Bits : 3` reflects with no `3` anywhere. A bitfield cannot be serialized, delta-encoded, or layout-matched against a shader without its width.

- **Parameters are decayed to a bare type.** `ArgumentBinding.cppm:28` stores `remove_cvref(type_of(param))`. The const and reference qualifiers survive only as `BindsByMove` / `RequiresMutable`, which are *binder policy* derived from the language fact, not the fact itself. The sink pair `f(const std::string&)` / `f(std::string&&)` stores one identical parameter type twice.

- **There is no scope.** `DeclarationInfo` holds an unqualified identifier. Reflection therefore **cannot distinguish `A::Foo` from `B::Foo`**, which is a structural fact about the language, not an identifier concern.

- **A template instance carries no arguments.** `TypeTraits::IsTemplateInstance` is a bare bool. `template_of` and `template_arguments_of` are never called, so `Grid<int>` reflects as an anonymous class (`has_identifier` is `false` for every template instance) with no recoverable relationship to `Grid` or to `int`.

- **Static data members are not reflected.** Only `nonstatic_data_members_of` is walked.

### Correct, and worth recording as settled

- **The `dealias` calls are right, and they are load-bearing.** GCC 16 is *inconsistent* about aliases: `type_of` on a member or a parameter silently strips the alias, but `return_type_of` retains it. Concretely, for `using Health = int;`, a field declared `Health hp` reports `is_type_alias == false`, while a function returning `Health` reports `is_type_alias == true` and displays `Health {aka int}`. Without the `dealias` in `TypeOfMeta` and `TypeReferenceTo`, the return type of that function would resolve to a *different* `TypeInfo` than the field, splitting type identity on nothing but spelling. The dealias normalizes an inconsistency the compiler hands us.

  The consequence for consumers is worth stating plainly: **the alias spelling cannot be exposed, because it is not uniformly available.** Presenting "Health" for a return type and "int" for a field of the same type would be worse than presenting "int" everywhere.

## Part 2: What to extract

Mechanism is uniform and already established, so it is stated once rather than per row: a `consteval` helper in a `Builders/` partition queries `std::meta`, results are frozen with `std::define_static_string` / `std::define_static_array`, and the metadata object stores a `string_view` or `span` into that static storage. Nothing allocates at runtime.

### `DeclarationInfo`

| Information | Query | Field |
|---|---|---|
| Scope path | `parent_of` walk until `!has_identifier` | `span<const string_view> _scopePath` |

The walk terminates on the global namespace, which has no identifier. Store the **chain** (`["PgE", "Game"]`), not a rendered string, so the separator stays the consumer's choice. Note the walk crosses inline namespaces: `std::string` yields `std.__cxx11.basic_string`. That is the correct structural answer and the reason a consumer must not build a durable identifier from a `std` type's path.

### `TypeInfo` / `TypeTraits`

| Information | Query | Field |
|---|---|---|
| Linkage | `has_external_linkage` / `has_module_linkage` / `has_internal_linkage` | `TypeTraits::Linkage` enum |
| Primary template | `template_of` | `const TemplateInfo* _template` |
| Template arguments | `template_arguments_of` | `span<const TemplateArgumentInfo>` |
| Final | `is_final` | `TypeTraits::IsFinal` |

`TemplateInfo : DeclarationInfo` carries only an identifier and a scope path. A template is not a type, so it cannot be a `TypeReference`; naming it is all that is possible and all that is needed.

A template argument has **three** kinds, not two. `is_type` and `is_value` are both false for a template template argument (`Holder<Grid, Foo>`), which is neither:

```cpp
enum class TemplateArgumentKind : std::uint8_t { Type, Value, Template };

struct TemplateArgumentInfo
{
    TemplateArgumentKind Kind = TemplateArgumentKind::Type;
    TypeReference Type;            // the type argument, or the non-type argument's type
    const void* Value = nullptr;   // set only for Value
    const TemplateInfo* Template = nullptr;  // set only for Template
};
```

**Defaulted template arguments are materialized and cannot be identified as defaults.** `Stream<Foo>` where `template <typename T, typename Policy = DefaultPolicy> struct Stream` reflects with **two** arguments; no query distinguishes the written from the defaulted one. This is correct as far as it goes, since `Stream<Foo>` and `Stream<Foo, DefaultPolicy>` are the same type and must reflect identically. It matters only to a consumer building a durable identifier, which is where the consequence belongs: adding a defaulted parameter to a template later changes its rendering.

Partial specializations report the **primary** template (`Spec<int*>` yields `template_of == Spec`), which is the wanted behavior. Alias templates dealias away entirely (`Ptr<Foo>` yields `Foo*`), consistent with the alias finding above.

### Compound types

`template_of` alone does not bottom out. Pointers, references, arrays, and fundamental types have **no identifier**, so recursion through a template's arguments terminates on entities that cannot be named. Decomposing them is a prerequisite for expressing a field's type at all, not an extra:

| Shape | Query | Decomposition |
|---|---|---|
| Const | `is_const` | `remove_const` |
| Pointer | `is_pointer_type` | `remove_pointer` |
| Reference | `is_lvalue_reference_type` / `is_rvalue_reference_type` | `remove_reference` |
| Array | `is_array_type` | `remove_extent` plus `extent` |

Fundamental types are named from **structure, not spelling**: `TypeKind` plus `size_of` plus `is_signed_type` gives `int32`, `uint64`, `float64`. That is what `has_identifier(^^int) == false` is telling us, and it is a better answer than a spelling would be: it is stdlib-neutral, and a non-native provider can supply the same description without a C++ name.

**The `char` caveat.** Size and signedness alone collapse `char`, `signed char`, and `int8_t`, which are distinct types (`char` is the string element type). Any structural naming of fundamentals must special-case `char` rather than deriving it, or `StringFacet` and a byte buffer become indistinguishable.

The result worth stating: a field's type is **fully identifiable with no spelling anywhere**. Verified end to end, from the field:

```
PgE::Foo**             -> pointer<pointer<PgE.Foo>>
const PgE::Foo*        -> pointer<const<PgE.Foo>>
PgE::Foo[4]            -> array<PgE.Foo, 4>
PgE::Grid<PgE::Foo*>   -> PgE.Grid<pointer<PgE.Foo>>
unsigned long long     -> uint64
std::vector<PgE::Foo>  -> std.vector<PgE.Foo, std.allocator<PgE.Foo>>
```

Linkage is what lets a consumer **refuse**. A module-linkage or internal-linkage type has no cross-translation-unit identity by construction, so persisting a reference to one is a category error, and only reflection can report that.

### `FieldInfo`

| Information | Query | Field |
|---|---|---|
| Bit width | `bit_size_of` (guard with `is_bit_field`) | `int _bitSize` |
| Access | `is_public` / `is_protected` | `AccessKind _access` (the existing enum) |
| Has default initializer | `has_default_member_initializer` | `bool _hasDefaultInitializer` |
| Mutable | `is_mutable_member` | `bool _isMutable` |
`has_default_member_initializer` is the field that lets a serializer omit a defaulted value from a text asset, which is exactly what makes assets small and mergeable.

### `StaticFieldInfo`

Static data members get their own metadata type and their own span on `TypeInfo`, not an entry in `_fields`: they have no offset and no instance, so every accessor signature differs (no object pointer).

| Information | Query | Field |
|---|---|---|
| Enumeration | `static_data_members_of` | `span<const StaticFieldInfo> _staticFields` |
| Type | `type_of` | `TypeReference _typeInfo` |
| Access | `is_public` / `is_protected` | `AccessKind _access` |

**The address of a static data member cannot be taken unconditionally.** This is a link-time trap, verified:

```cpp
struct S { static const int NoDefinition = 5; };   // in-class init, no out-of-line definition
const void* p = &[:member:];                       // undefined reference to `S::NoDefinition'
```

A `static const` member of integral or enumeration type may be initialized in-class and never defined out of line, which is legal and is exactly the editor-helper pattern (`static const int MaxSlots = 8;`). Taking its address is an odr-use that fails at **link** time. `requires` does not save us: forming the address is well-formed during substitution, so nothing at compile time detects it.

The rule that resolves it, and the reason it is not a workaround:

- **Constant-readable members are captured by value at `consteval`** and never odr-used. The thunk holds the value, not a pointer.
- **Everything else takes an address**, which is safe because any static member that is not constant-readable is required to have a definition anyway.

This maps exactly onto the capability, which is why it is the right shape rather than a dodge: a constant-readable static is **read-only** (a value), and an addressable one is **settable** (a reference). The split is the semantics, not the workaround. Detection is a `requires` on the constant read (`constexpr auto v = [:member:];`), which *is* a substitution failure and therefore detectable, unlike the address case.

### `FunctionInfo`

| Information | Query | Field |
|---|---|---|
| Static | `is_static_member` | `bool _isStatic`, **split from** `_constCallable` |
| Const | `is_const` | `bool _isConst` |
| Noexcept | `is_noexcept` | `bool _isNoexcept` |
| Virtual / pure / override | `is_virtual` / `is_pure_virtual` / `is_override` | three bools, or a small enum |
| Deleted | `is_deleted` | `bool _isDeleted` |
| Reference qualifier | `is_lvalue_reference_qualified` / `is_rvalue_reference_qualified` | a `RefQualifier` enum |
| Access | `is_public` / `is_protected` | `AccessKind _access` |

Splitting static from const is the one change here that alters existing behavior. The invoke path still wants "callable on a const object", which becomes a derived query (`IsConst() || IsStatic()`) rather than a stored bool.

The reference qualifier is worth capturing because it is already load-bearing and invisible: `MakeInvoker` returns `nullptr` for an rvalue-reference-qualified function, so such a function reflects with no thunk and the metadata gives no reason why. Storing the qualifier turns a silent absence into a stated fact.

`is_noexcept` and `is_virtual` are both C# projection inputs (error model translation, virtual dispatch).

### `ParameterInfo`

| Information | Query | Field |
|---|---|---|
| Has default argument | `has_default_argument` | `bool _hasDefaultArgument` |
| Qualifiers | `is_const` / `is_lvalue_reference_type` / `is_rvalue_reference_type` on the undecayed `type_of` | see below |

`has_default_argument` is a hard blocker for both the C# generator (which must emit the same optional parameter) and visual scripting (which must let a node omit the pin). It is currently unreachable.

The qualifier question was the deepest change proposed here. Everywhere the language spells a **qualified** type, the metadata stored a **bare** one and reconstructed a fragment of the difference as binder policy. Two options were considered:

1. **Qualifier flags beside the decayed type.** `ParameterInfo` gains `IsConst` / `IsLvalueReference` / `IsRvalueReference` as language facts, and `BindsByMove` / `RequiresMutable` stay as the binder's derived policy. Small, additive, no identity implications.
2. **A `QualifiedTypeReference { TypeReference Type; Qualifiers Q; }`** used wherever a qualified type is spelled: parameters, the function return, const data members. Expresses the language properly and in one place, but it is a shape change touching every builder.

**Option 1 was taken:** it is the smaller step and does not preclude option 2. The same flags carry the function return's qualifiers, which is what makes `InvokeAs<const std::string&>`'s legality a stated fact rather than an undocumented behavior of the thunk. The reason not to store the undecayed type as a `TypeReference` directly is that it would require a `TypeInfo` for `const std::string&`, multiplying the type graph with entries that have no members, no size, and no purpose beyond spelling.

Note that the compound-type work does add a node for `const std::string` (reached by decomposing `const std::string*`), but that is a different thing: a cv node names a real object type a field can have, and carries no reference qualifier.

## Part 3: Validated GCC facts

Everything above rests on these, each from a throwaway compile rather than from the paper. Established on GCC
16.1.1 and re-confirmed unchanged on **GCC 17.0.0**, including the two link-time traps, which stayed traps.

| Fact | Result |
|---|---|
| `has_identifier(^^int)` | `false`; only `display_string_of` yields "int" |
| `has_identifier(^^std::vector<int>)` | `false`; no template instance has an identifier |
| `display_string_of(^^std::string)` | `std::string {aka std::__cxx11::basic_string<char>}`, diagnostic prose |
| `parent_of` scope walk | Works; yields `PgE.Game.Weapon`, and `std.__cxx11.basic_string` |
| `has_module_linkage` / `has_internal_linkage` / `has_external_linkage` | All three discriminate correctly |
| `template_of` / `template_arguments_of` | Work; `std::vector<int>` has **2** arguments (the allocator is exposed) |
| Non-type template arguments | Work; `FixedArray<Foo, 8>` gives `is_value == true`, `extract<int>` yields `8` |
| `bit_size_of`, `has_default_argument`, `has_default_member_initializer`, `is_mutable_member` | All work |
| `is_noexcept`, `is_deleted`, `is_defaulted`, `is_virtual`, `is_pure_virtual`, `is_override`, `is_final` | All work |
| `is_lvalue_reference_qualified` / `is_rvalue_reference_qualified` | Both work |
| `static_data_members_of` | Works; finds `static`, `static const`, `static constexpr`, `static inline` |
| Address of a `static const` with no out-of-line definition | **Fails to link.** Not a substitution failure, so `requires` cannot detect it |
| Constant read of that same member | Works at `consteval`; captures the value without an odr-use |
| `type_order` | Works; a deterministic total order on types |
| Recursive template rendering | Works; nested, variadic, and non-type arguments all resolve |
| Defaulted template arguments | **Materialized, and indistinguishable from written ones.** `Stream<Foo>` reflects with 2 arguments |
| Template template arguments | A **third** kind: `is_type` and `is_value` are both false |
| Partial specialization | `template_of` yields the primary template |
| Alias templates | Dealias away; `Ptr<Foo>` yields `Foo*` |
| Compound decomposition | `remove_pointer` / `remove_extent` / `extent` / `remove_const` all work |
| Fundamental naming | No identifier; `TypeKind` + `size_of` + `is_signed_type` yields `int32` / `uint64` / `float64` |
| Alias retention | **Inconsistent.** `type_of` strips it (member, parameter); `return_type_of` retains it |
| No `qualified_name_of` | Does not exist; the scope walk is the only route |
| No module-name query | Only the linkage predicates exist. Two module-linkage types sharing a qualified name **cannot** be disambiguated |
| No defaulted-template-argument query | Does not exist |

## Part 4: Deliberately not extracted

- **`source_location_of`.** Only the native provider could supply it, so it breaks the one-`TypeInfo`-contract rule, and it changes on every edit above the declaration.
- **The `<type_traits>` mirrors** (`is_nothrow_swappable_type` and its ~80 relatives). Reflection exposes what a consumer cannot compute itself; a consumer with the type can ask `std::` directly.
- **`define_aggregate` / `data_member_spec` / `substitute`.** Type *synthesis*, not extraction. A separate capability with no current use case.
- **`symbol_of`.** Operator spelling only (it takes an `operators` enum); relevant if operator reflection is ever added, which it is not today.
- **`type_order`.** Not extracted, but recorded because it is the mechanism for a deterministic registry ordering if ordinals are ever wanted on the wire.
