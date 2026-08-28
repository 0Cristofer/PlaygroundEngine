export module PlaygroundEngine.Renderer.Frame;

import PlaygroundEngine.Renderer.View;
import PlaygroundEngine.Renderer.Mesh;

import std;

namespace PgE
{
	export struct ExtractedFrame
	{
		ExtractedView View;
		std::vector<ExtractedMesh> Meshes;
	};
}
