module;

#include <vulkan/vulkan.h>

module PlaygroundEngine.Window;

import :backend;

import std;

namespace PgE
{
	static_assert(WindowBackendInterface<WindowBackend>, "WindowBackend doesn't implement all of WindowBackendInterface");

	std::expected<std::unique_ptr<Window>, WindowError> Window::Create(const WindowSpecification& specification)
	{
		// The window context is fallible (the platform may fail to initialize, the
		// compositor may refuse the surface), so creation is a factory returning the
		// engine error model rather than a throwing constructor. Native runtime zone.

		auto backend = WindowBackend::Create(specification);
		if (!backend)
		{
			return std::unexpected(backend.error());
		}

		return std::unique_ptr<Window>(new Window(std::move(*backend), specification));
	}

	Window::~Window() = default;

	void Window::PollEvents()
	{
		_backend->PollEvents();
	}

	bool Window::ShouldClose() const
	{
		return _backend->ShouldClose();
	}

	FramebufferSize Window::GetFramebufferSize() const
	{
		return _backend->GetFramebufferSize();
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void Window::SetFramebufferResizedCallback(FramebufferResizedCallback callback)
	{
		_backend->SetFramebufferResizedCallback(std::move(callback));
	}

	std::expected<std::span<const char* const>, VulkanWindowError> Window::GetRequiredVulkanExtensions() const
	{
		return _backend->GetRequiredVulkanExtensions();
	}

	std::expected<VkSurfaceKHR, VulkanWindowError> Window::CreateVulkanSurface(const VkInstance vkInstance) const
	{
		return _backend->CreateVulkanSurface(vkInstance);
	}

	Window::Window(std::unique_ptr<WindowBackend> backend, WindowSpecification specification)
		: _backend(std::move(backend)), _specification(std::move(specification))
	{}
}
