export module PlaygroundEngine;

export import PlaygroundEngine.World;
export import PlaygroundEngine.GameObject;
export import PlaygroundEngine.Window;

import PlaygroundEngine.App;

import std;

namespace PgE
{
	export struct CommandLine
	{
		int Argc = 0;
		char** Argv = nullptr;

		// TODO: parsed accessors for the editor-spawn parameters
		// (--world=, --editor-connect=, --wait-for-debugger).
	};

	export struct AppCapabilities
	{
		bool Presentation = true;
		bool Rendering = true;
		bool Audio = true;
		bool Networking = false;
	};

	export enum class BootError : std::uint8_t
	{
		Platform = 1,
	};

	export class AppDescriptorBase
	{
	public:
		explicit AppDescriptorBase(const CommandLine commandLine) : _commandLine(commandLine)
		{}
		virtual ~AppDescriptorBase() = default;

		[[nodiscard]] virtual AppCapabilities GetCapabilities() const
		{
			return AppCapabilities{};
		}
		[[nodiscard]] virtual std::unique_ptr<AppBase> GetApp() = 0;

		[[nodiscard]] const CommandLine& GetCommandLine() const
		{
			return _commandLine;
		}

	private:
		CommandLine _commandLine;
	};

	export class Engine
	{
	public:
		explicit Engine(AppDescriptorBase& appDescriptor);

		[[nodiscard]] std::expected<void, BootError> Boot();
		void StartRun();
		void Shutdown();

		void RequestStop();

	private:
		[[nodiscard]] std::expected<void, BootError> BootPresentation();

		void Run();
		void RunFrame();

		AppDescriptorBase& _appDescriptor;
		bool _running = false;

		// Members double as the construction-order record: L1 first, then L2,
		// app last; Shutdown() resets in reverse.
		std::unique_ptr<Window> _window;
		std::unique_ptr<World> _world;
		std::unique_ptr<AppBase> _app;
	};
}
