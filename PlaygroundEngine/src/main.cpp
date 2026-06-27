#include "PlaygroundEngine/EntryPoint.h"

import std;

int main(int argc, char** argv)
{
	PgE::AppDescriptorBase* appDescriptor =
		PgE::GetAppDescriptor(new PgE::CommandLine(argc, argv));

	std::unique_ptr<PgE::Engine> engine = appDescriptor->GetEngine(appDescriptor);

	engine->Run();
	engine->Run();

	return 0;
}
