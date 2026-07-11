export module PlaygroundGame;

import PlaygroundEngine;
import PlaygroundEngine.App;

import std;

namespace PgG
{
	export class PlaygroundGameAppDescriptor : public PgE::AppDescriptorBase
	{
	public:
		explicit PlaygroundGameAppDescriptor(const PgE::CommandLine commandLine) : AppDescriptorBase(commandLine) {}

		std::unique_ptr<PgE::AppBase> GetApp() override;
	};

	export class App : public PgE::AppBase
	{
	public:
		void OnBooted(PgE::EngineContext& engine) override;
		void OnStartRun(PgE::World* world) override;
	};
}
