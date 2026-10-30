#pragma once

#include <world/ChunkManager.h>

namespace ScuffedMinecraft
{
	struct VoxelRaycastResult
	{
		bool hit;
		float hitX;
		float hitY;
		float hitZ;
	};

	VoxelRaycastResult VoxelRaycast(ChunkManager& chunkManager, const glm::vec3& origin, const glm::vec3& direction, float maxDistance);
}