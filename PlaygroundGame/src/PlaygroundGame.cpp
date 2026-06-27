#include <PlaygroundEngine/EntryPoint.h>

import PlaygroundEngine;
import PlaygroundGame;

import std;

PgE::AppDescriptorBase* PgE::GetAppDescriptor(PgE::CommandLine* commandLine)
{
	return new PgG::PlaygroundGameAppDescriptor(commandLine);
}
