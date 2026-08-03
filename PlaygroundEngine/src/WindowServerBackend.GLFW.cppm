module;

#include "PlaygroundEngine/Log.h"

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer:backend;

import PlaygroundEngine.Log;
import std;

import :common;

namespace PgE
{
	namespace
	{
		void OnGlfwError(const int code, const char* description)
		{
			PGE_LOG(Error, "GLFW error {}: {}", code, description);
		}

		// GLFW key tokens are already physical positions, which makes this a straight relabelling
		// rather than a layout translation.

		InputCode TranslateKey(const int key)
		{
			switch (key)
			{
				case GLFW_KEY_A:
					return InputCode::KeyA;
				case GLFW_KEY_B:
					return InputCode::KeyB;
				case GLFW_KEY_C:
					return InputCode::KeyC;
				case GLFW_KEY_D:
					return InputCode::KeyD;
				case GLFW_KEY_E:
					return InputCode::KeyE;
				case GLFW_KEY_F:
					return InputCode::KeyF;
				case GLFW_KEY_G:
					return InputCode::KeyG;
				case GLFW_KEY_H:
					return InputCode::KeyH;
				case GLFW_KEY_I:
					return InputCode::KeyI;
				case GLFW_KEY_J:
					return InputCode::KeyJ;
				case GLFW_KEY_K:
					return InputCode::KeyK;
				case GLFW_KEY_L:
					return InputCode::KeyL;
				case GLFW_KEY_M:
					return InputCode::KeyM;
				case GLFW_KEY_N:
					return InputCode::KeyN;
				case GLFW_KEY_O:
					return InputCode::KeyO;
				case GLFW_KEY_P:
					return InputCode::KeyP;
				case GLFW_KEY_Q:
					return InputCode::KeyQ;
				case GLFW_KEY_R:
					return InputCode::KeyR;
				case GLFW_KEY_S:
					return InputCode::KeyS;
				case GLFW_KEY_T:
					return InputCode::KeyT;
				case GLFW_KEY_U:
					return InputCode::KeyU;
				case GLFW_KEY_V:
					return InputCode::KeyV;
				case GLFW_KEY_W:
					return InputCode::KeyW;
				case GLFW_KEY_X:
					return InputCode::KeyX;
				case GLFW_KEY_Y:
					return InputCode::KeyY;
				case GLFW_KEY_Z:
					return InputCode::KeyZ;

				case GLFW_KEY_0:
					return InputCode::Key0;
				case GLFW_KEY_1:
					return InputCode::Key1;
				case GLFW_KEY_2:
					return InputCode::Key2;
				case GLFW_KEY_3:
					return InputCode::Key3;
				case GLFW_KEY_4:
					return InputCode::Key4;
				case GLFW_KEY_5:
					return InputCode::Key5;
				case GLFW_KEY_6:
					return InputCode::Key6;
				case GLFW_KEY_7:
					return InputCode::Key7;
				case GLFW_KEY_8:
					return InputCode::Key8;
				case GLFW_KEY_9:
					return InputCode::Key9;

				case GLFW_KEY_SPACE:
					return InputCode::KeySpace;
				case GLFW_KEY_APOSTROPHE:
					return InputCode::KeyApostrophe;
				case GLFW_KEY_COMMA:
					return InputCode::KeyComma;
				case GLFW_KEY_MINUS:
					return InputCode::KeyMinus;
				case GLFW_KEY_PERIOD:
					return InputCode::KeyPeriod;
				case GLFW_KEY_SLASH:
					return InputCode::KeySlash;
				case GLFW_KEY_SEMICOLON:
					return InputCode::KeySemicolon;
				case GLFW_KEY_EQUAL:
					return InputCode::KeyEqual;
				case GLFW_KEY_LEFT_BRACKET:
					return InputCode::KeyLeftBracket;
				case GLFW_KEY_BACKSLASH:
					return InputCode::KeyBackslash;
				case GLFW_KEY_RIGHT_BRACKET:
					return InputCode::KeyRightBracket;
				case GLFW_KEY_GRAVE_ACCENT:
					return InputCode::KeyGraveAccent;
				case GLFW_KEY_WORLD_1:
					return InputCode::KeyWorld1;
				case GLFW_KEY_WORLD_2:
					return InputCode::KeyWorld2;

				case GLFW_KEY_ESCAPE:
					return InputCode::KeyEscape;
				case GLFW_KEY_ENTER:
					return InputCode::KeyEnter;
				case GLFW_KEY_TAB:
					return InputCode::KeyTab;
				case GLFW_KEY_BACKSPACE:
					return InputCode::KeyBackspace;
				case GLFW_KEY_INSERT:
					return InputCode::KeyInsert;
				case GLFW_KEY_DELETE:
					return InputCode::KeyDelete;
				case GLFW_KEY_RIGHT:
					return InputCode::KeyRight;
				case GLFW_KEY_LEFT:
					return InputCode::KeyLeft;
				case GLFW_KEY_DOWN:
					return InputCode::KeyDown;
				case GLFW_KEY_UP:
					return InputCode::KeyUp;
				case GLFW_KEY_PAGE_UP:
					return InputCode::KeyPageUp;
				case GLFW_KEY_PAGE_DOWN:
					return InputCode::KeyPageDown;
				case GLFW_KEY_HOME:
					return InputCode::KeyHome;
				case GLFW_KEY_END:
					return InputCode::KeyEnd;
				case GLFW_KEY_CAPS_LOCK:
					return InputCode::KeyCapsLock;
				case GLFW_KEY_SCROLL_LOCK:
					return InputCode::KeyScrollLock;
				case GLFW_KEY_NUM_LOCK:
					return InputCode::KeyNumLock;
				case GLFW_KEY_PRINT_SCREEN:
					return InputCode::KeyPrintScreen;
				case GLFW_KEY_PAUSE:
					return InputCode::KeyPause;
				case GLFW_KEY_MENU:
					return InputCode::KeyMenu;

				case GLFW_KEY_F1:
					return InputCode::KeyF1;
				case GLFW_KEY_F2:
					return InputCode::KeyF2;
				case GLFW_KEY_F3:
					return InputCode::KeyF3;
				case GLFW_KEY_F4:
					return InputCode::KeyF4;
				case GLFW_KEY_F5:
					return InputCode::KeyF5;
				case GLFW_KEY_F6:
					return InputCode::KeyF6;
				case GLFW_KEY_F7:
					return InputCode::KeyF7;
				case GLFW_KEY_F8:
					return InputCode::KeyF8;
				case GLFW_KEY_F9:
					return InputCode::KeyF9;
				case GLFW_KEY_F10:
					return InputCode::KeyF10;
				case GLFW_KEY_F11:
					return InputCode::KeyF11;
				case GLFW_KEY_F12:
					return InputCode::KeyF12;
				case GLFW_KEY_F13:
					return InputCode::KeyF13;
				case GLFW_KEY_F14:
					return InputCode::KeyF14;
				case GLFW_KEY_F15:
					return InputCode::KeyF15;
				case GLFW_KEY_F16:
					return InputCode::KeyF16;
				case GLFW_KEY_F17:
					return InputCode::KeyF17;
				case GLFW_KEY_F18:
					return InputCode::KeyF18;
				case GLFW_KEY_F19:
					return InputCode::KeyF19;
				case GLFW_KEY_F20:
					return InputCode::KeyF20;
				case GLFW_KEY_F21:
					return InputCode::KeyF21;
				case GLFW_KEY_F22:
					return InputCode::KeyF22;
				case GLFW_KEY_F23:
					return InputCode::KeyF23;
				case GLFW_KEY_F24:
					return InputCode::KeyF24;
				case GLFW_KEY_F25:
					return InputCode::KeyF25;

				case GLFW_KEY_KP_0:
					return InputCode::KeyNumpad0;
				case GLFW_KEY_KP_1:
					return InputCode::KeyNumpad1;
				case GLFW_KEY_KP_2:
					return InputCode::KeyNumpad2;
				case GLFW_KEY_KP_3:
					return InputCode::KeyNumpad3;
				case GLFW_KEY_KP_4:
					return InputCode::KeyNumpad4;
				case GLFW_KEY_KP_5:
					return InputCode::KeyNumpad5;
				case GLFW_KEY_KP_6:
					return InputCode::KeyNumpad6;
				case GLFW_KEY_KP_7:
					return InputCode::KeyNumpad7;
				case GLFW_KEY_KP_8:
					return InputCode::KeyNumpad8;
				case GLFW_KEY_KP_9:
					return InputCode::KeyNumpad9;
				case GLFW_KEY_KP_DECIMAL:
					return InputCode::KeyNumpadDecimal;
				case GLFW_KEY_KP_DIVIDE:
					return InputCode::KeyNumpadDivide;
				case GLFW_KEY_KP_MULTIPLY:
					return InputCode::KeyNumpadMultiply;
				case GLFW_KEY_KP_SUBTRACT:
					return InputCode::KeyNumpadSubtract;
				case GLFW_KEY_KP_ADD:
					return InputCode::KeyNumpadAdd;
				case GLFW_KEY_KP_ENTER:
					return InputCode::KeyNumpadEnter;
				case GLFW_KEY_KP_EQUAL:
					return InputCode::KeyNumpadEqual;

				case GLFW_KEY_LEFT_SHIFT:
					return InputCode::KeyLeftShift;
				case GLFW_KEY_LEFT_CONTROL:
					return InputCode::KeyLeftControl;
				case GLFW_KEY_LEFT_ALT:
					return InputCode::KeyLeftAlt;
				case GLFW_KEY_LEFT_SUPER:
					return InputCode::KeyLeftSuper;
				case GLFW_KEY_RIGHT_SHIFT:
					return InputCode::KeyRightShift;
				case GLFW_KEY_RIGHT_CONTROL:
					return InputCode::KeyRightControl;
				case GLFW_KEY_RIGHT_ALT:
					return InputCode::KeyRightAlt;
				case GLFW_KEY_RIGHT_SUPER:
					return InputCode::KeyRightSuper;

				default:
					return InputCode::Unknown;
			}
		}

