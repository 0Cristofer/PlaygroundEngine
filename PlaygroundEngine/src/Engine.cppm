export module PlaygroundEngine;

export import PlaygroundEngine.World;
export import PlaygroundEngine.GameObject;
export import PlaygroundEngine.WindowServer;

import PlaygroundEngine.App;
import PlaygroundEngine.Renderer.Vulkan;

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
		Rendering = 2,
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
		// pre: a successful Boot() ran first, so the app and world exist.
		void StartRun() pre(_app != nullptr && _world != nullptr);
		void Shutdown();

		void RequestStop();

	private:
		[[nodiscard]] std::expected<void, BootError> BootPresentation();
		[[nodiscard]] std::expected<void, BootError> BootRendering();

		void Run();
		std::expected<void, RendererError<RendererRenderErrorKind>> RunFrame();

		AppDescriptorBase& _appDescriptor;
		bool _running = false;

		// Members double as the construction-order record: L1 first, then L2,
		// app last; Shutdown() resets in reverse.
		std::unique_ptr<WindowServer> _windowServer;
		Window* _window = nullptr;

		// The root owns the record and hands it to the pump, so a test can fill one by hand and
		// drive every consumer with no window, and a replay producer is a swap here rather than an
		// injection into somebody else's buffer.

		PlatformEventRecord _platformEvents;

		std::unique_ptr<RendererVulkan> _rendererVulkan;

		// Declared after the renderer so they are dropped before it: a subscription outliving its
		// subscriber would deliver a resize into a destroyed object.

		SignalSubscription _windowResizedSubscription;
		SignalSubscription _closeRequestedSubscription;

		std::unique_ptr<World> _world;
		std::unique_ptr<AppBase> _app;
	};
}
