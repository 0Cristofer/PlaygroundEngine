# WindowServer

Status: implemented, for the "now" rows of the ownership table. The boundary rule, the ownership split, and the event record's shape are settled. The interfaces below are what shipped; see [Implementation notes](#implementation-notes) for where they differ from the original sketch, and [Corrections during design](#corrections-during-design) for conclusions that would otherwise get re-derived. This is now the single source for this slice: the session handoff that used to accompany it is deleted. Parent document: [ApplicationArchitecture.md](ApplicationArchitecture.md). Engine-wide conventions live in [CoreConventions.md](CoreConventions.md). The dev overlay that consumes this first is sketched in [DebugUISketch.md](DebugUISketch.md), parts of which this document overrides (see "Overrides").

## Definition

> **`WindowServer` is the engine's connection to the OS window system, and it owns everything that must be performed over that connection.**

Everything above it is derived. The record it produces is the only part of this slice that cannot be reconstructed: a state table is a pure fold over the record, semantic actions are a mapping over that, and both can be rewritten against an unchanged record. Information dropped at the record is gone for every layer above.

## What this is not

It is not the input system. It does not know about actions, bindings, or players. It does not maintain key state. It does not decide what an editing command is, what a repeat means, or which events matter. It classifies nothing and filters nothing.

That paragraph is the primary defense against this becoming Godot's `DisplayServer`, a ~200-method singleton covering windows, screens, cursors, clipboard, IME, dialogs, and text-to-speech. The membership rule below admits most of that list correctly, so the rule alone is not sufficient protection.

## Membership: two orthogonal mechanisms

"Platform-specific" is not the criterion. If it were, file I/O, time, sockets, threads, audio, process spawning, and locale would all qualify, and this becomes a god object. Two separate mechanisms were being conflated:

**Backend partition.** Compile-time implementation selection. *Everything* platform-specific gets one: networking, file I/O, time, the window system. This is the pattern `WindowServer` uses, and it is what [ApplicationArchitecture.md](ApplicationArchitecture.md) mandates in place of runtime-polymorphic backends. It answers "this code differs per OS."

**Session object.** A runtime stateful connection. Admitted only when all of the following hold:

1. The platform requires an explicit connect/disconnect lifetime.
2. Per-object operations depend on that connection being alive.
3. The connection carries state that outlives any single object created through it (event ordering, focus, atoms, registries).
4. The composition root constructs it, rather than it being an implementation detail internal to one system.

Verdicts:

| | Session? | Why |
|---|---|---|
| Window system | **yes** | `wl_display` / X11 `Display*` / Win32 message queue; carries focus and event ordering |
| Audio device | **yes** | Own connection (PipeWire, WASAPI), own lifetime, unrelated to display |
| Networking | no | `WSAStartup` is an init flag; no state outlives a socket |
| File I/O | no | Each open is independent |
| Time, threads | no | Stateless |
| `io_uring` / IOCP | not here | A session, but internal to the file system, not an L1 peer the root wires (criterion 4) |

An earlier formulation of this rule was "operations multiplexed over one shared channel." It was rejected: under async I/O, `io_uring` is literally a shared submission ring multiplexing every operation, which flips networking and file I/O into session objects and destroys the rule's own headline counterexample. Criterion 4 is what keeps the verdicts stable.

### Naming

`Platform` was rejected: it over-claims, so networking's absence from it reads as an inconsistency rather than as the normal case. `Display` and `DisplayServer` were rejected because "display" reads as *monitor* outside X11 and Wayland circles.

"Server" names the stateful-connection property that defines membership, so the name and the boundary rule are the same idea. macOS's actual process is `WindowServer`.

On consoles and headless targets there is no window system in the desktop sense. `WindowServer` there is the host compositor and system session: the video-out handle plus the system-service event pump.

## Ownership

