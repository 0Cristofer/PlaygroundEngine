# Reflection Internals

The implementation knowledge behind the reflection system: how a `TypeInfo` is built and cached, the
partition structure that keeps it acyclic, and the validated `std::meta` patterns (with their GCC 16
workarounds) that the engine and the `PlaygroundReflection` scratchpad rely on. Parent document:
[EngineDesign.md](../EngineDesign.md). The user-facing design lives in
[ReflectionSystem.md](ReflectionSystem.md); this document is the "how" and the "why it is shaped this
way", the detail that would otherwise sit in long inline comments.

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
- **Function invocation through a pointer:** the invoker calls through `&[:MetaFunction:]` rather than a
  direct member splice `obj->[:M:](...)`. GCC applies member access control to a spliced **call**
  expression even when the reflection came from `access_context::unchecked()`, but not to forming a
  pointer from the reflection or calling through it. Routing the call through the pointer is what lets
  reflection invoke **private** member functions, keeping private-member metadata symmetric with fields.
- **Argument binding:** parameters are bound inline at the call so a by-value parameter is initialized
  directly from the bound prvalue (guaranteed copy elision), so a type with a deleted move constructor
  still binds by copy.

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
  trip. This is the pattern the engine uses instead of constructor reflection.
- **JSON deserialization** (`serialization.h`): `FromJson<T>` matches JSON keys to member names at compile
  time (`if constexpr` per field type, since a splicer cannot appear in a template argument) and writes
  through `obj.[:member:] = value`. No per-type boilerplate.
- **Inheritance, type names, type_index, enums** (`type_exploration.h`): `nonstatic_data_members_of`
  returns only **direct** members, so inherited fields need `bases_of` plus recursion. `display_string_of`
  names any type including template specializations (`identifier_of` fails on those). `typeid(typename [:splice:])`
  recovers a `std::type_info` from a reflection (the `typename` keyword is required). `enumerators_of` lists
  enum values; the `[:enumerator:]` splicer is consteval-only in GCC, so values must be bound to a
  `constexpr` local before runtime use.