		InputCode TranslatePointerButton(const int button)
		{
			switch (button)
			{
				case GLFW_MOUSE_BUTTON_LEFT:
					return InputCode::PointerButtonLeft;
				case GLFW_MOUSE_BUTTON_RIGHT:
					return InputCode::PointerButtonRight;
				case GLFW_MOUSE_BUTTON_MIDDLE:
					return InputCode::PointerButtonMiddle;
				case GLFW_MOUSE_BUTTON_4:
					return InputCode::PointerButtonExtra1;
				case GLFW_MOUSE_BUTTON_5:
					return InputCode::PointerButtonExtra2;
				case GLFW_MOUSE_BUTTON_6:
					return InputCode::PointerButtonExtra3;
				case GLFW_MOUSE_BUTTON_7:
					return InputCode::PointerButtonExtra4;
				case GLFW_MOUSE_BUTTON_8:
					return InputCode::PointerButtonExtra5;
				default:
					return InputCode::Unknown;
			}
		}

		InputModifiers TranslateModifiers(const int modifiers)
		{
			return InputModifiers{
				.Shift = (modifiers & GLFW_MOD_SHIFT) != 0,
				.Control = (modifiers & GLFW_MOD_CONTROL) != 0,
				.Alt = (modifiers & GLFW_MOD_ALT) != 0,
				.Super = (modifiers & GLFW_MOD_SUPER) != 0,
				.CapsLock = (modifiers & GLFW_MOD_CAPS_LOCK) != 0,
				.NumLock = (modifiers & GLFW_MOD_NUM_LOCK) != 0,
			};
		}

