module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import PlaygroundEngine.Components.TransformComponent;
import PlaygroundEngine.Log;

std::unique_ptr<PgE::AppBase> PgG::PlaygroundGameAppDescriptor::GetApp()
{
	return std::make_unique<App>();
}

void PgG::App::OnBooted(PgE::EngineContext&)
{
	PGE_LOG(Info);
}

void PgG::App::OnStartRun(PgE::World* world)
{
	static int a = 2;
	PgE::GameObject* gO1 = world->AddGameObject();
	PgE::TransformComponent* component = gO1->AddComponent<PgE::TransformComponent>();
	component->Position = a;
	PGE_LOG(Info, "{}", component->Position);
	a++;
}
