# Engine Boot Sketch

Status: implementation sketch for the L3 skeleton, the most basic shape that matches [ApplicationArchitecture.md](ApplicationArchitecture.md). Not a design document: this is the starting point for reshaping `Engine.cppm`/`Engine.cpp`/`App.cppm`/`main.cpp`, meant to be edited freely and deleted once implemented.

## The flow

```
main()
  -> CommandLine captured
  -> GetAppDescriptor(commandLine)          game's one entry point, returns ownership
  -> Engine engine(*descriptor)             trivial constructor, no work
  -> engine.Boot()                          fallible: capabilities, systems, wiring, app last
  -> engine.Run()                           OnRun, then the frame loop
  -> engine.Shutdown()                      reverse construction order, app first
```

Corrections over the current stub this encodes: the engine consumes the descriptor instead of being manufactured by it, `Boot()` is a fallible member function instead of a do-everything constructor, the app is constructed after the systems, there is a real loop, and teardown order is explicit.

## Engine.cppm

```cpp
export module PlaygroundEngine;

export import PlaygroundEngine.World;
import PlaygroundEngine.App;
import std;

namespace PgE
{
    export struct CommandLine
    {
        int argc = 0;
        char** argv = nullptr;

        // TODO: parsed accessors for the editor-spawn parameters
        // (--world=, --editor-connect=, --wait-for-debugger).
    };

    export struct AppCapabilities
    {
        bool presentation = true;
        bool rendering = true;
        bool audio = true;
        bool networking = false;
    };

    export enum class BootError : std::uint8_t
    {
        Platform = 1,
    };

    export class AppDescriptorBase
    {
    public:
        explicit AppDescriptorBase(CommandLine commandLine) : _commandLine(commandLine) {}
        virtual ~AppDescriptorBase() = default;

        [[nodiscard]] virtual AppCapabilities GetCapabilities() const
        {
            return AppCapabilities{};
        }
        [[nodiscard]] virtual std::unique_ptr<AppBase> GetApp() = 0;

        [[nodiscard]] const CommandLine& GetCommandLine() const { return _commandLine; }

    private:
        CommandLine _commandLine;
    };

    export class Engine
    {
    public:
        explicit Engine(AppDescriptorBase& appDescriptor);

        [[nodiscard]] std::expected<void, BootError> Boot();
        void Run();
        void Shutdown();

        void RequestStop();
        [[nodiscard]] World* GetWorld() const;

    private:
        void RunFrame();

        AppDescriptorBase& _appDescriptor;
        bool _running = false;

        // Members double as the construction-order record: L1 first, then L2,
        // app last; Shutdown() resets in reverse.
        std::unique_ptr<World> _world;
        std::unique_ptr<AppBase> _app;
    };
}
```

## Engine.cpp

```cpp
module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine;

import PlaygroundEngine.Log;

namespace PgE
{
    Engine::Engine(AppDescriptorBase& appDescriptor) : _appDescriptor(appDescriptor) {}

    std::expected<void, BootError> Engine::Boot()
    {
        // Logging is an ambient L0 facility (see ApplicationArchitecture.md):
        // constant-initialized, usable before Boot. The root only configures it.
        Log::Configure(/* TODO: sinks and levels, when the first knob is needed */);

        const AppCapabilities capabilities = _appDescriptor.GetCapabilities();
        PGE_LOG(Info, "Booting: presentation={} rendering={} audio={} networking={}",
            capabilities.presentation, capabilities.rendering,
            capabilities.audio, capabilities.networking);

        // TODO L1: platform slice, gated on capabilities.presentation
        //          (return std::unexpected(BootError::Platform) on failure).
        // TODO L2: systems in explicit dependency order, constructor injection.
        _world = std::make_unique<World>();

        // TODO: WireSignals() once the first signal exists.

        _app = _appDescriptor.GetApp();

        WiringContext wiring;
        _app->OnInitialized(wiring);

        return {};
    }

    void Engine::Run()
    {
        _app->OnRun(_world.get());

        _running = true;
        while (_running)
            RunFrame();
    }

    void Engine::RunFrame()
    {
        // Ordering is the contract; each line lands here as its system lands:
        // pump platform events, drain input into commands, step, render, present.
        _world->Run();

        // TODO: placeholder stop so the loop terminates while nothing drives
        // lifetime; remove when the presentation slice lands (window close signal).
        RequestStop();
    }

    void Engine::RequestStop()
    {
        _running = false;
    }

    void Engine::Shutdown()
    {
        _app.reset();
        _world.reset();
    }

    World* Engine::GetWorld() const
    {
        return _world.get();
    }
}
```

## App.cppm

```cpp
export module PlaygroundEngine.App;

import PlaygroundEngine.World;
import std;

namespace PgE
{
    export class WiringContext
    {
        // TODO: AddPhaseCallback(Phase, callable) and published signals.
        // Deliberately empty until the first real system exists.
    };

    export class AppBase
    {
    public:
        virtual ~AppBase() = default;

        virtual void OnInitialized(WiringContext& wiring) = 0;
        virtual void OnRun(World* world) = 0;
    };
}
```

## main.cpp

```cpp
#include "PlaygroundEngine/EntryPoint.h"

import PlaygroundEngine;
import std;

int main(const int argc, char** argv)
{
    const std::unique_ptr<PgE::AppDescriptorBase> appDescriptor =
        PgE::GetAppDescriptor(PgE::CommandLine{.argc = argc, .argv = argv});

    PgE::Engine engine(*appDescriptor);

    if (const auto booted = engine.Boot(); !booted)
        return static_cast<int>(booted.error());

    engine.Run();
    engine.Shutdown();

    return 0;
}
```

`EntryPoint.h` changes accordingly: `std::unique_ptr<AppDescriptorBase> GetAppDescriptor(CommandLine commandLine);`. The game side (`PlaygroundGame.cpp`) returns `std::make_unique<GameDescriptor>(commandLine)`, and its `App::OnInitialized` gains the `WiringContext&` parameter.

## Deliberately omitted

- `FrameContext` and timing (delta, fixed timestep): lands with the first system that consumes it.
- Phases, signals, queues: `WiringContext` stays empty until the first real seam exists; adding surface before a consumer is gold-plating.
- Capability consumption: flags are read and logged but gate nothing until the presentation slice exists.
- Command-line parsing: raw argc/argv until the first real flag (`--world=`) is needed.
- Any `Engine`-level test: `PlaygroundTests` boots no `Engine` by design; validation is running `PlaygroundGame` and reading the log.

## Continue from here

1. Reshape the four files to this skeleton; delete `AppDescriptorBase::GetEngine` and the double `Run()` call in `main`.
2. Make the default logger constant-initialized and rename `Log::Init` to `Log::Configure` (logging must work with no `Engine`, as `PlaygroundTests` already relies on).
3. Confirm it builds warnings-clean and `PlaygroundGame` runs one frame and exits.
4. Next real slice per the architecture doc: the platform/presentation bootstrap (`BootPresentation()`, window, raw event queue), which brings the first capability gate, the first signal (window close driving `RequestStop`), and the first `WiringContext` surface.
