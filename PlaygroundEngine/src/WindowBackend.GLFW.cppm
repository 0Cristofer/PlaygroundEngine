module;

#include "PlaygroundEngine/Log.h"

#define GLFW_INCLUDE_NONE
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

	private:
		explicit WindowBackend(GLFWwindow* handle) : _handle(handle) {}

		GLFWwindow* _handle;
	};

	std::expected<std::unique_ptr<WindowBackend>, WindowError> WindowBackend::Create(const WindowSpecification& specification)
	{
		if (!EnsurePlatformInitialized())
		{
			return std::unexpected(WindowError::PlatformInitializationFailed);
		}

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

		// Bind the window's GL context to this (main) thread so buffer swaps present to it.
		// A dedicated GraphicsContext will own this once the renderer exists.
		glfwMakeContextCurrent(handle);

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

	void WindowBackend::PollEvents()
	{
		// Processes the platform-wide event queue (all windows), not just this one.
		glfwPollEvents();
	}

	void WindowBackend::SwapBuffers() const
	{
		glfwSwapBuffers(_handle);
	}

	bool WindowBackend::ShouldClose() const
	{
		return glfwWindowShouldClose(_handle) != 0;
	}
}
