#include "PlaygroundEngine/EntryPoint.h"

import PlaygroundEngine;

import std;

int main(int argc, char** argv)
{
	PlaygroundEngine::AppDescriptorBase* appDescriptor =
		PlaygroundEngine::GetAppDescriptor(new PlaygroundEngine::CommandLine(argc, argv));

	std::unique_ptr<PlaygroundEngine::Engine> engine = appDescriptor->GetEngine(appDescriptor);

	engine->Run();
	engine->Run();

	return 0;
}
