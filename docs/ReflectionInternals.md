# Reflection Internals

The implementation knowledge behind the reflection system: how a `TypeInfo` is built and cached, the
partition structure that keeps it acyclic, and the validated `std::meta` patterns (with their GCC 16
workarounds) that the engine and the `PlaygroundReflection` scratchpad rely on. Parent document:
[EngineDesign.md](../EngineDesign.md). The user-facing design lives in
[ReflectionSystem.md](ReflectionSystem.md); this document is the "how" and the "why it is shaped this
way", the detail that would otherwise sit in long inline comments.

## EntityInfo and DeclarationInfo

The type model's root is split in two, and the language draws the line, not us:

- **`EntityInfo`** is what every named entity has: identifier, display name, and scope path. *Entity* is the
  language's own term ([basic.pre]/3), and it is what admits a **template** alongside a type, a member, and a
  function.
- **`DeclarationInfo : EntityInfo`** adds annotations. Its set is exactly what `annotations_of` accepts: a
  type, type alias, variable, function, function parameter, namespace, enumerator, direct base class
  relationship, or non-static data member. Every `...Info` except `TemplateInfo` is one.

**A template is an entity but not an annotatable declaration**, so `TemplateInfo` derives from `EntityInfo`.
This is not a technicality: `annotations_of` **throws** on a template, so a `TemplateInfo` inheriting the
annotation API could only ever answer "empty", which reads as *no annotations here* rather than the truth,
*that cannot be asked*. Nothing is lost by the split, because an annotation written on a class template lands
on each **instance** (`annotations_of(^^Grid<int>)` finds it), and an instance is a type, hence already a
`DeclarationInfo`.

The identifier is the structural query key; the display name is implementation-defined diagnostic text,
always present, never a key.

## Module layering

Three modules: `PlaygroundEngine.Reflection.Core`, `PlaygroundEngine.Reflection.Builtins`, and the
umbrella `PlaygroundEngine.Reflection` that re-exports both plus `PlaygroundEngine.Reflection.Contracts`.

- **Core** is the type model (the `...Info` types), the facet-table mechanism (`FacetEntry`, the generic
  `MakeFacetsFromType`), and the builders that assemble a `TypeInfo`. It names **no concrete facet**, so
  a facet is always an extension layered on top. `TypeOf` lives in Core because `TypeInfo::GetFacet`
  reflects the facet type through it, so the core cannot be built without it.
- **Builtins** adds the string, sequence, and enumeration facets as extensions.
- Rendering (`ObjectToString` / `ToString`) is Core too: it names no facet, so it belongs with the type
  model, not with any facet that renders through it.

## The `TypeOfMeta` recursion knot

`TypeOfMeta<MetaType>()` returns the one program-lifetime `TypeInfo&` for a type. Building it recurses:
a type's fields and function signatures are themselves types that need their own `TypeInfo`. The knot is
resolved by **partition structure**, not runtime indirection:

- `TypeOfMeta` is **declared** in the `:MetaCommon` partition and **defined** only in `:TypeBuilder`.
- The per-kind sub-builders (`:FieldsBuilder`, `:FunctionsBuilder`, ...) reference `&TypeOfMeta<memberType>`
  (its address, from the declaration) without instantiating it, so they depend on the **declaration
  alone**.
- `:TypeBuilder` is the only unit that imports the sub-builders and holds the definition. That keeps the
  partition import graph acyclic.

## Canonical caching and dealiasing

`TypeOfMeta` keys its cached `TypeInfo` on the **canonical (dealiased) type**, not the spelling. Every
alias of a type (`std::uint16_t`, `std::underlying_type_t<E>`, `unsigned short`) must resolve to one
instance, because pointer identity (`&TypeOf<T>()`) is what annotation matching, serialization, and the
C# dedup layer compare by. `TypeOfMeta` first tail-calls itself on `std::meta::dealias(MetaType)` when the
spelling is an alias, then caches the canonical instance.

## `TypeReference`: lazy cross-references

A field's type, a parameter or return type, an annotation's type, an enum's underlying type: every
cross-reference stored in the metadata is a `TypeReference`, which holds the **resolver's address**
(`&TypeOfMeta<...>`), not a `TypeInfo` pointer and not a call.

