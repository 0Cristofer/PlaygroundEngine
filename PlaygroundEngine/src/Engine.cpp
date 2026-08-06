module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine;

import PlaygroundEngine.App;
import PlaygroundEngine.Log;
import PlaygroundEngine.Reflection;

namespace PgE
{
	Engine::Engine(AppDescriptorBase& appDescriptor) : _appDescriptor(appDescriptor)
	{}

	std::expected<void, BootError> Engine::BootPresentation()
	{
		auto windowServer = WindowServer::Create();
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

		auto renderer = RendererVulkan::Create(RendererSpecification{}, *_windowServer, *_window);

		if (!renderer)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: renderer creation error {}", ToString(renderer.error()));
			return std::unexpected(BootError::Rendering);
		}

		_rendererVulkan = std::move(*renderer);

		return {};
	}

	std::expected<void, BootError> Engine::Boot()
	{
		PGE_LOG(Info);

		// Global facilities: logging, profiling, etc.
		Log::Configure();

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
		_world = std::make_unique<World>();

		_app = _appDescriptor.GetApp();

		EngineContext engineContext;
		_app->OnBooted(engineContext);

		return {};
	}

	void Engine::StartRun()
	{
		_app->OnStartRun(_world.get());
		Run();
	}

	void Engine::Run()
	{
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

		for (auto& event : _platformEvents.GetEvents())
		{
			PGE_LOG(Trace, ToString(event));
		}

		if (_rendererVulkan && _platformEvents.HasEvent(PlatformEventType::WindowResized))
		{
			_rendererVulkan->NotifyFramebufferResized();
		}

		if (_platformEvents.HasEvent(PlatformEventType::CloseRequested))
		{
			RequestStop();
		}

		_world->Run();

		if (_rendererVulkan && _window)
		{
			if (const std::expected<void, RendererError<RendererRenderErrorKind>> drawResult =
					_rendererVulkan->DrawFrame(_window->GetFramebufferSize());
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

	void Engine::RequestStop()
	{
		PGE_LOG(Info);

		_running = false;
	}

	void Engine::Shutdown()
	{
		PGE_LOG(Info);

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
	}
}
