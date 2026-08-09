module;

#include <vulkan/vulkan_core.h>

export module PlaygroundEngine.WindowServer:Window;

import std;

import :BackendDeclarations;
import :WindowServerErrors;
import :WindowSizes;
import :WindowSpecification;

namespace PgE
{
	// Vulkan surface creation is a deliberate deviation: it needs the native handle rather than the
	// connection. It stays required so a second backend hits a hard compile error and has to decide
	// explicitly, instead of failing inside the renderer.

	export template <typename Backend>
	concept WindowBackendInterface = requires(const Backend constBackend, VkInstance vkInstance) {
		{ constBackend.GetSize() } -> std::same_as<WindowSize>;
		{ constBackend.GetFramebufferSize() } -> std::same_as<FramebufferSize>;
		{ constBackend.GetContentScale() } -> std::same_as<ContentScale>;
		{ constBackend.CreateVulkanSurface(vkInstance) } -> std::same_as<std::expected<VkSurfaceKHR, VulkanWindowError>>;
	};

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

		[[nodiscard]] WindowSize GetSize() const;

		/// The authoritative drawable size, queried live.
		[[nodiscard]] FramebufferSize GetFramebufferSize() const;

		/// The display's density hint, used to size interface elements.
		[[nodiscard]] ContentScale GetContentScale() const;
		[[nodiscard]] std::expected<VkSurfaceKHR, VulkanWindowError> CreateVulkanSurface(VkInstance vkInstance) const;

	private:
		friend class WindowServer;

		Window(std::unique_ptr<WindowBackend> backend, WindowSpecification specification);

		std::unique_ptr<WindowBackend> _backend;
		WindowSpecification _specification;
	};
}
