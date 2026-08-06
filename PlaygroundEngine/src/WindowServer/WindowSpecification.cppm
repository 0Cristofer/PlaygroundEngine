export module PlaygroundEngine.WindowServer:WindowSpecification;

import std;

namespace PgE
{
	// What creation asks for. It is not a description of the live window: the size stops being true
	// at the first resize, which is why Window queries its sizes rather than remembering them.

	export struct WindowSpecification
	{
		std::string Title = "Playground";
		int Width = 1280;
		int Height = 720;
	};
}
