export module PlaygroundGame;

import PlaygroundEngine;
import PlaygroundEngine.App;
import PlaygroundEngine.DebugUi;
import PlaygroundEngine.Ecs;

import std;
import PlaygroundEngine.Ecs.InputSystem;

namespace PgG
{
	export class PlaygroundGameAppDescriptor : public PgE::AppDescriptorBase
	{
	public:
		explicit PlaygroundGameAppDescriptor(const PgE::CommandLine commandLine) : AppDescriptorBase(commandLine)
		{}

		std::unique_ptr<PgE::AppBase> GetApp() override;
	};

	export class App : public PgE::AppBase
	{
	public:
		void OnBooted(PgE::EngineContext& engine) override;
		void OnStartRun(PgE::Ecs& ecs) override;
		void OnStep(const PgE::PlatformEventRecord& platformEventRecord) override;

	private:
		PgE::InputSystem* _inputSystem = nullptr;
	};
}
