module;

#include <glm/glm.hpp>

export module PlaygroundEngine.Renderer.Vertex;

import std;
import vulkan;
import PlaygroundEngine.Math;

namespace PgE
{
	export struct Vertex
	{
		glm::vec3 Pos;
		glm::vec3 Color;
		glm::vec2 TexCoord;

		static vk::VertexInputBindingDescription GetBindingDescription()
		{
			return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
		}

		static std::array<vk::VertexInputAttributeDescription, 3> GetAttributeDescriptions()
		{
			return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, Pos)},
					 {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, Color)},
					 {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, TexCoord)}}};
		}
	};

	export struct UniformBufferObject
	{
		Matrix4x4 WorldToView;
		Matrix4x4 ViewToClip;
	};

	// One mesh's placement, pushed per draw. Sized to stay inside the 128 bytes a Vulkan implementation
	// is required to offer, so no device query gates it.
	export struct MeshPushConstants
	{
		Matrix4x4 LocalToWorld;
	};
}
