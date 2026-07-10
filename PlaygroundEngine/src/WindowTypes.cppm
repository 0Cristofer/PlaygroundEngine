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

	export enum class WindowError
	{
		PlatformInitializationFailed,
		WindowCreationFailed,
	};

	// The platform backend, forward-declared here so the primary interface can name it for
	// the window's PIMPL pointer without pulling in a definition. The per-platform :backend
	// partition completes it; consumers only ever see this incomplete name.

	export class WindowBackend;
}
