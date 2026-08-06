export module PlaygroundEngine.WindowServer:WindowSizes;

namespace PgE
{
	// The window's size in screen coordinates, which is what the window system positions and lays
	// out in.

	export struct WindowSize
	{
		int Width = 0;
		int Height = 0;
	};

	// The drawable size in pixels: the screen-coordinate size scaled by the display's content
	// scale, and equal to it only at a scale of 1. The two are separate types because confusing
	// them is a silently wrong swapchain rather than a compile error.

	export struct FramebufferSize
	{
		int Width = 0;
		int Height = 0;
	};
}
