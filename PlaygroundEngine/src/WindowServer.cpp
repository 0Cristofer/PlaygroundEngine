module;

#include <vulkan/vulkan.h>

module PlaygroundEngine.WindowServer;

import :backend;

import std;

namespace PgE
{
	static_assert(WindowServerBackendInterface<WindowServerBackend>, "WindowServerBackend doesn't implement all of WindowServerBackendInterface");
	static_assert(WindowBackendInterface<WindowBackend>, "WindowBackend doesn't implement all of WindowBackendInterface");

	std::expected<std::unique_ptr<WindowServer>, WindowServerError> WindowServer::Create()
	{
		// The connection is fallible (there may be no compositor, or no window system at all), so
		// creation is a factory returning the engine error model rather than a throwing
		// constructor. Native runtime zone.

		auto backend = WindowServerBackend::Create();
		if (!backend)
		{
			return std::unexpected(backend.error());
		}

		return std::unique_ptr<WindowServer>(new WindowServer(std::move(*backend)));
	}

	WindowServer::WindowServer(std::unique_ptr<WindowServerBackend> backend) : _backend(std::move(backend)), _windows(*_backend)
	{}

	WindowServer::~WindowServer() = default;

	void WindowServer::Pump(PlatformEventRecord& record)
	{
		_backend->Pump(record);
	}

	void WindowServer::DispatchWindowEvents(const PlatformEventRecord& record)
	{
		for (const PlatformEvent& event : record.GetEvents())
		{
			switch (event.Type)
			{
				case PlatformEventType::WindowResized:
					_windowResized.Emit(FramebufferSize{.Width = static_cast<int>(event.X), .Height = static_cast<int>(event.Y)});
					break;

				case PlatformEventType::CloseRequested:
					_closeRequested.Emit();
					break;

				default:
					break;
			}
		}
	}

	std::expected<std::span<const char* const>, VulkanWindowError> WindowServer::GetRequiredVulkanExtensions() const
	{
		return _backend->GetRequiredVulkanExtensions();
	}

	std::expected<Window*, WindowError> WindowCollection::Create(const WindowSpecification& specification)
	{
		auto backend = _backend.CreateWindow(specification);
		if (!backend)
		{
			return std::unexpected(backend.error());
		}

		_windows.push_back(std::unique_ptr<Window>(new Window(std::move(*backend), specification)));

		return _windows.back().get();
	}

	void WindowCollection::Destroy(Window* window)
	{
		std::erase_if(_windows, [window](const std::unique_ptr<Window>& owned) { return owned.get() == window; });
	}

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

	std::expected<VkSurfaceKHR, VulkanWindowError> Window::CreateVulkanSurface(const VkInstance vkInstance) const
	{
		return _backend->CreateVulkanSurface(vkInstance);
	}
}
