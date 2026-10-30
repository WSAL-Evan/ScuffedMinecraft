#pragma once

#include <wv/rendering/VertexAttribute.h>
#include <wv/pch.h>

namespace ScuffedMinecraft
{
	struct ChunkVertex
	{
		glm::vec3 pos;
		glm::vec2 texCoord;
		uint32_t lightData;

		ChunkVertex(glm::vec3 pos, glm::vec2 texCoord, uint32_t lightData)
			: pos(pos), texCoord(texCoord), lightData(lightData) {}

		static std::vector<WillowVox::VertexAttribute> GetVertexAttributes()
		{
			return {
				{.location = 0, .format = WillowVox::VertexAttributeType::FLOAT32_3, .offset = offsetof(ChunkVertex, pos) },
				{.location = 1, .format = WillowVox::VertexAttributeType::FLOAT32_2, .offset = offsetof(ChunkVertex, texCoord) },
				{.location = 2, .format = WillowVox::VertexAttributeType::UINT32, .offset = offsetof(ChunkVertex, lightData) },
			};
		}
	};
}