module;

#include <PlaygroundEngine/Log.h>

module PlaygroundGame;

import PlaygroundEngine.Components.TransformComponent;
import PlaygroundEngine.Reflection.TypeRegistry;
import PlaygroundEngine.Reflection;
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

std::unique_ptr<PgE::AppBase> PgG::PlaygroundGameAppDescriptor::GetApp()
{
	return std::make_unique<App>();
}

void PgG::App::OnInitialized()
{
	PGE_LOG(Info);

	PgE::TypeRegistry registry;
	registry.RegisterType<Actor>();
	registry.RegisterType<int>();
	Actor actor("John", 25);

	PGE_LOG(Info, PgE::ToString(actor));
	PGE_LOG(Info, "Actor functions:\n{}", PgE::TypeOf<Actor>().FunctionsToString());
}

void PgG::App::OnRun(PgE::World* world)
{
	static int a = 2;
	PgE::GameObject* gO1 = world->AddGameObject();
	PgE::TransformComponent* component = gO1->AddComponent<PgE::TransformComponent>();
	component->Position = a;
	PGE_LOG(Info, "{}", component->Position);
	a++;
}
