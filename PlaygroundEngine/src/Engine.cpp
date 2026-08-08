module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine;

import PlaygroundEngine.App;
import PlaygroundEngine.Log;
import PlaygroundEngine.Paths;
import PlaygroundEngine.Reflection;

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

		_world = std::make_unique<World>();

		_app = _appDescriptor.GetApp();

		EngineContext engineContext;
		_app->OnBooted(engineContext);

		PGE_LOG(Info, "Finished boot");

		return {};
	}

	void Engine::StartRun()
	{
		_app->OnStartRun(_world.get());
		Run();
	}

	void Engine::Shutdown()
	{
		PGE_LOG(Info, "Shutting down...");

		// First, and stated here rather than left to member declaration order: the reader thread must
		// be joined before anything it could still be handing work to goes away.

		_agentChannel.Stop();

		_app.reset();
		_world.reset();

		if (_rendererVulkan)
		{
			_rendererVulkan->Teardown();
		}
		_rendererVulkan.reset();

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

		_running = true;
		while (_running)
		{
			if (const std::expected<void, RendererError<RendererRenderErrorKind>> runFrameResult = RunFrame(); !runFrameResult)
			{
				RequestStop();
			}
		}
	}

	std::expected<void, RendererError<RendererRenderErrorKind>> Engine::RunFrame()
	{
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

		ServiceFrameCapture();

		if (_platformEvents.HasEvent(PlatformEventType::CloseRequested))
		{
			RequestStop();
		}

		_world->Run();

		if (_rendererVulkan && _window)
		{
			if (const std::expected<void, RendererError<RendererRenderErrorKind>> drawResult =
					_rendererVulkan->DrawFrame(_platformEvents, _window->GetFramebufferSize());
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

	void Engine::LogPlatformEvents() const
	{
		for (auto& event : _platformEvents.GetEvents())
		{
			PGE_LOG(Trace, ToString(event));
		}
	}

	void Engine::ServiceFrameCapture() const
	{
		if (!_rendererVulkan)
		{
			return;
		}

		const bool captureRequested = std::ranges::any_of(_platformEvents.GetEvents(), [](const PlatformEvent& event) {
			return event.Type == PlatformEventType::KeyPressed && event.Code == InputCode::KeyF12 && !event.Repeat;
		});

		if (!captureRequested)
		{
			return;
		}

		PGE_LOG(Trace, "Frame capture requested");
		if (const std::expected<std::filesystem::path, PathError> capturePath = GenerateCapturePath())
		{
			_rendererVulkan->RequestCapture(*capturePath);
		}
		else
		{
			PGE_LOG(Error, "Frame capture skipped: no usable capture path");
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

	std::expected<void, BootError> Engine::BootRendering()
	{
		if (!_windowServer || !_window)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: can't create renderer without window");
			return std::unexpected(BootError::Rendering);
		}

		std::expected<std::unique_ptr<RendererVulkan>, RendererError<RendererCreationErrorKind>> renderer =
			RendererVulkan::Create(RendererSpecification{}, *_windowServer, *_window);

		if (!renderer)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: renderer creation error {}", ToString(renderer.error()));
			return std::unexpected(BootError::Rendering);
		}

		_rendererVulkan = std::move(*renderer);

		return {};
	}
}
