module;

#include <vulkan/vulkan.h>

module PlaygroundEngine.WindowServer;

import :WindowBackend;

import std;

namespace PgE
{
	static_assert(WindowBackendInterface<WindowBackend>, "WindowBackend doesn't implement all of WindowBackendInterface");

	Window::Window(std::unique_ptr<WindowBackend> backend, WindowSpecification specification)
		: _backend(std::move(backend)), _specification(std::move(specification))
	{}

	Window::~Window() = default;

	WindowSize Window::GetSize() const
	{
		return _backend->GetSize();
	}

	FramebufferSize Window::GetFramebufferSize() const
	{
		return _backend->GetFramebufferSize();
	}

	ContentScale Window::GetContentScale() const
	{
		return _backend->GetContentScale();
	}

	std::expected<VkSurfaceKHR, VulkanWindowError> Window::CreateVulkanSurface(const VkInstance vkInstance) const
	{
		return _backend->CreateVulkanSurface(vkInstance);
	}
}
