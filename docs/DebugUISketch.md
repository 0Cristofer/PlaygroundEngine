# Debug UI Sketch

Status: implementation sketch for the ImGui-based debug overlay system, companion to [EngineBootSketch.md](EngineBootSketch.md). It exists mainly to record how the system connects, since it is the worked example of every interconnection mechanism in [ApplicationArchitecture.md](ApplicationArchitecture.md). Edit freely, delete once implemented.

`DebugUISystem` is an L2 system, dev builds only. Its stock backends (`imgui_impl_glfw` and friends) are not used; the system is its own thin backend over the engine's seams.

Encapsulation boundary: infrastructure is wrapped, vocabulary is not. The system's contract surface (`Dependencies`, loop steps, `DebugDrawData`, `AddPanel`, the capture POD) exposes no ImGui type, so nothing structural knows ImGui exists. Panel bodies, however, speak raw `ImGui::` as the debug dialect; a 1:1 facade over the widget API would be permanent ceremony guarding a mechanical one-time migration of dev-only leaf code. Containment rules, all greppable: no ImGui type in an exported signature; no ImGui state stored across a module boundary; `imgui.h` included only in the DebugUI module and dev-only panel implementation files (global module fragment, same pattern as `Log.h`).

## The connections

| Needs | Mechanism |
|---|---|
| Raw input events | L0 queue contract, borrowed pointer at construction (downward, allowed) |
| Input arbitration vs InputSystem | `InputCaptureState` POD at L0 plus loop ordering: DebugUI drains before InputSystem |
| Rendering its draw lists | Renderer's overlay-pass extension API, wired at the dev root (never a peer import) |
| Clipboard, cursor, display metrics | L1 platform services, borrowed at construction (TODO with the platform slice) |
| Delta time, frame position | `FrameContext` through its loop steps |
| Panels to draw | Registration rim: `AddPanel`, RAII handle, same shape as phase callbacks |

## L0 contract

```cpp
// Sketch. Lives with the input event contracts (e.g. PlaygroundEngine.InputEvents).
export struct InputCaptureState
{
    bool wantsMouse = false;
    bool wantsKeyboard = false;
};
```

Written by `DebugUISystem` at its drain step, read by `InputSystem` at its drain step. Neither imports the other; the loop's ordering makes it correct.

## DebugUISystem

```cpp
// Sketch. L2, dev-only module (PlaygroundEngine.DebugUI).
export class PanelRegistration
{
    // RAII: unregisters on destruction, like signal subscriptions.
};

export class DebugUISystem
{
public:
    struct Dependencies
    {
        RawInputQueue* rawEvents = nullptr;
        InputCaptureState* capture = nullptr;
        // TODO L1: clipboard, cursor shape, display metrics.
    };

    explicit DebugUISystem(const Dependencies& dependencies);

    // Loop steps, in invocation order. Main-thread: this phase documents it
    // (ImGui is single-context); other phases promise nothing.
    void BeginFrame(const FrameContext& frame); // observe raw events, publish capture, ImGui::NewFrame
    void DrawPanels();                          // invoke registered panels; ImGui:: is valid only here
    [[nodiscard]] const DebugDrawData& EndFrame(); // draw lists for the overlay pass

    [[nodiscard]] PanelRegistration AddPanel(std::string_view name, std::function<void()> panel);
};
```

`DebugDrawData` is extracted render data (vertices, indices, texture ids, clip rects): the same seam shape the renderer's normal extraction uses.

## Dev root wiring

```cpp
// Sketch. Inside Engine boot, dev configs only. This block is the composition-seam
// conditional; systems themselves contain no target- or build-identity checks.
#if defined(PGE_DEV)
    _debugUi = std::make_unique<DebugUISystem>(DebugUISystem::Dependencies{
        .rawEvents = &_rawEventQueue,
        .capture = &_inputCapture});

    // Panels: convention, not a base class. Register only systems that have one.
    _rendererPanel = _debugUi->AddPanel("Renderer",
        [renderer = _renderer.get()] { renderer->DrawDebugPanel(); });

    // Renderer extension seam: the debug overlay is the first customer of the
    // renderer's extension API and sets its template. Contract TODO with the renderer.
    _debugUiPass = _renderer->AddOverlayPass([debugUi = _debugUi.get()](OverlayPassContext& pass)
    {
        pass.Draw(debugUi->EndFrame());
    });
#endif
```

Game systems register panels the same way through the `EngineContext`, inside the game's one `PGE_DEV` block.

## Loop ordering

```cpp
// Sketch. The two adjacent drain lines are the input-arbitration contract.
void Engine::RunFrame()
{
    FrameContext frame{.delta = _frameDelta};

    if (_platformEvents)
        _platformEvents->Pump();
    if (_debugUi)
        _debugUi->BeginFrame(frame);    // publishes InputCaptureState
    if (_input)
        _input->Drain(_commands);       // reads it, filters captured input

    _world->Step(frame);

    if (_debugUi)
        _debugUi->DrawPanels();         // debug-UI phase, main thread
    if (_renderer)
        _renderer->Render(*_world);     // overlay pass pulls EndFrame() data
}
```

## ECS and generic panels

- The highest-value panel is generic and owned by this slice: an entity inspector walking the selected entity's components via reflection. No per-system code.
- Per-ECS-system panels piggyback on schedule registration: the record that registers a system to run carries an optional debug hook; the World's panel iterates what the schedule already knows (timings for free, hooks where provided). No second bookkeeping list.
- C#-authored panels are deferred: immediate-mode calls across the interop boundary are per-frame chatter; the generic inspector covers the common case natively.

## Practical notes

- ImGui arrives via FetchContent: expect the known global `-freflection` leak (root `add_compile_options` breaks dep TUs compiled below C++26); solve it at integration time.
- Dev-only linkage under Ninja Multi-Config: simplest is compile always, construct only under `PGE_DEV`; in a static-lib engine, unreferenced objects do not land in the shipping executable. Verify with a symbol check when shipping configs matter; the `PGE_DEV` grep stress test in the architecture doc is the guard.
- Fonts, DPI, gamepad navigation, multi-viewport: all omitted until wanted.

## Continue from here

1. Land after the presentation slice exists (needs the raw event queue and a window to draw over).
2. Bring up ImGui with a hardcoded demo window before panels: proves backend, input, and the overlay pass.
3. Add `AddPanel` plus `InputCaptureState`, then the first real panel (frame stats).
4. The generic reflection entity inspector once the ECS `World` replaces the placeholder.
