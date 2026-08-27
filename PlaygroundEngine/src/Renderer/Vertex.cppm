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
		Matrix4x4 Model;
		Matrix4x4 WorldToView;
		Matrix4x4 ViewToClip;
	};
}