| | Status | Why it is here |
|---|---|---|
| Connection lifetime | now | Is the connection |
| Window creation, destruction, properties | now | Objects on the connection |
| Event pump and the ordered record | now | All window-system traffic arrives on it |
| Window events (resize, focus, close) | now | Same channel |
| Keyboard, pointer, text events | now | Same channel (Wayland `wl_seat`, X11, Win32 message queue) |
| Vulkan surface and instance extensions | now | Deliberate deviation, see below |
| Key display labels | later | Needs `glfwGetKeyName` / `xkb_state_key_get_utf8`, a connection query |
| Pointer lock and relative motion mode | later | A connection mode, not a cursor property |
| Monitors, DPI, refresh rate | later | Enumerated through the connection; hotplug makes it stateful |
| Cursor shape and visibility | later | Connection operations |
| Clipboard | later | `wl_data_device` / X11 selections |
| IME composition | later | Per-surface protocol on the connection |
| Drag and drop | later | Same data-transfer mechanism as clipboard |

Explicitly **not** owned:

- **Gamepads and raw HID.** evdev and XInput involve no window-system connection. A separate session, and the reason their events carry no window identity.
- **Audio.** Its own connection and lifetime. Windows' per-application volume grouping is per *audio session*, which defaults to per *process*, not per window; the two subsystems share no channel. Policies that correlate them ("mute on focus loss") live above both, reading a focus event and calling an audio API.
- **Networking, file I/O, time, threads.** Backend partition, no session.
- **GPU properties.** Behind the Vulkan instance; the renderer's business.
- **Input state, bindings, semantic actions.** Derived layers above.

### Vulkan surface creation: a deliberate deviation

Creating a `VkSurfaceKHR` needs the native window handle, not our session object, so by the membership rule it does not belong here. GLFW conflates the two, which is why it looks like a window-system operation.

It stays anyway, in the *required* backend concept, for exploration-phase pragmatism. The correct seam is for `WindowServer` to vend an opaque `NativeWindowHandle` (display pointer, surface pointer, platform tag) and for the renderer's own platform partition to call `vkCreateWaylandSurfaceKHR` and friends. The cost today is that GLFW only exposes native handles behind `GLFW_EXPOSE_NATIVE_*` and `glfw3native.h`, so the backend would gain conditionally compiled platform code plus runtime X11-versus-Wayland detection, and the instance extension list would be hand-built instead of coming from `glfwGetRequiredInstanceExtensions`. That is reimplementing the part GLFW exists to do.

Keeping it in the *required* concept is deliberate: when a second backend arrives, the hard compile error forces the decision to be made explicitly, at the new backend's definition, rather than deferring it to a confusing instantiation error inside the renderer.

`GetRequiredVulkanExtensions` moves from `Window` to `WindowServer`, since it is per-connection rather than per-window.

## The event record

One ordered buffer of flat PODs. No union, no variant, no polymorphism.

```cpp
export enum class PlatformEventType : std::uint8_t
{
    KeyPressed, KeyReleased, CharacterTyped,
    PointerMoved, PointerMovedRelative, PointerButtonPressed, PointerButtonReleased, PointerScrolled,
    FocusGained, FocusLost, WindowResized, CloseRequested,
};

export struct PlatformEvent
{
    PlatformEventType Type;
    InputCode Code = InputCode::Unknown;
    InputModifiers Modifiers;
    InputLockState Locks;
    bool Repeat = false;
    PlatformKeyToken Token;
    char32_t Codepoint = 0;
    float X = 0.0f, Y = 0.0f;
    std::uint64_t Timestamp = 0;
};
```

### Why one ordered buffer

Cross-kind order is semantically meaningful to the engine itself, not just to one consumer:

- A pointer move before a button press determines where the click landed.
- A modifier press before a click determines whether it was a shift-click.
- A click before typing determines which field has focus.
- A character between two editing keys determines the resulting text (typing `a`, Backspace, `b` must yield `b`, and separate unordered buffers make that unrecoverable).

Separate per-kind queues would force reconstructing a merge key, which means building the ordered record anyway, worse. ImGui's input-event trickling also depends on true arrival order, but that is corroboration, not the reason; the argument above survives ImGui's replacement.

### Why buffer at all, rather than dispatching from callbacks

[ApplicationArchitecture.md](ApplicationArchitecture.md) requires that signals fire only at defined drain points, never from inside OS callbacks. Unreal reached the same rule the hard way: `FWindowsApplication` maintains a deferred message queue (`DeferMessage` / `ProcessDeferredEvents`) specifically because Win32 re-enters the window procedure during modal loops.

Consequence for the GLFW backend: callbacks append to the buffer and do nothing else.

### Lifetime and reads

