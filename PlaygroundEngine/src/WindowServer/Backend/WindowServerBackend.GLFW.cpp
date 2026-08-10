module;

#include "PlaygroundEngine/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer;

import PlaygroundEngine.Log;
import PlaygroundEngine.PlatformEvents;
import std;

import :GlfwInputTranslation;
import :WindowBackend;
import :WindowServerBackend;

namespace PgE
{
	namespace
	{
		void OnGlfwError(const int code, const char* description)
		{
			// Two of GLFW's codes mean "the platform declines", not "the engine is broken". An empty
			// clipboard is routine, and Wayland refuses whole calls the other backends serve (window
			// position, cursor position), so neither is worth an error line.

			switch (code)
			{
			case GLFW_FORMAT_UNAVAILABLE:
				PGE_LOG(Trace, "GLFW: {}", description);
				break;
			case GLFW_FEATURE_UNAVAILABLE:
				PGE_LOG(Warn, "GLFW: {}", description);
				break;
			default:
				PGE_LOG(Error, "GLFW error {}: {}", code, description);
				break;
			}
		}

		// GLFW exposes no event timestamps, so arrival at the pump is the best available base and
		// the precision is documented as exactly that. A native backend supplies real ones.

		std::uint64_t TimestampNow()
		{
			return static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
		}

		// Diagnostic only, not the refcount this replaced: glfwTerminate is process-global, so a
		// second connection's destructor would destroy the first one's windows and leave its handles
		// dangling. glfwInit succeeds silently on an already-initialized GLFW, so nothing else catches it.

		bool s_connectionExists = false;
	}

	std::expected<std::unique_ptr<WindowServerBackend>, WindowServerError> WindowServerBackend::Create()
	{
		// Guards the process-global glfwTerminate below; the composition root is the only thing that
		// should construct a window server, and it constructs exactly one.
		contract_assert(!s_connectionExists);

		glfwSetErrorCallback(OnGlfwError);

		if (glfwInit() != GLFW_TRUE)
		{
			return std::unexpected(WindowServerError::ConnectionFailed);
		}

		s_connectionExists = true;

		PGE_LOG(Info, "Connected to the window system through GLFW");

		return std::unique_ptr<WindowServerBackend>(new WindowServerBackend());
	}

	WindowServerBackend::~WindowServerBackend()
	{
		glfwTerminate();

		s_connectionExists = false;
	}

	std::expected<std::unique_ptr<WindowBackend>, WindowError> WindowServerBackend::CreateWindow(const WindowSpecification& specification)
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		GLFWwindow* handle = glfwCreateWindow(specification.Width, specification.Height, specification.Title.c_str(), nullptr, nullptr);

		if (handle == nullptr)
		{
			return std::unexpected(WindowError::WindowCreationFailed);
		}

		// Off by default, and without it GLFW reports no Caps Lock or Num Lock state at all, which
		// is the entire justification for the record carrying InputLockState.

		glfwSetInputMode(handle, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

		// The user pointer carries the server rather than the window: with no window identity field
		// in the record, a callback needs nothing but the server's pending batch.

		glfwSetWindowUserPointer(handle, this);

		glfwSetKeyCallback(handle, OnKey);
		glfwSetCharCallback(handle, OnCharacter);
		glfwSetCursorPosCallback(handle, OnCursorPosition);
		glfwSetMouseButtonCallback(handle, OnPointerButton);
		glfwSetScrollCallback(handle, OnScroll);
		glfwSetWindowFocusCallback(handle, OnWindowFocus);
		glfwSetFramebufferSizeCallback(handle, OnFramebufferSize);
		glfwSetWindowCloseCallback(handle, OnWindowClose);

		PGE_LOG(Info, "Created GLFW window \"{}\" ({}x{})", specification.Title, specification.Width, specification.Height);

		return std::make_unique<WindowBackend>(handle);
	}

