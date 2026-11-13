#pragma once

#include <world/WorldDefines.h>
#include <wv/core.h>

namespace ScuffedMinecraft
{
	struct FeatureSnapshot
	{
		glm::ivec3 id;
		uint32_t dataVersion;
		BlockId blocks[CHUNK_VOLUME];
		std::array<glm::ivec3, 27> ids;
		std::array<std::vector<int>, 27> surfaceBlocks;
	};
}