#include <PlaygroundEngine/EntryPoint.h>

import PlaygroundEngine;
import PlaygroundGame;

import std;

PlaygroundEngine::AppDescriptorBase* PlaygroundEngine::GetAppDescriptor(PlaygroundEngine::CommandLine* commandLine)
{
	return new PlaygroundGame::PlaygroundGameAppDescriptor(commandLine);
}
