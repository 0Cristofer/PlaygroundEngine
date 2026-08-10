module PlaygroundEngine.WindowServer;

import :WindowBackend;
import :WindowServerBackend;

import std;

namespace PgE
{
	static_assert(WindowServerBackendInterface<WindowServerBackend>, "WindowServerBackend doesn't implement all of WindowServerBackendInterface");

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

	WindowServer::WindowServer(std::unique_ptr<WindowServerBackend> backend) : _backend(std::move(backend))
	{}

	WindowServer::~WindowServer() = default;

	std::expected<Window*, WindowError> WindowServer::CreateWindow(const WindowSpecification& specification)
	{
		auto backend = _backend->CreateWindow(specification);
		if (!backend)
		{
			return std::unexpected(backend.error());
		}

		_windows.push_back(std::unique_ptr<Window>(new Window(std::move(*backend), specification)));

		return _windows.back().get();
	}

	void WindowServer::DestroyWindow(Window* window)
	{
		std::erase_if(_windows, [window](const std::unique_ptr<Window>& owned) { return owned.get() == window; });
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void WindowServer::Pump(PlatformEventRecord& record)
	{
		_backend->Pump(record);
	}

	// ReSharper disable once CppMemberFunctionMayBeConst
	void WindowServer::SetClipboardText(const std::string_view text)
	{
		_backend->SetClipboardText(text);
	}

	std::optional<std::string> WindowServer::GetClipboardText() const
	{
		return _backend->GetClipboardText();
	}

	std::expected<std::span<const char* const>, VulkanWindowError> WindowServer::GetRequiredVulkanExtensions() const
	{
		return _backend->GetRequiredVulkanExtensions();
	}
}