The record is a **per-pump immutable batch**. The root clears it, each producer's `Pump()` appends to it, and afterwards it is read-only for the rest of the frame. Consumers read a `std::span` and never drain it, so several can read the same batch.

Non-destructive reads do **not** mean events survive across pumps. A consumer that skips a pump loses that batch. The obligation therefore falls on the layer above: **the input state layer folds every pump, unconditionally**, even when the consumers it serves are not ticking. It accumulates, and those consumers clear edges at their own boundary. If the state layer is ever made conditional, input is silently dropped.

There are no read cursors at the record level. Cursors are a state-layer concept.

**The root owns the record**, clears it once a frame, and hands it to each producer to append to: `windowServer->Pump(record)`. Producers never clear it, or a second producer would erase the first one's batch. This costs one parameter and buys three things. A test can construct a record, fill it with hand-written events, and drive the entire state layer with no window, no GLFW, and no `Presentation` capability. Replay becomes a producer swap at the root rather than mid-frame injection into another object's buffer, consistent with the fork-surface ladder in the architecture document. And a second producer (device input) later appends to the same record with no restructuring.

Storage is a growable vector, reserved to a sane capacity and cleared rather than deallocated, so it amortizes to zero allocations and needs no overflow policy. A fixed ring would not survive the modal-loop batch described under Threading.

### Field decisions

**`Timestamp`.** Required, not optional. Correct fixed-timestep input assigns each event to the tick during which it occurred; without a time base, a 30Hz simulation with a 240Hz mouse dumps all motion onto one tick and none onto the others, producing judder and non-reproducible replay. GLFW does not expose event timestamps, so the backend stamps at pump time and the precision is documented as *arrival at pump*. Native backends supply real ones later (`GetMessageTime`, XI2 `time`, `wl_pointer` time, evdev `input_event.time`).

**Absolute and relative pointer motion are distinct event types.** "Last one wins" is correct for position; "sum" is correct for delta. A consumer cannot tell which rule applies from a shared `X, Y` pair plus one motion tag.

**Motion is never coalesced.** A 1000Hz mouse against a 60Hz pump yields up to sixteen motion events per pump, and motion will routinely outnumber everything else combined. Coalescing destroys the path, which high-precision aiming needs. Summaries are the state layer's job, computed by its fold for free.

**`Repeat` is carried and never filtered.** A text consumer must honor every repeat, since that is how held Backspace deletes a run. A gameplay consumer must honor only the first, or holding jump gives thirty jumps a second. Same events, opposite requirements, so the platform layer carries the flag and each consumer decides.

**Held modifiers and lock state are separate fields**, `InputModifiers` and `InputLockState`, because they have different information-theoretic status and only one of them justified the original single field.

`Shift`, `Control`, `Alt` and `Super` mean "held when this event occurred" and *are* derivable by folding the key stream. They are carried anyway, because a fold goes stale across focus loss (alt-tab while holding Shift) whereas the window system's own answer is right by construction. That is a convenience argument.

Caps Lock and Num Lock mean "latched", which is not a key being held and cannot be reconstructed from transitions at any cost: the initial value is unknown and the state can change while the application has no focus. This is the irreplaceable half, and the one the record exists to preserve. `GLFW_LOCK_KEY_MODS` is off by default and the backend must enable it at window creation, or the justification for `InputLockState` is void.

Merging the two, as SDL does in `SDL_Keymod`, makes `Modifiers.CapsLock` read exactly like `Modifiers.Shift` while meaning something else, which silently breaks any "Caps plus click" binding. The lock keys stay ordinary bindable keys through `InputCode::KeyCapsLock` and `KeyNumLock`, which is how a game binds one as a plain action.

**`PlatformKeyToken` is opaque**, meaningful only to the backend that produced it. GLFW documents its scancode as platform-specific and non-portable. It exists because a layout-dependent display name cannot be derived without it. Open question: since the record is a replay artifact, this field is either excluded from serialization or serialized alongside a backend identifier and treated as advisory. Decide before the first recording format exists.

**`InputCode` is one code space**, with ranges in the style of HID usage pages: keyboard, then pointer buttons, then a reserved range for gamepad buttons and analog axes. The reason is binding uniformity one layer up, not the saved struct field. A binding must say "Jump is Space, or Gamepad A, or Mouse4"; with split enums that becomes a variant, and every binding table, rebinding screen, and serialized binding file inherits it. Unreal reaches the same place with `FKey`. The cost is that `IsKeyboardKey`-style range predicates replace type-level separation.

