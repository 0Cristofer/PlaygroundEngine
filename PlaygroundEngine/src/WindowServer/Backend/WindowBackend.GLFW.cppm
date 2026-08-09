module;

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer:WindowBackend;

import std;

import :BackendDeclarations;
import :WindowServerErrors;
import :WindowSizes;

namespace PgE
{
	class WindowBackend
	{
	public:
		explicit WindowBackend(GLFWwindow* handle) : _handle(handle)
		{}

		~WindowBackend();

		WindowBackend(const WindowBackend&) = delete;
		WindowBackend& operator=(const WindowBackend&) = delete;

		[[nodiscard]] WindowSize GetSize() const;
		[[nodiscard]] FramebufferSize GetFramebufferSize() const;
		[[nodiscard]] ContentScale GetContentScale() const;
		[[nodiscard]] std::expected<VkSurfaceKHR, VulkanWindowError> CreateVulkanSurface(VkInstance instance) const pre(_handle != nullptr);

	private:
		GLFWwindow* _handle;
	};
}
