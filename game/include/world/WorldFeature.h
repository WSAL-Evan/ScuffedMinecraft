#pragma once

#include <world/WorldDefines.h>

namespace ScuffedMinecraft
{
	struct WorldFeature
	{
		std::vector<std::vector<std::vector<BlockId>>> layers;
		std::vector<std::vector<std::vector<bool>>> replace;
		glm::ivec3 center;
		float placementChance;
	};
}