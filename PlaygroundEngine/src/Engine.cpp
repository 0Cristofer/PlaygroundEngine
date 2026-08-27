module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine;

import PlaygroundEngine.App;
import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;
import imgui;

namespace PgE
{
	Engine::Engine(AppDescriptorBase& appDescriptor) : _appDescriptor(appDescriptor)
	{}

	std::expected<void, BootError> Engine::Boot()
	{
		PGE_LOG(Info, "Booting...");

		const CommandLine& commandLine = _appDescriptor.GetCommandLine();

		// Global facilities: logging, profiling, etc.
		Log::Configure(LogConfiguration{});

		const AppCapabilities capabilities = _appDescriptor.GetCapabilities();
		PGE_LOG(Info, "Booting with capabilities: {}", ToString(capabilities));

		if (capabilities.Presentation)
		{
			if (const auto presented = BootPresentation(); !presented)
			{
				return std::unexpected(presented.error());
			}
		}

		if (capabilities.Rendering)
		{
			if (const auto rendering = BootRendering(); !rendering)
			{
				return std::unexpected(rendering.error());
			}
		}

		_agentChannel.StartIfRequested(commandLine.Argc, commandLine.Argv);

		_ecs = std::make_unique<Ecs>();

		_app = _appDescriptor.GetApp();

		EngineContext engineContext;
		_app->OnBooted(engineContext);

		PGE_LOG(Info, "Finished boot");

		return {};
	}

	void Engine::StartRun()
	{
		_app->OnStartRun(*_ecs);
		Run();
	}

	void Engine::Shutdown()
	{
		PGE_LOG(Info, "Shutting down...");

		// First, and stated here rather than left to member declaration order: the reader thread must
		// be joined before anything it could still be handing work to goes away.

		_agentChannel.Stop();

		_app.reset();
		_ecs.reset();

		if (_rendererVulkan)
		{
			_rendererVulkan->Teardown();
		}
		_rendererVulkan.reset();

		// After the renderer, whose teardown detaches the overlay backend from this context.

		_debugUi.reset();

		// The renderer holds a surface referencing the window, so windows outlive it and the
		// connection outlives them.

		_window = nullptr;
		_windowServer.reset();

		// Last, so the trace tail of everything above reaches the file.

		Log::Flush();
	}

	void Engine::RequestStop()
	{
		PGE_LOG(Info, "Stop requested");

		_running = false;
	}

