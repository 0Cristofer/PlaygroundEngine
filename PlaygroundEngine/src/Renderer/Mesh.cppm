export module PlaygroundEngine.Renderer.Mesh;

import PlaygroundEngine.Math;

import std;

namespace PgE
{
	// Names a mesh the renderer owns. Not generational yet, the same gap Entity carries.
	export struct MeshHandle
	{
		static constexpr std::uint32_t InvalidIndex = 0;

		std::uint32_t Index = InvalidIndex;

		bool operator==(const MeshHandle& other) const = default;
	};

	export struct ExtractedMesh
	{
		MeshHandle Mesh;
		Transform Placement;
	};
}
