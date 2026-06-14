#include "PlaygroundEngine/EntryPoint.h"

import PlaygroundEngine;

import std;
import PlaygroundEngine.Reflection.TypeRegistry;

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

int main(int argc, char** argv)
{
	PlaygroundEngine::AppDescriptorBase* appDescriptor =
		PlaygroundEngine::GetAppDescriptor(new PlaygroundEngine::CommandLine(argc, argv));

	std::unique_ptr<PlaygroundEngine::Engine> engine = appDescriptor->GetEngine(appDescriptor);
	PlaygroundEngine::TypeRegistry registry;
	registry.RegisterType<Actor>();
	registry.RegisterType<int>();

	engine->Run();
	engine->Run();

	return 0;
}