- Storing the address, resolved on first read, is what lets a type name **itself** (a factory method
  returning the type by value) or two types name **each other**, without the consteval construction of
  one depending on the completion of the other.
- `&Get()` still yields the same pointer identity `&TypeOf<T>()` does, so all the pointer-comparison
  consumers are unaffected.
- Bind to the **dealiased** type. `TypeOfMeta` dealiases internally anyway, so binding an alias would only
  tail-call the canonical one, and skipping it dodges a **GCC 16 reflection mangling collision**: the
  alias's template argument is dropped from the mangled symbol, so two distinct alias instantiations (for
  example `underlying_type_t` of two different enums) would resolve to one duplicately-defined symbol.

## The builder pipeline

`:TypeBuilder`'s `MakeType<MetaType>()` orchestrates the per-kind builders (`MakeFieldsFromType`,
`MakeFunctionsFromType`, `MakeAnnotations`, `MakeTraits`, `MakeFacetsFromType`) into one `TypeInfo`. New
kinds of type information are added here, each delegating its collection-heavy work to its own `:*Builder`
partition.

- **Stringify thunk assignment:** a fieldless object is a leaf and gets a stringify thunk (it renders
  through its total trait); `std::is_object_v` excludes `void` (a function return type reached here),
  where a `const T*` cannot be formed. A type with fields gets no thunk and is rendered structurally.
- **Function invocation, and why the route depends on access:** the invoker calls a **public** function
  through a direct member splice (`obj->[:M:](...)`) and a non-public one through `&[:MetaFunction:]`. GCC
  applies member access control to a spliced **call** expression even when the reflection came from
  `access_context::unchecked()`, but not to forming a pointer from the reflection or calling through it, so
  the pointer is what lets reflection invoke **private** member functions, keeping private-member metadata
  symmetric with fields. The address is taken *only* there because **some functions have no address at all**:
  an `always_inline` member of a class with an explicit instantiation **declaration** is emitted nowhere, so
  taking its address fails at link. `extern template class allocator<char>;` plus
  `[[__gnu__::__always_inline__]] allocate` is exactly that pair, and `std::allocator<char>` is reachable as
  a materialized template argument of `std::string`. Like the static-member address trap below, forming the
  pointer is well-formed during substitution, so `requires` cannot detect it; not taking an address that
  access control does not require is the fix.
- **Deleted and consteval functions are excluded by a stated fact, not by SFINAE.** Naming a **deleted** one
  in `IsInvocable`'s requires-expression is a hard error rather than a substitution failure, so `MakeInvoker`
  checks `is_deleted` first. A **consteval** (immediate) function is the opposite trap: the requires-expression
  *accepts* it (an immediate call is well-formed in an unevaluated context), so `MakeInvoker` would emit a
  thunk whose body then calls it at runtime, which is a hard error that breaks the build for the whole type.
  `IsImmediateFunction` (shared with the constructor path) excludes it up front, and `IsConsteval` states the
  reason, exactly as for a `consteval` constructor. GCC 16 exposes no `is_consteval`, so both read the
  specifier from the display string.
