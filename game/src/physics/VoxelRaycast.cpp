#include <physics/VoxelRaycast.h>
#include <world/WorldCoords.h>

namespace ScuffedMinecraft
{
    VoxelRaycastResult VoxelRaycast(ChunkManager& chunkManager, const glm::vec3& origin, const glm::vec3& direction, float maxDistance)
    {
        ChunkData* lastChunkData = nullptr;
        glm::ivec3 lastChunkId;
        glm::ivec3 prevBlock = glm::ivec3(origin.x + 10, origin.y + 10, origin.z + 10);
        float dist = 0.0f;
        while (dist <= maxDistance)
        {
            // Get current position
            auto pos = origin + direction * dist;

            // Get chunk at position
            auto block = WorldToBlock(pos.x, pos.y, pos.z);
            if (block != prevBlock) // Don't check the same block multiple times
            {
                auto chunkId = BlockToChunkId(block.x, block.y, block.z);
                auto localPos = BlockToLocal(block.x, block.y, block.z);

                if (!lastChunkData || lastChunkId != chunkId)
                {
                    lastChunkData = chunkManager.GetChunkData(chunkId);
                    lastChunkId = chunkId;
                }

                // Check block
                if (lastChunkData && lastChunkData->GetBlock(localPos.x, localPos.y, localPos.z) != AIR)
                {
                    return { true, pos.x, pos.y, pos.z };
                }
            }

            // Update variables
            dist += 0.01f;
            prevBlock = block;
        }

        return { false, 0.0f, 0.0f, 0.0f };
    }
}