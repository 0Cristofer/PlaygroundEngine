module;

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer:WindowServerBackend;

import PlaygroundEngine.PlatformEvents;
import std;

import :BackendDeclarations;
import :WindowServerErrors;
import :WindowSpecification;

namespace PgE
{
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

		static void AppendEvent(GLFWwindow* handle, const PlatformEvent& event);

		static void OnKey(GLFWwindow* handle, int key, int scancode, int action, int modifiers);
		static void OnCharacter(GLFWwindow* handle, unsigned int codepoint);
		static void OnCursorPosition(GLFWwindow* handle, double x, double y);
		static void OnPointerButton(GLFWwindow* handle, int button, int action, int modifiers);
		static void OnScroll(GLFWwindow* handle, double xOffset, double yOffset);
		static void OnWindowFocus(GLFWwindow* handle, int focused);
		static void OnFramebufferSize(GLFWwindow* handle, int width, int height);
		static void OnWindowClose(GLFWwindow* handle);

		PlatformEventRecord _pendingEvents;
	};
}
