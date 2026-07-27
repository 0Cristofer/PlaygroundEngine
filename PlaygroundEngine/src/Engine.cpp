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
		auto window = Window::Create(WindowSpecification{});
		if (!window)
		{
			PGE_LOG(Error, "Presentation bootstrap failed: window creation error {}", ToString(window.error()));
			return std::unexpected(BootError::Platform);
		}

		_window = std::move(*window);
		return {};
	}

	std::expected<void, BootError> Engine::BootRendering()
	{
		if (!_window)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: can't create renderer without window");
			return std::unexpected(BootError::Rendering);
		}

		auto renderer = RendererVulkan::Create(RendererSpecification{}, *_window);

		if (!renderer)
		{
			PGE_LOG(Error, "Rendering bootstrap failed: renderer creation error {}", ToString(renderer.error()));
			return std::unexpected(BootError::Rendering);
		}

		_rendererVulkan = std::move(*renderer);

		// Wired at the root rather than the renderer subscribing itself: the window is the event
		// source and the renderer only consumes, so neither has to know how the other is built.

		_window->SetFramebufferResizedCallback([this](const FramebufferSize) { _rendererVulkan->NotifyFramebufferResized(); });

		return {};
	}

	std::expected<void, BootError> Engine::Boot()
	{
		PGE_LOG(Info);

		// Global facilities: logging, profiling, etc.
		Log::Configure();

		const AppCapabilities capabilities = _appDescriptor.GetCapabilities();
		PGE_LOG(Info, "Booting with capabilities: {}", PgE::ToString(capabilities));

		if (capabilities.Presentation)
		{
			if (const auto presented = BootPresentation(); !presented)
			{
				return std::unexpected(presented.error());
			}
		}

		// TODO L2: systems in explicit dependency order, constructor injection.

		if (capabilities.Rendering)
		{
			if (const auto rendering = BootRendering(); !rendering)
			{
				return std::unexpected(rendering.error());
			}
		}
		_world = std::make_unique<World>();

		// TODO: WireSignals() once the first signal exists.

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
		if (_window)
		{
			_window->PollEvents();
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

		if (_window)
		{
			if (_window->ShouldClose())
			{
				RequestStop();
			}
		}
		else
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

		// Dropped before the renderer it points at, so a late resize event cannot reach a
		// destroyed subscriber.

		if (_window)
		{
			_window->SetFramebufferResizedCallback({});
		}

		if (_rendererVulkan)
		{
			_rendererVulkan->Teardown();
		}
		_rendererVulkan.reset();
		_window.reset();
	}
}
