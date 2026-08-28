export module PlaygroundEngine.RenderExtraction;

import PlaygroundEngine.Ecs;
import PlaygroundEngine.MeshCatalog;
import PlaygroundEngine.Renderer.Frame;

namespace PgE
{
	// Fills frame from the world, overwriting what the previous frame left. The caller keeps the storage,
	// so the growing parts of a frame are not reallocated every step.
	export void ExtractFrame(const Ecs& world, const MeshCatalog& meshes, ExtractedFrame& frame);
}
