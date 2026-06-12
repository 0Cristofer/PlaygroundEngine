export module PlaygroundGame;

import PlaygroundEngine;
import PlaygroundEngine.App;

import std;

namespace PlaygroundGame
{
	export class PlaygroundGameAppDescriptor : public PlaygroundEngine::AppDescriptorBase
	{
	public:
		PlaygroundGameAppDescriptor(PlaygroundEngine::CommandLine* commandLine) : AppDescriptorBase(commandLine) {}

		virtual ~PlaygroundGameAppDescriptor() = default;

		std::unique_ptr<PlaygroundEngine::AppBase> GetApp() override;
	};

	export class App : public PlaygroundEngine::AppBase
	{
	public:
		void OnInitialized() override;
		void OnRun(PlaygroundEngine::World* world) override;
	};
}
