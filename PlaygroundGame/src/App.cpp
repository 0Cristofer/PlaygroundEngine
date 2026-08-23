module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import imgui;

import PlaygroundEngine.Log;
import PlaygroundEngine.DebugUi;
import PlaygroundGame.Ecs.EntitySpawnerSystem;
import PlaygroundGame.Ecs.EntityDebugPanelSystem;

std::unique_ptr<PgE::AppBase> PgG::PlaygroundGameAppDescriptor::GetApp()
{
	return std::make_unique<App>();
}

void PgG::App::OnBooted(PgE::EngineContext&)
{
	PGE_LOG(Info);
}

void PgG::App::OnStartRun(PgE::Ecs& ecs)
{
	auto inputSystem = std::make_unique<PgE::InputSystem>(ecs);
	_inputSystem = inputSystem.get();
	ecs.AddSystem(std::move(inputSystem));

	auto entitySpawnerSystem = std::make_unique<EntitySpawnerSystem>(ecs);
	ecs.AddSystem(std::move(entitySpawnerSystem));

	auto entityDebugPanelSystem = std::make_unique<EntityDebugPanelSystem>(ecs);
	ecs.AddSystem(std::move(entityDebugPanelSystem));
}

void PgG::App::OnStep(const PgE::PlatformEventRecord& platformEventRecord)
{
	_inputSystem->UpdatePlatformInput(platformEventRecord);
}