	void Engine::Run()
	{
		PGE_LOG(Info, "Running");

		_previousFrameTime = std::chrono::steady_clock::now();

		_running = true;
		while (_running)
		{
			if (const std::expected<void, RendererError<RendererRenderErrorKind>> runFrameResult = RunStep(); !runFrameResult)
			{
				RequestStop();
			}
		}
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> Engine::RunStep()
	{
		const float deltaTimeSeconds = AdvanceStepClock();

		_platformEvents.Clear();

		if (_windowServer)
		{
			_windowServer->Pump(_platformEvents);
		}

		// Straight after the pump, so injected events land in the same batch as real ones and every
		// consumer below reads one record without caring which producer filled it.
		_agentChannel.DrainInto(_platformEvents);

		LogPlatformEvents();

		if (_rendererVulkan && _platformEvents.HasEvent(PlatformEventType::WindowResized))
		{
			_rendererVulkan->NotifyFramebufferResized();
		}

		if (_rendererVulkan)
		{
			_frameCapture.ServiceRequests(_platformEvents, *_rendererVulkan);
		}

		if (_platformEvents.HasEvent(PlatformEventType::CloseRequested))
		{
			RequestStop();
		}

		// The debug UI frame opens here and closes below, so everything the loop reaches in between
		// can draw with no registration. The backend's own step has to precede ImGui's.

		if (_debugUi != nullptr && _window != nullptr)
		{
			if (_rendererVulkan)
			{
				_rendererVulkan->BeginDebugUiFrame();
			}

			_debugUi->BeginFrame(*_window, _platformEvents, deltaTimeSeconds);
		}

		_app->OnStep(_platformEvents);
		_ecs->Step(deltaTimeSeconds);

		// Placeholder standing in for real panels, written the way one would be: a guard on the frame
		// being open, then plain ImGui calls, with no reference to the DebugUi instance.

		if (_debugUi != nullptr)
		{
			_debugUi->DrawSettingsPanel();
		}

		ImDrawData* const debugUiDrawData = _debugUi != nullptr ? _debugUi->EndFrame() : nullptr;

		// After the frame closed, so this is the shape the frame just decided on rather than the one
		// before it. The window drops a shape that is already current, so this costs nothing to call.

		if (_window != nullptr)
		{
			_window->SetCursorShape(DebugUi::DesiredCursor());
		}

		if (_rendererVulkan && _window)
		{
			// After the simulation step and the debug panel, so an edit this frame is seen this frame.

			ExtractFrame(*_ecs, _extractedFrame);

			if (const std::expected<void, RendererError<RendererRenderErrorKind>> drawResult =
					_rendererVulkan->DrawFrame(_extractedFrame, _window->GetFramebufferSize(), debugUiDrawData);
				!drawResult)
			{
				return drawResult;
			}
		}

		if (!_windowServer)
		{
			// Headless (presentation disabled): nothing drives lifetime yet, so run a
			// single frame. Replaced when a headless target grows its own exit condition
			// (dedicated server: a shutdown command; cook tool: an empty work queue).
			RequestStop();
		}

		return {};
	}

	float Engine::AdvanceStepClock()
	{
		const std::chrono::steady_clock::time_point frameTime = std::chrono::steady_clock::now();
		const float deltaTimeSeconds = std::chrono::duration<float>(frameTime - _previousFrameTime).count();

		_previousFrameTime = frameTime;

		return deltaTimeSeconds;
	}

	void Engine::LogPlatformEvents() const
	{
		for (auto& event : _platformEvents.GetEvents())
		{
			PGE_LOG(Trace, ToString(event));
		}
	}

	std::expected<void, BootError> Engine::BootPresentation()
	{
		std::expected<std::unique_ptr<WindowServer>, WindowServerError> windowServer = WindowServer::Create();
		if (!windowServer)
		{
			PGE_LOG(Error, "Presentation bootstrap failed: window server error {}", ToString(windowServer.error()));
			return std::unexpected(BootError::Platform);
		}

		_windowServer = std::move(*windowServer);

		const std::expected<Window*, WindowError> window = _windowServer->CreateWindow(WindowSpecification{});
		if (!window)
		{
			PGE_LOG(Error, "Presentation bootstrap failed: window creation error {}", ToString(window.error()));
			return std::unexpected(BootError::Platform);
		}

		_window = window.value();

		return {};
	}

	void Engine::BootDebugUi()
	{
		// Development builds only: the composition seam is what makes this dev-only, not a check
		// inside DebugUi itself.

#if defined(PGE_DEV)
		_debugUi = std::make_unique<DebugUi>(*_window, *_windowServer);
#endif
	}

	std::expected<void, BootError> Engine::BootRendering()
	{
		if (!_windowServer || !_window)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: can't create renderer without window");
			return std::unexpected(BootError::Rendering);
		}

		// Past the check above, which is what makes the window and server safe to hand over, and ahead
		// of the renderer, which attaches its overlay backend to the context this creates.

		BootDebugUi();

		std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> renderer =
			RendererVulkan::Create(RendererSpecification{.DebugUiOverlay = _debugUi != nullptr}, *_windowServer, *_window);

		if (!renderer)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: renderer creation error {}", ToString(renderer.error()));
			return std::unexpected(BootError::Rendering);
		}

		_rendererVulkan = std::move(*renderer);

		// An overlay that did not come up leaves a context nobody draws, and ImGui's frame asserts on
		// a font atlas no backend ever built. Dropping the debug UI is what makes losing it survivable.

		if (_debugUi != nullptr && !_rendererVulkan->IsDebugUiOverlayEnabled())
		{
			PGE_LOG(Warn, "Debug UI disabled: the renderer brought up no overlay");
			_debugUi.reset();
		}

		return {};
	}
}
