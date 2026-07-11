#include <PlaygroundEngine/EntryPoint.h>

import PlaygroundEngine;
import PlaygroundGame;

import std;

std::unique_ptr<PgE::AppDescriptorBase> PgE::GetAppDescriptor(CommandLine commandLine)
{
	return std::make_unique<PgG::PlaygroundGameAppDescriptor>(commandLine);
}