		// GLFW exposes no event timestamps, so arrival at the pump is the best available base and
		// the precision is documented as exactly that. A native backend supplies real ones.

		std::uint64_t TimestampNow()
		{
			return static_cast<std::uint64_t>(
				std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
		}
	}

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
		[[nodiscard]] std::expected<VkSurfaceKHR, VulkanWindowError> CreateVulkanSurface(VkInstance instance) const pre(_handle != nullptr);

	private:
		GLFWwindow* _handle;
	};

	class WindowServerBackend
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<WindowServerBackend>, WindowServerError> Create();

		~WindowServerBackend();

		WindowServerBackend(const WindowServerBackend&) = delete;
		WindowServerBackend& operator=(const WindowServerBackend&) = delete;

		[[nodiscard]] std::expected<std::unique_ptr<WindowBackend>, WindowError> CreateWindow(const WindowSpecification& specification);

		void Pump(PlatformEventRecord& record);

		[[nodiscard]] std::expected<std::span<const char* const>, VulkanWindowError> GetRequiredVulkanExtensions() const;

	private:
		WindowServerBackend() = default;

		// Callbacks append to the active record and do nothing else. Anything more would run inside
		// the window system's own reentrant dispatch, which is where deferred message queues in
		// every other engine came from.

		static void AppendEvent(GLFWwindow* handle, const PlatformEvent& event);

