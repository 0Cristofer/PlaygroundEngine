module;

#include <vulkan/vulkan_core.h>

export module PlaygroundEngine.WindowServer;

export import :common;
export import PlaygroundEngine.Signal;

import std;

namespace PgE
{
	template <typename Backend>
	concept WindowBackendInterface = requires(const Backend constBackend, VkInstance vkInstance) {
		{ constBackend.GetSize() } -> std::same_as<WindowSize>;
		{ constBackend.GetFramebufferSize() } -> std::same_as<FramebufferSize>;
		{ constBackend.CreateVulkanSurface(vkInstance) } -> std::same_as<std::expected<VkSurfaceKHR, VulkanWindowError>>;
	};

	// Vulkan surface creation and the instance extension list are a deliberate deviation: they need
	// the native handle rather than the connection. They stay required so a second backend hits a
	// hard compile error and has to decide explicitly, instead of failing inside the renderer.

	template <typename Backend>
	concept WindowServerBackendInterface =
		requires(Backend backend, const Backend constBackend, const WindowSpecification& specification, PlatformEventRecord& record) {
			{ Backend::Create() } -> std::same_as<std::expected<std::unique_ptr<Backend>, WindowServerError>>;
			{ backend.CreateWindow(specification) } -> std::same_as<std::expected<std::unique_ptr<WindowBackend>, WindowError>>;
			{ backend.Pump(record) } -> std::same_as<void>;
			{ constBackend.GetRequiredVulkanExtensions() } -> std::same_as<std::expected<std::span<const char* const>, VulkanWindowError>>;
		};

	// One object on the connection. It holds only what is per-window; the event pump, the platform
	// lifetime and the instance extension list all belong to the server.

	export class Window
	{
	public:
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		[[nodiscard]] std::string_view GetTitle() const
		{
			return _specification.Title;
		}

		// Queried live rather than remembered from the specification, which is only what creation
		// asked for and stops being true at the first resize.

		[[nodiscard]] WindowSize GetSize() const;

		// The authoritative drawable size, queried live. It reports 0x0 while minimized, which is
		// what makes the renderer's early return correct; a value cached from resize events can
		// miss that zero, because a compositor may send no resize on minimize at all.

		[[nodiscard]] FramebufferSize GetFramebufferSize() const;

		[[nodiscard]] std::expected<VkSurfaceKHR, VulkanWindowError> CreateVulkanSurface(VkInstance vkInstance) const;

	private:
		friend class WindowCollection;

		Window(std::unique_ptr<WindowBackend> backend, WindowSpecification specification);

		std::unique_ptr<WindowBackend> _backend;
		WindowSpecification _specification;
	};

	// The Windows sub-facade. The server vends cohesive facades rather than growing one flat class,
	// because the operations still to come (monitors, clipboard, cursor, IME) are item for item the
	// contents of a two-hundred-method display singleton.

	export class WindowCollection
	{
	public:
		WindowCollection(const WindowCollection&) = delete;
		WindowCollection& operator=(const WindowCollection&) = delete;

		[[nodiscard]] std::expected<Window*, WindowError> Create(const WindowSpecification& specification);
		void Destroy(Window* window);

	private:
		friend class WindowServer;

		explicit WindowCollection(WindowServerBackend& backend) : _backend(backend)
		{}

		WindowServerBackend& _backend;
		std::vector<std::unique_ptr<Window>> _windows;
	};

	// The engine's connection to the OS window system, and everything that must be performed over
	// that connection. It is not the input system: it classifies nothing, filters nothing, and
	// keeps no key state.

	export class WindowServer
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<WindowServer>, WindowServerError> Create();

		~WindowServer();

		WindowServer(const WindowServer&) = delete;
		WindowServer& operator=(const WindowServer&) = delete;

		[[nodiscard]] WindowCollection& GetWindows()
		{
			return _windows;
		}
		[[nodiscard]] const WindowCollection& GetWindows() const
		{
			return _windows;
		}

		// Clears the record, drains the OS queue into it, and returns. Main thread only, and not
		// guaranteed to return promptly: dragging or resizing a window enters a nested modal loop
		// on some platforms, stopping the frame loop until it ends.

		void Pump(PlatformEventRecord& record);

		// A drain point, called explicitly by the root, never from inside an OS callback.

		void DispatchWindowEvents(const PlatformEventRecord& record);

		[[nodiscard]] std::expected<std::span<const char* const>, VulkanWindowError> GetRequiredVulkanExtensions() const;

		[[nodiscard]] Signal<FramebufferSize>& OnWindowResized()
		{
			return _windowResized;
		}

		// Fired when the window system asks for a close. Whether that ends the application is
		// policy for a layer above, which is why this is a signal and not a sticky flag.

		[[nodiscard]] Signal<>& OnCloseRequested()
		{
			return _closeRequested;
		}

	private:
		explicit WindowServer(std::unique_ptr<WindowServerBackend> backend);

		// Declared before the windows so they are destroyed first and the connection outlives every
		// object created through it.

		std::unique_ptr<WindowServerBackend> _backend;
		WindowCollection _windows;

		Signal<FramebufferSize> _windowResized;
		Signal<> _closeRequested;
	};
}