Analog-versus-digital is a property of the range, so it is a compile-time range check rather than a side table. Unreal carries this in `FKeyDetails`, and the binding layer needs it immediately. Nothing in scope is analog yet (keyboard keys and pointer buttons are all digital; pointer position and scroll are event types with `X, Y`, not codes), so today this is only a reservation in the range layout.

**Key identity is physical position**, not the layout-dependent letter, so movement bindings stay under the same fingers on AZERTY. GLFW's key tokens are already physical, making this the cheaper translation too. The governing principle: *this is a raw event stream, transformations are additive information, but the original event data cannot be lost.* Corollary: because a display label needs a connection query, the label lookup is a `WindowServer` member, or the additive-transformation claim is false.

**There is no window identity field**, deliberately. With one window nothing reads it, and the retrofit cost is currently zero because no recordings, replay harness, or layout-matched C# structs exist to migrate. The first real consumer is the renderer, at the moment a second window has a second swapchain and a resize event must say which to recreate. When it is added, some categories will legitimately have none: device input, monitor hotplug, application lifecycle.

### Keys versus characters

They are one stream but two concepts, and the distinction is type-level rather than a categorization preference: **a key is stateful, a character is not.** A key has duration, so "is W down" has a well-defined answer at any instant. A character is a zero-duration insertion; "is `é` down" is meaningless.

Neither derives from the other:

- Shift, F5, and arrow keys produce no character.
- A dead key produces one character from two presses, with the first press producing nothing at the time. GLFW gives no signal that a dead key is armed, so the pending accent cannot be rendered.
- Key repeat produces many characters from one press; IME composition is many-to-many.
- A character carries no back-reference to the keystrokes that made it.

**Consuming text is a single forward pass** over the ordered record, with no lookahead and no gap inspection:

```
Character                  -> insert(codepoint)
KeyPressed in edit set     -> apply(key)
KeyPressed otherwise       -> ignore; its effect already arrived as a Character, or there was none
everything else            -> ignore
```

Typing `´` then `e` then `a` then Backspace yields `é` from twelve events, eight of which are ignored, with the dead key needing no special handling.

**The platform layer classifies nothing.** The editing-command set is mode-dependent: during IME composition, Enter commits a candidate and arrows navigate the candidate list, while outside composition they mean newline and caret movement. That mode is a UI concept, so key classification lives above.

Portability note: GLFW drops codepoints below 32, so Backspace is unreachable from its character stream. That is GLFW *policy*, not universal, since Win32 delivers `WM_CHAR` 0x08. Because backends disagree, text consumers must take editing keys from the key stream regardless.

Growth path: IME composition arrives later as composition-start, composition-update, and composition-commit event types. This is the most-regretted omission in this problem space; Unreal retrofitted `ITextInputMethodSystem` and still pays for it.

## Sub-facades

`WindowServer` vends cohesive sub-facades rather than being one flat class: `Monitors`, `Clipboard`, `Cursor`, `TextInput`. Each has its own backend concept.

This is not cosmetic. The "later" rows in the ownership table are, item for item, the contents of Godot's `DisplayServer`. Compile-time backends make the god-object outcome *worse* than Godot's, because a concept with two hundred requirements is far harder to satisfy than a vtable when a second platform arrives.

**A facade does not, by itself, defer anything.** If the server unconditionally holds a `Clipboard` that unconditionally holds a `ClipboardBackend`, that backend must exist on every platform; the implementation has moved to another file, not disappeared. The actual chain is: split concepts make absence *expressible* (`if constexpr (ClipboardCapable<Backend>)`), an optionality mechanism can then exist, and only then is bring-up incremental. What the split buys on its own is precise diagnostics and file-level separation for a port.

That leaves the mechanism to be chosen before the first facade is built, and one option is already excluded. An accessor that vanishes on unsupported platforms cannot have a stable C# binding, which the cross-runtime type model needs; a uniform surface returning `std::unexpected(NotSupported)` can. Note what that implies: the per-capability *concepts* are doing the portability work, and the facade classes are then a navigability decision about a large method surface, which is a separate question from this one.

