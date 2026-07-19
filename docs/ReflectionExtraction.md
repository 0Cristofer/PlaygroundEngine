# Reflection Extraction, Implementation

What the reflection system pulls out of `std::meta`, where it is stored, and by what mechanism. Scope: the metadata surface only. It says nothing about identifiers, wire formats, or migration; those belong to the consumers (see the reasoning in [ReflectionSystem.md](ReflectionSystem.md)).

**Status:** implemented. Every fact below was re-validated against **GCC 17.0.0** (2026-07-14) after the
compiler upgrade, and all of them still hold. Extended 2026-07-18 with access on namespace-scope entities,
function-type signatures, member-pointer decomposition, namespace-scope extraction, and the namespace sweep
(`NamespaceOf`, which reverses this document's earlier rejection of one); the Part 3 table grew accordingly. The implementation knowledge (the guards, the cv-node rule, the value/address split) lives
in [ReflectionInternals.md](ReflectionInternals.md); this document keeps the reasoning about *what* to
capture and *why*.

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

- **A deducing-this member reflects a lie.** `parameters_of` counts the explicit object parameter, so `int Get(this const Self&, int)` reflects with arity 2, `IsStatic() == false`, and no invoker. Every fact is wrong at once: the caller supplies one argument, the const-ness is on the object parameter (not the function), and the call is genuinely possible. A C# generator reading arity 2 emits a two-argument method.

### Shallow

- **Bitfields have an offset but no width.** `FieldInfo` takes a `bitOffset`; `bit_size_of` is never called. `unsigned Bits : 3` reflects with no `3` anywhere. A bitfield cannot be serialized, delta-encoded, or layout-matched against a shader without its width.

- **Parameters are decayed to a bare type.** `ArgumentBinding.cppm:28` stores `remove_cvref(type_of(param))`. The const and reference qualifiers survive only as `BindsByMove` / `RequiresMutable`, which are *binder policy* derived from the language fact, not the fact itself. The sink pair `f(const std::string&)` / `f(std::string&&)` stores one identical parameter type twice.

- **Fields are decayed the same way, and it costs more here.** `FieldInfo` stores `remove_cvref(type_of(member))`, so `int Target`, `int& Alias`, and `const int& ConstAlias` share one tag with nothing to separate them, and unlike a parameter a field got no `BindsByMove` reconstruction at all. This is the tree-of-ownership distinction: an owned value serializes inline, a reference member is a cross-reference that must not, and `GetFields()` on the decayed tag cannot tell them apart.

- **Operators and conversions are mishandled in opposite directions.** A conversion function (`operator int()`) has no identifier, so it lands in `GetFunctions()` nameless; an operator (`operator==`) is filtered out of `GetFunctions()` and reflected nowhere. One leaks in unusable, the other is unreachable.

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
| Qualifiers | `is_const` / `is_volatile` / `is_lvalue_reference_type` / `is_rvalue_reference_type` on the undecayed `type_of` | `IsConst` / `IsVolatile` / `IsLvalueReference` / `IsRvalueReference` |
`has_default_member_initializer` is the field that lets a serializer omit a defaulted value from a text asset, which is exactly what makes assets small and mergeable.

`IsVolatile` completes the cv pair, so a memory-mapped register member is not collapsed into its unqualified tag. It also carried two latent build breaks, both from asking a question of the **decayed** type that only the member itself can answer:

- The borrow thunk cast the member's address through `const void*`, which is ill-formed from a `volatile int*`, so any type with a volatile member failed to compile. The cast now goes through `const volatile void*` (the erased `Data` is still a plain `void*`, with constness kept on `IsConst`).
- The getter and setter guards asked `is_copy_constructible_v` / `is_copy_assignable_v` of the decayed type, which is `true` for a `volatile S` member whose class type `S` is copyable. No copy constructor binds a volatile lvalue and no implicit `operator=` takes one, so the emitted thunk bodies were ill-formed. Both guards now **mirror the thunk** with a `requires` on the actual member expression, the way `IsInvocable` mirrors `DoCall`, and the setter thunk branches on the same two predicates so guard and body cannot disagree. A volatile class-typed member reflects with no getter and no setter (its borrow survives, since taking an address is unaffected by volatility); a volatile **scalar** keeps both.

The qualifier flags are the same option-1 shape taken for `ParameterInfo` below, and for the same reason: the stored `TypeReference` is decayed, so `int Target`, `int& Alias`, and `const int& ConstAlias` share one type tag. This is the asymmetry that mattered most, because it is what tells the **tree of ownership** apart: `Target` is owned and serialized inline, `Alias` is a cross-reference that must never be, and a serializer walking `GetFields()` on the decayed tag alone cannot see the difference. `const int Constant` vs `int Variable` is likewise indistinguishable without it.

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
| Defaulted | `is_defaulted` | `bool IsDefaulted` |
| Consteval | specifier in `display_string_of` (no `is_consteval` on GCC 16) | `bool _isConsteval` |
| Reference qualifier | `is_lvalue_reference_qualified` / `is_rvalue_reference_qualified` | a `RefQualifier` enum |
| Explicit object parameter | `is_explicit_object_parameter` on the first parameter | `bool _hasExplicitObjectParameter` |
| Access | `is_public` / `is_protected` | `AccessKind _access` |

Splitting static from const is the one change here that alters existing behavior.

The explicit object parameter is the deducing-this (`this Self`) case, and left unhandled it is the same class of lie as static-vs-const. `parameters_of` counts the object parameter, so `int Get(this const Self&, int)` reflects with arity 2, `IsStatic() == false`, and no invoker (the arity mismatch fails the thunk). Every fact is wrong: the caller supplies one argument, the const-ness lives on the object parameter (`is_const` is false on the function), and it is genuinely callable. Dropping a leading `is_explicit_object_parameter` from the callable parameters, reading `IsConst`/`RefQual` off the object type, and stating `HasExplicitObjectParameter` makes the metadata honest; the invoke path handles it (see [ReflectionInternals.md](ReflectionInternals.md), the deducing-this note). The invoke path still wants "callable on a const object", which becomes a derived query (`IsConst() || IsStatic()`) rather than a stored bool.

The reference qualifier is worth capturing because it is already load-bearing and invisible: `MakeInvoker` returns `nullptr` for an rvalue-reference-qualified function, so such a function reflects with no thunk and the metadata gives no reason why. Storing the qualifier turns a silent absence into a stated fact.

**Consteval is a stated reason and a build-break trap.** A `consteval` member has no runtime invoker, so `IsConsteval` records why, the same as the reference qualifier. But unlike deleted (a hard error the requires-expression rejects) and rvalue-qualified (a substitution failure it rejects), an immediate call is *well-formed* in the requires-expression, so it passes `IsInvocable` and the emitted thunk then calls it at runtime, a hard error that breaks the build for the whole type. It must be excluded up front (`IsImmediateFunction`, shared with the constructor path), which is why it is not merely a projection nicety.

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

### `ConstructorInfo` (stated reason for no thunk)

A constructor with no runtime thunk collapsed four distinct reasons to one `CanConstruct() == false`: deleted, inaccessible, `consteval`, and unbindable were indistinguishable. This is the same defect the reference qualifier fixed for `FunctionInfo`, and the same fix: store the reasons that are language facts so a null thunk is a **stated** absence.

| Information | Query | Field |
|---|---|---|
| Deleted | `is_deleted` | `ConstructorTraits::IsDeleted` |
| Consteval | specifier in `display_string_of` (no `is_consteval` on GCC 16) | `ConstructorTraits::IsConsteval` |
| Defaulted | `is_defaulted` | `ConstructorTraits::IsDefaulted` |

With `Access` already telling apart an inaccessible constructor, a residual null thunk now means "unbindable" specifically. `MoveOnly`'s deleted copy constructor reports `IsDeleted`, not a silent absence; a `consteval` constructor reports `IsConsteval`.

`IsDefaulted` is the orthogonal axis: it separates a compiler-generated special member from a user-written one (`Point() = default` from `explicit Point(int)`), which is what tells a serializer that a copy is memberwise rather than custom. It is stored on `FunctionTraits` too, so it reaches operators through `OperatorInfo` and distinguishes a defaulted `operator==` from a hand-written one.

### `DestructorInfo`

A type has exactly one destructor, so it is a single `DestructorInfo` on `TypeInfo` (`GetDestructor()`) rather than a list. It owns the destroy thunk that `CanDestroy()` / `Destroy()` delegate to, so the runtime path and its metadata are one object instead of a bare function pointer sitting beside unrelated traits.

| Information | Query | Field |
|---|---|---|
| Enumeration | the `is_destructor` member of `members_of` (guarded by class/union) | `DestructorInfo _destructor` |
| Virtual / pure | `is_virtual` / `is_pure_virtual` | `DestructorTraits::IsVirtual` / `IsPureVirtual` |
| Deleted / defaulted | `is_deleted` / `is_defaulted` | `DestructorTraits::IsDeleted` / `IsDefaulted` |
| Access | `is_public` / `is_protected` | `DestructorTraits::Access` |
| Trivial | `is_trivially_destructible_type` (whole-type) | `DestructorTraits::IsTrivial` |

The per-declaration facts come from the destructor member; triviality is a whole-type answer. Every class has an implicit destructor member to read, so the split only matters for non-class types, which have no member and keep the defaults (a scalar is destroyable and trivially destructible, but nothing about it is "defaulted"). A deleted or inaccessible destructor, an incomplete type, an array, and `void` all reflect with `CanDestroy() == false`, with `IsDeleted` turning the common case into a stated reason.

### Nested types and member aliases

`GetNestedTypes()` reports the types a class declares inside itself (nested classes, enums, unions) and its member type-aliases. This is the reflexive structure, and it is independent of the fields: previously a nested type was reachable only if some field or return type happened to use it, so a nested type with no member of that type was invisible. The C# binding layer is the consumer, since it mirrors nested types as nested types.

| Information | Query | Field |
|---|---|---|
| Enumeration | `is_type` over `members_of`, requiring `has_identifier` | `span<const NestedTypeInfo> _nestedTypes` |
| Target type | `dealias` | `NestedTypeInfo::GetTypeInfo()` |
| Alias | `dealias(member) != member` | `NestedTypeInfo::IsAlias()` |
| Access | `is_public` / `is_protected` | `NestedTypeInfo::GetAccess()` |

The declaration carries the name and the reference carries the type, so `using ValueType = int` reports the name `ValueType` and resolves to `int`'s `TypeInfo`. A self-alias (`using SelfAlias = Outer` inside `Outer`) resolves back to the enclosing type and cannot recurse during construction, for the same reason a self-referential field cannot: the `TypeReference` is lazy.

**Anonymous nested types are excluded**, which is why the filter is not `is_type` alone. `struct { int a; } field;` contributes an unnamed type member (GCC reports it as `Outer::<unnamed struct>`, `has_identifier() == false`). Nothing can name it, a projection cannot mirror it, and it is already reachable through the field that uses it; including it would also put a nameless entry in the list that a lookup by empty identifier would match.

### Operators and conversions

Part 4 previously recorded operator reflection as out of scope. It is now in: an operator and a user-defined conversion are member functions the invoke machinery already handles, and leaving them out left two holes. A **conversion** (`operator int()`) has no identifier, so it sat in `GetFunctions()` nameless; an **operator** (`operator==`) was excluded from `GetFunctions()` and so was unreachable entirely. Both are the value-semantics surface a C# generator projects (`op_Equality`, an explicit conversion), so both are modelled rather than half-supported.

| Information | Query | Field |
|---|---|---|
| Operators | `is_operator_function` (excluding `is_special_member_function`) | `span<const OperatorInfo> _operators` |
| Operator kind | `operator_of` mapped to an engine enum | `OperatorInfo::GetOperator()` |
| Conversions | `is_conversion_function` | `span<const ConversionInfo> _conversions` |
| Conversion target | `return_type_of` (a conversion's return is its target) | `ConversionInfo::GetTargetType()` |
| Explicit | `is_explicit` | `ConversionInfo::IsExplicit()` |

Both derive from `FunctionInfo` and reuse its whole builder, so they invoke through the same thunk. Two decisions carry weight. `OperatorKind` is engine-owned, mapped from `std::meta::operators`, so the public API does not leak the toolchain spelling. And copy/move assignment are excluded even though they are operator functions: they are **special members** (the lifetime story, like the constructors), and, separately, building an invoker for an implicitly-declared assignment operator is a GCC 16 ICE (see [ReflectionInternals.md](ReflectionInternals.md), operators and conversions).

### Access, and the entities that have none

`AccessKind` gained a fourth value, `None`, because access is a **class-member notion** and the enum had no way to say "does not apply". A namespace-scope function answers `false` to `is_public`, `is_protected` and `is_private` alike, so the old fallthrough reported every free function as `Private`. That is the same class of lie as the static-vs-const conflation in Part 1: a serializer or C# generator filtering on `Public` would silently skip the entire namespace-scope surface.

The predicate is not simply `is_class_member`. A **base specifier is not a class member** but does have access, and a private base answers `false` to both positive predicates, so it would have collapsed to `None` and taken every reflected base with it. `AccessOf` therefore answers the positive predicates first and falls through to `Private` only for an entity that is a class member **or** a base.

### Function types and member pointers

`TypeKind::Function` was assigned and then nothing was stored, so a walk that reached a function type bottomed out on a node that knew it was a function and could not say which one. That node is reached from four directions, which is why the fix belongs on the type rather than on `FieldInfo`: a field of function-pointer type, a parameter of function-pointer type, a return type, and a **template argument** (`std::function<void(int)>` reaches it with no pointer involved at all).

Both follow the `TemplateInfo` precedent already in `TypeInfo`: a nullable pointer to kind-specific data, null for the kinds it does not apply to. They are not facets. A facet is a *semantic* view (container, string, enum) carrying a `Supersedes` flag that replaces the raw structural view; a signature is plain language structure, and it belongs beside the compound decomposition in the core.

| Information | Query | Field |
|---|---|---|
| Signature | `return_type_of` / `parameters_of` on the function **type** | `const FunctionSignatureInfo* _signature` |
| Noexcept | `is_noexcept` (works on a function type) | `FunctionSignatureInfo::IsNoexcept()` |
| Variadic | partial specialization; no `std::meta` query exists | `FunctionSignatureInfo::IsVariadic()` |
| Member pointer halves | partial specialization; no `remove_member_pointer` exists | `const MemberPointerInfo* _memberPointer` |

**`parameters_of` means two different things.** On a function *declaration* it yields parameter declarations, which `type_of` must be applied to. On a function *type* it yields the parameter **types directly** (`is_type` is `true`), and asking `type_of` for them throws `reflection does not have a type`. A requires-expression does **not** catch that, for the same reason it does not catch an immediate call: the throwing `consteval` call is well-formed in an unevaluated context. This cost a build and is exactly the trap `IsImmediateFunction` documents.

The two decompositions **compose** rather than duplicating: `int (Foo::*)(double) const` yields a `MemberPointerInfo` of `Foo` plus the function type `int(double) const`, and that pointee carries its own signature. Note that `remove_pointer` does **not** peel a member pointer (it returns the type unchanged), which is why this is a separate decomposition from the single inner-type chain rather than another branch in it.

`IsVariadic` is detected structurally, since `std::meta` has no `is_variadic` to ask. Only the ellipsis forms are specialized; everything else falls to the primary template and reports `false`. Cv and ref qualifiers are independent of the ellipsis, so an **abominable function type** (`int(double) const`, reachable as a member-function-pointer pointee) needs its own specialization: all twelve cv/ref combinations are listed, doubled for `noexcept`.

### Namespace-scope entities

Free functions and namespace-scope variables are reflected by **being named**, exactly as `TypeOf` names a type:

```cpp
PgE::detail::FunctionOfMeta<^^Game::Spawn>()     // -> const FunctionInfo&
PgE::detail::VariableOfMeta<^^Game::Counter>()   // -> const StaticFieldInfo&
```

These are **not** the consumer surface, and are deliberately absent from `Core.cppm`. They take a `std::meta::info`, and they are the mechanism the namespace sweep names entities through.

### The namespace sweep

`PgE::NamespaceOf<^^Game>()` returns the `NamespaceInfo` for a namespace: its types, free functions, variables, and directly nested namespaces. It is the **one consumer-facing entry point that asks a caller to write `^^`**, and it has to be: a namespace is not a type, a value, or a template, so it cannot be a template argument in any other form. There is no `NamespaceOf<Game>` to offer, on GCC or on any conforming implementation.

The point-of-query problem is real and is **not** solved here, it is *relocated*. `members_of` on a namespace answers for what the asking TU has seen: two calls at different points in one TU return different sets, and two TUs with different imports both get legitimate, differing answers. The base layer therefore reports members as found and reconciles nothing. A consumer that wants a stable set filters the sweep on something it controls, an annotation on the entities it cares about, and owns the consequence: the union over all TUs is complete even though no single sweep is, provided registration is idempotent, which pointer identity of the cached metadata already provides.

Pointer identity is what makes that work, so a swept entity **is** the object naming it hands out: the `FunctionInfo*` in `GetFunctions()` is `&FunctionOfMeta<^^F>()`, the variable is `&VariableOfMeta<^^V>()`, and a member type resolves to `&TypeOf<T>()`. Sweeping and naming never produce two descriptions of one entity.

The sweep is also the **only** route to an overloaded free function: `^^Game::Spawn` where `Spawn` is overloaded is ill-formed ("cannot take the reflection of an overload set"), so naming cannot reach either overload and the member list can.

Four member kinds are skipped, each because it has nothing to reflect rather than as a policy choice: **templates and concepts** (not an entity until instantiated), **namespace aliases** (the namespace is already reported under its own name; `NamespaceOf` dealiases, so an alias and its target share one instance), **anonymous namespaces and unnamed types** (no identifier to key on, no identity outside the TU), and free **operators** (modelled by kind on the owning type, with no free-standing metadata to point at, see *Gaps*).

The global namespace is **rejected**, by a `static_assert` that gates the instantiation rather than merely diagnosing it. `members_of(^^::)` in a TU including `<string>`, `<vector>` and `<cstdio>` returns 680 members, nearly all of libc; recursing it reaches deprecated `std` entities, which are hard errors under `-Werror`, and then segfaults cc1plus. There is no such thing as "my" global namespace, so a reflected entity is required to live in a named one.

| Information | Query | Field |
|---|---|---|
| Free function | named directly; `FunctionInfo` reused whole | `detail::FunctionOfMeta<^^F>()` |
| Not a class member | `is_class_member` | `FunctionTraits::IsFreeFunction` |
| Namespace variable | named directly; `StaticFieldInfo` reused whole | `detail::VariableOfMeta<^^V>()` |
| Thread storage | `has_thread_storage_duration` | `StaticFieldTraits::IsThreadLocal` |
| Namespace members | `members_of`, sorted by kind | `NamespaceOf<^^Ns>()` |
| Namespace member type | `NestedTypeInfo` reused whole, alias flag included | `NamespaceInfo::GetTypes()` |
| Namespace annotation | `annotations_of`; a namespace is an annotatable declaration | `NamespaceInfo::GetAnnotations<A>()` |

Both reuse the existing metadata types rather than adding parallel ones. A namespace-scope variable **is** a static data member without a class: no offset, no instance, every accessor drops the object pointer, and the constant-readable/addressable split applies unchanged, which is what keeps `constexpr int MaxSlots = 8` from failing at link time (Part 2, `StaticFieldInfo`).

`IsFreeFunction` is stored rather than derived because it is distinct from `IsStatic`: a static member is scoped to its class and a projection places the two differently. The invoke path branches on neither directly but on `CallsWithoutObject` (`is_static_member || !is_class_member`), since `is_static_member` is `false` for a free function and would otherwise route it down the member-call path. `IsConstCallable` now reports `IsConst || IsStatic || IsFreeFunction`: all three ignore the object pointer.

### Gaps the sweep exposes

None of these block the current use case (annotation-filtered discovery), and all of them are properties of the sweep rather than bugs in it. They are recorded because a consumer will meet them.

- **Materialization is eager and unfilterable.** `NamespaceOf<^^Ns>()` builds the metadata for *every* reflectable member, then hands the consumer the list to filter. An annotation filter therefore runs too late to save the compile time, and, more sharply, too late to avoid a member that cannot be reflected: one bad entity fails the whole sweep, with no way to opt out. `TypeOf` never had this exposure, because you only ever paid for what you named. If this bites, the fix is a separate `consteval` query returning `std::meta::info` lists that the consumer filters *before* anything is materialized; that returns no cached objects, so it does not threaten pointer identity the way a filtered `NamespaceOf<^^Ns, Filter>` would (two filters, two `NamespaceInfo`s for one namespace).
- **A TU-local member makes the sweep an ODR violation, not merely a differing answer.** `NamespaceInfo` is a `static constexpr` inside a template, so every TU that sweeps a namespace must agree on its contents. An internal-linkage member (a `static` free function, and note that a namespace-scope `constexpr` variable is internal-linkage too) is a *different entity* in each TU, so two TUs cannot agree even in principle. Such members are still swept, because excluding all internal linkage would drop every `constexpr` namespace constant, which is exactly what a consumer wants to reflect. The anonymous-namespace exclusion is therefore a naming rule (no identifier to key on), not a linkage one, and does not close this. A consumer sweeping from more than one TU should keep its fixtures externally linked, or sweep from a single registration TU.
- **A deprecated member is a hard error, not metadata.** Reflecting an entity names it, which fires `-Wdeprecated-declarations`, which is an error here. Sweeping any namespace containing a deprecated entity fails the build. Suppressing it belongs in the shared builders (reflecting is not using), not in the sweep, so it is left alone until something needs it.
- **An `extern` variable that is never defined fails at link.** The reflected accessors take its address. Definedness is not queryable, so this cannot be guarded; the sweep only widens exposure to a hazard `VariableOfMeta` already had.
- **A reference variable crashes GCC 17, and the crash cannot be intercepted.** `int& Ref = Target;` at namespace scope kills cc1plus in `cgraph_node::verify_node`, in the symbol table, long after the front end has finished, so a `static_assert` in `VariableOfMeta` never gets the chance to fire (verified: it does not). Exporting a `consteval` predicate over such a variable produces a *different* ICE (`import_export_decl`). Nothing here is a language rule; reflecting a reference variable is well-formed in principle. The sweep therefore **omits** them, which is verified and is what stops one declaration from failing a whole namespace, but **naming one directly through `VariableOfMeta` is unguarded** and will crash the compiler. No guard was added rather than shipping one that does not fire.
- **Inline namespaces read as ordinary nested ones.** `std::meta` has no `is_inline_namespace`, and the members of an inline namespace are not lifted into the parent's `members_of` even though name lookup finds them there. A consumer that mirrors lookup semantics must recurse and flatten itself. Note this disagrees with `ScopePathOf`, which *does* cross the inline namespace.
- **Free operators are not swept.** `OperatorInfo` is built per owning type and has no free-standing singleton to point at, so a namespace-scope `operator==` is currently invisible. A `OperatorOfMeta<^^F>()` mirroring `FunctionOfMeta` would close it.
- **`NestedTypeInfo` is reused for a namespace's types**, which reads oddly (nothing is nested in a namespace) but is the identical shape: name, alias flag, type reference, annotations. Reusing beat adding a parallel type; a rename to `MemberTypeInfo` would touch the existing public surface and was left out of scope.

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
| `members_of` on a class with a hidden friend | Does **not** surface it; a friend is not a class member, so `CallsWithoutObject` never sees one |
| Non-type template arguments | Work; `FixedArray<Foo, 8>` gives `is_value == true`, `extract<int>` yields `8` |
| `bit_size_of`, `has_default_argument`, `has_default_member_initializer`, `is_mutable_member` | All work |
| `is_noexcept`, `is_deleted`, `is_defaulted`, `is_virtual`, `is_pure_virtual`, `is_override`, `is_final` | All work |
| `is_lvalue_reference_qualified` / `is_rvalue_reference_qualified` | Both work |
| `is_explicit_object_parameter` | Works; the deducing-this object parameter is the first of `parameters_of`, and `&[:M:]` on such a member is a plain function pointer |
| `is_operator_function` / `operator_of` | Work; `operator_of` yields the `operators` enum. Copy/move assignment are operator functions **and** special members |
| `is_conversion_function` / `is_explicit` | Work; a conversion's `return_type_of` is its target, and it is **not** an operator function |
| Invoker for an implicit copy/move assignment operator | **ICEs cc1plus** on import; excluded by `is_special_member_function` (value operators, conversions, `operator new` are unaffected) |
| `is_type` over `members_of` | Yields nested classes, enums, and member aliases, and **not** the injected class name, so no self-exclusion guard is needed |
| Anonymous nested types | **Are** returned by `is_type` with `has_identifier() == false` (`Outer::<unnamed struct>`), so the filter must require an identifier |
| Copy-constructing a decayed type from a `volatile` member | **Ill-formed**; no copy constructor binds a volatile lvalue, so accessor guards must mirror the member expression, not test the decayed type |
| Alias detection via `dealias` | `dealias(member) != member` separates `using ValueType = int` from a real nested definition |
| `is_destructor` in `members_of` | Works; every class has an implicit destructor member, and `is_virtual` / `is_deleted` / `is_defaulted` / access all read off it |
| `members_of` on a non-class type | **Throws** ("neither complete class type nor namespace"), so the nested-type and destructor walks are guarded by class/union, not filtered afterwards |
| Address of a `volatile` member through `const void*` | **Ill-formed** (drops `volatile`); the borrow cast must go through `const volatile void*` |
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
| `is_class_member` | Discriminates correctly; **`false` for a base specifier**, which still has access, so `is_base` is needed beside it |
| Access predicates on a free function | All three are `false`, so a fallthrough reports `Private` |
| `parameters_of` on a function **type** | Yields the parameter **types** (`is_type == true`), not parameter declarations; `type_of` on one **throws** |
| That throw under `requires` | **Not caught.** Well-formed in an unevaluated context, like an immediate call |
| `return_type_of` / `is_noexcept` on a function type | Both work |
| No `is_variadic` | Does not exist; a C-style ellipsis needs a partial specialization |
| No `remove_member_pointer` / `member_pointer_class_of` | Neither exists; `remove_pointer` leaves a member pointer **unchanged** |
| No `template_parameters_of` | Does not exist. A template's own parameter list, arity and constraints are unreachable |
| `members_of` on a namespace | Works, but is **point-of-query dependent**: two calls in one TU returned 1 and 3 |
| Namespace members across modules | Visible when imported, and `display_string_of` tags the owning module (`FromA@NsA`) |
| `members_of(^^::)` | 680 members (445 functions) from `<string>`/`<vector>`/`<cstdio>` alone; unusable as a sweep |
| Recursive sweep of `^^::` | `-Werror` on deprecated `std` entities, then **ICE** (cc1plus segfault). Rejected by a gating `if constexpr`, since a `static_assert` alone does not stop the instantiation that crashes |
| `^^Overloaded` | **Ill-formed**, "cannot take the reflection of an overload set"; the sweep is the only route to an overload |
| `is_namespace` on a namespace alias | **`true`**, so `is_namespace_alias` must be tested first; `dealias` yields the target |
| No `is_inline_namespace` | Does not exist. An inline namespace is indistinguishable from a plain nested one |
| Inline namespace members | **Not lifted** into the enclosing namespace's `members_of`, though name lookup finds them there, and `ScopePathOf` crosses into it. A consumer must recurse |
| `annotations_of` on a namespace | Works: `[[=Tag{}]] namespace Ns` reflects, which is what a consumer filters a sweep on |
| A using-declaration in a namespace | **Not** reported by `members_of`, so a swept entity always really lives in the namespace |
| Reopened namespace | Both halves reported; a namespace is not closed the way a class is |
| Annotations across reopenings | **All** collected, from every `namespace X { }` block |
| `consteval auto` returning `std::array<T, sizeof...(I)>` | **GCC bug**: reflecting a reference variable through one fails with "use of ... before deduction of `auto`", though nothing in the body reads the entity's type. All 18 such builders now spell the return type out |
| Reflecting a reference variable | ICE in `cgraph_node::verify_node` (symbol table), too late for a `static_assert` to intercept |
| Access at namespace scope | `None` for types, functions and variables alike; no `Private` lie |
| `source_location_of` | Works and discriminates a project file from `/usr/include/stdio.h`, but see Part 4 |

## Part 4: Deliberately not extracted

- **`source_location_of`.** Only the native provider could supply it, so it breaks the one-`TypeInfo`-contract rule, and it changes on every edit above the declaration.
- **The `<type_traits>` mirrors** (`is_nothrow_swappable_type` and its ~80 relatives). Reflection exposes what a consumer cannot compute itself; a consumer with the type can ask `std::` directly.
- **`define_aggregate` / `data_member_spec` / `substitute`.** Type *synthesis*, not extraction. A separate capability with no current use case.
- **`symbol_of`.** Operator *spelling* only (it takes an `operators` enum). Operators are now reflected (see Part 2), but keyed on an engine-owned `OperatorKind` mapped from `operator_of`, not on `symbol_of`'s string: a consumer keys on the fact, and the C-style spelling is left to a renderer if one ever wants it.
- **`type_order`.** Not extracted, but recorded because it is the mechanism for a deterministic registry ordering if ordinals are ever wanted on the wire.

- **Reconciling a namespace sweep.** The sweep itself now exists (`NamespaceOf`, Part 2), but nothing in the base layer merges or stabilizes what two sweeps report. That is deliberate: answers that disagree by construction cannot be repaired downstream, only filtered on something the consumer controls.

- **Template parameters.** A `TemplateInfo` names its template and nothing more, because `template_parameters_of` does not exist on GCC. `Grid<int, 4>` is fully described, but `Grid`'s own parameter list, arity, and constraints are unreachable, so a projection can mirror instantiations but never an open generic.

- **The active member of a union.** A union reflects its `TypeKind` and its members, and the language offers nothing to say which member is live. This is stated rather than silently half-supported: a consumer walking a union's fields as if they were a struct's will produce garbage, and the refusal belongs in the consumer (a serializer) or in a discriminator convention, not in a query that cannot exist.

- **`source_location_of` as a file filter.** It works, and it would let generated registration filter the global namespace down to one file. Rejected for the same reason as storing it: it makes the native provider's registration structurally unlike a managed or script provider's. Requiring a named namespace costs nothing here and removes the need entirely.
