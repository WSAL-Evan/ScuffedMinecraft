#pragma once

#include <wv/rendering/VertexAttribute.h>
#include <wv/pch.h>

namespace WillowVox
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec2 texCoord;

		Vertex(glm::vec3 pos, glm::vec2 texCoord)
			: pos(pos), texCoord(texCoord) {}

		static std::vector<VertexAttribute> GetVertexAttributes()
		{
			return {
				{.location = 0, .format = VertexAttributeType::FLOAT32_3, .offset = offsetof(Vertex, pos) },
				{.location = 1, .format = VertexAttributeType::FLOAT32_2, .offset = offsetof(Vertex, texCoord) }
			};
		}
	};
}