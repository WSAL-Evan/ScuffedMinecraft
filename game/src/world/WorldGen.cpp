#include <world/WorldGen.h>

namespace ScuffedMinecraft
{
    WorldGen::WorldGen(float minHeight, float maxHeight, float surfaceScale,
        int surfaceOctaves, float surfaceGain, float surfaceLacunarity,
        float caveScale, int caveOctaves, float caveGain, float caveLacunarity, float caveThreshold)
        : _caveThreshold(caveThreshold)
    {
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

    void WorldGen::GenerateChunk(const glm::ivec3& id, ChunkData* data)
    {
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
                                data->SetBlock(index, 3);
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
    }
}