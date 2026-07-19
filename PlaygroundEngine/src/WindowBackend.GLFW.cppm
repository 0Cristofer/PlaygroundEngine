module;

#include "PlaygroundEngine/Log.h"

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

module PlaygroundEngine.Window:backend;

import PlaygroundEngine.Log;
import std;

import :common;

namespace PgE
{
	namespace
	{
		// GLFW's init/terminate are process-global, so windows share one platform
		// instance reference-counted here: the first window brings it up, the last
		// tears it down. Single-threaded; GLFW requires these calls on the main thread.

		int s_liveWindowCount = 0;

		void OnGlfwError(const int code, const char* description)
		{
			PGE_LOG(Error, "GLFW error {}: {}", code, description);
		}

		bool EnsurePlatformInitialized()
		{
			if (s_liveWindowCount > 0)
			{
				return true;
			}

			glfwSetErrorCallback(OnGlfwError);
			return glfwInit() == GLFW_TRUE;
		}
	}

	class WindowBackend
	{
	public:
		[[nodiscard]] static std::expected<std::unique_ptr<WindowBackend>, WindowError> Create(const WindowSpecification& specification);

		~WindowBackend();

		WindowBackend(const WindowBackend&) = delete;
		WindowBackend& operator=(const WindowBackend&) = delete;

		void PollEvents();
		void SwapBuffers() const;
		[[nodiscard]] bool ShouldClose() const;
		[[nodiscard]] std::expected<std::span<const char* const>, VulkanWindowError> GetRequiredVulkanExtensions() const;
		[[nodiscard]] std::expected<VkSurfaceKHR, VulkanWindowError> CreateVulkanSurface(VkInstance instance) const pre(_handle);

	private:
		explicit WindowBackend(GLFWwindow* handle) : _handle(handle)
		{}

		GLFWwindow* _handle;
	};

	std::expected<std::unique_ptr<WindowBackend>, WindowError> WindowBackend::Create(const WindowSpecification& specification)
	{
		if (!EnsurePlatformInitialized())
		{
			return std::unexpected(WindowError::PlatformInitializationFailed);
		}

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		GLFWwindow* handle = glfwCreateWindow(specification.Width, specification.Height, specification.Title.c_str(), nullptr, nullptr);

		if (handle == nullptr)
		{
			// Roll back the init this call may have triggered so a failed creation
			// doesn't leave the platform up with no windows behind it.
			if (s_liveWindowCount == 0)
			{
				glfwTerminate();
			}

			return std::unexpected(WindowError::WindowCreationFailed);
		}

		++s_liveWindowCount;

		PGE_LOG(Info, "Created GLFW window \"{}\" ({}x{})", specification.Title, specification.Width, specification.Height);

		return std::unique_ptr<WindowBackend>(new WindowBackend(handle));
	}

	WindowBackend::~WindowBackend()
	{
		glfwDestroyWindow(_handle);

		if (--s_liveWindowCount == 0)
		{
			glfwTerminate();
		}
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	void WindowBackend::PollEvents()
	{
		// Processes the platform-wide event queue (all windows), not just this one.
		glfwPollEvents();
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	void WindowBackend::SwapBuffers() const
	{
		// glfwSwapBuffers(_handle);
	}

	bool WindowBackend::ShouldClose() const
	{
		return glfwWindowShouldClose(_handle) != 0;
	}

	// ReSharper disable once CppMemberFunctionMayBeStatic
	std::expected<std::span<const char* const>, VulkanWindowError> WindowBackend::GetRequiredVulkanExtensions() const
	{
		std::uint32_t extensionCount = 0;
		const char** extensionNames = glfwGetRequiredInstanceExtensions(&extensionCount);

		// GLFW yields null when the Vulkan loader is missing and does not define what it leaves in the count, so the pointer is the only signal
		// that can be trusted here. A working Vulkan build always reports at least VK_KHR_surface plus a platform
		// surface extension, making an empty list a broken platform rather than a valid answer.
		if (extensionNames == nullptr || extensionCount == 0)
		{
			return std::unexpected(VulkanWindowError::ExtensionsUnavailable);
		}

		return std::span<const char* const>{extensionNames, extensionCount};
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