**Windows are the exception, and sit on the server itself.** An earlier iteration had a `Windows` facade and it was removed. What every other entry on that list has in common is that it is a *deferrable capability*: a new backend can ship with no clipboard and no IME and still be a working window server, which is exactly what per-facade concepts buy. Windows are not deferrable, because they are the objects the connection exists to create and own. A backend that satisfies no window requirements is not a partially brought-up window server, it is not one at all. Godot's `DisplayServer` is bloated by the cursor, clipboard, IME, dialog and text-to-speech surface, not by `window_create`.

The practical cost of getting this wrong was `windowServer->GetWindows().Create(spec)` in place of `windowServer->CreateWindow(spec)`: one indirection and one type, to save nothing from a class that had two methods.

## Interface

```cpp
export class WindowServer
{
public:
    [[nodiscard]] static std::expected<std::unique_ptr<WindowServer>, WindowServerError> Create();
    ~WindowServer();

    WindowServer(const WindowServer&) = delete;
    WindowServer& operator=(const WindowServer&) = delete;

    [[nodiscard]] std::expected<Window*, WindowError> CreateWindow(const WindowSpecification& specification);
    void DestroyWindow(Window* window);

    // Drains the OS queue and appends it to the record. Does not clear: the root owns the
    // record and empties it. Main thread only.
    void Pump(PlatformEventRecord& record);

    [[nodiscard]] std::expected<std::span<const char* const>, VulkanWindowError> GetRequiredVulkanExtensions() const;
};
```

`Window` keeps only what is per-window: size, framebuffer size, title, surface creation. It loses `PollEvents`, `ShouldClose`, and `SetFramebufferResizedCallback`. Both its sizes are live queries: the specification records what creation asked for and stops being true at the first resize.

`WindowServer` owns windows as `unique_ptr` and hands out borrowed `Window*`. There is no handle table and no `TryGet`: with one window they would serve nobody, and [CoreConventions.md](CoreConventions.md) already names the two backing schemes a real handle would adopt later.

**There is no signal on `WindowServer`, and no `DispatchWindowEvents`.** An earlier iteration had both, per the architecture document's rule that signals belong to the emitting system. It was removed: with the root owning the record, the batch is already the data seam that document asks for at frame time, so a query over it (`PlatformEventRecord::HasEvent`) does the job that the rarer mechanism was doing. Typed signals remain the right answer for notifications with no frame-time batch behind them, and are deferred to their own piece of work rather than being designed in passing here.

## Consumption

```cpp
void Engine::RunFrame()
{
    _platformEvents.Clear();

    if (_windowServer)
    {
        _windowServer->Pump(_platformEvents);
    }

    if (_rendererVulkan && _platformEvents.HasEvent(PlatformEventType::WindowResized))
    {
        _rendererVulkan->NotifyFramebufferResized();
    }

    if (_platformEvents.HasEvent(PlatformEventType::CloseRequested))
    {
        RequestStop();
    }

    // Later: the debug overlay adapter and the input state layer both read
    // _platformEvents.GetEvents() as a read-only span.

    _world->Run();
}
```

Three consumption modes, all over the same batch:

1. **The root** asks the batch whether something happened, for the two facts that are its own policy: which close request ends the application, and whether the renderer's swapchain is stale. Both are decisions `WindowServer` has no business making, which is why they are read here rather than pushed from there.
2. **The debug overlay adapter** walks the span in order and forwards to `io.AddKeyEvent`, `io.AddInputCharacter`, and friends. It needs the record and nothing else, which keeps the first slice small.
3. **The input state layer** (later) folds it into state and vends per-consumer edge boundaries.

Raw record access is an engine-internal seam. It never appears on `EngineContext`; games consume semantic actions.

## Lifetime and threading

The root constructs `WindowServer` only when `AppCapabilities::Presentation` is set, and the dedicated server therefore constructs none with no special-casing.

**Destruction order is renderer, then windows, then server.** The renderer holds a `vk::raii::SurfaceKHR` referencing a window, so the renderer must go first; `Engine::Shutdown` does that explicitly and the member order backs it up. The server does own its windows, and that is fine because `WindowServer` declares `_backend` before `_windows`, so the windows are destroyed first and the connection outlives every object created through it. Both orderings are load-bearing and neither is expressed by anything stronger than declaration order.