- **A deducing-this member's object parameter is not a call argument.** `parameters_of` counts the explicit
  object parameter (`int Get(this const Self&, int)` has arity 2), so `CallParametersOf` drops a leading
  `is_explicit_object_parameter` and every function path (the `ParameterInfo` list, the arity check, the
  per-argument binding, the invoke call) keys off it, keeping the reflected arity the caller's. Left uncut it
  is a lie as metadata: a C# generator reading arity 2 emits a two-argument method. The object binds the same
  way an implicit `this` does, so the public member-splice route is unchanged; the **private** route differs,
  because `&[:M:]` on a deducing-this member is a plain function pointer (not a pointer-to-member), called
  free-function style with the object first. `is_const` and the ref-qualifier queries are all **false** on the
  function itself (the qualification lives on the object parameter), so `MakeFunctionTraits` reads `IsConst`
  and `RefQual` off the object type. A **by-value** object parameter (`this Self`) is const-callable (a copy
  leaves the caller's object untouched), so `ObjectIsConstCallable` treats const-ref and by-value alike and a
  mutable lvalue-ref or rvalue-ref as not.
## The argument binder

`:ArgumentBinding` is the shared machinery every erased callable uses: parameter metadata, the
type-driven per-argument binding, and the neutral validation each caller maps to its own error type. It
exists so `FunctionInfo` and `ConstructorInfo` agree on what an argument means, rather than each deciding.

- **The parameter's own type drives the call.** `ArgumentBinding<P>::Bind` returns an lvalue for a
  reference parameter, moves for an rvalue-ref or move-only by-value parameter, and copies otherwise.
  Parameters are bound **inline at the call**, so a by-value parameter is initialized directly from the
  bound prvalue (guaranteed copy elision) and a type with a deleted move constructor still binds by copy.
- **`Movable` is the caller's offer, not a hint.** Wherever `Bind` moves out of the argument, the erased
  caller must have said the object may be moved from; otherwise the call is refused (`NotMovable`) rather
  than silently gutting the caller's object. `ArgumentBinding` publishes exactly this as `NeedsMovable`,
  with `NeedsMutable` its superset (mutable lvalue reference, or anything that moves).
- **A const argument is never an offer.** `Movable && !IsConst` is the whole condition, so tagging a const
  argument movable does not make it one.
- **`Data` is the argument object's address**, so a null one names no object and is refused
  (`NullArgument`); a null *pointer* argument still has an address. This is unlike a function's **return**
  slot, where null is the caller discarding the result.
- **Validation is neutral, mapping is per caller.** `CheckArgument` returns an `ArgumentError`, which
  `ToInvokeError` / `ToConstructError` map onto the caller's own enum. A new failure kind is one enumerator
  plus one `case` per caller, not another `else if` in two chains.
- **GCC 16 gap, contracts on inline free functions in module interfaces:** a `pre`/`post` on an **`inline`
  or `constexpr` free** function defined in a `.cppm` leaves `undefined reference to __tu_has_violation` in
  every importing TU. A non-inline free function, a function **template**, and an **in-class member** all
  link (templates and members are emitted downstream too, so "emitted in the importer" is not the trigger);
  the type model's own preconditions are all members, which is why they are unaffected. `ToInvokeError` /
  `ToConstructError` are `constexpr`, hence inline, hence hit it, and they are runtime-only callers so the
  `constexpr` buys nothing: dropping it would restore the contract but leave a landmine where re-adding
  `constexpr` breaks the build. They state the invariant in a comment instead.

## Lifetime ops

A constructor has no object pointer and its "return" is the mandatory construction into a caller-owned
slot, so `ConstructorInfo` is a callable distinct from `FunctionInfo`, sharing only the argument binder.

- **The thunk never splices the constructor.** P2996 forbids a constructor in a splice (GCC 16 included),
  so the thunk casts the erased arguments to the constructor's **own parameter types** and placement-news;
  overload resolution then selects exactly that constructor. This is why the engine reflects constructors
  directly rather than routing construction through an annotated static factory.
- **No thunk means not callable, and the traits name why.** A constructor that is deleted, inaccessible,
  `consteval`, or whose placement-new is otherwise ill-formed reflects as metadata with
  `CanConstruct() == false`, the way an rvalue-qualified function reflects with no invoker. `ConstructorTraits`
  carries the reasons that are language facts (`IsDeleted`, `IsConsteval`, and `Access` for an inaccessible
  one), so a residual null thunk means "unbindable" and a deleted copy is told apart from a silent absence.
  `consteval` is detected by the specifier in the display string: GCC 16 exposes no `is_consteval`, and
  immediateness is invisible to SFINAE.
- **The slot protocol is stated once.** `detail::ValueFromSlot` runs a slot-taking call, then launders the
  object out of the stack slot and destroys it there. A function's return and a constructor's object differ
  only in the call and the error reported, so both `InvokeAs` and `ConstructAs` go through it.

### The destructor

A type has exactly one destructor, so it is a single `DestructorInfo` on `TypeInfo` (`GetDestructor()`), not
a list. It owns the erased destroy thunk that `TypeInfo::CanDestroy` / `Destroy` delegate to, so the runtime
path and the metadata are one object rather than a bare function pointer beside unrelated traits.

- **The traits are read off the destructor member, the triviality off the type.** `is_virtual`,
  `is_pure_virtual`, `is_deleted`, `is_defaulted`, and access come from the `is_destructor` member found in
  `members_of`; `IsTrivial` is the whole-type `is_trivially_destructible_type`. Every class has an implicit
  destructor member to read, so the split is only needed for non-class types, which have no member at all and
  keep the defaults (a scalar is trivially destructible and destroyable, but nothing about it is "defaulted").
- **No thunk means not destroyable, and the traits name why.** A deleted or inaccessible destructor, an
  incomplete type, an array, and `void` all reflect with `CanDestroy() == false`, the way a constructor
  reflects with no thunk; `IsDeleted` turns the common case from a silent absence into a stated reason.
- **`members_of` is guarded, not merely filtered.** It throws on a non-class type, so the destructor-member
  walk sits behind `IsClassOrUnion` rather than being filtered afterwards.

### Selecting a constructor from erased arguments

`TypeInfo::Construct` / `ConstructAs` pick the constructor themselves, so a caller never has to name one.
This is overload resolution, restricted to what erased arguments can carry:

- **Match is viability, on the tag alone.** The binder admits no conversions, so a candidate matches when
  the arity agrees and every parameter's decayed type is the argument's tag. A constructor with no thunk is
  never a candidate: selection must not resolve to something the erased path cannot call.
- **Rank on the one axis the arguments carry.** `ParameterInfo` publishes `BindsByMove` because its stored
  type is **decayed**, so `const T&` and `T&&` are one tag and nothing else could separate them. A
  parameter suits an argument when `BindsByMove == (Movable && !IsConst)`; the better candidate is better
  in at least one parameter and worse in none, then confirmed against every other (being best of a pairwise
  sweep is not the same as beating all).
- **This subsumes copy/move rather than special-casing it.** The same rule resolves a copy/move pair and a
  `Foo(const std::string&)` / `Foo(std::string&&)` sink pair; `ConstructorKind` is a query, never a
  selection input.
- **Genuine ties are reported, not guessed.** Overloads the arguments cannot rank (`int&` against
  `const int&`: both bind by reference, neither moves) yield `AmbiguousConstructor`, and the caller falls
  back to `FindConstructor` plus `ConstructorInfo::ConstructAs` to name one.

## Operators and conversions

An overloaded operator (`operator+`, `operator==`) and a user-defined conversion (`operator int()`) are
member functions, so they carry a return type, parameters, traits, and an invoker like any other. They are
kept in **their own lists** (`GetOperators` / `GetConversions`), not `GetFunctions`, so a consumer keys on a
fact rather than parsing a name: an operator on its `OperatorKind`, a conversion on its target type. This is
also what closes two holes: a conversion has **no identifier** and would otherwise sit in `GetFunctions`
nameless, and operators were excluded from `GetFunctions` entirely and so were unreachable.

- **The whole function machinery is reused.** `OperatorInfo` and `ConversionInfo` derive from `FunctionInfo`;
  their builders call the same `MakeParameters` / `MakeFunctionTraits` / `MakeInvoker`, so `a + b`, `a == b`,
  `a[i]`, and `operator int()` all invoke through reflection with no second code path. `OperatorKind` is an
  engine-owned enum mapped from `std::meta::operator_of`, so the public API does not leak the `std::meta`
  spelling. A conversion's **target is its return type**: `ConversionInfo::GetTargetType()` is `GetReturnType()`,
  decayed with the `ReturnIs*` trait flags carrying the qualifiers, exactly as every return type is modelled.
- **Copy and move assignment are excluded, and this is load-bearing (a GCC 16 ICE).** They are operator
  functions (`op_equals`) but also **special members**, so `GetOperatorFunctions` excludes
  `is_special_member_function`, the way `GetMemberFunctions` already does: they belong to the lifetime story,
  not the value-operator one, and a converting assignment (`operator=(int)`) is not special and stays.
  Beyond the modelling, building an **invoker** for an implicitly-declared copy/move assignment operator
  **segfaults cc1plus** in every TU that imports the reflection module and reflects a type carrying one
  (every class has them, so the whole engine failed to build). The exclusion is narrow: user-defined value
  operators, private operators, conversions, and member `operator new` / `operator delete` all build invokers
  without incident.
- **Nothing about their *shape* is special, which is why the ICE is a compiler bug and not a design limit.**
  Were the ICE lifted, copy and move assignment would need no specialized invoke at all: they are ordinary
  member functions (`(const T&) -> T&` and `(T&&) -> T&`), and `ArgumentBinding` already binds both parameter
  forms (the lvalue-ref branch copies, the rvalue-ref branch moves out). Copy versus move is not a mode the
  invoker forces, it is the parameter type of two distinct overloads, so a caller picks the overload and the
  existing binder does the rest. The only genuine work would be classification (grouping them with the
  copy/move constructors in the lifetime story) rather than mechanism.

## Nested types

`GetNestedTypes()` reports the types a class declares **inside itself**: nested classes, enums, and unions,
and member type-aliases (`using` / `typedef`). It is the reflexive half of the structure, and it is
independent of the fields: a nested type with no member of that type was previously unreachable, since the
only way to a nested type was through some field or return type that happened to use it. The C# binding layer
is the consumer that needs it, since it mirrors nested types as nested types.

- **`is_type` plus `has_identifier` is the whole of the filter.** GCC 16 does **not** return the injected
  class name from `members_of`, so no self-exclusion guard is needed (validated: a type with four type
  members reports exactly four). It *does* return anonymous nested unions and structs, which have no
  identifier, so those are excluded: nothing can name one, and the field that uses it already reaches it.
- **An alias is detected by dealiasing, and keeps its own name.** A member is an alias when
  `dealias(member) != member`, a real nested definition when dealiasing is the identity. The stored
  `TypeReference` is to the **dealiased** type, so `using ValueType = int` reports the name `ValueType` and
  resolves to `int`'s `TypeInfo`; the declaration carries the name, the reference carries the type.
- **A self-alias is safe because the reference is lazy.** `using SelfAlias = Outer` inside `Outer` resolves
  back to the enclosing type. It cannot recurse during construction, for the same reason a self-referential
  field cannot: `TypeReference` stores the resolver's address, not a resolved pointer.

## Naming: scope, structure, and decomposition

What the extraction stores so that **a type is nameable from facts alone**, with no spelling anywhere. The
end-to-end claim (`const PgE::Foo*` renders as `pointer<const<PgE.Foo>>`, `std::vector<Foo>` as
`std.vector<PgE.Foo, std.allocator<PgE.Foo>>`) is pinned by a renderer in `ReflectionExtractionTests.cpp`
that reads no display string. The scope of the extraction and its rationale live in
[ReflectionExtraction.md](ReflectionExtraction.md); this section is the implementation knowledge.

- **Scope path.** `DeclarationInfo::GetScopePath()` is the chain of enclosing named scopes, outermost first,
  not a rendered string, so the separator stays the consumer's choice. It is what distinguishes `A::Foo` from
  `B::Foo`, which is a structural fact about the language, not an identifier concern. There is no
  `qualified_name_of`; the `parent_of` walk is the only route. **`has_parent` is the guard, not an
  optimization:** `parent_of` *throws* for an entity with no parent (a fundamental type, `void`, a pointer),
  which would abort the build. The walk crosses inline namespaces (`std::string` yields `["std", "__cxx11"]`),
  which is the correct structural answer and the reason a consumer must not build a durable identifier from a
  `std` type's path.
- **The chain is collected as reflections, not `string_view`s.** `define_static_array` needs a **structural**
  element type and `std::string_view` is not one (`std::meta::info` is), so the identifiers are frozen
  per-index with `define_static_string` instead. Same shape as `MakeAnnotations`.
- **Fundamentals are named from structure.** `has_identifier(^^int)` is `false`, so an identifier derived from
  `TypeKind` + `size_of` + `is_signed_type` (`int32`, `uint64`, `float64`) fills `_identifier`. It is
  stdlib-neutral, and a non-native provider can describe the same type with no C++ name. Two exceptions are
  named by spelling because size and signedness **collapse** them: the **character types** (`char`,
  `signed char`, and `int8_t` are distinct, and `char` is the string element type `StringFacet` keys on), and
  **`long double`** (x86 stores 80-bit extended in 16 bytes, so a width would claim a `binary128` it is not,
  and would collide with a real one).

## Compound types

`template_of` alone does not bottom out: pointers, references, arrays, and fundamentals have **no**
identifier, so recursion through a template's arguments terminates on entities that cannot be named.
`TypeInfo::GetInnerType()` peels exactly **one** shape, so a walk chains (`const Foo*` yields `const Foo`,
which yields `Foo`), with `TypeTraits::Extent` carrying an array's length.

- **A cv-qualified type is a decomposition node, and this is load-bearing.** `is_class_type(^^const Foo)` is
  **true** and `nonstatic_data_members_of` enumerates its members, so walking it would duplicate `Foo`'s entire
  structure under a second identity. `IsClassOrUnion` therefore excludes cv-qualified types: it is the single
  gate every member-walking builder already shares, so the rule is stated once. The cv node keeps only
  `IsConst`/`IsVolatile` plus its inner type; the structure lives on the unqualified type alone.
- **cv is peeled before the other shapes**, since a `const` pointer is a cv node whose inner is the pointer.
- **Why not canonicalize `const` away** the way aliases are: then `const Foo*` and `Foo*` would have the same
  pointee, losing a fact the language spells.
- Unbounded arrays (`int[]`) are object types but **incomplete**, so `size_of`/`alignment_of` are ill-formed
  for them and the traits builder guards on the zero extent.

## Templates

`TypeInfo::GetTemplate()` names the primary template and `GetTemplateArguments()` lists what it was
instantiated with, so `Grid<int>` keeps a recoverable relationship to `Grid` and to `int`.

- **A template is not a type**, so it cannot be a `TypeReference`; `TemplateInfo` names it (identifier plus
  scope) and that is all that is possible. One program-lifetime instance per template, so templates compare by
  pointer identity the way types do. It is an `EntityInfo`, not a `DeclarationInfo`, because a template cannot
  be annotated (see the taxonomy section above).
- **`template_of` / `template_arguments_of` throw** for a type that is not an instance, so
  `has_template_arguments` is a guard, not a filter.
- **Three argument kinds, not two.** `is_type` and `is_value` are both false for a template template argument
  (`Holder<Grid, Foo>`), which is neither.
- **A reference parameter's argument is an object, not a value.** `template <int& R>` instantiated with a
  global reflects with `is_object == true`, `is_value == false`, and `type_of == int` (the *referenced* type).
  There is nothing to materialize: the argument **is** that object, so the erasure is its address. Reading it
  as a value is not a constant expression, so conflating it with the value case fails to compile. A **pointer**
  parameter is the opposite (`is_value == true`, `type_of == int*`): its value really is the pointer.
- **A partial specialization reports the primary template** (`Spec<int*>` yields `Spec`), which is the wanted
  behavior. **Alias templates dealias away entirely** (`Ptr<Foo>` yields `Foo*`).
- **Defaulted arguments are materialized and cannot be identified as defaults.** `Stream<Foo>` reflects with
  **two** arguments and no query distinguishes the written one from the defaulted one. This is correct, since
  `Stream<Foo>` and `Stream<Foo, DefaultPolicy>` are the same type and must reflect identically; it matters
  only to a consumer building a durable identifier, where adding a defaulted parameter later changes rendering.
- The allocator of a `std` container is **exposed** as a materialized argument. That is the correct structural
  answer, and it is what drags `std::allocator<char>` into the graph (see the invocation note above).

## Incomplete types

A pointer names its pointee, so an **incomplete** type is now reachable: `struct Opaque;` behind an
`Opaque*` member is the opaque-handle / PIMPL shape, and it must not break the build. Every member walk
(`nonstatic_data_members_of`, `bases_of`, `members_of`) **throws** on an incomplete type, and `size_of`,
`alignment_of`, and the whole `<type_traits>` family have a complete-type precondition.

- **`is_complete_type` is the guard, and it is stated in the shared gates**, not per builder: `IsClassOrUnion`
  (which every member walk already goes through) and the traits builder's `SizeOf`/`AlignmentOf`. It subsumes
  the other sizeless cases for free: `void` and an **unbounded array** (`int[]`) are both incomplete.
- **Nest the `if constexpr`, do not `&&` it.** A variable template such as `std::is_destructible_v<T>` is
  *instantiated* even where `&&` would short-circuit its evaluation, so `is_complete_type(T) && is_destructible_v<T>`
  still hard-errors on an incomplete `T`. The completeness check has to be its own enclosing `if constexpr`.
- The result is honest rather than degraded: an opaque type reflects with its **identifier and scope** and
  nothing else. Its definition is what decides size, layout, and the trait predicates, so they stay unset
  instead of being guessed.

## Static data members

`static_data_members_of` gets its own metadata type and its own span on `TypeInfo`, not an entry in `_fields`:
a static has no offset and no instance, so every accessor signature differs (no object pointer).

**The address of a static data member cannot be taken unconditionally.** A `static const` member of integral or
enumeration type may be initialized in-class and never defined out of line, which is legal and is exactly the
editor-helper pattern (`static const int MaxSlots = 8;`). Taking its address is an odr-use that fails at
**link**. `requires` does not save us: forming the address is well-formed during substitution, so nothing at
compile time detects it. The rule that resolves it:

- **Constant-readable members are captured by value** at `consteval` and never odr-used; the thunk holds the
  value, not a pointer. Detection is a constant read (binding the splice to a non-type template parameter),
  which **is** a substitution failure and therefore detectable, unlike the address case.
- **Everything else takes an address**, which is safe because any static member that is not constant-readable
  is required to have a definition anyway.

This maps onto the capability rather than dodging it: a constant-readable static is **read-only** (a value, so
no setter and no borrow), and an addressable one is **settable** (a reference). The split is the semantics.

## Type hierarchy

`:BasesBuilder`'s `MakeBasesFromType` collects a type's **direct** base classes into `BaseInfo` entries
(`bases_of` returns direct bases only, so inherited bases are reached by recursing through a base's own
`GetBases()`, never flattened in, the same direct-only convention the field walk uses). Each entry carries
a lazy `TypeReference` to the base, its `AccessKind`, its byte `Offset`, and its annotations. A superseding
facet suppresses bases for the same reason it suppresses fields.

- **Offset is a chainable constant because virtual inheritance is rejected.** `MakeBase` `static_assert`s
  on `is_virtual`. A virtual base's derived-to-base distance lives in the vtable and depends on the
  most-derived type, so a stored constant would be wrong once the type is embedded further down; a
  non-virtual base subobject sits at a fixed offset that sums along a path, which is what a later
  `Cast<T>`/`IsA<T>` built on this layer relies on. The rejection fires lazily, when the offending type is
  actually reflected. If a real virtual case ever appears, the fix is a per-base upcast thunk
  (`static_cast<Base*>(static_cast<Derived*>(p))`, correct because it reads the runtime vptr), added then.
- **Downward is not enumerable.** P2996 cannot list derived classes, so the hierarchy is upward-only;
  object-to-data polymorphism is `Poly<T>`'s capture-at-erasure, not a bases walk.
- **GCC 16 gap, annotated base specifiers:** `[[=Ann]]` on a base specifier ICEs GCC's module writer
  (`write_class_def`), so a type that annotates a base **cannot be defined in a `.cppm` module**; it
  compiles fine in a normal TU. `BaseInfo` carries the annotation span and the builder reads it, but until
  the compiler is fixed the feature is only exercisable (and tested) for types defined outside module
  interfaces.

## Facets

A facet is a semantic view (string, sequence, enumeration) layered on a `TypeInfo`. The mechanism names
no facet kind:

- A `TypeInfoTraits<T>` specialization returns its facets from one `MakeFacets()` hook, a `std::tuple`
  so a type may carry several heterogeneous facets. `:FacetsBuilder` keys each entry by the facet's own
  type and reads its `Supersedes` flag generically off that type. Adding a facet is a new
  `TypeInfoTraits` specialization (and, if it stops the structural walk, a `Supersedes = true` member),
  never an edit to the builder. The specializations are found at `MakeType`'s instantiation site.
- **`Supersedes`:** a facet whose value is entirely its own content (a `std::span<const T>` sequence, a
  string) sets `Supersedes = true`, which empties the structural field/function view because that
  structure is an implementation detail. A facet that adds information alongside the fields omits the
  member and reads as `false`.
- **Facet-authoring toolkit:** `TypeOfMeta`, `TypeReferenceTo`, `IdentifierOf`, and `DisplayStringOf` are
  exported from `:MetaCommon` because a facet binding lives in its own module (Builtins, or a user's) and
  reaches back for them across the module boundary, where only exported names are visible.

## Rendering

`ObjectToString(typeInfo, obj)` is total: every reflected value renders.

- A type **with** a stringify thunk renders through it: a formattable leaf, the type-name placeholder for
  an unformattable leaf, or a facet that installed its own rendering (a quoted string, a `[...]` sequence,
  an enumerator name). The renderer names no facet; each facet's rendering lives in the thunk its
  `TypeInfoTraits` set, and a sequence thunk recurses back into `ObjectToString` for its elements.
- A type **without** a thunk is a struct, rendered by walking its fields. Field access prefers the borrow
  (`GetRef`, reads in place at any size); the stack-slot fallback is for a non-addressable field (a
  bitfield), always a small trivial type that fits the slot.

## Validated `std::meta` patterns (GCC 16)

The `PlaygroundReflection/` scratchpad validates the patterns the engine builds on. Each file is a
focused demo; the reusable knowledge, including the GCC 16 workarounds, is captured here.

- **Field enumeration** (`field_printing.h`): `^^T` reflects a type; `nonstatic_data_members_of(r)` lists
  its data members; `template for (constexpr ...)` over `std::define_static_array(range)` loops at compile
  time (each iteration its own instantiation); `obj.[:member:]` splices a member reflection back into a
  field access.
- **Annotations** (`annotation_registry.h`): `[[=Tag{}]]` attaches an empty compile-time tag, `[[=Range{0,100}]]`
  a value-carrying one; `annotations_of_with_type(member, ^^Tag)` retrieves them and `std::meta::extract<Tag>(anno)`
  reads the value back. An annotation type must be structural. `+[](...){...}` converts a captureless
  lambda to a plain function pointer for a name-to-function map.
- **Erased function registry** (`generic_registry.h`): `parameters_of`, `return_type_of`, `type_of`, and
  `std::index_sequence` erase any signature to a shared invoker at the cost of boxing to `std::any`. Two
  GCC workarounds: a bare splicer is forbidden as a template argument inside a function body
  (`-Wtemplate-body`), so move the splice into a struct's using-alias (`ArgCast`); and
  `std::make_index_sequence<N>` in a lambda body is consteval-only in GCC, so split into two functions to
  lift the `index_sequence` construction to template-instantiation time.
- **Construction via annotated static factory** (`construction.h`): constructor splicing is not supported
  in GCC 16 (or P2996: constructors cannot appear in a splice), so construction goes through a
  `[[=Factory{}]]`-annotated static function whose parameters are tagged `[[=Injected{}]]` (caller-provided)
  or `[[=Serialized{}]]` (from data). `InvokeStaticTyped` splices the reflected return type into the
  invoker's own signature (`[:return_type_of(Fn):]`), returning the real type with no `std::any` round
  trip. The engine no longer needs this to construct (see Lifetime ops: the thunk casts arguments to the
  constructor's own parameter types, so nothing is spliced), but the annotated-factory pattern still stands
  for construction that is genuinely a named operation with injected and serialized parameters.
- **JSON deserialization** (`serialization.h`): `FromJson<T>` matches JSON keys to member names at compile
  time (`if constexpr` per field type, since a splicer cannot appear in a template argument) and writes
  through `obj.[:member:] = value`. No per-type boilerplate.
- **Inheritance, type names, type_index, enums** (`type_exploration.h`): `nonstatic_data_members_of`
  returns only **direct** members, so inherited fields need `bases_of` plus recursion. `display_string_of`
  names any type including template specializations (`identifier_of` fails on those). `typeid(typename [:splice:])`
  recovers a `std::type_info` from a reflection (the `typename` keyword is required). `enumerators_of` lists
  enum values; the `[:enumerator:]` splicer is consteval-only in GCC, so values must be bound to a
  `constexpr` local before runtime use.
