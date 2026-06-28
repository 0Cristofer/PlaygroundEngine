module;

#include "PlaygroundEngine/Log.h"

module PlaygroundEngine;

import PlaygroundEngine.Log;

namespace PgE
{
	std::unique_ptr<Engine> AppDescriptorBase::GetEngine(AppDescriptorBase* appDescriptor)
	{
		return std::make_unique<Engine>(appDescriptor);
	}

	std::unique_ptr<CommandLine> AppDescriptorBase::GetCommandLine()
	{
		return std::move(_commandLine);
	}

	Engine::Engine(AppDescriptorBase* appDescriptor)
	{
		Log::Init();
		PGE_LOG(Info, "Creating engine");

		_app = appDescriptor->GetApp();
		_currentWorld = std::make_unique<World>();

		_app->OnInitialized();
	}

	World* Engine::GetWorld() const
	{
		return _currentWorld.get();
	}

	void Engine::Run()
	{
		PGE_LOG(Info);
		_app->OnRun(_currentWorld.get());
		_currentWorld->Run();
	}
}