**One connection per process.** `glfwTerminate` is process-global, and `glfwInit` succeeds silently on an already-initialized GLFW, so a second `WindowServer` would tear down the first one's windows and leave its handles dangling. A `contract_assert` in the backend's `Create` catches it. That is a diagnostic, not a return of the reference count this replaced, which was a correctness mechanism deciding whether to call `glfwInit` at all.

Main thread only. That is macOS's genuine constraint generalized by GLFW to every platform; underneath, Win32 windows belong to their *creating* thread, Xlib needs `XInitThreads`, and Wayland is dispatchable off-thread. The record is immutable after `Pump()` returns, so any number of threads may read it with no locking, which is what non-destructive reads buy.

`Pump()` is **not guaranteed to return promptly.** Dragging or resizing a window enters a nested modal loop on Win32 and GLFW inherits it, so the frame loop stops: no rendering, no simulation, and the next pump delivers one enormous batch. This is accepted and documented rather than solved. The known escape is `glfwSetWindowRefreshCallback`, which fires *inside* the modal loop and therefore collides directly with the no-logic-in-callbacks rule; Unreal's answer is to pump and render from inside the modal loop. The dedicated-platform-thread pattern (main thread pumps, game loop elsewhere) is the eventual fix and is confined to the root.

## Module layout

The record is **not** part of this module. It is `PlaygroundEngine.PlatformEvents`, a peer:

```
PlaygroundEngine.PlatformEvents        (src/PlatformEvents/)
  :InputCode  :InputModifiers  :InputLockState  :PlatformKeyToken
  :PlatformEvent  :PlatformEventRecord

PlaygroundEngine.WindowServer          (src/WindowServer/)
  :WindowSizes  :WindowSpecification  :WindowServerErrors   the vocabulary
  :BackendDeclarations                                      the two incomplete backend names
  :Window                                                   the class and its backend concept
  :WindowBackend  :WindowServerBackend  :GlfwInputTranslation   CMake-selected, under Backend/
```

Every class declares in a `.cppm` and defines in a `.cpp`, the backends included. Validated on GCC 16: a class declared in an *implementation* partition takes its member definitions from a separate implementation unit, and a free function declared in one links against a definition in another. Two things follow. The partition reads as an interface, which is what the backend concept is checked against; and editing a callback body no longer invalidates the partition's BMI, so nothing downstream rebuilds. `:GlfwInputTranslation` gains a third property: with the tables in the implementation unit, GLFW's header does not appear in that partition at all.

Making the record a partition of `WindowServer` would have encoded "the window system is the only producer," which is already known to be false: device input, replay, and eventually the wire format all touch a `PlatformEvent` and none of them should import a window module to see one.

The backends are two implementation partitions rather than the single one an earlier draft called for. The seam that draft was protecting against was an *exposed* one, and neither partition is reachable by a consumer; splitting them keeps one class per file and lifts the two-hundred-line key table out of the file that holds the pump. Cross-partition use of non-exported functions is validated on GCC 16.

Their cooperation stays an implementation detail with no exposed seam, which is what makes the "how do the window and the pump connect, given that the connection is backend-specific?" problem disappear: the server creates the windows, so its backend already holds whatever native handle it needs. The GLFW backend registers per-window callbacks that route into one buffer; a Wayland backend never touches a surface for input at all, reading the seat and attributing to the focused surface. Same contract, opposite internals.

Concept-checked, compile-time selected, no vtable.

## Implementation notes

Where the shipped code differs from the sketches above, and what the slice added that the design did not name.

