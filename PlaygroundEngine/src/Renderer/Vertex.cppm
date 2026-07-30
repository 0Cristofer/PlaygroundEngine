module;

#include <glm/glm.hpp>

export module PlaygroundEngine.Renderer.Vertex;

import std;
import vulkan;

namespace PgE
{
	export struct Vertex
	{
		glm::vec2 Pos;
		glm::vec3 Color;

		static vk::VertexInputBindingDescription GetBindingDescription()
		{
			return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
		}

		static std::array<vk::VertexInputAttributeDescription, 2> GetAttributeDescriptions()
		{
			return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, Pos)},
					 {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, Color)}}};
		}
	};

	export struct UniformBufferObject
	{
		glm::mat4 Model;
		glm::mat4 View;
		glm::mat4 Proj;
	};
}
