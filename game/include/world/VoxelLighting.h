#pragma once

#include <world/ChunkHolder.h>
#include <unordered_set>

namespace ScuffedMinecraft
{
	class ChunkManager;

	namespace VoxelLighting
	{
		std::vector<ChunkHolder*> AddLightSource(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z, int lightLevel);
		std::vector<ChunkHolder*> RemoveLightSource(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z);

		std::vector<ChunkHolder*> AddLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z);
		std::vector<ChunkHolder*> RemoveLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z);

		std::vector<ChunkHolder*> AddSkyLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z);
		std::vector<ChunkHolder*> RemoveSkyLightBlocker(ChunkManager* chunkManager, ChunkHolder* chunkHolder, int x, int y, int z);

		std::vector<ChunkHolder*> CalculateFullLighting(ChunkManager* chunkManager, ChunkHolder* chunkHolder);
	}
}