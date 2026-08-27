# Core Conventions

Engine-wide decisions: object model, memory, native/managed boundary, std usage, error handling. Parent document: [EngineDesign.md](../EngineDesign.md). Rationale notes live in [CLAUDE.md](../CLAUDE.md).

## Object Model

- **No garbage collector.** Native lifetime is explicit. Gameplay object graphs are the .NET GC's job, on the C# side.
- **Generational handles are the canonical reference to engine objects** (entities, components, assets), in C++, in C#, on the wire, on disk. A handle is a POD stable ID, validated on dereference: `TryGet(handle)` returns a pointer or null. Handles never own; destruction is deterministic and invalidates all outstanding handles immediately.
  - Backings: **slot + generation** for pooled simulation objects; **ID table** for tree-owned objects (e.g. UI widgets), registered/unregistered by the object's constructor/destructor.
  - A field that references an object it does not own is a handle. The `TryGet` result is a transient borrow, used in scope, never stored.
- **The simulation is a full ECS**: entities are handles; components are concrete value types in contiguous per-type storage; behavior is composition. The first ECS iteration is in place; pooled storage, generational handles, and system phases are not built yet. No runtime polymorphism in component storage; struct inheritance for field reuse is allowed.
- **Not everything is ECS.** Retained object trees with runtime polymorphism are the correct model for some domains (UI is the anticipated case): parent owns children, cross-references are handles.
- A simple-game **facade** over the ECS may exist later. It owns no state; engine systems see only ECS state.
- **Reference vocabulary in reflected data:**

  | Field type | Meaning | Serializes as |
  |---|---|---|
  | by value / `unique_ptr<T>` | Owned | Inline, at the owner |
  | `Handle<T>` | Non-owning reference to an engine object | Stable object ID |
  | `AssetRef<T>` | Asset reference | Asset GUID |
  | `Poly<T>` | Owned, polymorphic (boxed) | Type discriminator + fields |

  `Poly<T>` captures the concrete type's `TypeInfo` at the point of erasure (templated constructor), no RTTI, no common base class required.

## World Space & Transforms

- **Metric, SI.** 1 unit = 1 meter; seconds, kilograms. Angles are **radians** in code, degrees only at UI and authoring edges.
- **Positions are `float`.** Roughly 1 mm of precision at 8 km from the origin is the accepted limit. Double precision and origin rebasing for large worlds are deferred, not designed against.
- **Right-handed, Z up:**

  | Axis | Direction |
  |---|---|
  | +X | Right |
  | +Y | Forward |
  | +Z | Up |

  The right-hand rule applies twice: fingers along the first axis curled toward the second give the third as the thumb (`cross(X, Y) = Z`), and the thumb along a rotation axis gives the positive angle as the curl. Equivalently, positive rotation carries the next axis into the one after it, cycling X, Y, Z, X.
- **Rotation names:** yaw about Z (+X toward +Y, so positive yaw turns left when facing +Y), pitch about X (positive is nose up), roll about Y (positive banks right).
- Chosen over Y-up because the ground plane is XY (navigation, spatial partitioning, terrain, replication interest management), yaw is the rotation that gets replicated, and it matches Blender and the physics literature. Importers convert at the boundary, so the conversion never reaches gameplay code.

### View & Clip Space

World space is the engine's; view and clip space belong to the graphics API, and the two builders in
`PlaygroundEngine.Renderer.View` are the only place either convention is written down.

- **View space is +X right, +Y up, -Z forward**, the graphics convention, so `MakeWorldToViewMatrix` is
  where the Z-up world basis changes. Nothing else performs that swizzle, and a model authored Z-up needs
  no fixup.
- **Clip space is Vulkan's**: Y points down and depth runs 0..1, both produced directly by
  `MakePerspectiveProjectionMatrix` rather than corrected afterwards. A second backend replaces that one
  function.
- **Depth is not reversed** and the far plane is finite. Reversed-Z is the expected change once depth
  precision matters, and it is confined to the same function.
