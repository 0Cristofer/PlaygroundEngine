export module PlaygroundEngine.MeshCatalog;

import PlaygroundEngine.Renderer.Mesh;

import std;

namespace PgE
{
	// Maps the path a MeshComponent names onto the handle the renderer handed back for it. The stand-in
	// for the asset system: it is filled by the composition root at load, and only read during extraction.
	export class MeshCatalog
	{
	public:
		// An invalid handle for a path that is unknown, and for one whose load failed.
		[[nodiscard]] MeshHandle Find(std::string_view path) const;

		// Whether the path was already attempted, which is what keeps a failed load from being retried
		// every frame.
		[[nodiscard]] bool Contains(std::string_view path) const;

		void Insert(std::string path, MeshHandle handle);

	private:
		// std::less<> so a string_view looks up without building a string.
		std::map<std::string, MeshHandle, std::less<>> _handlesPerPath;
	};
}
