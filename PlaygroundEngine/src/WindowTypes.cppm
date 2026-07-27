export module PlaygroundEngine.Window:common;

import std;

namespace PgE
{
	export struct WindowSpecification
	{
		std::string Title = "Playground";
		int Width = 1280;
		int Height = 720;
	};

	// The drawable size in pixels: the window's screen-coordinate size scaled by the display's
	// content scale.
	export struct FramebufferSize
	{
		int Width = 0;
		int Height = 0;
	};

	// Notification that the drawable size changed. A stand-in for the event queue: once input and
	// window events are routed properly this becomes one more event, not a registered callback.

	export using FramebufferResizedCallback = std::function<void(FramebufferSize)>;

	export enum class WindowError
	{
		PlatformInitializationFailed,
		WindowCreationFailed,
	};

	export enum class VulkanWindowError
	{
		ExtensionsUnavailable,
		SurfaceCreationFailed
	};

	// The platform backend, forward-declared here so the primary interface can name it for
	// the window's PIMPL pointer without pulling in a definition. The per-platform :backend
	// partition completes it; consumers only ever see this incomplete name.

	export class WindowBackend;
}