- **No signal mechanism was built.** An iteration of this slice added one and it was removed on review: a general delegate system is its own topic and deserves its own research rather than arriving as a side effect of the platform layer. Nothing here needs it, because the record already lets every consumer respond to the same events.
- **Window creation is flat on the server**, as the interface sketch always had it. A `Windows` sub-facade was built first, on the strength of the sub-facade decision, and removed: see the exception recorded under Sub-facades.
- **`CloseRequested` reaches the root as `record.HasEvent(...)`.** That is what "the root remembers" means with the record as the seam.
- **The GLFW backend clears GLFW's own close flag** when it turns a close request into an event. The flag is sticky, so leaving it set would make an unhandled event (a veto) impossible.
- **The window user pointer carries the server, not the window.** With no window identity field in the record, a callback needs nothing but the server's own pending batch. GLFW's callbacks take no user data beyond the window, so the destination has to be reachable this way; the backend owns it outright (`_pendingEvents`) rather than borrowing the caller's record for the length of a pump, which is also why a callback GLFW fires outside `glfwPollEvents` is captured rather than dropped.
- **`PlatformEvent::Type` carries no default member initializer**, so an aggregate that names no type fails the warning bar rather than silently meaning one. `Modifiers` and `Token` carry `{}` for the same warning's sake.
- **`InputModifiers` and `InputLockState` are structs of `bool`s**, not bitmasks. Four bytes against one, in exchange for a structural reflected view (the useful one for a replay and wire artifact), a field-for-field C# match, and the project's designated-initializer rule staying applicable. The reflection system has no flags facet, but that is not what decided this: a flags facet is worth building for render state, replication conditions and component masks, and these would stay `bool`s even once it exists.
- **Timestamps are `steady_clock` nanoseconds read when the callback runs.** That is normally "arrival at pump" precision, since the callbacks normally run inside `glfwPollEvents`; a callback GLFW fires from some other call is stamped when it fires and delivered on the next pump.

## Migration

Done; kept as the record of what moved and why.

- `WindowBackend.GLFW.cppm`: the `s_liveWindowCount` refcount and `EnsurePlatformInitialized` become the server's constructor and destructor. Windows stop participating in platform lifetime. Note the current code uses window count as a proxy for "is GLFW initialized", so a failed creation re-runs `glfwInit`; that incoherence disappears rather than being ported.
- `WindowBackend.GLFW.cppm`: `PollEvents` moves to the server, and its comment about pumping every window rather than just this one becomes true instead of apologetic.
- `Window.cppm`: `Window::Create` becomes `WindowServer::CreateWindow`. `GetRequiredVulkanExtensions` moves to `WindowServer`, changing its call site in `VulkanUtils.cpp`.
- `WindowTypes.cppm`: `FramebufferResizedCallback` is deleted. **This is the strongest single argument for the migration**: the teardown code in `Engine.cpp` exists solely to clear the callback so a late resize cannot reach a destroyed renderer, and with events as data drained by the root, that hazard cannot exist. It also removes a `std::function` from an L1 interface.
- `Engine.cpp`: boot creates the server; the frame loop pumps and dispatches; the resize wiring and the teardown callback reset both disappear.
- `Window::ShouldClose` becomes a `CloseRequested` event that the root remembers. GLFW's flag is sticky and settable, whereas a veto ("unsaved changes") is natural as an unhandled event. Who decides application exit (last window closed versus main window closed) is policy for L3 or L4, not for `WindowServer`.

**Not migrating:** the live `GetFramebufferSize` query in the frame loop stays. Replacing it with a value cached from resize events was considered and rejected. The live query returns 0x0 while minimized, which is what makes the renderer's early return correct; a cache can miss that zero, since Wayland may send no resize on minimize at all, and then swapchain recreation clamps a nonzero extent against surface capabilities that report zero while minimized, and fails. The current split is already right: **the resize event is the "something changed" signal, the live query is the authoritative size.** A cache becomes necessary only when a threaded renderer must not touch the connection, and the fix then is the root reading the size once per frame into `FrameContext`, which is a cache with a defined refresh point rather than one fed by events.

## Overrides to DebugUISketch

- `RawInputQueue` must die as a name: the record is neither a queue nor drained.
- The record should reach the overlay as frame data rather than as a construction-time borrowed pointer, matching the frame-time POD data seam rule.
- The sketch's "DebugUI drains before InputSystem" contract is **simplified away**. Both read the same non-destructive batch, so ordering between them now matters only for the `InputCaptureState` POD.
- `InputCaptureState` survives, but as a state-layer concept rather than a record-level one.
- The sketch references `EngineBootSketch.md`, which no longer exists.

## Known gaps

- **Cross-producer ordering is producer order, not arrival order.** Each producer appends its whole batch in one block, so within a producer the record is in true arrival order (which is the load-bearing property, and what the tests pin), but a second producer's events all land after the first's regardless of when they arrived. `Timestamp` is carried and nothing merges on it. Harmless with one producer; it has to be decided before device input lands, either by merging on `Timestamp` at the root or by narrowing the documented guarantee to per-producer.

