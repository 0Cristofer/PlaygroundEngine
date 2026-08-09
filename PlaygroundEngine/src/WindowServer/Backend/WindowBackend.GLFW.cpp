module;

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer;

import std;

import :WindowBackend;

namespace PgE
{
	WindowBackend::~WindowBackend()
	{
		glfwDestroyWindow(_handle);
	}

	WindowSize WindowBackend::GetSize() const
	{
		int width = 0;
		int height = 0;
		glfwGetWindowSize(_handle, &width, &height);

		return WindowSize{.Width = width, .Height = height};
	}

	FramebufferSize WindowBackend::GetFramebufferSize() const
	{
		int width = 0;
		int height = 0;
		glfwGetFramebufferSize(_handle, &width, &height);

		return FramebufferSize{.Width = width, .Height = height};
	}

	ContentScale WindowBackend::GetContentScale() const
	{
		float horizontal = 1.0f;
		float vertical = 1.0f;
		glfwGetWindowContentScale(_handle, &horizontal, &vertical);

		return ContentScale{.X = horizontal, .Y = vertical};
	}

	std::expected<VkSurfaceKHR, VulkanWindowError> WindowBackend::CreateVulkanSurface(const VkInstance instance) const
	{
		VkSurfaceKHR surface;
		if (glfwCreateWindowSurface(instance, _handle, nullptr, &surface) != 0)
		{
			return std::unexpected(VulkanWindowError::SurfaceCreationFailed);
		}

		return surface;
	}
}
