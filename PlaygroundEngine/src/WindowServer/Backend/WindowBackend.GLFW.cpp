module;

#define GLFW_INCLUDE_NONE
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

module PlaygroundEngine.WindowServer;

import std;

import :WindowBackend;

namespace PgE
{
	namespace
	{
		int ToGlfwCursorShape(const CursorShape shape)
		{
			switch (shape)
			{
			case CursorShape::TextInput:
				return GLFW_IBEAM_CURSOR;
			case CursorShape::Hand:
				return GLFW_POINTING_HAND_CURSOR;
			case CursorShape::NotAllowed:
				return GLFW_NOT_ALLOWED_CURSOR;
			case CursorShape::ResizeHorizontal:
				return GLFW_RESIZE_EW_CURSOR;
			case CursorShape::ResizeVertical:
				return GLFW_RESIZE_NS_CURSOR;
			case CursorShape::ResizeTopLeftBottomRight:
				return GLFW_RESIZE_NWSE_CURSOR;
			case CursorShape::ResizeTopRightBottomLeft:
				return GLFW_RESIZE_NESW_CURSOR;
			case CursorShape::ResizeAll:
				return GLFW_RESIZE_ALL_CURSOR;
			case CursorShape::Arrow:
			case CursorShape::Hidden:
				return GLFW_ARROW_CURSOR;
			}

			return GLFW_ARROW_CURSOR;
		}
	}

	WindowBackend::~WindowBackend()
	{
		for (GLFWcursor* cursor : _cursors)
		{
			if (cursor != nullptr)
			{
				glfwDestroyCursor(cursor);
			}
		}

		glfwDestroyWindow(_handle);
	}

	GLFWcursor* WindowBackend::GetCursor(const CursorShape shape)
	{
		const auto index = std::to_underlying(shape);
		if (index >= _cursors.size())
		{
			return nullptr;
		}

		if (_cursors[index] == nullptr)
		{
			// A shape the cursor theme does not carry comes back null, which glfwSetCursor reads as
			// the system default. That is the right outcome, so the failure needs no branch of its own.

			_cursors[index] = glfwCreateStandardCursor(ToGlfwCursorShape(shape));
		}

		return _cursors[index];
	}

	void WindowBackend::SetCursorShape(const CursorShape shape)
	{
		if (shape == _currentShape)
		{
			return;
		}

		_currentShape = shape;

		if (shape == CursorShape::Hidden)
		{
			glfwSetInputMode(_handle, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
			return;
		}

		glfwSetCursor(_handle, GetCursor(shape));
		glfwSetInputMode(_handle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
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

	ContentScale WindowBackend::GetContentScale() const
	{
		float horizontal = 1.0f;
		float vertical = 1.0f;
		glfwGetWindowContentScale(_handle, &horizontal, &vertical);

		return ContentScale{.X = horizontal, .Y = vertical};
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
