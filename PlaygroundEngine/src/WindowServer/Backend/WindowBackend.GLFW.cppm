module;

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer:WindowBackend;

import std;

import :BackendDeclarations;
import :CursorShape;
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

		void SetCursorShape(CursorShape shape);

	private:
		// Built on first request rather than up front, since a window typically asks for two or three
		// of them, and cached because setting one is a round trip to the window system.

		GLFWcursor* GetCursor(CursorShape shape);

		GLFWwindow* _handle;

		// One slot per shape, Hidden's included but never filled, since hiding goes through the input
		// mode rather than a cursor object. A new shape cannot slip past this unnoticed: the switch
		// that maps them carries no default, so -Wswitch stops a build that forgot one.

		std::array<GLFWcursor*, std::to_underlying(CursorShape::Hidden) + 1> _cursors{};
		CursorShape _currentShape = CursorShape::Arrow;
	};
}
