module;

#include "PlaygroundEngine/Log.h"

#include <tiny_obj_loader.h>

module PlaygroundEngine.Mesh;

import std;
import PlaygroundEngine.Log;

namespace PgE
{
	namespace
	{
		// Deduplication keys on the Wavefront index triple rather than on the built vertex, so no
		// float ever takes part in a hash or an equality test.

		struct VertexIndexKey
		{
			int PositionIndex;
			int NormalIndex;
			int TextureCoordinateIndex;

			bool operator==(const VertexIndexKey& other) const = default;
		};

		struct ObjectIndexKeyHash
		{
			std::size_t operator()(const VertexIndexKey& key) const
			{
				// The usual golden-ratio mix: the three indices are small and highly correlated
				// across a mesh, so folding them without one leaves most bits of the result unused.

				auto combine = [](std::size_t seed, const int value) {
					return seed ^ (std::hash<int>{}(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2));
				};

				return combine(combine(combine(0, key.PositionIndex), key.NormalIndex), key.TextureCoordinateIndex);
			}
		};

		MeshVertex MakeVertex(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index)
		{
			// A negative component index means the file omitted that attribute for this vertex, which
			// Wavefront permits and which would otherwise index the array from the wrong end.

			MeshVertex vertex{.Position = {}, .Normal = {}, .TextureCoordinate = {}};

			if (index.vertex_index >= 0)
			{
				const std::size_t offset = 3 * static_cast<std::size_t>(index.vertex_index);
				vertex.Position = {attributes.vertices[offset], attributes.vertices[offset + 1], attributes.vertices[offset + 2]};
			}

			if (index.normal_index >= 0)
			{
				const std::size_t offset = 3 * static_cast<std::size_t>(index.normal_index);
				vertex.Normal = {attributes.normals[offset], attributes.normals[offset + 1], attributes.normals[offset + 2]};
			}

			if (index.texcoord_index >= 0)
			{
				const std::size_t offset = 2 * static_cast<std::size_t>(index.texcoord_index);
				vertex.TextureCoordinate = {attributes.texcoords[offset], 1.0f - attributes.texcoords[offset + 1]};
			}

			return vertex;
		}
	}

	std::expected<Mesh, MeshError> ParseWavefrontMesh(const std::string_view objectText)
	{
		// No material search path: resolving an .mtl reference would need file access, which this
		// module deliberately does not have, and no material data is consumed yet.

		tinyobj::ObjReaderConfig config;
		config.mtl_search_path = "";
		config.triangulate = true;

		tinyobj::ObjReader reader;
		if (!reader.ParseFromString(std::string(objectText), "", config))
		{
			PGE_LOG(Error, "Unable to parse Wavefront mesh: {}", reader.Error());
			return std::unexpected(MeshError::ParseFailed);
		}

		if (!reader.Warning().empty())
		{
			PGE_LOG(Info, "Wavefront mesh parsed with warnings: {}", reader.Warning());
		}

		const tinyobj::attrib_t& attributes = reader.GetAttrib();

		Mesh mesh;
		std::unordered_map<VertexIndexKey, std::uint32_t, ObjectIndexKeyHash> vertexIndices;

		for (const tinyobj::shape_t& shape : reader.GetShapes())
		{
			// Every shape folds into one mesh: the renderer draws a single vertex and index buffer,
			// and Wavefront groups carry no meaning the engine acts on yet.

			for (const tinyobj::index_t& index : shape.mesh.indices)
			{
				const VertexIndexKey key{
					.PositionIndex = index.vertex_index, .NormalIndex = index.normal_index, .TextureCoordinateIndex = index.texcoord_index};

				const auto [existingIndex, inserted] = vertexIndices.try_emplace(key, static_cast<std::uint32_t>(mesh.Vertices.size()));
				if (inserted)
				{
					mesh.Vertices.push_back(MakeVertex(attributes, index));
				}

				mesh.Indices.push_back(existingIndex->second);
			}
		}

		if (mesh.Indices.empty())
		{
			return std::unexpected(MeshError::NoGeometry);
		}

		return mesh;
	}
}