	void WindowServerBackend::Pump(PlatformEventRecord& record)
	{
		glfwPollEvents();

		record.Append(_pendingEvents.GetEvents());
		_pendingEvents.Clear();
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	void WindowServerBackend::SetClipboardText(const std::string_view text)
	{
		// Copied because GLFW takes a null-terminated string and a view carries no such promise. GLFW
		// copies it again on its side, so nothing here has to outlive the call.

		const std::string terminated(text);

		glfwSetClipboardString(nullptr, terminated.c_str());
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	std::optional<std::string> WindowServerBackend::GetClipboardText() const
	{
		// Null covers an empty clipboard and one holding something that is not text. Neither is a
		// failure, so both come back as nothing rather than as an error.

		const char* text = glfwGetClipboardString(nullptr);
		if (text == nullptr)
		{
			return std::nullopt;
		}

		return std::string(text);
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	std::expected<std::span<const char* const>, VulkanWindowError> WindowServerBackend::GetRequiredVulkanExtensions() const
	{
		std::uint32_t extensionCount = 0;
		const char** extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);

		// GLFW yields null when the Vulkan loader is missing and does not define what it leaves in
		// the count, so the pointer is the only signal that can be trusted. A working Vulkan build
		// always reports at least a surface extension, making an empty list a broken platform.

		if (extensionNames == nullptr || extensionCount == 0)
		{
			return std::unexpected(VulkanWindowError::ExtensionsUnavailable);
		}

		return std::span<const char* const>{extensionNames, extensionCount};
	}

	void WindowServerBackend::AppendEvent(GLFWwindow* handle, const PlatformEvent& event)
	{
		auto* backend = static_cast<WindowServerBackend*>(glfwGetWindowUserPointer(handle));
		if (backend == nullptr)
		{
			return;
		}

		PlatformEvent stampedEvent = event;
		stampedEvent.Timestamp = TimestampNow();

		backend->_pendingEvents.Append(stampedEvent);
	}

	void WindowServerBackend::OnKey(GLFWwindow* handle, const int key, const int scancode, const int action, const int modifiers)
	{
		if (action != GLFW_PRESS && action != GLFW_RELEASE && action != GLFW_REPEAT)
		{
			return;
		}

		AppendEvent(handle, PlatformEvent{
								.Type = action == GLFW_RELEASE ? PlatformEventType::KeyReleased : PlatformEventType::KeyPressed,
								.Code = TranslateGlfwKey(key),
								.Modifiers = TranslateGlfwModifiers(modifiers),
								.Locks = TranslateGlfwLockState(modifiers),
								.Repeat = action == GLFW_REPEAT,
								.Token = PlatformKeyToken{.Value = scancode},
							});
	}

	void WindowServerBackend::OnCharacter(GLFWwindow* handle, const unsigned int codepoint)
	{
		AppendEvent(handle, PlatformEvent{.Type = PlatformEventType::CharacterTyped, .Codepoint = static_cast<char32_t>(codepoint)});
	}

	void WindowServerBackend::OnCursorPosition(GLFWwindow* handle, const double x, const double y)
	{
		AppendEvent(handle, PlatformEvent{.Type = PlatformEventType::PointerMoved, .X = static_cast<float>(x), .Y = static_cast<float>(y)});
	}

	void WindowServerBackend::OnPointerButton(GLFWwindow* handle, const int button, const int action, const int modifiers)
	{
		AppendEvent(handle, PlatformEvent{
								.Type = action == GLFW_RELEASE ? PlatformEventType::PointerButtonReleased : PlatformEventType::PointerButtonPressed,
								.Code = TranslateGlfwPointerButton(button),
								.Modifiers = TranslateGlfwModifiers(modifiers),
								.Locks = TranslateGlfwLockState(modifiers),
							});
	}

	void WindowServerBackend::OnScroll(GLFWwindow* handle, const double xOffset, const double yOffset)
	{
		AppendEvent(handle,
					PlatformEvent{.Type = PlatformEventType::PointerScrolled, .X = static_cast<float>(xOffset), .Y = static_cast<float>(yOffset)});
	}

	void WindowServerBackend::OnWindowFocus(GLFWwindow* handle, const int focused)
	{
		AppendEvent(handle, PlatformEvent{.Type = focused == GLFW_TRUE ? PlatformEventType::FocusGained : PlatformEventType::FocusLost});
	}

	void WindowServerBackend::OnFramebufferSize(GLFWwindow* handle, const int width, const int height)
	{
		AppendEvent(handle, PlatformEvent{.Type = PlatformEventType::WindowResized, .X = static_cast<float>(width), .Y = static_cast<float>(height)});
	}

	void WindowServerBackend::OnWindowClose(GLFWwindow* handle)
	{
		// GLFW's own close flag is sticky and would have to be cleared to keep the window alive. As
		// an event it is simply unhandled, which is what a veto ("unsaved changes") needs.

		glfwSetWindowShouldClose(handle, GLFW_FALSE);

		AppendEvent(handle, PlatformEvent{.Type = PlatformEventType::CloseRequested});
	}
}
