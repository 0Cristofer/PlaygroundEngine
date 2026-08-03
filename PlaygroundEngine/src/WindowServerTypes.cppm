export module PlaygroundEngine.WindowServer:common;

export import :events;

import std;

namespace PgE
{
	export struct WindowSpecification
	{
		std::string Title = "Playground";
		int Width = 1280;
		int Height = 720;
	};

	// The window's size in screen coordinates, which is what the window system positions and lays
	// out in. Distinct from the drawable size below, and equal to it only at a content scale of 1.

	export struct WindowSize
	{
		int Width = 0;
		int Height = 0;
	};

	// The drawable size in pixels: the window's screen-coordinate size scaled by the display's
	// content scale.

	export struct FramebufferSize
	{
		int Width = 0;
		int Height = 0;
	};

	export enum class WindowServerError
	{
		ConnectionFailed,
	};

	export enum class WindowError
	{
		WindowCreationFailed,
	};

	export enum class VulkanWindowError
	{
		ExtensionsUnavailable,
		SurfaceCreationFailed
	};

	// The platform backends, forward-declared here so the primary interface can name them for its
	// PIMPL pointers without pulling in a definition. The per-platform :backend partition completes
	// them; consumers only ever see these incomplete names.

	export class WindowServerBackend;
	export class WindowBackend;
}