- **The aspect ratio belongs to the render target, not the camera.** A camera authors a vertical field of
  view; the consumer combines it with the extent it is drawing into.
- **The simulation ships a description, never a matrix.** `ExtractedView` carries a position, a rotation and
  a lens, so replication, the C# boundary and future render interpolation all get an authored value, and
  a change of clip convention stays inside the renderer.

### Transform

- **The engine owns its math types** (`Vector2/3/4`, `Quaternion`, `Matrix4x4`, in `PlaygroundEngine.Math`) rather than aliasing a library's. A class template specialization has no reflected identifier, so a field of one reflects nameless and gives serialization and the C# boundary nothing to key on; an alias also cannot carry `Vector3::Up`, cannot be annotated, and does not export its library's operators across a module boundary. A third-party library stays available behind these types for heavy operations (decomposition, slerp, projection).
- **`Transform` is a decomposed value type, always world space.** `TransformComponent` derives from it, so a system writes `component->Position`; that wrapper is transitional and disappears when components stop needing a polymorphic base, at which point `Transform` is itself the component.

  | Field | Type | Default |
  |---|---|---|
  | Position | 3-vector | (0, 0, 0) |
  | Rotation | quaternion | identity |
  | Scale | 3-vector | (1, 1, 1) |

- **No scene hierarchy.** Nothing composes transforms, so the parent-scale-times-child-rotation shear case cannot arise and non-uniform scale is safe. Adding a hierarchy means revisiting non-uniform scale first.
- **Composition order is `M = T * R * S`**: scale in the object's own frame, then rotate, then translate. Any other order scales the translation or stretches along world axes instead of the object's.
- **Column vectors** (`v' = M * v`), so `A * B` applies B first, and `Matrix4x4` stores four columns with the translation in the last one.
- **The matrix is derived, never stored.** Building it is on the order of 40 flops, and a cached one would be an invariant, which contradicts components being open data written field-wise by the debug panel, serialization, replication, and the C# bindings. If profiling ever demands caching, the cache belongs in a system-owned per-frame array, not in the component.
- **The quaternion is the stored truth**; `EulerAngles` (pitch, yaw, roll, in radians) is a display and authoring form only, never a runtime representation. The composition is **yaw about world Z, then pitch about the yawed right axis, then roll about the resulting forward axis**, spelled `Yaw * Pitch * Roll` since the right operand applies first; a different order names a different rotation from the same three numbers, so it is part of the authoring contract.
- Quaternion to Euler is not unique, so a triple recovered from a rotation need not be the one authored, and at a pitch of +/-90 degrees yaw and roll collapse onto one axis, where the whole turn is reported as yaw with zero roll and pitch is set to exactly vertical. Editing UI therefore holds its own triple while a widget is active and rereads only when it is not; the write is never deferred, so what the rotation drives follows the edit live.
- Non-uniform scale obliges the renderer to transform normals by the inverse-transpose of the upper 3x3, and will need constraining at the physics boundary, where primitive collision shapes do not scale non-uniformly.
- Panels present a vector or a rotation as **one row of X/Y/Z components**, not a subtree, and a rotation's boxes are ordered by axis rather than by name so they align with the position and scale rows.
- **2D uses the same `Transform`**, as a usage convention (a working plane plus an orthographic camera), not a separate type or a parallel node tree. The world stays metric; sprites carry a pixels-per-unit setting at import.

## Ownership & Memory

