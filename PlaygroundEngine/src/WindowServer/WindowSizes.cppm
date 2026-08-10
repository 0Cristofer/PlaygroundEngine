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

	// How much larger interface elements should be drawn than their nominal size. A density hint
	// rather than a measurement, so it is neither of the sizes above and does not derive from them:
	// a display can be dense without the window system scaling the framebuffer.

	export struct ContentScale
	{
		float X = 1.0f;
		float Y = 1.0f;
	};
}
