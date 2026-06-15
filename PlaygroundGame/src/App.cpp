module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import PlaygroundEngine.Components.TransformComponent;
import PlaygroundEngine.Reflection.TypeRegistry;
import PlaygroundEngine.Reflection.TypeInfo;
import PlaygroundEngine.Log;

class BaseActor
{
public:
	int GetGeneration()
	{
		return _generation;
	}
private:
	int _generation = 0;
};

class Actor
{
public:
	Actor(const std::string& name, int age)
		: _name(name),
		  _age(age)
	{
	}

	const std::string& GetName()
	{
		return _name;
	}

	int GetAge()
	{
		return _age;
	}

private:
	std::string _name;
	int _age = 0;
	Actor* other = nullptr;
};

std::unique_ptr<PlaygroundEngine::AppBase> PlaygroundGame::PlaygroundGameAppDescriptor::GetApp()
{
	return std::make_unique<App>();
}

void PlaygroundGame::App::OnInitialized()
{
	LOG_INFO("PlaygroundGame::App::OnInitialized");

	PlaygroundEngine::TypeRegistry registry;
	registry.RegisterType<Actor>();
	registry.RegisterType<int>();
	Actor actor("John", 25);

	LOG_INFO(PlaygroundEngine::TypeInfo::ToStringTyped(actor));
}

void PlaygroundGame::App::OnRun(PlaygroundEngine::World* world)
{
	static int a = 2;
	PlaygroundEngine::GameObject* gO1 = world->AddGameObject();
	PlaygroundEngine::TransformComponent* component = gO1->AddComponent<PlaygroundEngine::TransformComponent>();
	component->Position = a;
	LOG_INFO("PlaygroundGame::App::OnRun {}", component->Position);
	a++;
}
