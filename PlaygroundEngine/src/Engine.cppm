export module PlaygroundEngine;

export import PlaygroundEngine.World;
export import PlaygroundEngine.GameObject;
export import PlaygroundEngine.WindowServer;

import PlaygroundEngine.AgentChannel;
import PlaygroundEngine.App;
import PlaygroundEngine.DebugUi;
import PlaygroundEngine.FrameCapture;
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

		[[nodiscard]] std::expected<void, BootError> Boot() pre(_app == nullptr);
		void StartRun() pre(_app != nullptr && _world != nullptr);
		void Shutdown();

		void RequestStop();

	private:
		void Run();
		std::expected<void, RendererError<RendererRenderErrorKind>> RunStep() pre(_world != nullptr);

		void LogPlatformEvents() const;

		std::expected<void, BootError> BootPresentation();
		std::expected<void, BootError> BootRendering();
		void BootDebugUi();

		[[nodiscard]] float AdvanceStepClock();

		AppDescriptorBase& _appDescriptor;
		bool _running = false;

		PlatformEventRecord _platformEvents;

		std::unique_ptr<WindowServer> _windowServer;
		Window* _window = nullptr;

		AgentChannelHost _agentChannel;
		FrameCapture _frameCapture;

		std::chrono::steady_clock::time_point _previousFrameTime = std::chrono::steady_clock::now();

		std::unique_ptr<DebugUi> _debugUi;
		std::unique_ptr<RendererVulkan> _rendererVulkan;
		std::unique_ptr<World> _world;
		std::unique_ptr<AppBase> _app;
	};
}
