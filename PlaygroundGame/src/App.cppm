export module PlaygroundGame;

import PlaygroundEngine;
import PlaygroundEngine.App;

import std;

namespace PgG
{
	export class PlaygroundGameAppDescriptor : public PgE::AppDescriptorBase
	{
	public:
		PlaygroundGameAppDescriptor(PgE::CommandLine* commandLine) : AppDescriptorBase(commandLine) {}

		virtual ~PlaygroundGameAppDescriptor() = default;

		std::unique_ptr<PgE::AppBase> GetApp() override;
	};

	export class App : public PgE::AppBase
	{
	public:
		void OnInitialized() override;
		void OnRun(PgE::World* world) override;
	};
}
