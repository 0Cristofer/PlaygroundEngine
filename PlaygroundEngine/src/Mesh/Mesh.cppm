module;

#include <glm/glm.hpp>

export module PlaygroundEngine.Mesh;

import std;

namespace PgE
{
	export enum class MeshError
	{
		ParseFailed,
		NoGeometry,
	};

	export struct MeshVertex
	{
		glm::vec3 Position;
		glm::vec3 Normal;
		glm::vec2 TextureCoordinate;
	};

	// Texture coordinates carry a top-left origin, which is what the rendering backends sample with.
	// Wavefront's own V axis runs the other way and is flipped during parsing

	export struct Mesh
	{
		std::vector<MeshVertex> Vertices;
		std::vector<std::uint32_t> Indices;
	};

	// Faces are triangulated and
	// vertices sharing a position/normal/texture-coordinate triple are merged onto one index.

	export std::expected<Mesh, MeshError> ParseWavefrontMesh(std::string_view objectText);
}
