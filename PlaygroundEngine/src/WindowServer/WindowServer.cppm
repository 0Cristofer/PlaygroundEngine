export module PlaygroundEngine.WindowServer;

export import :BackendDeclarations;
export import :Window;
export import :WindowServerErrors;
export import :WindowSizes;
export import :WindowSpecification;

export import PlaygroundEngine.PlatformEvents;

import std;

namespace PgE
{
	export template <typename Backend>
	concept WindowServerBackendInterface =
		requires(Backend backend, const Backend constBackend, const WindowSpecification& specification, PlatformEventRecord& record) {
			{ Backend::Create() } -> std::same_as<std::expected<std::unique_ptr<Backend>, WindowServerError>>;
			{ backend.CreateWindow(specification) } -> std::same_as<std::expected<std::unique_ptr<WindowBackend>, WindowError>>;
			{ backend.Pump(record) } -> std::same_as<void>;
			{ constBackend.GetRequiredVulkanExtensions() } -> std::same_as<std::expected<std::span<const char* const>, VulkanWindowError>>;
		};

	/// The engine's connection to the OS window system, and everything that must be performed over
	/// that connection. It is not the input system: it classifies nothing, filters nothing, and
	/// keeps no key state.
	export class WindowServer
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<WindowServer>, WindowServerError> Create();

		~WindowServer();

		WindowServer(const WindowServer&) = delete;
		WindowServer& operator=(const WindowServer&) = delete;

		[[nodiscard]] std::expected<Window*, WindowError> CreateWindow(const WindowSpecification& specification);
		void DestroyWindow(Window* window);

		/// Drains the OS queue and appends it to the record, never clearing it: the record's owner is what
		/// empties it, so a second producer cannot erase this one's batch. Main thread only, and not
		/// guaranteed to return promptly, since a window drag enters a nested modal loop on some platforms.
		void Pump(PlatformEventRecord& record);

		[[nodiscard]] std::expected<std::span<const char* const>, VulkanWindowError> GetRequiredVulkanExtensions() const;

	private:
		explicit WindowServer(std::unique_ptr<WindowServerBackend> backend);

		std::unique_ptr<WindowServerBackend> _backend;
		std::vector<std::unique_ptr<Window>> _windows;
	};
}