- **Stuck keys.** Alt-tab while holding a key and it stays held forever. GLFW 3.3+ synthesizes releases on focus loss, but the guarantee must be part of *our* contract, since a future backend may not, and the same problem recurs when the debug overlay captures input mid-press. The record must deliver `FocusLost` ordered relative to key events; synthesizing releases is the state layer's obligation, and that layer is not being built in this slice. This is a live gap in the interim.
- **Pointer lock** is not in the first slice, and relative motion is meaningless without it. Day one for a 3D camera.
- **Variable-length payloads.** IME composition strings, dropped file paths, and console lifecycle payloads do not fit a fixed POD. The intended mechanism is a side byte arena in the record, with events carrying offset and length, cleared with the record. Named now, unbuilt.
- **Reserved event types** for console lifecycle (suspend, resume, user sign-out, controller reassignment) and for `SurfaceLost` / `SurfaceCreated`. The latter is not mobile-only: `VK_ERROR_SURFACE_LOST_KHR` is real on desktop and the renderer does not handle it. Android additionally destroys and recreates the native window under a surviving window object, so surface lifetime and window lifetime are not the same thing.
- **`PlatformKeyToken` serialization** is undecided (see Field decisions).

## Deferred

- **A second producer.** Device input (gamepads, raw HID) is a separate session appending to the same root-owned record. OpenXR is the hard version: a second stateful connection with its own `xrPollEvent` queue and an action-based input model whose ordering cannot be merged with this one.
- **Fixed-timestep placement**, already open in the architecture document. The timestamp field is what keeps the options available.
- **Multi-window focus and hit-test routing**, and with it the window identity field.
- **The input state layer**: state tables folded from the record, per-consumer cursors so a consumer declares an edge boundary without owning storage, and typed sub-views. Then bindings and semantic `InputCommand` PODs above that.

## Corrections during design

Conclusions reached the long way, recorded so they are not re-derived from scratch. The rest of this document already carries the ones that changed a decision; these two changed how a problem was framed.

- **A shared sequence number across separate key and character buffers was proposed, dropped, then made moot.** The lesson that stuck is the test, not the answer: *ask which consumer needs it.* It was dropped when the only consumer was a hypothetical text field, revived when ImGui turned out to need arrival order, then obsoleted entirely by the single-record decision. The same test later justified dropping the window identity field, and dropping the `Windows` sub-facade.
- **The input state layer was first framed as a POD plus two free functions, and that was wrong.** The fold being pure is an argument about implementation and testability, not about packaging. That layer is system-shaped: it owns storage, it vends per-consumer cursors so a consumer can declare an edge boundary without maintaining its own tracker, and it will accrue device enumeration, hot-plug and capture arbitration.

## Next

The debug overlay adapter, which is the [DebugUISketch.md](DebugUISketch.md) slice. It needs the record and nothing else, which is what keeps it small: walk the span in arrival order and forward to `io.AddKeyEvent`, `io.AddInputCharacter` and friends. The input state layer comes after it, not before, because the overlay does not need folded state and the layer's design wants a real second consumer to answer to.

## Rejected alternatives

- **Separate queues per event kind.** Destroys cross-kind ordering, which is semantically meaningful, and forces every ordering-sensitive consumer to reconstruct a merge key.
- **A polymorphic event hierarchy.** Better ergonomics, but costs the contiguous buffer, trivial copyability, and the serializable-record property. The escape hatch is accessor sugar (`AsKey(event)` returning a view) over unchanged flat storage, and it requires no reflection support that does not exist.
- **`std::optional` for optional event fields.** Layout is unspecified, which breaks byte-level serialization and layout-matched C# structs; no optional facet exists in the reflection system; and it doubles a four-byte field. Sentinels inside serialized PODs, `std::optional` in interfaces.
- **Destructive drain** (SDL's `SDL_PollEvent` model). Forces watcher callbacks and makes every additional consumer awkward, which is visible in every ImGui backend.
- **Coalescing pointer motion in the record.** Destroys the path that high-precision input needs; the fold produces summaries for free.
- **Per-window event queues, or a window-owned pump.** Window association is a platform-specific derivation over device-level input (Wayland's seat, evdev), not an intrinsic property of an event. It must be a field, never structure, and a large category of events has no window at all.
- **Runtime-polymorphic backends.** Compile-time selection, per the architecture document.
