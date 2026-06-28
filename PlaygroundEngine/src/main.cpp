#include "PlaygroundEngine/EntryPoint.h"

import std;

int main(const int argc, char** argv)
{
	PgE::AppDescriptorBase* appDescriptor =
		PgE::GetAppDescriptor(new PgE::CommandLine(argc, argv));

	const std::unique_ptr<PgE::Engine> engine = appDescriptor->GetEngine(appDescriptor);

	engine->Run();
	engine->Run();

	return 0;
}