- **Ownership is a tree.** Every object has one serialization home; every other mention is an ID. Assets are never serialized inline, always referenced by GUID.
- **Smart pointers are for plumbing, not gameplay references.** `unique_ptr` is the default owner. `shared_ptr` is rare and justified case-by-case (shared non-entity resources, e.g. asset payloads pinned by in-flight GPU work); `weak_ptr` only alongside those. Raw pointers/references are transient borrows. `shared_ptr` does not appear in reflected data.
- **Entities and components are not created with `make_unique`**: pools construct in place and return a handle.
- **Memory funnel, two entry points, one allocator:**
  - *Ambient:* a global `operator new`/`delete` override (eventual) routes all plain allocations, `std` containers, `make_unique`, third-party, through the engine allocator with tracking and budgets.
  - *Deliberate:* `std::pmr` memory resources where the allocation pattern is a design property (per-frame arenas, asset streaming, anything allocating inside the frame loop). pmr upstreams chain to the same engine allocator.
  - *Structural:* ECS chunk storage and GPU memory are allocator code themselves and bypass `new`.
- **The day-one requirement is the seam, not the pools**: pattern-sensitive systems take a `memory_resource*` (defaulted to the global resource), and reflection-driven construction is placement-agnostic, factories return by value or accept a placement target. Plain `new` is a valid default because it is engine-defined.
- Plain `std` containers by default; `pmr` only where the pattern matters. Game code never sees allocators. Scratch arenas are thread-local (the engine is multithread-first).

## Native/Managed Boundary

- Engine objects cross the boundary **as handles only**. C# wrappers are value types: no native pointers, no finalizers, no `Dispose`. Liveness API: `IsValid`/`TryGet`.
- Native→managed callbacks go through explicit subscription objects owning GCHandles, unregistered deterministically with the subscriber's lifetime.
- Generated bindings translate error models: native `std::expected` ⇄ C# exceptions. Exceptions never cross the boundary in either direction.
- Performance goal: C# accesses component data through layout-matched structs viewing native memory directly, not per-field marshaling.

## Standard Library

Use `std` directly, no engine wrapper aliases. Components are replaced individually when profiling demands it.

Deny-list for runtime code:

| Avoid | Use instead |
|---|---|
| `std::regex` | anything else |
| iostreams | `std::format`/`std::print`, spdlog |
| `map`, `set`, `list` | `vector` (sorted if needed) |
| `unordered_map`/`unordered_set` in hot paths | open-addressing map (adopt when profiling shows it) |
| `std::function` in hot paths, `std::async` | templates/`function_ref`; engine job system |
| Throwing forms: `at()`, `stoi`/`stof`, value `any_cast` | indexing + checks, `from_chars`, pointer `any_cast` |

## Error Handling

Zones split by **"does this native code ship in the console runtime?"**:

| Zone | Error model |
|---|---|
| C# (all, including shipping console builds) | Exceptions, idiomatic. Native errors arrive as thrown exceptions via bindings. |
| Native runtime code | `std::expected` / error codes / asserts. Must build and behave identically under `-fno-exceptions`. |
| Native tooling-only code (editor, importers, build tools) | Exceptions permitted; caught at module edges, converted to the engine error model. |

- Exploration builds compile with exceptions ON; runtime code must not depend on them. Shipping runtime binaries target `-fno-exceptions`.
- Exceptions never cross an exceptions-on/off boundary or the native/managed boundary.
- Code migrating from tooling to runtime gets its error handling rewritten at that moment.
- **RTTI:** engine correctness is independent of RTTI (`-fno-rtti` stays viable).

## Open Questions

- **Handle granularity**, per-entity only, or per-component too. Belongs to the simulation design.
- **One handle scheme or siblings**, whether asset GUIDs and entity handles share an identity scheme. Tied to the stable-ID design in [ReflectionSystem.md](ReflectionSystem.md).
- **C# event subscription lifetime**, needs a focused pass when binding generator work starts.
- **2D working plane and sort axis**, chosen when the 2D render path exists. Skew stays out until 2D authoring is real.
- **Drawing foreign types in a debug panel.** A field of a type the engine does not own (a `std::pair`, a third-party struct) cannot carry a `DrawDebug` annotation, and annotations are required at every level, so it renders as nothing. Deferred; see the per-type customization question in [ReflectionSystem.md](ReflectionSystem.md).
