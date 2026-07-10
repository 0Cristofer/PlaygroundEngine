# Application Architecture

Status: the general architecture described here is settled. Everything below that level (band contents, class structure, API shapes, phase sets) is not; see "What is settled" below. Parent document: [EngineDesign.md](../EngineDesign.md). Engine-wide conventions live in [CoreConventions.md](CoreConventions.md); the reflection substrate in [ReflectionSystem.md](ReflectionSystem.md).

## The architecture

The engine is organized as five dependency bands with a single composition root at the top:

| Band | Role |
|---|---|
| L4 Application | Game and app code (C# eventually). `AppDescriptor` implementations. |
| L3 Framework | Composition root, frame loop, capability wiring. The only band that sees the whole graph. |
| L2 Systems | Engine systems as peers: input, renderer, simulation, assets, networking, audio. |
| L1 Platform | OS seams: window backends, surfaces, event pump, file I/O, time. |
| L0 Core | Log, math, containers, handles, error types, reflection, POD contract types. |

The organizing decisions:

1. **Imports point downward only.** L2 systems never import each other; peers meet through data contracts that live in a lower band (event PODs, command queues, extracted render data, handles). C++ module imports make the rule mechanical: the import graph is the architecture, and a violation is a visible diff or a build-breaking cycle.
2. **The `Engine` (L3) is the composition root.** It constructs systems in explicit dependency order with constructor injection, driven by the capabilities the `AppDescriptor` declares. No globals, no service locator, no DI container in native code. Every cross-system edge is written down in root-owned code, in one file per target.
3. **Every capability is optional.** Window, rendering, audio, and networking are flags the app declares; the root builds only what is declared. A dedicated server, the test harness, and a cook tool are different root configurations, not conditional compilation inside systems. The acceptance test: the dedicated server has zero `#if SERVER` in any system.
4. **The frame loop is hand-authored code in L3.** Frame ordering is the contract, so it is written as literal calls, not derived from registration priorities. There is no `ISystem` base and no generic for-each-system update.
5. **Systems interconnect through exactly three mechanisms**, one per kind of coupling (detailed below): constructor injection at boot, POD data seams at frame time, typed signals for rare notifications.
6. **Extension is closed core, open rim.** The engine's own systems are a closed compile-time set wired concretely. Game-added systems register into named schedule phases and subscribe to published signals and queues through a wiring context; that narrow published surface is the only thing games can touch.

A *system* is a vertical slice spanning bands: for input, contract types at L0, producers at L1, semantic mapping at L2, a schedule position at L3. The peer-isolation rule applies between systems, not between every pair of modules inside one slice.

### Provisional band assignment

Current and near-future modules, provisional except where a placement is called out as settled:

- **L4:** `PlaygroundGame`.
- **L3:** `PlaygroundEngine` umbrella (`Engine`, `AppDescriptorBase`, `CommandLine`, `main.cpp`), `PlaygroundEngine.App`. The umbrella currently mixes the root with convenience re-exports of L2; whether the root splits into its own module is an open cleanup.
- **L2:** `.World` (placeholder; the ECS `World` replacing it stays L2), `.GameObject` and `.Components` (placeholder, do not build on). Future: InputSystem, Renderer with Viewport and RenderTarget, AssetSystem core, Networking, Audio.
- **L1:** `.Window` (`:common` contracts, CMake-selected per-platform `:backend`; the template for all L1 seams). Future: the platform event pump, file I/O, time, file watching.
- **L0:** `.Log`, `.Reflection` (settled by its own design doc; the registry *instance* is owned by L3, which schedules its mutation barriers). Future: input event and command POD contracts, ECS storage primitives, handles, error enums, math.

Placement calls already settled: the asset system splits (runtime core at L2, file I/O and watching at L1, hot reload wired at L3, the import pipeline is editor tooling outside the runtime cake); ECS storage primitives (chunk storage, entity allocator, handle types) are L0 containers usable without a `World`, while the `World` itself is the L2 system.

## What is settled, what is not

**Settled:** the band structure and its rules, the composition-root pattern, capability-driven composition, the hand-authored frame loop, the three interconnection mechanisms, the closed-core/open-rim extension model, the editor and play model (editor as an L4 application, play always out of process, live-edit protocol as a dev facility), the build-configuration branching rule, the two-option distribution model (prebuilt binaries for C#-only projects, source-built binaries for native projects), and the rejected alternatives listed at the end.

**Not settled:** everything inside that frame. Specifically:

- The exact contents of each band. The band assignment table below is provisional, not a contract.
- Class structure and API shapes. Everything sketched during exploration (the capability struct, the wiring context, queue types, the boot sequence, the frame loop body) is expected to change when implemented.
- Which schedule phases exist, their granularity, and within-phase ordering.
- The C# boundary surface for descriptors, phases, and signals.
- Per-system extension APIs; each is designed with its system.
- The live-edit protocol: message shapes, transport details, the agent-facing tool surface the editor exposes on top of it.
- Editor UI sequencing: whether the editor dogfoods the engine's retained UI system from the start or begins on a disposable framework while that system matures.
- The source-versus-derived asset split mechanics (import pipeline, derived-data cache).
- The developer debugging workflow for out-of-process play: a wait-for-debugger spawn option and an editor-attaches-to-running-game mode are the likely baseline, IDE integration the eventual polish; dual-runtime (native plus embedded .NET) debugging is the hard part regardless of process model.
- Fixed-timestep placement (loop, `World`, or per-system) and render interpolation.
- Render extraction shape (snapshot versus double-buffered), tied to the multithread-first rule and the future job system.
- Multi-window focus and hit-test routing once more than one window exists.
- Lifetime scopes (app versus world/match) for C# game services.
- The project manifest format for the generic host, and C# AOT for consoles.

When implementing, treat the six decisions above as constraints and everything else as open design work.

## The three interconnection mechanisms

**Construction and lifetime: the composition root.** Systems are constructed bottom-up by band, receive their dependencies as constructor parameters, and are torn down in reverse order. References established at construction are borrowed pointers or references, valid for the system's whole life because the root guarantees strictly nested lifetimes. Handles are the canonical reference to engine *objects* (entities, assets, windows, viewports); there are no handles to the singleton systems themselves.

**Frame-time data flow: data seams plus the explicit loop.** Everything that crosses a system boundary at frame time is a POD or a generational handle, moved through queues or extracted data, never a method call into a peer. This one rule makes seam data serializable, replicable, injectable in tests, and C#-crossable without per-system adapters. The loop pumps the platform, drains input into semantic commands, steps the simulation, and renders; the simulation only ever consumes commands, so it cannot tell local input from replicated input.

**Rare notifications: typed signals.** Resize, device lost, asset reloaded. Signals are owned by the emitting system, carry POD payloads, and are subscribed at wiring time in the composition root (or in the game's one composition function). Subscriptions are RAII objects. Signals fire only from defined drain points in the frame, never from inside OS callbacks. There is no global event bus.

## Capabilities and targets

The app descriptor declares plain-data capability flags (presentation, rendering, audio, networking); the root interprets them. Rendering and presentation are independent: an offline GPU job is rendering without presentation. The current targets and their shapes:

| Target | Capabilities | Loop shape |
|---|---|---|
| Desktop game | presentation, rendering, audio, optionally networking | Frame loop |
| Dedicated server | networking only | Fixed tick: receive, step, send |
| Test harness | none | None; each test composes the objects it needs directly |
| Cook tool | optionally rendering | Work queue over jobs |
| Editor | presentation (multi-window), rendering, asset import | Edit loop over a non-simulating preview world; play spawns the game target as a child process |

## The extension model

Game-added systems get three things, each an extension of an existing framework piece:

1. **A place to run:** named schedule phases (pre-simulation, simulation, post-simulation, pre-render) that accept registered callbacks. Phase callbacks must not assume main-thread execution; threading is a documented property of each phase.
2. **A place to wire:** `AppBase::OnInitialized` receives a wiring context through which the game registers phase callbacks and subscribes to published signals and queues.
3. **Lifetime:** the root constructs game systems last and tears them down first.

The published surface is deliberately narrow because whatever a game can touch becomes frozen API. Games consume semantic input actions, never raw OS events. Deep customization (custom render passes, viewport handling) goes through per-system extension APIs designed with each system, never through subscription to a system's internal events.

In the C# game band, a DI container is acceptable for the game's own service layer (analytics, backend, shop). Engine systems are constructed before any game code runs and are not resolvable services, and the engine's C# binding surface never requires a container.

## The editor and play model

The asset authoring tool is an L4 application, not an engine build configuration. It is its own target in the same repo and build, links the engine as a library, and runs under its own composition root. The `WITH_EDITOR` model (editor concerns compiled into runtime types and systems) is rejected; it is the same disease as `#if SERVER`.

- **Authoring data is the editor's truth.** The world the editor holds is a derived, non-simulating preview constructed from asset data, re-derivable at any time, never saved itself. Edits mutate authoring data first; the preview refreshes.
- **Runtime types carry zero editor state.** Editor metadata (gizmo shapes, thumbnails, import settings, categories) lives in editor-owned side tables keyed by `TypeInfo` and handles. Shipping without the editor is not a strip step; the data never existed in runtime types.
- **The editor is built on the public rim.** Selection, gizmo, and tool systems register into phases and use per-system extension APIs, the same surface games get. Editing gizmos are editor-side systems that exist in no game configuration; they are distinct from gameplay debug visualization (a dev facility, below), though both share the drawing facility.
- **Play always runs out of process.** The editor spawns the real game target as a child process; there is no in-process play-in-editor. The game under test is the shipped composition, a child crash loses nothing because truth is editor-side, and the play-in-editor-versus-standalone divergence bug class cannot exist.
- **The live-edit protocol** rides the replication substrate: reflection-driven property access, handles as wire identifiers, `InputCommand` PODs pushed into the same command queue (the `World` cannot tell editor input from local or replicated input). Inspection is interest-managed: hierarchy skeleton at low frequency, full component data only for the selection. Write-back applies at frame boundaries through the command queue and the reflection registry's mutation barriers. Editor and child verify compatibility at connect time with an engine and protocol version handshake; the prebuilt distribution gets exact build match as the stricter guarantee (see Distribution model). The same protocol pointed at a devkit or device instead of a child process is remote live editing. Only reflected state is visible; system internals stay invisible by design.
- **The game-side protocol endpoint is a dev facility and a rim citizen.** Dev-build roots construct it only when activated by command line (the editor passes the flag and socket at spawn; a dev build launched normally runs without it). No engine system references it. Shipping builds do not link it: a read/write protocol into game memory is a cheating and attack surface, so absence means the code is not in the binary. Default transport is a local socket with a spawn token; TCP is an explicit opt-in for remote targets.
- **Editor launch differs by parameters, never by identity.** There is no launched-by-editor flag; the game must not know who started it. Each behavioral difference is an explicit command-line parameter with a standalone default flowing through the existing `CommandLine`-to-descriptor path: the startup world comes from the app descriptor unless `--world=<reference>` overrides it, and the editor passes the open world by serializing a session snapshot of current authoring state and pointing `--world=` at it. The child boots through the one normal world-loading path and cannot tell a session snapshot from shipped content, so every play test exercises real deserialization. Every editor-spawn parameter must also work from a plain terminal (the editor shows the child's full command line for reproduction under a debugger).
- **Agents enter through the editor.** The wire protocol stays client-agnostic; the editor re-exposes curated, semantic, undoable operations on top of it (the natural agent-tool surface). Headless drivers (CI, training backends) speak the protocol directly. Nothing agent-specific enters the engine.

## Distribution model

The engine ships two ways, mirroring UE's launcher-versus-source duality:

1. **Editor plus scripting (C# and visual scripting).** All binaries are prebuilt engine artifacts; the game is C# assemblies, visual scripts, and assets. No native toolchain required.
2. **Editor plus native game plus scripting.** The project compiles its own game and server executables against engine source, adding native systems as needed. Game-native code links the engine library; modifying the engine itself means rebuilding the editor too, since the editor links the same engine.

- **Artifacts are targets crossed with build configurations:** targets {editor, game, dedicated server, cooker} times configs {dev, shipping}, minus cells that do not exist (the editor ships only in dev config). Dev binaries link the dev facilities (live-edit endpoint, debug draw, profiling); shipping binaries do not contain them, so only dev-config binaries can participate in editor play, by construction.
- **The stock game and server binaries are the engine plus a generic host:** an L4 application whose descriptor is data-driven. It reads the project manifest (capabilities, startup world, C# assemblies to load), boots the .NET runtime, and hands the lifecycle to the C# `AppBase`. This is how an `AppDescriptor` is authored in C#: manifest plus managed code, with no game-written native descriptor.
- **The editor launches a slot, not a binary.** Play spawns the project-designated game binary in dev config: the stock host for C#-only projects, the user-compiled executable for native projects. The editor has no branch between the two; its entire contract with the child is the spawn parameters and the live-edit protocol, checked at connect by the version handshake.
- **Fork surface, for the native option:** the escalation ladder is designed extension APIs (zero fork), then replacing whole systems at the composition root (a shallow, trivially mergeable fork, possible only because systems have one birthplace and no globals), then in-place patches to system internals as the irreducible tail.
- **Known hard edge, flagged rather than designed:** the C#-only option on consoles requires AOT compilation, since consoles prohibit JIT.

## Build-configuration branching

Systems never ask who is running them: target identity (game, server, editor) is expressed only by composition. Build configuration (dev versus shipping, the existing `PGE_DEV`/`PGE_RELEASE` axis) is the one legitimate branching axis; it selects the presence of facilities, not the behavior of systems, and it is confined to two places:

1. **The composition seam.** Dev roots construct facilities (debug draw, live-edit server, profiling) that shipping roots do not; dev-only game systems are registered inside one conditional block in the composition function.
2. **Call-site facades.** Dev-facing calls inside shipping code paths go through one seam that compiles to nothing in shipping, like the existing `LOG_*` macros; call sites never branch themselves.

## Ambient facilities: the bounded exception to no-globals

Logging and profiling are not systems; they are ambient L0 facilities reached through their macro facades (`LOG_*`, future profiling scopes), not injected. The no-globals rule governs dependency edges that carry information; an edge every system has carries none. Admission criteria, all required:

1. **Fire-and-forget:** callers only write into it, nothing reads program state back, so it can never become a channel between systems.
2. **Presence never varies, only configuration:** every target has it; the root configures sinks and levels, and dev-only facilities compile to nothing in shipping via the facade rule.
3. **No lifetime:** constant-initialized, valid before `Boot()` and after `Shutdown()` begins; the root configures first and flushes last but never creates it.
4. **L0, importing no engine concepts.**

Anything failing a criterion is a system and gets wired: RNG fails (replication needs seeded, world-owned streams), the job system fails (lifetime, schedule position), time fails (frame time is `FrameContext` data). Consequence for logging: no `Log::Init()`; the default logger is constant-initialized and `Configure` only swaps sinks and levels. Per-system filtering comes from a category parameter on the macros, not from injection; tests capture output by swapping the sink.

## Use cases: architecture stress tests

Each of these is a use case the architecture must support as designed. Failing one means the design broke and gets revisited, not excepted.

1. **Dedicated server:** a different root configuration with zero `#if SERVER` in any system. Grep for target-identity checks inside systems returns nothing.
2. **Dev-facility confinement:** grep for `PGE_DEV` hits only composition roots and facility seams, never the middle of simulation or renderer logic.
3. **Editor on the rim:** the editor is buildable entirely from phases, published signals and queues, per-system extension APIs, and reflection. If the rim cannot build the editor, it cannot support ambitious games.
4. **Multiple worlds:** several `World` instances coexist in one process (editor preview, in-process client-plus-server tests).
5. **Viewport without a window:** a viewport renders to a texture target composited by editor UI, validating the Window/Viewport split.
6. **Cheap play:** starting play from the editor is constructing a fresh `World` from authoring data, fast enough to do constantly.
7. **Generic remote inspection:** a running game is inspectable and live-editable through reflection and op-tables alone; this doubles as the integration test of serialization, replication, and the C# binding contract.
8. **Indistinguishable input:** an agent, replay harness, or editor drives the game by pushing `InputCommand` PODs into the command queue, and the `World` cannot tell them from local input.

## Rejected alternatives

- **Service locator and globals** (`GEngine`, `gEnv` style): hidden dependencies, discovered missing at runtime on headless and tool targets.
- **`ISystem` base with a priority-sorted update loop:** frame ordering becomes an emergent property of registration numbers instead of authored code. This rejects a generic loop for the engine's own closed set; game systems register into named phases instead.
- **Global event bus:** makes every system a potential coupling of every other; flow and delivery order become untraceable.
- **DI container or autowiring in native engine wiring:** moves dependency errors from compile time to startup and hides construction order behind resolution. The bounded C# game-band exception is in the extension model.
- **Window-is-a-Viewport inheritance** (Godot): conflates two orthogonal capabilities; composition over a `RenderTarget` covers render-to-texture and UI-only windows.
- **Runtime-polymorphic platform backends:** backend choice is compile time (CMake-selected implementation partition, concept-checked, no vtable).
- **Editor as an engine build configuration** (the `WITH_EDITOR` mold): compiles target identity into runtime types and systems; the editor is an L4 application instead.
