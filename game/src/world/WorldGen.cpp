#include <world/WorldGen.h>

#include <wv/core.h>
#include <random>

namespace ScuffedMinecraft
{
    WorldGen::WorldGen(float minHeight, float maxHeight, float surfaceScale,
        int surfaceOctaves, float surfaceGain, float surfaceLacunarity,
        float caveScale, int caveOctaves, float caveGain, float caveLacunarity, float caveThreshold,
        std::vector<WorldFeature>&& features)
        : _caveThreshold(caveThreshold)
    {
        _features = std::move(features);

        // Surface Noise
        {
            auto simplex = FastNoise::New<FastNoise::Simplex>();
            simplex->SetOutputMin(minHeight);
            simplex->SetOutputMax(maxHeight);
            simplex->SetScale(surfaceScale);
            _surfaceNoise = FastNoise::New<FastNoise::FractalFBm>();
            _surfaceNoise->SetOctaveCount(surfaceOctaves);
            _surfaceNoise->SetGain(surfaceGain);
            _surfaceNoise->SetLacunarity(surfaceLacunarity);
            _surfaceNoise->SetSource(simplex);
        }
        // Cave Noise
        {
            auto simplex = FastNoise::New<FastNoise::Simplex>();
            simplex->SetOutputMin(0.0f);
            simplex->SetOutputMax(1.0f);
            simplex->SetScale(caveScale);
            _caveNoise = FastNoise::New<FastNoise::FractalFBm>();
            _caveNoise->SetOctaveCount(caveOctaves);
            _caveNoise->SetGain(caveGain);
            _caveNoise->SetLacunarity(caveLacunarity);
            _caveNoise->SetSource(simplex);
        }
    }

    void WorldGen::GenerateChunkShape(const glm::ivec3& id, ChunkData* data)
    {
        std::vector<int> surfaceBlocks;

        glm::ivec3 chunkOffset = id * CHUNK_SIZE;

	    float heights[CHUNK_SIZE * CHUNK_SIZE];
	    _surfaceNoise->GenUniformGrid2D(heights, chunkOffset.x, chunkOffset.z, CHUNK_SIZE, CHUNK_SIZE, 1.0f, 1.0f, 0);

        float caveValues[CHUNK_VOLUME];
        _caveNoise->GenUniformGrid3D(caveValues, chunkOffset.x, chunkOffset.y, chunkOffset.z,
            CHUNK_SIZE, CHUNK_SIZE, CHUNK_SIZE, 1.0f, 1.0f, 1.0f, 0);

        int index = 0;
        for (int z = 0; z < CHUNK_SIZE; z++)
        {
            int worldZ = chunkOffset.z + z;
            for (int x = 0; x < CHUNK_SIZE; x++)
            {
                int worldX = chunkOffset.x + x;
                int height = std::round(heights[z * CHUNK_SIZE + x]);
                for (int y = 0; y < CHUNK_SIZE; y++)
                {
                    int worldY = chunkOffset.y + y;
                    if (worldY <= height)
                    {
                        if (caveValues[x + CHUNK_SIZE * (y + CHUNK_SIZE * z)] < _caveThreshold)
                        {
                            if (worldY == height)
                            {
                                data->SetBlock(index, 3);
                                surfaceBlocks.push_back(index);
                            }
                            else if (worldY < height - 5)
                                data->SetBlock(index, 4);
                            else
                                data->SetBlock(index, 2);
                        }
                    }

                    index++;
                }
            }
        }

        data->SetSurfaceBlocks(std::move(surfaceBlocks));
    }

    static inline bool IsOverlappingChunk(glm::ivec3 chunkPos, glm::ivec3 featurePos, glm::ivec3 featureSize)
    {
        return chunkPos.x < featurePos.x + featureSize.x && chunkPos.x + CHUNK_SIZE > featurePos.x
            && chunkPos.y < featurePos.y + featureSize.y && chunkPos.y + CHUNK_SIZE > featurePos.y
            && chunkPos.z < featurePos.z + featureSize.z && chunkPos.z + CHUNK_SIZE > featurePos.z;
    }

    void WorldGen::GenerateChunkFeatures(const glm::ivec3& id, FeatureSnapshot* snapshot)
    {
        glm::ivec3 chunkPos = id * CHUNK_SIZE;

        for (int n = 0; n < snapshot->ids.size(); n++)
        {
            glm::ivec3 nId = snapshot->ids[n];
            std::mt19937 nGen(nId.x * 12 + nId.y * 23 + nId.z * 34);
            std::uniform_real_distribution<float> nDist(0.0f, 1.0f);

            glm::ivec3 nOffset = (nId - id) * CHUNK_SIZE;

            for (int surfaceBlock : snapshot->surfaceBlocks[n])
            {
                for (auto& feature : _features)
                {
                    float val = nDist(nGen);
                    if (val <= feature.placementChance)
                    {
                        auto fPos = ChunkData::IndexToPos(surfaceBlock);
                        
                        //if (nId != id && !IsOverlappingChunk(chunkPos, fPos + (nId * CHUNK_SIZE) + feature.center, { feature.layers[0].size(), feature.layers.size(), feature.layers[0][0].size() }))
                        //    break;

                        // Place feature
                        for (int y = 0; y < feature.layers.size(); y++)
                        {
                            int localY = fPos.y + nOffset.y + y + feature.center.y;
                            if (localY < 0) continue;
                            if (localY >= CHUNK_SIZE) break;
                            for (int x = 0; x < feature.layers[y].size(); x++)
                            {
                                int localX = fPos.x + nOffset.x + x + feature.center.x;
                                if (localX < 0) continue;
                                if (localX >= CHUNK_SIZE) break;
                                for (int z = 0; z < feature.layers[y][x].size(); z++)
                                {
                                    int localZ = fPos.z + nOffset.z + z + feature.center.z;
                                    if (localZ < 0) continue;
                                    if (localZ >= CHUNK_SIZE) break;

                                    if (feature.replace[y][x][z] || snapshot->blocks[ChunkData::Index(localX, localY, localZ)] == AIR)
                                        snapshot->blocks[ChunkData::Index(localX, localY, localZ)] = feature.layers[y][x][z];
                                }
                            }
                        }

                        break;
                    }
                }
            }
        }
    }
}