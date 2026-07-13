module;

#include <cassert>

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
			PGE_LOG(Error, "Presentation bootstrap failed: window creation error {}", static_cast<int>(window.error()));
			return std::unexpected(BootError::Platform);
		}

		_window = std::move(*window);
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
		_world = std::make_unique<World>();

		// TODO: WireSignals() once the first signal exists.

		_app = _appDescriptor.GetApp();

		EngineContext engineContext;
		_app->OnBooted(engineContext);

		return {};
	}

	void Engine::StartRun()
	{
		assert(_app && _world && "Engine::Run called before a successful Boot()");

		_app->OnStartRun(_world.get());
		Run();
	}

	void Engine::Run()
	{
		_running = true;
		while (_running)
		{
			RunFrame();
		}
	}
	void Engine::RunFrame()
	{
		if (_window)
		{
			_window->PollEvents();
		}

		_world->Run();

		if (_window)
		{
			_window->SwapBuffers();

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
		_window.reset();
	}
}