		static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int modifiers);
		static void OnCharacter(GLFWwindow* handle, unsigned int codepoint);
		static void OnCursorPosition(GLFWwindow* handle, double x, double y);
		static void OnPointerButton(GLFWwindow* handle, int button, int action, int modifiers);
		static void OnScroll(GLFWwindow* handle, double xOffset, double yOffset);
		static void OnWindowFocus(GLFWwindow* handle, int focused);
		static void OnFramebufferSize(GLFWwindow* handle, int width, int height);
		static void OnWindowClose(GLFWwindow* handle);

		PlatformEventRecord* _activeRecord = nullptr;
	};

	std::expected<std::unique_ptr<WindowServerBackend>, WindowServerError> WindowServerBackend::Create()
	{
		glfwSetErrorCallback(OnGlfwError);

		if (glfwInit() != GLFW_TRUE)
		{
			return std::unexpected(WindowServerError::ConnectionFailed);
		}

		PGE_LOG(Info, "Connected to the window system through GLFW");

		return std::unique_ptr<WindowServerBackend>(new WindowServerBackend());
	}

	WindowServerBackend::~WindowServerBackend()
	{
		glfwTerminate();
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
		// is the entire justification for carrying modifiers per event.

		glfwSetInputMode(handle, GLFW_LOCK_KEY_MODS, GLFW_TRUE);

		// The user pointer carries the server rather than the window: with no window identity field
		// in the record, a callback needs nothing but the record the server is currently filling.

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
		record.Clear();

		_activeRecord = &record;
		glfwPollEvents();
		_activeRecord = nullptr;
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
		if (backend == nullptr || backend->_activeRecord == nullptr)
		{
			return;
		}

		PlatformEvent stampedEvent = event;
		stampedEvent.Timestamp = TimestampNow();

		backend->_activeRecord->Append(stampedEvent);
	}

	void WindowServerBackend::OnKey(GLFWwindow* handle, const int key, const int scancode, const int action, const int modifiers)
	{
		if (action != GLFW_PRESS && action != GLFW_RELEASE && action != GLFW_REPEAT)
		{
			return;
		}

		AppendEvent(handle,
					PlatformEvent{
						.Type = action == GLFW_RELEASE ? PlatformEventType::KeyReleased : PlatformEventType::KeyPressed,
						.Code = TranslateKey(key),
						.Modifiers = TranslateModifiers(modifiers),
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
		AppendEvent(handle,
					PlatformEvent{
						.Type = action == GLFW_RELEASE ? PlatformEventType::PointerButtonReleased : PlatformEventType::PointerButtonPressed,
						.Code = TranslatePointerButton(button),
						.Modifiers = TranslateModifiers(modifiers),
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
		AppendEvent(handle,
					PlatformEvent{.Type = PlatformEventType::WindowResized, .X = static_cast<float>(width), .Y = static_cast<float>(height)});
	}

	void WindowServerBackend::OnWindowClose(GLFWwindow* handle)
	{
		// GLFW's own close flag is sticky and would have to be cleared to keep the window alive. As
		// an event it is simply unhandled, which is what a veto ("unsaved changes") needs.

		glfwSetWindowShouldClose(handle, GLFW_FALSE);

		AppendEvent(handle, PlatformEvent{.Type = PlatformEventType::CloseRequested});
	}

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
