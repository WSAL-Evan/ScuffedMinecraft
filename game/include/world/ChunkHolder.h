#pragma once

#include <world/ChunkData.h>
#include <wv/core.h>

namespace ScuffedMinecraft
{
	enum class ChunkStage : uint8_t { Missing, Loading, Generated, Lit };

	struct ChunkHolder
	{
		// Data
		glm::ivec3 id;
		uint64_t token;
		std::unique_ptr<ChunkData> data;

		ChunkStage stage = ChunkStage::Missing;
		bool jobInFlight = false;

		// Meshing
		std::unique_ptr<WillowVox::Mesh> mesh;
		bool meshDirty = false;
		uint32_t dataVersion = 0;

		ChunkHolder(const glm::ivec3& id, uint64_t token)
			: id(id), token(token) {}
	};
}