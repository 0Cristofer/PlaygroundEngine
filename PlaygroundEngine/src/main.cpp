#include "PlaygroundEngine/EntryPoint.h"

import PlaygroundEngine;
import std;

int main(const int argc, char** argv)
{
	const std::unique_ptr<PgE::AppDescriptorBase> appDescriptor =
		PgE::GetAppDescriptor(PgE::CommandLine{.Argc = argc, .Argv = argv});

	PgE::Engine engine(*appDescriptor);

	if (const auto booted = engine.Boot(); !booted)
	{
		engine.Shutdown();
		return static_cast<int>(booted.error());
	}

	engine.StartRun();
	engine.Shutdown();

	return 0;
}
