# WindowServer: session handoff

Ephemeral. Delete once the platform slice lands. Design lives in [WindowServer.md](WindowServer.md); this file only carries what that document deliberately leaves out (process state, resumption order, and the reasoning behind decisions that would otherwise get re-derived).

## State

Design complete and reviewed by `engine-architect`; findings folded into [WindowServer.md](WindowServer.md). **Zero implementation.** No files under `PlaygroundEngine/src/` have been touched.

Scope covered is the **platform layer only**. The input state layer and the semantic action layer were deliberately excluded and are sketched only as "Deferred" in the design document.

## Next step

The refactor that has to happen before any event work has somewhere to live. Suggested split, each commit green:

**1. Mechanical: `Window` becomes `WindowServer` + `Window`.**
Rename the module to `PlaygroundEngine.WindowServer`, keep `:common` / `:backend`, move `glfwInit` / `glfwTerminate` out of `WindowBackend`'s refcount into the server's constructor and destructor, turn `Window::Create` into `WindowServer::CreateWindow`, move `GetRequiredVulkanExtensions` to the server and update its call site in `VulkanUtils.cpp`, update boot in `Engine.cpp`. No behavior change, `FramebufferResizedCallback` survives this commit.

**2. Behavioral: the event record replaces the callbacks.**
Add `PlatformEvent` / `PlatformEventRecord` to `:common`, have the root own the record and the backend fill it from GLFW callbacks, add `DispatchWindowEvents`, delete `FramebufferResizedCallback` and `ShouldClose`. The renderer's resize notification now comes from a signal `WindowServer` owns.

There is a tension worth naming: the project rule is that old approaches are replaced rather than kept alongside, and commit 1 leaves the callback alive. That is mid-refactor rather than a durable parallel mechanism, but if you would rather not have it in history at all, the two collapse into one larger commit.

**3. Then** the ImGui adapter, which is the `DebugUISketch.md` slice and needs only the record.

Test seam worth using from commit 2: because the root owns the record and `PlatformEvent` is a plain POD, a test can construct a record, fill it by hand, and exercise consumers with no window and no GLFW. That is the intended way to test everything above this layer.

## Settled, do not re-litigate

Each of these was argued to a conclusion. Reasons are in [WindowServer.md](WindowServer.md); the one-liners here exist so a fresh session recognizes them as closed.

| Decision | Short reason |
|---|---|
| `WindowServer` is a session object, not a "platform" god object | Membership is the connection, not platform-specificity |
| The name, over `Platform` / `Display` / `DisplayServer` | "Platform" over-claims; "display" reads as monitor |
| One ordered record, not per-kind queues | Cross-kind order is semantically meaningful (click position, shift-click, focus, text) |
| Flat POD event, no union or variant | Contiguity, trivial copyability, serializability; reflection has no variant facet |
| Per-pump immutable batch, non-destructive reads | Several consumers per batch; the 0/1/N problem belongs to the state layer |
| The root owns the record, `Pump(record&)` | Testability without a window; replay as a producer swap; second producer later |
| `Timestamp` is required | Fixed-step partitioning and faithful replay are impossible without it |
| No window identity field yet | No consumer; retrofit cost is currently zero |
| No handle table for windows | One window, nothing reads it; `CoreConventions` already names the backings for later |
| Keys and characters: separate concepts, same stream | Stateful versus eventful; ordering is a platform property, not a per-world one |
| Vulkan surface stays in the *required* backend concept | Deliberate deviation; the hard compile error is the forcing function for backend two |
| Sub-facades from the start | The "later" list is exactly Godot's `DisplayServer`; concepts make the god object worse than a vtable |
| Physical key positions, label lookup on the server | Bindings survive layout changes; labels need a connection query |
| One `InputCode` space with HID-style ranges | Binding uniformity one layer up; split enums force variant bindings |
| The live `GetFramebufferSize` query stays | See corrections below |
| Gamepads deferred entirely | Separate session; nothing in the first slice needs them |

## Open

- **`PlatformKeyToken` serialization.** Backend-opaque, but the record is a replay artifact. Either excluded from serialization or serialized with a backend identifier and treated as advisory. Must be decided before a recording format exists.
- **Variable-length payloads.** Side byte arena in the record, events carrying offset and length. Named, unbuilt; needed by IME, file drop, and console lifecycle.
- **Stuck keys.** Live gap between this slice and the state layer. The record must deliver `FocusLost` ordered against key events; synthesizing releases belongs to the layer above, which does not exist yet.
- **Pointer lock and relative motion mode.** Not in the first slice, needed for a 3D camera.

## Corrections made during design

Recorded so they are not re-derived from scratch.

- **Caching `GetFramebufferSize` from resize events was proposed and rejected.** The live query returns 0x0 while minimized, which is what makes the renderer's early return correct. A cache can miss that zero (Wayland may send no resize on minimize), and swapchain recreation then clamps a nonzero extent against capabilities reporting zero, and fails. The event is the "something changed" signal; the live query is the authoritative size. Revisit only for a threaded renderer, and then as a once-per-frame read into `FrameContext`, not an event-fed cache.
- **The session criterion was first stated as "operations multiplexed over one shared channel" and that formulation is broken.** Under async I/O, `io_uring` and IOCP are shared rings multiplexing every operation, which flips networking and file I/O into session objects. The fixed version adds "the composition root constructs it," which is what keeps the verdicts stable.
- **A shared sequence number across separate key and character buffers was proposed, dropped, then made moot.** The lesson that stuck: ask which consumer needs it. It was dropped when the only consumer was a hypothetical text field, revived when ImGui turned out to need arrival order, and then obsoleted by the single-record decision. The same test later justified dropping the window identity field.
- **The input state layer was first framed as a POD plus two free functions; that was wrong.** The fold being pure is an argument about implementation and testability, not about packaging. It is system-shaped: it owns storage and vends per-consumer cursors so a consumer declares an edge boundary without maintaining its own tracker, and it accrues device enumeration, hot-plug, and capture arbitration.
- **The one-buffer decision was first justified by ImGui's input trickling; that justification is over-fitted.** ImGui is corroboration. The load-bearing argument is that cross-kind ordering is meaningful to the engine itself and survives ImGui's replacement.
- **`GLFW_LOCK_KEY_MODS` is off by default.** Per-event modifiers are justified by lock-key state being underivable from transitions, so the backend must enable that input mode or the justification is void.
- **GLFW dropping codepoints below 32 is GLFW policy, not universal.** Win32 delivers `WM_CHAR` 0x08. Backends disagree, so editing keys must come from the key stream regardless, which is a stronger argument than the GLFW quirk.

## Working context

- **Pair mode, deliberately slow.** The user drives, one piece at a time, and takes the final stance on each fork. Surface trade-offs with a recommendation; do not batch decisions or run ahead into implementation.
- **The documents in `docs/` are open to challenge.** The user's framing: they are "closer to thought experiments than to actual decisions." Several statements in `ApplicationArchitecture.md` and `DebugUISketch.md` were contradicted during this design, deliberately.
- **`DebugUISketch.md` has five statements this design overrides**, listed in the design document's "Overrides" section rather than edited into that file. It also contains a dangling reference to `EngineBootSketch.md`, which no longer exists. Folding the corrections in was offered and not yet requested.
